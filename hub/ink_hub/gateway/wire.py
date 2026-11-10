"""Device protocol §2: count the serialized envelope, including keys."""

import json
import struct

MAX_TOKENS = 128
PAGE_TOKENS = 110
MAX_TEXT = 511
ANSWER_CUT = "\n回答太长，只显示了前面部分。"  # the device's INK_AI_ANSWER_CUT
ANSWER_BYTES = 32768 - len(ANSWER_CUT.encode("utf-8")) - 1  # what the device keeps of a turn (INK_AI_ANSWER_MAX) before it cuts
HEADER = struct.Struct("<BBHIBB")


class ProtocolError(ValueError):
    def __init__(self, message, code=-32602):
        super().__init__(message)
        self.code = code


def tokens(value):
    if isinstance(value, dict):
        return 1 + sum(1 + tokens(v) for v in value.values())
    if isinstance(value, list):
        return 1 + sum(tokens(v) for v in value)
    return 1


def dumps(value):
    return json.dumps(value, ensure_ascii=False, separators=(",", ":"), allow_nan=False)


def encode(message):
    text = dumps(message)
    actual = json.loads(text)
    if tokens(actual) > MAX_TOKENS:
        raise ProtocolError("消息超过设备解析上限")
    return text


def response(rid, result):
    return {"jsonrpc": "2.0", "id": rid, "result": result}


def notification(method, params):
    return {"jsonrpc": "2.0", "method": method, "params": params}


def chunks(text, limit=MAX_TEXT, total=None):
    raw = text.encode("utf-8")
    if total is not None:
        raw = raw[:total]
    while raw:
        part = raw[:limit].decode("utf-8", errors="ignore")
        if not part:
            break
        yield part
        raw = raw[len(part.encode("utf-8")):]


def answer(text):
    """The turn's text as the device can keep it: an over-long answer ends with the device's own note."""
    if len(text.encode("utf-8")) <= ANSWER_BYTES:
        return text
    return "".join(chunks(text, total=ANSWER_BYTES - len(ANSWER_CUT.encode("utf-8")))) + ANSWER_CUT


def page(entries, rid, cursor=None, rev=1):
    try:
        start = int(cursor or "0")
    except (TypeError, ValueError):
        raise ProtocolError("分页位置无效") from None
    if start < 0 or start > len(entries):
        raise ProtocolError("分页位置无效")
    result = {"tools": [], "rev": rev}
    end = start
    for item in entries[start:]:
        candidate = {"tools": result["tools"] + [item], "rev": rev}
        if end + 1 < len(entries):
            candidate["nextCursor"] = str(end + 1)
        if tokens(json.loads(dumps(response(rid, candidate)))) > PAGE_TOKENS:
            break
        result = candidate
        end += 1
    if end == start and start < len(entries):
        raise ProtocolError("单条能力描述超过分页预算")
    return result


def frame(kind, sid, seq, payload, flags=0):
    return HEADER.pack(kind, sid, seq & 65535, seq * 60, flags, 0) + payload


def parse_frame(data):
    if len(data) < HEADER.size:
        raise ProtocolError("帧头不完整")
    kind, sid, seq, millis, flags, reserved = HEADER.unpack_from(data)
    if reserved or kind not in range(1, 8) or flags & ~7:
        raise ProtocolError("帧头无效")
    return kind, sid, seq, millis, flags, data[HEADER.size:]
