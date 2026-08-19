#pragma once

#include "Types.h"

namespace fakede {

// Combines independent Evidence entries into one Verdict. Phase 1 deliberately uses a
// transparent, fixed weighted average instead of a calibrated model (Platt/isotonic
// scaling) — we have no labeled validation data yet to fit calibration against, and a
// fixed, inspectable formula is more honest than a black box that merely looks
// calibrated. Revisit once real usage data exists.
class FusionEngine {
public:
    Verdict fuse(std::vector<Evidence> evidence) const;

private:
    // Per-analyzer-id weight in the fused average. Unlisted ids default to 1.0.
    // Marked provisional: these are reasoned-about starting points, not fitted values.
    double weightFor(const std::string& analyzerId) const;
};

} // namespace fakede
