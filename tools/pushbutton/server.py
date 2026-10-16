"""Tiny localhost server: one page, one button, one job -- POST /push stages
everything, commits it (skipped if nothing changed), and pushes. Binds to
127.0.0.1 only (never 0.0.0.0), so it's not reachable from outside this
machine.

Run:  python server.py [port]   (default port 5959)
"""
import datetime
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

    def _run(self, args, timeout=120):
        result = subprocess.run(
            args, cwd=str(REPO_ROOT), capture_output=True, text=True, timeout=timeout
        )
        out = (result.stdout or "") + (result.stderr or "")
        return result.returncode, out.strip()

    def do_POST(self):
        if self.path != "/push":
            self.send_error(404)
            return
        try:
            log = []

            code, out = self._run(["git", "add", "-A"])
            log.append("$ git add -A\n" + out)
            if code != 0:
                self._send_json(200, {"ok": False, "output": "\n\n".join(log)})
                return

            # Nothing staged -- `git diff --cached --quiet` exits 1 if there
            # IS a diff, 0 if there isn't, so skip the commit (and its
            # otherwise-guaranteed failure) rather than creating empty ones.
            code, _ = self._run(["git", "diff", "--cached", "--quiet"])
            if code != 0:
                stamp = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
                code, out = self._run(["git", "commit", "-m", f"Update {stamp}"])
                log.append("$ git commit\n" + out)
                if code != 0:
                    self._send_json(200, {"ok": False, "output": "\n\n".join(log)})
                    return
            else:
                log.append("(nothing to commit)")

            code, out = self._run(["git", "push"])
            log.append("$ git push\n" + out)
            self._send_json(200, {"ok": code == 0, "output": "\n\n".join(log)})
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
