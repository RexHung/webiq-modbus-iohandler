# Feature Specification: WebIQ ioHandler Modbus SDK 擴充模組

**Feature Branch**: `001-modbus-sdk`  
**Created**: 2025-10-17  
**Status**: Draft  
**Input**: User description: "/speckit.specify 使用 WebIQ ioHandler SDK 開發支援 Modbus TCP/RTU 的擴充模組，明確定義 API（CreateIoInstance/DestroyIoInstance/SubscribeItems/UnsubscribeItems/ReadItem/WriteItem/CallMethod）、錯誤模型、日誌界面、JSON 組態（TCP/RTU 與 items mapping）、IModbusClient 抽象（FC1/2/3/4/5/6/15/16）、CMake 選項（WITH_LIBMODBUS、WITH_TESTS↔BUILD_TESTING）、CI 與測試策略。"

## User Scenarios & Testing (mandatory)

### User Story 1 - 穩定 C API 與 JSON 組態 (Priority: P1)

作為 SDK 使用者，我可以透過固定 C API（Create/Destroy/Subscribe/Unsubscribe/Read/Write/CallMethod）與 JSON 組態在 Windows/Linux 連線至 Modbus TCP 伺服器，讀寫 coils/holding/input registers。

- Why this priority: 為最小可用能力與向後相容的介面基石。
- Independent Test: 使用 `tests/integration/modbus_sim.py` 啟動 TCP 模擬器，`test_e2e` 以 `ci.modbus.json` 成功讀寫三個項目並結束碼 0。

Acceptance Scenarios:
1. Given 模擬器於 127.0.0.1:1502，When Create→Subscribe→Read `coil.run`，Then 成功回傳布林字串且不崩潰。
2. Given 已訂閱 `holding.temp`，When Read，Then 取得縮放後數值（預設 12.3）。

---

### User Story 2 - RTU 連線與序列埠參數 (Priority: P2)

作為 SDK 使用者，我可以提供 RTU 組態（COM/tty、baud、parity、data/stop bits、timeout）並透過同一套 API 操作。

- Why this priority: 常見現場需求，但相依硬體，次於 TCP。
- Independent Test: 在具硬體 Runner 或本機，使用迴路/模擬器測試；若 CI 缺硬體則標記為可選，仍需本地可重現。

Acceptance Scenarios:
1. Given 正確 RTU 組態，When Subscribe→Read，Then 成功取得資料且逾時與錯誤可觀測。

---

### User Story 3 - 錯誤模型與日誌 (Priority: P2)

作為維運/開發者，我希望 API 以一致錯誤碼/訊息回報，並能注入日誌 callback 以追蹤協議/重連狀態。

- Why this priority: 可診斷性直接影響交付與維護效率。
- Independent Test: 使連線失敗/超時，驗證錯誤碼、訊息、日誌輸出符合規格。

Acceptance Scenarios:
1. Given 錯誤主機，When CreateIoInstance，Then 以定義的錯誤碼/訊息回傳並寫入日誌。

---

### User Story 4 - CI 與測試矩陣 (Priority: P1)

作為專案維護者，我需要單元（CTests）與整合（pymodbus）在 Windows/Linux 與多個 PORT 並行通過，且 libmodbus 為可選相依。

- Why this priority: 憲章要求，保證跨平台品質。
- Independent Test: GitHub Actions matrix 成功；單元排除 E2E；整合以 `-R e2e` 跑。

Acceptance Scenarios:
1. Given CI matrix os∈{ubuntu,windows}, port∈{1502,1503}，When workflow 觸發，Then 所有工作綠燈。

### Edge Cases

- TCP 連線逾時、RST、DNS 解析失敗。
- RTU 序列埠不存在、權限不足、CRC/Frame error、baud/parity 不相容。
- 型別/縮放：int16/uint16/int32/uint32/float/double；大小端與 32-bit word swap。
- 批次寫入長度超限（FC15/16），地址越界，unit_id 不存在。
- 多執行緒併發 Read/Write 與重入安全。
- 斷線自動重連與背壓；讀取間隔（poll_ms）小於處理能力。
- JSON 無效或缺必要欄位；未知 item 名稱。

## Requirements (mandatory)

### Functional Requirements

- FR-001: 提供下列 C API（C linkage）：
  - `IoHandle CreateIoInstance(void* user_param, const char* jsonConfigPath);`
  - `void DestroyIoInstance(IoHandle h);`
  - `int SubscribeItems(IoHandle h, const char** names, int count);`
  - `int UnsubscribeItems(IoHandle h, const char** names, int count);`
  - `int ReadItem(IoHandle h, const char* name, char* outJson, int outSize);`
  - `int WriteItem(IoHandle h, const char* name, const char* valueJson);`
  - `int CallMethod(IoHandle h, const char* method, const char* paramsJson, char* outJson, int outSize);`
- FR-002: 錯誤模型：所有函式回傳 0=成功；負值為錯誤碼（例如 `-1 invalid_arg`, `-2 not_found`, `-3 io_timeout`, `-4 io_error`, `-5 not_connected`, `-6 unsupported`, `-7 parse_error`）。`outJson` 若包含錯誤需提供 `{ "error": { "code": <int>, "message": "..." } }`。
- FR-003: 日誌界面：透過 `CallMethod("logger.set", {"level":"info|debug|warn|error", "sink":"stdout|stderr|none" | {"type":"callback"}})` 設定；若為 callback，於 `CreateIoInstance(user_param=...)` 傳入結構體指標，SDK 取用其中函式指標。
- FR-004: JSON 組態模式（最小）：
  - `transport`: `"tcp" | "rtu"`
  - `tcp`: `{ "host": string, "port": int, "timeout_ms": int }`
  - `rtu`: `{ "port": string, "baud": int, "parity": "N|E|O", "data_bits": 7|8, "stop_bits": 1|2, "timeout_ms": int }`
  - `items`: `[{ "name": string, "unit_id": int, "function": 1|2|3|4|5|6|15|16, "address": int, "count"?: int, "type": "bool|int16|uint16|int32|uint32|float|double", "scale"?: number, "swap_words"?: bool, "poll_ms"?: int }]`
- FR-005: `ReadItem` 以 JSON 字串回傳值，例如：`true`、`123`、`12.3` 或 `{ "array": [ ... ] }`，依型別決定；`WriteItem` 以 JSON 字串提供目標值。
- FR-006: `SubscribeItems`/`UnsubscribeItems` 應為冪等；重複訂閱相同名稱不應失敗。
- FR-007: `IModbusClient` 抽象須涵蓋功能碼：FC1/2/3/4/5/6/15/16；提供同步 API 與逾時設定；TCP 與 RTU 兩個具體實作（後端可由 libmodbus 或 stub）。
- FR-008: CMake 選項：`WITH_LIBMODBUS`（預設 OFF）；`WITH_TESTS` 影響 `BUILD_TESTING`（兩者綁定）；在 `WITH_LIBMODBUS=OFF` 時以 stub 仍需通過所有單元測試。
- FR-009: 執行緒安全：公開 C API 對多執行緒呼叫需明確規範（讀取/寫入序列化，或提供鎖保護）。
- FR-010: 可觀測性：錯誤訊息具備上下文（函式、項目名、位址、fc）；必要時輸出到 stderr 或 logger。

### Key Entities

- TransportConfig: `TcpConfig | RtuConfig`
- ItemMapping: 名稱→(unit_id, function, address, count?, type, scale?, swap_words?)
- IModbusClient: 介面層定義；實作 `TcpModbusClient`、`RtuModbusClient`；在 `WITH_LIBMODBUS=OFF` 提供 `StubModbusClient`。

## Success Criteria (mandatory)

### Measurable Outcomes

- SC-001: `test_e2e` 在 Linux/Windows、PORT ∈ {1502,1503} 全通過；退出碼 0。
- SC-002: 單元測試全綠；在 `WITH_LIBMODBUS=OFF` 時可編譯並通過所有單元測試。
- SC-003: CI matrix 成功（unit: 2×2；integration: 2×2）。
- SC-004: API 穩定，`README.md`/`tests/README.md` 包含 JSON 組態與使用範例；變更附最小重現測試與 git diff 片段。

## Proposed API Details

- Error Codes: `{ INVALID_ARG=-1, NOT_FOUND=-2, IO_TIMEOUT=-3, IO_ERROR=-4, NOT_CONNECTED=-5, UNSUPPORTED=-6, PARSE_ERROR=-7 }`。
- CallMethod methods:
  - `"logger.set"` → 設定層級與輸出；
  - `"connection.reconnect"` → 觸發重連；
  - `"item.reload"` → 重新載入組態（可選）。
- Value JSON conventions: bool→`true|false`，整數→數字，浮點→數字；字陣列/批次讀可回 `{ "array": [ ... ] }`。

## IModbusClient Interface (sketch)

```
struct IModbusClient {
  virtual ~IModbusClient() {}
  virtual int connect() = 0;
  virtual void close() = 0;
  // FC1,2
  virtual int read_coils(int unit, int addr, int count, uint8_t* out) = 0;
  virtual int read_discrete_inputs(int unit, int addr, int count, uint8_t* out) = 0;
  // FC3,4
  virtual int read_holding_regs(int unit, int addr, int count, uint16_t* out) = 0;
  virtual int read_input_regs(int unit, int addr, int count, uint16_t* out) = 0;
  // FC5,6
  virtual int write_single_coil(int unit, int addr, bool on) = 0;
  virtual int write_single_reg(int unit, int addr, uint16_t value) = 0;
  // FC15,16
  virtual int write_multiple_coils(int unit, int addr, int count, const uint8_t* values) = 0;
  virtual int write_multiple_regs(int unit, int addr, int count, const uint16_t* values) = 0;
  virtual void set_timeout_ms(int ms) = 0;
};
```

## CMake Options & Targets

- Options:
  - `-DWITH_LIBMODBUS=ON|OFF`（預設 OFF）
  - `-DWITH_TESTS=ON|OFF`（鏡射 `BUILD_TESTING`）
- Targets:
  - `ioh_modbus` 共享函式庫；視選項連結 libmodbus 或 stub。
  - `test_e2e` 整合測試可執行檔；CTest 名稱 `e2e`。
- Policies:
  - Windows/Linux 可建置；MSVC 編譯警告處理；PIE/Visibility 預設安全。

## CI & Testing Strategy

- Unit (CTests):
  - `WITH_LIBMODBUS=OFF`；`ctest -E e2e`；OS∈{ubuntu,windows}。
- Integration (pymodbus):
  - 安裝 `pymodbus`；Linux 安裝 `libmodbus-dev`，Windows 用 vcpkg；
  - 啟動 `tests/integration/modbus_sim.py`，以 matrix `PORT ∈ {1502,1503}`；
  - `ctest -R e2e`；匯出 `config/ci.modbus.json` 指向對應 PORT。
- Artifacts/Logs: CI 失敗時上傳 `build/Testing/` 與程式輸出。

## Open Questions

- `CallMethod` 用法是否還需要查詢狀態（如 `connection.status`）？
- 是否需要提供批次多項同時讀寫 API（name list/values）以降低往返？
- RTU 整合測試在 CI 是否以 Linux-only 或僅本機執行？

