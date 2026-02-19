//
// Copyright © 2021-2024, David Priver <david@davidpriver.com>
//
#ifndef STRING_DISTANCES_H
#define STRING_DISTANCES_H

// size_t
#include <stddef.h>

#ifdef _WIN32
typedef long long ssize_t;
#else
// ssize_t
#include <sys/types.h>
#endif

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

//
// byte_expansion_distance
// -----------------
// Calculates the number of insertions necessary to make needle equal to haystack.
//
// Arguments:
// ----------
//   haystack: pointer to larger string
//   haystack_len: length of haystack
//   needle: pointer to needle string
//   needle_len: length of needle
//
// Returns:
// --------
// The number of insertions necessary to make needle equal to haystack.
// Returns -1 if the input is invalid or if it is impossible.
//
// This function can return -1 in the following circumstances:
//    1. Needle is longer than haystack.
//    2. Needle contains characters not in haystack.
//    3. It is impossible to make them match.
//

static inline
ssize_t
byte_expansion_distance(const char* haystack, size_t haystack_len, const char* needle, size_t needle_len){
    ssize_t difference = 0;
    for(;;){
        if(needle_len > haystack_len) return -1;
        if(!needle_len) {
            difference += haystack_len;
            return difference;
        }
        // Strip off the leading extent that matches
        for(;;){
            if(!needle_len) {
                difference += haystack_len;
                return difference;
            }
            if(!haystack_len) return -1;
            if(*haystack == *needle){
                haystack++; needle++; haystack_len--; needle_len--;
                continue;
            }
            break;
        }
        // Strip off haystack until we find a match, counting each one.
        for(;;){
            if(!haystack_len) return -1;
            if(*haystack == *needle) break;
            difference++; haystack++; haystack_len--;
        }
        // First character now matches. back to top
    }
    return difference;
}

static inline
ssize_t
byte_expansion_distance_icase(const char* haystack, size_t haystack_len, const char* needle, size_t needle_len){
    ssize_t difference = 0;
    for(;;){
        if(needle_len > haystack_len) return -1;
        if(!needle_len) {
            difference += haystack_len;
            return difference;
        }
        // Strip off the leading extent that matches
        for(;;){
            if(!needle_len) {
                difference += haystack_len;
                return difference;
            }
            if(!haystack_len) return -1;
            if((*haystack | 0x20) == (*needle | 0x20)){
                haystack++; needle++; haystack_len--; needle_len--;
                continue;
            }
            break;
        }
        // Strip off haystack until we find a match, counting each one.
        for(;;){
            if(!haystack_len) return -1;
            if((*haystack|0x20) == (*needle|0x20)) break;
            difference++; haystack++; haystack_len--;
        }
        // First character now matches. back to top
    }
    return difference;
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
