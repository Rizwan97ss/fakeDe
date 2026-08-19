import { useState } from "react";
import { ChevronDown } from "lucide-react";
import type { Evidence } from "../../api/types";
import { toneForScore } from "../../lib/status";
import { ScoreBar } from "./ScoreBar";
import { StatusIcon } from "./StatusIcon";

export function EvidenceCard({ evidence }: { evidence: Evidence }) {
  const [expanded, setExpanded] = useState(false);
  const tone = toneForScore(evidence.score);
  const hasVisualization = Boolean(evidence.visualizationPngBase64);

  return (
    <div
      className="rounded-xl p-4"
      style={{ background: "var(--surface-1)", border: "1px solid var(--border-hairline)" }}
    >
      <button
        type="button"
        className="w-full flex items-start gap-3 text-left cursor-pointer"
        onClick={() => setExpanded((v) => !v)}
        aria-expanded={expanded}
      >
        <StatusIcon tone={tone} size={18} />
        <div className="flex-1 min-w-0">
          <div className="flex items-center justify-between gap-2">
            <h3 className="font-medium" style={{ color: "var(--text-primary)" }}>
              {evidence.humanLabel}
            </h3>
            <span className="text-xs tabular-nums shrink-0" style={{ color: "var(--text-muted)" }}>
              confidence {Math.round(evidence.confidence * 100)}%
            </span>
          </div>
          <div className="mt-2">
            <ScoreBar value={evidence.score} tone={tone} />
          </div>
          <p className="mt-2 text-sm" style={{ color: "var(--text-secondary)" }}>
            {evidence.explanation}
          </p>
        </div>
        {hasVisualization && (
          <ChevronDown
            size={18}
            style={{
              color: "var(--text-muted)",
              transform: expanded ? "rotate(180deg)" : "none",
              transition: "transform 200ms ease",
              flexShrink: 0,
              marginTop: 2,
            }}
          />
        )}
      </button>

      {expanded && hasVisualization && (
        <div className="mt-4 rounded-lg overflow-hidden" style={{ border: "1px solid var(--border-hairline)" }}>
          <img
            src={`data:image/png;base64,${evidence.visualizationPngBase64}`}
            alt={`${evidence.humanLabel} visualization`}
            className="w-full block"
            style={{ maxHeight: "60vh", objectFit: "contain", background: "var(--surface-0)" }}
          />
        </div>
      )}
    </div>
  );
}
