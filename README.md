# fakeDe

AI/Fake/Altered File Detector — a C++ analysis engine plus a React frontend that
inspects an uploaded file and returns a fused, evidence-based verdict (never a bare
yes/no) on whether it's authentic, AI-generated, or altered.

**Status: Phases 1-4 done — images, text, PDFs, audio (WAV), and video (MP4).** See
[docs/ROADMAP.md](docs/ROADMAP.md) for what's next (product hardening) and
[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for how the pieces fit together.

## Why trust this over a black-box "AI or not" badge?

Every verdict ships with the full breakdown of independent signals that produced it —
for images: metadata forensics, Error Level Analysis, frequency-spectrum analysis,
sensor-noise residuals, and a pretrained AI-image classifier; for text: stylometry and
GPT-2 perplexity/burstiness; for PDFs: incremental-update revision forensics; for audio:
jitter/shimmer voice-naturalness, noise-floor consistency, and a pretrained RawNet2
anti-spoofing classifier; for video: per-frame reuse of the image forensic signals plus
a frame-to-frame consistency check — each with its own score, confidence, and
plain-language explanation. See [docs/model-sourcing.md](docs/model-sourcing.md) for exactly which
models are integrated, their license, and how reliable each detection domain actually
is (text is notably weaker than the others, and the UI says so).

## Quickstart

### Engine (C++)

```powershell
cd engine
.\scripts\bootstrap-vcpkg.ps1        # one-time: local vcpkg checkout
.\vcpkg\vcpkg.exe install --triplet x64-windows --x-install-root=vcpkg_installed
.\scripts\fetch_onnxruntime.ps1      # one-time: official ONNX Runtime prebuilt
.\scripts\fetch_models.ps1           # optional: AI-image classifier, GPT-2, RawNet2 weights (see docs/model-sourcing.md)

cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
.\build\Release\fakede_engine.exe    # serves on http://localhost:8080
```

The engine runs fine without any ONNX models fetched — each model-backed analyzer
reports itself unavailable and every other analyzer still produces a verdict.

### Frontend (React)

```powershell
cd web
npm install
npm run dev                          # http://localhost:5173, proxies /api -> :8080
```

## Repo layout

```
engine/   C++20 analysis engine (Drogon + OpenCV + ONNX Runtime + exiv2 + SQLite)
web/      Vite + React + TypeScript frontend
docs/     Architecture, roadmap, and model licensing/provenance notes
```
