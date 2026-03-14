#pragma once

#include <string>

namespace ship_sim {

struct PlotRequest {
    std::string script_path;
    std::string input_csv_path;
    std::string commands_csv_path;
    std::string output_path;
    std::string title;
};

class PlotRunner {
public:
    static void run(const PlotRequest& request);
};

}  // namespace ship_sim
