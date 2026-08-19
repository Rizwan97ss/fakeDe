import { useState } from "react";
import { ShieldCheck, History, Upload } from "lucide-react";
import { analyzeFile, getResult, ApiError } from "./api/client";
import type { AnalysisResponse } from "./api/types";
import { HomePage } from "./pages/HomePage";
import { ResultsPage } from "./pages/ResultsPage";
import { HistoryPage } from "./pages/HistoryPage";

type View = "home" | "results" | "history";

export default function App() {
  const [view, setView] = useState<View>("home");
  const [isAnalyzing, setIsAnalyzing] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [result, setResult] = useState<AnalysisResponse | null>(null);

  async function handleFileSelected(file: File) {
    setIsAnalyzing(true);
    setError(null);
    try {
      const response = await analyzeFile(file);
      setResult(response);
      setView("results");
    } catch (err) {
      setError(err instanceof ApiError ? err.message : "Something went wrong analyzing this file.");
    } finally {
      setIsAnalyzing(false);
    }
  }

  async function handleHistorySelect(id: string) {
    setError(null);
    try {
      const response = await getResult(id);
      setResult(response);
      setView("results");
    } catch (err) {
      setError(err instanceof ApiError ? err.message : "Failed to load that result.");
    }
  }

  function resetToHome() {
    setResult(null);
    setError(null);
    setView("home");
  }

  return (
    <div className="min-h-full flex flex-col">
      <header className="border-b" style={{ borderColor: "var(--border-hairline)" }}>
        <div className="max-w-3xl mx-auto px-6 py-5 flex items-center justify-between gap-3">
          <div className="flex items-center gap-3">
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
          <nav className="flex items-center gap-1">
            <button
              type="button"
              onClick={resetToHome}
              className="flex items-center gap-1.5 rounded-lg px-3 py-1.5 text-xs cursor-pointer transition-colors"
              style={{
                background: view !== "history" ? "var(--surface-2)" : "transparent",
                color: view !== "history" ? "var(--text-primary)" : "var(--text-muted)",
              }}
            >
              <Upload size={13} />
              Analyze
            </button>
            <button
              type="button"
              onClick={() => setView("history")}
              className="flex items-center gap-1.5 rounded-lg px-3 py-1.5 text-xs cursor-pointer transition-colors"
              style={{
                background: view === "history" ? "var(--surface-2)" : "transparent",
                color: view === "history" ? "var(--text-primary)" : "var(--text-muted)",
              }}
            >
              <History size={13} />
              History
            </button>
          </nav>
        </div>
      </header>

      <main className="flex-1 max-w-3xl w-full mx-auto px-6 py-10">
        {view === "history" && <HistoryPage onSelect={handleHistorySelect} />}
        {view === "results" && result && <ResultsPage result={result} onReset={resetToHome} />}
        {view === "home" && <HomePage onFileSelected={handleFileSelected} isAnalyzing={isAnalyzing} error={error} />}
      </main>

      <footer className="px-6 py-6 text-center text-xs" style={{ color: "var(--text-muted)" }}>
        Every verdict is a fused set of inspectable evidence, not a black-box yes/no.
      </footer>
    </div>
  );
}
