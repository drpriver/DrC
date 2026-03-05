#ifdef __linux__
#pragma pkg_config "python3-embed"
#include <Python.h>
#else
#pragma lib "Python"
#include <Python/Python.h>
#endif

Py_Initialize();
PyRun_SimpleString(
    "import sys\n"
    "print('Hello from Python', sys.version)\n"
    "print('2 + 2 =', 2 + 2)\n"
);
Py_Finalize();
