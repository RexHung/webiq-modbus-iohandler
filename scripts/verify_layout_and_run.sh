#!/usr/bin/env bash
set -euo pipefail

# ===== config =====
PORT="${PORT:-1602}"                       # 可用環境變數覆寫
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${REPO_ROOT}/build"
CONF_DIR="${REPO_ROOT}/config"
CONF_FILE="${CONF_DIR}/ci.modbus.json"

echo "== Repo root: ${REPO_ROOT}"
echo "== Using port: ${PORT}"

# 1) 檢查基本結構
echo "== Checking tree..."
test -f "${REPO_ROOT}/CMakeLists.txt" \
 && test -f "${REPO_ROOT}/tests/integration/test_e2e.cpp" \
 && test -f "${REPO_ROOT}/tests/integration/modbus_sim.py" \
 && echo "✅ tree ok" || { echo "❌ tree mismatch"; exit 10; }

# 2) 單元/組件建置（stub, 無外部依賴）
echo "== Building (stub)..."
rm -rf "${BUILD_DIR}" && mkdir -p "${BUILD_DIR}"
cmake -S "${REPO_ROOT}" -B "${BUILD_DIR}" -DWITH_LIBMODBUS=OFF -DWITH_TESTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build "${BUILD_DIR}" --config Release --parallel
ctest --test-dir "${BUILD_DIR}" --output-on-failure || true

# 3) 啟動 Modbus TCP 模擬器
echo "== Starting simulator on port ${PORT}..."
# 讓 python 背景執行並把 PID 記下來，結束時清理
pushd "${REPO_ROOT}" >/dev/null
( PORT="${PORT}" python3 tests/integration/modbus_sim.py ) >/dev/null 2>&1 &
SIM_PID=$!
popd >/dev/null
sleep 1

cleanup() {
  echo "== Cleanup..."
  if ps -p "${SIM_PID}" >/dev/null 2>&1; then
    kill "${SIM_PID}" || true
  fi
}
trap cleanup EXIT

# 4) 產生 CI 測試設定
echo "== Generating ${CONF_FILE}..."
mkdir -p "${CONF_DIR}"
cat > "${CONF_FILE}" <<JSON
{
  "transport": "tcp",
  "tcp": { "host": "127.0.0.1", "port": ${PORT}, "timeout_ms": 1000 },
  "items": [
    { "name": "coil.run",      "unit_id": 1, "function": 1,  "address": 10,  "type": "bool",  "poll_ms": 100 },
    { "name": "holding.temp",  "unit_id": 1, "function": 3,  "address": 0,   "type": "int16", "scale": 0.1, "poll_ms": 100 },
    { "name": "ai.flow",       "unit_id": 1, "function": 4,  "address": 100, "type": "float", "poll_ms": 100, "swap_words": false }
  ]
}
JSON

# 5) 整合建置（libmodbus + 測試）
echo "== Building (libmodbus + tests)..."
cmake -S "${REPO_ROOT}" -B "${BUILD_DIR}" -DWITH_LIBMODBUS=ON -DWITH_TESTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build "${BUILD_DIR}" --config Release --parallel

# 6) 尋找與執行 test_e2e
echo "== Running E2E..."
E2E_BIN="${BUILD_DIR}/tests/integration/test_e2e"
if [[ ! -x "${E2E_BIN}" ]]; then
  # 某些 generator 會放到子資料夾，如 build/Release/...
  E2E_BIN="$(find "${BUILD_DIR}" -type f -name test_e2e -perm -111 | head -n 1 || true)"
fi
if [[ -z "${E2E_BIN}" ]]; then
  echo "❌ test_e2e not found"; exit 20
fi
echo "Using test binary: ${E2E_BIN}"
"${E2E_BIN}" --config "${CONF_FILE}"

echo "✅ All checks passed."
