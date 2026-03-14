#include <exception>
#include <iostream>

#include "app/simulation_app.h"

int main(int argc, char* argv[]) {
    try {
        const ship_sim::CliOptions options = ship_sim::parseCliOptions(argc, argv);
        ship_sim::SimulationApp app;
        return app.run(options);
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << '\n';
        return 1;
    }
}
