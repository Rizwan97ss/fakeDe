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

} // namespace

int main() {
    testAllHighConfidenceFakeSignalsYieldFakeVerdict();
    testAllHighConfidenceAuthenticSignalsYieldAuthenticVerdict();
    testNoUsableSignalYieldsInconclusive();
    testEmptyEvidenceIsInconclusiveNotCrash();
    std::cout << "fusion_engine_test: all tests passed\n";
    return 0;
}
