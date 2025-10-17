# tests/integration/modbus_sim.py
from pymodbus.server import StartTcpServer
from pymodbus.datastore import ModbusSlaveContext, ModbusServerContext
from pymodbus.datastore import ModbusSequentialDataBlock
from threading import Thread

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
    StartTcpServer(context, address=("0.0.0.0", port))

if __name__ == "__main__":
    main()
