Summary

- 擴充並修正 IOHandler API 與編碼邏輯，新增 64‑bit double 字序控制、修正 FC3/FC4 count=2 非 float 陣列行為，並在 I/O 執行時新增 NOT_CONNECTED 自動重連。
- 恢復並擴充 CI 矩陣（Linux x86_64/i386/armhf/arm64 + Windows win64/win32），強化整合測試模擬器啟動，並加入 pip/vcpkg/ccache 快取以加速。
- 新增 CMake install() 與 CPack 封裝，以及 Release workflow（tag v* 觸發），並已驗證 v0.2.2 釋出流程與封裝。

Changes

- API/Codec
  - Auto‑reconnect：任何讀/寫呼叫若回傳 NOT_CONNECTED，會嘗試 connect() 一次後重試（src/Export.cpp）。
  - 64‑bit double 字序控制：新增 `word_order`（ABCD/BADC/CDAB/DCBA），FC3/FC4/FC16 對 double 強制 `count=4`；FC3/FC4 陣列 `count=2` 非 float 不再套用 float 解析（src/Export.cpp）。
  - 修正 MSVC 縮窄轉型警告：uint16_t 初始化明確轉型（src/Export.cpp）。

- Schema/Docs
  - Schema：新增 `word_order`（config/schema/modbus.schema.json）。
  - 設定文件：補充 double `word_order` 與陣列行為（config/README.md）。
  - SDK 快速參考：補充 auto‑reconnect 與打包字序行為（doc/sdk_ref.md）。
  - CHANGELOG：新增 v0.2.0 條目（CHANGELOG.md）。

- Tests
  - double 字序測試：ABCD/BADC/CDAB/DCBA 全覆蓋。
  - FC3/FC4 非 float、count=2 陣列測試（避免被當成 float）。
  - 新增檔案：
    - tests/unit/test_api_double_word_order_abcd.cpp
    - tests/unit/test_api_double_word_order_badc.cpp
    - tests/unit/test_api_double_word_order_cdab.cpp
    - tests/unit/test_api_fc3_array_2.cpp
    - tests/unit/test_api_fc4_array_2.cpp
  - CMake 註冊並納入 PATH/LD_LIBRARY_PATH 測試環境（CMakeLists.txt）。

- CI
  - 矩陣：Unit（Linux x86_64/i386/armhf/arm64；Windows win64/win32），Integration（Linux x86_64、Windows win64/win32）。
  - ARM multiarch：限制 native sources=amd64、為 foreign arch（armhf/arm64）加入 ports.ubuntu.com sources；安裝 libc6/libstdc++ 外架構與 QEMU。
  - 快取：pip/vcpkg/ccache；Linux Configure 啟用 `-DCMAKE_*_COMPILER_LAUNCHER=ccache`。
  - Ubuntu 模擬器：pymodbus 檢查、nohup log、延長 readiness、失敗印出 log。
  - Windows release：解析 cmake/cpack 路徑（Get‑Command + Program Files fallback）。

- Packaging/Release
  - CMake install() + CPack：產出 ZIP/TGZ 封裝（含 LICENSE 與 README；提供 CMake package config 供 `find_package`）。
  - Release workflow（.github/workflows/release.yml）：tag `v*` 觸發，於 Ubuntu/Windows 建置並 CPack 打包，上傳資產到 GitHub Releases。
  - 已驗證：v0.2.2 釋出流程與封裝（兩平台）

Migration/Compatibility

- double 類型在 FC3/FC4/FC16 強制 `count=4`；float 在 FC3/FC4/FC16 強制 `count=2`。若省略 `count`，CreateIoInstance 會自動補正；若填錯，將返回錯誤（易於早期發現設定問題）。
- 對於 count=2 非 float 陣列，不再當作 float 解析（不再有誤判）。

Release Notes（建議貼在 Release v0.2.2 中）

- v0.2.0 要點：
  - API/Codec
    - Add 64‑bit `double` with `word_order`（ABCD/BADC/CDAB/DCBA）
    - Enforce counts for float/double；Fix FC3/FC4 array handling
    - Auto‑reconnect on NOT_CONNECTED
  - Schema/Docs：增加 `word_order` 與文件
  - Tests：增加 word_order 與 FC3/FC4 陣列
  - CI：矩陣擴充與快取、ARM multiarch、模擬器穩健化

Verification

- Unit：Linux x86_64/i386/armhf/arm64，Windows win64/win32 全綠
- Integration：Ubuntu/Windows 全綠
- Release：v0.2.2 GitHub Releases 可附加 Linux/Windows 封裝資產

Follow‑ups（可後續 PR）

- 可選新增 DEB/NSIS 產生器
- Sanitizers：視資源考量，擴充到整合測試或啟用 WITH_LIBMODBUS=ON 變體

Tags/Release 註記

- 保留 v0.2.0 與 v0.2.1 之 Release 為 Pre‑release（測試紀錄）
- 正式對外使用 v0.2.2
