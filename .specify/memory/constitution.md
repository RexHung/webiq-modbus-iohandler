<!--
Sync Impact Report
- Updated: .specify/memory/constitution.md (v1.0.0) to encode C++14+, CMake + optional libmodbus, CTest unit tests, Python+pymodbus integration tests, and GitHub Actions matrix (OS x PORT) policies. 
- Reviewed dependent templates under .specify/templates; no structural changes required at this time.
-->

# WebIQ Modbus IO Handler Constitution

## Core Principles

### I. Cross-Platform C++14+
以 C++14（或以上）為最低標準，必須支援 Windows 與 Linux。平台相依實作需以明確抽象層包裝，禁止在核心邏輯中混入特定 OS API。

### II. CMake 為單一建置系統（libmodbus 可選）
使用 CMake（建議 3.14+）。`WITH_LIBMODBUS` 為可選開關（預設 OFF 以確保可移植性）；開啟時連結 libmodbus，關閉時以 stub/adapter 通過所有單元測試。不得出現強制性隱性相依。

### III. 單元測試以 CTest 為準（非談判）
所有功能與修復需附對應 CTest 測試與最小重現案例（MRE）。PR 必須先新增失敗測試再修復（Red→Green→Refactor）。單元測試不得依賴網路或外部服務。

### IV. 整合測試以 Python + pymodbus 模擬器
以 Python 3 的 `pymodbus` 作為 Modbus TCP 模擬器（例如 `tests/integration/modbus_sim.py`）。整合測試在 `WITH_LIBMODBUS=ON` 時執行，驗證端到端流程、錯誤處理與協議相容性。

### V. CI 一致性與並行矩陣
GitHub Actions 必須同時提供：
- 單元測試工作（`WITH_LIBMODBUS=OFF`，Windows/Linux 皆跑）
- 整合測試工作（`WITH_LIBMODBUS=ON`，啟動 `pymodbus` 模擬器）
- 以矩陣並行 OS（`ubuntu-latest`, `windows-latest`）與 PORT（如 `1502`, `1503`）組合。

## 平台與技術約束

- 語言與標準：C++14 以上；禁用未標準化編譯器擴充（除非以可移植宏封裝且提供備援路徑）。
- 建置：CMake 為唯一來源；提供 `BUILD_TESTING` 與 `WITH_LIBMODBUS` 選項；產物與測試目標必須可由 `cmake --build` 與 `ctest` 獨立觸發。
- 依賴：libmodbus 為可選；Python 3 與 `pymodbus` 僅用於整合測試；單元測試不能依賴外部網路或服務。
- 組態：整合測試需以檔案或參數提供目標 `HOST/PORT`（預設 `127.0.0.1:1502`），PORT 需可透過 CI 矩陣覆寫。
- 時間限制：整合測試啟動模擬器後需有合理等待與超時；單元測試單例執行時間建議 < 200ms。
- 可觀測性：錯誤訊息與返回碼一律可判讀；整合測試必要時記錄到檔案以利 CI 取證。

## 工作流程與品質檢核

- 分支與 PR：每項變更需開分支並送 PR；描述需包含「最小重現測試步驟」與對應的 `git diff` 片段（核心變更處）。
- 測試門檻：
  - 單元測試（CTests）必須全綠；新增/修改程式碼需具備足夠涵蓋率（以邏輯路徑與邊界為主）。
  - 整合測試在 `WITH_LIBMODBUS=ON` 時執行並全綠；模擬器 PORT 與 OS 由矩陣並行執行。
- TDD 與回歸：修 bug 先加失敗測試再修；回歸缺陷需新增對應測試以防止重現。
- 版本與相容：採用 SemVer；破壞性變更需在 `CHANGELOG.md` 註記與升版，並提供遷移指引與測試覆蓋。
- 文件：對外 API、CMake 選項、測試與 CI 使用方式須在 `README.md`/`tests/README.md` 可被跟隨。

## Governance

- 本憲章優先於其他慣例；所有 PR Review 必須檢核是否遵循：
  - C++14+、跨平台抽象、CMake 單一事實來源
  - CTest 單元測試與 MRE 必備
  - Python+pymodbus 整合測試策略
  - GitHub Actions 矩陣（OS × PORT）
- 變更憲章需在 PR 中說明影響與遷移方案，並更新相關文件與 CI。小幅澄清可合併至修訂版本，重大調整需標記版本升級。

**Version**: 1.0.0 | **Ratified**: 2025-10-17 | **Last Amended**: 2025-10-17
