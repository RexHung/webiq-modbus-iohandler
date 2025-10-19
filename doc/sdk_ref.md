# SDK API Quick Reference

| Function             | Purpose                               | Notes / Our mapping                              |
|---------------------|----------------------------------------|--------------------------------------------------|
| CreateIoInstance    | Parse JSON, create handler instance    | Select TCP/RTU, build IModbusClient, validate    |
| DestroyIoInstance   | Stop polling, free resources           | Join threads, close sockets/ports                |
| SubscribeItems      | Bind items                             | No-op today (reserved for future pollers)        |
| UnsubscribeItems    | Unbind items                           | No-op                                            |
| ReadItem            | Synchronous read                       | FC1/2/3/4; float/double packing; auto-reconnect  |
| WriteItem           | Synchronous write                      | FC5/6/15/16; type-safe encode; auto-reconnect    |
| CallMethod          | Reserved/custom                        | connection.reconnect, logger.set (stubs)         |

Types and packing
- bool, int16/uint16, int32/uint32, float, double; `scale`/`offset` for numeric types
- float (32-bit): 2 x 16-bit registers; `swap_words` toggles word swap
- double (64-bit): 4 x 16-bit registers; `word_order` in {ABCD,BADC,CDAB,DCBA}
