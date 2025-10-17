# RTU Stub 指南（無硬體環境）

目的：在 `WITH_LIBMODBUS=OFF` 或 CI 缺少 RS‑485/COM 裝置時，仍能以單元測試驗證 RTU 路徑的型別轉換、縮放/偏移、word order、與錯誤行為。

## 設計建議

- 記憶體模型：
  - `co`: 0/1 陣列（coils）
  - `di`: 0/1 陣列（discrete inputs）
  - `hr`: `uint16_t[]`（holding registers）
  - `ir`: `uint16_t[]`（input registers）
- 時序與逾時：以 `set_timeout_ms()` 設定 deadline；超時則回 `IO_TIMEOUT`。
- 錯誤注入：支援下列模式以驗證重試策略：
  - `once_timeout(fc, addr)`、`n_errors(fc, n)`、`crc_error(fc)`（以泛用 `IO_ERROR` 表示）。
- 功能碼：完整覆蓋 FC1/2/3/4/5/6/15/16，含邊界檢查與批次長度限制。

## 測試最小集

- 32‑bit/float 的 `join_u32/split_u32` 與 `swap_words` 組裝/還原一致。
- `apply_scale/unscale` 可逆；`scale=0` 應回 `PARSE_ERROR`。
- 讀寫 bool/int16/float 的 Variant ↔ 寄存器/線圈編解碼往返。
- 重試：注入一次超時，確認第二次成功（指數退避／步進退避）。

## 參考

- `doc/sdk/ioHandlerSDK/include/ioHandler.h`（日誌、回呼、訊號定義）
- `doc/sdk/ioHandlerSDK/include/variant_value.h`（Variant 型別）
- `doc/sdk/notes/sdk_ref_variant.md`（型別轉換與範例）
