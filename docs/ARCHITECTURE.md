# Architecture

## Why C++ backend + React frontend, and how they're wired together

The engine (`engine/`) is a standalone C++20 HTTP service (Drogon) that does all file
analysis and ML inference natively — no Python at runtime. The frontend (`web/`) is a
Vite/React/TypeScript SPA that talks to it over plain REST/JSON. They're independent
processes; in development, Vite's dev server proxies `/api/*` to the engine.

## Why ONNX Runtime is NOT installed via vcpkg

Microsoft's own vcpkg port docs flag `onnxruntime` as still-maturing: it builds from
source and is a common source of protobuf-version collisions with other vcpkg ports
(OpenCV in particular). Instead, `scripts/fetch_onnxruntime.ps1` downloads Microsoft's
official prebuilt Windows x64 distribution directly from the `microsoft/onnxruntime`
GitHub releases, unpacked into `engine/onnxruntime/` (gitignored) and linked manually
in `CMakeLists.txt` as an imported target.

## Why WAV decoding is a vendored single header, not a vcpkg dependency

`engine/third_party/dr_wav.h` (public domain/MIT-0) decodes WAV directly with zero
external build dependency. This project previously dropped a vcpkg `libmagic`
dependency for the same reason: autotools-style ports that compile a native helper
tool from source and then execute it mid-build turned out to be a real, repeated
source of environment friction on Windows. For a single well-scoped format, a vendored
header sidesteps that whole class of risk entirely. Revisit with a real vcpkg audio
library (e.g. libsndfile) if/when broader format support (MP3, FLAC, etc.) is needed.

## Why video decoding uses OpenCV + Windows Media Foundation, not FFmpeg

`VideoFrameSampler` (`engine/src/util/VideoFrameSampler.cpp`) opens files via
`cv::VideoCapture(path, cv::CAP_MSMF)` — OpenCV's binding to Windows' built-in Media
Foundation decoder, enabled by adding the `msmf` feature to the `opencv4` vcpkg port
already in the dependency tree, rather than pulling in a separate FFmpeg/libav vcpkg
port. FFmpeg's vcpkg port is large, slow to build, and (consistent with this project's
`libmagic` experience) another autotools-adjacent surface for Windows build friction;
MSMF is a real, already-present OS component with no extra dependency at all. The
tradeoff: container/codec support is whatever MSMF has installed, not whatever FFmpeg
can be built with. Revisit if a real usage pattern hits a codec MSMF can't decode.

## Every other C++ dependency comes from vcpkg (manifest mode)

`engine/vcpkg.json` pins dependencies; `engine/vcpkg/` is a local vcpkg checkout
bootstrapped by cloning `microsoft/vcpkg` (see `scripts/bootstrap-vcpkg.ps1` — not
committed, not a git submodule, just a plain gitignored local tool install so any
contributor can reproduce it with one command). `CMakeLists.txt` auto-detects this
local vcpkg and sets `CMAKE_TOOLCHAIN_FILE` unless one is already passed in.

## The plugin architecture: `IAnalyzer` + `FusionEngine`

Every detection technique — a classical forensic heuristic or an ML model — implements
`IAnalyzer` (`engine/src/core/IAnalyzer.h`) and is registered in
`AnalyzerRegistry::buildDefaultRegistry()`. `AnalyzerRegistry` routes an uploaded file
to every analyzer that both supports its (sniffed, not client-claimed) MIME type and
reports `isAvailable() == true`. Each analyzer returns an independent `Evidence`
(score + confidence + human-readable explanation + optional visualization).
`FusionEngine` combines all `Evidence` for one request into a single `Verdict`.

This is the extension point for the whole roadmap: adding audio, video, text, or
document support (see `docs/ROADMAP.md`) means writing new `IAnalyzer` implementations
and registering them — `AnalyzerRegistry`, `FusionEngine`, the HTTP layer, and the
frontend's evidence-breakdown UI don't need to change shape.

## Why verdicts are never a bare true/false

AI/fake-content detection is an unsolved, adversarial problem — see
`docs/model-sourcing.md` for per-model reliability notes. `Verdict` always carries the
full `evidenceBreakdown` alongside the fused `overallLabel`/`overallScore`, and the
frontend renders that breakdown, not just a badge. `FusionEngine` currently uses a
transparent, fixed weighted average (see the comment in `FusionEngine.h`) rather than a
calibrated model, because there's no labeled validation data yet to calibrate against —
upgrading this is real future work, not a placeholder to hide.

## Data flow (Phase 1: images)

1. `POST /api/v1/analyze` (multipart) hits `AnalysisController::handleAnalyze`.
2. `FileTypeSniffer` (magic-byte signature check) determines the real MIME type from file bytes.
3. `AnalyzerRegistry::analyzersFor(mimeType)` returns the applicable, available analyzers.
4. Each analyzer runs synchronously (image analysis is sub-second-to-seconds; no job
   queue yet — see `docs/ROADMAP.md` for when that becomes necessary).
5. `FusionEngine::fuse()` produces the `Verdict`.
6. `JobStore` (SQLite) persists it, keyed by a generated id.
7. The full `Verdict` JSON (plus `id`, `fileName`, `detectedMimeType`) is returned
   synchronously; `GET /api/v1/analyze/{id}` re-fetches it later.

This same flow handles every later file type unchanged — text, PDFs, WAV audio, and
MP4 video all go through the identical sniff → route → analyze → fuse → persist
pipeline; only the registered `IAnalyzer` set differs. Video's per-frame analyzers
(`VideoTemporalConsistencyAnalyzer`, `VideoAiFrameAnalyzer`) hold raw pointers into the
already-constructed image analyzers so they can reuse `ElaAnalyzer`/`FrequencyAnalyzer`/
`NoiseResidualAnalyzer`/`AiGeneratedModelAnalyzer` per sampled frame without duplicating
any analysis logic — see the pointer-capture in `AnalyzerRegistry::buildDefaultRegistry()`.
