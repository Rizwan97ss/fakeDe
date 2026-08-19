import { Loader2 } from "lucide-react";
import { Dropzone } from "../components/upload/Dropzone";

interface HomePageProps {
  onFileSelected: (file: File) => void;
  isAnalyzing: boolean;
  error: string | null;
}

export function HomePage({ onFileSelected, isAnalyzing, error }: HomePageProps) {
  return (
    <div className="flex flex-col gap-4">
      <Dropzone onFileSelected={onFileSelected} disabled={isAnalyzing} />

      {isAnalyzing && (
        <div className="flex items-center justify-center gap-2 text-sm" style={{ color: "var(--text-secondary)" }}>
          <Loader2 size={16} className="animate-spin" />
          Running metadata, error-level, frequency, noise-residual, and AI-classifier analysis…
        </div>
      )}

      {error && (
        <div
          className="rounded-xl p-4 text-sm"
          style={{ background: "var(--surface-1)", border: "1px solid var(--status-critical)", color: "var(--text-primary)" }}
        >
          {error}
        </div>
      )}
    </div>
  );
}
