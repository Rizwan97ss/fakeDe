import type { Verdict } from "../../api/types";
import { STATUS_LABEL, toneForVerdictLabel } from "../../lib/status";
import { ConfidenceGauge } from "./ConfidenceGauge";
import { StatusIcon } from "./StatusIcon";

export function VerdictSummary({ verdict, fileName }: { verdict: Verdict; fileName: string }) {
  const tone = toneForVerdictLabel(verdict.overallLabel);

  return (
    <div
      className="flex flex-col sm:flex-row items-center sm:items-stretch gap-6 rounded-2xl p-6 sm:p-8"
      style={{ background: "var(--surface-1)", border: "1px solid var(--border-hairline)" }}
    >
      <ConfidenceGauge score={verdict.overallScore} tone={tone} />

      <div className="flex flex-col justify-center gap-2 min-w-0">
        <span className="text-sm truncate" style={{ color: "var(--text-muted)" }}>
          {fileName}
        </span>
        <div className="flex items-center gap-2">
          <StatusIcon tone={tone} size={24} />
          <h2 className="text-xl sm:text-2xl font-semibold" style={{ color: "var(--text-primary)" }}>
            {STATUS_LABEL[verdict.overallLabel]}
          </h2>
        </div>
        <p className="text-sm max-w-prose" style={{ color: "var(--text-secondary)" }}>
          Overall confidence in this verdict: <strong>{Math.round(verdict.overallConfidence * 100)}%</strong>.
          This is a fused estimate from {verdict.evidenceBreakdown.length} independent signals below — read
          the breakdown, not just this badge. AI/fake-content detection is an unsolved, adversarial problem;
          treat this as strong advisory evidence, not a certainty.
        </p>
      </div>
    </div>
  );
}
