import type { Evidence } from "../../api/types";
import { EvidenceCard } from "./EvidenceCard";

export function EvidenceBreakdownList({ evidence }: { evidence: Evidence[] }) {
  if (evidence.length === 0) {
    return (
      <p className="text-sm" style={{ color: "var(--text-muted)" }}>
        No analyzers were available for this file type.
      </p>
    );
  }

  return (
    <div className="flex flex-col gap-3">
      <h3 className="text-sm font-medium uppercase tracking-wide" style={{ color: "var(--text-muted)" }}>
        Evidence breakdown
      </h3>
      {evidence.map((e) => (
        <EvidenceCard key={e.analyzerId} evidence={e} />
      ))}
    </div>
  );
}
