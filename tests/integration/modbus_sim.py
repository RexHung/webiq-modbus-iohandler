# tests/integration/modbus_sim.py
try:
    from pymodbus.server import StartTcpServer  # pymodbus >=3
except Exception:  # pragma: no cover
    from pymodbus.server.sync import StartTcpServer  # pymodbus 2.x

import importlib
import pkgutil


_RESOLVE_CACHE = {}


# Fallback scan handles reorganized modules introduced in newer pymodbus releases.
def _scan_for_symbol(symbol, *packages):
    if symbol in _RESOLVE_CACHE:
        return _RESOLVE_CACHE[symbol]

    for package_name in packages:
        try:
            package = importlib.import_module(package_name)
        except ImportError:
            continue

        to_inspect = [package.__name__]

        if hasattr(package, "__path__"):
            for modinfo in pkgutil.walk_packages(package.__path__, package.__name__ + "."):
                to_inspect.append(modinfo.name)

        for module_name in to_inspect:
            try:
                module = importlib.import_module(module_name)
            except Exception:
                continue

            try:
                value = getattr(module, symbol)
            except AttributeError:
                continue

            _RESOLVE_CACHE[symbol] = value
            return value

    raise ImportError(f"Cannot resolve {symbol} from packages: {packages}")


def resolve(symbol, *candidates):
    for module_name in candidates:
        try:
            module = importlib.import_module(module_name)
            return getattr(module, symbol)
        except (ImportError, AttributeError):
            continue

    return _scan_for_symbol(symbol, "pymodbus.datastore", "pymodbus.server", "pymodbus")


ModbusServerContext = resolve(
    "ModbusServerContext",
    "pymodbus.datastore",
    "pymodbus.datastore.context",
    "pymodbus.datastore.server",
)
try:
    ModbusSlaveContext = resolve(
        "ModbusSlaveContext",
        "pymodbus.datastore",
        "pymodbus.datastore.context",
        "pymodbus.datastore.store",
    )
except ImportError:
    _DeviceContext = resolve(
        "ModbusDeviceContext",
        "pymodbus.datastore",
        "pymodbus.datastore.context",
    )

    class ModbusSlaveContext(_DeviceContext):
        """Compatibility shim for pymodbus 3.11+ removals."""

        def __init__(self, *args, zero_mode=False, **kwargs):
            self.zero_mode = kwargs.pop("zero_mode", zero_mode)
            super().__init__(*args, **kwargs)

        def getValues(self, func_code, address, count=1):  # type: ignore[override]
            if self.zero_mode:
                block = self.store[self.decode(func_code)]
                return block.getValues(address, count)
            return super().getValues(func_code, address, count)

        def setValues(self, func_code, address, values):  # type: ignore[override]
            if self.zero_mode:
                block = self.store[self.decode(func_code)]
                return block.setValues(address, values)
            return super().setValues(func_code, address, values)

        def validate(self, func_code, address, count=1):
            if self.zero_mode:
                block = self.store[self.decode(func_code)]
                validator = getattr(block, "validate", None)
                if validator:
                    return validator(address, count)
                return True
            parent_validate = getattr(super(), "validate", None)
            if parent_validate:
                return parent_validate(func_code, address, count)
            return True

ModbusSequentialDataBlock = resolve(
    "ModbusSequentialDataBlock",
    "pymodbus.datastore",
    "pymodbus.datastore.store",
    "pymodbus.datastore.context",
    "pymodbus.datastore.sequential",
)
import os
import asyncio
import sys

try:
    import pymodbus as _pymodbus_mod  # type: ignore
    _PYMODBUS_MAJOR = int(str(getattr(_pymodbus_mod, "__version__", "0")).split(".")[0])
except Exception:  # pragma: no cover - defensive fallback
    _PYMODBUS_MAJOR = 0

if sys.platform.startswith("win") and _PYMODBUS_MAJOR < 3:
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

try:
    context = ModbusServerContext(slaves=store, single=True)
except TypeError:
    context = ModbusServerContext(devices=store, single=True)

def _trace_connect(connected):
    state = "connected" if connected else "disconnected"
    print(f"Client {state}", flush=True)


def main():
    # 允許以環境變數 PORT 覆寫預設 1502，利於 CI 並行測試
    port = int(os.getenv("PORT", "1502"))
    host = os.getenv("HOST", "127.0.0.1")
    print(f"Simulator starting on {host}:{port}", flush=True)
    kwargs = {"context": context, "address": (host, port), "trace_connect": _trace_connect}
    try:
        StartTcpServer(**kwargs, allow_reuse_address=True)
    except TypeError as exc:
        if "trace_connect" in str(exc):
            kwargs.pop("trace_connect", None)
            StartTcpServer(**kwargs, allow_reuse_address=True)
            return
        if "allow_reuse_address" in str(exc):
            StartTcpServer(**kwargs)
            return

        # 舊版 pymodbus 2.x 僅支援 positional context 參數
        try:
            StartTcpServer(context, address=(host, port), allow_reuse_address=True)
        except TypeError:
            StartTcpServer(context, address=(host, port))

if __name__ == "__main__":
    main()
