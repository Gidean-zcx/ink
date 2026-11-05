import html
import asyncio
import json
import os
import secrets
from pathlib import Path

from aiohttp import web


class Setup:
    def __init__(self, directory):
        self.path = Path(directory) / "setup.json"
        self.code = secrets.token_urlsafe(24)
        self.progress = json.loads(self.path.read_text()) if self.path.exists() else {"mode": "example", "step": 0}

    def update(self, values):
        if set(values) - {"step", "mode"} or values.get("mode", "example") != "example":
            raise ValueError("第 1 步只开放示例数据；账号、备份和真实服务尚未接入")
        step = values.get("step", self.progress["step"])
        if type(step) is not int or step not in range(5):
            raise ValueError("步骤无效")
        self.progress = {"mode": "example", "step": step}
        self.path.parent.mkdir(parents=True, exist_ok=True)
        temporary = self.path.with_suffix(".tmp")
        with temporary.open("w") as stream:
            os.chmod(temporary, 0o600)
            json.dump(self.progress, stream)
        temporary.replace(self.path)
        return dict(self.progress)

    async def page(self, request):
        return web.Response(text="""<!doctype html><html lang="zh-CN"><meta charset="utf-8">
<title>设置家庭服务器</title><h1>设置家庭服务器</h1>
<p>当前为示例演示，只能使用示例资料。账号、家庭数据保护和真实服务还没有接入。</p>
<ol><li>准备服务：本机通道已就绪，账号在第 3 步接入。</li>
<li>保护家庭数据：真实使用前要连接对象存储、确认加密方式并演练恢复。</li>
<li>接通模型和语音：本次只测试示例服务，不接收密钥。</li>
<li>连接设备：可连接电脑上的模拟设备，配对在第 3 步接入。</li></ol>
<form id="wizard"><label>本次设置码 <input name="code" type="password" autocomplete="off"></label>
<label>已查看到第几段 <select name="step"><option>1</option><option>2</option><option>3</option><option>4</option></select></label>
<button>保存进度</button></form><p>PROGRESS</p><p id="result"></p>
<script>document.querySelector('form').onsubmit=async e=>{e.preventDefault();
let f=new FormData(e.target);let r=await fetch('/setup',{method:'POST',headers:{'Content-Type':'application/json','X-Setup-Code':f.get('code')},body:JSON.stringify({step:Number(f.get('step')),mode:'example'})});
if(r.ok){let saved=await r.json();e.target.elements.code.value=saved.next_code;}
document.querySelector('#result').textContent=r.ok?'进度已保存，下次可继续':'没有保存，请核对设置码和连接方式。';};</script>
</html>""".replace("PROGRESS", "已查看到第 " + html.escape(str(self.progress["step"])) + " 段。"), content_type="text/html")

    async def save(self, request):
        # Proxy headers are deliberately ignored. There is no public setup mode in step 1.
        origin = request.headers.get("Origin")
        if request.remote not in ("127.0.0.1", "::1") or (origin and origin != f"{request.scheme}://{request.host}"):
            raise web.HTTPForbidden(text="请通过本机通道设置")
        if not secrets.compare_digest(request.headers.get("X-Setup-Code", ""), self.code):
            raise web.HTTPForbidden(text="设置码不正确")
        try:
            result = await asyncio.to_thread(self.update, await request.json())
        except (ValueError, TypeError, AttributeError):
            raise web.HTTPBadRequest(text="仅可保存示例向导进度，不接收密钥") from None
        self.code = secrets.token_urlsafe(24)
        return web.json_response({**result, "next_code": self.code})
