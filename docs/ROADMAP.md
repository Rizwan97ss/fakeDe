# Roadmap

Phases 1-4 are built and verified working. Phase 5 below is architected for (every
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

## Phase 3 — Audio (done)

- **Classical, no model needed**: `VoiceNaturalnessAnalyzer` (frame-level jitter/shimmer
  approximation via autocorrelation-based pitch tracking) and
  `SpliceDetectionAnalyzer` (noise-floor consistency across quiet segments). Both
  verified against synthetic test tones with correct, sensible directional behavior.
- **ML**: `AntiSpoofingAnalyzer` runs RawNet2 (Tak et al., arXiv:2011.01108) directly
  on the raw waveform. Its graph-attention cousin, AASIST, was *not* attempted - see
  `docs/model-sourcing.md` for why RawNet2's simpler architecture made it tractable
  and what AASIST would need. Verified loading, running, and producing sensible
  (non-degenerate) output; **not yet validated for real bonafide/spoof discrimination
  accuracy** against actual speech samples (test inputs were synthetic tones) - a real
  open item, not a completed accuracy validation.
- Format support: WAV only, decoded via a vendored single-header library
  (`engine/third_party/dr_wav.h`, public domain) rather than a new vcpkg dependency -
  see `docs/ARCHITECTURE.md` for why that tradeoff was made repeatedly in this project.
- Still synchronous (no job queue/WebSocket yet) - audio clips tested so far are short
  enough that this wasn't yet a forcing function. Revisit if real usage needs it before
  Phase 4 gets there anyway.

## Phase 4 — Video (done)

- **Decoding**: `VideoFrameSampler` uses OpenCV's `cv::VideoCapture` with the
  Windows Media Foundation backend (`cv::CAP_MSMF`) rather than FFmpeg/libav — see
  `docs/ARCHITECTURE.md` for why. Container support is therefore whatever MSMF's
  installed codecs handle; verified against H.264-in-MP4 test files. `FileTypeSniffer`
  detects MP4 via the `ftyp` box at byte offset 4.
- **`VideoTemporalConsistencyAnalyzer`**: samples up to 8 frames evenly across the
  clip, re-runs the existing Phase 1 `ElaAnalyzer`/`FrequencyAnalyzer`/
  `NoiseResidualAnalyzer` on each sampled frame, and scores the *frame-to-frame
  standard deviation* of those three signals — continuous camera footage should stay
  fairly stable across frames; spliced/regenerated content tends to wobble. Verified
  directionally correct against two synthetic test clips (uniform vs. alternating
  per-frame noise characteristics): the inconsistent clip scored 1.00 vs. 0.30 for the
  consistent one.
- **`VideoAiFrameAnalyzer`**: samples up to 6 frames and reuses the Phase 1
  `AiGeneratedModelAnalyzer` (UniversalFakeDetect) per frame, reporting the mean
  score. No dedicated video-deepfake model (e.g. Self-Blended Images or
  `SCLBD/DeepfakeBench`) was integrated — this reuses the image classifier as-is,
  which is a real accuracy gap for video-specific manipulation styles it wasn't
  trained on.
- Still synchronous (`POST /api/v1/analyze`, no job queue/WebSocket) — deliberately
  deferred, not forgotten. Video is the slowest pipeline so far (frame decode +
  multiple per-frame analyzer passes), and Drogon's default 1MB max body size was
  raised to 200MB to even accept typical clips, but there's no per-frame progress
  streaming yet.
- **Known scope gaps, not yet closed**: no face detection/landmark tracking, no
  head-pose flicker analysis, and no audio-visual sync check (the audio track, if
  present, isn't analyzed at all — video and audio pipelines don't talk to each
  other yet). These were explicitly scoped out of this pass rather than attempted
  and left half-working.

## Phase 5 — Product hardening (not started)

- **Async & WebSocket**: video (and, to a lesser extent, audio) jobs are the reason
  the synchronous Phase-1 API isn't enough long-term — this would add a job queue and
  a WebSocket progress channel (Drogon has native WebSocket support) so the frontend
  can show per-frame/per-analyzer progress instead of blocking on one long HTTP
  request. Deferred through Phase 4 because test clips were short enough not to force
  the issue yet.
- Auth + a real analysis-history dashboard (Phase 1's SQLite store only supports
  fetch-by-id).
- Fusion calibration: replace `FusionEngine`'s fixed weighted average with a properly
  calibrated model (Platt/isotonic scaling) once real usage produces labeled
  validation data.
- Production CORS/auth hardening (Phase 1's `main.cpp` allows any origin — fine for
  local development, not for a public deployment).
- Revisit `docs/model-sourcing.md` licensing for every integrated model before any
  commercial launch.
