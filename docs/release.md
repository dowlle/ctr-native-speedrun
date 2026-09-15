# Release scheme

The speedrun client is its own product with its own version stream, independent of the upstream `beta-*` tags and the Archipelago `v0.2.x` line.

## Versioning

Semantic versioning, for example `0.1.0`. The embedded version is `CTR_NATIVE_VERSION`, and the embedded commit is the short git hash. Both are compiled into the binary.

## Build identity

Every `CTR_SPEEDRUN` build carries a `.ctrbid` section:

- magic `0x43545242` (`CTRB`)
- ABI version, currently `1`
- the semver string
- the short commit
- the full build id string

A verifier reads the section instead of parsing stdout or hashing the whole file.

## Reproducibility

- The dirty marker carries no wall-clock timestamp, so an unchanged tree hashes the same.
- Absolute source paths are stripped with `-ffile-prefix-map`, so the binary does not depend on where it was built.
- `SOURCE_DATE_EPOCH` defaults to the commit time when not already set.
- Toolchain and dependency versions must be pinned for a byte-identical rebuild. That is not yet enforced; until it is, the published artifact hash is the identity anchor, not a rebuild.

## Manifest and signing

`tools/release-manifest.py` records the version, the commit and the name, size and SHA-256 of every release artifact:

```
python3 tools/release-manifest.py create --version 0.1.0 --commit <hash> --out manifest.json <files...>
python3 tools/release-manifest.py verify manifest.json
```

Sign the manifest out of band with a key the maintainer holds, for example `minisign -Sm manifest.json`. The verifier pins only the public key and checks the client's reported identity against the signed manifest.

## Immutability

A released tag is never rebuilt or re-uploaded. A fix gets a new version.

## Not yet done

- Pinned toolchain and dependency versions for byte-identical rebuilds.
- A CI job that creates and verifies the manifest on every release.
- The verifier-side allowlist service.
