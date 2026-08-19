#pragma once

#include "Types.h"

namespace fakede {

// Combines independent Evidence entries into one Verdict. Phase 1 deliberately uses a
// transparent, fixed weighted average instead of a calibrated model (Platt/isotonic
// scaling) — we have no labeled validation data yet to fit calibration against, and a
// fixed, inspectable formula is more honest than a black box that merely looks
// calibrated. Revisit once real usage data exists.
//
// The fused score blends that weighted average with the plain median of all usable
// scores (see fuse()'s implementation comment) so a single confident, favorably-
// weighted analyzer (e.g. a purpose-built ai-model:* classifier) can never fully
// override what several independent signals otherwise agree on - found and fixed
// after a real case where one confident ML signal alone pulled a 4-against-1
// classical-signal disagreement all the way to "inconclusive". Reported confidence is
// also dampened by how much the evidence disagrees with itself, so a genuinely split
// verdict is shown as low-confidence rather than a flat, disagreement-blind number.
class FusionEngine {
public:
    Verdict fuse(std::vector<Evidence> evidence) const;

private:
    // Per-analyzer-id weight in the fused average. Unlisted ids default to 1.0.
    // Marked provisional: these are reasoned-about starting points, not fitted values.
    double weightFor(const std::string& analyzerId) const;
};

} // namespace fakede
