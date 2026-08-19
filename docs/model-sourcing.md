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

## Not yet integrated (Phase 3-4 roadmap — see docs/ROADMAP.md)

- Text (stronger method): [`ahans30/Binoculars`](https://github.com/ahans30/Binoculars), [`baoguangsheng/fast-detect-gpt`](https://github.com/baoguangsheng/fast-detect-gpt)
- Audio: [`clovaai/aasist`](https://github.com/clovaai/aasist), RawNet2 anti-spoofing (Tak et al., arXiv:2011.01108)
- Video: [`SCLBD/DeepfakeBench`](https://github.com/SCLBD/DeepfakeBench) unified pretrained release, Self-Blended Images (SBI)

License and conversion notes for each of these get added here when that phase starts —
not before, so this file never claims more than what's actually integrated.
