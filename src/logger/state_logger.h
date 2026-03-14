#pragma once

#include <iosfwd>

#include "common/types.h"

namespace ship_sim {

class StateLogger {
public:
    explicit StateLogger(std::ostream& output_stream);

    void writeHeader() const;
    void writeState(const ShipState& state) const;

private:
    std::ostream& output_stream_;
};

}  // namespace ship_sim
