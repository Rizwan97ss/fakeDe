<#
.SYNOPSIS
  Ensures every ML model the engine can use is present: the image AI-classifier
  (UniversalFakeDetect) and the text perplexity model (GPT-2). For each missing one,
  prints the steps to produce it rather than downloading/converting silently - these
  involve multi-hundred-MB-to-GB downloads and aren't something to run unprompted.

  The engine works without any of them - each model-backed analyzer's isAvailable()
  just reports false and every other analyzer still runs and produces a verdict.
#>

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$exportDir = Join-Path $root "scripts\export_to_onnx"

$imageModelPath = Join-Path $root "models\image\ai_image_classifier.onnx"
$textModelPath = Join-Path $root "models\text\gpt2.onnx"

$missingAny = $false

if (Test-Path $imageModelPath) {
    Write-Output "Image classifier already present at $imageModelPath"
} else {
    $missingAny = $true
    Write-Output @"

--- Image AI-classifier (ai_image_classifier.onnx) ---
Not present. This engine works without it (metadata/ELA/frequency/noise-residual
analyzers still run), but the highest-weighted image signal will be missing.

To produce it (~1.7GB CLIP ViT-L/14 download):
  1. git clone https://github.com/WisconsinAIVision/UniversalFakeDetect.git `$env:TEMP\UniversalFakeDetect
  2. python $exportDir\export_universal_fake_detect.py ``
       --fc-weights `$env:TEMP\UniversalFakeDetect\pretrained_weights\fc_weights.pth ``
       --output $imageModelPath
"@
}

if (Test-Path $textModelPath) {
    Write-Output "Text perplexity model already present at $textModelPath"
} else {
    $missingAny = $true
    Write-Output @"

--- Text perplexity model (gpt2.onnx + vocab.json + merges.txt) ---
Not present. This engine works without it (text-stylometry still runs on its own,
just with lower overall confidence).

To produce it (~500MB GPT-2 download):
  python $exportDir\export_gpt2.py --output $textModelPath
"@
}

if ($missingAny) {
    Write-Output @"

--- One-time Python setup (shared by both exports above) ---
  1. python -m venv $exportDir\.venv
  2. $exportDir\.venv\Scripts\Activate.ps1
  3. pip install -r $exportDir\requirements.txt

See docs/model-sourcing.md for license/provenance notes on every model before shipping.
"@
} else {
    Write-Output "`nAll models present."
}
