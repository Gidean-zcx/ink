"""Chat Completions and Messages SSE adapters for the existing one-tool turn."""
import json
import os
from urllib.parse import urlsplit

from aiohttp import ClientError, ClientSession, ClientTimeout

from .fake import ModelResult

SYSTEM_PROMPT = (
    "你是这家人用的墨水屏设备上的助手。不要给自己起名字，不要自称哪个模型或代号；"
    "被问你是谁，就说是这台设备上的助手。屏幕小，回答还要朗读，回答简短，默认三句以内。"
    "要操作设备就调用一个能力，按说明填全参数；拿不准先问一句，不要编参数。"
    "设备回执回来之前不要说已经做好了。不知道的事不要编。孩子在用时用孩子听得懂的话。"
    "按设备给的 lang 回答。"
)


class RemoteModel:
    def __init__(self, provider, prefix, *, deepseek=False):
        self.provider, self.prefix = provider, prefix
        self.model = "deepseek-chat" if deepseek else os.environ.get(prefix + "_MODEL", "")
        default = "https://api.deepseek.com" if deepseek else "https://api.openai.com/v1" if provider == "openai" else "https://api.anthropic.com/v1"
        self.base = (default if deepseek else os.environ.get(prefix + "_BASE_URL", default)).rstrip("/")
        url = urlsplit(self.base)
        if (not self.model or url.username or url.password or url.query or url.fragment
                or not url.hostname or (url.scheme != "https" and
                    not (url.scheme == "http" and url.hostname in ("127.0.0.1", "::1", "localhost")))):
            raise RuntimeError("模型名称或服务地址无效")

    async def _events(self, payload):
        # Read credentials only from the environment, never from caller arguments.
        key = os.environ.get(self.prefix + "_API_KEY")
        if not key:
            raise RuntimeError("模型凭据未配置")
        anthropic = self.provider == "anthropic"
        headers = {"x-api-key": key, "anthropic-version": "2023-06-01"} if anthropic else {"Authorization": "Bearer " + key}
        path = "/messages" if anthropic else "/chat/completions"
        try:
            async with ClientSession(timeout=ClientTimeout(total=40, sock_read=15), trust_env=False) as session:
                async with session.post(self.base + path, json=payload, headers=headers, allow_redirects=False) as reply:
                    if reply.status != 200 or reply.content_type != "text/event-stream":
                        raise RuntimeError("模型服务拒绝请求")
                    data, size = [], 0
                    async for raw in reply.content:
                        size += len(raw)
                        if size > 1024 * 1024:
                            raise RuntimeError("模型回复过长")
                        line = raw.decode("utf-8").rstrip("\r\n")
                        if line.startswith("data:"):
                            data.append(line[5:].lstrip(" "))
                        elif not line and data:
                            body = "\n".join(data)
                            data = []
                            if body == "[DONE]":
                                return
                            event = json.loads(body)
                            if not isinstance(event, dict) or "error" in event or event.get("type") == "error":
                                raise RuntimeError("模型回复无效")
                            yield event
        except (ClientError, TimeoutError, ValueError, UnicodeError):
            # Vendor bodies, URLs, headers and exception messages may contain credentials.
            raise RuntimeError("模型服务请求失败") from None

    async def stream(self, question, tools, lang="zh", member=None):
        tools = tools or {}
        names = {"ink_" + str(i): name for i, name in enumerate(tools)}
        descriptors = [{"name": key, "description": tools[name].get("title", "设备能力：" + name),
                        "parameters": tools[name].get("inputSchema", {"type": "object"})}
                       for key, name in names.items()]
        instruction = SYSTEM_PROMPT + "\nlang=" + lang
        if member:
            instruction += "\n当前使用者：" + ("孩子，用孩子听得懂的话。" if member.get("role") == "child" else "大人。")
            if os.environ.get("INK_HUB_SHARE_MEMBER_NAME") == "1" and member.get("name"):
                instruction += "\n称呼：" + str(member["name"])
        payload = {"model": self.model, "stream": True, "messages": [{"role": "user", "content": question}]}
        if self.provider == "openai":
            payload["messages"].insert(0, {"role": "system", "content": instruction})
            if descriptors:
                payload.update(tools=[{"type": "function", "function": d} for d in descriptors], parallel_tool_calls=False)
        else:
            payload.update(system=instruction, max_tokens=2048)
            if descriptors:
                payload["tools"] = [{"name": d["name"], "description": d["description"],
                                     "input_schema": d["parameters"]} for d in descriptors]
        calls, finished = {}, False
        try:
            async for event in self._events(payload):
                text = ""
                if self.provider == "openai":
                    choices = event.get("choices", [])
                    if not choices:
                        continue  # Usage-only chunk.
                    choice = choices[0]
                    delta = choice.get("delta", {})
                    text = delta.get("content") or ""
                    if choice.get("finish_reason") is not None:
                        if choice["finish_reason"] not in ("stop", "tool_calls"):
                            raise RuntimeError("模型回复未完成")
                        finished = True
                    for call in delta.get("tool_calls", []):
                        item = calls.setdefault(call["index"], ["", ""])
                        function = call.get("function", {})
                        item[0] += function.get("name", "")
                        item[1] += function.get("arguments", "")
                else:
                    kind = event.get("type")
                    if kind == "content_block_start":
                        block = event["content_block"]
                        if block["type"] == "tool_use":
                            calls[event["index"]] = [block["name"], ""]
                        elif block["type"] == "text":
                            text = block.get("text", "")
                    elif kind == "content_block_delta":
                        delta = event["delta"]
                        if delta["type"] == "text_delta":
                            text = delta["text"]
                        elif delta["type"] == "input_json_delta":
                            calls[event["index"]][1] += delta["partial_json"]
                    elif kind == "message_delta":
                        if event.get("delta", {}).get("stop_reason") not in ("end_turn", "tool_use", "stop_sequence"):
                            raise RuntimeError("模型回复未完成")
                    elif kind == "message_stop":
                        finished = True
                if not isinstance(text, str):
                    raise RuntimeError("模型回复无效")
                if text:
                    yield ModelResult(text)
            if not finished or len(calls) > 1:
                raise RuntimeError("模型回复未完成或工具调用过多")
            if calls:
                name, arguments = next(iter(calls.values()))
                arguments = json.loads(arguments or "{}")
                if name not in names or not isinstance(arguments, dict) or "_meta" in arguments:
                    raise RuntimeError("模型工具调用无效")
                yield ModelResult("", names[name], arguments)
        except (KeyError, TypeError, ValueError, IndexError, AttributeError):
            raise RuntimeError("模型回复无效") from None

    async def answer(self, question, tools, lang="zh", member=None):
        text, tool, arguments = [], None, None
        async for part in self.stream(question, tools, lang, member):
            text.append(part.text)
            if part.tool:
                tool, arguments = part.tool, part.arguments
        return ModelResult("".join(text), tool, arguments)
