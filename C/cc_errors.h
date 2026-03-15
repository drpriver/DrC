#ifndef C_CC_ERRORS_H
#define C_CC_ERRORS_H
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//

// Shared underlying error values so that errors can safely be implicitly
// converted between the different layers without having to actually share
// names.
enum {
    _cc_no_error = 0,
    _cc_oom_error = 1,
    _cc_syntax_error = 2,
    _cc_unreachable_error = 3,
    _cc_unimplemented_error = 4,
    _cc_file_not_found_error = 5,
    _cc_macro_already_exists_error = 6,
    _cc_redefining_builtin_macro_error = 7,
    _cc_runtime_error = 8,
    _cc_invalid_value_error = 9,
    _cc_symbol_not_found_error = 10,
};
static const char* const _cc_error_names[] = {
    [_cc_no_error]                       = "No Error",
    [_cc_oom_error]                      = "Out of Memory Error: Buy more RAM",
    [_cc_syntax_error]                   = "Syntax Error",
    [_cc_unreachable_error]              = "Unreachable code reached",
    [_cc_unimplemented_error]            = "Unimplemented code reached",
    [_cc_file_not_found_error]           = "File not found",
    [_cc_macro_already_exists_error]     = "Macro already exists",
    [_cc_redefining_builtin_macro_error] = "Redefining builtin macro",
    [_cc_runtime_error]                  = "Runtime error",
    [_cc_invalid_value_error]            = "Invalid value",
    [_cc_symbol_not_found_error]         = "Symbol not found",
};
#endif
