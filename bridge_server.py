from __future__ import annotations

import io
import json
import os
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlparse
import urllib.request
import urllib.error

from web_handlers import WebHandlers

ROOT = Path(__file__).resolve().parent
WEB_ROOT = ROOT / "web"
handlers = WebHandlers()


def json_response(handler, payload, status=200):
    data = json.dumps(payload, ensure_ascii=False).encode('utf-8')
    handler.send_response(status)
    handler.send_header('Content-Type', 'application/json; charset=utf-8')
    handler.send_header('Content-Length', str(len(data)))
    handler.send_header('Access-Control-Allow-Origin', '*')
    handler.end_headers()
    handler.wfile.write(data)


class App(BaseHTTPRequestHandler):
    def do_OPTIONS(self):
        self.send_response(204)
        self.send_header('Access-Control-Allow-Origin', '*')
        self.send_header('Access-Control-Allow-Methods', 'GET, POST, OPTIONS')
        self.send_header('Access-Control-Allow-Headers', 'Content-Type')
        self.end_headers()

    def do_GET(self):
        parsed = urlparse(self.path)
        if parsed.path == '/api/presets':
            from preset_modes import PresetModes
            return json_response(self, {"success": True, "data": PresetModes.list_all()})
        if parsed.path == '/api/preview':
            return json_response(self, handlers.handle_preview())
        if parsed.path == '/api/validate-paths':
            return json_response(self, handlers.handle_validate_paths())
        if parsed.path == '/':
            return self._serve_file(WEB_ROOT / 'index.html', 'text/html; charset=utf-8')
        target = (WEB_ROOT / parsed.path.lstrip('/')).resolve()
        if WEB_ROOT.resolve() in target.parents and target.is_file():
            ctype = 'text/plain; charset=utf-8'
            if target.suffix == '.html': ctype = 'text/html; charset=utf-8'
            elif target.suffix == '.js': ctype = 'application/javascript; charset=utf-8'
            elif target.suffix == '.css': ctype = 'text/css; charset=utf-8'
            elif target.suffix == '.svg': ctype = 'image/svg+xml'
            return self._serve_file(target, ctype)
        self.send_error(404)

    def do_POST(self):
        parsed = urlparse(self.path)
        if parsed.path == '/api/process-image':
            return self._handle_process_image()
        if parsed.path == '/api/generate-gcode':
            return self._handle_generate_gcode()
        if parsed.path == '/api/upload-to-esp':
            return self._handle_upload_to_esp()
        self.send_error(404)

    def _read_json(self):
        length = int(self.headers.get('Content-Length', '0'))
        raw = self.rfile.read(length) if length else b'{}'
        return json.loads(raw.decode('utf-8') or '{}')

    def _handle_process_image(self):
        ctype = self.headers.get('Content-Type', '')
        if 'multipart/form-data' not in ctype:
            return json_response(self, {"success": False, "error": "Expected multipart/form-data"}, 400)
        import cgi
        environ = {'REQUEST_METHOD': 'POST', 'CONTENT_TYPE': ctype}
        form = cgi.FieldStorage(fp=self.rfile, headers=self.headers, environ=environ)
        if 'image' not in form:
            return json_response(self, {"success": False, "error": "Missing image field"}, 400)
        file_item = form['image']
        image_bytes = file_item.file.read()
        filename = file_item.filename or 'upload.png'
        params = {}
        for key in ['preset','threshold_value','auto_threshold','invert','simplify_tolerance','min_path_length','min_point_distance_mm','min_area_px','morph_kernel','workspace_width_mm','workspace_height_mm','processing_width_px','retrieve_mode','blur_kernel']:
            if key in form:
                value = form.getfirst(key)
                if value in ('true','false'):
                    params[key] = value == 'true'
                else:
                    try:
                        if '.' in value:
                            params[key] = float(value)
                        else:
                            params[key] = int(value)
                    except Exception:
                        params[key] = value
        return json_response(self, handlers.handle_process_image(image_bytes, filename, params))

    def _handle_generate_gcode(self):
        payload = self._read_json()
        return json_response(self, handlers.handle_generate_gcode(payload))

    def _handle_upload_to_esp(self):
        payload = self._read_json()
        esp_url = payload.get('esp_url', '').rstrip('/')
        gcode = payload.get('gcode', [])
        auto_execute = bool(payload.get('auto_execute', False))
        if not esp_url or not gcode:
            return json_response(self, {"success": False, "error": "esp_url and gcode are required"}, 400)
        body = ('\n'.join(gcode)).encode('utf-8')
        try:
            req = urllib.request.Request(esp_url + '/upload-text', data=body, method='POST', headers={'Content-Type': 'text/plain'})
            with urllib.request.urlopen(req, timeout=15) as resp:
                upload_msg = resp.read().decode('utf-8', errors='replace')
            execute_msg = None
            if auto_execute:
                with urllib.request.urlopen(esp_url + '/execute', timeout=15) as resp:
                    execute_msg = resp.read().decode('utf-8', errors='replace')
            return json_response(self, {"success": True, "message": upload_msg, "execute_message": execute_msg})
        except Exception as exc:
            return json_response(self, {"success": False, "error": str(exc)}, 502)

    def _serve_file(self, path: Path, ctype: str):
        data = path.read_bytes()
        self.send_response(200)
        self.send_header('Content-Type', ctype)
        self.send_header('Content-Length', str(len(data)))
        self.end_headers()
        self.wfile.write(data)


if __name__ == '__main__':
    port = int(os.environ.get('PORT', '8080'))
    httpd = ThreadingHTTPServer(('0.0.0.0', port), App)
    print(f'Bridge server running on http://127.0.0.1:{port}')
    httpd.serve_forever()
