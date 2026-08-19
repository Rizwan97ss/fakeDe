import { useState } from "react";
import { ShieldCheck } from "lucide-react";
import { analyzeFile, ApiError } from "./api/client";
import type { AnalysisResponse } from "./api/types";
import { HomePage } from "./pages/HomePage";
import { ResultsPage } from "./pages/ResultsPage";

export default function App() {
  const [isAnalyzing, setIsAnalyzing] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [result, setResult] = useState<AnalysisResponse | null>(null);

  async function handleFileSelected(file: File) {
    setIsAnalyzing(true);
    setError(null);
    try {
      const response = await analyzeFile(file);
      setResult(response);
    } catch (err) {
      setError(err instanceof ApiError ? err.message : "Something went wrong analyzing this file.");
    } finally {
      setIsAnalyzing(false);
    }
  }

  return (
    <div className="min-h-full flex flex-col">
      <header className="border-b" style={{ borderColor: "var(--border-hairline)" }}>
        <div className="max-w-3xl mx-auto px-6 py-5 flex items-center gap-3">
          <ShieldCheck size={26} style={{ color: "var(--series-blue)" }} />
          <div>
            <h1 className="text-lg font-semibold leading-tight" style={{ color: "var(--text-primary)" }}>
              fakeDe
            </h1>
            <p className="text-xs" style={{ color: "var(--text-muted)" }}>
              Evidence-based AI/altered-file detection
            </p>
          </div>
        </div>
      </header>

      <main className="flex-1 max-w-3xl w-full mx-auto px-6 py-10">
        {result ? (
          <ResultsPage result={result} onReset={() => setResult(null)} />
        ) : (
          <HomePage onFileSelected={handleFileSelected} isAnalyzing={isAnalyzing} error={error} />
        )}
      </main>

      <footer className="px-6 py-6 text-center text-xs" style={{ color: "var(--text-muted)" }}>
        Every verdict is a fused set of inspectable evidence, not a black-box yes/no.
      </footer>
    </div>
  );
}
