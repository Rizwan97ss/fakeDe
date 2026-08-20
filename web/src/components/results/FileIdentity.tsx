import { useState } from "react";
import { ChevronDown, Fingerprint } from "lucide-react";

// Cryptographic hashes, not analysis results - just the file's identity, useful for
// reproducibility/chain-of-custody (confirming two reports refer to the exact same
// bytes). Collapsed by default since most users never need this.
export function FileIdentity({ sha256, blake3 }: { sha256: string; blake3: string }) {
  const [expanded, setExpanded] = useState(false);

  return (
    <div className="rounded-xl p-4" style={{ background: "var(--surface-1)", border: "1px solid var(--border-hairline)" }}>
      <button
        type="button"
        className="w-full flex items-center gap-2 text-left cursor-pointer"
        onClick={() => setExpanded((v) => !v)}
        aria-expanded={expanded}
      >
        <Fingerprint size={16} style={{ color: "var(--text-muted)" }} />
        <span className="text-sm font-medium flex-1" style={{ color: "var(--text-primary)" }}>
          File identity (technical)
        </span>
        <ChevronDown
          size={16}
          style={{ color: "var(--text-muted)", transform: expanded ? "rotate(180deg)" : "none", transition: "transform 200ms ease" }}
        />
      </button>
      {expanded && (
        <div className="mt-3 flex flex-col gap-2 text-xs font-mono" style={{ color: "var(--text-secondary)" }}>
          <p>
            <span style={{ color: "var(--text-muted)" }}>SHA-256: </span>
            <span className="break-all">{sha256}</span>
          </p>
          <p>
            <span style={{ color: "var(--text-muted)" }}>BLAKE3: </span>
            <span className="break-all">{blake3}</span>
          </p>
          <p className="font-sans mt-1" style={{ color: "var(--text-muted)" }}>
            These are cryptographic hashes of the exact bytes uploaded, not a similarity
            or perceptual fingerprint - useful for confirming two files (or reports) refer
            to the identical file.
          </p>
        </div>
      )}
    </div>
  );
}
