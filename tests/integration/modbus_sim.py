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
import atexit
import logging
import os
import signal
import struct
import sys
import threading
import traceback
from typing import Callable, Optional

# Decide which backend to use (default to the simple server on Windows runners).
_USE_SIMPLE = os.getenv("SIMPLE_MODBUS")
if _USE_SIMPLE is None:
    _USE_SIMPLE = "1" if sys.platform.startswith("win") else "0"
USE_SIMPLE = _USE_SIMPLE == "1"


def configure_logging() -> None:
    level_name = os.getenv("MODBUS_SIM_LOG", "INFO").upper()
    level = getattr(logging, level_name, logging.INFO)
    logging.basicConfig(
        level=level,
        format="%(asctime)s %(levelname)s %(message)s",
    )


configure_logging()
logger = logging.getLogger("modbus_sim")


def _parse_heartbeat_interval() -> float:
    raw = os.getenv("MODBUS_SIM_HEARTBEAT", "15")
    try:
        interval = float(raw)
    except ValueError:
        logger.warning("Invalid MODBUS_SIM_HEARTBEAT value %r; defaulting to 15s", raw)
        return 15.0
    if interval <= 0:
        logger.info("Heartbeat disabled via MODBUS_SIM_HEARTBEAT=%s", raw)
        return 0.0
    return interval


def _start_heartbeat() -> Optional[threading.Event]:
    interval = _parse_heartbeat_interval()
    if interval <= 0:
        return None
    stop_event = threading.Event()
    logger.info("Heartbeat thread starting (interval=%ss)", interval)

    def _beat() -> None:
        while not stop_event.wait(interval):
            logger.info("Heartbeat: simulator alive (threads=%s)", threading.active_count())

    thread = threading.Thread(target=_beat, name="modbus-heartbeat", daemon=True)
    thread.start()
    return stop_event


_signal_handlers_installed = False


def _install_signal_logging() -> None:
    global _signal_handlers_installed
    if _signal_handlers_installed:
        return
    _signal_handlers_installed = True
    signals_to_track: list[tuple[int, Optional[Callable]]]
    signals_to_track = [(signal.SIGTERM, signal.getsignal(signal.SIGTERM))]
    signals_to_track.append((signal.SIGINT, signal.getsignal(signal.SIGINT)))
    if hasattr(signal, "SIGBREAK"):
        sig = getattr(signal, "SIGBREAK")
        signals_to_track.append((sig, signal.getsignal(sig)))

    def _signal_name(sig: int) -> str:
        name = signal.Signals(sig).name if hasattr(signal, "Signals") else None
        return name or f"SIG{sig}"

    for sig, previous in signals_to_track:
        def _handler(signum: int, frame, *, _prev=previous, _sig=sig) -> None:  # type: ignore[no-untyped-def]
            name = _signal_name(_sig)
            logger.warning("Simulator received %s (%s)", signum, name)
            if callable(_prev):
                try:
                    _prev(signum, frame)
                except Exception:
                    logger.exception("Previous handler for %s raised", name)
                return
            if _prev == signal.SIG_IGN:
                return
            raise SystemExit(128 + signum)

        signal.signal(sig, _handler)

    def _log_exit() -> None:
        logger.info("Simulator process exiting (threads=%s)", threading.active_count())

    atexit.register(_log_exit)


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
            logger.info("Client connected from %s:%s", host, port)

        def finish(self) -> None:  # pragma: no cover - exercised in CI
            host, port = self.client_address
            logger.info("Client disconnected from %s:%s", host, port)

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
                logger.exception("Simple server handler error")

    class SimpleModbusServer(socketserver.ThreadingTCPServer):  # pragma: no cover
        allow_reuse_address = True
        daemon_threads = True

        def handle_error(self, request, client_address):
            logger.exception("Simple server handle_error for %s", client_address)

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
            payload = bytearray(2 + count * 2)
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
        logger.info("Simple Modbus server starting on %s:%s", host, port)
        try:
            _install_signal_logging()
            heartbeat = _start_heartbeat()
            with SimpleModbusServer((host, port), ModbusTCPHandler) as server:
                logger.info("Simple Modbus server entering serve_forever loop")
                try:
                    server.serve_forever()
                except BaseException as exc:  # pragma: no cover - exercised in CI
                    if isinstance(exc, (SystemExit, KeyboardInterrupt)):
                        logger.warning("Simple Modbus server interrupted: %s", exc)
                    else:
                        logger.exception("Simple Modbus server stopped due to exception")
                    raise
                finally:
                    if heartbeat:
                        heartbeat.set()
                    shutdown_request = getattr(server, "_BaseServer__shutdown_request", None)
                    is_set = getattr(server, "_BaseServer__is_shut_down", None)
                    if hasattr(is_set, "is_set"):
                        is_set = is_set.is_set()
                    logger.info(
                        "Simple Modbus server shutting down (shutdown_request=%s, shutdown_flag=%s)",
                        shutdown_request,
                        is_set,
                    )
        except Exception:
            logger.exception("Failed to start Simple Modbus server")
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
        logger.info("pymodbus server selected on %s:%s", host, port)
        logging.getLogger("pymodbus").setLevel(logging.DEBUG)
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
                    logger.info("pymodbus server shutting down (retry without trace_connect)")
                return
            if "allow_reuse_address" in str(exc):
                try:
                    StartTcpServer(**kwargs)
                finally:
                    logger.info("pymodbus server shutting down (retry without reuse flag)")
                return
            try:
                StartTcpServer(context, address=(host, port), allow_reuse_address=True)
            except TypeError:
                StartTcpServer(context, address=(host, port))
        finally:
            logger.info("pymodbus server stopping")


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
