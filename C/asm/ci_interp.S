// Copyright © 2026-2026, David Priver <david@davidpriver.com>
#if   defined __APPLE__ && defined __aarch64__ && __SIZEOF_POINTER__ == 8 && !defined __AARCH64EB__ && !defined __arm64e__
#include "ci_interp_aarch64_darwin.s"
#elif defined __linux__ && defined __aarch64__ && __SIZEOF_POINTER__ == 8 && !defined __AARCH64EB__ && !defined __arm64e__
#include "ci_interp_aarch64_linux.s"
#elif defined __x86_64__ && defined _WIN32
#include "ci_interp_x86_64_windows.s"
#elif defined __x86_64__
#include "ci_interp_x86_64_sysv.s"
#else
#error Assembly interpreter is not implemented for this platform or architecture
#endif
