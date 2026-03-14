#pragma once

#include <string>
#include <vector>

#include "common/types.h"

namespace ship_sim {

class CommandSchedule {
public:
    explicit CommandSchedule(CommandEvents events);

    static CommandSchedule loadFromCsvFile(const std::string& path);

    CommandEvents popReadyEvents(double sim_time_s);
    bool empty() const;

private:
    CommandEvents events_;
    std::size_t next_index_ {0};
};

}  // namespace ship_sim
