<#
.SYNOPSIS
  Ensures engine/models/image/ai_image_classifier.onnx exists. If it's missing, walks
  through the steps to produce it, since converting UniversalFakeDetect involves a
  ~1.7GB CLIP ViT-L/14 download and isn't something to run silently/automatically.

  The engine runs fine without this model - AiGeneratedModelAnalyzer.isAvailable()
  just reports false and the other four image analyzers (metadata, ELA, frequency,
  noise-residual) still produce a verdict.
#>

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$modelPath = Join-Path $root "models\image\ai_image_classifier.onnx"
$exportDir = Join-Path $root "scripts\export_to_onnx"

if (Test-Path $modelPath) {
    Write-Output "Model already present at $modelPath"
    exit 0
}

Write-Output @"
ai_image_classifier.onnx is not present yet. This engine works without it (the
metadata/ELA/frequency/noise-residual analyzers still run), but the highest-weighted
signal in the fused verdict will be missing until it's added.

To produce it (one-time, Python required, ~1.7GB CLIP download):

  1. python -m venv $exportDir\.venv
  2. $exportDir\.venv\Scripts\Activate.ps1
  3. pip install -r $exportDir\requirements.txt
  4. git clone https://github.com/WisconsinAIVision/UniversalFakeDetect.git `$env:TEMP\UniversalFakeDetect
  5. python $exportDir\export_universal_fake_detect.py ``
       --fc-weights `$env:TEMP\UniversalFakeDetect\pretrained_weights\fc_weights.pth ``
       --output $modelPath

See docs/model-sourcing.md for license/provenance notes before shipping this model.
"@
