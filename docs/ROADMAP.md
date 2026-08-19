# Roadmap

Phases 1-2 are built and verified working. Phases 3-4 below are architected for (every
new analyzer just implements `IAnalyzer` and gets registered — see
`docs/ARCHITECTURE.md`) but not yet built.

## Phase 1 — Images (done)

- Analyzers: metadata (exiv2), Error Level Analysis, frequency/FFT spectral analysis,
  sensor-noise-residual, and an ONNX AI-image classifier (UniversalFakeDetect).
- Synchronous REST API (`POST /api/v1/analyze`) — no job queue needed yet since image
  analysis is sub-second-to-seconds.
- SQLite result store, keyed by id.

## Phase 2 — Text & documents (done)

- **Text**: two analyzers, both live. `TextStylometryAnalyzer` (sentence-length
  burstiness, vocabulary richness, repetition — no model needed) and
  `TextPerplexityAnalyzer` (GPT-2 small via ONNX + a from-scratch C++ BPE tokenizer,
  classic single-model perplexity+burstiness scoring). **Known scope gap, not yet
  closed**: this is not the stronger ratio-of-two-models method used by
  Binoculars/Fast-DetectGPT — that remains real future work, tracked in
  `docs/model-sourcing.md`. Text stays the least reliable detection domain in this
  product either way; both analyzers cap their own confidence low regardless of input.
- **Documents/PDF**: `PdfForensicsAnalyzer` does incremental-update/xref-history
  forensics via direct byte-scanning (`%%EOF`/`/Prev` counting) rather than a full
  parser or Poppler — deliberately scoped down from the original plan to avoid a new
  vcpkg dependency; it under-counts on PDF 1.5+ files using compressed
  cross-reference streams (documented in the analyzer's own header) rather than
  over-claiming. Does not yet extract embedded text/images to reuse the text/image
  pipelines — that's still open if it turns out to matter in practice.

## Phase 3 — Audio

- RawNet2 anti-spoofing (Tak et al., arXiv:2011.01108) first — simpler architecture
  (conv+GRU, no graph attention), more likely to export cleanly to ONNX.
- [`clovaai/aasist`](https://github.com/clovaai/aasist) as a follow-up ensemble
  addition once its graph-attention layers' ONNX export is proven out (flagged as a
  real conversion risk, not assumed-solved).
- This is where async job handling first becomes necessary for longer audio files —
  see "Async & WebSocket" below.

## Phase 4 — Video

- Frame extraction via FFmpeg/libav.
- ONNX face detector (RetinaFace/SCRFD from the insightface project — not dlib) +
  per-frame reuse of the Phase 1 image pipeline.
- Temporal consistency checks (landmark/head-pose flicker) and audio-visual sync
  checks when an audio track is present.
- Models from [`SCLBD/DeepfakeBench`](https://github.com/SCLBD/DeepfakeBench)'s
  unified pretrained release; Self-Blended Images (SBI) for generalization to unseen
  manipulation methods.
- **Async & WebSocket**: video jobs are the reason the synchronous Phase-1 API isn't
  enough — this phase adds a job queue and a WebSocket progress channel (Drogon has
  native WebSocket support) so the frontend can show per-frame progress instead of
  blocking on one long HTTP request.

## Phase 5 — Product hardening

- Auth + a real analysis-history dashboard (Phase 1's SQLite store only supports
  fetch-by-id).
- Fusion calibration: replace `FusionEngine`'s fixed weighted average with a properly
  calibrated model (Platt/isotonic scaling) once real usage produces labeled
  validation data.
- Production CORS/auth hardening (Phase 1's `main.cpp` allows any origin — fine for
  local development, not for a public deployment).
- Revisit `docs/model-sourcing.md` licensing for every integrated model before any
  commercial launch.
