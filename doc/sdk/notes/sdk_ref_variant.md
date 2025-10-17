# WebIQ Variant Value – 快速對照（供 ioHandler 開發）

> 目的：幫助把 **Modbus 資料型別** 與 **WebIQ 變數值容器**（variant）對齊，
> 用於 `ReadItem`/`WriteItem`、訂閱比對（變更才推送）、與 encode/decode。

> **來源對照**：請在本機對照官方 SDK 中的 `variant_value*.h`
> （例如：`variant_value.h`, `variant_value_compare.h`, `variant_value_helpers.h`）。
> 本文件暫以常見設計推測欄位/介面，**實作前請依實際標頭替換**（見每段 `TODO:`）。

---

## 1) 名詞定義

* **VariantValue**：WebIQ/SHMI 的通用值容器，能裝 `bool`、整數、浮點、字串、位元組等。
  常見用途：HMI 變數值、ioHandler 的回傳值與寫入值承載器。
* **ItemValue / IoValue**：ioHandler 對外交換的值型別（**可能** 就是 `VariantValue` 或含 `VariantValue` 的結構）。

> `TODO:` 對照實際標頭中：
>
> * 型別名稱是否為 `variant_value` / `shmi::variant_value` / `Variant`？
> * 是否存在 `variant_value_helpers.h` 提供 `to_*` / `from_*` 輔助？

---

## 2) 常見型別對照（Modbus ↔ Variant）

| Modbus 資料源讀取功能碼原始寬度變換後（Variant）備註 |            |        |                                                   |                      |
| --------------------------------- | ---------- | ------ | ------------------------------------------------- | -------------------- |
| Coil (0xxxx)                      | FC01/05/15 | 1 bit  | `bool`                                            | `true/false`         |
| Discrete Input (1xxxx)            | FC02       | 1 bit  | `bool`                                            | 唯讀                   |
| Holding Register (4xxxx)          | FC03/06/16 | 16 bit | `int16` / `uint16` / `int32` / `uint32` / `float` | 32-bit 需兩個 16-bit 組字 |
| Input Register (3xxxx)            | FC04       | 16 bit | 同上                                                | 唯讀                   |

> `TODO:` 確認 `VariantValue` 是否支援 `int32/uint32/float` 直接型別；若無，改用 `bytes`/`string` 或 `double` 承載。

---

## 3) 編解碼規則（32-bit / 浮點）

* **字序**：預設 **Big-endian word order**（先高位字），可透過 `swap_words` 切換。
* **整數**：

  * `int32/uint32` 以兩個 `uint16` 組裝。
* **浮點**：

  * 預設 IEEE-754 float（32-bit），以兩個 `uint16` 組裝。
* **縮放**：`scaled = raw * scale + offset`（讀取）；寫入需反向：`raw = round((value - offset) / scale)`。

---

## 4) API 草擬（比照 helpers – 請對照實際標頭）

```cpp
// TODO: 依實際 SDK 命名調整命名空間與函式簽名
namespace sdk {

// 檢查類型並安全取值
bool as_bool(const VariantValue& v, bool& out);
bool as_int16(const VariantValue& v, int16_t& out);
bool as_uint16(const VariantValue& v, uint16_t& out);
bool as_int32(const VariantValue& v, int32_t& out);     // 可能需要 from two u16
bool as_uint32(const VariantValue& v, uint32_t& out);
bool as_float(const VariantValue& v, float& out);

// 建立 Variant
VariantValue make_bool(bool b);
VariantValue make_int16(int16_t x);
VariantValue make_uint16(uint16_t x);
VariantValue make_int32(int32_t x);
VariantValue make_uint32(uint32_t x);
VariantValue make_float(float f);

} // namespace sdk

```
> TODO: 若官方已提供 variant_value_helpers.h 的 to_* / from_*，請直接使用，不必重造。

## 5) 16-bit 陣列 ↔ 32-bit/float 組裝

```cpp
// 組裝 2 x uint16 → uint32 / float
inline uint32_t join_u32(uint16_t hi, uint16_t lo, bool swap_words) {
    return swap_words ? (uint32_t(lo) << 16) | hi
                      : (uint32_t(hi) << 16) | lo;
}

inline void split_u32(uint32_t v, bool swap_words, uint16_t& hi, uint16_t& lo) {
    uint16_t h = uint16_t((v >> 16) & 0xFFFF);
    uint16_t l = uint16_t(v & 0xFFFF);
    if (swap_words) { hi = l; lo = h; } else { hi = h; lo = l; }
}

inline float u32_to_float(uint32_t u) {
    static_assert(sizeof(float) == 4, "unexpected");
    float f;
    std::memcpy(&f, &u, 4);
    return f;
}

inline uint32_t float_to_u32(float f) {
    uint32_t u;
    std::memcpy(&u, &f, 4);
    return u;
}
```

## 6) 縮放與偏移

```cpp
inline double apply_scale(double raw, double scale, double offset) {
    return raw * scale + offset;
}

inline double unscale(double scaled, double scale, double offset) {
    if (scale == 0.0) return 0.0; // or error
    return (scaled - offset) / scale;
}

```

## 7) 讀寫範例（Variant ↔ Modbus）

### 7.1 讀取 Holding Register（`int16` 帶縮放）
```cpp
// 原始 registers[addr] = 123, scale=0.1 → 12.3
uint16_t raw = regs[addr];
double scaled = apply_scale(int16_t(raw), 0.1, 0.0);
VariantValue v = sdk::make_float(static_cast<float>(scaled)); // 或 make_double/number
```
### 7.2 讀取 Input Registers（float 兩字組裝）
```cpp
uint16_t hi = regs[addr];
uint16_t lo = regs[addr+1];
uint32_t u = join_u32(hi, lo, /*swap_words*/ false);
float f = u32_to_float(u);
VariantValue v = sdk::make_float(f);
```
### 7.3 寫入 Coil（bool）
```cpp
bool b{};
if (!sdk::as_bool(inputVariant, b)) /* error */;
uint16_t coil = b ? 0xFF00 : 0x0000; // libmodbus 規定
// → 發送 FC05
```
### 7.4 寫入 Holding（int16 帶縮放）
```cpp
double val{};
if (!sdk::as_float(inputVariant, reinterpret_cast<float&>(val))) {
    // 或 as_number / as_int16 視 SDK 介面
}
int32_t raw = (int32_t)std::llround(unscale(val, scale, offset));
uint16_t reg = (uint16_t)(int16_t)raw;
// → 發送 FC06
```
### 7.5 寫入 Holding（float 兩字）
```cpp
float f{};
if (!sdk::as_float(inputVariant, f)) /* error */;
uint32_t u = float_to_u32(f);
uint16_t hi, lo;
split_u32(u, /*swap_words*/ false, hi, lo);
// → 發送 FC16（2 字）
```

## 8) 變更偵測（只在值變動時推送）

> 推薦採用 原始 bytes 層級 比對，可避免浮點誤差問題。
```cpp
struct CacheEntry {
    std::vector<uint8_t> lastBytes; // 上次推送的序列化結果
};

bool has_changed(const std::vector<uint8_t>& now, CacheEntry& c) {
    if (c.lastBytes != now) { c.lastBytes = now; return true; }
    return false;
}
```

**序列化策略（建議）**：
* `bool` → 1B（0/1）
* `int16/uint16` → 2B（big-endian）
* `int32/uint32/float` → 4B（big-endian，已處理 word order）

## 9) 錯誤模型（摘要）

| 類別     | 典型代碼                 | 說明 / 建議處理                 |
| ------ | -------------------- | ------------------------- |
| 類型不匹配  | `ERR_TYPE_MISMATCH`  | `Variant` 取值失敗或寫入型別不支援    |
| 解析錯誤   | `ERR_DECODE`         | 32-bit 組裝、縮放失敗（scale=0 等） |
| 通訊錯誤   | `ERR_IO`             | TCP/RTU 逾時、連線失敗，支援重試      |
| 重複訂閱   | `ERR_DUP_SUB`        | 同一 item 重複不同輪詢參數          |
| 不支援功能碼 | `ERR_FC_UNSUPPORTED` | 試圖對該類型使用不支援的 FC           |

> TODO: 對照你的 ModbusIoHandler 內部 enum class ErrorCode 或返回約定，統一訊息字串。

## 10) 單元測試建議（最小集）

* `join_u32/split_u32` 與 `u32_to_float/float_to_u32` 往返一致
* `apply_scale/unscale` 可逆（含 scale=1/offset=0、負數、邊界）
* `bool/int16/float` 各型 encode/decode 成 `VariantValue` 與還原
* `swap_words=true/false` 的 32-bit 一致性
* 變更偵測：同值不推送、異值推送

## 11) 與 JSON 組態對齊（摘錄）

```json
{
  "items": [
    { "name": "coil.run",      "unit_id": 1, "function": 1,  "address": 10,  "type": "bool",  "poll_ms": 200 },
    { "name": "holding.temp",  "unit_id": 1, "function": 3,  "address": 0,   "type": "int16", "scale": 0.1, "poll_ms": 200 },
    { "name": "ai.flow",       "unit_id": 1, "function": 4,  "address": 100, "type": "float", "swap_words": false }
  ]
}

```

> 讀寫流程請依 type、function 採用對應 encode/decode，並在 ReadItem/WriteItem 內呼叫本文件的轉換輔助。

## 12) TODO 對照清單（導入前請逐一核對）

* `VariantValue` **實際型別名稱** 與命名空間
* 取值/建值 helper **實際函式**（若 SDK 已提供）
* 內建是否支持 `int32/uint32/float`，或需以 `bytes/double` 承載
* 32-bit 字序（word order）預設是 big-endian？（官方範例如何處理）
* 你的 `ErrorCode` 枚舉與訊息對照
* 測試：以 `pymodbus` 模擬器驗證 FC1/3/4/5/6/16

---

### 放置位置建議

```bash
doc/sdk/notes/sdk_ref_variant.md          ← 本檔
doc/sdk/ioHandlerSDK/include/variant_value*.h  ← 官方標頭（節選，無二進位）
```

---

###  commit 訊息

```scss
docs(sdk): add VariantValue quick reference and Modbus↔Variant mapping
```