#ifndef NATIVE_BUILDID_H
#define NATIVE_BUILDID_H

#include <macros.h>

// Machine-readable build identity. Placed in its own named section so a
// verifier can read it without parsing stdout or hashing the whole binary.
// Section name: .ctrbid

#define NATIVE_BUILDID_MAGIC       0x43545242u // 'CTRB'
#define NATIVE_BUILDID_ABI_VERSION 1u

#define NATIVE_BUILDID_VERSION_MAX 24
#define NATIVE_BUILDID_COMMIT_MAX  24
#define NATIVE_BUILDID_BUILD_MAX   48

struct NativeBuildId
{
	u32 magic;
	u32 abiVersion;
	char version[NATIVE_BUILDID_VERSION_MAX]; // semver, for humans
	char commit[NATIVE_BUILDID_COMMIT_MAX];   // short git hash
	char buildId[NATIVE_BUILDID_BUILD_MAX];   // full build id string
};

#endif
