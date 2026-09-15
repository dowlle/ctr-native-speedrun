#!/usr/bin/env python3
"""Companion bridge: drive LiveSplit from the CTR speedrun client.

Two sources:

- events: tail speedrun-events.log, which the client always writes. Correct at
  splits, approximate for the live clock between them.
- surface: read the .ctrsr surface block from the running client and poll it, so
  LiveSplit's game time tracks the client's loadless clock continuously.

It talks to LiveSplit Server over TCP (enable it in LiveSplit under
Edit Layout, Add, Control, LiveSplit Server, default port 16834). Protocol
reference: src/LiveSplit.Core/Server/CommandServer.cs.

Usage:
    speedrun-bridge.py --source events --events speedrun-events.log
    speedrun-bridge.py --source surface --surface-file speedrun-surface.bin
    speedrun-bridge.py --source surface --process ctr_native
    speedrun-bridge.py --source surface --pid 1234 --duration 5
    speedrun-bridge.py --source events --events speedrun-events.log --dry-run
"""

import argparse
import ctypes
import os
import socket
import struct
import sys
import time

SURFACE_MAGIC = 0x43545253
SURFACE_ABI = 1
SURFACE_SECTION = ".ctrsr"
SURFACE_FORMAT = "<IIIIiIIiiIiIIIi"  # see include/platform/native_speedrun.h
SURFACE_FIELDS = (
    "magic",
    "abiVersion",
    "sequence",
    "flags",
    "segmentIndex",
    "loadlessMS",
    "rtaMS",
    "lastEventType",
    "lastEventLevelID",
    "lastEventGameMode1",
    "lastEventSegmentIndex",
    "lastEventSequence",
    "lastEventSegmentTimeMS",
    "lastEventTotalTimeMS",
    "lastEventFinishPosition",
)
SURFACE_STRUCT = struct.Struct(SURFACE_FORMAT)

FLAG_ACTIVE = 1 << 0

EVENT_RUN_START = 1
EVENT_SPLIT = 2
EVENT_RUN_END = 3
EVENT_RESET = 4
EVENT_LEVEL_ENTER = 5
EVENT_LEVEL_EXIT = 6
EVENT_RACE_FINISH = 7

EVENT_KIND = {
    EVENT_RUN_START: "run_start",
    EVENT_SPLIT: "split",
    EVENT_RUN_END: "run_end",
    EVENT_RESET: "reset",
    EVENT_LEVEL_ENTER: "level_enter",
    EVENT_LEVEL_EXIT: "level_exit",
    EVENT_RACE_FINISH: "race_finish",
}


def format_gametime(milliseconds):
    """LiveSplit timespan text, for example 0:02:08.128."""
    milliseconds = int(milliseconds)
    seconds, millis = divmod(milliseconds, 1000)
    minutes, seconds = divmod(seconds, 60)
    hours, minutes = divmod(minutes, 60)
    return f"{hours}:{minutes:02d}:{seconds:02d}.{millis:03d}"


def parse_event_line(line):
    """Parses one key=value event line into a dict. Returns {} if malformed."""
    fields = {}
    for token in line.split():
        if "=" in token:
            key, value = token.split("=", 1)
            fields[key] = value
    return fields if "type" in fields else {}


def commands_for_kind(kind, total_ms=0):
    """Maps an event kind to the LiveSplit Server commands it triggers."""
    if kind == "run_start":
        return ["reset", "switchto gametime", "setgametime " + format_gametime(0), "starttimer"]

    if kind in ("split", "run_end"):
        return ["setgametime " + format_gametime(total_ms), "split"]

    if kind == "reset":
        return ["reset"]

    return []


def commands_for_event(fields):
    """Maps one parsed event line to the LiveSplit Server commands."""
    return commands_for_kind(fields.get("type"), fields.get("loadless_ms", "0"))


def decode_surface(data):
    """Decodes a .ctrsr byte block. Returns None if the magic or ABI is wrong."""
    if len(data) < SURFACE_STRUCT.size:
        return None

    values = SURFACE_STRUCT.unpack_from(data, 0)
    if values[0] != SURFACE_MAGIC or values[1] != SURFACE_ABI:
        return None

    return dict(zip(SURFACE_FIELDS, values))


class SurfaceReader:
    """Reads the .ctrsr surface from a running process."""

    def __init__(self, pid):
        self.pid = pid

    def read(self):
        if sys.platform == "win32":
            data = _read_surface_bytes_windows(self.pid)
        else:
            data = _read_surface_bytes_linux(self.pid)
        return decode_surface(data) if data else None


class FileSurfaceReader:
    """Reads the surface from the client's surface file.

    The preferred path on Linux, where yama ptrace_scope blocks cross-process
    memory reads. The client writes speedrun-surface.bin every frame.
    """

    def __init__(self, path):
        self.path = path

    def read(self):
        try:
            with open(self.path, "rb") as handle:
                data = handle.read(SURFACE_STRUCT.size)
        except OSError:
            return None
        return decode_surface(data)


def _elf_find_section(path, wanted):
    with open(path, "rb") as handle:
        ident = handle.read(16)
        if ident[:4] != b"\x7fELF":
            return None
        is64 = ident[4] == 2

        if is64:
            handle.seek(0x28)
            shoff = struct.unpack("<Q", handle.read(8))[0]
            handle.seek(0x3A)
            shentsize, shnum, shstrndx = struct.unpack("<HHH", handle.read(6))
        else:
            handle.seek(0x20)
            shoff = struct.unpack("<I", handle.read(4))[0]
            handle.seek(0x2E)
            shentsize, shnum, shstrndx = struct.unpack("<HHH", handle.read(6))

        def read_header(index):
            handle.seek(shoff + index * shentsize)
            raw = handle.read(shentsize)
            if is64:
                name, _type, _flags, addr, offset, size = struct.unpack_from("<IIQQQQ", raw, 0)
            else:
                name, _type, _flags, addr, offset, size = struct.unpack_from("<IIIIII", raw, 0)
            return name, addr, offset, size

        _, _, str_offset, str_size = read_header(shstrndx)
        handle.seek(str_offset)
        strtab = handle.read(str_size)

        for index in range(shnum):
            name, addr, offset, size = read_header(index)
            end = strtab.find(b"\x00", name)
            if strtab[name:end].decode("ascii", "replace") == wanted:
                return {"addr": addr, "offset": offset, "size": size}

    return None


def _find_mapping_for_offset(pid, exe_path, file_offset):
    real = os.path.realpath(exe_path)
    with open(f"/proc/{pid}/maps", encoding="ascii", errors="replace") as handle:
        for line in handle:
            parts = line.split()
            if len(parts) < 6 or parts[-1] != real:
                continue
            start_str, end_str = parts[0].split("-")
            mapping_offset = int(parts[2], 16)
            if mapping_offset <= file_offset < mapping_offset + (int(end_str, 16) - int(start_str, 16)):
                return int(start_str, 16) + (file_offset - mapping_offset)
    return None


def _read_surface_bytes_linux(pid):
    try:
        exe_path = os.readlink(f"/proc/{pid}/exe")
    except OSError:
        return None

    section = _elf_find_section(exe_path, SURFACE_SECTION)
    if section is None:
        return None

    address = _find_mapping_for_offset(pid, exe_path, section["offset"])
    if address is None:
        return None

    try:
        with open(f"/proc/{pid}/mem", "rb", buffering=0) as handle:
            handle.seek(address)
            return handle.read(SURFACE_STRUCT.size)
    except OSError:
        return None


def _read_surface_bytes_windows(pid):
    PROCESS_VM_READ = 0x0010
    PROCESS_QUERY_INFORMATION = 0x0400
    TH32CS_SNAPMODULE = 0x00000008
    TH32CS_SNAPMODULE32 = 0x00000010
    INVALID_HANDLE = ctypes.c_void_p(-1).value

    kernel32 = ctypes.windll.kernel32
    handle = kernel32.OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, False, pid)
    if not handle:
        return None

    try:
        class MODULEENTRY32(ctypes.Structure):
            _fields_ = [
                ("dwSize", ctypes.c_uint32),
                ("th32ModuleID", ctypes.c_uint32),
                ("th32ProcessID", ctypes.c_uint32),
                ("GlblcntUsage", ctypes.c_uint32),
                ("ProccntUsage", ctypes.c_uint32),
                ("modBaseAddr", ctypes.c_void_p),
                ("modBaseSize", ctypes.c_uint32),
                ("hModule", ctypes.c_void_p),
                ("szModule", ctypes.c_char * 256),
                ("szExePath", ctypes.c_char * 260),
            ]

        snapshot = kernel32.CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid)
        if snapshot == INVALID_HANDLE:
            return None

        try:
            entry = MODULEENTRY32()
            entry.dwSize = ctypes.sizeof(MODULEENTRY32)
            if not kernel32.Module32First(snapshot, ctypes.byref(entry)):
                return None
            base = entry.modBaseAddr
            image_name = entry.szModule.decode("ascii", "replace").lower()
        finally:
            kernel32.CloseHandle(snapshot)

        if not image_name.endswith(".exe"):
            return None

        headers = ctypes.create_string_buffer(0x1000)
        read = ctypes.c_size_t(0)
        if not kernel32.ReadProcessMemory(handle, ctypes.c_void_p(base), headers, len(headers), ctypes.byref(read)):
            return None

        pe_offset = struct.unpack_from("<I", headers, 0x3C)[0]
        number_of_sections = struct.unpack_from("<H", headers, pe_offset + 6)[0]
        optional_size = struct.unpack_from("<H", headers, pe_offset + 20)[0]
        section_table = pe_offset + 24 + optional_size

        for index in range(number_of_sections):
            offset = section_table + index * 40
            name = headers[offset : offset + 8].rstrip(b"\x00").decode("ascii", "replace")
            if name != SURFACE_SECTION:
                continue
            virtual_address = struct.unpack_from("<I", headers, offset + 12)[0]
            address = base + virtual_address
            buffer = ctypes.create_string_buffer(SURFACE_STRUCT.size)
            if not kernel32.ReadProcessMemory(
                handle, ctypes.c_void_p(address), buffer, SURFACE_STRUCT.size, ctypes.byref(read)
            ):
                return None
            return buffer.raw[: SURFACE_STRUCT.size]

        return None
    finally:
        kernel32.CloseHandle(handle)


def find_pid_by_name(name):
    """Finds a process whose executable matches name (with or without .exe)."""
    wanted = name.lower()
    if wanted.endswith(".exe"):
        wanted = wanted[:-4]

    if sys.platform == "win32":
        TH32CS_SNAPPROCESS = 0x00000002
        INVALID_HANDLE = ctypes.c_void_p(-1).value
        kernel32 = ctypes.windll.kernel32

        class PROCESSENTRY32(ctypes.Structure):
            _fields_ = [
                ("dwSize", ctypes.c_uint32),
                ("cntUsage", ctypes.c_uint32),
                ("th32ProcessID", ctypes.c_uint32),
                ("th32DefaultHeapID", ctypes.c_void_p),
                ("th32ModuleID", ctypes.c_uint32),
                ("cntThreads", ctypes.c_uint32),
                ("th32ParentProcessID", ctypes.c_uint32),
                ("pcPriClassBase", ctypes.c_long),
                ("dwFlags", ctypes.c_uint32),
                ("szExeFile", ctypes.c_char * 260),
            ]

        snapshot = kernel32.CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)
        if snapshot == INVALID_HANDLE:
            return None
        try:
            entry = PROCESSENTRY32()
            entry.dwSize = ctypes.sizeof(PROCESSENTRY32)
            if not kernel32.Process32First(snapshot, ctypes.byref(entry)):
                return None
            while True:
                exe = entry.szExeFile.decode("ascii", "replace").lower()
                if exe.endswith(".exe"):
                    exe = exe[:-4]
                if exe == wanted:
                    return entry.th32ProcessID
                if not kernel32.Process32Next(snapshot, ctypes.byref(entry)):
                    return None
        finally:
            kernel32.CloseHandle(snapshot)

    for entry in os.listdir("/proc"):
        if not entry.isdigit():
            continue
        try:
            with open(f"/proc/{entry}/comm", encoding="ascii", errors="replace") as handle:
                comm = handle.read().strip().lower()
        except OSError:
            continue
        if comm == wanted:
            return int(entry)
    return None


class LiveSplitClient:
    def __init__(self, host, port, dry_run=False):
        self.host = host
        self.port = port
        self.dry_run = dry_run
        self.sock = None

    def _connect(self):
        self.sock = socket.create_connection((self.host, self.port), timeout=5)

    def send(self, command):
        if self.dry_run:
            print(command)
            return
        for attempt in (1, 2):
            try:
                if self.sock is None:
                    self._connect()
                self.sock.sendall((command + "\n").encode("ascii"))
                return
            except OSError:
                self.close()
                if attempt == 2:
                    print(f"bridge: could not reach LiveSplit at {self.host}:{self.port}", file=sys.stderr)
                    raise
                time.sleep(0.5)

    def close(self):
        if self.sock is not None:
            try:
                self.sock.close()
            except OSError:
                pass
            self.sock = None


def follow(events_path, client, poll, exit_at_eof):
    """Tails the event log and forwards new events until interrupted."""
    offset = 0
    while True:
        if not os.path.exists(events_path):
            if exit_at_eof:
                return 0
            time.sleep(poll)
            continue

        size = os.path.getsize(events_path)
        if size < offset:
            offset = 0  # file was truncated or rotated

        with open(events_path, "r", encoding="utf-8", errors="replace") as handle:
            handle.seek(offset)
            for line in handle:
                for command in commands_for_event(parse_event_line(line)):
                    client.send(command)
            offset = handle.tell()

        if exit_at_eof:
            return 0

        time.sleep(poll)


def surface_loop(reader, client, poll, duration, idle_timeout=None):
    """Polls the surface and keeps LiveSplit in step with the client clock.

    With idle_timeout set, exits once the surface has not changed for that many
    seconds, so a launcher can start it and forget it.
    """
    start = time.time()
    last_change = time.time()
    last_event_sequence = None
    last_active = False
    last_loadless = None
    have_sample = False

    while duration is None or (time.time() - start) < duration:
        surface = reader.read()
        if surface is None:
            if idle_timeout is not None and have_sample and (time.time() - last_change) > idle_timeout:
                return 0
            time.sleep(poll)
            continue

        if (
            not have_sample
            or surface["sequence"] != last_event_sequence
            or surface["loadlessMS"] != last_loadless
            or bool(surface["flags"] & FLAG_ACTIVE) != last_active
        ):
            last_change = time.time()
            have_sample = True

        active = bool(surface["flags"] & FLAG_ACTIVE)
        event_sequence = surface["lastEventSequence"]
        event_type = surface["lastEventType"]
        total = surface["lastEventTotalTimeMS"]

        if active and not last_active:
            for command in commands_for_kind("run_start"):
                client.send(command)

        if (last_event_sequence is None or event_sequence != last_event_sequence) and event_type != EVENT_RUN_START:
            for command in commands_for_kind(EVENT_KIND.get(event_type), total):
                client.send(command)

        if active and surface["loadlessMS"] != last_loadless:
            client.send("setgametime " + format_gametime(surface["loadlessMS"]))

        last_event_sequence = event_sequence
        last_active = active
        last_loadless = surface["loadlessMS"]

        if idle_timeout is not None and have_sample and (time.time() - last_change) > idle_timeout:
            return 0

        time.sleep(poll)

    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--source", choices=("events", "surface"), default="events")
    parser.add_argument("--events", default="speedrun-events.log")
    parser.add_argument("--process", default="ctr_native")
    parser.add_argument("--pid", type=int)
    parser.add_argument("--surface-file", help="read the surface from this file instead of process memory")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=16834)
    parser.add_argument("--poll", type=float, default=0.02)
    parser.add_argument("--duration", type=float, help="stop after this many seconds")
    parser.add_argument("--idle-timeout", type=float, help="in surface mode, exit after the surface stops changing for this many seconds")
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--exit-at-eof", action="store_true", help="process current events then exit")
    args = parser.parse_args()

    client = LiveSplitClient(args.host, args.port, dry_run=args.dry_run)

    try:
        if args.source == "events":
            return follow(args.events, client, args.poll, args.exit_at_eof)

        if args.surface_file:
            reader = FileSurfaceReader(args.surface_file)
        else:
            pid = args.pid or find_pid_by_name(args.process)
            if not pid:
                print(f"bridge: no process matching '{args.process}'", file=sys.stderr)
                return 1
            reader = SurfaceReader(pid)

        return surface_loop(reader, client, args.poll, args.duration, args.idle_timeout)
    except KeyboardInterrupt:
        return 0
    finally:
        client.close()


if __name__ == "__main__":
    sys.exit(main())
