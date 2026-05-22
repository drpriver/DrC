//usr/bin/env drc "$0" "$@"; exit
// macOS File Picker using Objective-C Runtime C API
#ifndef __APPLE__
#error "This only works on macos"
_Static_assert(0, "This only works on macos");
#endif
#pragma framework "Cocoa"
#include "objc_helpers.h"
#include <stdio.h>

id app = cls("NSApplication").msg("sharedApplication");
app.msgv_long("setActivationPolicy:", 0L);

id panel = cls("NSOpenPanel").msg("openPanel");
panel.msgv_bool("setCanChooseFiles:", 1);
panel.msgv_bool("setCanChooseDirectories:", 0);
panel.msgv_bool("setAllowsMultipleSelection:", 0);
panel.msgv_id("setMessage:", nsstr("Pick a file!"));

app.msgv_bool("activateIgnoringOtherApps:", 1);
long result = panel.msgl("runModal");
if(result != 1){
    printf("Cancelled\n");
    return 0;
}
id url = panel.msg("URL");
id path = url.msg("path");
const char* pathStr = path.msg_cstr("UTF8String");
printf("Opening: %s\n", pathStr);

id workspace = cls("NSWorkspace").msg("sharedWorkspace");
workspace.msgv_id("openFile:", path);
