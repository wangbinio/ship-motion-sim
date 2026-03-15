#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

#include <QApplication>
#include <QDir>
#include <QIcon>

#include "gui/main_window.h"

namespace {

// 解析命令行配置路径，未显式传入时回退到默认 XML 配置。
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

    if (!config_path.empty()) {
        return config_path;
    }

    const QString default_path =
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("../config/default_config.xml"));
    return QDir::cleanPath(default_path).toStdString();
}

}  // namespace

int main(int argc, char* argv[]) {
    try {
        QApplication app(argc, argv);
        QApplication::setWindowIcon(QIcon(QStringLiteral(":/app.png")));
        ship_sim::MainWindow window(parseConfigPath(argc, argv));
        window.show();
        return app.exec();
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << '\n';
        return 1;
    }
}
