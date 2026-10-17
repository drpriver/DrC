#include "nc_capture_config.h"
#if defined(__APPLE__) && defined(__aarch64__)
#include "nc_capture_macos_arm64.s"
#elif defined(__APPLE__) && defined(__x86_64__)
#include "nc_capture_macos_x64.s"
#elif defined(__linux__) && defined(__aarch64__)
#include "nc_capture_linux_arm64.s"
#elif defined(__linux__) && defined(__x86_64__)
#include "nc_capture_linux_x64.s"
#elif defined(_WIN32) && defined(__x86_64__)
#include "nc_capture_windows_x64.s"
#else
#error Unsupported native capture platform or architecture
#endif
