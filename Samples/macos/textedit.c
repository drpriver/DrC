// Text Editor — Objective-C classes backed by interpreted code
//
// Demonstrates creating ObjC classes at runtime with methods implemented
// in interpreted C. The AppDelegate and EditorController classes are
// registered dynamically via the ObjC runtime API, with their method
// IMPs pointing to interpreted functions.
//
// Usage: Bin/cc Samples/textedit.c

#ifndef __APPLE__
#error "This only works on macos"
__builtin_abort();
#endif

#pragma lib "Cocoa"
#include <std.h>
#include <objc/objc.h>
#include <objc/runtime.h>
#include <objc/message.h>

// Core Graphics types (matching macOS CGGeometry)
typedef struct { double x, y; } CGPoint;
typedef struct { double width, height; } CGSize;
typedef struct { CGPoint origin; CGSize size; } CGRect;

// NSWindow style masks
enum {
    NSWindowStyleMaskTitled         = 1 << 0,
    NSWindowStyleMaskClosable       = 1 << 1,
    NSWindowStyleMaskMiniaturizable = 1 << 2,
    NSWindowStyleMaskResizable      = 1 << 3,
};

// Forward declarations
void doNew(id self, SEL _cmd, id sender);
void doOpen(id self, SEL _cmd, id sender);
void doSave(id self, SEL _cmd, id sender);
void doSaveAs(id self, SEL _cmd, id sender);
void appDidFinishLaunching(id self, SEL _cmd, id note);
_Bool appShouldTerminate(id self, SEL _cmd, id app);
void createMenuBar(void);

// ---------------------------------------------------------------------------
// objc_msgSend wrappers — ARM64 requires exact type signatures
// ---------------------------------------------------------------------------

SEL sel(const char* name){
    return sel_registerName(name);
}

id cls(const char* name){
    return (id)objc_getClass(name);
}

// Tuple helpers: T((int, x)) -> int, N((int, x)) -> x
#define T(t, n) t
#define N(t, n) n
#define PARAM(a)  , T a N a
#define CAST_T(a) , T a
#define FWD(a)    , N a

#define MSG(ret, name, ...) \
    ret name(id self, const char* s __map(PARAM, __VA_ARGS__)){ \
        return ((ret(*)(id, SEL __map(CAST_T, __VA_ARGS__)))objc_msgSend)(self, sel(s) __map(FWD, __VA_ARGS__)); }

MSG(id,          msg)
MSG(id,          msg_id,     (id, a))
MSG(void,        msgv)
MSG(void,        msgv_id,    (id, a))
MSG(void,        msgv_bool,  (_Bool, a))
MSG(void,        msgv_long,  (unsigned long, a))
MSG(long,        msg_long)
MSG(id,          msg_str,    (const char*, a))
MSG(id,          msg_rect,   (CGRect, r))
MSG(const char*, msg_cstr)
MSG(id,          msg_double, (double, a))
MSG(id,          msg_double2,(double, a), (double, b))

// Convenience: create NSString from C string
id nsstr(const char* s){
    return msg_str(cls("NSString"), "stringWithUTF8String:", s);
}
enum : unsigned long {
    NSUTF8StringEncoding = 4ul,
};

// Read file contents into an NSString (NSUTF8StringEncoding = 4)
id nsstr_from_file(const char* path){
    return ((id(*)(id, SEL, id, unsigned long, id*))objc_msgSend)(
        cls("NSString"), sel("stringWithContentsOfFile:encoding:error:"),
        nsstr(path), NSUTF8StringEncoding, NULL);
}

// Write an NSString to a file (path is an NSString)
_Bool nsstr_to_file(id string, id path){
    return ((_Bool(*)(id, SEL, id, _Bool, unsigned long, id*))objc_msgSend)(
        string, sel("writeToFile:atomically:encoding:error:"),
        path, (_Bool)1, NSUTF8StringEncoding, NULL);
}

// ---------------------------------------------------------------------------
// Global state
// ---------------------------------------------------------------------------

id g_textView;
id g_window;
id g_filePath; // NSString, retained

// ---------------------------------------------------------------------------
// EditorController methods — interpreted functions used as ObjC methods
// ---------------------------------------------------------------------------

void doNew(id self, SEL _cmd, id sender){
    msgv_id(g_textView, "setString:", nsstr(""));
    if(g_filePath){ msgv(g_filePath, "release"); g_filePath = NULL; }
    msgv_id(g_window, "setTitle:", nsstr("Untitled"));
}

void doOpen(id self, SEL _cmd, id sender){
    id panel = msg(cls("NSOpenPanel"), "openPanel");
    msgv_bool(panel, "setCanChooseFiles:", 1);
    msgv_bool(panel, "setCanChooseDirectories:", 0);

    long result = msg_long(panel, "runModal");
    if(result != 1) return;

    id path = msg(msg(panel, "URL"), "path");
    id contents = nsstr_from_file(msg_cstr(path, "UTF8String"));
    if(!contents) return;
    msgv_id(g_textView, "setString:", contents);

    if(g_filePath) msgv(g_filePath, "release");
    g_filePath = msg(path, "copy");
    msgv_id(g_window, "setTitle:", g_filePath);
}

void doSave(id self, SEL _cmd, id sender){
    if(!g_filePath){
        doSaveAs(self, _cmd, sender);
        return;
    }
    nsstr_to_file(msg(g_textView, "string"), g_filePath);
}

void doSaveAs(id self, SEL _cmd, id sender){
    id panel = msg(cls("NSSavePanel"), "savePanel");
    long result = msg_long(panel, "runModal");
    if(result != 1) return;

    id path = msg(msg(panel, "URL"), "path");
    nsstr_to_file(msg(g_textView, "string"), path);

    if(g_filePath) msgv(g_filePath, "release");
    g_filePath = msg(path, "copy");
    msgv_id(g_window, "setTitle:", g_filePath);
}

// ---------------------------------------------------------------------------
// Menu bar creation
// ---------------------------------------------------------------------------

// Helper: create a menu item with title, action, key equivalent, and target
id menuItem(const char* title, const char* action, const char* key, id target){
    id item = msg(cls("NSMenuItem"), "alloc");
    item = ((id(*)(id, SEL, id, SEL, id))objc_msgSend)(
        item, sel("initWithTitle:action:keyEquivalent:"),
        nsstr(title),
        action ? sel(action) : (SEL)0,
        nsstr(key));
    if(target)
        msgv_id(item, "setTarget:", target);
    return item;
}

void createMenuBar(void){
    id mainMenu = msg(msg(cls("NSMenu"), "alloc"), "init");

    // -- App menu --
    id appItem = msg(msg(cls("NSMenuItem"), "alloc"), "init");
    id appMenu = msg_id(msg(cls("NSMenu"), "alloc"), "initWithTitle:", nsstr("Dr Text"));
    msgv_id(appMenu, "addItem:", menuItem("About Dr Text", "orderFrontStandardAboutPanel:", "", NULL));
    msgv_id(appMenu, "addItem:", msg(cls("NSMenuItem"), "separatorItem"));
    msgv_id(appMenu, "addItem:", menuItem("Quit Dr Text", "terminate:", "q", NULL));
    msgv_id(appItem, "setSubmenu:", appMenu);
    msgv_id(mainMenu, "addItem:", appItem);

    // -- File menu --
    id fileItem = msg(msg(cls("NSMenuItem"), "alloc"), "init");
    id fileMenu = msg_id(msg(cls("NSMenu"), "alloc"), "initWithTitle:", nsstr("File"));

    // Create controller for file actions (ObjC class backed by interpreted code)
    id controller = msg(msg(cls("EditorController"), "alloc"), "init");

    msgv_id(fileMenu, "addItem:", menuItem("New",     "doNew:",    "n", controller));
    msgv_id(fileMenu, "addItem:", menuItem("Open...", "doOpen:",   "o", controller));
    msgv_id(fileMenu, "addItem:", msg(cls("NSMenuItem"), "separatorItem"));
    msgv_id(fileMenu, "addItem:", menuItem("Save",       "doSave:",   "s", controller));
    msgv_id(fileMenu, "addItem:", menuItem("Save As...", "doSaveAs:", "S", controller));
    msgv_id(fileItem, "setSubmenu:", fileMenu);
    msgv_id(mainMenu, "addItem:", fileItem);

    // -- Edit menu (responder chain handles these automatically) --
    id editItem = msg(msg(cls("NSMenuItem"), "alloc"), "init");
    id editMenu = msg_id(msg(cls("NSMenu"), "alloc"), "initWithTitle:", nsstr("Edit"));
    msgv_id(editMenu, "addItem:", menuItem("Undo",       "undo:",      "z", NULL));
    msgv_id(editMenu, "addItem:", menuItem("Redo",       "redo:",      "Z", NULL));
    msgv_id(editMenu, "addItem:", msg(cls("NSMenuItem"), "separatorItem"));
    msgv_id(editMenu, "addItem:", menuItem("Cut",        "cut:",       "x", NULL));
    msgv_id(editMenu, "addItem:", menuItem("Copy",       "copy:",      "c", NULL));
    msgv_id(editMenu, "addItem:", menuItem("Paste",      "paste:",     "v", NULL));
    msgv_id(editMenu, "addItem:", menuItem("Select All", "selectAll:", "a", NULL));
    msgv_id(editItem, "setSubmenu:", editMenu);
    msgv_id(mainMenu, "addItem:", editItem);

    id app = msg(cls("NSApplication"), "sharedApplication");
    msgv_id(app, "setMainMenu:", mainMenu);
}

// ---------------------------------------------------------------------------
// AppDelegate methods — also interpreted functions registered as ObjC methods
// ---------------------------------------------------------------------------

void appDidFinishLaunching(id self, SEL _cmd, id note){
    createMenuBar();

    // Create window
    CGRect frame = {{200, 200}, {800, 600}};
    unsigned long style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable
                        | NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable;
    id window = msg(cls("NSWindow"), "alloc");
    window = ((id(*)(id, SEL, CGRect, unsigned long, unsigned long, _Bool))objc_msgSend)(
        window, sel("initWithContentRect:styleMask:backing:defer:"),
        frame, style, 2UL /* NSBackingStoreBuffered */, (_Bool)0);
    g_window = window;
    msgv_id(window, "setTitle:", nsstr("Untitled"));

    // Create scroll view
    id scrollView = msg_rect(msg(cls("NSScrollView"), "alloc"), "initWithFrame:", frame);
    msgv_bool(scrollView, "setHasVerticalScroller:", 1);
    msgv_bool(scrollView, "setHasHorizontalScroller:", 0);
    msgv_long(scrollView, "setAutoresizingMask:", 18); // width + height

    // Create text view
    id textView = msg_rect(msg(cls("NSTextView"), "alloc"), "initWithFrame:", frame);
    g_textView = textView;

    // Configure text editing
    msgv_bool(textView, "setEditable:", 1);
    msgv_bool(textView, "setSelectable:", 1);
    msgv_bool(textView, "setRichText:", 0);
    msgv_bool(textView, "setAllowsUndo:", 1);
    msgv_bool(textView, "setVerticallyResizable:", 1);
    msgv_bool(textView, "setHorizontallyResizable:", 0);
    msgv_long(textView, "setAutoresizingMask:", 2); // width

    // Set monospace font
    id font = msg_double2(cls("NSFont"), "monospacedSystemFontOfSize:weight:", 13.0, 0.0);
    msgv_id(textView, "setFont:", font);

    // Text container tracks scroll view width
    id tc = msg(textView, "textContainer");
    msgv_bool(tc, "setWidthTracksTextView:", 1);

    // Max text size
    CGSize big = {1e7, 1e7};
    ((void(*)(id, SEL, CGSize))objc_msgSend)(textView, sel("setMaxSize:"), big);

    // Assemble views
    msgv_id(scrollView, "setDocumentView:", textView);
    msgv_id(window, "setContentView:", scrollView);

    // Minimum window size
    CGSize minSize = {400, 300};
    ((void(*)(id, SEL, CGSize))objc_msgSend)(window, sel("setMinSize:"), minSize);

    // Open file from command line argument if given
    const char* initialPath = __argv(1, NULL);
    if(initialPath){
        id contents = nsstr_from_file(initialPath);
        if(contents){
            msgv_id(g_textView, "setString:", contents);
            g_filePath = msg(nsstr(initialPath), "retain");
            msgv_id(window, "setTitle:", g_filePath);
        }
    }

    // Show
    msgv_id(window, "makeKeyAndOrderFront:", (id)0);
    id app = msg(cls("NSApplication"), "sharedApplication");
    msgv_bool(app, "activateIgnoringOtherApps:", 1);
}

_Bool appShouldTerminate(id self, SEL _cmd, id app){
    return 1;
}

// ---------------------------------------------------------------------------
// Register ObjC classes and run
// ---------------------------------------------------------------------------

// Create the AppDelegate class — an NSObject subclass whose methods are
// implemented by the interpreted functions above.
Class AppDelegate = objc_allocateClassPair(
    (Class)objc_getClass("NSObject"), "AppDelegate", 0);
class_addMethod(AppDelegate,
    sel("applicationDidFinishLaunching:"),
    (IMP)appDidFinishLaunching, "v@:@");
class_addMethod(AppDelegate,
    sel("applicationShouldTerminateAfterLastWindowClosed:"),
    (IMP)appShouldTerminate, "B@:@");
objc_registerClassPair(AppDelegate);

// Create the EditorController class — handles File menu actions.
Class EditorController = objc_allocateClassPair(
    (Class)objc_getClass("NSObject"), "EditorController", 0);
class_addMethod(EditorController, sel("doNew:"),    (IMP)doNew,    "v@:@");
class_addMethod(EditorController, sel("doOpen:"),   (IMP)doOpen,   "v@:@");
class_addMethod(EditorController, sel("doSave:"),   (IMP)doSave,   "v@:@");
class_addMethod(EditorController, sel("doSaveAs:"), (IMP)doSaveAs, "v@:@");
objc_registerClassPair(EditorController);

// Boot the application
id pool = msg(msg(cls("NSAutoreleasePool"), "alloc"), "init");

// Set app name for menu bar (unbundled app trick)
id bundle = msg(cls("NSBundle"), "mainBundle");
id info = msg(bundle, "infoDictionary");
((void(*)(id, SEL, id, id))objc_msgSend)(
    info, sel("setObject:forKey:"), nsstr("Dr Text"), nsstr("CFBundleName"));

id app = msg(cls("NSApplication"), "sharedApplication");
msgv_long(app, "setActivationPolicy:", 0L); // NSApplicationActivationPolicyRegular

id delegate = msg(msg(cls("AppDelegate"), "alloc"), "init");
msgv_id(app, "setDelegate:", delegate);

// Run the event loop (does not return — run loop manages its own pools)
msgv(app, "run");
