#ifndef NC_CAPTURE_CONFIG_H
#define NC_CAPTURE_CONFIG_H
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//


// Shared by C and preprocessed assembly.
#define NC_CAPTURE_X64_TABLE_SHIFT 12
#define NC_CAPTURE_X64_TABLE_SIZE (1 << NC_CAPTURE_X64_TABLE_SHIFT)
#if defined(__linux__) && defined(__aarch64__)
#define NC_CAPTURE_TABLE_SHIFT 16
#elif defined(__APPLE__) && defined(__aarch64__)
#define NC_CAPTURE_TABLE_SHIFT 14
#else
#define NC_CAPTURE_TABLE_SHIFT NC_CAPTURE_X64_TABLE_SHIFT
#endif
#define NC_CAPTURE_TABLE_SIZE (1 << NC_CAPTURE_TABLE_SHIFT)
#define NC_CAPTURE_SLOT_COUNT (NC_CAPTURE_TABLE_SIZE / 32)

#endif
