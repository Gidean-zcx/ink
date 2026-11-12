"""XiaoZhi Silero windows and hysteresis; each stream owns recurrent state.

Copyright (c) 2025 xinnan-tech. MIT; see XIAOZHI-LICENSE.
"""
import math
import struct


class VoiceActivity:
    def __init__(self, model=None):
        self.voiced, self.quiet, self.last = False, 0, False
        self.window, self.buffer = [], bytearray()
        self.session = None
        if model:
            try:
                import numpy as np
                import onnxruntime as ort
            except ImportError:
                raise RuntimeError("Silero 需要 numpy、onnxruntime 和本地模型；不会自动下载") from None
            self.np = np
            options = ort.SessionOptions()
            options.inter_op_num_threads = options.intra_op_num_threads = 1
            self.session = ort.InferenceSession(model, sess_options=options, providers=["CPUExecutionProvider"])
            self.state, self.context = np.zeros((2, 1, 128), np.float32), np.zeros((1, 64), np.float32)

    def feed(self, pcm):
        if self.session is None:
            samples = struct.unpack(f"<{len(pcm) // 2}h", pcm)
            voice = math.sqrt(sum(s * s for s in samples) / max(1, len(samples))) >= 100
        else:
            self.buffer.extend(pcm)
            voice = self.last
            while len(self.buffer) >= 1024:
                chunk, self.buffer = self.buffer[:1024], self.buffer[1024:]
                audio = self.np.frombuffer(chunk, dtype=self.np.int16).astype(self.np.float32) / 32768
                audio = self.np.concatenate([self.context, audio.reshape(1, -1)], axis=1)
                prob, self.state = self.session.run(None, {"input": audio, "state": self.state,
                                                          "sr": self.np.array(16000, self.np.int64)})
                probability = prob.item()
                self.last = True if probability >= .5 else False if probability <= .2 else self.last
                self.context = audio[:, -64:]
                self.window = (self.window + [self.last])[-5:]
                voice = self.window.count(True) >= 3
        self.voiced |= voice
        self.quiet = 0 if voice else self.quiet + len(pcm) // 32
        return self.voiced and self.quiet >= 600
