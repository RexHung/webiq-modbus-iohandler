# Implementation Plan: WebIQ ioHandler Modbus TCP/RTU 擴充模組

**Branch**: `002-webiq-modbus-sdk` | **Date**: 2025-10-17 | **Spec**: .specify/specs/2025-10-17-webiq-modbus-tcp-rtu-spec.md:1
**Input**: Feature specification from `/speckit.specify`

## Summary

在現有骨架（C++14、CMake、可選 libmodbus、CTests、pymodbus 模擬器、CI）上，完成 WebIQ ioHandler 的 Modbus TCP/RTU 擴充模組。對外暴露固定 C API（Create/Destroy/Subscribe/Unsubscribe/Read/Write/CallMethod），以 JSON 組態描述傳輸與 items 映射，支援輪詢與「只在變更時推送」策略；內部以 `IModbusClient` 抽象封裝 FC1/2/3/4/5/6/15/16，提供 libmodbus 與 stub 實作；單元測試覆蓋編解碼/縮放/字序與 C API 端到端；整合測試以 `pymodbus` 模擬器驗證 TCP；CI 進行多 OS/架構矩陣。

技術依據：doc/sdk/notes/sdk_ref_example.md:1、doc/sdk/notes/sdk_ref_variant.md:1（型別轉換、縮放、字組順序與範例）。

## Technical Context

- Language/Version: C++14
- Primary Dependencies: CMake 3.14+、nlohmann_json、libmodbus（可選，整合測試時啟用）
- Testing: CTest（單元）、Python+pymodbus（整合）
- Target Platform: Windows (x86/x64)、Linux (x86/x64/ARMhf/ARM64)
- Project Type: 單一原生函式庫 + 測試可執行檔
- Performance Goals: 輪詢路徑單次讀寫 < 5ms（stub/本地），避免重入；E2E 測試 < 60s
- Constraints: C API 穩定；變更需附最小重現測試與 git diff；遵循憲章的 CI 矩陣

## Constitution Check

- C++14+、跨平台抽象：OK（現有 CMake 與 IModbusClient 抽象）
- CMake+WITH_LIBMODBUS 可選：OK（預設 OFF）
- 單元測試 CTest 非談判：OK（已建置一組 unit_* 測試）
- 整合測試 Python+pymodbus：OK（tests/integration/modbus_sim.py, test_e2e.cpp）
- CI：OS×PORT 矩陣與多架構：OK（.github/workflows/CI.yml 已擴充）

## Project Structure（選用單一專案）

```
include/
  IModbusClient.hpp          # FC1/2/3/4/5/6/15/16 介面 + make_stub_client
  ModbusIoHandler.hpp        # Handler 協調者（將於 Phase 2 完成）
  export.hpp                 # WIQ_IOH_API 匯出

src/
  Export.cpp                 # C API 實作（Create/Read/Write/CallMethod...）
  ModbusIoHandler.cpp        # Handler 協調者（將於 Phase 2 完成）
  modbus/ModbusClient.cpp    # Stub 實作；WITH_LIBMODBUS 時提供 Tcp 客戶端

config/
  README.md                  # 驗證與使用說明
  examples/{tcp,rtu}.modbus.json
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
    - FC15/16：多點寫入（bool 陣列或 u16 陣列；float 兩字）
  - CallMethod：`connection.reconnect`、`logger.set`、（可選）`item.reload`

- ModbusIoHandler（include/ModbusIoHandler.hpp, src/ModbusIoHandler.cpp）
  - 責任：持有 items registry、啟停 Poller、維護快取與變更推送；向 C API 提供查詢/操作介面

- IModbusClient（include/IModbusClient.hpp）
  - 抽象：connect/close/timeout + FC1/2/3/4/5/6/15/16
  - 實作：
    - Stub（src/modbus/ModbusClient.cpp）：記憶體模型
    - Tcp（WITH_LIBMODBUS）：libmodbus TCP；後續補 RTU 客戶端

- Poller（新檔）
  - 每 item（或合併區段）依 `poll_ms` 輪詢，做 bytes-level 變更比對，只在變更時上報
  - 背壓與超時：避免重入，尊重逾時；遇錯誤按策略重試

- ConfigParser（內嵌於 Export.cpp，後續可抽出）
  - 檢核：transport/type/fc 相容、unit/address 範圍、float=>count>=2；失敗返回 nullptr
  - 支援外部 Schema 驗證（工具面），以 in-code 檢核為主

## Phases & Tasks

- Phase 1: API 覆蓋擴充（進行中）
  - 完成 FC2 讀、FC3/4 陣列讀、FC15/16 陣列覆蓋更多 case
  - 單元：新增邊界/錯誤測試（長度越界、型別不符）

- Phase 2: ModbusIoHandler + ConfigParser 抽出
  - 新增 `ModbusIoHandler` 類別與 `Poller` 類別；`Subscribe/Unsubscribe` 實作項目級訂閱
  - 建立變更快取；僅變更才推送；提供 thread-safe 查詢 API 供 C API 呼叫

- Phase 3: Poller 與重試策略
  - 指數退避（TCP）/步進退避（RTU）；錯誤分類與記錄；可配置最大重試次數
  - 單元：錯誤注入（stub）驗證重試行為

- Phase 4: RTU Backend
  - `RtuModbusClient`：序列參數（port/baud/parity/data/stop/timeout）
  - 單元：RTU stub 行為（doc/sdk/notes/rtu_stub.md）

- Phase 5: 整合測試擴充
  - E2E 加入 FC2、FC15/16 陣列；超時/重連場景

- Phase 6: 文件與示例完善
  - README、tests/README、config/examples、錯誤碼與日誌說明

## CMake Configuration

- Options：`WITH_LIBMODBUS`（預設 OFF）、`WITH_TESTS`（鏡射 `BUILD_TESTING`）
- Targets：
  - `ioh_modbus` 共享庫
  - 單元：`test_stub`、`test_codec`、`test_scale_zero`、`test_api_float_swap`、`test_api_coils_array`、`test_config_invalid`
  - 整合：`test_e2e`（CTest 名稱 `e2e`）
- Dependencies：nlohmann_json（FetchContent）、libmodbus（WITH_LIBMODBUS=ON 時）

## libmodbus Option

- OFF：使用 stub，單元測試全綠
- ON（TCP）：以 libmodbus 實連線；若找不到，警告且退回 stub（不中斷開發）
- RTU：下一階段新增（CI 以 stub 驗證）

## Testing Flow

- Unit（所有架構）：
  - 編解碼與縮放、swap_words；C API 端到端（float/陣列）；組態不合法拒絕
- Integration（x64 Windows/Linux）：
  - 啟動 `pymodbus` 模擬器；`ctest -R e2e` 驗證 FC1/3/4/5/6/16
- Manual（可選）：WebIQ Runtime 實機驗收

## CI Requirements（現況皆已設置）

- 單元（矩陣）
  - Windows：x64、x86（WITH_LIBMODBUS=OFF；`ctest -R ^unit_`）
  - Linux：x86_64/i386（原生）、armhf/arm64（QEMU，`^unit_(stub|codec|config_invalid|api_float_swap|api_coils_array)$`）
  - 失敗上傳 `build/Testing` 與 `LastTest.log`
- 整合（矩陣）
  - OS：ubuntu-latest、windows-latest；PORT：1502、1503
  - Linux/Windows 啟動模擬器並做連線就緒探測；`WITH_LIBMODBUS=ON`；`ctest -R e2e`
  - max-parallel: 2；失敗上傳 logs

## Risks & Mitigations

- Windows libmodbus 供應：以 vcpkg + 快取緩解；若失敗回退 stub
- ARM 測試穩定性：QEMU 減並行、加長 timeout、挑選核心測試集合
- 浮點精度：以 bytes 組裝避免解析度損失；`sdk_ref_variant.md` 指南

## Acceptance Criteria

- 單元綠燈（WITH_LIBMODBUS=OFF）
- 整合綠燈（OS×PORT 矩陣）
- config/examples 可通過 Schema，且 CreateIoInstance 檢核嚴謹
- 文檔與憲章要求符合

## References

- doc/sdk/notes/sdk_ref_example.md:1
- doc/sdk/notes/sdk_ref_variant.md:1
- .specify/specs/2025-10-17-webiq-modbus-tcp-rtu-spec.md:1
