#ifndef DRP_RAPIDHASH_H
#define DRP_RAPIDHASH_H
/*
 * Derived from rapidhash.h.
 * Copyright (C) 2025 Nicolas De Carli
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
//
// Extensions by David Priver
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
// Modifications by David Priver are granted with the same license as above,
// or public domain, whatever is easier for you.
//
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#if defined _MSC_VER
#include <intrin.h>
#endif

#if !defined __clang__ && !defined _Null_unspecified
#define _Null_unspecified
#endif

#ifndef drp_memcpy_inline
    #if defined __has_builtin
        #if __has_builtin(__builtin_memcpy_inline)
            #define drp_memcpy_inline __builtin_memcpy_inline
        #elif __has_builtin(__builtin_memcpy)
            #define drp_memcpy_inline __builtin_memcpy
        #endif
    #endif
#endif
#if !defined drp_memcpy_inline && (defined __GNUC__ || defined __clang__)
    #define drp_memcpy_inline __builtin_memcpy
#endif
#ifndef drp_memcpy_inline
    #define drp_memcpy_inline memcpy
#endif

#ifndef drp_memcpy
    #if defined __has_builtin
        #if __has_builtin(__builtin_memcpy)
            #define drp_memcpy __builtin_memcpy
        #endif
    #endif
#endif
#if !defined drp_memcpy && (defined __GNUC__ || defined __clang__)
    #define drp_memcpy __builtin_memcpy
#endif
#ifndef drp_memcpy
    #define drp_memcpy memcpy
#endif

/* Capture both operands before writing outputs, which may alias the inputs. */
#if defined _MSC_VER && defined _M_X64 && !defined _M_ARM64EC
#pragma intrinsic(_umul128)
#define DRP_RAPIDHASH_MUL(A, B, LO, HI) do { \
    uint64_t lhs = (A), rhs = (B), high; \
    uint64_t low = _umul128(lhs, rhs, &high); \
    (LO) = low; (HI) = high; \
} while(0)
#elif defined _MSC_VER && !defined __SIZEOF_INT128__ && (defined _M_ARM64 || defined _M_ARM64EC)
#define DRP_RAPIDHASH_MUL(A, B, LO, HI) do { \
    uint64_t lhs = (A), rhs = (B); \
    (LO) = lhs * rhs; (HI) = __umulh(lhs, rhs); \
} while(0)
#elif defined __SIZEOF_INT128__
#define DRP_RAPIDHASH_MUL(A, B, LO, HI) do { \
    __uint128_t product = (__uint128_t)(A) * (B); \
    (LO) = (uint64_t)product; (HI) = (uint64_t)(product >> 64); \
} while(0)
#else
#define DRP_RAPIDHASH_MUL(A, B, LO, HI) do { \
    uint64_t lhs = (A), rhs = (B); \
    uint64_t ha = lhs >> 32, hb = rhs >> 32; \
    uint64_t la = (uint32_t)lhs, lb = (uint32_t)rhs; \
    uint64_t rh = ha * hb, rm0 = ha * lb, rm1 = hb * la, rl = la * lb; \
    uint64_t t = rl + (rm0 << 32), carry = t < rl; \
    uint64_t low = t + (rm1 << 32); \
    carry += low < t; \
    (LO) = low; (HI) = rh + (rm0 >> 32) + (rm1 >> 32) + carry; \
} while(0)
#endif

static inline
uint64_t
rapidhash_hash(const void *_Null_unspecified key, size_t len){
    const uint8_t* p = key;
    uint64_t seed = 0x422765567d8fbfd6ull;
    uint64_t a = 0, b = 0;
    size_t i = len;
    if(len <= 16){
        if(len >= 4){
            seed ^= len;
            if(len >= 8){
                drp_memcpy_inline(&a, p, 8);
                drp_memcpy_inline(&b, p + len - 8, 8);
            }
            else {
                drp_memcpy_inline(&a, p, 4);
                drp_memcpy_inline(&b, p + len - 4, 4);
            }
        }
        else if(len){
            a = ((uint64_t)p[0] << 45) | p[len-1];
            b = p[len >> 1];
        }
    }
    else {
        if(i > 112){
            uint64_t see1 = seed, see2 = seed, see3 = seed;
            uint64_t see4 = seed, see5 = seed, see6 = seed;
            do {
                {
                    uint64_t x, y;
                    drp_memcpy_inline(&x, p + 0, 8);
                    drp_memcpy_inline(&y, p + 8, 8);
                    DRP_RAPIDHASH_MUL(x ^ 0x2d358dccaa6c78a5ull, y ^ seed, x, y);
                    seed = x ^ y;
                }
                {
                    uint64_t x, y;
                    drp_memcpy_inline(&x, p + 16, 8);
                    drp_memcpy_inline(&y, p + 24, 8);
                    DRP_RAPIDHASH_MUL(x ^ 0x8bb84b93962eacc9ull, y ^ see1, x, y);
                    see1 = x ^ y;
                }
                {
                    uint64_t x, y;
                    drp_memcpy_inline(&x, p + 32, 8);
                    drp_memcpy_inline(&y, p + 40, 8);
                    DRP_RAPIDHASH_MUL(x ^ 0x4b33a62ed433d4a3ull, y ^ see2, x, y);
                    see2 = x ^ y;
                }
                {
                    uint64_t x, y;
                    drp_memcpy_inline(&x, p + 48, 8);
                    drp_memcpy_inline(&y, p + 56, 8);
                    DRP_RAPIDHASH_MUL(x ^ 0x4d5a2da51de1aa47ull, y ^ see3, x, y);
                    see3 = x ^ y;
                }
                {
                    uint64_t x, y;
                    drp_memcpy_inline(&x, p + 64, 8);
                    drp_memcpy_inline(&y, p + 72, 8);
                    DRP_RAPIDHASH_MUL(x ^ 0xa0761d6478bd642full, y ^ see4, x, y);
                    see4 = x ^ y;
                }
                {
                    uint64_t x, y;
                    drp_memcpy_inline(&x, p + 80, 8);
                    drp_memcpy_inline(&y, p + 88, 8);
                    DRP_RAPIDHASH_MUL(x ^ 0xe7037ed1a0b428dbull, y ^ see5, x, y);
                    see5 = x ^ y;
                }
                {
                    uint64_t x, y;
                    drp_memcpy_inline(&x, p + 96, 8);
                    drp_memcpy_inline(&y, p + 104, 8);
                    DRP_RAPIDHASH_MUL(x ^ 0x90ed1765281c388cull, y ^ see6, x, y);
                    see6 = x ^ y;
                }
                p += 112;
                i -= 112;
            } while(i > 112);
            seed ^= see1 ^ see2 ^ see3 ^ see4 ^ see5 ^ see6;
        }
        if(i > 16){
            uint64_t x, y;
            drp_memcpy_inline(&x, p + 0, 8);
            drp_memcpy_inline(&y, p + 8, 8);
            DRP_RAPIDHASH_MUL(x ^ 0x4b33a62ed433d4a3ull, y ^ seed, x, y);
            seed = x ^ y;
        }
        if(i > 32){
            uint64_t x, y;
            drp_memcpy_inline(&x, p + 16, 8);
            drp_memcpy_inline(&y, p + 24, 8);
            DRP_RAPIDHASH_MUL(x ^ 0x4b33a62ed433d4a3ull, y ^ seed, x, y);
            seed = x ^ y;
        }
        if(i > 48){
            uint64_t x, y;
            drp_memcpy_inline(&x, p + 32, 8);
            drp_memcpy_inline(&y, p + 40, 8);
            DRP_RAPIDHASH_MUL(x ^ 0x8bb84b93962eacc9ull, y ^ seed, x, y);
            seed = x ^ y;
        }
        if(i > 64){
            uint64_t x, y;
            drp_memcpy_inline(&x, p + 48, 8);
            drp_memcpy_inline(&y, p + 56, 8);
            DRP_RAPIDHASH_MUL(x ^ 0x8bb84b93962eacc9ull, y ^ seed, x, y);
            seed = x ^ y;
        }
        if(i > 80){
            uint64_t x, y;
            drp_memcpy_inline(&x, p + 64, 8);
            drp_memcpy_inline(&y, p + 72, 8);
            DRP_RAPIDHASH_MUL(x ^ 0x4b33a62ed433d4a3ull, y ^ seed, x, y);
            seed = x ^ y;
        }
        if(i > 96){
            uint64_t x, y;
            drp_memcpy_inline(&x, p + 80, 8);
            drp_memcpy_inline(&y, p + 88, 8);
            DRP_RAPIDHASH_MUL(x ^ 0x8bb84b93962eacc9ull, y ^ seed, x, y);
            seed = x ^ y;
        }
        drp_memcpy_inline(&a, p + i - 16, 8);
        a ^= i;
        drp_memcpy_inline(&b, p + i - 8, 8);
    }
    DRP_RAPIDHASH_MUL(a ^ 0x8bb84b93962eacc9ull, b ^ seed, a, b);
    DRP_RAPIDHASH_MUL(a ^ 0xaaaaaaaaaaaaaaaaull, b ^ 0x8bb84b93962eacc9ull ^ i, a, b);
    return a ^ b;
}
static inline
uint64_t
rapidhash_hash_ascii_insensitive(const void *_Null_unspecified key, size_t len){
    const uint8_t* p = key;
    uint64_t seed = 0x422765567d8fbfd6ull;
    uint64_t a = 0, b = 0;
    size_t i = len;
    if(len <= 16){
        if(len >= 4){
            seed ^= len;
            if(len >= 8){
                drp_memcpy_inline(&a, p, 8);
                a |= 0x2020202020202020ull;
                drp_memcpy_inline(&b, p + len - 8, 8);
                b |= 0x2020202020202020ull;
            }
            else {
                drp_memcpy_inline(&a, p, 4);
                a |= 0x20202020ull;
                drp_memcpy_inline(&b, p + len - 4, 4);
                b |= 0x20202020ull;
            }
        }
        else if(len){
            a = ((uint64_t)(p[0] | 0x20u) << 45) | (p[len-1] | 0x20u);
            b = p[len >> 1] | 0x20u;
        }
    }
    else {
        if(i > 112){
            uint64_t see1 = seed, see2 = seed, see3 = seed;
            uint64_t see4 = seed, see5 = seed, see6 = seed;
            do {
                {
                    uint64_t x, y;
                    drp_memcpy_inline(&x, p + 0, 8);
                    x |= 0x2020202020202020ull;
                    drp_memcpy_inline(&y, p + 8, 8);
                    y |= 0x2020202020202020ull;
                    DRP_RAPIDHASH_MUL(x ^ 0x2d358dccaa6c78a5ull, y ^ seed, x, y);
                    seed = x ^ y;
                }
                {
                    uint64_t x, y;
                    drp_memcpy_inline(&x, p + 16, 8);
                    x |= 0x2020202020202020ull;
                    drp_memcpy_inline(&y, p + 24, 8);
                    y |= 0x2020202020202020ull;
                    DRP_RAPIDHASH_MUL(x ^ 0x8bb84b93962eacc9ull, y ^ see1, x, y);
                    see1 = x ^ y;
                }
                {
                    uint64_t x, y;
                    drp_memcpy_inline(&x, p + 32, 8);
                    x |= 0x2020202020202020ull;
                    drp_memcpy_inline(&y, p + 40, 8);
                    y |= 0x2020202020202020ull;
                    DRP_RAPIDHASH_MUL(x ^ 0x4b33a62ed433d4a3ull, y ^ see2, x, y);
                    see2 = x ^ y;
                }
                {
                    uint64_t x, y;
                    drp_memcpy_inline(&x, p + 48, 8);
                    x |= 0x2020202020202020ull;
                    drp_memcpy_inline(&y, p + 56, 8);
                    y |= 0x2020202020202020ull;
                    DRP_RAPIDHASH_MUL(x ^ 0x4d5a2da51de1aa47ull, y ^ see3, x, y);
                    see3 = x ^ y;
                }
                {
                    uint64_t x, y;
                    drp_memcpy_inline(&x, p + 64, 8);
                    x |= 0x2020202020202020ull;
                    drp_memcpy_inline(&y, p + 72, 8);
                    y |= 0x2020202020202020ull;
                    DRP_RAPIDHASH_MUL(x ^ 0xa0761d6478bd642full, y ^ see4, x, y);
                    see4 = x ^ y;
                }
                {
                    uint64_t x, y;
                    drp_memcpy_inline(&x, p + 80, 8);
                    x |= 0x2020202020202020ull;
                    drp_memcpy_inline(&y, p + 88, 8);
                    y |= 0x2020202020202020ull;
                    DRP_RAPIDHASH_MUL(x ^ 0xe7037ed1a0b428dbull, y ^ see5, x, y);
                    see5 = x ^ y;
                }
                {
                    uint64_t x, y;
                    drp_memcpy_inline(&x, p + 96, 8);
                    x |= 0x2020202020202020ull;
                    drp_memcpy_inline(&y, p + 104, 8);
                    y |= 0x2020202020202020ull;
                    DRP_RAPIDHASH_MUL(x ^ 0x90ed1765281c388cull, y ^ see6, x, y);
                    see6 = x ^ y;
                }
                p += 112;
                i -= 112;
            } while(i > 112);
            seed ^= see1 ^ see2 ^ see3 ^ see4 ^ see5 ^ see6;
        }
        if(i > 16){
            uint64_t x, y;
            drp_memcpy_inline(&x, p + 0, 8);
            x |= 0x2020202020202020ull;
            drp_memcpy_inline(&y, p + 8, 8);
            y |= 0x2020202020202020ull;
            DRP_RAPIDHASH_MUL(x ^ 0x4b33a62ed433d4a3ull, y ^ seed, x, y);
            seed = x ^ y;
        }
        if(i > 32){
            uint64_t x, y;
            drp_memcpy_inline(&x, p + 16, 8);
            x |= 0x2020202020202020ull;
            drp_memcpy_inline(&y, p + 24, 8);
            y |= 0x2020202020202020ull;
            DRP_RAPIDHASH_MUL(x ^ 0x4b33a62ed433d4a3ull, y ^ seed, x, y);
            seed = x ^ y;
        }
        if(i > 48){
            uint64_t x, y;
            drp_memcpy_inline(&x, p + 32, 8);
            x |= 0x2020202020202020ull;
            drp_memcpy_inline(&y, p + 40, 8);
            y |= 0x2020202020202020ull;
            DRP_RAPIDHASH_MUL(x ^ 0x8bb84b93962eacc9ull, y ^ seed, x, y);
            seed = x ^ y;
        }
        if(i > 64){
            uint64_t x, y;
            drp_memcpy_inline(&x, p + 48, 8);
            x |= 0x2020202020202020ull;
            drp_memcpy_inline(&y, p + 56, 8);
            y |= 0x2020202020202020ull;
            DRP_RAPIDHASH_MUL(x ^ 0x8bb84b93962eacc9ull, y ^ seed, x, y);
            seed = x ^ y;
        }
        if(i > 80){
            uint64_t x, y;
            drp_memcpy_inline(&x, p + 64, 8);
            x |= 0x2020202020202020ull;
            drp_memcpy_inline(&y, p + 72, 8);
            y |= 0x2020202020202020ull;
            DRP_RAPIDHASH_MUL(x ^ 0x4b33a62ed433d4a3ull, y ^ seed, x, y);
            seed = x ^ y;
        }
        if(i > 96){
            uint64_t x, y;
            drp_memcpy_inline(&x, p + 80, 8);
            x |= 0x2020202020202020ull;
            drp_memcpy_inline(&y, p + 88, 8);
            y |= 0x2020202020202020ull;
            DRP_RAPIDHASH_MUL(x ^ 0x8bb84b93962eacc9ull, y ^ seed, x, y);
            seed = x ^ y;
        }
        drp_memcpy_inline(&a, p + i - 16, 8);
        a |= 0x2020202020202020ull;
        a ^= i;
        drp_memcpy_inline(&b, p + i - 8, 8);
        b |= 0x2020202020202020ull;
    }
    DRP_RAPIDHASH_MUL(a ^ 0x8bb84b93962eacc9ull, b ^ seed, a, b);
    DRP_RAPIDHASH_MUL(a ^ 0xaaaaaaaaaaaaaaaaull, b ^ 0x8bb84b93962eacc9ull ^ i, a, b);
    return a ^ b;
}

// Hash as if the input were zero-padded to a multiple of 16 bytes.
static inline
uint64_t
rapidhash_hash_zero_padded(const void *_Null_unspecified key, size_t actual_len){
    const uint8_t* p = key;
    uint64_t seed = 0x422765567d8fbfd6ull;
    uint64_t a = 0, b = 0;
    size_t len = (actual_len + 15) & ~(size_t)15;
    size_t i = len;
    if(len <= 16){
        seed ^= len;
        if(actual_len){
            size_t first = actual_len < 8 ? actual_len : 8;
            drp_memcpy(&a, p, first);
            if(actual_len > 8) drp_memcpy(&b, p + 8, actual_len - 8);
        }
    }
    else {
        if(i > 112){
            uint64_t see1 = seed, see2 = seed, see3 = seed;
            uint64_t see4 = seed, see5 = seed, see6 = seed;
            do {
                {
                    uint64_t x, y;
                    drp_memcpy_inline(&x, p + 0, 8);
                    drp_memcpy_inline(&y, p + 8, 8);
                    DRP_RAPIDHASH_MUL(x ^ 0x2d358dccaa6c78a5ull, y ^ seed, x, y);
                    seed = x ^ y;
                }
                {
                    uint64_t x, y;
                    drp_memcpy_inline(&x, p + 16, 8);
                    drp_memcpy_inline(&y, p + 24, 8);
                    DRP_RAPIDHASH_MUL(x ^ 0x8bb84b93962eacc9ull, y ^ see1, x, y);
                    see1 = x ^ y;
                }
                {
                    uint64_t x, y;
                    drp_memcpy_inline(&x, p + 32, 8);
                    drp_memcpy_inline(&y, p + 40, 8);
                    DRP_RAPIDHASH_MUL(x ^ 0x4b33a62ed433d4a3ull, y ^ see2, x, y);
                    see2 = x ^ y;
                }
                {
                    uint64_t x, y;
                    drp_memcpy_inline(&x, p + 48, 8);
                    drp_memcpy_inline(&y, p + 56, 8);
                    DRP_RAPIDHASH_MUL(x ^ 0x4d5a2da51de1aa47ull, y ^ see3, x, y);
                    see3 = x ^ y;
                }
                {
                    uint64_t x, y;
                    drp_memcpy_inline(&x, p + 64, 8);
                    drp_memcpy_inline(&y, p + 72, 8);
                    DRP_RAPIDHASH_MUL(x ^ 0xa0761d6478bd642full, y ^ see4, x, y);
                    see4 = x ^ y;
                }
                {
                    uint64_t x, y;
                    drp_memcpy_inline(&x, p + 80, 8);
                    drp_memcpy_inline(&y, p + 88, 8);
                    DRP_RAPIDHASH_MUL(x ^ 0xe7037ed1a0b428dbull, y ^ see5, x, y);
                    see5 = x ^ y;
                }
                {
                    uint64_t x, y;
                    drp_memcpy_inline(&x, p + 96, 8);
                    drp_memcpy_inline(&y, p + 104, 8);
                    DRP_RAPIDHASH_MUL(x ^ 0x90ed1765281c388cull, y ^ see6, x, y);
                    see6 = x ^ y;
                }
                p += 112;
                i -= 112;
            } while(i > 112);
            seed ^= see1 ^ see2 ^ see3 ^ see4 ^ see5 ^ see6;
        }
        if(i > 16){
            uint64_t x, y;
            drp_memcpy_inline(&x, p + 0, 8);
            drp_memcpy_inline(&y, p + 8, 8);
            DRP_RAPIDHASH_MUL(x ^ 0x4b33a62ed433d4a3ull, y ^ seed, x, y);
            seed = x ^ y;
        }
        if(i > 32){
            uint64_t x, y;
            drp_memcpy_inline(&x, p + 16, 8);
            drp_memcpy_inline(&y, p + 24, 8);
            DRP_RAPIDHASH_MUL(x ^ 0x4b33a62ed433d4a3ull, y ^ seed, x, y);
            seed = x ^ y;
        }
        if(i > 48){
            uint64_t x, y;
            drp_memcpy_inline(&x, p + 32, 8);
            drp_memcpy_inline(&y, p + 40, 8);
            DRP_RAPIDHASH_MUL(x ^ 0x8bb84b93962eacc9ull, y ^ seed, x, y);
            seed = x ^ y;
        }
        if(i > 64){
            uint64_t x, y;
            drp_memcpy_inline(&x, p + 48, 8);
            drp_memcpy_inline(&y, p + 56, 8);
            DRP_RAPIDHASH_MUL(x ^ 0x8bb84b93962eacc9ull, y ^ seed, x, y);
            seed = x ^ y;
        }
        if(i > 80){
            uint64_t x, y;
            drp_memcpy_inline(&x, p + 64, 8);
            drp_memcpy_inline(&y, p + 72, 8);
            DRP_RAPIDHASH_MUL(x ^ 0x4b33a62ed433d4a3ull, y ^ seed, x, y);
            seed = x ^ y;
        }
        if(i > 96){
            uint64_t x, y;
            drp_memcpy_inline(&x, p + 80, 8);
            drp_memcpy_inline(&y, p + 88, 8);
            DRP_RAPIDHASH_MUL(x ^ 0x8bb84b93962eacc9ull, y ^ seed, x, y);
            seed = x ^ y;
        }
        // Only the final 16-byte block can include implicit padding.
        uint8_t tail[16] = {0};
        drp_memcpy(tail, (const uint8_t*)key + len - 16, actual_len - (len - 16));
        drp_memcpy_inline(&a, tail, 8);
        a ^= i;
        drp_memcpy_inline(&b, tail + 8, 8);
    }
    DRP_RAPIDHASH_MUL(a ^ 0x8bb84b93962eacc9ull, b ^ seed, a, b);
    DRP_RAPIDHASH_MUL(a ^ 0xaaaaaaaaaaaaaaaaull, b ^ 0x8bb84b93962eacc9ull ^ i, a, b);
    return a ^ b;
}
#undef DRP_RAPIDHASH_MUL
#endif
