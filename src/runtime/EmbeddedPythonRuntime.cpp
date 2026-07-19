#include "runtime/EmbeddedPythonRuntime.h"

#include <Python.h>

#include <optional>

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

std::optional<double> clockTempo(PyObject* globals)
{
    auto* clock = PyDict_GetItemString(globals, "clock");
    if (clock == nullptr)
        return std::nullopt;
    auto* bpm = PyObject_GetAttrString(clock, "bpm");
    if (bpm == nullptr) {
        PyErr_Clear();
        return std::nullopt;
    }
    const auto value = PyFloat_AsDouble(bpm);
    const auto valid = ! PyErr_Occurred();
    PyErr_Clear();
    Py_DECREF(bpm);
    return valid ? std::optional<double>(value) : std::nullopt;
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
            "class _FishpondClock:\n"
            "    def __init__(self):\n"
            "        self._bpm = 120.0\n"
            "        self._beat = 0.0\n"
            "        self._running = True\n"
            "    @property\n"
            "    def bpm(self):\n"
            "        return self._bpm\n"
            "    @bpm.setter\n"
            "    def bpm(self, value):\n"
            "        if not isinstance(value, (int, float)) or isinstance(value, bool) or not 30 <= value <= 300:\n"
            "            raise ValueError('FP_TEMPO_INVALID: bpm must be between 30 and 300')\n"
            "        self._bpm = float(value)\n"
            "    tempo = bpm\n"
            "    @property\n"
            "    def beat(self):\n"
            "        return self._beat\n"
            "    @property\n"
            "    def bar(self):\n"
            "        return int(self._beat // 4)\n"
            "    @property\n"
            "    def phase(self):\n"
            "        return self._beat % 4\n"
            "    @property\n"
            "    def running(self):\n"
            "        return self._running\n"
            "    def start(self):\n"
            "        self._running = True\n"
            "    def pause(self):\n"
            "        self._running = False\n"
            "    def stop(self):\n"
            "        self._running = False\n"
            "        self._beat = 0.0\n"
            "    def advance(self, beats):\n"
            "        if not isinstance(beats, (int, float)) or isinstance(beats, bool) or beats < 0:\n"
            "            raise ValueError('FP_BEAT_INVALID: beats must be a non-negative number')\n"
            "        if self._running:\n"
            "            self._beat += float(beats)\n"
            "clock = _FishpondClock()\n"
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
    const auto priorTempo = clockTempo(static_cast<PyObject*>(globals));
    PyObject* result = PyRun_StringFlags(source.c_str(), Py_file_input,
                                         static_cast<PyObject*>(globals), static_cast<PyObject*>(globals), nullptr);
    if (result == nullptr) {
        diagnostic = pythonError();
        PyGILState_Release(state);
        return { false, diagnostic };
    }
    Py_DECREF(result);
    const auto currentTempo = clockTempo(static_cast<PyObject*>(globals));
    PyGILState_Release(state);
    diagnostic.clear();
    return { true, "Executed", currentTempo != priorTempo ? currentTempo : std::nullopt };
}
}
