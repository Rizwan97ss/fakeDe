<#
.SYNOPSIS
  Ensures every ML model the engine can use is present: the image AI-classifier
  (UniversalFakeDetect), the text perplexity model (GPT-2), and the audio
  anti-spoofing model (RawNet2). For each missing one, prints the steps to produce it
  rather than downloading/converting silently - these involve multi-hundred-MB-to-GB
  downloads and aren't something to run unprompted.

  The engine works without any of them - each model-backed analyzer's isAvailable()
  just reports false and every other analyzer still runs and produces a verdict.
#>

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$exportDir = Join-Path $root "scripts\export_to_onnx"

$imageModelPath = Join-Path $root "models\image\ai_image_classifier.onnx"
$textModelPath = Join-Path $root "models\text\gpt2.onnx"
$audioModelPath = Join-Path $root "models\audio\antispoofing.onnx"

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

if (Test-Path $audioModelPath) {
    Write-Output "Audio anti-spoofing model already present at $audioModelPath"
} else {
    $missingAny = $true
    Write-Output @"

--- Audio anti-spoofing model (antispoofing.onnx) ---
Not present. This engine works without it (voice-naturalness and splice-detection
still run on their own).

To produce it (~65MB checkpoint download):
  1. Invoke-WebRequest https://www.asvspoof.org/asvspoof2021/pre_trained_DF_RawNet2.zip -OutFile `$env:TEMP\rawnet2.zip
  2. Expand-Archive `$env:TEMP\rawnet2.zip -DestinationPath `$env:TEMP\rawnet2 -Force
  3. python $exportDir\export_rawnet2.py --checkpoint `$env:TEMP\rawnet2\pre_trained_DF_RawNet2.pth --output $audioModelPath
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
