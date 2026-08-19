import type { StatusTone } from "../../lib/status";
import { STATUS_COLOR } from "../../lib/status";

export function ScoreBar({ value, tone }: { value: number; tone: StatusTone }) {
  const pct = Math.round(Math.max(0, Math.min(1, value)) * 100);
  return (
    <div
      className="h-2 w-full rounded-full overflow-hidden"
      style={{ background: "var(--gridline)" }}
      role="progressbar"
      aria-valuenow={pct}
      aria-valuemin={0}
      aria-valuemax={100}
    >
      <div
        className="h-full rounded-full"
        style={{ width: `${pct}%`, background: STATUS_COLOR[tone], transition: "width 400ms ease" }}
      />
    </div>
  );
}
