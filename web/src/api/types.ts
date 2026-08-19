// Mirrors engine/src/core/Types.h - keep in sync with Evidence::toJson() / Verdict::toJson().

export type VerdictLabel = "likely_authentic" | "inconclusive" | "likely_ai_generated_or_altered";

export interface Evidence {
  analyzerId: string;
  humanLabel: string;
  score: number; // 0.0 authentic .. 1.0 fake/altered
  confidence: number; // 0.0 .. 1.0
  explanation: string;
  visualizationPngBase64: string | null;
  rawDetails: Record<string, unknown>;
}

export interface Verdict {
  overallLabel: VerdictLabel;
  overallScore: number;
  overallConfidence: number;
  evidenceBreakdown: Evidence[];
}

export interface AnalysisResponse extends Verdict {
  id: string;
  fileName: string;
  detectedMimeType: string;
}

export interface ApiErrorResponse {
  error: string;
  detectedMimeType?: string;
}
