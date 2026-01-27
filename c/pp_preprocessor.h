#ifndef C_PP_PREPROCESSOR_H
#define C_PP_PREPROCESSOR_H
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//

#include "../Drp/atom_map.h"
#include "../Drp/Allocators/allocator.h"
#include "../Drp/long_string.h"
#include "../Drp/file_cache.h"
#include "../Drp/atom_table.h"
#include "../Drp/logger.h"
#include "../Drp/env.h"

#ifndef MARRAY_STRING_VIEW
#define MARRAY_STRING_VIEW
#define MARRAY_T StringView
#include "../Drp/Marray.h"
#endif

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

#ifndef arrlen
#define arrlen(arr) (sizeof(arr)/sizeof((arr)[0]))
#endif

typedef struct IncludePosition IncludePosition;
struct IncludePosition {
    size_t array, // which array we are scanning through
           idx;   // actual index into the array.
};

typedef struct CPreprocessor CPreprocessor;
struct CPreprocessor {
    Allocator allocator;
    AtomMap(CMacro) macros;
    FileCache* fc;
    AtomTable* at;
    Logger* logger;
    Environment* env;
    /*
     * 1. For the quote form of the include directive, the directory of the
     *    current file is searched first.
     * 2. For the quote form of the include directive, the directories specified
     *    by -iquote options are searched in left-to-right order, as they appear
     *    on the command line.
     * 3. Directories specified with -I options are scanned in left-to-right
     *    order.
     * 4. Directories specified with -isystem options are scanned in
     *    left-to-right order.
     * 5. Standard system directories are scanned.
     * 6. Directories specified with -idirafter options are scanned in
     *    left-to-right order.
     */
    IncludePosition include_position; // where we are in the include lookup, for include_next and related.
    union {
        struct {
            Marray(StringView)  iquote_paths,
                                Ipaths,
                                isystem_paths,
                                istandard_system_paths,
                                idirafter_paths,
                                framework_paths;
        };
        Marray(StringView) include_paths[5];
    };


};
static
_Bool
cpp_has_include(CPreprocessor* cpp, _Bool quote, StringView header_name){
    MStringBuilder* sb = fc_path_builder(cpp->fc);
    for(size_t i = quote?0:1; i < arrlen(cpp->include_paths); i++){
        Marray(StringView)* dirs = &cpp->include_paths[i];
        MARRAY_FOR_EACH_VALUE(StringView, d, *dirs){
            msb_reset(sb);
            if(!d.length) continue;
            msb_write_str(sb, d.text, d.length);
            if(msb_peek(sb) != '/')
                msb_write_char(sb, '/');
            msb_write_str(sb, header_name.text, header_name.length);
            if(fc_is_file(cpp->fc)){
                return 1;
            }
        }
    }
    return 0;
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
