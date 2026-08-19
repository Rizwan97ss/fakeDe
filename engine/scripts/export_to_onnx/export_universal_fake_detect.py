"""
One-time conversion: WisconsinAIVision/UniversalFakeDetect (CVPR 2023) -> ONNX.

This is run OFFLINE, separately from the C++ build (see requirements.txt for the
Python-only dependencies). The C++ engine only ever consumes the resulting .onnx file;
it never depends on Python/PyTorch at runtime.

Model: a frozen OpenAI CLIP ViT-L/14 visual encoder + a single trained linear layer
("pretrained_weights/fc_weights.pth" in the upstream repo) that outputs one logit -
sigmoid(logit) is the probability the image is AI-generated. See
https://github.com/WisconsinAIVision/UniversalFakeDetect for the paper and original
(non-ONNX) inference code; docs/model-sourcing.md tracks the exact commit/license used.

Usage:
    pip install -r requirements.txt
    python export_universal_fake_detect.py --fc-weights path/to/fc_weights.pth \
        --output ../../models/image/ai_image_classifier.onnx

The upstream repo ships fc_weights.pth directly (pretrained_weights/fc_weights.pth) -
clone it and pass that path here. CLIP ViT-L/14 backbone weights are downloaded
automatically by the `clip` package on first run (~1.7GB).
"""

import argparse
from pathlib import Path

import clip
import torch
import torch.nn as nn


class UniversalFakeDetectModel(nn.Module):
    """CLIP ViT-L/14 visual encoder (frozen) + linear probe, fused into one forward
    pass so the whole thing exports as a single ONNX graph with one input, one output.
    """

    def __init__(self, fc_weights_path: str):
        super().__init__()
        clip_model, _ = clip.load("ViT-L/14", device="cpu")
        self.visual = clip_model.visual.float()
        for p in self.visual.parameters():
            p.requires_grad = False

        # CLIP ViT-L/14's visual encoder outputs a 768-d embedding.
        self.fc = nn.Linear(768, 1)
        state = torch.load(fc_weights_path, map_location="cpu")
        # Upstream checkpoints are sometimes saved as a raw state_dict, sometimes
        # nested under a "model"/"fc" key - handle both.
        if isinstance(state, dict) and "fc" in state:
            state = state["fc"]
        elif isinstance(state, dict) and "model" in state:
            state = state["model"]
        self.fc.load_state_dict(state)

    def forward(self, x: torch.Tensor) -> torch.Tensor:
        features = self.visual(x)
        return self.fc(features)  # raw logit; sigmoid applied in the C++ analyzer


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--fc-weights", required=True, help="Path to fc_weights.pth from the upstream repo")
    parser.add_argument("--output", default="../../models/image/ai_image_classifier.onnx")
    parser.add_argument("--opset", type=int, default=17)
    args = parser.parse_args()

    output_path = Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)

    model = UniversalFakeDetectModel(args.fc_weights)
    model.eval()

    dummy_input = torch.randn(1, 3, 224, 224, dtype=torch.float32)

    torch.onnx.export(
        model,
        dummy_input,
        str(output_path),
        input_names=["input"],
        output_names=["logit"],
        opset_version=args.opset,
        do_constant_folding=True,
    )

    print(f"Exported ONNX model to {output_path}")
    print("Note: the C++ AiGeneratedModelAnalyzer expects a single-element output "
          "(one logit) and applies sigmoid itself.")


if __name__ == "__main__":
    main()
