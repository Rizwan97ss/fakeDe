import type { AnalysisResponse, ApiErrorResponse } from "./types";

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

export async function analyzeFile(file: File): Promise<AnalysisResponse> {
  const formData = new FormData();
  formData.append("file", file);

  const res = await fetch(`${API_BASE}/analyze`, {
    method: "POST",
    body: formData,
  });

  if (!res.ok) {
    const body = (await res.json().catch(() => null)) as ApiErrorResponse | null;
    throw new ApiError(body?.error ?? `Analysis failed (${res.status})`, body?.detectedMimeType);
  }

  return (await res.json()) as AnalysisResponse;
}

export async function getResult(id: string): Promise<AnalysisResponse> {
  const res = await fetch(`${API_BASE}/analyze/${encodeURIComponent(id)}`);
  if (!res.ok) {
    throw new ApiError(`Result not found (${res.status})`);
  }
  return (await res.json()) as AnalysisResponse;
}
