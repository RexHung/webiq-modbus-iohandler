# tests/integration/modbus_sim.py
try:
    from pymodbus.server import StartTcpServer  # pymodbus >=3
except Exception:  # pragma: no cover
    from pymodbus.server.sync import StartTcpServer  # pymodbus 2.x

import importlib


def resolve(symbol, *candidates):
    for module_name in candidates:
        try:
            module = importlib.import_module(module_name)
            return getattr(module, symbol)
        except (ImportError, AttributeError):
            continue
    raise ImportError(f"Cannot resolve {symbol} from candidates: {candidates}")


ModbusServerContext = resolve(
    "ModbusServerContext",
    "pymodbus.datastore",
    "pymodbus.datastore.context",
    "pymodbus.datastore.server",
)
ModbusSlaveContext = resolve(
    "ModbusSlaveContext",
    "pymodbus.datastore",
    "pymodbus.datastore.context",
    "pymodbus.datastore.store",
)
ModbusSequentialDataBlock = resolve(
    "ModbusSequentialDataBlock",
    "pymodbus.datastore",
    "pymodbus.datastore.store",
    "pymodbus.datastore.context",
)
import os
import asyncio
import sys

if sys.platform.startswith("win"):
    asyncio.set_event_loop_policy(asyncio.WindowsSelectorEventLoopPolicy())

# 初始資料：
#  - coils[10] = 0 (FC1/5)     -> 測試寫入/讀取 coil.run
#  - holding[0] = 123          -> 測試 FC3 讀 holding.temp (之後0.1縮放=12.3)
#  - input[100,101] = float32  -> 測試 FC4 讀 ai.flow (我們預先放 1.5)
import struct

def float_to_regs(f):
    b = struct.pack('>f', f)  # big-endian float
    hi, lo = b[0]<<8 | b[1], b[2]<<8 | b[3]
    return [hi, lo]

store = ModbusSlaveContext(
    di=ModbusSequentialDataBlock(0, [0]*200),                        # discrete inputs
    co=ModbusSequentialDataBlock(0, [0]*200),                        # coils
    hr=ModbusSequentialDataBlock(0, [123] + [0]*199),                # holding registers
    ir=ModbusSequentialDataBlock(0, [0]*200),                        # input registers
    zero_mode=True
)
regs = float_to_regs(1.5)
store.setValues(4, 100, regs)  # FC=4 input registers at 100/101

context = ModbusServerContext(slaves=store, single=True)

def main():
    # 允許以環境變數 PORT 覆寫預設 1502，利於 CI 並行測試
    port = int(os.getenv("PORT", "1502"))
    print(f"Simulator starting on port {port}", flush=True)
    try:
        StartTcpServer(context=context, address=("0.0.0.0", port), allow_reuse_address=True)
    except TypeError:
        # 舊版 pymodbus 2.x 僅支援 positional context 參數
        StartTcpServer(context, address=("0.0.0.0", port), allow_reuse_address=True)

if __name__ == "__main__":
    main()
