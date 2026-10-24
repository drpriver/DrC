#pragma typedef on
struct Bar {
    int x, y;
};
struct Baz {
    Bar b;
    int z;
};
struct Qux {
    Bar a;
    Baz c;
    float i;
};
struct Wux {
    Qux q, w;
    Bar b;
    Baz c;
};
struct Foo {
    Bar b[2];
    Baz ba;
    Qux q;
    Wux w;
};


struct PrintCursor {
    _Type type;
    size_t offset, depth, index;
    const PrintCursor* parent;
};

#defblock PRINTER(T)
// Each descent saves the current iteration's cursor as its parent. On return,
// advance that parent's child index. The entire traversal runs at compile time.
static for(PrintCursor c = {.type=T}; c.type != void;
    c = c.type.is_struct && c.index < c.type.fields?  (PrintCursor){c.type.field(c.index).type, c.offset + c.type.field(c.index).offset, c.depth + 1, 0, &c}
        : c.type.is_array && c.index < c.type.count?  (PrintCursor){c.type.element_type, c.offset + c.index * c.type.element_type.sizeof_, c.depth + 1, 0, &c}
        : c.parent ?                                  (PrintCursor){c.parent->type, c.parent->offset, c.parent->depth, c.parent->index + 1, c.parent->parent}
        :                                             (PrintCursor){.type=void}){{
    // Print each node only when first entering it.
    static if(c.index != 0) static continue;
    printf("%*s", (int)(c.depth * 2), "");
    static if(!c.parent)
        printf("%s", #T);
    else static if(c.parent->type.is_array)
        printf("[%zu]", c.parent->index);
    else {
        constexpr field = c.parent->type.field(c.parent->index);
        static_assert(!field.is_bitfield, "PRINTER does not support bitfields");
        printf("%.*s", (int)field.name.count, field.name.data);
    }
    static if(c.type.is_struct || c.type.is_array)
        printf(":\n");
    else {
        static switch(c.type){
            static case int:
                printf(" = %d\n", *(const int*)((const char*)f + c.offset));
                static break;
            static case float:
                printf(" = %g\n", *(const float*)((const char*)f + c.offset));
                static break;
            static default:
                static_assert(c.type == int, "PRINTER supports int leaves");
                static break;
        }
    }
}}
#endblock

void
print_foo(const Foo* f){
    PRINTER(Foo)
}

Foo f = {__let(it(x), x __mixin(","), __for(0, 30, it))};
f.print_foo();
