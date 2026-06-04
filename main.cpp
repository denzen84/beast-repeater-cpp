#include "Application.hpp"
#include <iostream>
#include <stdexcept>

int main(int argc, char* argv[]) {
    try {
        Application app;
        return app.run(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << "Fatal: " << e.what() << '\n';
        return 1;
    }
}
