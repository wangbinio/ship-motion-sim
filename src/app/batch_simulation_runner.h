#pragma once

#include "common/types.h"

namespace ship_sim {

class BatchSimulationRunner {
public:
    BatchRunResult run(const BatchRunOptions& options) const;
};

}  // namespace ship_sim
