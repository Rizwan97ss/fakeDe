import type { StatusTone } from "../../lib/status";
import { STATUS_COLOR } from "../../lib/status";

interface ConfidenceGaugeProps {
  score: number; // 0..1, drives the fill sweep
  tone: StatusTone;
  size?: number;
}

// A single-hue radial meter: muted track + status-colored fill arc, matching the
// dataviz skill's "meter / progress track" component (same-ramp track, status color
// never used alone - the numeric label and the verdict text elsewhere always pair
// with it).
export function ConfidenceGauge({ score, tone, size = 128 }: ConfidenceGaugeProps) {
  const stroke = 10;
  const radius = (size - stroke) / 2;
  const circumference = 2 * Math.PI * radius;
  const clamped = Math.max(0, Math.min(1, score));
  const dashOffset = circumference * (1 - clamped);

  return (
    <div className="relative" style={{ width: size, height: size }}>
      <svg width={size} height={size} className="-rotate-90">
        <circle
          cx={size / 2}
          cy={size / 2}
          r={radius}
          fill="none"
          stroke="var(--gridline)"
          strokeWidth={stroke}
        />
        <circle
          cx={size / 2}
          cy={size / 2}
          r={radius}
          fill="none"
          stroke={STATUS_COLOR[tone]}
          strokeWidth={stroke}
          strokeLinecap="round"
          strokeDasharray={circumference}
          strokeDashoffset={dashOffset}
          style={{ transition: "stroke-dashoffset 500ms ease" }}
        />
      </svg>
      <div className="absolute inset-0 flex flex-col items-center justify-center">
        <span className="text-2xl font-semibold tabular-nums" style={{ color: "var(--text-primary)" }}>
          {Math.round(clamped * 100)}%
        </span>
        <span className="text-xs" style={{ color: "var(--text-muted)" }}>
          fake score
        </span>
      </div>
    </div>
  );
}
