"""Device audio: libopus, 16 kHz mono, 60 ms, signed little-endian PCM."""
import ctypes as c
import os
import struct
import sys
from contextlib import contextmanager
from pathlib import Path

RATE = 16000
FRAME_SAMPLES = 960
FRAME_BYTES = FRAME_SAMPLES * 2
OPUS_APPLICATION_VOIP = 2048


class Opus:
    def __init__(self, path=None):
        suffix = "dylib" if sys.platform == "darwin" else "so"
        default = Path(__file__).resolve().parents[3] / "work" / "hub-opus" / f"libopus.{suffix}"
        try:
            self.lib = c.CDLL(str(path or os.environ.get("INK_HUB_LIBOPUS") or default))
            signatures = {
                "opus_encoder_create": ([c.c_int, c.c_int, c.c_int, c.POINTER(c.c_int)], c.c_void_p),
                "opus_decoder_create": ([c.c_int, c.c_int, c.POINTER(c.c_int)], c.c_void_p),
                "opus_encode": ([c.c_void_p, c.POINTER(c.c_int16), c.c_int, c.c_void_p, c.c_int32], c.c_int32),
                "opus_decode": ([c.c_void_p, c.c_char_p, c.c_int32, c.POINTER(c.c_int16), c.c_int, c.c_int], c.c_int),
                "opus_encoder_destroy": ([c.c_void_p], None),
                "opus_decoder_destroy": ([c.c_void_p], None),
            }
            for name, (args, result) in signatures.items():
                fn = getattr(self.lib, name)
                fn.argtypes, fn.restype = args, result
        except (OSError, AttributeError):
            raise RuntimeError("无法加载 Opus，请编译共享库并检查 INK_HUB_LIBOPUS") from None

    @contextmanager
    def encoder(self):
        error = c.c_int()
        handle = self.lib.opus_encoder_create(RATE, 1, OPUS_APPLICATION_VOIP, c.byref(error))
        if not handle or error.value:
            raise RuntimeError("无法创建 Opus 编码器")
        try:
            yield handle
        finally:
            self.lib.opus_encoder_destroy(handle)

    @contextmanager
    def decoder(self):
        error = c.c_int()
        handle = self.lib.opus_decoder_create(RATE, 1, c.byref(error))
        if not handle or error.value:
            raise RuntimeError("无法创建 Opus 解码器")
        try:
            yield handle
        finally:
            self.lib.opus_decoder_destroy(handle)

    def encode(self, handle, pcm):
        if len(pcm) != FRAME_BYTES:
            raise RuntimeError("Opus 输入必须是 960 个采样")
        samples = (c.c_int16 * FRAME_SAMPLES)(*struct.unpack("<960h", pcm))
        packet = (c.c_ubyte * 4000)()
        size = self.lib.opus_encode(handle, samples, FRAME_SAMPLES, packet, len(packet))
        if size < 0:
            raise RuntimeError("Opus 编码失败")
        return bytes(packet[:size])

    def decode(self, handle, packet):
        samples = (c.c_int16 * FRAME_SAMPLES)()
        count = self.lib.opus_decode(handle, packet, len(packet), samples, FRAME_SAMPLES, 0)
        if count != FRAME_SAMPLES:
            raise RuntimeError("Opus 帧无效，必须是 60 毫秒")
        return struct.pack("<960h", *samples)
