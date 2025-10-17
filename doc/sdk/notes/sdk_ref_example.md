# SDK Reference – `ioHandlerExample` 對照我們的 C API

> 來源：`doc/sdk/ioHandlerSDK/examples/example.h`（官方 SDK 範例，© Smart HMI GmbH）
> 目的：協助將官方範例類別 `ioHandlerExample` 的設計，對齊我們的 **Modbus ioHandler 擴充**實作（C API：Create/Destroy/Subscribe/Unsubscribe/Read/Write/CallMethod）。

## 1) 快速對照表

| 官方範例 (`example.h`) 我們的 C API / 行為 關鍵重點                            |                                                         |                                                                                                               |
| ----------------------------------------------------------------- | ------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------- |
| `ioHandlerExample::ioHandlerExample(const ioHandlerParam* param)` | `CreateIoInstance(void* param, const char* jsonConfig)` | 由 WebIQ 傳入的 `param`/callback 設定 → 我方解析 `jsonConfig`，建立 `ModbusIoHandler`、啟動通訊/輪詢執行緒。                          |
| `~ioHandlerExample()` / `terminate()`                             | `DestroyIoInstance(handle)`                             | 終止輪詢、喚醒與 join 背景執行緒（simulation/monitor 類比我們的 poller），釋放連線/資源。                                                 |
| `subscribe(interval, symbols, types)`（兩種簽名）                       | `SubscribeItems(handle, names[], count)`                | 綁定 item 到刷新群組（範例是 `SubscriptionGroup`），我們以 `poll_ms` 聚類；必要時建立新 poller；`types` 需回報型別（不確定時 `VAR_UNDEFINED` 類似）。 |
| `unsubscribe(symbols)`（兩種簽名）                                      | `UnsubscribeItems(handle, names[], count)`              | 從所有輪詢群組移除；若群組空了則釋放；同名重複訂閱/退訂行為需明確（見錯誤模型）。                                                                     |
| `read(symbol)`                                                    | `ReadItem(handle, name, outBuf, outSize)`               | 同步即時讀取：範例直接把值送回 WebIQ；我們需**透過 outBuf 回傳**或**呼叫 readCallback**（依 SDK 簽名）；不要求已訂閱才可讀。                            |
| `write(symbol, ioDataValue value)`                                | `WriteItem(handle, name, valueJson)`                    | 依 item 型別/FC 編碼後發送（Coils: FC5/15；Holding: FC6/16）；成功/失敗回報 `writeResult`。                                      |
| `method(context, methodName, jsonArgs)`                           | `CallMethod(handle, name, argsJson)`                    | 自訂功能；預設回傳 `false`/非 0；可保留擴充點（如診斷/重連）。                                                                         |
| `thread_proc_simulation()`                                        | （無直接對應；僅相當於資料來源）                                        | 範例是模擬器；我們資料來源為 **Modbus** → 將此邏輯替換為輪詢 PLC 地址與變更推送。                                                            |
| `thread_proc_monitor()`                                           | **變更檢測與批次推送**                                           | 範例為「監看變更並推送」；我們同樣採 raw-bytes 去抖動策略（變更才上報）。                                                                    |
| `get_subscription_group(interval)`                                | **poller 分群**                                           | 與我們的 `poll_ms` → poller pool 對齊：同 `poll_ms` → 同一個 poll thread。                                                |
| `get_item_value(name)`                                            | **item cache**                                          | 對應我們的 item→lastBytes/lastValue 快取（供變更比較、同步讀寫）。                                                                |
> 註：官方 example.h 以 C++ 類別呈現；我們輸出的是 C 介面 + 內部 C++ 類別。對接時，C 匯出（Export.cpp）只做薄包裝，真正邏輯在 ModbusIoHandler 類別。

## 2) 資料模型對齊

### 範例側（SDK）

* `items_ : unordered_map<string, shmi::variant_value>`：所有變數值的儲存（變更比對/回推）。
* `SubscriptionGroup`：相同刷新區間的變數集合，由 monitor thread 統一驅動。

### 我方側（Modbus）

* `ItemBinding`：

  * `name, unit_id, function (FC), address`
  * `type (bool/i16/u16/i32/u32/float)`, `scale`, `offset`, `swap_words`, `poll_ms`
* `Poller`（by `poll_ms`）：

  * 擁有一批 `ItemBinding`；定期批讀/分段讀取（FC1/2/3/4），解碼→變更檢測→回推
* `CacheEntry`：

  * `lastBytes`（raw 序列化結果），`lastVariant`（選擇性）

## 3) 生命週期（建立/銷毀）

**Create → 運行 → 終止** 對齊：

1. `CreateIoInstance(param, jsonConfig)`

   * 解析 JSON：選 TCP/RTU、建 `IModbusClient`、建立 poller(s)
   * 註冊回呼、日誌器
2. `SubscribeItems` / `UnsubscribeItems`

   * 更新 poller 內容、起停 poller thread
3. `DestroyIoInstance`

   * `terminate()`：`terminate_ = true` → `cond*.notify_all()` → join threads → close client

**驗證要點**：Destroy 之後不再有任何回呼；資源（通訊/執行緒）釋放完整。

## 4) 變更偵測與推送策略（對齊 monitor thread）

* 範例：`thread_proc_monitor()` 會等待需要更新的 `SubscriptionGroup`，按需推送。
* 我們：每個 poller thread 在一次批讀後，對每個 item 執行 **raw bytes** 比對：

  * 若不同 → 序列化 → 觸發回推
  * 若相同 → 不推送，並動態調整 sleep（由 `poll_ms` 決定）

**建議序列化**：

* `bool` → 1B
* `int16/uint16` → 2B big-endian
* `int32/uint32/float` → 4B big-endian（已處理 word swap）

## 5) Read/Write 對齊（行為準則）

### Read（同步）

* **不必已訂閱**；直接發出單點讀（FC1/2=bit；FC3/4=reg）→ decode → 回傳/回呼
* 若為 32-bit/float：按 `swap_words` 組裝
* `scale/offset`：讀取時 **套用**；寫入時 **反向套用**

### Write（FC5/6/15/16）

* `bool` → FC5/15；線圈值按 libmodbus 慣例 `0xFF00/0x0000`
* `int16/uint16` → FC6（單一 reg）
* `int32/uint32/float` → FC16（兩個 reg），注意 word order 與 encode

**錯誤模型**（建議對齊範例語義）：

* `ERR_DUP_SUB`：重複訂閱同 item 且參數不同
* `ERR_TYPE_MISMATCH`：寫入型別不符
* `ERR_FC_UNSUPPORTED`：對該型別使用不支援 FC
* `ERR_IO`：通訊逾時/斷線（可自動重試 n 次）
* `ERR_DECODE`：解碼/縮放失敗（scale=0, 超界等）

## 6) 對應程式骨架（偽碼）

```cpp
// Export.cpp ─ C 介面（薄包裝）
IoHandle CreateIoInstance(void* param, const char* jsonCfg) {
  auto h = new ModbusIoHandler(param, jsonCfg);   // 解析、建立 client、準備 pollers
  h->start();                                     // 若需立即啟動
  return h;
}
int SubscribeItems(IoHandle h, const char** names, int n) {
  return static_cast<ModbusIoHandler*>(h)->subscribe(names, n);
}
int ReadItem(IoHandle h, const char* name, char* out, int outSz) {
  return static_cast<ModbusIoHandler*>(h)->read(name, out, outSz);   // 單點即時讀取
}
// ... WriteItem / UnsubscribeItems / CallMethod / Destroy...
```
```cpp
// ModbusIoHandler::pollerLoop(poll_ms)
for (;;) {
  wait_until(next_tick);
  if (terminate_) break;

  auto batch = planBatchFc3Fc4(fcGroups);   // 依地址連續區間最小化讀取次數
  for (auto& op : batch) {
    auto regs = client.read(op);            // libmodbus 或 stub
    for (auto& item : op.items) {
      auto value = decode(regs, item);      // 16→32/float + scale/offset
      auto bytes = serialize(value, item);  // raw bytes for change detection
      if (has_changed(bytes, cache[item.name])) {
        pushUpdate(item.name, value);       // 只在變更才推送
      }
    }
  }
}
```

## 7) 最小測試建議（對照官方範例行為）

* **生命週期**：Create → Subscribe → Read → Write → Unsubscribe → Destroy，無資源泄漏
* **讀寫**：

  * `coil.run`：`false → true → false`；readback 正確
  * `holding.temp`：raw=123、`scale=0.1` → 12.3（讀），寫入 25.0 → raw=250（FC06）
  * `ai.flow`：`float` 1.5 組裝/還原一致（含 `swap_words` 兩路）
* **變更偵測**：連續讀兩次同值只推送一次；寫入後推送一次
* **錯誤**：不支援 FC / 型別不符 / 連線失敗 → 明確錯誤碼
* **退訂**：退訂後不再推播；重複訂閱同 item 不覆蓋但回報

## 8) 導入與放置

* 官方檔案：`doc/sdk/ioHandlerSDK/examples/example.h`
  （若有 `example.cpp`、`SubscriptionGroup.h/.cpp` 也放同資料夾）
* 我方筆記：本檔 `doc/sdk/notes/sdk_ref_example.md`
* 在 `plan.md` 中引用本檔：「External Reference → SDK Example Mapping」

**建議 commit 訊息**

```
docs(sdk): add example.h mapping to our C API and behavior (subscribe/read/write/lifecycle)
```

## 9) 注意（授權與安全）

* 只納入 **原始碼文字檔**（.h / .cpp）；不含 .dll/.so/.lib 或專案檔
* 保留原檔頭版權聲明
* 若 repository 將公開，建議在 `doc/sdk/README.md` 標註「僅供內部參考」
