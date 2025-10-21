# Codex Context Log v0.1.0

## 更新內容
- `tests/integration/modbus_sim.py` 新增 logging、強化 simple/pymodbus 伺服器的例外追蹤與 Windows event loop 設定。
- `.github/workflows/CI.yml` 調整 Windows simulator 步驟：恢復 simple server 預設、記錄 PID、診斷步驟加上 process dump，Python smoke test 改用暫存檔並允許失敗。
- `src/Export.cpp` 與 `CMakeLists.txt` 加入 Windows socket 錯誤診斷與 `ws2_32` 連結，提供 `modbus_connect` 詳細錯誤資訊。

## CI 訊息
- 目前 Windows integration job (`run 18690289652`) 仍在 `Test-NetConnection` 及 `modbus_connect` 階段出現 `WinError 10061`，`modbus_sim_windows.log` 只有啟動與一次連線後即關閉，顯示 simple server 在 readiness 後迅速離線。
- Ubuntu integration job 維持正常，確認 Linux pipeline 不受影響。

## 後續建議
1. 持續監控 simple server 的生命週期（例如加入 heartbeat 或改寫成自控 loop），避免 readiness 後立即結束。
2. 在 Windows runner 上直接啟動 `python tests/integration/modbus_sim.py` 並以 telnet/python 測試連線，檢查是否為環境層面的權限或防火牆問題。
3. 若 Python simulator 仍不穩定，評估改用 libmodbus C/C++ TCP server 作為 Windows 測試後端，保留 Python 模擬器作為 Linux fallback。

## 附註與快速參考
- 最近失敗的 Windows run：18690289652（Test-NetConnection `TcpTestSucceeded=False`，`modbus_connect` 超時，`WinError 10061`）。
- 模擬器 stdout/stderr 位於 `build/ci_logs/modbus_sim_windows.log(.err.log)`；若需更多細節可設定 `MODBUS_SIM_LOG=DEBUG` 後再執行。
- `MODBUS_SIM_PID` 會在 pipeline 診斷步驟中輸出，方便檢查 `Get-Process` 是否仍存在；目前觀察是 readiness 後即離線。
