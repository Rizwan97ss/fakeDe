"""
One-time conversion: RawNet2 anti-spoofing (Tak et al., arXiv:2011.01108), ASVspoof
2021 baseline checkpoint -> ONNX. Used by AntiSpoofingAnalyzer.

The RawNet class below is vendored (not imported from a cloned repo, so this script is
self-contained) from https://github.com/asvspoof-challenge/2021
(LA/Baseline-RawNet2/model.py, MIT license, by Hemlata Tak/EURECOM), with one
deliberate change: forward() returns raw logits instead of calling the original
model's final LogSoftmax - AntiSpoofingAnalyzer.cpp applies its own softmax, matching
every other multi-class ML analyzer in this codebase.

Pretrained weights: https://www.asvspoof.org/asvspoof2021/pre_trained_DF_RawNet2.zip

Export gotcha this model actually needs (unlike the other two models in this project):
SincConv's forward() builds its filter bank using numpy (np.sinc/np.hamming) from
fixed, input-independent hyperparameters set in __init__. torch.onnx.export's default
"dynamo" exporter (torch.export.export-based) does not tolerate that kind of
non-tensor, non-traceable code path. The legacy TorchScript-tracing exporter
(dynamo=False) handles it correctly and is semantically right here anyway: the sinc
filters are fixed after training, so baking their computed values into the graph as
constants during tracing is the correct behavior, not a workaround.

Usage:
    pip install -r requirements.txt
    python export_rawnet2.py --checkpoint path/to/pre_trained_DF_RawNet2/*.pth \
        --output ../../models/audio/antispoofing.onnx
"""

import argparse
from collections import OrderedDict
from pathlib import Path

import numpy as np
import torch
import torch.nn as nn
import torch.nn.functional as F
from torch import Tensor


class SincConv(nn.Module):
    @staticmethod
    def to_mel(hz):
        return 2595 * np.log10(1 + hz / 700)

    @staticmethod
    def to_hz(mel):
        return 700 * (10 ** (mel / 2595) - 1)

    def __init__(self, out_channels, kernel_size, in_channels=1, sample_rate=16000):
        super().__init__()
        self.out_channels = out_channels
        self.kernel_size = kernel_size + 1 if kernel_size % 2 == 0 else kernel_size
        self.sample_rate = sample_rate

        nfft = 512
        f = int(sample_rate / 2) * np.linspace(0, 1, int(nfft / 2) + 1)
        fmel = self.to_mel(f)
        filbandwidthsmel = np.linspace(np.min(fmel), np.max(fmel), out_channels + 1)
        self.mel = self.to_hz(filbandwidthsmel)
        self.hsupp = torch.arange(-(self.kernel_size - 1) / 2, (self.kernel_size - 1) / 2 + 1)
        self.band_pass = torch.zeros(out_channels, self.kernel_size)

    def forward(self, x):
        for i in range(len(self.mel) - 1):
            fmin, fmax = self.mel[i], self.mel[i + 1]
            hhigh = (2 * fmax / self.sample_rate) * np.sinc(2 * fmax * self.hsupp / self.sample_rate)
            hlow = (2 * fmin / self.sample_rate) * np.sinc(2 * fmin * self.hsupp / self.sample_rate)
            hideal = hhigh - hlow
            self.band_pass[i, :] = Tensor(np.hamming(self.kernel_size)) * Tensor(hideal)

        filters = self.band_pass.view(self.out_channels, 1, self.kernel_size)
        return F.conv1d(x, filters, stride=1, padding=0, dilation=1, bias=None, groups=1)


class ResidualBlock(nn.Module):
    def __init__(self, nb_filts, first=False):
        super().__init__()
        self.first = first
        if not first:
            self.bn1 = nn.BatchNorm1d(nb_filts[0])
        self.lrelu = nn.LeakyReLU(negative_slope=0.3)
        self.conv1 = nn.Conv1d(nb_filts[0], nb_filts[1], kernel_size=3, padding=1, stride=1)
        self.bn2 = nn.BatchNorm1d(nb_filts[1])
        self.conv2 = nn.Conv1d(nb_filts[1], nb_filts[1], kernel_size=3, padding=1, stride=1)
        self.downsample = nb_filts[0] != nb_filts[1]
        if self.downsample:
            self.conv_downsample = nn.Conv1d(nb_filts[0], nb_filts[1], kernel_size=1, padding=0, stride=1)
        self.mp = nn.MaxPool1d(3)

    def forward(self, x):
        # Faithfully reproduces an upstream quirk: when not self.first, bn1+lrelu are
        # computed into `out` but then immediately discarded - conv1 always runs on
        # the raw `x`, never on that `out`. Looks like a bug in the original baseline,
        # but the released pretrained weights were trained against exactly this graph,
        # so reproducing it (not "fixing" it) is what makes the checkpoint load and
        # score correctly. See asvspoof-challenge/2021 LA/Baseline-RawNet2/model.py.
        identity = x
        out = self.conv1(x)
        out = self.lrelu(self.bn2(out))
        out = self.conv2(out)
        if self.downsample:
            identity = self.conv_downsample(identity)
        out += identity
        return self.mp(out)


class RawNet(nn.Module):
    """Matches asvspoof-challenge/2021 LA/Baseline-RawNet2/model.py's RawNet, except
    forward() returns raw logits (no final LogSoftmax) - see module docstring."""

    def __init__(self, d_args):
        super().__init__()
        self.sinc_conv = SincConv(d_args["filts"][0], d_args["first_conv"], d_args["in_channels"])
        self.first_bn = nn.BatchNorm1d(d_args["filts"][0])
        self.selu = nn.SELU(inplace=True)

        self.block0 = nn.Sequential(ResidualBlock(d_args["filts"][1], first=True))
        self.block1 = nn.Sequential(ResidualBlock(d_args["filts"][1]))
        self.block2 = nn.Sequential(ResidualBlock(d_args["filts"][2]))
        d_args["filts"][2][0] = d_args["filts"][2][1]
        self.block3 = nn.Sequential(ResidualBlock(d_args["filts"][2]))
        self.block4 = nn.Sequential(ResidualBlock(d_args["filts"][2]))
        self.block5 = nn.Sequential(ResidualBlock(d_args["filts"][2]))
        self.avgpool = nn.AdaptiveAvgPool1d(1)

        # Each element is a Sequential wrapping one Linear, matching the checkpoint's
        # key structure exactly ("fc_attention{i}.0.weight" - the original
        # _make_attention_fc() wraps a single Linear in nn.Sequential).
        attn_in = [d_args["filts"][1][-1]] * 2 + [d_args["filts"][2][-1]] * 4
        self.fc_attention = nn.ModuleList([nn.Sequential(nn.Linear(n, n)) for n in attn_in])

        self.bn_before_gru = nn.BatchNorm1d(d_args["filts"][2][-1])
        self.gru = nn.GRU(
            input_size=d_args["filts"][2][-1],
            hidden_size=d_args["gru_node"],
            num_layers=d_args["nb_gru_layer"],
            batch_first=True,
        )
        self.fc1_gru = nn.Linear(d_args["gru_node"], d_args["nb_fc_node"])
        self.fc2_gru = nn.Linear(d_args["nb_fc_node"], d_args["nb_classes"], bias=True)
        self.sig = nn.Sigmoid()

    def _attend(self, block, x, attn_idx):
        out = block(x)
        y = self.avgpool(out).view(out.size(0), -1)
        y = self.fc_attention[attn_idx](y)
        y = self.sig(y).view(y.size(0), y.size(1), -1)
        return out * y + y

    def forward(self, x):
        nb_samp, len_seq = x.shape[0], x.shape[1]
        x = x.view(nb_samp, 1, len_seq)
        x = self.sinc_conv(x)
        x = F.max_pool1d(torch.abs(x), 3)
        x = self.selu(self.first_bn(x))

        for i, block in enumerate([self.block0, self.block1, self.block2, self.block3, self.block4, self.block5]):
            x = self._attend(block, x, i)

        x = self.selu(self.bn_before_gru(x))
        x = x.permute(0, 2, 1)
        self.gru.flatten_parameters()
        x, _ = self.gru(x)
        x = x[:, -1, :]
        x = self.fc1_gru(x)
        x = self.fc2_gru(x)
        return x  # raw logits - see module docstring


D_ARGS = {
    "nb_samp": 64600,
    "first_conv": 1024,
    "in_channels": 1,
    "filts": [20, [20, 20], [20, 128], [128, 128]],
    "nb_fc_node": 1024,
    "gru_node": 1024,
    "nb_gru_layer": 3,
    "nb_classes": 2,
}


def remap_state_dict(state_dict):
    """The vendored module names (sinc_conv, fc_attention.N) differ slightly from the
    upstream checkpoint's (Sinc_conv, fc_attentionN as separate top-level modules) -
    remap keys rather than exactly mirror the upstream naming, which was intentionally
    cleaned up above (a ModuleList instead of six near-identical attributes)."""
    remapped = OrderedDict()
    for key, value in state_dict.items():
        new_key = key.replace("Sinc_conv.", "sinc_conv.")
        for i in range(6):
            new_key = new_key.replace(f"fc_attention{i}.", f"fc_attention.{i}.")
        remapped[new_key] = value
    return remapped


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--checkpoint", required=True, help="Path to the pretrained .pth checkpoint")
    parser.add_argument("--output", default="../../models/audio/antispoofing.onnx")
    parser.add_argument("--opset", type=int, default=18)
    args = parser.parse_args()

    output_path = Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)

    d_args = {**D_ARGS, "filts": [D_ARGS["filts"][0], list(D_ARGS["filts"][1]), list(D_ARGS["filts"][2]), list(D_ARGS["filts"][2])]}
    model = RawNet(d_args)

    state_dict = torch.load(args.checkpoint, map_location="cpu")
    if "model_state_dict" in state_dict:
        state_dict = state_dict["model_state_dict"]
    model.load_state_dict(remap_state_dict(state_dict), strict=True)
    model.eval()

    dummy_input = torch.randn(1, D_ARGS["nb_samp"], dtype=torch.float32)

    torch.onnx.export(
        model,
        dummy_input,
        str(output_path),
        input_names=["waveform"],
        output_names=["logits"],
        opset_version=args.opset,
        do_constant_folding=True,
        dynamo=False,  # see module docstring - required for SincConv's numpy-based filter generation
    )

    print(f"Exported ONNX model to {output_path}")


if __name__ == "__main__":
    main()
