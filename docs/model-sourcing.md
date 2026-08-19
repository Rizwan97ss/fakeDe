# Model sourcing & licensing

Every ML model that ships in the product is tracked here: source, license, and how it
was converted to ONNX. Check this **before** any model goes into a build that reaches
real users — several detection-model repos in this space carry non-commercial
licenses, which is disqualifying for a monetized/public product.

## Image: AI-generated classifier

| | |
|---|---|
| Model | CLIP ViT-L/14 (frozen) + linear probe |
| Source | [WisconsinAIVision/UniversalFakeDetect](https://github.com/WisconsinAIVision/UniversalFakeDetect) (Ojha et al., CVPR 2023) |
| Checkpoint | `pretrained_weights/fc_weights.pth` from the upstream repo (linear layer only; CLIP backbone weights come from OpenAI's `clip` package) |
| License | MIT (WisconsinAIVision/UniversalFakeDetect repo, confirmed at commit `76a0e3e`, 2025 copyright) |
| Conversion | `engine/scripts/export_to_onnx/export_universal_fake_detect.py` |
| Input | `float32[1,3,224,224]`, RGB, CLIP normalization (mean `[0.48145466, 0.4578275, 0.40821073]`, std `[0.26862954, 0.26130258, 0.27577711]`) |
| Output | `float32[1,1]` raw logit; sigmoid applied in `AiGeneratedModelAnalyzer` |
| Consumed by | `engine/src/analyzers/image/AiGeneratedModelAnalyzer.cpp` |
| Status | **Live** — fetched and verified working (2026-08-19) |

**Explicitly rejected**: [`PeterWang512/CNNDetection`](https://github.com/PeterWang512/CNNDetection) — CC BY-NC-SA 4.0,
non-commercial only, disqualifying for this product's stated ambition.

## Text: language-model perplexity

| | |
|---|---|
| Model | GPT-2 small (124M), OpenAI's original open release, distributed via Hugging Face `gpt2` |
| License | MIT (OpenAI's `gpt-2` repo license; the Hugging Face-hosted weights mirror that release) |
| Conversion | `engine/scripts/export_to_onnx/export_gpt2.py` (also fetches `vocab.json`/`merges.txt` directly from the `gpt2` HF repo — not via a tokenizer's `save_vocabulary()`, whose output format proved unstable across `transformers` versions) |
| Tokenizer | **Not** a bundled/HF runtime dependency — `engine/src/analyzers/text/Gpt2Tokenizer.cpp` is a from-scratch C++ implementation of GPT-2's byte-level BPE algorithm, verified against the tokenizer's well-known "Ġ = space" artifact and against sensible perplexity discrimination on real test text |
| Input | `int64[1,seqLen]` token ids (≤256 tokens; longer text is truncated, not chunked) |
| Output | `float32[1,seqLen,50257]` logits; per-token log-softmax computed in C++ |
| Consumed by | `engine/src/analyzers/text/TextPerplexityAnalyzer.cpp` |
| Status | **Live** — fetched and verified working (2026-08-19) |
| Known gap | Single-model perplexity + burstiness (classic GPTZero-era method), not the ratio-of-two-models approach used by Binoculars/Fast-DetectGPT — see "Not yet integrated" below. Text remains the least reliable detection domain in this product regardless. |

**Export gotcha worth knowing**: requesting ONNX opset 17 from `torch.onnx.export` on
this model triggered an automatic "downgrade" conversion (exporter defaults to opset 18,
then converts down) that silently produced an invalid graph — a `Split` node kept an
opset-18-only `num_outputs` attribute, which ONNX Runtime rejected at load time.
Exporting directly at opset 18 avoids the conversion path entirely. Both export scripts
now default to opset 18 for this reason.

## Audio: synthetic-speech (anti-spoofing) classifier

| | |
|---|---|
| Model | RawNet2 (Tak et al., ICASSP 2021, arXiv:2011.01108) - raw-waveform CNN + GRU with a fixed (non-learned) SincNet-style front end |
| Source | [asvspoof-challenge/2021](https://github.com/asvspoof-challenge/2021) `LA/Baseline-RawNet2/model.py` |
| Checkpoint | [`pre_trained_DF_RawNet2.zip`](https://www.asvspoof.org/asvspoof2021/pre_trained_DF_RawNet2.zip), the official ASVspoof2021 DF-track pretrained release |
| License | MIT (`asvspoof-challenge/2021` repo, copyright 2021 eurecom-asp) |
| Conversion | `engine/scripts/export_to_onnx/export_rawnet2.py` - vendors the model class (self-contained script, not a repo dependency) with one deliberate change (returns raw logits, not the original's final LogSoftmax) and one deliberate *non*-change: an upstream quirk in `Residual_block.forward()` where `bn1`+`lrelu` are computed then discarded (conv1 always runs on the raw input) is faithfully reproduced rather than "fixed", since the released weights were trained against that exact graph |
| Input | `float32[1,64600]` raw waveform, 16kHz mono, padded (tiled) or cropped to exactly 64600 samples (~4s) - matches the official baseline's `pad()` convention in `data_utils.py` |
| Output | `float32[1,2]` raw logits (index 0 = bonafide/real, index 1 = spoof/fake per ASVspoof convention); softmax applied in `AntiSpoofingAnalyzer` |
| Consumed by | `engine/src/analyzers/audio/AntiSpoofingAnalyzer.cpp` |
| Status | **Live** — fetched and verified working (2026-08-19): loads, runs, and produces confident, sensible (non-degenerate) output on test audio. Not yet validated for actual bonafide/spoof *discrimination* against real speech samples - the test inputs used were synthetic sine tones, not real human speech or real TTS output, so this confirms the pipeline works end-to-end but not yet its real-world accuracy. |

**Export gotcha worth knowing (different from the opset-18 one above)**: `SincConv`'s
filter bank is built with `numpy` operations (`np.sinc`/`np.hamming`) from fixed,
input-independent hyperparameters - `torch.onnx.export`'s default "dynamo" exporter
doesn't tolerate that. The legacy TorchScript-tracing exporter (`dynamo=False`) handles
it correctly, and is semantically the *right* choice here (not a workaround): since the
filters are fixed after training, baking their traced values in as graph constants is
exactly correct.

## Video: no new model — reuses the image classifier per-frame

`VideoAiFrameAnalyzer` (`engine/src/analyzers/video/VideoAiFrameAnalyzer.cpp`) calls
the existing `AiGeneratedModelAnalyzer` (UniversalFakeDetect, above) once per sampled
frame and averages the score. No separate video/deepfake-specific model — e.g.
Self-Blended Images or `SCLBD/DeepfakeBench` — was fetched or integrated. This is a
real, known accuracy gap: the image classifier was trained on still images from
GAN/diffusion generators, not on video-specific manipulation artifacts (face-swap
boundaries, temporal flicker), so its per-frame scores on video should be read as a
weaker signal than they are for standalone images. Tracked as an open item below.

## Not yet integrated

- Text (stronger method): [`ahans30/Binoculars`](https://github.com/ahans30/Binoculars), [`baoguangsheng/fast-detect-gpt`](https://github.com/baoguangsheng/fast-detect-gpt)
- Audio (stronger/ensemble method): [`clovaai/aasist`](https://github.com/clovaai/aasist) - graph-attention layers make its ONNX export a real open question, unlike RawNet2 above
- Video (dedicated model): [`SCLBD/DeepfakeBench`](https://github.com/SCLBD/DeepfakeBench) unified pretrained release, Self-Blended Images (SBI) - would replace/augment the per-frame image-classifier reuse described above with a model actually trained on video manipulation artifacts

License and conversion notes for each of these get added here when integrated —
not before, so this file never claims more than what's actually integrated.
