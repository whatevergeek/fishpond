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
    std::string result = utf8 ? utf8 : "Embedded Python evaluation failed";
    PyObject* line = value ? PyObject_GetAttrString(value, "lineno") : nullptr;
    if (line != nullptr) {
        const auto number = PyLong_AsLong(line);
        if (! PyErr_Occurred() && number > 0)
            result = "FP_PYTHON_ERROR: line " + std::to_string(number) + ": " + result;
        Py_DECREF(line);
    }
    PyErr_Clear();
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
            "_fishpond_players = {}\n"
            "class _FishpondPattern:\n"
            "    def __init__(self, notes, target, keywords):\n"
            "        self.notes = notes\n"
            "        self.target = target\n"
            "        self.keywords = keywords\n"
            "class _FishpondPlayer:\n"
            "    def __init__(self, name):\n"
            "        self.name = name\n"
            "    def __rshift__(self, pattern):\n"
            "        _fishpond_players[self.name] = pattern\n"
            "        return self\n"
            "    def __mul__(self, pattern):\n"
            "        return self.__rshift__(pattern)\n"
            "def n(notes, *, target=None, **keywords):\n"
            "    if not isinstance(target, str) or not target:\n"
            "        raise ValueError('FP_TARGET_REQUIRED: n() requires target=\"channel\"')\n"
            "    return _FishpondPattern(notes, target, keywords)\n"
            "def silence():\n"
            "    _fishpond_players.clear()\n"
            "def panic():\n"
            "    _fishpond_players.clear()\n"
            "for _letter in 'abcdefghijklmnopqrstuvwxyz':\n"
            "    globals()['P' + _letter] = _FishpondPlayer('P' + _letter)\n",
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
