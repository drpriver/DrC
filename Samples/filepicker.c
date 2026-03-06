// macOS File Picker using Objective-C Runtime C API
// Demonstrates calling native macOS frameworks from interpreted C
#pragma lib "Cocoa"
#include <objc/objc.h>
#include <objc/runtime.h>
#include <objc/message.h>
#include <stdio.h>

// Typed wrappers for objc_msgSend (ARM64 needs exact signatures)
id send(id self, const char* sel){
    return ((id(*)(id, SEL))objc_msgSend)(self, sel_registerName(sel));
}
id send_str(id self, const char* sel, const char* s){
    return ((id(*)(id, SEL, const char*))objc_msgSend)(self, sel_registerName(sel), s);
}
void send_id(id self, const char* sel, id arg){
    ((void(*)(id, SEL, id))objc_msgSend)(self, sel_registerName(sel), arg);
}
void send_int(id self, const char* sel, int arg){
    ((void(*)(id, SEL, int))objc_msgSend)(self, sel_registerName(sel), arg);
}
void send_long(id self, const char* sel, long arg){
    ((void(*)(id, SEL, long))objc_msgSend)(self, sel_registerName(sel), arg);
}
long send_long_ret(id self, const char* sel){
    return ((long(*)(id, SEL))objc_msgSend)(self, sel_registerName(sel));
}
const char* send_cstr(id self, const char* sel){
    return ((const char*(*)(id, SEL))objc_msgSend)(self, sel_registerName(sel));
}
id cls(const char* name){
    return (id)objc_getClass(name);
}

// NSApplication setup - required for GUI on macOS
id app = send(cls("NSApplication"), "sharedApplication");
send_long(app, "setActivationPolicy:", 0L);

// Create and configure NSOpenPanel
id panel = send(cls("NSOpenPanel"), "openPanel");
send_int(panel, "setCanChooseFiles:", 1);
send_int(panel, "setCanChooseDirectories:", 0);
send_int(panel, "setAllowsMultipleSelection:", 0);

// Set title
id title = send_str(cls("NSString"), "stringWithUTF8String:", "Pick a file!");
send_id(panel, "setMessage:", title);

// Bring app to front and run modal
send_int(app, "activateIgnoringOtherApps:", 1);
long result = send_long_ret(panel, "runModal");
if(result != 1){
    printf("Cancelled\n");
    return 0;
}
id url = send(panel, "URL");
id path = send(url, "path");
const char* pathStr = send_cstr(path, "UTF8String");
printf("Opening: %s\n", pathStr);

// Open the file with its default application
id workspace = send(cls("NSWorkspace"), "sharedWorkspace");
send_id(workspace, "openFile:", path);
