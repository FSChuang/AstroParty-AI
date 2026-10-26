#include <cstdlib>
#include <exception>
#include <iostream>

#include "Application.hpp"

int main() {
    try {
        astroparty::Application application;
        return application.run();
    } catch (const std::exception& error) {
        std::cerr << "Fatal error: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
