#include <cassert>
#include <iostream>

#include "core/FusionEngine.h"

using namespace fakede;

namespace {

Evidence makeEvidence(std::string id, double score, double confidence) {
    Evidence e;
    e.analyzerId = std::move(id);
    e.humanLabel = e.analyzerId;
    e.score = score;
    e.confidence = confidence;
    return e;
}

void testAllHighConfidenceFakeSignalsYieldFakeVerdict() {
    FusionEngine fusion;
    std::vector<Evidence> evidence{
        makeEvidence("ai-model:image-classifier", 0.95, 0.9),
        makeEvidence("ela", 0.8, 0.6),
        makeEvidence("freq-fft", 0.75, 0.5),
    };
    Verdict v = fusion.fuse(evidence);
    assert(v.overallLabel == VerdictLabel::LikelyAiGeneratedOrAltered);
    assert(v.overallScore > 0.65);
}

void testAllHighConfidenceAuthenticSignalsYieldAuthenticVerdict() {
    FusionEngine fusion;
    std::vector<Evidence> evidence{
        makeEvidence("ai-model:image-classifier", 0.05, 0.9),
        makeEvidence("metadata", 0.1, 0.6),
        makeEvidence("noise-residual", 0.15, 0.5),
    };
    Verdict v = fusion.fuse(evidence);
    assert(v.overallLabel == VerdictLabel::LikelyAuthentic);
}

void testNoUsableSignalYieldsInconclusive() {
    FusionEngine fusion;
    std::vector<Evidence> evidence{
        makeEvidence("ela", 0.9, 0.0), // zero confidence -> shouldn't drive the verdict
    };
    Verdict v = fusion.fuse(evidence);
    assert(v.overallLabel == VerdictLabel::Inconclusive);
}

void testEmptyEvidenceIsInconclusiveNotCrash() {
    FusionEngine fusion;
    Verdict v = fusion.fuse({});
    assert(v.overallLabel == VerdictLabel::Inconclusive);
}

// Regression test for a real case: an uploaded image got 4 classical signals leaning
// "fake" (metadata, ela, freq-fft, noise-residual) and 1 confident ai-model signal
// leaning "authentic". Under pure confidence-weighted averaging, that one ai-model
// signal (1.5x weight * 0.85 confidence) single-handedly pulled the fused score down
// to inconclusive/authentic-leaning territory despite being outnumbered 4 to 1. The
// median-blended fusion should no longer let one signal fully override that majority.
void testMinorityConfidentModelDoesNotFullyOverrideMajorityConsensus() {
    FusionEngine fusion;
    std::vector<Evidence> evidence{
        makeEvidence("metadata", 0.58, 0.3),
        makeEvidence("ela", 0.59, 0.39),
        makeEvidence("freq-fft", 0.98, 0.5),
        makeEvidence("noise-residual", 0.56, 0.45),
        makeEvidence("ai-model:image-classifier", 0.0, 0.85),
    };
    Verdict v = fusion.fuse(evidence);
    assert(v.overallScore > 0.45);
    // The signals genuinely disagree, so confidence should reflect that honestly
    // rather than reporting a flat, disagreement-blind number.
    assert(v.overallConfidence < 0.4);
}

} // namespace

int main() {
    testAllHighConfidenceFakeSignalsYieldFakeVerdict();
    testAllHighConfidenceAuthenticSignalsYieldAuthenticVerdict();
    testNoUsableSignalYieldsInconclusive();
    testEmptyEvidenceIsInconclusiveNotCrash();
    testMinorityConfidentModelDoesNotFullyOverrideMajorityConsensus();
    std::cout << "fusion_engine_test: all tests passed\n";
    return 0;
}
