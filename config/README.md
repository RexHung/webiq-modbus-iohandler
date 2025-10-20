# Modbus IOHandler Configuration

This folder contains example JSON configurations and a JSON Schema for validating configurations used by the WebIQ Modbus IOHandler.

- See `config/examples/tcp.modbus.json` for a Modbus TCP example.
- See `config/examples/rtu.modbus.json` for a Modbus RTU example.
- See `config/schema/modbus.schema.json` for a JSON Schema (Draft-07) that tools like `ajv` or `python-jsonschema` can use to validate configs.

Quick validation (Python):

- `python -m pip install jsonschema`
- `python - <<'PY'
import json, sys
from jsonschema import validate
schema=json.load(open('config/schema/modbus.schema.json'))
cfg=json.load(open('config/examples/tcp.modbus.json'))
validate(cfg, schema)
print('ok')
PY`

Notes
- `transport` is `tcp` or `rtu`.
- Coils/discrete functions (1,2,5,15) require `type: "bool"`.
- Register functions (3,4,6,16) must not use `type: "bool"`.
- 32-bit float uses two 16-bit registers; set `count: 2` and optionally `swap_words: true` when the device uses reversed word order.
- 64-bit double uses four 16-bit registers; `count: 4` is enforced. Use `word_order` to match device word order for 64-bit values:
  - `word_order`: one of `ABCD`, `BADC`, `CDAB`, `DCBA` (default `ABCD`).
  - For 32-bit float, `swap_words` remains applicable (two-register swap).
  - For 16-bit integer types, `count` can be 1 or an array (for FC16/FC3/FC4 bulk), but will not be treated as float even when `count: 2`.

Auto‑Reconnect Policy (top‑level `reconnect`)
- `retries` (int, >=0): number of reconnect attempts on NOT_CONNECTED. Default: 1.
- `interval_ms` (int, >=0): base delay before each reconnect attempt. Default: 0 ms.
- `backoff_multiplier` (number, >=1.0): exponential backoff multiplier. Default: 1.0.
- `max_interval_ms` (int, >=0): cap for backoff delay (0 = uncapped). Default: 0.

Recommended: `{ "retries": 3, "interval_ms": 500, "backoff_multiplier": 2.0, "max_interval_ms": 4000 }`.

Word Order Reference (double)
- ABCD: R0→A, R1→B, R2→C, R3→D
- BADC: R0→B, R1→A, R2→D, R3→C
- CDAB: R0→C, R1→D, R2→A, R3→B
- DCBA: R0→D, R1→C, R2→B, R3→A

Full config example (TCP)
```json
{
  "transport": "tcp",
  "tcp": { "host": "127.0.0.1", "port": 1502, "timeout_ms": 1000 },
  "items": [
    { "name": "coil.run",      "unit_id": 1, "function": 1,  "address": 10,  "type": "bool",   "poll_ms": 100 },
    { "name": "hr.temp_c",     "unit_id": 1, "function": 3,  "address": 0,   "type": "int16",  "scale": 0.1, "offset": 0.0, "poll_ms": 200 },
    { "name": "ai.flow_f",     "unit_id": 1, "function": 4,  "address": 100, "count": 2, "type": "float",  "swap_words": false, "poll_ms": 250 },
    { "name": "hr.energy_kwh", "unit_id": 1, "function": 3,  "address": 200, "count": 4, "type": "double", "word_order": "CDAB", "poll_ms": 500 }
  ]
}
```
