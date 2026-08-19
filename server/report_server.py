#!/usr/bin/env python3
"""BuriedPoint 埋点上报测试服务端。

支持 HTTP 和 HTTPS，按 report_id 幂等去重。

用法:
    python3 report_server.py [port] [http|https] [cert] [key]

    port   监听端口，默认 8080
    mode   http（默认）或 https
    cert   HTTPS 证书文件（默认 server/cert.pem）
    key    HTTPS 私钥文件（默认 server/key.pem）

示例:
    python3 report_server.py 8080 http
    python3 report_server.py 8443 https server/cert.pem server/key.pem
"""

import json
import ssl
import sys
from http.server import BaseHTTPRequestHandler, HTTPServer


class ReportHandler(BaseHTTPRequestHandler):
    seen_report_ids = set()
    total_new = 0
    total_dup = 0

    def do_POST(self):
        length = int(self.headers.get("Content-Length", 0))
        body = self.rfile.read(length)

        if self.path != "/api/v1/report":
            self.send_response(404)
            self.end_headers()
            return

        new_count = 0
        dup_count = 0
        try:
            items = json.loads(body)
            for item in items:
                if isinstance(item, str):
                    obj = json.loads(item)
                else:
                    obj = item
                rid = str(obj.get("report_id", ""))
                if rid and rid in ReportHandler.seen_report_ids:
                    dup_count += 1
                else:
                    if rid:
                        ReportHandler.seen_report_ids.add(rid)
                    new_count += 1
                    print(
                        f"[report] title={obj.get('title')} "
                        f"user={obj.get('user_id')} "
                        f"app={obj.get('app_name')} "
                        f"priority={obj.get('priority')} "
                        f"report_id={rid}"
                    )
        except Exception as exc:
            print(f"[error] parse failed: {exc}", file=sys.stderr)
            self.send_response(400)
            self.end_headers()
            return

        ReportHandler.total_new += new_count
        ReportHandler.total_dup += dup_count
        print(
            f"[summary] new={new_count} dup={dup_count} "
            f"total_new={ReportHandler.total_new} "
            f"total_dup={ReportHandler.total_dup} "
            f"unique_ids={len(ReportHandler.seen_report_ids)}"
        )

        self.send_response(200)
        self.end_headers()

    def log_message(self, fmt, *args):
        pass


def main():
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8080
    mode = sys.argv[2] if len(sys.argv) > 2 else "http"
    cert = sys.argv[3] if len(sys.argv) > 3 else "server/cert.pem"
    key = sys.argv[4] if len(sys.argv) > 4 else "server/key.pem"

    httpd = HTTPServer(("127.0.0.1", port), ReportHandler)
    if mode == "https":
        ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        ctx.load_cert_chain(cert, key)
        httpd.socket = ctx.wrap_socket(httpd.socket, server_side=True)
        print(f"HTTPS report server listening on 127.0.0.1:{port}")
    else:
        print(f"HTTP report server listening on 127.0.0.1:{port}")
    print("POST /api/v1/report, dedup by report_id, Ctrl+C to stop")
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\nserver stopped")


if __name__ == "__main__":
    main()
