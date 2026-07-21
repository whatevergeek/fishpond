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

std::optional<double> masterVolume(PyObject* globals)
{
    auto* master = PyDict_GetItemString(globals, "master");
    if (master == nullptr)
        return std::nullopt;
    auto* volume = PyObject_GetAttrString(master, "volume");
    if (volume == nullptr) {
        PyErr_Clear();
        return std::nullopt;
    }
    const auto value = PyFloat_AsDouble(volume);
    const auto valid = ! PyErr_Occurred();
    PyErr_Clear();
    Py_DECREF(volume);
    return valid ? std::optional<double>(value) : std::nullopt;
}

bool consoleClearRequested(PyObject* globals)
{
    auto* console = PyDict_GetItemString(globals, "console");
    if (console == nullptr)
        return false;
    auto* requested = PyObject_GetAttrString(console, "_clear_requested");
    if (requested == nullptr) {
        PyErr_Clear();
        return false;
    }
    const auto value = PyObject_IsTrue(requested);
    PyErr_Clear();
    Py_DECREF(requested);
    return value > 0;
}

std::string consoleOutput(PyObject* globals)
{
    auto* console = PyDict_GetItemString(globals, "console");
    if (console == nullptr)
        return {};
    auto* output = PyObject_CallMethod(console, "_take_output", nullptr);
    if (output == nullptr) {
        PyErr_Clear();
        return {};
    }
    const auto* utf8 = PyUnicode_Check(output) ? PyUnicode_AsUTF8(output) : nullptr;
    std::string result = utf8 != nullptr ? utf8 : "";
    PyErr_Clear();
    Py_DECREF(output);
    return result;
}

void beginConsoleEvaluation(PyObject* globals)
{
    auto* console = PyDict_GetItemString(globals, "console");
    if (console == nullptr)
        return;
    auto* result = PyObject_CallMethod(console, "_begin_evaluation", nullptr);
    Py_XDECREF(result);
    PyErr_Clear();
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
            "class _FishpondMaster:\n"
            "    def __init__(self):\n"
            "        self._volume = 0.0\n"
            "    @property\n"
            "    def volume(self):\n"
            "        return self._volume\n"
            "    @volume.setter\n"
            "    def volume(self, value):\n"
            "        if not isinstance(value, (int, float)) or isinstance(value, bool) or not -60 <= value <= 0:\n"
            "            raise ValueError('FP_MASTER_VOLUME_INVALID: volume must be between -60 and 0 dB')\n"
            "        self._volume = float(value)\n"
            "master = _FishpondMaster()\n"
            "class _FishpondConsole:\n"
            "    def __init__(self):\n"
            "        self._clear_requested = False\n"
            "        self._output = []\n"
            "    def clear(self):\n"
            "        self._clear_requested = True\n"
            "    def _begin_evaluation(self):\n"
            "        self._clear_requested = False\n"
            "        self._output.clear()\n"
            "    def _write(self, text):\n"
            "        self._output.append(str(text))\n"
            "    def _take_output(self):\n"
            "        output = ''.join(self._output)\n"
            "        self._output.clear()\n"
            "        return output\n"
            "console = _FishpondConsole()\n"
            "def _fishpond_print(*values, sep=' ', end='\\n', file=None, flush=False):\n"
            "    if not isinstance(sep, str) or not isinstance(end, str):\n"
            "        raise TypeError('sep and end must be strings')\n"
            "    console._write(sep.join(str(value) for value in values) + end)\n"
            "print = _fishpond_print\n"
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
            "    def stop(self):\n"
            "        _fishpond_players.pop(self.name, None)\n"
            "def n(notes, *, target=None, **keywords):\n"
            "    if not isinstance(target, str) or not target:\n"
            "        raise ValueError('FP_TARGET_REQUIRED: n() requires target=\"channel\"')\n"
            "    return _FishpondPattern(notes, target, keywords)\n"
            "def silence(*players):\n"
            "    if players:\n"
            "        for player in players:\n"
            "            _fishpond_players.pop(player.name, None)\n"
            "    else:\n"
            "        _fishpond_players.clear()\n"
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
    const auto priorMasterVolume = masterVolume(static_cast<PyObject*>(globals));
    beginConsoleEvaluation(static_cast<PyObject*>(globals));
    PyObject* result = PyRun_StringFlags(source.c_str(), Py_file_input,
                                         static_cast<PyObject*>(globals), static_cast<PyObject*>(globals), nullptr);
    if (result == nullptr) {
        diagnostic = pythonError();
        const auto clearRequested = consoleClearRequested(static_cast<PyObject*>(globals));
        const auto output = consoleOutput(static_cast<PyObject*>(globals));
        PyGILState_Release(state);
        return { false, diagnostic, {}, {}, clearRequested, output };
    }
    Py_DECREF(result);
    const auto currentTempo = clockTempo(static_cast<PyObject*>(globals));
    const auto currentMasterVolume = masterVolume(static_cast<PyObject*>(globals));
    const auto clearRequested = consoleClearRequested(static_cast<PyObject*>(globals));
    const auto output = consoleOutput(static_cast<PyObject*>(globals));
    PyGILState_Release(state);
    diagnostic.clear();
    return { true, "Executed", currentTempo != priorTempo ? currentTempo : std::nullopt,
             currentMasterVolume != priorMasterVolume ? currentMasterVolume : std::nullopt,
             clearRequested, output };
}
}
