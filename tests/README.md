# Tests

本專案將測試分兩層，以減少手動在 WebIQ Runtime + 實機上的測試負擔：

* **Layer 1 — Unit / Component（無外部依賴）**
  目標：驗證編解碼（`int16/uint16/int32/uint32/float`）、`scale/offset`、`swap_words`、以及 C API 的錯誤碼行為。
  作法：`WITH_LIBMODBUS=OFF`（stub 後端），僅做純邏輯測試與編譯健檢（可用 CTest）。
* **Layer 2 — Integration（外部模擬器）**
  目標：在 CI 或本機啟動 **Modbus TCP 模擬器**（使用 `pymodbus`），改以 `WITH_LIBMODBUS=ON` 編譯，跑真正的 FC 讀寫（1/3/4/5/6/16），確保連線、縮放與字組處理無誤。
  作法：在 `tests/integration/` 內提供：

  * `modbus_sim.py`：啟動 TCP 模擬器（`127.0.0.1:1502`）
  * `test_e2e.cpp`：以**本專案 C API** 進行 E2E 測試

> 最終端對端（E2E）在 WebIQ Runtime 上的測試保留為**發版前驗收**；一般開發循環只需上述兩層自動化。

---

## 目錄結構

```
tests/
├─ README.md                   # 本文件
├─ integration/
│  ├─ modbus_sim.py            # Modbus TCP 模擬器（pymodbus）
│  └─ test_e2e.cpp             # 以 C API 驗證 FC1/3/4/5/6/16
```

> 提醒：請在 `CMakeLists.txt` 末尾加入 E2E 測試可執行檔（見下方 §4）。

---

## 1) 先治條件

### Linux / macOS

* CMake 3.14+
* C++14 編譯器
* Python 3.8+（for integration）
* `pymodbus`（for integration）
* `libmodbus`（for integration；macOS 可用 Homebrew）

安裝參考（Ubuntu）：

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake python3 python3-pip libmodbus-dev
python3 -m pip install --upgrade pip
python3 -m pip install pymodbus
```

（macOS）

```bash
brew install cmake libmodbus python
pip3 install pymodbus
```

### Windows（PowerShell）

* Visual Studio 2019/2022（含 C++ 工作貨）或 MSVC Build Tools
* CMake 3.14+
* Python 3.8+（for integration）
* libmodbus for Windows（可自建或使用預編譯二進位；若暫缺，可先跑 Layer 1）

---

## 2) Layer 1 — Unit / Component（Stub backend）

> 目的：確保專案可編譯、公共 API 正常、基礎邏輯無誤；**無需 libmodbus / 無需模擬器**。

### Linux / macOS

```bash
mkdir -p build && cd build
cmake -DWITH_LIBMODBUS=OFF -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --config Release --parallel
# （若已加入 CTest 的單元測試）
ctest --output-on-failure
```

### Windows（PowerShell）

```powershell
mkdir build; cd build
cmake -G "Visual Studio 17 2022" -DWITH_LIBMODBUS=OFF -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --config Release
# （若已加入 CTest）
ctest --output-on-failure
```

**預期結果**

* 成功產生 `ioh_modbus.(dll|so|dylib)`
* CTest 全綠（若尚未加入單元測試，這步可先只做編譯驗證）

---

## 3) Layer 2 — Integration（libmodbus + 模擬器）

> 目的：在「無實機」情況下，驗證真實 TCP 連線與功能碼行為；CI 亦可執行。

### 步驟 A：啟動 Modbus TCP 模擬器

```bash
# 於專案根目錄
python3 tests/integration/modbus_sim.py &
# 預設監聽 127.0.0.1:1502

# 也可以環境變數覆寫 PORT（便於並行測試）
# Linux/macOS:
PORT=1503 python3 tests/integration/modbus_sim.py &
# Windows (PowerShell):
$env:PORT=1503; python tests/integration/modbus_sim.py
```

模擬器預設資料（建議值，可在 `modbus_sim.py` 調整）：

* `coils[10] = 0`（供 FC1/5 測寫讀）
* `holding[0] = 123`（供 FC3 測讀＋scale 0.1 ⇒ 12.3）
* `input[100..101]` = IEEE754 `float32(1.5)`（供 FC4 測讀）

### 步驟 B：準備測試用 JSON

```bash
cat > config/ci.modbus.json <<'JSON'
{
  "transport": "tcp",
  "tcp": { "host": "127.0.0.1", "port": 1502, "timeout_ms": 1000 },
  "items": [
    { "name": "coil.run",      "unit_id": 1, "function": 1,  "address": 10,  "type": "bool",  "poll_ms": 100 },
    { "name": "holding.temp",  "unit_id": 1, "function": 3,  "address": 0,   "type": "int16", "scale": 0.1, "poll_ms": 100 },
    { "name": "ai.flow",       "unit_id": 1, "function": 4,  "address": 100, "type": "float", "poll_ms": 100, "swap_words": false }
  ]
}
JSON
```

### 步驟 C：以 `WITH_LIBMODBUS=ON` 編譯並跑 E2E

```bash
mkdir -p build && cd build
cmake -DWITH_LIBMODBUS=ON -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --config Release --parallel
./tests/integration/test_e2e --config ../config/ci.modbus.json
```

**預期輸出（範例）**

```
coil.run=false
coil.run(after)=true
holding.temp=12.300000
ai.flow=1.500000
```

> 若看到連線失敗，請先確認 `modbus_sim.py` 有啟動、port 1502 未被佔用、以及 `libmodbus` 已正確安裝。

---

## 4) CMake 設定（加入 E2E 測試可執行檔）

請在專案根的 `CMakeLists.txt` 末尾加入（若尚未加入）：

```cmake
# ----- Integration Test (E2E) -----
add_executable(test_e2e tests/integration/test_e2e.cpp)
target_include_directories(test_e2e PRIVATE include)
target_link_libraries(test_e2e PRIVATE ioh_modbus)
add_test(NAME e2e COMMAND test_e2e --config ${CMAKE_SOURCE_DIR}/config/ci.modbus.json)
```

---

## 5) GitHub Actions（CI）建議

新增 `.github/workflows/ci.yml`：

* **Job 1**：`WITH_LIBMODBUS=OFF` 單元/編譯檢查
* **Job 2**：安裝 `libmodbus-dev` + `pymodbus`，啟動 `modbus_sim.py`，以 `WITH_LIBMODBUS=ON` 跑 `test_e2e`

（YAML 例在專案 `README.md` 或 `.github/workflows/ci.yml` 中，請依照你的實際 CI 檔案為準）

---

## 6) 常見問題（Troubleshooting）

* **`Read coil.run failed rc=...`**
  確認 `modbus_sim.py` 已啟動、`port 1502` 未被佔用；或調整 `ci.modbus.json` 使用其他 port。
* **`ai.flow` 值為 0 或不正確**
  請檢查 `swap_words` 與你的模擬器/設備字組順序是否一致（32-bit/float 的高/低字組在不同設備可能相反）。
* **Windows 缺少 libmodbus**
  若尚未安裝 libmodbus，可先只跑 Layer 1；或把 Runner 換成 Linux 做整合測試。
* **位址 0-based vs 40001**
  JSON 中 `address` 為 0-based；若設備手冊以 40001/30001 類型的人類位址，請自行減一後填入。

---

## 7) 最終端對端（WebIQ Runtime）驗收（選擇性）
自動化兩層測試通過後，再於發版前在 WebIQ Runtime 上做一次端到端驗收（安裝 `.dll/.so`、載入 JSON、建立對應變數、驗證「變更才推送」與寫入回報）。
此步驟不進 CI，只保留為產品驗收。

## 8) 開發者快速清單
* [X] WITH_LIBMODBUS=OFF：能編譯、CTests 皆綠
* [X] 啟動 modbus_sim.py 成功
* [X] WITH_LIBMODBUS=ON：test_e2e 讀寫皆正確
* [X] CI（unit + integration）綠燈
* [ ]（發版前）WebIQ Runtime E2E OK

---

> 註：若你要加上 RTU 整合測試，可另行在 Runner 上接 USB-RS485（或使用虛擬串口環境）並加一個 `modbus_rtu_sim` 腳本與對應 `ci.modbus.rtu.json`；流程與 TCP 類似，但因硬體依賴，通常僅在本機或專用硬體 Runner 上跑。

---

## 9) RTU 測試與 Stub 策略（建議）

- 在 `WITH_LIBMODBUS=OFF` 或 CI 缺少硬體時，RTU 相關單元測試以 Stub 實作模擬：
  - 以記憶體模型代表寄存器/線圈；模擬超時/CRC/Frame 錯誤；
  - 驗證功能碼 FC1/2/3/4/5/6/15/16 的引數檢查與資料編解碼（含 `scale/offset/swap_words`）。
- 若需在 CI 驗證 RTU，可考慮：
  - Linux 使用 `socat` 建虛擬串口配對，或跑容器化虛擬 RTU 模擬器；
  - Windows 使用 com0com 建虛擬 COM；
  - 提供 `tests/integration/modbus_rtu_sim.py` 與 `config/ci.modbus.rtu.json`（非必須）。
