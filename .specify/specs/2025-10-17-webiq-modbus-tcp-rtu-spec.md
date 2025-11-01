# Feature Specification: WebIQ ioHandler Modbus TCP/RTU 擴充模組

**Feature Branch**: `002-webiq-modbus-sdk`
**Created**: 2025-10-17
**Status**: Draft
**Input**: User description: "/speckit.specify 為 WebIQ ioHandler SDK 開發一個支援 Modbus TCP/RTU 的擴充模組。請根據目前 repo 內容 (src/, include/, doc/sdk/) 重新產生功能規格，並明確定義：1. API: CreateIoInstance / DestroyIoInstance / SubscribeItems / UnsubscribeItems / ReadItem / WriteItem / CallMethod 2. JSON 組態結構與範例 (transport, items, scale, offset, poll_ms, swap_words)  3. 輪詢與變更推送策略（每項只在值變更時上報） 4. 錯誤模型與重試策略 5. 測試與驗證需求 (unit, integration, CI) 6. 請引用 /doc/sdk/ioHandlerSDK/include/*.h 及 /doc/sdk/notes/* 內容作為參考。"

## User Scenarios & Testing (mandatory)

### User Story 1 - 以固定 C API 完成 Modbus TCP 讀寫 (Priority: P1)

以 JSON 組態啟動 Handler，透過 Create/Destroy/Subscribe/Unsubscribe/Read/Write/CallMethod 完成 TCP 目標的讀寫。

- Why this priority: 最小可用能力，對接 WebIQ Connect 基礎。
- Independent Test: `tests/integration/modbus_sim.py` 啟動後，`test_e2e` 以 `config/ci.modbus.json` 成功讀寫 `coil.run`/`holding.temp`/`ai.flow`。

Acceptance Scenarios:
1. Given 模擬器在 127.0.0.1:1502, When Create→Subscribe→Read, Then 無錯誤且數值符合預期。
2. When Write `coil.run=true`, Then 後續 Read 回 true。

---

### User Story 2 - 支援 RTU 參數與逾時 (Priority: P2)

以相同 API 對串列埠 RTU 目標進行輪詢/讀寫，具備 baud/parity/timeout 設定與錯誤回報。

- Independent Test: 在具硬體 Runner 或本機（可選），驗證連線與逾時。

---

### User Story 3 - 僅變更推送與可診斷性 (Priority: P2)

只在值變更時上報；可設定日誌輸出與層級，錯誤具備代碼與訊息，並支援重試/backoff。

- Independent Test: 模擬短線/超時，驗證重試與日誌輸出。

## Requirements (mandatory)

### 1) C API 定義

參考 WebIQ 官方 ioHandler C API（`doc/sdk/ioHandlerSDK/include/ioHandler.h`）與變體值格式（`doc/sdk/ioHandlerSDK/include/variant_value.h`），本模組對外暴露下列 C 連結介面：

```
extern "C" {
  using IoHandle = void*;
  IoHandle CreateIoInstance(void* user_param, const char* jsonConfigPath);
  void     DestroyIoInstance(IoHandle h);
  int      SubscribeItems(IoHandle h, const char** names, int count);
  int      UnsubscribeItems(IoHandle h, const char** names, int count);
  int      ReadItem(IoHandle h, const char* name, /*out*/char* outJson, int outSize);
  int      WriteItem(IoHandle h, const char* name, const char* valueJson);
  int      CallMethod(IoHandle h, const char* method, const char* paramsJson,
                      /*out*/char* outJson, int outSize);
}
```

- 參照文件：`doc/sdk/ioHandlerSDK/include/ioHandler.h`, `doc/sdk/notes/sdk_ref_example.md`, `doc/sdk/notes/sdk_ref_variant.md`
- 回傳慣例：0=成功；負值為錯誤碼（見「錯誤模型」）。`outJson` 採 UTF-8。
- `CallMethod` 必須至少支援：
  - `"logger.set"`：`{"level":"debug|info|warn|error","sink":"stdout|stderr|none"}`
  - `"connection.reconnect"`：強制重連，回傳 `{ "ok": true }`
  - `"diagnostics.reset"`：清除累計計數，回 `{ "reset": true }`
  - `"diagnostics.snapshot"`：回傳最近計數與例外列表
  - 可擴充 `"item.reload"`：重載組態，回 `{ "reloaded": N }`

### 2) JSON 組態結構與範例

- 根節點
  - `transport`: `"tcp" | "rtu" | "ascii"`
  - `tcp`: `{ "host": string, "port": int, "timeout_ms": int }`
  - `rtu`: `{ "port": string, "baud": int, "parity": "N|E|O", "data_bits": 7|8, "stop_bits": 1|2, "timeout_ms": int, "frame_silence_ms"?: int }`
  - `ascii`: `{ "port": string, "baud": int, "parity": "N|E|O", "data_bits": 7|8, "stop_bits": 1|2, "timeout_ms": int, "lrc_check": bool, "frame_silence_ms"?: int }`
  - `items`: `Item[]`
- Item 結構
  - `name`: string（唯一鍵）
  - `unit_id`: int
  - `function`: 1|2|3|4|5|6|8|15|16
  - `address`: int
  - `count`?: int（FC15/16 或 32-bit/float 需 2）
  - `type`: `bool|int16|uint16|int32|uint32|float|double`
  - `scale`?: number（預設 1.0）
  - `offset`?: number（預設 0.0）
  - `swap_words`?: bool（32-bit/float word order）
  - `poll_ms`?: int（輪詢間隔；0/缺省代表不輪詢，只支援按需讀）
  - `broadcast_allowed`?: bool（預設 false，僅在 `unit_id=0` 時允許寫入）

範例：
```
{
  "transport": "ascii",
  "ascii": { "port": "/dev/ttyUSB0", "baud": 9600, "parity": "E", "data_bits": 7, "stop_bits": 1, "timeout_ms": 1500, "lrc_check": true },
  "items": [
    { "name": "coil.run",     "unit_id": 1, "function": 1,  "address": 10,  "type": "bool",  "poll_ms": 100 },
    { "name": "holding.temp", "unit_id": 1, "function": 3,  "address": 0,   "type": "int16", "scale": 0.1, "offset": 0.0, "poll_ms": 200 },
    { "name": "ai.flow",      "unit_id": 1, "function": 4,  "address": 100, "count": 2,     "type": "float", "swap_words": false, "poll_ms": 250 },
    { "name": "diagnostics",  "unit_id": 1, "function": 8,  "address": 0,   "type": "uint16", "poll_ms": 1000 }
  ]
}
```
- 編碼/解碼與縮放：參考 `doc/sdk/notes/sdk_ref_variant.md` 中 join/split 與 apply_scale/unscale 範例。

### 3) 輪詢與變更推送策略

- 輪詢器：為每個具 `poll_ms>0` 的 item 啟動工作（可歸併相同 unit/fc 區段以減少往返），TCP/RTU/ASCII 共用。
- 變更偵測：以序列化後的原始 bytes 比對（bool→1B；16-bit→2B；32-bit/float→4B，已處理 word order/LRC），不同才上報。
- 上報：透過 WebIQ 的讀回調（對應官方 `tReadCallback` 概念，見 `doc/sdk/ioHandlerSDK/include/ioHandler.h`）或在本 C API 中以 `ReadItem` 查詢與使用者側 callback 機制配合。
- 背壓：同一 item 在上一筆尚未處理完成時避免重入；必要時丟棄中間重覆值。
- 傳輸細節：RTU/ASCII 須尊重 `frame_silence_ms` 設定以符合 t1.5/t3.5 要求，ASCII frame 含 `":"` 起始、`"\r\n"` 結尾與 LRC 兩字節校驗。
- 最小間隔：上報與重讀需尊重 `poll_ms` 與通訊逾時，避免壓力碰撞。

### 4) 錯誤模型與重試策略

- 返回碼（負整數）：
  - `-1 INVALID_ARG`（空指標/長度不符/JSON 不合法）
  - `-2 NOT_FOUND`（未知 item 名稱）
  - `-3 IO_TIMEOUT`（連線/請求逾時）
  - `-4 IO_ERROR`（I/O錯誤/CRC/LRC/Frame）
  - `-5 NOT_CONNECTED`（尚未建立/已失去連線）
  - `-6 UNSUPPORTED`（不支援功能碼/型別）
  - `-7 PARSE_ERROR`（數值/型別轉換失敗；scale=0 等）
- JSON 錯誤回傳格式（僅在 `outJson` 場景需要）：`{"error":{"code":-3,"message":"timeout"}}`
- Modbus Exception Response：遇到 Slave 回覆 exception 時，`ReadItem/WriteItem` 需將 `result` 設為 `{"error":{"code":-32X,"exception":"ILLEGAL_DATA_ADDRESS"}}` 並保留原負值返回碼；`CallMethod("diagnostics.snapshot")` 可查詢最近 50 筆 exception。
- 重試策略：
  - TCP：指數退避（初始 100ms，最大 3s，Jitter），連線/請求皆適用；最大重試次數後回錯。
  - RTU/ASCII：固定步進退避（200ms*n），含開啟埠與請求；遇 `IO_ERROR` 或 LRC/CRC 錯誤時可增量延遲。
  - 寫入：避免無限重試；預設嘗試 3 次並回報。
- Frame 驗證：RTU 需檢查 CRC16；ASCII 需檢查 LRC 與 `":"`/`"\r\n"` 封裝，並遵守 `frame_silence_ms` (預設 5ms，對應 t3.5)。
- 日誌：以 `CallMethod("logger.set", ...)` 控制層級；錯誤包含上下文（item、fc、addr、unit、嘗試次數、exception code）。參考 `doc/sdk/ioHandlerSDK/include/ioHandler.h` 的日誌嚴重度定義。

### 5) Modbus 診斷與廣播

- Diagnostic Counter (FC08)：支援讀取 `return_query_data`、`retries`, `crc_errors`, `lrc_errors`, `timeouts`, `broadcasts_sent` 等欄位；結果以 JSON 回傳，例如 `{ "counter": { "crc_errors": 2, "timeouts": 1 } }`。
- 廣播行為：允許 `unit_id=0` 搭配 FC05/FC06/FC15/FC16 寫入；必須在 item 層設定 `broadcast_allowed=true`，否則拒絕。
- `CallMethod("diagnostics.reset", {})` 用於重置計數器，回傳 `{ "reset": true }`。
- 安全限制：廣播寫入僅適用於未訂閱的更新，避免覆蓋需要 ACK 的關鍵資料；需在組態檔註記且於文件說明。

### 6) IModbusClient 抽象（功能碼）

- 介面位於 `include/IModbusClient.hpp`（將擴充）；至少提供：
  - 連線：`connect() / close() / set_timeout_ms(int) / set_frame_silence(std::chrono::milliseconds)`
  - 讀：FC1/2 → `read_coils`/`read_discrete_inputs`；FC3/4 → `read_holding_regs`/`read_input_regs`
  - 寫：FC5/6 → `write_single_coil`/`write_single_reg`；FC15/16 → `write_multiple_coils`/`write_multiple_regs`
  - 診斷：`read_diagnostics(DiagnosticField)` 支援 FC08
  - 廣播：`write_broadcast(FunctionCode, payload)` 封裝 unit_id=0。
- 實作：
  - `WITH_LIBMODBUS=ON`：使用系統 libmodbus；ASCII 模式須提供額外 `AsciiModbusClient`
  - `WITH_LIBMODBUS=OFF`：stub 保證單元測試通過（可模擬超時/錯誤）並支援 CRC/LRC 模擬

### 7) CMake 選項與目標

- 選項：
  - `-DWITH_LIBMODBUS=ON|OFF`（預設 OFF）
  - `-DWITH_TESTS=ON|OFF`（與 `BUILD_TESTING` 綁定）
- 目標：
  - `ioh_modbus` 共享庫（見 `CMakeLists.txt`）
  - `test_e2e` 整合測試執行檔；CTest 名稱 `e2e`
- 平台：C++14+；Windows/Linux 皆可建置；參考 `doc/sdk/README.md` 與 `doc/sdk/sdk_ref.md`。

## Success Criteria (mandatory)

- `test_e2e` 在 Linux/Windows 與 PORT ∈ {1502,1503} 綠燈；退出碼 0；ASCII 模式以 `PORT=1502`、`transport=ascii` 驗證。
- 單元測試全綠；`WITH_LIBMODBUS=OFF` 仍可通過，並涵蓋 CRC/LRC 驗證與 exception mapping。
- CI（`.github/workflows/CI.yml`）矩陣成功（unit: 排除 `e2e`；integration: `-R e2e`），新增 ASCII channel。
- README/tests/SDK 參考說明可依步驟重現 TCP/RTU/ASCII 三種模式。

## Assumptions

- 部署環境可提供 TCP、RTU、ASCII 其中一種連線通道；ASCII 需支援 7E1 或 7O1。
- 使用者保證廣播指令僅用於可無回覆確認的設備；必要時自行監控。
- 診斷統計僅追蹤最近一次啟動後的值，如需持久化需上層處理。

## References

- SDK C API：`doc/sdk/ioHandlerSDK/include/ioHandler.h`
- Variant 型別與轉換：`doc/sdk/ioHandlerSDK/include/variant_value.h`
- 變體處理與範例：`doc/sdk/notes/sdk_ref_variant.md`, `doc/sdk/notes/sdk_ref_example.md`
- 既有程式骨架：`include/IModbusClient.hpp`, `include/ModbusIoHandler.hpp`, `src/ModbusIoHandler.cpp`
