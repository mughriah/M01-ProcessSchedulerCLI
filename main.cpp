
#include "console.h"

#include <exception>
#include <iostream>

int main() {
    try {
        Console console;
        console.run();
    }
    catch (const std::exception& exception) {
        std::cerr
            << "Fatal error: "
            << exception.what()
            << '\n';

        return 1;
    }

    return 0;
}
