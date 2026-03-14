#include "logger/state_logger.h"

#include <iomanip>
#include <ostream>

namespace ship_sim {

StateLogger::StateLogger(std::ostream& output_stream) : output_stream_(output_stream) {}

void StateLogger::writeHeader() const {
    output_stream_ << "time_s,lat_deg,lon_deg,heading_deg,speed_mps,yaw_rate_deg_s\n";
}

void StateLogger::writeState(const ShipState& state) const {
    output_stream_ << std::fixed << std::setprecision(6)
                   << state.time_s << ','
                   << state.lat_deg << ','
                   << state.lon_deg << ','
                   << state.heading_deg << ','
                   << state.speed_mps << ','
                   << state.yaw_rate_deg_s << '\n';
}

}  // namespace ship_sim
