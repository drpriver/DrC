void print_dynamic(_Type T, const void* p){
    switch(T){
        case float:              printf("%g", *(const float*)p); break;
        case double:             printf("%g", *(const double*)p); break;
        case char:               printf("%d", *(const char*)p); break;
        case signed char:        printf("%d", *(const signed char*)p); break;
        case unsigned char:      printf("%u", *(const unsigned char*)p); break;
        case short:              printf("%d", *(const short*)p); break;
        case unsigned short:     printf("%u", *(const unsigned short*)p); break;
        case int:                printf("%d", *(const int*)p); break;
        case unsigned int:       printf("%u", *(const unsigned int*)p); break;
        case long:               printf("%ld", *(const long*)p); break;
        case unsigned long:      printf("%lu", *(const unsigned long*)p); break;
        case long long:          printf("%lld", *(const long long*)p); break;
        case unsigned long long: printf("%llu", *(const unsigned long long*)p); break;
        case char*:
        case const char*:        printf("%s", *(const char**)p); break;
        case void*:
        case const void*:        printf("%p", *(const void**)p); break;
        case typeof(nullptr):    printf("(NULL)"); break;
        case _Type:              printf("%s", (*(_Type*)p).name); break;
        default:
            if(T.is_slice){
                const E = T.element_type;
                const struct {
                    size_t count;
                    void* data;
                } *slice = p;
                printf("[");
                for(size_t i = 0, count=slice.count; i < count; i++){
                    if(i) printf(", ");
                    print_dynamic(E, slice.data+i*E.sizeof_);
                }
                printf("]");
                break;
            }
            if(T.is_struct){
                printf("{");
                const char* base = p;
                for(size_t i = 0, n = T.fields; i < n; i++){
                    if(i) printf(", ");
                    auto f = T.field(i);
                    printf(".%s=", f.name);
                    if(f.is_bitfield){
                        printf("<bitfield>");
                        continue;
                    }
                    const char* pf = base + f.offset;
                    print_dynamic(f.type, pf);
                }
                printf("}");
                break;
            }
            if(T.is_pointer){
                _Type P = T.pointee;
                const void* pointer = *(const void**)p;
                if(!pointer){
                    printf("(NULL)");
                    break;
                }
                if(P.is_array && P.element_type.unqual == char){
                    printf("%.*s", (int)P.count, (const char*)pointer);
                    break;
                }
                print_dynamic(P, pointer);
                break;
            }
            if(T.is_array){
                printf("[");
                for(size_t i = 0, count=T.count; i < count; i++){
                    if(i) printf(", ");
                    const E = T.element_type.unqual;
                    print_dynamic(E, p+i*E.sizeof_);
                }
                printf("]");
                break;
            }
            printf("Unhandled T: %s\n", T.name);
        break;
    }
}
_Any print(_Any args[:]){
    for(size_t i = 0; i < _Countof args; i++){
        if(i) printf(" ");
        print_dynamic(args[i].type, args[i].payload);
    }
    printf("\n");
}

#define print(...) print((_Any[]){__VA_ARGS__})
print("hello", "world");
print(1);
struct S { int x, y;} s = {1, 2};
print("s =", s);
struct Big {struct S a, b;} b = {1, 2, 3, 4};
print("b =", &b);
struct A {struct S arr[4];} a = {11, 12, 13, 14, 15, 16, 17, 18};
print("a =", &a);
int x[5] = {5, 4, 3, 2, 1};
print(x);
print(nullptr);
int slice[:] = x[:3];
print("slice =", &slice);

print(int, float, const char*);
