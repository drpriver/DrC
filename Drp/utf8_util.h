#ifndef UTF8_UTIL_H
#define UTF8_UTIL_H

#include <stddef.h>

#ifdef __clang__
#pragma clang assume_nonnull begin
#else
#ifndef _Nonnull
#define _Nonnull
#endif
#endif

// Move cursor to previous character boundary
// Returns new cursor position (>= 0)
static
size_t
utf8_prev_char(const char* text, size_t cursor){
    if(cursor <= 0) return 0;
    cursor--;
    // Skip UTF-8 continuation bytes (10xxxxxx)
    while(cursor && ((unsigned char)text[cursor] & 0xC0) == 0x80){
        cursor--;
    }
    return cursor;
}

// Move cursor to next character boundary
// Returns new cursor position (<= text_len)
static
size_t
utf8_next_char(const char* text, size_t cursor, size_t text_len){
    if(cursor >= text_len) return text_len;
    cursor++;
    // Skip UTF-8 continuation bytes (10xxxxxx)
    while(cursor < text_len && ((unsigned char)text[cursor] & 0xC0) == 0x80){
        cursor++;
    }
    return cursor;
}

// Decode one UTF-8 codepoint from string, advance pointer
// Returns the codepoint, or -1 on error
static
int32_t
utf8_decode_one(const char*_Nonnull*_Nonnull p, const char* end){
    const unsigned char* s = (const unsigned char*)*p;
    if(s >= (const unsigned char*)end) return -1;

    if(s[0] < 0x80){
        // ASCII
        *p += 1;
        return s[0];
    }
    else if((s[0] & 0xE0) == 0xC0 && s + 1 < (const unsigned char*)end){
        // 2-byte sequence
        *p += 2;
        return ((s[0] & 0x1F) << 6) | (s[1] & 0x3F);
    }
    else if((s[0] & 0xF0) == 0xE0 && s + 2 < (const unsigned char*)end){
        // 3-byte sequence
        *p += 3;
        return ((s[0] & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F);
    }
    else if((s[0] & 0xF8) == 0xF0 && s + 3 < (const unsigned char*)end){
        // 4-byte sequence
        *p += 4;
        return ((s[0] & 0x07) << 18) | ((s[1] & 0x3F) << 12) | ((s[2] & 0x3F) << 6) | (s[3] & 0x3F);
    }
    // Invalid, skip one byte
    *p += 1;
    return -1;
}


// Decode UTF-8 string into codepoint array
// Returns number of codepoints written
static
int
utf8_decode_string(const char* s, size_t len, int32_t* out){
    const char* end = s + len;
    int i = 0;
    while(s < end){
        out[i++] = utf8_decode_one(&s, end);
    }
    return i;
}

// Count UTF-8 codepoints in string
static
size_t
utf8_count_codepoints(const char* s, size_t len){
    const char* end = s + len;
    size_t count = 0;
    while(s < end){
        utf8_decode_one(&s, end);
        count++;
    }
    return count;
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
