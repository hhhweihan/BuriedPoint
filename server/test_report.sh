#!/bin/bash
# 端到端集成测试：启动测试 server -> 运行示例客户端 -> 校验收发与去重
#
# 覆盖:
#   1. HTTP 上报（默认批量 10）
#   2. report_id 幂等去重（模拟客户端重试，服务端应拒绝重复）
#   3. HTTPS 上报（mbedtls TLS 客户端 + 自签名证书）
#
# 用法: ./test_report.sh [build_dir]
set -e
cd "$(dirname "$0")"
BUILD_DIR="${1:-../build}"
SERVER_LOG=$(mktemp)
SERVER_PID=""
trap 'if [ -n "$SERVER_PID" ]; then kill $SERVER_PID 2>/dev/null || true; fi; rm -f "$SERVER_LOG" || true' EXIT

start_server() {  # $1=port $2=mode
    python3 -u report_server.py "$1" "$2" "cert.pem" "key.pem" >"$SERVER_LOG" 2>&1 &
    SERVER_PID=$!
    sleep 0.5
}

check_server_log() {
    if grep -q "\[report\]" "$SERVER_LOG"; then
        echo "PASS: events received by server"
    else
        echo "FAIL: no events received"
        cat "$SERVER_LOG"
        exit 1
    fi
    if grep -q "\[error\]" "$SERVER_LOG"; then
        echo "FAIL: server reported errors"
        cat "$SERVER_LOG"
        exit 1
    fi
}

stop_server() {
    kill $SERVER_PID 2>/dev/null
    wait $SERVER_PID 2>/dev/null || true
    SERVER_PID=""
    rm -f "$SERVER_LOG"
}

[ -f cert.pem ] || ./gen_cert.sh

# ========== 1. HTTP 上报 + report_id 去重 ==========
echo "=== case 1: HTTP 上报 + 去重 (port 8080, batch=10) ==="
start_server 8080 http
"${BUILD_DIR}/examples/buried_example" http 8080 10 >/dev/null 2>&1
sleep 7   # 等待 5 秒上报周期触发
check_server_log

RID=$(grep -o "report_id=[a-zA-Z0-9]*" "$SERVER_LOG" | head -1 | cut -d= -f2)
echo "模拟客户端重试: 以相同 report_id=$RID 重发..."
curl -s -X POST http://127.0.0.1:8080/api/v1/report \
    -d "[{\"title\":\"dup_test\",\"user_id\":\"user_10086\",\"report_id\":\"$RID\"}]"
sleep 0.5
if grep -q "dup=[1-9]" "$SERVER_LOG"; then
    echo "PASS: 服务端按 report_id 幂等去重生效"
else
    echo "FAIL: 去重未生效"
    cat "$SERVER_LOG"
    exit 1
fi
stop_server

# ========== 2. HTTPS 上报 ==========
echo "=== case 2: HTTPS 上报 (port 8443, batch=5) ==="
start_server 8443 https
"${BUILD_DIR}/examples/buried_example" https 8443 5 >/dev/null 2>&1
sleep 7
check_server_log
stop_server

echo "=== all integration tests passed ==="
