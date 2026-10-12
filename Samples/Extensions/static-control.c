// Generate a configuration printer from the fields of a struct.
// Run with: drc Samples/Extensions/static-control.c
#include <stdio.h>

struct Config {
    const char* host;
    int port;
    _Bool tracing;
    double timeout;
};

void print_config(const struct Config* config){
    // Generate one printing sequence per field
    static for(size_t i = 0; i < struct Config.fields; i++){
        { // Each generated sequence gets its own local declarations.
            constexpr field = struct Config.field(i);
            constexpr T = field.type;
            const T* value = (const T*)((const char*)config + field.offset);
            printf("%.*s = ", (int)field.name.count, field.name.data);

            // Strings use a special path, then join the common line ending.
            static if(T == const char*){
                printf("\"%s\"", *value);
                static goto line_end;
            }

            // Only the matching formatter is parsed for each numeric field.
            static switch(T){
                static case int: printf("%d", *value); static break;
                static case _Bool: printf("%s", *value ? "true" : "false"); static break;
                static case double: printf("%.1f", *value); static break;
                static default: static_assert(0, "unsupported configuration field");
            }

            // This compile-time label is reusable in every loop iteration.
            // If you remove `static`, you'd get a redef error, as normal
            // labels have function scope.
            static line_end:
            putchar('\n');
        }
    }
}

struct Config config = {"localhost", 8080, 1, 2.5};
print_config(&config);
