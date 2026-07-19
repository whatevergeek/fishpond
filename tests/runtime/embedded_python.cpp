#include "runtime/EmbeddedPythonRuntime.h"

#include <iostream>
#include <string>

int main(int argc, char** argv)
{
    fishpond::EmbeddedPythonRuntime runtime;
    if (! runtime.ready()) {
        std::cerr << runtime.lastDiagnostic() << '\n';
        return 1;
    }

    const std::string test = argc == 2 ? argv[1] : "";
    if (test == "persistent")
        return runtime.evaluate("tempo = 120").accepted && runtime.evaluate("assert tempo == 120").accepted ? 0 : 1;
    if (test == "error-isolation")
        return runtime.evaluate("player = 'valid'").accepted
            && ! runtime.evaluate("raise RuntimeError('broken evaluation')").accepted
            && runtime.evaluate("assert player == 'valid'").accepted ? 0 : 1;

    return 2;
}
