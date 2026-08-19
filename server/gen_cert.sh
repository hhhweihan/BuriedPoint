#!/bin/sh
# 生成 HTTPS 测试用的自签名证书（client 使用 verify_none 模式，证书无需受信任）
set -e
cd "$(dirname "$0")"
openssl req -x509 -newkey rsa:2048 -nodes \
    -keyout key.pem -out cert.pem -days 365 \
    -subj "/CN=localhost" >/dev/null 2>&1
echo "certificate generated: server/cert.pem server/key.pem"
