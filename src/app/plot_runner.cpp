#include "app/plot_runner.h"

#include <filesystem>
#include <stdexcept>

#include <QProcess>
#include <QStringList>

namespace ship_sim {

void PlotRunner::run(const PlotRequest& request) {
    if (request.script_path.empty()) {
        throw std::runtime_error("Plot script path must not be empty");
    }
    if (request.input_csv_path.empty()) {
        throw std::runtime_error("Plot input CSV path must not be empty");
    }
    if (request.output_path.empty()) {
        throw std::runtime_error("Plot output path must not be empty");
    }

    if (!std::filesystem::exists(request.script_path)) {
        throw std::runtime_error("Plot script does not exist: " + request.script_path);
    }

    QProcess process;
    process.setProgram(QStringLiteral("python3"));

    QStringList arguments;
    arguments << QString::fromStdString(request.script_path) << QStringLiteral("--input")
              << QString::fromStdString(request.input_csv_path) << QStringLiteral("--output")
              << QString::fromStdString(request.output_path) << QStringLiteral("--title")
              << QString::fromStdString(request.title);
    if (!request.commands_csv_path.empty()) {
        arguments << QStringLiteral("--commands") << QString::fromStdString(request.commands_csv_path);
    }

    process.setArguments(arguments);
    process.start();
    if (!process.waitForStarted()) {
        throw std::runtime_error("Failed to start plot command");
    }
    if (!process.waitForFinished(60000)) {
        process.kill();
        throw std::runtime_error("Plot command timed out");
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        throw std::runtime_error(
            "Plot command failed: " + process.readAllStandardError().toStdString());
    }
}

}  // namespace ship_sim
