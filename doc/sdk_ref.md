# SDK API Quick Reference

| Function             | Purpose                               | Notes / Our mapping                    |
|---------------------|----------------------------------------|----------------------------------------|
| CreateIoInstance    | Parse JSON, create handler instance    | 連結 IModbusClient (TCP/RTU)           |
| DestroyIoInstance   | Stop polling, free resources           | join threads, close sockets/ports      |
| SubscribeItems      | Bind item→(unit, fc, addr, type, …)    | 建立輪詢表與去重策略                   |
| UnsubscribeItems    | Remove item bindings                    | 若為最後一項可縮減輪詢負載             |
| ReadItem            | Sync read now                          | 直接 read，一律走 client               |
| WriteItem           | FC5/6/15/16                            | 編碼 type → coils/registers            |
| CallMethod          | Reserved/custom                         | 先回 false                             |

Types: bool, int16/uint16, int32/uint32, float; scale/offset; 32-bit word order; default big-endian.

- variant_value.h：定義一個通用數值容器（類似 QVariant / std::variant）。
- variant_value_compare.h：比較運算工具。
- variant_value_helpers.h：型別轉換、操作輔助。