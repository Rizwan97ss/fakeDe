import { RotateCcw } from "lucide-react";
import type { AnalysisResponse } from "../api/types";
import { VerdictSummary } from "../components/results/VerdictSummary";
import { EvidenceBreakdownList } from "../components/results/EvidenceBreakdownList";

export function ResultsPage({ result, onReset }: { result: AnalysisResponse; onReset: () => void }) {
  return (
    <div className="flex flex-col gap-6">
      <VerdictSummary verdict={result} fileName={result.fileName} />
      <EvidenceBreakdownList evidence={result.evidenceBreakdown} />

      <button
        type="button"
        onClick={onReset}
        className="self-start flex items-center gap-2 text-sm rounded-lg px-4 py-2 cursor-pointer transition-colors"
        style={{ background: "var(--surface-1)", color: "var(--text-secondary)", border: "1px solid var(--border-hairline)" }}
      >
        <RotateCcw size={14} />
        Analyze another file
      </button>
    </div>
  );
}
