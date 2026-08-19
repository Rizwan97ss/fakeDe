import type { VerdictLabel } from "../api/types";

export type StatusTone = "good" | "warning" | "critical";

// Thresholds mirror engine/src/core/FusionEngine.cpp so per-analyzer evidence cards
// and the overall verdict badge read consistently.
export function toneForScore(score: number): StatusTone {
  if (score < 0.35) return "good";
  if (score > 0.65) return "critical";
  return "warning";
}

export function toneForVerdictLabel(label: VerdictLabel): StatusTone {
  switch (label) {
    case "likely_authentic":
      return "good";
    case "likely_ai_generated_or_altered":
      return "critical";
    default:
      return "warning";
  }
}

export const STATUS_COLOR: Record<StatusTone, string> = {
  good: "var(--status-good)",
  warning: "var(--status-warning)",
  critical: "var(--status-critical)",
};

export const STATUS_LABEL: Record<VerdictLabel, string> = {
  likely_authentic: "Likely Authentic",
  inconclusive: "Inconclusive",
  likely_ai_generated_or_altered: "Likely AI-Generated or Altered",
};

// Status color is never the only signal (see dataviz palette notes) - every status
// render pairs the color with one of these icons plus a text label.
export const STATUS_ICON: Record<StatusTone, string> = {
  good: "check-circle",
  warning: "alert-triangle",
  critical: "x-octagon",
};

// Plain-language reading of a single evidence card's score, so "confidence 85%" next
// to a low score bar can't be misread as "85% likely fake" - the tag says which way
// the signal actually leans, in words, before the reader has to interpret a percentage.
export const TONE_READING: Record<StatusTone, string> = {
  good: "Looks authentic",
  warning: "Not clear either way",
  critical: "Signs of AI or editing",
};
