#include "runtime/EmbeddedPythonRuntime.h"

#include <Python.h>

namespace fishpond {
namespace {
std::string pythonError()
{
    PyObject *type = nullptr, *value = nullptr, *traceback = nullptr;
    PyErr_Fetch(&type, &value, &traceback);
    PyErr_NormalizeException(&type, &value, &traceback);
    PyObject* text = value ? PyObject_Str(value) : nullptr;
    const char* utf8 = text ? PyUnicode_AsUTF8(text) : nullptr;
    const std::string result = utf8 ? utf8 : "Embedded Python evaluation failed";
    Py_XDECREF(text); Py_XDECREF(type); Py_XDECREF(value); Py_XDECREF(traceback);
    return result;
}
}

EmbeddedPythonRuntime::EmbeddedPythonRuntime()
{
    if (! Py_IsInitialized()) {
        PyConfig config;
        PyConfig_InitPythonConfig(&config);
        config.parse_argv = 0;
        const auto status = Py_InitializeFromConfig(&config);
        PyConfig_Clear(&config);
        if (PyStatus_Exception(status)) {
            diagnostic = "Unable to initialize embedded CPython 3.12";
            return;
        }
    }
    const auto state = PyGILState_Ensure();
    globals = PyDict_New();
    if (globals != nullptr) {
        PyDict_SetItemString(static_cast<PyObject*>(globals), "__builtins__", PyEval_GetBuiltins());
        PyObject* bootstrapResult = PyRun_StringFlags(
            "class _FishpondPlayer:\n"
            "    def __rshift__(self, value):\n"
            "        return value\n"
            "def n(*notes, **keywords):\n"
            "    return (notes, keywords)\n"
            "Pa = _FishpondPlayer()\n",
            Py_file_input, static_cast<PyObject*>(globals), static_cast<PyObject*>(globals), nullptr);
        if (bootstrapResult != nullptr) {
            Py_DECREF(bootstrapResult);
            isReady = true;
        } else {
            diagnostic = pythonError();
            Py_CLEAR(globals);
        }
    } else {
        diagnostic = pythonError();
    }
    PyGILState_Release(state);
}

EmbeddedPythonRuntime::~EmbeddedPythonRuntime()
{
    if (globals == nullptr || ! Py_IsInitialized())
        return;
    const auto state = PyGILState_Ensure();
    Py_DECREF(static_cast<PyObject*>(globals));
    globals = nullptr;
    PyGILState_Release(state);
}

PythonEvaluationResult EmbeddedPythonRuntime::evaluate(const std::string& source)
{
    if (! isReady)
        return { false, diagnostic };
    const auto state = PyGILState_Ensure();
    PyObject* result = PyRun_StringFlags(source.c_str(), Py_file_input,
                                         static_cast<PyObject*>(globals), static_cast<PyObject*>(globals), nullptr);
    if (result == nullptr) {
        diagnostic = pythonError();
        PyGILState_Release(state);
        return { false, diagnostic };
    }
    Py_DECREF(result);
    PyGILState_Release(state);
    diagnostic.clear();
    return { true, "Executed" };
}
}
