# fakeDe web

Vite + React + TypeScript frontend for the fakeDe engine. See the [repo root README](../README.md)
for the full quickstart (engine + frontend together) and [../docs/](../docs/) for architecture notes.

```powershell
npm install
npm run dev       # http://localhost:5173, proxies /api -> the engine on :8080
npm run build     # type-checks and produces web/dist/
```

## Layout

```
src/api/         Typed client + response types, mirroring engine/src/core/Types.h
src/components/  upload/ (Dropzone) and results/ (verdict + evidence breakdown UI)
src/pages/       HomePage (upload state) and ResultsPage (verdict display)
src/lib/         Shared UI helpers (status/color mapping)
```
