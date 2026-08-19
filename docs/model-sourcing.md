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
| License | **Verify before shipping** — check the upstream repo's LICENSE file at the exact commit used; not yet confirmed in this document |
| Conversion | `engine/scripts/export_to_onnx/export_universal_fake_detect.py` |
| Input | `float32[1,3,224,224]`, RGB, CLIP normalization (mean `[0.48145466, 0.4578275, 0.40821073]`, std `[0.26862954, 0.26130258, 0.27577711]`) |
| Output | `float32[1,1]` raw logit; sigmoid applied in `AiGeneratedModelAnalyzer` |
| Consumed by | `engine/src/analyzers/image/AiGeneratedModelAnalyzer.cpp` |

**Explicitly rejected**: [`PeterWang512/CNNDetection`](https://github.com/PeterWang512/CNNDetection) — CC BY-NC-SA 4.0,
non-commercial only, disqualifying for this product's stated ambition.

## Not yet integrated (Phase 2-4 roadmap — see docs/ROADMAP.md)

- Text: [`ahans30/Binoculars`](https://github.com/ahans30/Binoculars), [`baoguangsheng/fast-detect-gpt`](https://github.com/baoguangsheng/fast-detect-gpt)
- Audio: [`clovaai/aasist`](https://github.com/clovaai/aasist), RawNet2 anti-spoofing (Tak et al., arXiv:2011.01108)
- Video: [`SCLBD/DeepfakeBench`](https://github.com/SCLBD/DeepfakeBench) unified pretrained release, Self-Blended Images (SBI)

License and conversion notes for each of these get added here when that phase starts —
not before, so this file never claims more than what's actually integrated.
