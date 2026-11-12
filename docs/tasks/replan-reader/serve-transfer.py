"""Serve unchanged upstream HTML with synthetic GET responses on loopback only."""
import json
import os
import sys
from http.server import BaseHTTPRequestHandler, HTTPServer
from pathlib import Path
from urllib.parse import urlsplit

candidate, port = sys.argv[1], int(sys.argv[2])
base = Path(os.environ['INK_DEPS']).parent / 'ink-hw/upstream/oss' / candidate
kiiko = candidate.startswith('kiiko')
html = 'components/read_pico_transfer/upload.html' if kiiko else 'src/network/html/' + ('zh-CN/' if candidate == 'crossmux' else '') + 'FilesPage.html'

class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        name = urlsplit(self.path).path
        fixtures = {
            '/info': {'is_flash': False, 'free_bytes': 1073741824},
            '/files': {'total': 2, 'page': 0, 'pages': 1, 'items': [{'name': 'books', 'directory': True, 'size': 0}, {'name': '示例.txt', 'directory': False, 'size': 4096}]},
            '/signature': {'text': ''},
            '/api/files': [{'name': 'books', 'isDirectory': True, 'size': 0}, {'name': '示例.txt', 'isDirectory': False, 'size': 4096}],
            '/api/status': {'version': 'offline-source-inspection', 'device': 'readpico'},
        }
        if name == '/':
            data, mime = (base / html).read_bytes(), 'text/html; charset=utf-8'
        elif name in fixtures:
            data, mime = json.dumps(fixtures[name]).encode(), 'application/json'
        elif name.startswith(('/assets/', '/js/')) and '..' not in name:
            file = base / 'src/network/html' / name[1:]
            if not file.is_file():
                self.send_error(404)
                return
            data = file.read_bytes()
            mime = 'text/css' if file.suffix == '.css' else 'image/svg+xml' if file.suffix == '.svg' else 'application/javascript'
        else:
            self.send_error(404)
            return
        self.send_response(200)
        self.send_header('Content-Type', mime)
        self.send_header('Cache-Control', 'no-store')
        self.send_header('Content-Security-Policy', "default-src 'self' data:; script-src 'self' 'unsafe-inline'; style-src 'self' 'unsafe-inline'; connect-src 'self'; frame-src 'none'; form-action 'none'")
        self.end_headers()
        self.wfile.write(data)

HTTPServer(('127.0.0.1', port), Handler).serve_forever()
