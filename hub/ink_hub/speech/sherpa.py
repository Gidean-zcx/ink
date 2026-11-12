"""Resident streaming Paraformer; no model download or audio persistence."""
import os
import asyncio
from pathlib import Path

from .opus import Opus, RATE


class SherpaASR:
    def __init__(self, directory=None, opus=None):
        root = Path(directory or os.environ.get("INK_HUB_SHERPA_MODEL", ""))
        files = {key: str(root / name) for key, name in (("encoder", "encoder.int8.onnx"),
                                                       ("decoder", "decoder.int8.onnx"), ("tokens", "tokens.txt"))}
        if not all(Path(path).is_file() for path in files.values()):
            raise RuntimeError("Paraformer 模型不完整，请检查 INK_HUB_SHERPA_MODEL（不会自动下载）")
        try:
            import numpy as np
            import sherpa_onnx
        except ImportError:
            raise RuntimeError("本地流式识别需要 numpy 和 sherpa-onnx；不会自动安装") from None
        self.np, self.opus = np, opus or Opus()
        self.model = sherpa_onnx.OnlineRecognizer.from_paraformer(**files, num_threads=2, sample_rate=RATE,
                                                                feature_dim=80, decoding_method="greedy_search")

    async def stream(self, audio):
        stream = self.model.create_stream()
        def decode():
            while self.model.is_ready(stream):
                self.model.decode_stream(stream)
            return self.model.get_result(stream)
        async for pcm in audio:
            stream.accept_waveform(RATE, self.np.frombuffer(pcm, dtype=self.np.int16).astype(self.np.float32) / 32768)
            yield await asyncio.to_thread(decode), False
        stream.accept_waveform(RATE, self.np.zeros(RATE // 2, dtype=self.np.float32))
        stream.input_finished()
        yield await asyncio.to_thread(decode), True
