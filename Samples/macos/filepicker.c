//usr/bin/env drc "$0" "$@"; exit
// macOS File Picker using Objective-C Runtime C API
#ifndef __APPLE__
#error "This only works on macos"
_Static_assert(0, "This only works on macos");
#endif
#pragma framework "Cocoa"
#include "objc_helpers.h"
#include <stdio.h>

id app = msg(cls("NSApplication"), "sharedApplication");
msgv_long(app, "setActivationPolicy:", 0L);

id panel = msg(cls("NSOpenPanel"), "openPanel");
msgv_bool(panel, "setCanChooseFiles:", 1);
msgv_bool(panel, "setCanChooseDirectories:", 0);
msgv_bool(panel, "setAllowsMultipleSelection:", 0);
msgv_id(panel, "setMessage:", nsstr("Pick a file!"));

msgv_bool(app, "activateIgnoringOtherApps:", 1);
long result = msgl(panel, "runModal");
if(result != 1){
    printf("Cancelled\n");
    return 0;
}
id url = msg(panel, "URL");
id path = msg(url, "path");
const char* pathStr = msg_cstr(path, "UTF8String");
printf("Opening: %s\n", pathStr);

id workspace = msg(cls("NSWorkspace"), "sharedWorkspace");
msgv_id(workspace, "openFile:", path);
