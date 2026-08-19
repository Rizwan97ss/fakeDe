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

**Known, confirmed gap (2026-08-19): unreliable against modern commercial generators.** A user
confirmed a real DALL-E 3 (via ChatGPT) image was scored as "very likely a real photo" by this
classifier (0.0 fake-probability at 0.85 self-confidence), while 4 of the image pipeline's other 5
signals correctly leaned toward "AI/edited". This is not a bug specific to our integration —
research below confirms it's an industry-wide gap. `AiGeneratedModelAnalyzer` and
`VideoAiFrameAnalyzer` now carry an in-product caveat on their "looks authentic" reading for this
reason (see their explanation strings).

**Real mitigation added (2026-08-19), not a model swap**: the same user found that a competitor
tool caught this exact case not with a better classifier, but by reading the image's embedded
**C2PA Content Credentials** — provenance metadata OpenAI signs into its generated images.
`C2paManifestAnalyzer` now checks for this directly via byte-scan (see "Why C2PA Content
Credentials are detected via byte-scan" in `docs/ARCHITECTURE.md`) and is weighted above even
`ai-model:*` in fusion when it fires. This only helps when the manifest survives intact (it's
easily stripped by re-saving/converting), so it complements rather than replaces the classifier
gap above — not yet verified against the user's actual original file, only against synthetic
test markers (see project memory for follow-up status).

**Investigated and explicitly not integrated as a replacement/addition (2026-08-19):**

- A comprehensive 2026 benchmark ([arXiv:2602.07814](https://arxiv.org/html/2602.07814v1)) tested 23
  open-source detector variants across 16 methods (PatchCraft, AIDE, CNNSpot, SPAI, Effort,
  ForgeLens, DRCT, FreDect, Gram, LGrad, Fusing, UnivFD (this project's model family), Community-Forensics,
  SAFE, Forensic-MoE, and others). **Every one of them performs badly against current commercial
  generators**: ~31% mean accuracy on DALL-E 3, ~24% on Midjourney v7, 18-30% on Flux/Firefly v4 —
  all below chance. The best overall performer, Community-Forensics, still only reaches 35-42% on
  the newest generators despite 75% mean accuracy across the full benchmark. This is a real,
  current, industry-wide ceiling, not a gap specific to what this project has integrated.
  - Community-Forensics itself was ruled out immediately: it's primarily distributed as a *dataset*
    (`OwensLab/CommunityForensics` on Hugging Face) under CC BY-NC-SA 4.0 — non-commercial,
    same disqualifying reason as CNNDetection above — not a ready-to-use permissively-licensed model.
- [`shilinyan99/AIDE`](https://github.com/shilinyan99/AIDE) (ICLR 2025) was the most promising
  concrete candidate found: MIT-licensed, pretrained weights available, evaluated including
  Midjourney (though not confirmed against DALL-E 3). Ruled out after reading the actual source
  (`models/AIDE.py`, `data/dct.py`), not just the README, because it turned out impractical for
  this project's deployment shape:
  - Backbone is OpenCLIP's `convnext_xxlarge` — several GB on its own, far larger than the ~350MB
    CLIP ViT-L/14 already in use, plus two full ResNet-50 branches processing SRM-noise-filtered
    patches.
  - Preprocessing is not resize+normalize — `DCT_base_Rec_Module` unfolds the image into
    overlapping windows, scores each by a custom DCT-energy heuristic, sorts them, and selects the
    "smoothest" and "busiest" patches to feed the network. Faithfully reproducing this exactly in
    C++ (this project has no Python at runtime) would carry real bug risk, on the scale of or
    beyond the RawNet2 quirk-replication work in `export_rawnet2.py` — but for an unconfirmed,
    likely-modest accuracy gain given the benchmark numbers above.
  - CPU-only inference (this project's ONNX Runtime deployment target, no GPU) with a model this
    large would likely be far slower than acceptable for a synchronous HTTP request.
- [`mever-team/spai`](https://github.com/mever-team/spai) (CVPR 2025, Apache 2.0) was noted as a
  lighter-looking alternative (ViT-B/16-based vs. AIDE's ConvNeXt-XXLarge) but its preprocessing
  pipeline wasn't clearly documented and wasn't source-inspected before the investigation was
  stopped — a legitimate next candidate if this gets revisited, but not vetted enough to integrate.

**Conclusion, stated plainly**: given even the best current open-source system tops out around
35-42% on today's leading commercial generators, swapping or adding a model right now has a poor
effort/risk-to-payoff ratio. The honest fix available today is the in-product caveat above plus
leaning on the classical signals (which did correctly flag the confirmed DALL-E 3 test case) —
not a black-box model upgrade that wouldn't reliably solve the problem anyway. Revisit if a
future open-source release meaningfully changes these numbers.

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
- Image (modern-generator coverage): investigated and explicitly deferred, not just unscheduled -
  see "Known, confirmed gap" and "Investigated and explicitly not integrated" above for why AIDE
  and SPAI weren't adopted, and the industry-wide benchmark numbers behind that call

License and conversion notes for each of these get added here when integrated —
not before, so this file never claims more than what's actually integrated.
