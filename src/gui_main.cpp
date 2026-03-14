#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

#include <QApplication>

#include "gui/main_window.h"

namespace {

std::string parseConfigPath(int argc, char* argv[]) {
    std::string config_path;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--config" && i + 1 < argc) {
            config_path = argv[++i];
        } else {
            throw std::runtime_error("Unknown or incomplete argument: " + arg);
        }
    }
    return config_path;
}

}  // namespace

int main(int argc, char* argv[]) {
    try {
        QApplication app(argc, argv);
        ship_sim::MainWindow window(parseConfigPath(argc, argv));
        window.show();
        return app.exec();
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << '\n';
        return 1;
    }
}
