"""
One-time conversion: GPT-2 small (124M, OpenAI, MIT-licensed release via Hugging Face
`gpt2`) -> ONNX, used by TextPerplexityAnalyzer for perplexity/burstiness scoring.

Also writes vocab.json/merges.txt alongside the model - engine/src/analyzers/text/
Gpt2Tokenizer.cpp is a from-scratch C++ implementation of GPT-2's byte-level BPE that
reads these two files directly. No Python/HF tokenizer is used at runtime.

Usage:
    pip install -r requirements.txt
    python export_gpt2.py --output ../../models/text/gpt2.onnx
"""

import argparse
import shutil
from pathlib import Path

import torch
import torch.nn as nn
from huggingface_hub import hf_hub_download
from transformers import GPT2LMHeadModel


class Gpt2LogitsOnly(nn.Module):
    """Wraps GPT2LMHeadModel to export as a single-input (input_ids), single-output
    (logits) graph - no attention_mask/past_key_values, matching what
    OnnxSession::runInt64InputFloatOutput expects."""

    def __init__(self, model: GPT2LMHeadModel):
        super().__init__()
        self.model = model

    def forward(self, input_ids: torch.Tensor) -> torch.Tensor:
        return self.model(input_ids=input_ids).logits


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", default="../../models/text/gpt2.onnx")
    # 18, not 17: the exporter's opset-downgrade fallback (17 -> onnx C API convert)
    # has been observed to produce an invalid graph (Split with an opset-18-only
    # "num_outputs" attribute left in place) - exporting directly at the version the
    # exporter actually wants to use avoids that conversion path entirely.
    parser.add_argument("--opset", type=int, default=18)
    args = parser.parse_args()

    output_path = Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)

    print("Downloading GPT-2 (124M)...")
    hf_model = GPT2LMHeadModel.from_pretrained("gpt2")
    hf_model.eval()

    model = Gpt2LogitsOnly(hf_model)
    model.eval()
    dummy_input = torch.randint(0, 50257, (1, 16), dtype=torch.long)

    torch.onnx.export(
        model,
        dummy_input,
        str(output_path),
        input_names=["input_ids"],
        output_names=["logits"],
        dynamic_axes={"input_ids": {1: "sequence"}, "logits": {1: "sequence"}},
        opset_version=args.opset,
        do_constant_folding=True,
    )

    # Fetch the original vocab.json/merges.txt directly rather than going through a
    # tokenizer's save_vocabulary() - that method's output format has changed across
    # transformers versions (observed writing a single "tokenizer.model" file instead
    # of the classic pair on one run), so this is the more stable path.
    for filename in ("vocab.json", "merges.txt"):
        downloaded = hf_hub_download(repo_id="gpt2", filename=filename)
        shutil.copy(downloaded, output_path.parent / filename)

    print(f"Exported ONNX model to {output_path}")
    print(f"Tokenizer files written to {output_path.parent}")


if __name__ == "__main__":
    main()
