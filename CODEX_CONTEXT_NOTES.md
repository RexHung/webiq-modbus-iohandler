# Codex Context Log v0.1.3

## 更新內容
- `tests/integration/modbus_sim.py`: 將 `run_pymodbus_server` 與相關 helper 包在 `USE_SIMPLE` 判斷內，確保僅在啟用 pymodbus 後端時才解析 `resolve`，避免 Windows 預設 simple server 流程在模組載入期拋出 `NameError`。
- `tests/integration/modbus_sim.py`: 維持跨平台 heartbeat、signal logging 與 FC3/FC4 回應修正；最新的 Windows run 仍可觀察到定期 heartbeat 與連線紀錄。
- `tests/integration/test_modbus_simple.py`: 新增針對 simple server 的單元測試，覆蓋多暫存器讀取（FC3/FC4）與單暫存器寫入（FC6），確保 buffer 長度與資料寫入行為不會回歸。
- `tests/integration/requirements.txt`: 補上 `pytest>=8`，讓 CI 能執行新加入的 simple server 單元測試。
- `.github/workflows/CI.yml`: 加入 `python -m pytest tests/integration/test_modbus_simple.py` 步驟，讓 CI 同步驗證 simple server 邏輯。

## CI 訊息
- Run `18701308350`（Windows win64）仍落在 readiness 階段：`run_pymodbus_server` 重構後，因 `USE_SIMPLE=True` 未定義 `resolve` 造成 `NameError`，已藉由本次修正封裝 helper 解決。
- Run `18713819625`：Ubuntu、Windows x64/x86 全數通過。Windows `modbus_sim_windows.err.log` 僅有 heartbeat 與 client connect/disconnect，`ctest` 成功；Ubuntu job 同樣成功啟動 pymodbus 後端並完成 E2E。

## 新發現
- Linux `pymodbus` 分支需要在模組載入階段保留 `resolve` 定義；將 helper 受 `USE_SIMPLE` 保護後，Windows simple server 不再解析多餘符號，兩邊流程皆正常啟動。
- 最新成功 run 仍顯示 heartbeat 於啟動後 15 秒輸出一次，確認整合步驟可維持 simulator 存活至整個測試結束。

## 後續建議
1. 監控後續 CI，確保 Windows 端長期穩定；若再次出現 readiness 失敗，可直接檢視綜合腳本輸出的診斷資訊與 log tail。
2. 更新/撰寫針對 simple server 的自動化測試，覆蓋多暫存器讀寫與 heartbeat 行為，降低未來回歸風險。
3. 若需擴充功能（例如支援 pymodbus 在 Windows 上運行），可考慮將 `_TRACE_CONNECT` 行為改為跨平台可選項並調整 `SIMPLE_MODBUS` 旗標預設值。

## 附註與快速參考
- Windows simulator log：`build/ci_logs/modbus_sim_windows.log(.err.log)`，整合腳本結束會自動印出尾端內容。
- Ubuntu simulator log：`/tmp/modbus_sim.log`（由 nohup 產生），若遇到啟動失敗，可從 artifact `integration-logs-ubuntu-latest-*-port_1502.zip` 取得。
- 近期關鍵 run：
  - `18713819625` – 全平台通過（最新成功）。
  - `18701308350` – Windows readiness 阻塞，觸發 `resolve` `NameError`（已修正）。
  - `18701055969` – Windows `IndexError` 已於 v0.1.2 修復。

---
## v0.1.2 (歷史紀錄)

### 更新內容
- `.github/workflows/CI.yml`: 將 Windows integration 流程合併為單一 PowerShell 步驟，於同一 process 內啟動/監控 simulator、執行診斷、smoke test 與 ctest，結束時主動停止 simulator 並輸出尾端 log，避免 job object 於步驟結束時硬殺。
- `tests/integration/modbus_sim.py`: 新增 heartbeat 啟動訊息並修正 FC3/4 回應 buffer 長度（`bytearray(2 + count * 2)`），修復多暫存器讀取時的 `IndexError`，同時保留先前 signal/heartbeat/atexit 記錄機制。

### CI 訊息
- Run `18692468108`（Windows win64）：新整合腳本成功保留 simulator 生命週期並捕捉 `IndexError: bytearray index out of range`（`build_simple_response`），`ctest` 因 `ReadItem('holding.temp') failed rc=-4` 失敗，log 亦紀錄多筆 heartbeat 與 client connect/disconnect。
- Run `18701055969`（Windows win64）：套用 buffer 修正後，smoke test 取得 `modbus handshake bytes 00010000000401010100`，`ctest` 通過；`modbus_sim_windows.err.log` 僅剩連線紀錄，無例外堆疊。整體 workflow 當時因 Ubuntu job 的 `Start Modbus TCP simulator (Ubuntu)` 失敗而標紅。
- 先前 run `18692053877` 亦顯示 heartbeat 只印出一次，證實舊流程在 readiness 後約 10~20 秒內遭 runner 強制終止。

### 新發現
- heartbeat/signal 機制證實 simple server 在 readiness 之後仍存活至少 15 秒，沒有收到任何 `SIGTERM/SIGINT/SIGBREAK` 訊息；先前提早終止的罪魁禍首是 GitHub runner 對背景行程的 job object。
- Windows `IndexError` 由 FC3/4 回應長度計算錯誤造成，修正後 `ReadItem` 正常回傳資料，E2E 測試通過。
- 整合腳本尾端會印出 simulator log tail 與 `netstat` 結果，提供後續診斷依據。
- Ubuntu integration 當時在 `Start Modbus TCP simulator (Ubuntu)` 失敗（後續已於 v0.1.3 修正）。

### 後續建議
1. 監控下一輪 CI，確認 Windows job 持續綠燈並檢查是否仍有殘餘 job cleanup 問題。
2. 調查 Ubuntu job 新增的失敗點（`Start Modbus TCP simulator (Ubuntu)`），檢視 `/tmp/modbus_sim.log` 與最近變更是否影響 Linux 流程。
3. 補上 simple server 的單元或整合測試（至少覆蓋多暫存器讀取/寫入），防止 buffer regression。
4. 若需更細粒度偵錯，可在整合腳本增加 Wait-Process/診斷輸出，或評估在 Windows runner 上手動重現以驗證環境差異。

## v0.1.1 (歷史紀錄)

### 更新內容
- `tests/integration/modbus_sim.py`: 增加 heartbeat 執行緒、signal logging 與 atexit hook，方便釐清 Windows 上 simulator 提早終止的觸發來源。
- 紀錄本機因 macOS sandbox 導致 socket bind `PermissionError`，暫時無法在本地重現 Windows 行為。

### CI 訊息
- Windows integration job 仍遭遇 `WinError 10061`；預期下一次 run 的 `modbus_sim_windows.log` 會額外輸出 signal/heartbeat 訊息，可用來對照 simulator 何時被終止。
- Ubuntu integration job 當時維持正常。

### 新發現
- 現有 log 僅看到正常 shutdown 訊息，推測 `serve_forever()` 收到 `SystemExit` 或類似訊號而結束，非自發例外；新增的 signal handler 與 heartbeat 可驗證此假設。
- 新環境變數 `MODBUS_SIM_HEARTBEAT`（秒）可調整 heartbeat 頻率，預設 15 秒，<=0 代表停用。
- 嘗試在 sandbox 環境啟動 simple server 時出現 `PermissionError`（macOS Seatbelt），後續需改在 Windows runner 觀察。

### 後續建議
1. 觀察下一次 Windows CI 的 `modbus_sim_windows.log/.err.log`，留意 heartbeat 與 signal 訊息是否顯示在 readiness 之後即被外部訊號終止。
2. 若確認是 step 結束造成的訊號，可試行以 PowerShell `Start-Job`、持續輪詢或切換到 pymodbus backend，避免背景程序被 job object 收斂。
3. 儘快在 Windows runner 上直接啟動 `python tests/integration/modbus_sim.py` 並以 telnet/python 測試連線，排除防火牆／權限問題。
4. 若 Python simulator 後續仍不穩定，評估改用 libmodbus C/C++ TCP server 作為 Windows 測試後端，保留 Python 模擬器作為 Linux fallback。

## v0.1.0 (歷史紀錄)

### 更新內容
- `tests/integration/modbus_sim.py` 新增 logging、強化 simple/pymodbus 伺服器的例外追蹤與 Windows event loop 設定。
- `.github/workflows/CI.yml` 調整 Windows simulator 步驟：恢復 simple server 預設、記錄 PID、診斷步驟加上 process dump，Python smoke test 改用暫存檔並允許失敗。
- `src/Export.cpp` 與 `CMakeLists.txt` 加入 Windows socket 錯誤診斷與 `ws2_32` 連結，提供 `modbus_connect` 詳細錯誤資訊。

### CI 訊息
- 目前 Windows integration job (`run 18690289652`) 仍在 `Test-NetConnection` 及 `modbus_connect` 階段出現 `WinError 10061`，`modbus_sim_windows.log` 只有啟動與一次連線後即關閉，顯示 simple server 在 readiness 後迅速離線。
- Ubuntu integration job 維持正常，確認 Linux pipeline 不受影響。

### 後續建議
1. 持續監控 simple server 的生命週期（例如加入 heartbeat 或改寫成自控 loop），避免 readiness 後立即結束。
2. 在 Windows runner 上直接啟動 `python tests/integration/modbus_sim.py` 並以 telnet/python 測試連線，檢查是否為環境層面的權限或防火牆問題。
3. 若 Python simulator 仍不穩定，評估改用 libmodbus C/C++ TCP server 作為 Windows 測試後端，保留 Python 模擬器作為 Linux fallback。

### 附註與快速參考
- 最近失敗的 Windows run：18690289652（Test-NetConnection `TcpTestSucceeded=False`，`modbus_connect` 超時，`WinError 10061`）。
- 模擬器 stdout/stderr 位於 `build/ci_logs/modbus_sim_windows.log(.err.log)`；若需更多細節可設定 `MODBUS_SIM_LOG=DEBUG` 後再執行。
- `MODBUS_SIM_PID` 會在 pipeline 診斷步驟中輸出，方便檢查 `Get-Process` 是否仍存在；目前觀察是 readiness 後即離線。
