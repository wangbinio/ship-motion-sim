#include "scenario/command_schedule.h"

#include <algorithm>

namespace ship_sim {

CommandSchedule::CommandSchedule(CommandEvents events) : events_(std::move(events)) {
    std::stable_sort(
        events_.begin(),
        events_.end(),
        [](const CommandEvent& lhs, const CommandEvent& rhs) { return lhs.time_s < rhs.time_s; });
}

CommandEvents CommandSchedule::popReadyEvents(double sim_time_s) {
    constexpr double kTimeTolerance = 1e-9;

    CommandEvents ready;
    while (next_index_ < events_.size() && events_[next_index_].time_s <= sim_time_s + kTimeTolerance) {
        ready.push_back(events_[next_index_]);
        ++next_index_;
    }
    return ready;
}

bool CommandSchedule::empty() const {
    return next_index_ >= events_.size();
}

}  // namespace ship_sim
