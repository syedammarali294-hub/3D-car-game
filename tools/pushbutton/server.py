"""Tiny localhost server: one page, one button, one job -- POST /push runs
`git push` in this repo and streams the result back. Binds to 127.0.0.1
only (never 0.0.0.0), so it's not reachable from outside this machine.

Run:  python server.py [port]   (default port 5959)
"""
import http.server
import json
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]  # tools/pushbutton/ -> tools/ -> repo root
INDEX_HTML = Path(__file__).with_name("index.html")


class Handler(http.server.BaseHTTPRequestHandler):
    def _send_json(self, status, payload):
        body = json.dumps(payload).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        if self.path == "/" or self.path == "/index.html":
            body = INDEX_HTML.read_bytes()
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
        else:
            self.send_error(404)

    def do_POST(self):
        if self.path != "/push":
            self.send_error(404)
            return
        try:
            result = subprocess.run(
                ["git", "push"],
                cwd=str(REPO_ROOT),
                capture_output=True,
                text=True,
                timeout=120,
            )
            ok = result.returncode == 0
            output = (result.stdout or "") + (result.stderr or "")
            self._send_json(200, {"ok": ok, "output": output.strip()})
        except Exception as e:  # noqa: BLE001 -- always report back to the page, never 500 silently
            self._send_json(200, {"ok": False, "output": f"Server error: {e}"})

    def log_message(self, format, *args):  # noqa: A002 -- quiet the default stderr access log spam
        pass


def main():
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 5959
    server = http.server.HTTPServer(("127.0.0.1", port), Handler)
    print(f"Push button running at http://127.0.0.1:{port}  (repo: {REPO_ROOT})")
    server.serve_forever()


if __name__ == "__main__":
    main()
