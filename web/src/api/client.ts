import type { AnalysisResponse, AnalysisSummary, ApiErrorResponse } from "./types";

export class ApiError extends Error {
  detectedMimeType?: string;
  constructor(message: string, detectedMimeType?: string) {
    super(message);
    this.name = "ApiError";
    this.detectedMimeType = detectedMimeType;
  }
}

// In dev, Vite proxies /api -> the engine (see vite.config.ts). In production, the
// engine should be served behind the same origin/reverse proxy as the frontend.
const API_BASE = "/api/v1";

// Only set when the engine is deployed with FAKEDE_API_KEY - see docs/ARCHITECTURE.md
// for why this is a single shared-secret gate, not real per-user auth. Baked in at
// build time via VITE_FAKEDE_API_KEY; harmless to omit when the engine has no key set.
const API_KEY = import.meta.env.VITE_FAKEDE_API_KEY as string | undefined;

function authHeaders(): HeadersInit {
  return API_KEY ? { "X-API-Key": API_KEY } : {};
}

export async function analyzeFile(file: File): Promise<AnalysisResponse> {
  const formData = new FormData();
  formData.append("file", file);

  const res = await fetch(`${API_BASE}/analyze`, {
    method: "POST",
    headers: authHeaders(),
    body: formData,
  });

  if (!res.ok) {
    const body = (await res.json().catch(() => null)) as ApiErrorResponse | null;
    throw new ApiError(body?.error ?? `Analysis failed (${res.status})`, body?.detectedMimeType);
  }

  return (await res.json()) as AnalysisResponse;
}

export async function getResult(id: string): Promise<AnalysisResponse> {
  const res = await fetch(`${API_BASE}/analyze/${encodeURIComponent(id)}`, { headers: authHeaders() });
  if (!res.ok) {
    throw new ApiError(`Result not found (${res.status})`);
  }
  return (await res.json()) as AnalysisResponse;
}

export async function listRecentAnalyses(limit = 20): Promise<AnalysisSummary[]> {
  const res = await fetch(`${API_BASE}/analyses?limit=${limit}`, { headers: authHeaders() });
  if (!res.ok) {
    throw new ApiError(`Failed to load history (${res.status})`);
  }
  const body = (await res.json()) as { results: AnalysisSummary[] };
  return body.results;
}
