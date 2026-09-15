#!/usr/bin/env python3
"""Tests for the LiveSplit companion bridge.

Covers the pure mappings plus an end-to-end run against a stub LiveSplit Server
speaking the real newline protocol.
"""

import importlib.util
import os
import socket
import subprocess
import sys
import tempfile
import threading

BRIDGE_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "speedrun-bridge.py")

spec = importlib.util.spec_from_file_location("speedrun_bridge", BRIDGE_PATH)
bridge = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bridge)

checks = 0
failures = 0


def check(condition, label):
    global checks, failures
    checks += 1
    if not condition:
        failures += 1
        print(f"FAIL: {label}")


def test_format_gametime():
    check(bridge.format_gametime(0) == "0:00:00.000", "zero time")
    check(bridge.format_gametime(128) == "0:00:00.128", "128 ms")
    check(bridge.format_gametime(48 * 60 * 1000 + 500) == "0:48:00.500", "48 minutes")
    check(bridge.format_gametime(3661000) == "1:01:01.000", "one hour one minute one second")


def test_parse_event_line():
    fields = bridge.parse_event_line("seq=4 type=split level=3 mode=524288 segment=0 pos=1 seg_ms=128 total_ms=128 loadless_ms=128 rta_ms=130\n")
    check(fields.get("type") == "split", "type parsed")
    check(fields.get("loadless_ms") == "128", "loadless parsed")
    check(fields.get("level") == "3", "level parsed")
    check(bridge.parse_event_line("garbage line") == {}, "garbage rejected")
    check(bridge.parse_event_line("") == {}, "empty rejected")


def test_commands_for_event():
    start = bridge.commands_for_event({"type": "run_start", "loadless_ms": "0"})
    check(start == ["reset", "switchto gametime", "setgametime 0:00:00.000", "starttimer"], "run start commands")

    split = bridge.commands_for_event({"type": "split", "loadless_ms": "128"})
    check(split == ["setgametime 0:00:00.128", "split"], "split commands")

    end = bridge.commands_for_event({"type": "run_end", "loadless_ms": "2880000"})
    check(end == ["setgametime 0:48:00.000", "split"], "run end commands")

    check(bridge.commands_for_event({"type": "reset"}) == ["reset"], "reset commands")
    check(bridge.commands_for_event({"type": "none"}) == [], "unknown event is ignored")


class StubServer:
    """Accepts one TCP connection and records every newline-delimited command."""

    def __init__(self):
        self.sock = socket.socket()
        self.sock.bind(("127.0.0.1", 0))
        self.sock.listen(1)
        self.port = self.sock.getsockname()[1]
        self.received = []
        self.thread = threading.Thread(target=self._serve, daemon=True)
        self.thread.start()

    def _serve(self):
        conn, _ = self.sock.accept()
        with conn:
            data = b""
            while True:
                chunk = conn.recv(4096)
                if not chunk:
                    break
                data += chunk
            self.received.extend(data.decode("ascii").splitlines())
        self.sock.close()

    def join(self):
        self.thread.join(timeout=5)


def test_end_to_end():
    server = StubServer()

    log = tempfile.NamedTemporaryFile("w", suffix=".log", delete=False)
    log.write("seq=1 type=run_start level=26 mode=524288 segment=0 pos=0 seg_ms=0 total_ms=0 loadless_ms=0 rta_ms=0\n")
    log.write("seq=2 type=split level=3 mode=524288 segment=0 pos=1 seg_ms=128 total_ms=128 loadless_ms=128 rta_ms=130\n")
    log.write("seq=3 type=run_end level=13 mode=2148007936 segment=20 pos=1 seg_ms=64 total_ms=2880000 loadless_ms=2880000 rta_ms=2900000\n")
    log.close()

    client = bridge.LiveSplitClient("127.0.0.1", server.port)
    bridge.follow(log.name, client, poll=0.01, exit_at_eof=True)
    client.close()
    server.join()
    os.unlink(log.name)

    expected = [
        "reset",
        "switchto gametime",
        "setgametime 0:00:00.000",
        "starttimer",
        "setgametime 0:00:00.128",
        "split",
        "setgametime 0:48:00.000",
        "split",
    ]
    check(server.received == expected, f"end to end commands were {server.received}")


def test_surface_integration(fixture_path):
    if not fixture_path or not os.path.exists(fixture_path):
        print("SKIP: surface integration (no fixture)")
        return

    surface_file = tempfile.NamedTemporaryFile(suffix=".bin", delete=False)
    surface_file.close()

    server = StubServer()
    fixture = subprocess.Popen([fixture_path, surface_file.name], stdout=subprocess.DEVNULL)
    try:
        subprocess.run(
            [
                sys.executable,
                BRIDGE_PATH,
                "--source",
                "surface",
                "--surface-file",
                surface_file.name,
                "--port",
                str(server.port),
                "--duration",
                "2.0",
                "--poll",
                "0.02",
            ],
            timeout=20,
            check=False,
        )
    finally:
        fixture.wait(timeout=10)
        os.unlink(surface_file.name)

    server.join()
    received = server.received

    check("reset" in received, "surface: reset sent on run start")
    check("starttimer" in received, "surface: starttimer sent on run start")
    check(sum(1 for command in received if command.startswith("setgametime")) >= 2, "surface: game time polled")
    check(received.count("split") == 2, f"surface: two splits, got {received.count('split')}")


def main():
    fixture_path = None
    if len(sys.argv) > 2 and sys.argv[1] == "--fixture":
        fixture_path = sys.argv[2]

    test_format_gametime()
    test_parse_event_line()
    test_commands_for_event()
    test_end_to_end()
    test_surface_integration(fixture_path)
    print(f"speedrun_bridge: {checks} checks, {failures} failures")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
