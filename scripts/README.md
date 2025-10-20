# 一鍵檢查＋自動測試

## Linux/macOS 使用方式：
``` bash
chmod +x scripts/verify_layout_and_run.sh
```

預設 1602 埠
``` bash
./scripts/verify_layout_and_run.sh
```
或指定埠
``` bash
#PORT=1702 ./scripts/verify_layout_and_run.sh
```

## PowerShell 使用方式：
預設 1602 埠
``` bash
pwsh -File scripts/verify_layout_and_run.ps1
```
或指定埠
``` bash
$env:PORT=1702; pwsh -File scripts/verify_layout_and_run.ps1
```
## 小提醒
這兩支腳本會自動：
1. 檢查目錄結構
2. 建置 stub 模式並（可選）跑 CTest
3. 啟動模擬器（支援 PORT 覆寫）
4. 產生 config/ci.modbus.json
5. 建置 libmodbus + 測試
6. 執行 test_e2e 並輸出結果
7. 自動清理模擬器進程

## 常見補充
1) 用的是 Windows PowerShell 還是 PowerShell 7？
    - powershell：Windows 內建（5.1）
    - pwsh：PowerShell 7（你機器沒有，會報錯）
    > 想安裝 pwsh：從 Microsoft Store 或 GitHub Releases 安裝「PowerShell 7」。安裝後就能用 pwsh -File ...。

2) Python 找不到怎麼辦？
    腳本會用 python 啟動模擬器。若你是用 Windows Store/官方安裝器，通常已在 PATH。否則可以先測
    ```powershell
    python --version
    # 或
    py -3 --version

    ```
    若 python 找不到，可改用 py 啟動模擬器（臨時執行一次）：
    ```powershell
    $env:PORT=1602; py -3 tests/integration/modbus_sim.py
    ```
3) 路徑有空白（如 C:\Git Repositories\...）
    ```powershell
    powershell -File ".\scripts\verify_layout_and_run.ps1"

    ```
    如果你想改回用 PowerShell 7，安裝好之後用這條就行：
    ```powershell
    pwsh -NoProfile -ExecutionPolicy Bypass -File ".\scripts\verify_layout_and_run.ps1"
    ```
