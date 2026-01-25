//
// Copyright © 2021-2025, David Priver <david@davidpriver.com>
//
#ifndef BASE64_H
#define BASE64_H
#include <stddef.h>
#include <stdint.h>
#include <assert.h>

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

#ifndef unreachable
#if defined(__GNUC__) || defined(__clang__)
#define unreachable() __builtin_unreachable()
#else
#define unreachable() __assume(0)
#endif
#endif

#ifndef warn_unused
#if defined(__GNUC__) || defined(__clang__)
#define warn_unused __attribute__((warn_unused_result))
#elif defined(_MSC_VER)
#define warn_unused
#else
#define warn_unused
#endif
#endif

#if !defined(likely) && !defined(unlikely)
#if defined(__GNUC__) || defined(__clang__)
#define likely(x)      __builtin_expect(!!(x), 1)
#define unlikely(x)    __builtin_expect(!!(x), 0)
#else
#define likely(x) (x)
#define unlikely(x) (x)
#endif
#endif

typedef enum Base64Error{
    BASE64_NO_ERROR = 0,
    BASE64_DECODING_ERROR = 1,
    BASE64_WOULD_OVERFLOW = 2,
} Base64Error;

// The size needed to encode a data buffer into a string.
static inline
size_t
base64_encode_size(size_t src_length){
    size_t size_needed = src_length * 4;
    size_needed = size_needed / 3 + !!(size_needed % 3);
    return size_needed;
}

// The size needed to decode a str into a buffer.
static inline
size_t
base64_decode_size(size_t src_length){
    size_t size_needed = src_length * 3;
    size_needed = size_needed / 4 + !!(size_needed % 4);
    return size_needed;
}

// Base64 encodes the data into the output string.
// This uses '+' and '/' and does not pad the end with '='.
// Some implementations don't like that, but they are wrong!
//
// Returns the amount of space actually used for encoding.
static inline
size_t
base64_encode(char* restrict dst, size_t dst_length, const void* restrict src, size_t src_length){
    static const char  base64_encode_table0[256] =
        "AAAABBBBCC"
        "CCDDDDEEEE"
        "FFFFGGGGHH"
        "HHIIIIJJJJ"
        "KKKKLLLLMM"
        "MMNNNNOOOO"
        "PPPPQQQQRR"
        "RRSSSSTTTT"
        "UUUUVVVVWW"
        "WWXXXXYYYY"
        "ZZZZaaaabb"
        "bbccccdddd"
        "eeeeffffgg"
        "gghhhhiiii"
        "jjjjkkkkll"
        "llmmmmnnnn"
        "ooooppppqq"
        "qqrrrrssss"
        "ttttuuuuvv"
        "vvwwwwxxxx"
        "yyyyzzzz00"
        "0011112222"
        "3333444455"
        "5566667777"
        "88889999++"
        "++////";
    static const char base64_encode_table1[256] =
        "ABCDEFGHIJ"
        "KLMNOPQRST"
        "UVWXYZabcd"
        "efghijklmn"
        "opqrstuvwx"
        "yz01234567"
        "89+/ABCDEF"
        "GHIJKLMNOP"
        "QRSTUVWXYZ"
        "abcdefghij"
        "klmnopqrst"
        "uvwxyz0123"
        "456789+/AB"
        "CDEFGHIJKL"
        "MNOPQRSTUV"
        "WXYZabcdef"
        "ghijklmnop"
        "qrstuvwxyz"
        "0123456789"
        "+/ABCDEFGH"
        "IJKLMNOPQR"
        "STUVWXYZab"
        "cdefghijkl"
        "mnopqrstuv"
        "wxyz012345"
        "6789+/";
    static const char base64_encode_table2[256] =
        "ABCDEFGHIJ"
        "KLMNOPQRST"
        "UVWXYZabcd"
        "efghijklmn"
        "opqrstuvwx"
        "yz01234567"
        "89+/ABCDEF"
        "GHIJKLMNOP"
        "QRSTUVWXYZ"
        "abcdefghij"
        "klmnopqrst"
        "uvwxyz0123"
        "456789+/AB"
        "CDEFGHIJKL"
        "MNOPQRSTUV"
        "WXYZabcdef"
        "ghijklmnop"
        "qrstuvwxyz"
        "0123456789"
        "+/ABCDEFGH"
        "IJKLMNOPQR"
        "STUVWXYZab"
        "cdefghijkl"
        "mnopqrstuv"
        "wxyz012345"
        "6789+/";


    size_t i = 0;
    uint8_t* d = (uint8_t*) dst;
    const uint8_t* str = src;
    size_t len = src_length;

    // unsigned here is important!
    // But, note that we still need to cast before a shift as unsigned char
    // gets promoted to int.
    uint8_t t1, t2, t3, t4, t5, t6;

    if(len > 5) {
        for(; i < len - 5; i += 6) {
            t1 = str[i];
            t2 = str[i+1];
            t3 = str[i+2];
            t4 = str[i+3];
            t5 = str[i+4];
            t6 = str[i+5];
            *d++ = base64_encode_table0[t1];
            unsigned index1 = ((t1 & 0x3u) << 4) | (((unsigned)t2 >> 4) & 0xfu);
            *d++ = base64_encode_table1[index1];
            unsigned index2 = ((t2 & 0xfu) << 2) | (((unsigned)t3 >> 6) & 0x3u);
            *d++ = base64_encode_table1[index2];
            *d++ = base64_encode_table2[t3];

            *d++ = base64_encode_table0[t4];
            unsigned index3 = ((t4 & 0x3u) << 4) | (((unsigned)t5 >> 4) & 0xfu);
            *d++ = base64_encode_table1[index3];
            unsigned index4 = ((t5 & 0xfu) << 2) | (((unsigned)t6 >> 6) & 0x3u);
            *d++ = base64_encode_table1[index4];
            *d++ = base64_encode_table2[t6];
        }
    }
    if(i < len -2){
        t1 = str[i];
        t2 = str[i+1];
        t3 = str[i+2];
        *d++ = base64_encode_table0[t1];
        unsigned index1 = ((t1 & 0x3u) << 4) | (((unsigned)t2 >> 4) & 0xfu);
        *d++ = base64_encode_table1[index1];
        unsigned index2 = ((t2 & 0xfu) << 2) | (((unsigned)t3 >> 6) & 0x3u);
        *d++ = base64_encode_table1[index2];
        *d++ = base64_encode_table2[t3];
        i += 3;
    }

    switch(len - i) {
        case 0:
            break;
        case 1:
            t1 = str[i];
            *d++ = base64_encode_table0[t1];
            *d++ = base64_encode_table1[(t1 & 0x3u) << 4];
            break;
        default: // case 2
            t1 = str[i]; t2 = str[i+1];
            *d++ = base64_encode_table0[t1];
            *d++ = base64_encode_table1[((t1 & 0x3u) << 4) | (((unsigned)t2 >> 4) & 0xfu)];
            *d++ = base64_encode_table2[(t2 & 0xfu) << 2];
    }
    ptrdiff_t used_length = d - (uint8_t*)dst;
    (void)dst_length;
    // assert(used_length == (ptrdiff_t)dst_length);
    return used_length;
#if 0
    // A simpler, but slower version.
    const uint8_t base64_table[] = {
        [ 0] = 'A', [16] = 'Q', [32] = 'g', [48] = 'w',
        [ 1] = 'B', [17] = 'R', [33] = 'h', [49] = 'x',
        [ 2] = 'C', [18] = 'S', [34] = 'i', [50] = 'y',
        [ 3] = 'D', [19] = 'T', [35] = 'j', [51] = 'z',
        [ 4] = 'E', [20] = 'U', [36] = 'k', [52] = '0',
        [ 5] = 'F', [21] = 'V', [37] = 'l', [53] = '1',
        [ 6] = 'G', [22] = 'W', [38] = 'm', [54] = '2',
        [ 7] = 'H', [23] = 'X', [39] = 'n', [55] = '3',
        [ 8] = 'I', [24] = 'Y', [40] = 'o', [56] = '4',
        [ 9] = 'J', [25] = 'Z', [41] = 'p', [57] = '5',
        [10] = 'K', [26] = 'a', [42] = 'q', [58] = '6',
        [11] = 'L', [27] = 'b', [43] = 'r', [59] = '7',
        [12] = 'M', [28] = 'c', [44] = 's', [60] = '8',
        [13] = 'N', [29] = 'd', [45] = 't', [61] = '9',
        [14] = 'O', [30] = 'e', [46] = 'u', [62] = '+',
        [15] = 'P', [31] = 'f', [47] = 'v', [63] = '/',
    };
    size_t size_needed = base64_encode_size(src_length);
    if(unlikely(!size_needed)) return 0;
    assert(dst_length >= size_needed);
    const uint8_t* d = src;
    size_t size_used = 0;
    size_t length = src_length;
    // read 3 bytes at at a time
    for(;length > 2;length-=3){
        unsigned b0 = *(d++);
        unsigned b1 = *(d++);
        unsigned b2 = *(d++);
        unsigned index0 = (b0 & 0xfc) >> 2;
        unsigned index1 = (b0 & 0x3) | (( b1 & 0xf0) >> 4);
        unsigned index2 = (b1 & 0xf) | ((b2 & 0xc0) >> 6);
        unsigned index3 = b2 & 0x3f;
        size_used +=4;
        *(dst++) = base64_table[index0];
        *(dst++) = base64_table[index1];
        *(dst++) = base64_table[index2];
        *(dst++) = base64_table[index3];
    }
    uint32_t bits = 0;
    uint32_t remaining_bits = 0;
    int n_remaining = 0;
    // cleanup
    for(;length;){
        switch(n_remaining){
            case 0:
                bits = (*d & 0xfc) >> 2;
                remaining_bits = ((*d) & 0x3) << 4;
                n_remaining = 2;
                *(dst++) = base64_table[bits];
                size_used++;
                break;
            case 2:
                bits = (*d & 0xf0) >> 4 | remaining_bits;
                remaining_bits = (*d & 0xf) << 2;
                n_remaining = 4;
                *(dst++) = base64_table[bits];
                size_used++;
                break;
            case 4:
                bits = ((*d & 0xc0) >> 6 ) | remaining_bits;
                remaining_bits = *d & 0x3f;
                n_remaining = 0;
                *(dst++) = base64_table[bits];
                *(dst++) = base64_table[remaining_bits];
                size_used++;
                size_used++;
                remaining_bits = 0;
                break;
            default:
                unreachable();
        }
        length--;
        d++;
    }
    if(n_remaining){
        *(dst++) = base64_table[remaining_bits];
        size_used++;
    }
    assert(size_used == size_needed);
    return size_needed;
#endif
}

// Decodes a base64 string into a data buffer.
// Returns a DECODING_ERROR if data is not a base64 character, like it's a '}' or something.
// Doesn't support '=' as zero end-padding.
static inline
warn_unused
Base64Error
base64_decode(void* restrict dst, size_t dst_length, const uint8_t* restrict src, size_t src_length){
    // In order to detect invalid inputs, but without introducing a branch on
    // every byte of input, we set bad inputs to have the 0xc0  bits set
    // (which is outside the range of a 64 bit number). We then OR our bad mask
    // with this from every byte, which lets us check at the end whether there
    // was an invalid byte. We lose which byte is bad and we always decode
    // the entire input data.
    enum {BAD=0xff};
    // Unfortunately, gnu ranges is an extension, so we need to have the ugly giant version
#if 0
    const uint8_t base64_decode_table[] = {
        [  0 ...  42] = BAD,
        ['+'] = 62, // '+' is 43
        [ 44 ...  46] = BAD,
        ['/'] = 63, // '/' is 47
        ['0'] = 52, ['1'] = 53, ['2'] = 54, ['3'] = 55,
        ['4'] = 56, ['5'] = 57, ['6'] = 58, ['7'] = 59,
        ['8'] = 60, ['9'] = 61,
        [ 58 ...  64] = BAD,
        ['A'] =  0,  // 'A' is 65
        ['B'] =  1, ['C'] =  2, ['D'] =  3, ['E'] =  4,
        ['F'] =  5, ['G'] =  6, ['H'] =  7, ['I'] =  8,
        ['J'] =  9, ['K'] = 10, ['L'] = 11, ['M'] = 12,
        ['N'] = 13, ['O'] = 14, ['P'] = 15, ['Q'] = 16,
        ['R'] = 17, ['S'] = 18, ['T'] = 19, ['U'] = 20,
        ['V'] = 21, ['W'] = 22, ['X'] = 23, ['Y'] = 24,
        ['Z'] = 25,
        [ 91 ...  96] = BAD,
        ['a'] = 26, ['b'] = 27, ['c'] = 28, ['d'] = 29,
        ['e'] = 30, ['f'] = 31, ['g'] = 32, ['h'] = 33,
        ['i'] = 34, ['j'] = 35, ['k'] = 36, ['l'] = 37,
        ['m'] = 38, ['n'] = 39, ['o'] = 40, ['p'] = 41,
        ['q'] = 42, ['r'] = 43, ['s'] = 44, ['t'] = 45,
        ['u'] = 46, ['v'] = 47, ['w'] = 48, ['x'] = 49,
        ['y'] = 50, ['z'] = 51,
        [123 ... 255] = BAD,
    };
#else
    // Totally unreadable, but standard-compliant
    const uint8_t base64_decode_table[] = {
        BAD,  BAD,  BAD,  BAD,  BAD,  BAD,  BAD,  BAD,
        BAD,  BAD,  BAD,  BAD,  BAD,  BAD,  BAD,  BAD,
        BAD,  BAD,  BAD,  BAD,  BAD,  BAD,  BAD,  BAD,
        BAD,  BAD,  BAD,  BAD,  BAD,  BAD,  BAD,  BAD,
        BAD,  BAD,  BAD,  BAD,  BAD,  BAD,  BAD,  BAD,
        BAD,  BAD,  BAD,  0x3e, BAD,  BAD,  BAD,  0x3f,
        0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b,
        0x3c, 0x3d, BAD,  BAD,  BAD,  BAD,  BAD,  BAD,
        BAD,  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
        0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e,
        0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
        0x17, 0x18, 0x19, BAD,  BAD,  BAD,  BAD,  BAD,
        BAD,  0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20,
        0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
        0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30,
        0x31, 0x32, 0x33, BAD,  BAD,  BAD,  BAD,  BAD,
        BAD,  BAD,  BAD,  BAD,  BAD,  BAD,  BAD,  BAD,
        BAD,  BAD,  BAD,  BAD,  BAD,  BAD,  BAD,  BAD,
        BAD,  BAD,  BAD,  BAD,  BAD,  BAD,  BAD,  BAD,
        BAD,  BAD,  BAD,  BAD,  BAD,  BAD,  BAD,  BAD,
        BAD,  BAD,  BAD,  BAD,  BAD,  BAD,  BAD,  BAD,
        BAD,  BAD,  BAD,  BAD,  BAD,  BAD,  BAD,  BAD,
        BAD,  BAD,  BAD,  BAD,  BAD,  BAD,  BAD,  BAD,
        BAD,  BAD,  BAD,  BAD,  BAD,  BAD,  BAD,  BAD,
        BAD,  BAD,  BAD,  BAD,  BAD,  BAD,  BAD,  BAD,
        BAD,  BAD,  BAD,  BAD,  BAD,  BAD,  BAD,  BAD,
        BAD,  BAD,  BAD,  BAD,  BAD,  BAD,  BAD,  BAD,
        BAD,  BAD,  BAD,  BAD,  BAD,  BAD,  BAD,  BAD,
        BAD,  BAD,  BAD,  BAD,  BAD,  BAD,  BAD,  BAD,
        BAD,  BAD,  BAD,  BAD,  BAD,  BAD,  BAD,  BAD,
        BAD,  BAD,  BAD,  BAD,  BAD,  BAD,  BAD,  BAD,
        BAD,  BAD,  BAD,  BAD,  BAD,  BAD,  BAD,  BAD,
    };
#endif
    _Static_assert(sizeof(base64_decode_table) == 256, "");
    size_t size_needed = base64_decode_size(src_length);
    if(!size_needed) return BASE64_NO_ERROR;
    // we allow a 1 byte under in case last few bits were just padding.
    // AUDITME: make sure the math actually works out. Tests are passing for now.
    if(dst_length +1 < size_needed)
        return BASE64_WOULD_OVERFLOW;
    unsigned bad = 0;
    uint8_t* out = dst;
    size_t length = src_length;
    // Read 4 chars at a time into 3 bytes
    for(;length > 3;length-=4){
        unsigned v1 = base64_decode_table[*(src++)];
        unsigned v2 = base64_decode_table[*(src++)];
        unsigned v3 = base64_decode_table[*(src++)];
        unsigned v4 = base64_decode_table[*(src++)];
        bad |= v1;
        bad |= v2;
        bad |= v3;
        bad |= v4;
        *(out++) = (uint8_t)((v1 << 2) | (v2 >> 4));
        *(out++) = (uint8_t)((v2 << 4) | (v3 >> 2));
        *(out++) = (uint8_t)((v3 << 6) | v4);
    }
    {
        // Since I did the optimization above, this becomes the cleanup loop instead
        // of the main loop. Some of these cases are now dead code. So, TODO.
        unsigned bits_remaining = 0;
        unsigned remainder = 0;
        for(;length;length--){
            unsigned v = base64_decode_table[*(src++)];
            bad |= v;
            switch(bits_remaining){
                case 0:
                    remainder = v;
                    bits_remaining = 6;
                    break;
                case 2:
                    *(out++) = (uint8_t)((remainder << 6) | v);
                    remainder = 0;
                    bits_remaining = 0;
                    break;
                case 4:
                    *(out++) = (uint8_t)((remainder << 4) | (v >> 2));
                    remainder = v & 0x3;
                    bits_remaining = 2;
                    break;
                case 6:
                    *(out++) = (uint8_t)((remainder << 2) | (v >> 4));
                    remainder = v & 0xf;
                    bits_remaining = 4;
                    break;
                default:
                    unreachable();
            }
        }
        // It's possible the leftover bits are the result of padding with zeros.
        if(size_needed == dst_length){
            switch(bits_remaining){
                case 0: break;
                case 2: *(out++) = (uint8_t)(remainder << 6); break;
                case 4: *(out++) = (uint8_t)(remainder << 4); break;
                case 6: *(out++) = (uint8_t)(remainder << 2); break;
                default: unreachable();
            }
        }
        else {
            // Assuming the last bits are always 0. Check that assumption here.
            if(remainder)
                return BASE64_DECODING_ERROR;
        }
    }
    // Return error if we read a bad input character.
    if(unlikely(bad & 0xc0))
        return BASE64_DECODING_ERROR;

    return BASE64_NO_ERROR;
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#endif
