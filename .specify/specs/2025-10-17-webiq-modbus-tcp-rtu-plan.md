# Implementation Plan: WebIQ ioHandler Modbus TCP/RTU 擴充模組

**Branch**: `002-webiq-modbus-sdk` | **Date**: 2025-10-17 | **Spec**: .specify/specs/2025-10-17-webiq-modbus-tcp-rtu-spec.md:1
**Input**: Feature specification from `/speckit.specify`

## Summary

在現有骨架（C++14、CMake、可選 libmodbus、CTests、pymodbus 模擬器、CI）上，完成 WebIQ ioHandler 的 Modbus TCP/RTU/ASCII 擴充模組。對外暴露固定 C API（Create/Destroy/Subscribe/Unsubscribe/Read/Write/CallMethod），以 JSON 組態描述傳輸與 items 映射，支援輪詢與「只在變更時推送」策略；內部以 `IModbusClient` 抽象封裝 FC1/2/3/4/5/6/8/15/16 與廣播行為，提供 libmodbus、ASCII、自家 stub 實作；單元測試覆蓋編解碼/縮放/字序/CRC/LRC 與 C API 端到端；整合測試以 `pymodbus` 模擬器與 ASCII stub 驗證；CI 進行多 OS/架構/Transport 矩陣。

技術依據：doc/sdk/notes/sdk_ref_example.md:1、doc/sdk/notes/sdk_ref_variant.md:1（型別轉換、縮放、字組順序與範例）。

## Technical Context

- Language/Version: C++14
- Primary Dependencies: CMake 3.14+、nlohmann_json、libmodbus（可選，整合測試時啟用）
- Testing: CTest（單元）、Python+pymodbus（整合）
- Target Platform: Windows (x86/x64)、Linux (x86/x64/ARMhf/ARM64)；Transport 覆蓋 TCP / RTU / ASCII
- Project Type: 單一原生函式庫 + 測試可執行檔
- Performance Goals: 輪詢路徑單次讀寫 < 5ms（stub/本地），避免重入；ASCII 模式 frame 處理 < 10ms；E2E 測試 < 60s
- Constraints: C API 穩定；變更需附最小重現測試與 git diff；遵循憲章的 CI 矩陣；RTU/ASCII 必須遵守 t1.5/t3.5 frame silence 與 LRC/CRC 驗證

## Constitution Check

- C++14+、跨平台抽象：OK（現有 CMake 與 IModbusClient 抽象）
- CMake+WITH_LIBMODBUS 可選：OK（預設 OFF）
- 單元測試 CTest 非談判：OK（已建置一組 unit_* 測試）
- 整合測試 Python+pymodbus：OK（tests/integration/modbus_sim.py, test_e2e.cpp）
- CI：OS×PORT 矩陣與多架構：OK（.github/workflows/CI.yml 已擴充）

## Project Structure（選用單一專案）

```
include/
  IModbusClient.hpp          # FC1/2/3/4/5/6/8/15/16 介面 + make_stub_client
  ModbusIoHandler.hpp        # Handler 協調者（將於 Phase 2 完成）
  export.hpp                 # WIQ_IOH_API 匯出

src/
  Export.cpp                 # C API 實作（Create/Read/Write/CallMethod...）
  ModbusIoHandler.cpp        # Handler 協調者（將於 Phase 2 完成）
  modbus/ModbusClient.cpp    # Stub 實作；WITH_LIBMODBUS 時提供 Tcp/RTU 客戶端
  modbus/AsciiModbusClient.cpp  # ASCII 封包與 LRC 控制（Phase 4）

config/
  README.md                  # 驗證與使用說明
  examples/{tcp,rtu,ascii}.modbus.json
  schema/modbus.schema.json  # Draft-07 外部驗證

tests/
  unit/                      # 單元（不依賴 libmodbus）
  integration/               # 整合（pymodbus 模擬器 + E2E）
```

**Structure Decision**: 單一原生函式庫，跨平台以 CMake 選項切換後端；單元/整合測試獨立目標。

## Module Architecture

- C API（src/Export.cpp）
  - CreateIoInstance/DestroyIoInstance：讀取 JSON、驗證、建立 IoContext 與 IModbusClient（libmodbus 或 stub）
  - Subscribe/Unsubscribe：後續接入 Poller 訂閱管理（目前 no-op）
  - ReadItem/WriteItem：路由至 IModbusClient；支援：
    - FC1/2：單點 bool 與區段（JSON 陣列）
    - FC3/4：int16/uint16/float；`swap_words`/`count>=2` 轉為 32-bit
    - FC5/6：單點寫入；scale/offset 反向（scale=0 錯）
    - FC8：Diagnostic counter 查詢，轉換為 JSON 統計
    - FC15/16：多點寫入（bool 陣列或 u16 陣列；float 兩字）
    - Exception Response：解碼 slave 回覆並填入 JSON 錯誤結構
  - CallMethod：`connection.reconnect`、`logger.set`、`diagnostics.reset`、`diagnostics.snapshot`、（可選）`item.reload`

- ModbusIoHandler（include/ModbusIoHandler.hpp, src/ModbusIoHandler.cpp）
  - 責任：持有 items registry、啟停 Poller、維護快取與變更推送；向 C API 提供查詢/操作介面

- IModbusClient（include/IModbusClient.hpp）
  - 抽象：`connect/close/set_timeout_ms/set_frame_silence`、FC1/2/3/4/5/6/8/15/16、`read_diagnostics`、`write_broadcast`
  - 實作：
    - Stub（src/modbus/ModbusClient.cpp）：記憶體模型 + CRC/LRC 注入
    - Tcp/Rtu（WITH_LIBMODBUS）：libmodbus TCP/RTU 適配
    - AsciiModbusClient（新）：' : ' 前綴、CRLF 結尾、LRC 計算與 frame silence 管理

- Poller（新檔）
  - 每 item（或合併區段）依 `poll_ms` 輪詢，做 bytes-level 變更比對，只在變更時上報；依 transport 採用對應 frame_silence 與 batching
  - 背壓與超時：避免重入，尊重逾時；遇錯誤按策略重試並更新診斷計數器

- ConfigParser（內嵌於 Export.cpp，後續可抽出）
  - 檢核：transport/type/fc 相容、unit/address 範圍、float=>count>=2；ASCII 必須指定 `lrc_check`；`unit_id=0` 僅在 `broadcast_allowed=true` 時通過
  - 支援外部 Schema 驗證（工具面），以 in-code 檢核為主，未來可輸出 JSON Schema（含 ASCII 欄位）

## Phases & Tasks

- Phase 1: API 覆蓋擴充（進行中）
  - 完成 FC2 讀、FC3/4 陣列讀、FC15/16 陣列覆蓋更多 case
  - Exception Response 轉換：將 slave exception 轉為 JSON 錯誤碼與訊息
  - 單元：新增邊界/例外測試（長度越界、非法功能碼、例外碼 mapping）

- Phase 2: ModbusIoHandler + ConfigParser 抽出
  - 新增 `ModbusIoHandler` 類別與 `Poller` 類別；`Subscribe/Unsubscribe` 實作項目級訂閱
  - 建立變更快取；僅變更才推送；提供 thread-safe 查詢 API 供 C API 呼叫
  - ConfigParser 支援 ASCII 欄位、frame_silence、`broadcast_allowed` 檢核，並預留 FC08 項目

- Phase 3: Poller + Diagnostic Counter + 廣播
  - 指數退避（TCP）/步進退避（RTU/ASCII）；錯誤分類與記錄；可配置最大重試次數
  - 建立診斷統計（FC08、CRC/LRC/Timeout/Broadcast）與 `diagnostics.*` CallMethod
  - 廣播寫入策略：僅允許標記 `broadcast_allowed=true` 的 item
  - 單元：錯誤注入（stub）驗證重試行為、診斷累計、廣播拒絕與允許案例

- Phase 4: RTU + ASCII Backend + Frame 驗證
  - `RtuModbusClient`：序列參數（port/baud/parity/data/stop/timeout/frame_silence）
  - `AsciiModbusClient`：':' header、CRLF footer、LRC 計算、frame silence 管理
  - 共用 `CrcLrcUtils`：提供 CRC16/LRC 計算與比對（使用官方範例）
  - 單元：RTU/ASCII stub 行為、CRC/LRC 驗證、frame silence 單元測試

- Phase 5: 整合測試擴充（TCP/RTU/ASCII）
  - E2E 加入 FC2、FC15/16 陣列；超時/重連與 Exception Response
  - ASCII 模式測試：涵蓋 LRC 錯誤、廣播寫入、FC08 診斷
  - 若 pymodbus 不支援 ASCII，建立 stub server 或測試 harness

- Phase 6: 文件與示例完善
  - README、tests/README、config/examples 更新 TCP/RTU/ASCII、診斷與廣播說明；提供 ASCII JSON 範例
  - 錯誤碼與日誌章節描述 exception code、diagnostic counter、frame 設定

## CMake Configuration

- Options：`WITH_LIBMODBUS`（預設 OFF）、`WITH_TESTS`（鏡射 `BUILD_TESTING`）
- Targets：
  - `ioh_modbus` 共享庫
  - 單元：`test_stub`、`test_codec`、`test_scale_zero`、`test_api_float_swap`、`test_api_coils_array`、`test_config_invalid`、`test_exception_map`、`test_crc_lrc`
  - 整合：`test_e2e`（CTest 名稱 `e2e`）、`test_e2e_ascii`
- Dependencies：nlohmann_json（FetchContent）、libmodbus（WITH_LIBMODBUS=ON 時）、pymodbus（整合）、ASCII stub server（Python 腳本或 C++ 工具）

## libmodbus Option

- OFF：使用 stub，單元測試全綠
- ON（TCP）：以 libmodbus 實連線；若找不到，警告且退回 stub（不中斷開發）
- RTU：下一階段新增（CI 以 stub 驗證）

## Testing Flow

- Unit（所有架構）：
  - 編解碼與縮放、swap_words、Exception mapping、CRC/LRC 工具；C API 端到端（float/陣列/ASCII）；組態不合法拒絕
- Integration（x64 Windows/Linux）：
  - 啟動 `pymodbus` 模擬器（TCP/RTU）；`ctest -R e2e` 驗證 FC1/3/4/5/6/16 + Exception
  - ASCII 模式：若無官方模擬器則啟動自家 `tests/integration/ascii_sim.py`；`ctest -R e2e_ascii` 驗證 LRC、廣播、FC08
- Manual（可選）：WebIQ Runtime 實機驗收

## CI Requirements（現況皆已設置）

- 單元（矩陣）
  - Windows：x64、x86（WITH_LIBMODBUS=OFF；`ctest -R ^unit_`）
  - Linux：x86_64/i386（原生）、armhf/arm64（QEMU，`^unit_(stub|codec|config_invalid|api_float_swap|api_coils_array|exception_map|crc_lrc)$`）
  - 失敗上傳 `build/Testing` 與 `LastTest.log`
- 整合（矩陣）
  - OS：ubuntu-latest、windows-latest；PORT：1502、1503；Transport：tcp、rtu、ascii
  - Linux/Windows 啟動模擬器並做連線就緒探測；`WITH_LIBMODBUS=ON`；`ctest -R e2e`、`ctest -R e2e_ascii`
  - max-parallel: 2；失敗上傳 logs、診斷快照與 ASCII frame dump

## Risks & Mitigations

- Windows libmodbus 供應：以 vcpkg + 快取緩解；若失敗回退 stub
- ARM 測試穩定性：QEMU 減並行、加長 timeout、挑選核心測試集合
- 浮點精度：以 bytes 組裝避免解析度損失；`sdk_ref_variant.md` 指南
- ASCII 模擬器缺口：若 pymodbus 未支援 ASCII，需自建 stub server；在 CI 上傳 frame dump 以利除錯
- t1.5/t3.5 設定錯誤：在單元測試驗證 frame silence、文件提供建議值並加入 runtime 日誌

## Acceptance Criteria

- 單元綠燈（WITH_LIBMODBUS=OFF），涵蓋 Exception/CRC/LRC 測試
- 整合綠燈（OS×PORT×Transport 矩陣），ASCII 場景包含 LRC 錯誤與廣播
- config/examples 可通過 Schema，且 CreateIoInstance 檢核嚴謹（TCP/RTU/ASCII）
- 文檔與憲章要求符合，並附上診斷 counter reset/ snapshot 的測試證據

## References

- doc/sdk/notes/sdk_ref_example.md:1
- doc/sdk/notes/sdk_ref_variant.md:1
- .specify/specs/2025-10-17-webiq-modbus-tcp-rtu-spec.md:1
