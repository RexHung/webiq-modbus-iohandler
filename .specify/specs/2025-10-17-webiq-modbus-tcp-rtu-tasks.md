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
- [ ] T005 [API] Exception Response：`src/Export.cpp` / handler 將 Modbus exception code 轉為 JSON 錯誤結構，並記錄診斷快照
- [ ] T006 [Unit] Exception 測試：新增 `tests/unit/test_exception_map.cpp` 覆蓋 FC1/3/5 的例外回覆與 JSON 格式

---

## Phase 2: ModbusIoHandler + ConfigParser 抽出

- [ ] T010 [P] [Core] 新增 `include/ModbusIoHandler.hpp` 與 `src/ModbusIoHandler.cpp`：封裝 items registry、I/O 路由
- [ ] T011 [P] [Core] 將組態解析自 `src/Export.cpp` 抽出為 `src/ConfigParser.cpp|hpp`（並在 Export.cpp 使用）
- [ ] T012 [API] 實作 `SubscribeItems/UnsubscribeItems`：在 ModbusIoHandler 中維護訂閱表
- [ ] T013 [Unit] 新增 handler/config 單元測試：`tests/unit/test_handler_config.cpp`
- [ ] T014 [Core] ConfigParser 擴充：驗證 `transport`=ascii、`frame_silence_ms`、`broadcast_allowed` 與 FC08 定義；更新 JSON Schema

---

## Phase 3: Poller + 變更推送

- [ ] T020 [Core] 新增 `src/Poller.cpp|hpp`：依 `poll_ms` 合併區段（相同 unit/fc）輪詢
- [ ] T021 [Core] 變更快取：bytes-level 比對（bool=1B、16-bit=2B、32-bit=4B，尊重 `swap_words`）
- [ ] T022 [Core] 錯誤與重試：TCP 指數退避、RTU/ASCII 步進退避；最大重試次數可組態
- [ ] T023 [API] `ReadItem` 從快取提供最新值；`CallMethod("logger.set")` 調整層級；`diagnostics.reset/snapshot`
- [ ] T024 [Unit] Poller/快取 單元測試：`tests/unit/test_poller.cpp`
- [ ] T025 [Core] Diagnostic Counter：實作 FC08 統計與 `diagnostics.*` CallMethod、累計 CRC/LRC/timeout/broadcast
- [ ] T026 [Core] 廣播寫入守門：僅允許 `broadcast_allowed=true` 的 `unit_id=0` item，並記錄成功次數
- [ ] T027 [Unit] Diagnostic & Broadcast 測試：`tests/unit/test_diagnostics.cpp` 覆蓋計數累加、重置、廣播拒絕案例

---

## Phase 4: RTU Backend

- [ ] T030 [Core] `RtuModbusClient`（WITH_LIBMODBUS）：`src/modbus/RtuModbusClient.cpp|hpp`，參數對應 `rtu{}` 並支援 `frame_silence_ms`
- [ ] T031 [Unit] RTU stub 測試（無硬體）：參考 `doc/sdk/notes/rtu_stub.md` 新增 `tests/unit/test_rtu_stub.cpp`
- [ ] T032 [Docs] 在 `tests/README.md` 補充 RTU 模擬/本機測試說明
- [ ] T033 [Core] `AsciiModbusClient`：`src/modbus/AsciiModbusClient.cpp|hpp`，完成 ':'/CRLF 封包、LRC 計算、frame silence 控制
- [ ] T034 [Core] 共用 `CrcLrcUtils`：建立 CRC16/LRC 計算工具與對照測試資料
- [ ] T035 [Unit] ASCII 測試：`tests/unit/test_ascii_client.cpp` 覆蓋 LRC 正確性、frame_silence、錯誤情境

---

## Phase 5: 整合測試擴充

- [ ] T040 [Int] 擴充 `tests/integration/modbus_sim.py`：支援更多測資（FC2、FC15/16 陣列、例外回覆、FC08）
- [ ] T041 [Int] 擴充 `tests/integration/test_e2e.cpp`：加入 FC2、陣列讀寫、超時→重連案例
- [ ] T042 [Int] 建立 `tests/integration/ascii_sim.py`（或等效 stub）：支援 ASCII frame、LRC、廣播寫入
- [ ] T043 [Int] 新增 `tests/integration/test_e2e_ascii.cpp`：驗證 ASCII 讀寫、LRC 錯誤、廣播、FC08 診斷
- [ ] T044 [Int] CI 腳本更新：新增 ASCII transport job、輸出診斷 snapshot 與 frame dump artifact

---

## Phase 6: 文件與 CI

- [ ] T050 [Docs] README 與 `config/README.md` 補充更多範例與錯誤碼對照（含 TCP/RTU/ASCII 設定、exception/diagnostic 說明）
- [ ] T051 [CI] 在整合工作上傳額外 artifacts（可選：二進位、模擬器輸出、ASCII frame dump、diagnostic snapshot）
- [ ] T052 [CI] 可選新增 Schema 驗證步驟：以 `jsonschema` 驗證 `config/examples/*.json`
- [ ] T053 [Docs] 更新 `config/examples/` 與 `tests/README.md`，加入 ASCII 組態範例與操作指引

---

## Nice-to-have / 後續

- [ ] T060 [Perf] 輪詢批次策略與多執行緒調度微調（避免裝置背壓）
- [ ] T061 [API] `CallMethod("connection.status")`/`("item.reload")` 完整規格化
- [ ] T062 [DX] 提供 `scripts/` 快速啟動與本機檢查腳本

---

## Tracing to Spec & Plan

- 覆蓋 `spec.md` 之 API / JSON 組態 / Poller / 錯誤模型 / 測試與 CI 需求
- 與 `plan.md` 階段對應：Phase 1→6 對應 M1→M6 交付
