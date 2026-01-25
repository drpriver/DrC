//
// Copyright © 2021-2025, David Priver <david@davidpriver.com>
//
#ifndef GET_INPUT_H
#define GET_INPUT_H
//
// GetInput is a simple line-editing module to add interactive terminal line editing
// in a cross-platform manner.
//
// This is spiritually a single-header library,
// but it needs <Windows.h> on windows, so to help isolate from
// that, it is implemented as .h and .c
//
// If you need them as externs, just trivially wrap them.

// size_t
#include <stddef.h>
#include <stdint.h>
#include "stringview.h"
#include "long_string.h"

#ifdef _WIN32
// allow user to suppress this def
#ifndef HAVE_SSIZE_T
typedef long long ssize_t;
#endif
#else
// ssize_t
#include <sys/types.h>
#endif

#ifdef __clang__
#pragma clang assume_nonnull begin
#else
#ifndef _Nullable
#define _Nullable
#endif
#endif

#ifndef GET_INPUT_API
#define GET_INPUT_API static inline
#endif

enum {GI_LINE_HISTORY_MAX = 100};
enum {GI_BUFF_SIZE=4092};

typedef struct GetInputCtx GetInputCtx;
typedef int(GiTabCompletionFunc)(
    GetInputCtx* ctx,
    size_t original_curr_pos,
    size_t original_used_len,
    int n_tabs
);
//
// Return a negative error code if you encounter an error.
// Return 0 if tab completion succeeded.
//
// Complex tab completion behavior should be implemented by you.


typedef struct GetInputCtx GetInputCtx;
struct GetInputCtx {
    StringView prompt;
    size_t prompt_display_length;
    int _hst_count;
    int _hst_cursor;
    int _cols;
    int _history_index;
    LongString _history[GI_LINE_HISTORY_MAX];
    char buff[GI_BUFF_SIZE];
    size_t buff_cursor;
    size_t buff_count; // number of used characters.
    // The tab completion func uses this to save/restore the user's original input.
    char altbuff[GI_BUFF_SIZE];
    GiTabCompletionFunc*_Nullable tab_completion_func;
    void*_Nullable tab_completion_user_data;
    // Reset to 0 after non-tab input.
    // Do not store resources that need to be freed
    // here.
    uintptr_t tab_completion_cookie;
};
// -----------
//
// The context structure that holds all internal state necessary across
// `gi_get_input` calls.
//
// Members:
// --------
//   prompt:
//     This is displayed before the input line.
//   prompt_display_length:
//     If the prompt includes escape codes, then the length of the string will be different
//     than the display length of the string. If so, then set this member.
//     If this member is 0, it uses the length of the prompt.
//   _hst_count:
//     internal use
//   _hst_cursor:
//     internal use
//   _history_index;
//     intenral use
//   _cols:
//     internal use
//   _history:
//     internal use
//   buff:
//     The buffer that the user types into. After a successful call to
//     `gi_get_input`, the input will be in this buffer and the return value
//     from gi_get_input is how many characters.
//   buff_cursor:
//     Where the conceptual cursor into the buff is. In the tab completion
//     function, you can change this and should if you replace the contents of
//     buff. After `gi_get_input` returns, this is meaningless.
//   buff_count:
//     How many characters of the buffer are currently used. In the tab
//     completion function you can change this and should change it if you
//     replace the contents of buff. After `gi_get_input` returns, this is
//     meaningless.
//   altbuff:
//     When the tab completion function is invoked, the original buffer is
//     copied into this buffer.  You can use this to restore the buff after
//     cycling through all options.
//   tab_completion_func:
//     Your tab completion function, which is invoked when the user hits tab.
//     This is optional.
//   tab_completion_user_data:
//     A pointer you provide to store state or other data for use in the
//     completion function.  This pointer will be untouched by get_input.
//   tab_completion_cookie:
//     This is set to 0 the first time the user hits tab after making other
//     edits. You can use this to record auxillary data for tab completion,
//     such as an index into a list of completions.
//     Do not store resources that need to be freed.
//

GET_INPUT_API
int
gi_dump_history(GetInputCtx*, const char* prefix, const char* filename);
// ---------------
// Arguments:
//   ctx:
//     The ctx to dump the history from.
//
//   prefix:
//     Prefix to prepend to filename.
//
//   filename:
//     Filepath to dump the history at.
//
// Returns:
// --------
// Returns zero on success. Returns non-zero if there was an error.
//

GET_INPUT_API
int
gi_load_history(GetInputCtx*, const char* prefix, const char* filename);
// ---------------
// Arguments:
//   ctx:
//     The ctx to load the history into.
//
//   prefix:
//     Prefix to prepend to filename.
//
//   filename:
//     Filepath to read the history from.
//
// Returns:
// --------
// Returns zero on success. Returns non-zero if there was an error.
//

GET_INPUT_API
void
gi_destroy_ctx(GetInputCtx*);
// --------------
// Cleans up all resources in the ctx.
//
// NOTE: this does not save the history to disk. Use `gi_dump_history` to do that.
//


GET_INPUT_API
void
gi_add_line_to_history_len(GetInputCtx*, const char*, size_t);
// ----------------------
// Adds the string to the history. If the last line is the same as this line,
// it is skipped.  Internally makes a copy of the input string, so it is safe
// to pass stack allocated buffers or strings you're about to free, etc.
//

GET_INPUT_API
void
gi_add_line_to_history(GetInputCtx*, StringView line);
// ----------------------
// Like `gi_add_line_to_history`, but takes a string view.

GET_INPUT_API
void
gi_remove_last_line_from_history(GetInputCtx*);
// --------------------------------
// Removes the last entry in the input history
//

GET_INPUT_API
ssize_t
gi_get_input(GetInputCtx* ctx);
// --------------
// Handles user input, filling the buffer with the text.
//
// NOTE: This does not add the input line to the history. You should
//       do that yourself by calling `gi_add_line_to_history_len`. This allows
//       you to filter out bad inputs from the history, which can be quite
//       annoying to scroll through.
//
// NOTE: This function does not return the cursor to column 0 and does not
//       scroll the terminal down by 1. This allows you to "erase" the prompt
//       and input. If you don't want this, then do a puts("\r") or similar to
//       return to column 0 and go down to a fresh line.
//
// Arguments:
// ----------
// ctx:
//   A fully initialized `GetInputCtx` structure.
//
// Returns:
// --------
// Returns >= 0 on success.
//
// Returns -1 on ctrl-d.
//
// Returns an error code < 0 if an error is returned from the tab completion
// function.
//
// For >= 0, the return value is the c-string length of the buffer. The buffer
// will be nul-terminated, but as it is an internal structure, it will be
// invalidated when you call this again.
//

GET_INPUT_API
ssize_t
gi_get_input2(GetInputCtx* ctx, size_t preserved);
// --------------
// Handles user input, filling the buffer with the text.
// Like `gi_get_input`, but preserves part of the initial buffer.
//
// Arguments:
// ----------
// ctx:
//   A fully initialized `GetInputCtx` structure.
//
// preserved:
//   How many bytes to preserve in the `buff`
//
// Returns:
// --------
// Returns >= 0 on success.
//
// Returns -1 on ctrl-d.
//
// Returns an error code < 0 if an error is returned from the tab completion
// function.
//
// For >= 0, the return value is the c-string length of the buffer. The buffer
// will be nul-terminated, but as it is an internal structure, it will be
// invalidated when you call this again.
//

//
// gi_get_cols
// --------
// Returns the width of the terminal.
//
GET_INPUT_API
int
gi_get_cols(void);

GET_INPUT_API
int
gi_get_one_char(void);

#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#endif
