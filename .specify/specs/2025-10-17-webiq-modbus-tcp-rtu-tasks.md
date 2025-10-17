# Tasks: WebIQ ioHandler Modbus TCP/RTU 擴充模組

**Input**: Spec (.specify/specs/2025-10-17-webiq-modbus-tcp-rtu-spec.md), Plan (.specify/specs/2025-10-17-webiq-modbus-tcp-rtu-plan.md)
**Prerequisites**: 現有骨架（IModbusClient + stub、基本 C API、單元/整合測試、CI 矩陣）

Format: `[ID] [P] [Phase/Story] Description`
- [P]: 可平行
- Paths must be concrete and minimal

---

## Phase 1: API 覆蓋擴充（進行中）

- [ ] T001 [P] [API] FC2 讀取：`src/Export.cpp` 的 ReadItem 支援 function=2（discrete inputs，bool / 陣列）
- [ ] T002 [P] [API] FC3/4 陣列讀取：`src/Export.cpp` 在 `type!=float` 且 `count>1` 時回傳 JSON 陣列（holding/input）
- [ ] T003 [P] [API] FC15/16 陣列覆蓋：`src/Export.cpp` 寫入時補齊邊界檢查與錯誤回傳（越界/型別不符）
- [ ] T004 [P] [Unit] 針對 T001–T003 新增/擴充測試：`tests/unit/`（包含錯誤案例）

---

## Phase 2: ModbusIoHandler + ConfigParser 抽出

- [ ] T010 [P] [Core] 新增 `include/ModbusIoHandler.hpp` 與 `src/ModbusIoHandler.cpp`：封裝 items registry、I/O 路由
- [ ] T011 [P] [Core] 將組態解析自 `src/Export.cpp` 抽出為 `src/ConfigParser.cpp|hpp`（並在 Export.cpp 使用）
- [ ] T012 [API] 實作 `SubscribeItems/UnsubscribeItems`：在 ModbusIoHandler 中維護訂閱表
- [ ] T013 [Unit] 新增 handler/config 單元測試：`tests/unit/test_handler_config.cpp`

---

## Phase 3: Poller + 變更推送

- [ ] T020 [Core] 新增 `src/Poller.cpp|hpp`：依 `poll_ms` 合併區段（相同 unit/fc）輪詢
- [ ] T021 [Core] 變更快取：bytes-level 比對（bool=1B、16-bit=2B、32-bit=4B，尊重 `swap_words`）
- [ ] T022 [Core] 錯誤與重試：TCP 指數退避、RTU 步進退避；最大重試次數可組態
- [ ] T023 [API] `ReadItem` 從快取提供最新值；`CallMethod("logger.set")` 調整層級；（可選）`item.reload`
- [ ] T024 [Unit] Poller/快取 單元測試：`tests/unit/test_poller.cpp`

---

## Phase 4: RTU Backend

- [ ] T030 [Core] `RtuModbusClient`（WITH_LIBMODBUS）：`src/modbus/RtuModbusClient.cpp|hpp`，參數對應 `rtu{}`
- [ ] T031 [Unit] RTU stub 測試（無硬體）：參考 `doc/sdk/notes/rtu_stub.md` 新增 `tests/unit/test_rtu_stub.cpp`
- [ ] T032 [Docs] 在 `tests/README.md` 補充 RTU 模擬/本機測試說明

---

## Phase 5: 整合測試擴充

- [ ] T040 [Int] 擴充 `tests/integration/modbus_sim.py`：支援更多測資（FC2、FC15/16 陣列）
- [ ] T041 [Int] 擴充 `tests/integration/test_e2e.cpp`：加入 FC2、陣列讀寫、超時→重連案例

---

## Phase 6: 文件與 CI

- [ ] T050 [Docs] README 與 `config/README.md` 補充更多範例與錯誤碼對照
- [ ] T051 [CI] 在整合工作上傳額外 artifacts（可選：二進位、模擬器輸出）
- [ ] T052 [CI] 可選新增 Schema 驗證步驟：以 `jsonschema` 驗證 `config/examples/*.json`

---

## Nice-to-have / 後續

- [ ] T060 [Perf] 輪詢批次策略與多執行緒調度微調（避免裝置背壓）
- [ ] T061 [API] `CallMethod("connection.status")`/`("item.reload")` 完整規格化
- [ ] T062 [DX] 提供 `scripts/` 快速啟動與本機檢查腳本

---

## Tracing to Spec & Plan

- 覆蓋 `spec.md` 之 API / JSON 組態 / Poller / 錯誤模型 / 測試與 CI 需求
- 與 `plan.md` 階段對應：Phase 1→6 對應 M1→M6 交付

