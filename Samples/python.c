#ifdef __linux__
#pragma pkg_config "python3-embed"
#include <Python.h>
#else
#pragma lib "Python"
#include <Python/Python.h>
#endif
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

static PyObject* PyInit_native(void);
PyImport_AppendInittab("native", &PyInit_native);
Py_Initialize();
PyRun_SimpleString(
    "import native\n"
    "print('3 + 4 =', native.add(3, 4))\n"
    "print('100 + 200 =', native.add(100, 200))\n"
);
Py_Finalize();

static PyObject*
PyInit_native(void){
    return PyModule_Create(&module);
}
