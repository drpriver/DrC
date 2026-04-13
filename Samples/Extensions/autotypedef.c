#pragma typedef on
struct Foo {
    struct F { int y;} f;
    int x;
} _f;

Foo f = {3};
F in = {4};

#pragma typedef off
struct Bar {
    int x;
};
// Bar f2 = {2}; /// error
