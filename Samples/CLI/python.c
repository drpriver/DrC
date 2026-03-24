#ifdef __APPLE__
#pragma framework "Python"
#include <Python/Python.h>
#elif defined __linux__
#pragma pkg_config "python3-embed"
#include <Python.h>
#else
// untested windows
#include <Python/Python.h>
#pragma lib "Python"
#endif
#include <stdarg.h>
#include <stdio.h>

static PyObject*
py_add(PyObject* self, PyObject* args){
    (void)self;
    int a, b;
    if(!PyArg_ParseTuple(args, "ii", &a, &b))
        return NULL;
    printf("%s:%d: %s: hello from C (%d, %d)\n", __FILE__, __LINE__, __func__, a, b);
    return PyLong_FromLong(a + b);
}

static PyMethodDef methods[] = {
    {"add", py_add, METH_VARARGS, "Add two integers."},
    {NULL, NULL, 0, NULL},
};

static PyModuleDef module = {
    PyModuleDef_HEAD_INIT,
    .m_name = "native",
    .m_doc = "A native module registered from C.",
    .m_size = -1,
    .m_methods = methods,
};

static PyObject*
PyInit_native(void){
    return PyModule_Create(&module);
}

static PyObject* PyInit_native(void);
PyImport_AppendInittab("native", &PyInit_native);
Py_Initialize();
PyRun_SimpleString(
    "import native\n"
    "print('3 + 4 =', native.add(3, 4))\n"
    "print('100 + 200 =', native.add(100, 200))\n"
);

// Don't write your code like this at home kids,
// but it shows we're the real deal.
static void
crazy_call(int components, ...){
    if(components < 2) return;
    va_list va;
    va_start(va);
    PyObject *o = PyImport_ImportModule(va_arg(va, const char *));
    PyObject* prev;
    for(int i = 1; i < components; i++){
        prev = o;
        o = PyObject_GetAttrString(o, va_arg(va, const char *));
        Py_DECREF(prev);
    }
    const char* fmt = va_arg(va, const char*);
    PyObject* tup = Py_VaBuildValue(fmt, va);
    va_end(va);
    PyObject* result = PyObject_Call(o, tup, NULL);
    Py_DECREF(o);
    Py_DECREF(tup);
    if(result != Py_None){
        PyObject_Print(result, stdout, 0);
        printf("\n");
    }
    Py_DECREF(result);
}
crazy_call(2, "builtins", "pow", "(ii)", 2, 10);
crazy_call(2, "builtins", "print", "(s)", "hello from crazy_call");
crazy_call(2, "builtins", "abs", "(i)", -42);
crazy_call(2, "builtins", "max", "(iii)", 3, 7, 5);
crazy_call(3, "os", "path", "join", "(ss)", "/usr", "local");
crazy_call(2, "math", "factorial", "(i)", 10);
crazy_call(2, "math", "gcd", "(ii)", 12, 8);
crazy_call(2, "math", "isqrt", "(i)", 144);
crazy_call(2, "operator", "mul", "(si)", "ha", 3);
crazy_call(2, "native", "add", "(ii)", 4, 4);

Py_Finalize();

