import { useEffect, useState } from "react";
import { Loader2, FileClock } from "lucide-react";
import { listRecentAnalyses, ApiError } from "../api/client";
import type { AnalysisSummary } from "../api/types";
import { StatusIcon } from "../components/results/StatusIcon";
import { STATUS_LABEL, toneForVerdictLabel } from "../lib/status";

function formatTimestamp(sqliteUtc: string): string {
  // JobStore stores SQLite's datetime('now') - "YYYY-MM-DD HH:MM:SS" UTC, no
  // timezone suffix - append one so the Date parser treats it as UTC, not local.
  const date = new Date(sqliteUtc.replace(" ", "T") + "Z");
  if (Number.isNaN(date.getTime())) return sqliteUtc;
  return date.toLocaleString();
}

export function HistoryPage({ onSelect }: { onSelect: (id: string) => void }) {
  const [items, setItems] = useState<AnalysisSummary[] | null>(null);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    let cancelled = false;
    listRecentAnalyses(20)
      .then((results) => {
        if (!cancelled) setItems(results);
      })
      .catch((err) => {
        if (!cancelled) setError(err instanceof ApiError ? err.message : "Failed to load history.");
      });
    return () => {
      cancelled = true;
    };
  }, []);

  if (error) {
    return (
      <div
        className="rounded-xl p-4 text-sm"
        style={{ background: "var(--surface-1)", border: "1px solid var(--status-critical)", color: "var(--text-primary)" }}
      >
        {error}
      </div>
    );
  }

  if (!items) {
    return (
      <div className="flex items-center justify-center gap-2 text-sm py-16" style={{ color: "var(--text-secondary)" }}>
        <Loader2 size={16} className="animate-spin" />
        Loading history…
      </div>
    );
  }

  if (items.length === 0) {
    return (
      <div className="flex flex-col items-center justify-center gap-3 py-16 text-center" style={{ color: "var(--text-muted)" }}>
        <FileClock size={32} strokeWidth={1.5} />
        <p className="text-sm">No analyses yet. Upload a file to get started.</p>
      </div>
    );
  }

  return (
    <div className="flex flex-col gap-2">
      {items.map((item) => {
        const tone = item.overallLabel ? toneForVerdictLabel(item.overallLabel) : "warning";
        return (
          <button
            key={item.id}
            type="button"
            onClick={() => onSelect(item.id)}
            className="flex items-center gap-3 rounded-xl px-4 py-3 text-left cursor-pointer transition-colors"
            style={{ background: "var(--surface-1)", border: "1px solid var(--border-hairline)" }}
          >
            <StatusIcon tone={tone} size={18} />
            <div className="flex-1 min-w-0">
              <p className="text-sm font-medium truncate" style={{ color: "var(--text-primary)" }}>
                {item.fileName}
              </p>
              <p className="text-xs" style={{ color: "var(--text-muted)" }}>
                {item.overallLabel ? STATUS_LABEL[item.overallLabel] : "Unknown"} · {Math.round(item.overallScore * 100)}%
                fake score · {formatTimestamp(item.createdAt)}
              </p>
            </div>
          </button>
        );
      })}
    </div>
  );
}
