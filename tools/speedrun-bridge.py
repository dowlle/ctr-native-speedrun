#!/usr/bin/env python3
"""Companion bridge: drive LiveSplit from the client's run event log.

The client always writes speedrun-events.log. This bridge tails that file and
translates events into LiveSplit Server commands over TCP, so LiveSplit shows
the client's loadless time and splits.

LiveSplit Server must be enabled in LiveSplit (Layout Settings, Add, Control,
LiveSplit Server). Default port 16834. Protocol reference:
src/LiveSplit.Core/Server/CommandServer.cs.

Usage:
    speedrun-bridge.py --events speedrun-events.log --port 16834
    speedrun-bridge.py --events speedrun-events.log --dry-run
"""

import argparse
import os
import socket
import sys
import time


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


def commands_for_event(fields):
    """Maps one event to the LiveSplit Server commands it should trigger."""
    event_type = fields.get("type")
    total = fields.get("loadless_ms", "0")

    if event_type == "run_start":
        return ["reset", "setgametime " + format_gametime(0), "starttimer"]

    if event_type == "split":
        return ["setgametime " + format_gametime(total), "split"]

    if event_type == "run_end":
        return ["setgametime " + format_gametime(total), "split"]

    if event_type == "reset":
        return ["reset"]

    return []


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
                commands = commands_for_event(parse_event_line(line))
                for command in commands:
                    client.send(command)
            offset = handle.tell()

        if exit_at_eof:
            return 0

        time.sleep(poll)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--events", default="speedrun-events.log")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=16834)
    parser.add_argument("--poll", type=float, default=0.05)
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--exit-at-eof", action="store_true", help="process current events then exit")
    args = parser.parse_args()

    client = LiveSplitClient(args.host, args.port, dry_run=args.dry_run)
    try:
        return follow(args.events, client, args.poll, args.exit_at_eof)
    except KeyboardInterrupt:
        return 0
    finally:
        client.close()


if __name__ == "__main__":
    sys.exit(main())
