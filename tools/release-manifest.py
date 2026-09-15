#!/usr/bin/env python3
"""Create or verify a CTR speedrun release manifest.

The manifest records the version, the git commit and the name, size and
SHA-256 of every release artifact. It is the allowlist a verifier trusts once
it is signed. Signing is a separate step so the private key never touches the
build host:

    minisign -Sm manifest.json      # Ed25519, key held out of band

Usage:
    release-manifest.py create --version 0.1.0 --commit abc123 --out manifest.json FILE...
    release-manifest.py verify manifest.json
"""

import argparse
import hashlib
import json
import os
import sys


def sha256_of(path):
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def command_create(args):
    artifacts = []
    for path in args.files:
        if not os.path.isfile(path):
            print(f"not a file: {path}", file=sys.stderr)
            return 1
        artifacts.append(
            {
                "name": os.path.basename(path),
                "size": os.path.getsize(path),
                "sha256": sha256_of(path),
            }
        )

    manifest = {
        "schema": 1,
        "version": args.version,
        "commit": args.commit,
        "artifacts": sorted(artifacts, key=lambda item: item["name"]),
    }

    with open(args.out, "w", encoding="utf-8") as handle:
        json.dump(manifest, handle, indent=2, sort_keys=True)
        handle.write("\n")

    print(f"wrote {args.out}: {len(artifacts)} artifacts")
    return 0


def command_verify(args):
    with open(args.manifest, encoding="utf-8") as handle:
        manifest = json.load(handle)

    base = os.path.dirname(os.path.abspath(args.manifest))
    failures = 0

    for artifact in manifest.get("artifacts", []):
        path = os.path.join(base, artifact["name"])
        if not os.path.isfile(path):
            print(f"MISSING  {artifact['name']}")
            failures += 1
            continue

        actual = sha256_of(path)
        if actual != artifact["sha256"]:
            print(f"MISMATCH {artifact['name']}")
            failures += 1
        else:
            print(f"ok       {artifact['name']}")

    print(f"{len(manifest.get('artifacts', []))} artifacts, {failures} failures")
    return 1 if failures else 0


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    subparsers = parser.add_subparsers(dest="command", required=True)

    create = subparsers.add_parser("create", help="write a manifest for the given files")
    create.add_argument("--version", required=True)
    create.add_argument("--commit", required=True)
    create.add_argument("--out", required=True)
    create.add_argument("files", nargs="+")
    create.set_defaults(func=command_create)

    verify = subparsers.add_parser("verify", help="verify files against a manifest")
    verify.add_argument("manifest")
    verify.set_defaults(func=command_verify)

    args = parser.parse_args()
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
