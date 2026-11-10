"""One-time synthetic fixtures: qrcode 8.2 (BSD), Pillow; no network or personal data."""
from pathlib import Path
import json
import qrcode
from PIL import Image, ImageFilter

ROOT = Path(__file__).resolve().parent / "images"
ROOT.mkdir(exist_ok=True)
PAYLOADS = {
    "url": "https://example.com/qr",
    "tel": "tel:10000000000",
    "mecard": "MECARD:N:测试联系人;TEL:10000000000;;",
    "vcard": "BEGIN:VCARD\nVERSION:3.0\nFN:测试联系人\nTEL:10000000000\nEND:VCARD",
    "wifi": "WIFI:S:qr-test;T:WPA;P:FAKE-qr-secret;;",
    "text": "你好，二维码。Hello QR!",
    "tilted": "https://example.com/qr",
    "blurred": "https://example.com/qr",
    "small": "https://example.com/qr",
    "escaped": r"WIFI:S:qr\;test;T:WPA;P:FAKE\;secret;;",
}
for name, text in PAYLOADS.items():
    qr = qrcode.QRCode(error_correction=qrcode.constants.ERROR_CORRECT_M, box_size=3 if name == "small" else 7, border=4)
    qr.add_data(text.encode("utf-8"))
    qr.make(fit=True)
    im = qr.make_image().convert("L")
    if name == "tilted":
        im = im.rotate(23, resample=Image.Resampling.BICUBIC, expand=True, fillcolor=255)
    if name == "blurred":
        im = im.filter(ImageFilter.GaussianBlur(1.2))
    frame = Image.new("L", (640, 480), 255)
    frame.paste(im, ((640 - im.width) // 2, (480 - im.height) // 2))
    frame.save(ROOT / f"{name}.png", optimize=True)
Image.new("L", (640, 480), 255).save(ROOT / "blank.png", optimize=True)
(ROOT / "payloads.json").write_text(json.dumps(PAYLOADS, ensure_ascii=False, indent=2) + "\n")
