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
