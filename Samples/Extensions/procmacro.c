int printf(const char*, ...);
int puts(const char*);
// Example 1: compile-time string hashing for switch dispatch

unsigned long hash(const char* s){
    unsigned long h = 5381;
    while(*s)
        h = h * 33 + *s++;
    return h;
}
#pragma procmacro hash

void handle(const char* cmd){
    switch((hash)(cmd)){
        case hash("quit"):    printf("  -> quit\n"); break;
        case hash("help"):    printf("  -> help\n"); break;
        case hash("run"):     printf("  -> run\n"); break;
        default:              printf("  -> unknown\n"); break;
    }
}

const char* commands[] = {"help", "run", "quit", "foobar"};
for(int i = 0; i < 4; i++){
    printf("handle(\"%s\"):", commands[i]);
    handle(commands[i]);
}

// Example 2: code generation with __mixin

const char* gen_vec(int n){
    char buf[4096];
    int off = 0;
    off += snprintf(buf + off, sizeof buf - off,
        "typedef struct Vec%d Vec%d;\nstruct Vec%d {\n", n, n, n);
    for(int i = 0; i < n; i++)
        off += snprintf(buf + off, sizeof buf - off, "    float v%d;\n", i);
    off += snprintf(buf + off, sizeof buf - off, "};");
    return __builtin_intern(buf);
}
#pragma procmacro gen_vec

__mixin(gen_vec(2))  // generates: struct Vec2 { float v0; float v1; };
__mixin(gen_vec(3))  // generates: struct Vec3 { float v0; float v1; float v2; };
__mixin(gen_vec(4))  // generates: struct Vec4 { float v0; float v1; float v2; float v3; };

Vec2 a = {1.0f, 2.0f};
Vec3 b = {1.0f, 2.0f, 3.0f};
Vec4 c = {1.0f, 2.0f, 3.0f, 4.0f};
puts(gen_vec(2));
printf("Vec2: {%.0f, %.0f}\n", a.v0, a.v1);
puts(gen_vec(3));
printf("Vec3: {%.0f, %.0f, %.0f}\n", b.v0, b.v1, b.v2);
puts(gen_vec(4));
printf("Vec4: {%.0f, %.0f, %.0f, %.0f}\n", c.v0, c.v1, c.v2, c.v3);
