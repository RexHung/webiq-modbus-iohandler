# tests/integration/modbus_sim.py
"""Standalone Modbus TCP simulator for integration tests.

The script offers two backends:
- A pure-Python minimal server (default on Windows) that implements the small
  subset of Modbus functions required by the test suite (FC1/3/4/5/6).
- The original pymodbus-powered simulator (kept for Linux compatibility and as a
  fallback when the minimal server is disabled via SIMPLE_MODBUS=0).
"""

from __future__ import annotations

import asyncio
import os
import struct
import sys
import threading
import traceback

# Decide which backend to use (default to the simple server on Windows runners).
_USE_SIMPLE = os.getenv("SIMPLE_MODBUS")
if _USE_SIMPLE is None:
    _USE_SIMPLE = "1" if sys.platform.startswith("win") else "0"
USE_SIMPLE = _USE_SIMPLE == "1"


def float_to_regs(value: float) -> list[int]:
    """Convert a float into two big-endian Modbus registers."""
    hi, lo = struct.unpack(">HH", struct.pack(">f", value))
    return [hi, lo]


# Shared backing store initialised with the values expected by the tests.
COILS = [0] * 200
DISCRETE = [0] * 200
HOLDING = [123] + [0] * 199
INPUT = [0] * 200
INPUT[100], INPUT[101] = float_to_regs(1.5)
STORE_LOCK = threading.Lock()


def _exception(fc: int, code: int) -> bytes:
    return bytes([(fc | 0x80) & 0xFF, code & 0xFF])


if USE_SIMPLE:
    import socketserver

    class ModbusTCPHandler(socketserver.BaseRequestHandler):
        """Tiny Modbus TCP handler supporting the opcodes used in tests."""

        def setup(self) -> None:  # pragma: no cover - exercised in CI
            self.request.settimeout(5.0)
            host, port = self.client_address
            print(f"Client connected from {host}:{port}", flush=True)

        def finish(self) -> None:  # pragma: no cover - exercised in CI
            host, port = self.client_address
            print(f"Client disconnected from {host}:{port}", flush=True)

        def _recv_exact(self, size: int) -> bytes | None:
            buffer = bytearray()
            while len(buffer) < size:
                chunk = self.request.recv(size - len(buffer))
                if not chunk:
                    return None
                buffer.extend(chunk)
            return bytes(buffer)

        def handle(self) -> None:  # pragma: no cover - exercised in CI
            try:
                while True:
                    header = self._recv_exact(7)
                    if not header:
                        break
                    transaction_id = int.from_bytes(header[0:2], "big")
                    length = int.from_bytes(header[4:6], "big")
                    unit_id = header[6]
                    if length < 1:
                        continue
                    pdu = self._recv_exact(length - 1)
                    if not pdu:
                        break
                    response_pdu = build_simple_response(pdu)
                    response = struct.pack(
                        ">HHHB", transaction_id, 0, len(response_pdu) + 1, unit_id
                    ) + response_pdu
                    try:
                        self.request.sendall(response)
                    except OSError:
                        break
            except Exception:  # pragma: no cover
                print("Simple server handler error:", flush=True)
                traceback.print_exc()

    class SimpleModbusServer(socketserver.ThreadingTCPServer):  # pragma: no cover
        allow_reuse_address = True
        daemon_threads = True

        def handle_error(self, request, client_address):
            print(f"Simple server handle_error for {client_address}", flush=True)
            traceback.print_exc()

    def build_simple_response(pdu: bytes) -> bytes:
        fc = pdu[0]
        data = pdu[1:]

        if fc == 1:  # Read Coils
            if len(data) < 4:
                return _exception(fc, 0x03)
            address, count = struct.unpack(">HH", data[:4])
            if count < 1 or address + count > len(COILS):
                return _exception(fc, 0x02)
            with STORE_LOCK:
                bits = COILS[address : address + count]
            byte_count = (count + 7) // 8
            payload = bytearray(byte_count)
            for idx, bit in enumerate(bits):
                if bit:
                    payload[idx // 8] |= 1 << (idx % 8)
            return bytes([fc, byte_count]) + bytes(payload)

        if fc in (3, 4):  # Read Holding/Input Registers
            if len(data) < 4:
                return _exception(fc, 0x03)
            address, count = struct.unpack(">HH", data[:4])
            registers = HOLDING if fc == 3 else INPUT
            if count < 1 or address + count > len(registers):
                return _exception(fc, 0x02)
            with STORE_LOCK:
                values = registers[address : address + count]
            payload = bytearray(1 + count * 2)
            payload[0] = fc
            payload[1] = count * 2
            for idx, value in enumerate(values):
                payload[2 + idx * 2] = (value >> 8) & 0xFF
                payload[3 + idx * 2] = value & 0xFF
            return bytes(payload)

        if fc == 5:  # Write Single Coil
            if len(data) < 4:
                return _exception(fc, 0x03)
            address, value = struct.unpack(">HH", data[:4])
            if address >= len(COILS):
                return _exception(fc, 0x02)
            if value == 0xFF00:
                bit = 1
            elif value == 0x0000:
                bit = 0
            else:
                return _exception(fc, 0x03)
            with STORE_LOCK:
                COILS[address] = bit
            return bytes([fc]) + data[:4]

        if fc == 6:  # Write Single Holding Register
            if len(data) < 4:
                return _exception(fc, 0x03)
            address, value = struct.unpack(">HH", data[:4])
            if address >= len(HOLDING):
                return _exception(fc, 0x02)
            with STORE_LOCK:
                HOLDING[address] = value & 0xFFFF
            return bytes([fc]) + data[:4]

        return _exception(fc, 0x01)

    def run_simple_server(host: str, port: int) -> None:
        print(f"Simple Modbus server starting on {host}:{port}", flush=True)
        try:
            with SimpleModbusServer((host, port), ModbusTCPHandler) as server:
                try:
                    server.serve_forever()
                except Exception:  # pragma: no cover
                    print("Simple Modbus server stopped due to exception:", flush=True)
                    traceback.print_exc()
                    raise
                finally:
                    print("Simple Modbus server shutting down", flush=True)
        except Exception:
            print("Failed to start Simple Modbus server:", flush=True)
            traceback.print_exc()
            raise

else:
    import importlib
    import pkgutil

    try:
        from pymodbus.server import StartTcpServer  # pymodbus >=3
    except Exception:  # pragma: no cover
        from pymodbus.server.sync import StartTcpServer  # pymodbus 2.x

    _RESOLVE_CACHE: dict[str, object] = {}

    def _scan_for_symbol(symbol: str, *packages: str) -> object:
        if symbol in _RESOLVE_CACHE:
            return _RESOLVE_CACHE[symbol]
        for package_name in packages:
            try:
                package = importlib.import_module(package_name)
            except ImportError:
                continue
            to_inspect = [package.__name__]
            if hasattr(package, "__path__"):
                prefix = package.__name__ + "."
                for modinfo in pkgutil.walk_packages(package.__path__, prefix):
                    to_inspect.append(modinfo.name)
            for module_name in to_inspect:
                try:
                    module = importlib.import_module(module_name)
                except Exception:
                    continue
                if hasattr(module, symbol):
                    value = getattr(module, symbol)
                    _RESOLVE_CACHE[symbol] = value
                    return value
        raise ImportError(f"Cannot resolve {symbol} from packages: {packages}")

    def resolve(symbol: str, *candidates: str) -> object:
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

        class ModbusSlaveContext(_DeviceContext):  # type: ignore
            """Shim replicating the removed ModbusSlaveContext from pymodbus 3.x."""

            def __init__(self, *args, zero_mode: bool = False, **kwargs):
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

            def validate(self, func_code, address, count=1):  # type: ignore[override]
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

if sys.platform.startswith("win"):
    asyncio.set_event_loop_policy(asyncio.WindowsSelectorEventLoopPolicy())

    def build_pymodbus_store() -> object:
        return ModbusSlaveContext(
            di=ModbusSequentialDataBlock(0, list(DISCRETE)),
            co=ModbusSequentialDataBlock(0, list(COILS)),
            hr=ModbusSequentialDataBlock(0, list(HOLDING)),
            ir=ModbusSequentialDataBlock(0, list(INPUT)),
            zero_mode=True,
        )

    def _trace_connect(connected: bool) -> None:
        state = "connected" if connected else "disconnected"
        print(f"Client {state}", flush=True)

    def run_pymodbus_server(host: str, port: int) -> None:
        store = build_pymodbus_store()
        try:
            context = ModbusServerContext(slaves=store, single=True)
        except TypeError:
            context = ModbusServerContext(devices=store, single=True)
        kwargs = {"context": context, "address": (host, port), "trace_connect": _trace_connect}
        try:
            StartTcpServer(**kwargs, allow_reuse_address=True)
        except TypeError as exc:
            if "trace_connect" in str(exc):
                kwargs.pop("trace_connect", None)
                try:
                    StartTcpServer(**kwargs, allow_reuse_address=True)
                finally:
                    print("pymodbus server shutting down", flush=True)
                return
            if "allow_reuse_address" in str(exc):
                try:
                    StartTcpServer(**kwargs)
                finally:
                    print("pymodbus server shutting down", flush=True)
                return
            try:
                StartTcpServer(context, address=(host, port), allow_reuse_address=True)
            except TypeError:
                StartTcpServer(context, address=(host, port))
        finally:
            print("pymodbus server stopping", flush=True)


def main() -> None:  # pragma: no cover - executed in CI
    port = int(os.getenv("PORT", "1502"))
    host = os.getenv("HOST", "127.0.0.1")
    if USE_SIMPLE:
        run_simple_server(host, port)
    else:
        print(f"Simulator starting on {host}:{port}", flush=True)
        run_pymodbus_server(host, port)


if __name__ == "__main__":  # pragma: no mutate
    main()
