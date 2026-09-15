// Build identity, exposed in a fixed-layout section rather than only through
// --version. Gated behind CTR_SPEEDRUN so the default build is unchanged.

#include <platform/native_buildid.h>

#ifndef CTR_NATIVE_VERSION
#define CTR_NATIVE_VERSION "0.0.0-dev"
#endif

#ifndef CTR_NATIVE_COMMIT
#define CTR_NATIVE_COMMIT "unknown"
#endif

#ifndef CTR_NATIVE_BUILD_ID
#define CTR_NATIVE_BUILD_ID "unknown"
#endif

__attribute__((section(".ctrbid"), used)) global_variable struct NativeBuildId g_ctr_build_id = {
    .magic = NATIVE_BUILDID_MAGIC,
    .abiVersion = NATIVE_BUILDID_ABI_VERSION,
    .version = CTR_NATIVE_VERSION,
    .commit = CTR_NATIVE_COMMIT,
    .buildId = CTR_NATIVE_BUILD_ID,
};
