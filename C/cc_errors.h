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
    _cc_oom_error,
    _cc_syntax_error,
    _cc_unreachable_error,
    _cc_unimplemented_error,
    _cc_file_not_found_error,
    _cc_macro_already_exists_error,
    _cc_redefining_builtin_macro_error,
    _cc_runtime_error,
};
#endif
