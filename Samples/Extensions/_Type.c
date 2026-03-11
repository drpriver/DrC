#define log(fmt, ...) printf("%s:%d: " fmt, __FILE__, __LINE__ __VA_OPT__(,) __VA_ARGS__)
int printf(const char*, ...);
_Type t = int;
// special fields
log("%s.is_integer: %s\n", t.name, t.is_integer?"true":"false");
t = float;
log("%s.is_integer: %s\n", t.name, t.is_integer?"true":"false");
t = int(int);
log("%s.is_integer: %s\n", t.name, t.is_integer?"true":"false");
log("%s.is_callable: %s\n", t.name, t.is_callable?"true":"false");
// special methods
log("%s.is_callable_with(int): %s\n", t.name, t.is_callable_with(int)?"true":"false");
// this one is type-or-expression
log("%s.is_callable_with(1): %s\n", t.name, t.is_callable_with(1)?"true":"false");
t = int[4];
log("%s.count: %zu\n", t.name, t.count);
log("%s.is_castable_to(int*): %s\n", t.name, t.is_castable_to(int*)?"true":"false");

// parenthesized types can be used as values
// disambiguate from compound literals and casts
log("%s\n", (int).name);

// usable in static if: 
static if((int[4]).is_castable_to(int*)){
    log("true!\n");
}
else {
    log("false!\n");
}

_Type id(_Type T){ return T; }

// we know id() takes type, so parse type-name instead of expression
log("%s\n", id(int).name);

_Type ty = _Type;
log("%s\n", ty.name);

_Type intty(void){ return int;}
ty = id(intty());
log("%s\n", ty.name);
