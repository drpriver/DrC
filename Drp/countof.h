#ifndef DRP_COUNTOF_H
#define DRP_COUNTOF_H
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#if !defined countof && defined __STDC_VERSION__ && __STDC_VERSION__ >= 202500L
#define countof _Countof
#endif

#if !defined countof && defined __has_feature  
#if __has_feature(c_countof)
#define countof _Countof
#endif
#endif

#ifndef countof
#define countof(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif

#endif
