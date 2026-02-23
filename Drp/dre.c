#ifndef DRE_C
#define DRE_C
#define dre_assert(...) (void)(__VA_ARGS__)
#include "dre.h"

#define AS_STRING_ENTRY(s) #s,
DRE_API
const char * _Nonnull const dre_error_name_table[] = {
    "NO_ERROR",
    RE_ERRORS(AS_STRING_ENTRY)
};

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

#ifndef DRE_TRACE
//#include <stdio.h>
//#define DRE_TRACE(fmt,...) do { fprintf(stderr, "DRE_TRACE: " fmt "\n", ##__VA_ARGS__); } while (0)
#define DRE_TRACE(fmt,...)
#endif

// the length of 1 node in the regex
static
size_t
dre_nodelen(const char *regex, size_t regex_len, enum re_error *error_code){
    if(!regex_len)
        return 0;

    const char c = regex[0];
    if(c == '\\'){
        if(regex_len==1){
            *error_code = RE_ERROR_ENDS_WITH_BACKSLASH;
            return 0;
        }
        return 2;
    }
    if(c == '*' || c == '+' || c == '.' || c == '$' || c == '^' || c == '?')
        return 1;
    if(c == '['){
        _Bool in_escape = 0;
        for(size_t i = 1; i < regex_len;i++){
            switch(regex[i]){
                case ']':
                    if(!in_escape)
                        return i + 1;
                    in_escape = 0;
                    break;
                case '\\':
                    in_escape = !in_escape;
                    break;
                default:
                    in_escape = 0;
                    break;
            }
        }
        *error_code = RE_ERROR_MISSING_RIGHT_SQUARE_BRACKET;
        return 0;
    }
    return 1;
}

static inline
_Bool
dre_isalpha(int c){
    c |= 0x20;
    return c >= 'a' && c <= 'z';
}

static inline
_Bool
dre_isdigit(int c){
    return c >= '0' && c <= '9';
}

static inline
_Bool
dre_isspace(int c){
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f';
}

static inline
_Bool
dre_isalphanum(char c){
  return (c == '_') || dre_isalpha(c) || dre_isdigit(c);
}

// TODO: return -1 on BAD_ESCAPE
static
int
dre_try_match_escape(char regex_c, char text_c){
  switch(regex_c){
      case 'd': return dre_isdigit(text_c)    ? 1 : 0;
      case 'D': return dre_isdigit(text_c)    ? 0 : 1;
      case 'w': return dre_isalphanum(text_c) ? 1 : 0;
      case 'W': return dre_isalphanum(text_c) ? 0 : 1;
      case 's': return dre_isspace(text_c)    ? 1 : 0;
      case 'S': return dre_isspace(text_c)    ? 0 : 1;

      // common escapes
      case 't': return text_c == '\t';
      case 'n': return text_c == '\n';

      case '\\': case '*': case '+':
      case '?': case '.':
      case '$': case '^':
      case '(': case ')':
      case '[': case ']':
      case '{': case '}':
      case '-': case '|':
      return regex_c == text_c;
  }
  return -1; // BAD_ESCAPE
}

static inline int dre_matchdot(char c){ return c != '\n'; }

// TODO: return -1 on BAD_ESCAPE
// caller guarantees regex is terminated by ']'
// regex starts after the [ and the first ^ if it exists.
static
int
dre_try_matchcharset(const char *regex, size_t regex_len, char c){
    for(;regex_len;){
        char regex_c = regex[0];
        if(regex_c == ']')
            return 0;

        if(regex_len >= 3 && regex[1] == '-' && regex[2] != ']'){
            if(c >= regex_c && c <= regex[2]){
                return 1;
            }
            regex += 3;
            regex_len -= 3;
        }
        else {
            if(regex_c == '\\' && regex_len >= 2){
                int result = dre_try_match_escape(regex[1], c);
                if(result)
                    return result;
                regex += 2;
                regex_len -= 2;
            }
            else {
                if(regex_c == c)
                    return 1;
                regex++;
                regex_len--;
            }
        }
    }
    return 0;
}

// inverts the result of a match if it's not an error
static inline
int
dre_flip_try_match_result(int result){
    static const int try_not_table[] = {-1, 1, 0};
    dre_assert(result >= -1);
    dre_assert(result <= 1);
    return try_not_table[result + 1];
}

// returns -1 on invalid escape sequence
static
int
dre_try_matchone(const char *regex, size_t regex_len, char text_c){
    DRE_TRACE("matchone regex='%.*s' c='%c'", (int)regex_len, regex, text_c);
    if(!regex_len) return 0;
    char regex_c = regex[0];
    if(regex_c == '\\')
        return regex_len>1?dre_try_match_escape(regex[1], text_c) : -1;

    if(regex_c == '.')
        return text_c != '\n';

    if(regex_c == '[' && regex_len > 2){
        if(regex[1] ==  '^'){
            return dre_flip_try_match_result(dre_try_matchcharset(regex + 2, regex_len-2, text_c));
        }
        return dre_try_matchcharset(regex + 1, regex_len-1, text_c);
    }
    return regex_c == text_c;
}

static
_Bool
dre_matchquestion(DreContext *context, const char *regex_left, size_t regex_left_len, const char *regex_right, size_t regex_right_len, const char* text_ptr, size_t text_len){
    DRE_TRACE("matchquestion regex_left='%.*s' text='%.*s'", (int)regex_left_len, regex_left, (int)text_len, text_ptr);
    // Try WITH the optional character first (greedy)
    if(text_len > 0){
        int result = dre_try_matchone(regex_left, regex_left_len, text_ptr[0]);
        if(result == -1){
            context->error = RE_ERROR_BAD_ESCAPE;
            context->error_location = regex_left;
            return 0;
        }
        if(result){
            if(dre_match_start_only(context, regex_right, regex_right_len, text_ptr + 1, text_len - 1)){
                context->match_length++;
                return 1;
            }
        }
    }
    // Fallback: try WITHOUT the optional character
    if(dre_match_start_only(context, regex_right, regex_right_len, text_ptr, text_len))
        return 1;
    return 0;
}

static
_Bool
dre_matchplus(DreContext *context, const char *regex_left, size_t regex_left_len, const char *regex_right, size_t regex_right_len, const char* text_ptr, size_t text_len){
    DRE_TRACE("matchplus regex_right='%.*s' text='%.*s'", (int)regex_right_len, regex_right, (int)text_len, text_ptr);
    const char *text_start = text_ptr;
    const char *text_end = text_start + text_len;
    while(text_ptr != text_end){
        int result = dre_try_matchone(regex_left, regex_left_len, text_ptr[0]);
        if(result == -1){
            context->error = RE_ERROR_BAD_ESCAPE;
            context->error_location = regex_left;
            return 0;
        }
        if(!result)
        break;
        text_ptr++;
        text_len--;
    }
    for(;text_ptr > text_start; text_ptr--, text_len++){
        if(dre_match_start_only(context, regex_right, regex_right_len, text_ptr, text_len)){
            context->match_length += text_ptr - text_start;
            return 1;
        }
    }
    return 0;
}

DRE_API
_Bool
dre_match_start_only(DreContext *context, const char *regex, size_t regex_len, const char* text_ptr, size_t text_len){
    DRE_TRACE("dre_match_start_only regex='%.*s' text='%.*s' (match=%zu)", (int)regex_len, regex, (int)text_len, text_ptr, context->match_length);
    size_t save_length = context->match_length;
    const char* regex_end = regex + regex_len;
    const char* text_end = text_ptr + text_len;
    for(;regex != regex_end;){
        const char current_c = regex[0];
        enum re_error node_length_error = RE_ERROR_NONE;
        const char *next_regex = regex + dre_nodelen(regex, regex_end - regex, &node_length_error);
        if(node_length_error != RE_ERROR_NONE){
            context->error = node_length_error;
            context->error_location = regex;
            return 0; // just continue, the user can handle the error if they so chose
        }
        const char next_c = next_regex != regex_end? next_regex[0] : 0;
        if(next_c == '?'){
            enum re_error ignoreme = 0;
            dre_assert(dre_nodelen(next_regex, regex_end - next_regex, &ignoreme) == 1);
            dre_assert(ignoreme == 0);
            return dre_matchquestion(context, regex, next_regex - regex, next_regex + 1, regex_end - (next_regex + 1), text_ptr, text_end - text_ptr);
        }

        if(next_c == '+'){
            enum re_error ignoreme = 0;
            dre_assert(dre_nodelen(next_regex, regex_end - next_regex, &ignoreme) == 1);
            dre_assert(ignoreme == 0);
            return dre_matchplus(context, regex, next_regex - regex, next_regex + 1, regex_end - (next_regex + 1), text_ptr, text_end - text_ptr);
        }

        if(current_c == '$' && next_c == '\0'){
            return text_ptr == text_end;
        }

        if(current_c == '|'){
            context->error = RE_ERROR_BRANCH_NOT_IMPLEMENTED;
            context->error_location = regex;
            return 0;
        }

        if(next_c == '*'){
            enum re_error ignoreme = 0;
            dre_assert(dre_nodelen(next_regex, regex_end - next_regex, &ignoreme) == 1);
            dre_assert(ignoreme == 0);
            if(dre_matchplus(context, regex, next_regex - regex, next_regex + 1, regex_end - (next_regex + 1), text_ptr, text_end - text_ptr)){
                return 1;
            }
            regex = next_regex + 1;
        }
        else {
            if(text_ptr == text_end){
                break;
            }
            int result = dre_try_matchone(regex, next_regex - regex, text_ptr[0]);
            if(result == -1){
                context->error = RE_ERROR_BAD_ESCAPE;
                context->error_location = regex;
                break;
            }
            if(result == 0)
                break;
            context->match_length++;
            text_ptr++;
            regex = next_regex;
        }
    }

    // If we've consumed all of the regex, we matched successfully
    if(regex == regex_end){
        return 1;
    }

    // Otherwise, restore and return failure
    context->match_length = save_length;
    return 0;
}

DRE_API
_Bool
dre_match(DreContext *context, const char *regex, size_t regex_len, const char *text_start, size_t text_len, size_t *out_match_start){
    DRE_TRACE("match '%.*s' '%.*s'", (int)regex_len, regex, (int)text_len, text_start);
    const char *text_ptr = text_start;
    const char *text_end = text_start + text_len;

    if(regex_len > 0 && regex[0] == '^'){
        context->match_length = 0;
        if(!dre_match_start_only(context, regex + 1, regex_len - 1, text_ptr, text_end - text_ptr))
            return 0;
        *out_match_start = 0;
        return 1;
    }

    for(;; text_ptr++){
        context->match_length = 0;
        if(dre_match_start_only(context, regex, regex_len, text_ptr, text_end - text_ptr)){
            *out_match_start = text_ptr - text_start;
            return 1;
        }
        if(text_ptr == text_end)
            return 0;
    }
}
#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
