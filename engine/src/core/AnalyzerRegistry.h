#pragma once

#include <memory>
#include <vector>

#include "IAnalyzer.h"

namespace fakede {

// Owns every registered analyzer and hands back the subset applicable to a given
// MIME type. One registry instance lives for the lifetime of the process.
class AnalyzerRegistry {
public:
    void registerAnalyzer(std::unique_ptr<IAnalyzer> analyzer);

    // Only analyzers that both support the MIME type and report isAvailable() == true.
    std::vector<const IAnalyzer*> analyzersFor(const std::string& mimeType) const;

    const std::vector<std::unique_ptr<IAnalyzer>>& all() const { return analyzers_; }

private:
    std::vector<std::unique_ptr<IAnalyzer>> analyzers_;
};

// Builds a registry with every analyzer this build supports, wired up with their
// model paths. Phase 1: image analyzers only.
AnalyzerRegistry buildDefaultRegistry(const std::string& modelsDir);

} // namespace fakede
