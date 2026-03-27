#ifndef DRE_H
#define DRE_H
#include <stddef.h>
#include "stringview.h"
/*
 *
 * Mini regex-module inspired by Rob Pike's regex code described in:
 *
 * http://www.cs.princeton.edu/courses/archive/spr09/cos333/beautiful.html
 *
 * Supports:
 * ----------------------------------------
 *   .        Dot, matches any character (unless RE_DOT_NO_NEWLINE is defined)
 *   ^        Start anchor, matches beginning of string
 *   $        End anchor, matches end of string
 *   *        Asterisk, match zero or more (greedy)
 *   +        Plus, match one or more (greedy)
 *   ?        Question, match zero or one (greedy)
 *   [abc]    Character class, match if one of {'a', 'b', 'c'}
 *   [^abc]   Inverted class, match if NOT one of {'a', 'b', 'c'}
 *   [a-zA-Z] Character ranges, the character set of the ranges { a-z | A-Z }

 * Some common "character classes":
 * ----------------------------------------
 *   \s       Whitespace, \t \f \r \n \v and spaces
 *   \S       Non-whitespace
 *   \w       Alphanumeric, [a-zA-Z0-9_]
 *   \W       Non-alphanumeric
 *   \d       Digits, [0-9]
 *   \D       Non-digits
 *
 * All of the control characters mentioned above can be escaped with a leading backslash.
 * The following escape sequences are also supported:
 * ----------------------------------------
 *   \\
 *   \t   \n
 *   \-   \|
 *   \(   \)
 *   \[   \]
 *   \{   \}
 */

#define RE_ERRORS(H) \
  H(ENDS_WITH_BACKSLASH) \
  H(MISSING_RIGHT_SQUARE_BRACKET) \
  H(BAD_ESCAPE) \
  H(BRANCH_NOT_IMPLEMENTED)

#define RE_ERROR_AS_ENUM_MEMBER(s) RE_ERROR_##s,
enum re_error { RE_ERROR_NONE, RE_ERRORS(RE_ERROR_AS_ENUM_MEMBER) };
#undef RE_ERROR_AS_ENUM_MEMBER

#ifndef DRE_API
#define DRE_API static
#endif

#if !defined __clang__ && !defined _Nonnull
#define _Nonnull
#endif

#define AS_STRING_ENTRY(s) #s,
DRE_API
const char * _Nonnull const dre_error_name_table[] = {
    "NO_ERROR",
    RE_ERRORS(AS_STRING_ENTRY)
};
#undef AS_STRING_ENTRY

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

typedef struct DreContext DreContext;
struct DreContext {
  size_t match_length;
  enum re_error error;
  const char *_Nullable error_location;
};

// returns 1 on match, 0 for no match
// start of the match is returned in out_match_start
// check context->error for any regex errors
DRE_API
_Bool
dre_match(DreContext *context, const char *regex, size_t regex_len, const char *text_start, size_t text_len, size_t *out_match_start);

static inline
_Bool
dre_match_sv(DreContext *context, StringView re, StringView text, size_t *out_match_start){
    return dre_match(context, re.text, re.length, text.text, text.length, out_match_start);
}
// returns 1 on match, 0 for no match
// check context->error for any regex errors
DRE_API
_Bool
dre_match_start_only(DreContext *context, const char *regex, size_t regex_len, const char *text_start, size_t text_len);

static inline
_Bool
dre_match_entire(const char* regex, size_t regex_len, const char* text, size_t len){
    DreContext ctx = {0};
    if(!dre_match_start_only(&ctx, regex, regex_len, text, len))
        return 0;
    return ctx.match_length == len;
}

static inline
_Bool
dre_match_entire_sv(StringView re, StringView text){
    DreContext ctx = {0};
    if(!dre_match_start_only(&ctx, re.text, re.length, text.text, text.length))
        return 0;
    return ctx.match_length == text.length;
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
