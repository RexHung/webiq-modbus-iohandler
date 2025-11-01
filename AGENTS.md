# Repository Guidelines

## Project Structure & Module Organization
- `include/` 暴露公共 API（如 `ModbusIoHandler.hpp`），搭配 `export.hpp` 定義 DLL 介面。
- `src/` 實作核心邏輯與後端客戶端；`src/modbus/` 容納 stub 或 libmodbus 適配層。
- `config/` 提供 JSON 組態與說明，`config/ci.modbus.json` 與 CI 測試共用。
- `tests/unit/` 以 CTest 執行的單元測試；`tests/integration/` 含 `modbus_sim.py`、`test_e2e.cpp`。
- `examples/` 布建示例工程，`doc/` 蒐集 SDK 策略與規範備忘。

## Build, Test, and Development Commands
- 產生建置：`cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DWITH_LIBMODBUS=OFF`（stub 後端預設）。
- 編譯：`cmake --build build --config Release`；在 Windows 可指定 VS 生成器與架構。
- 啟用 libmodbus：`cmake -S . -B build -DWITH_LIBMODBUS=ON`，需事先裝妥 system/vcpkg 套件。
- 執行測試：`ctest --test-dir build --output-on-failure`；整合測試預設會啟動 `tests/integration/modbus_sim.py`。
- 驗證 Python 模擬器：`python3 tests/integration/modbus_sim.py --help` 了解可用參數。

## Coding Style & Naming Conventions
- 全專案採 C++14 與 LLVM 風格 `clang-format`（請在提交前執行）。
- 檔名使用 `PascalCase` 類別與 `snake_case` 測試檔，例如 `test_poller.cpp`。
- 公共標頭需具 include guard 與簡短 Doxygen 註解；命名空間保持 `wiq::` 前綴。

## Testing Guidelines
- 使用 CTest 驅動單元測試；測試檔名遵循 `test_<feature>.cpp`。
- 整合測試依賴 Python `pymodbus`，確保本機具備對應虛擬環境；跑 `pip install -r tests/integration/requirements.txt`。
- 新增功能時補齊正向與錯誤路徑案例；變更 Modbus 功能碼時同步更新模擬器測資。
- 提交前需確保 `WITH_TESTS=ON` 情況下的 `ctest` 全綠，並於 PR 描述列出執行命令。

## Commit & Pull Request Guidelines
- Commit 類型沿用 `feat:`, `fix:`, `docs:`, `test:`, `chore:` 模式；訊息第一行限制在 72 字元內。
- PR 需說明目的、重點變更、測試結果與相關 issue 連結；若調整 JSON 組態，附上差異片段或新增樣例。
- 請在 PR 中標註是否啟用 `WITH_LIBMODBUS`、是否跑過整合測試，以及必要的環境設定（如 `CMAKE_PREFIX_PATH`）。

## Configuration & Environment Tips
- Windows 建議以 vcpkg 安裝 libmodbus，並於 `CMAKE_TOOLCHAIN_FILE` 指向 `scripts/buildsystems/vcpkg.cmake`。
- Linux 開發可利用 `LD_LIBRARY_PATH` 指向 build 產物，以便執行 `test_e2e` 或範例程式。
- JSON 組態中的 `transport` 可選 `tcp` 或 `rtu`；確保 `unit_id`、`function`、`poll_ms` 等欄位遵守 README 中的範例。
