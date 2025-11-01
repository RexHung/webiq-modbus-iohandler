#!/usr/bin/env python3
"""Simple Modbus ASCII simulator for integration testing.

This script starts a pymodbus ASCII server backed by in-memory data blocks. It is
intended for lightweight local testing where Modbus RTU ASCII frames are
required without external hardware.
"""

import argparse
import logging
import signal
import sys

from pymodbus.datastore import ModbusSequentialDataBlock, ModbusSlaveContext, ModbusServerContext
from pymodbus.server.sync import StartSerialServer
from pymodbus.transaction import ModbusAsciiFramer


def build_context(coils_size: int, discrete_size: int, holding_size: int, input_size: int) -> ModbusServerContext:
    store = ModbusSlaveContext(
        di=ModbusSequentialDataBlock(0, [0] * discrete_size),
        co=ModbusSequentialDataBlock(0, [0] * coils_size),
        hr=ModbusSequentialDataBlock(0, [0] * holding_size),
        ir=ModbusSequentialDataBlock(0, [0] * input_size),
        zero_mode=True,
    )
    return ModbusServerContext(slaves=store, single=True)


def parse_args(argv):
    parser = argparse.ArgumentParser(description="Modbus ASCII simulator")
    parser.add_argument("--port", default="/dev/ttyS10", help="Serial port path (default: %(default)s)")
    parser.add_argument("--baud", type=int, default=9600, help="Baud rate (default: %(default)s)")
    parser.add_argument("--parity", choices=["N", "E", "O"], default="E", help="Parity (default: %(default)s)")
    parser.add_argument("--data-bits", type=int, choices=[7, 8], default=7, help="Data bits (default: %(default)s)")
    parser.add_argument("--stop-bits", type=int, choices=[1, 2], default=1, help="Stop bits (default: %(default)s)")
    parser.add_argument("--timeout-ms", type=int, default=1500, help="Read timeout in milliseconds")
    parser.add_argument("--unit-id", type=int, default=1, help="Unit identifier exposed by the server")
    parser.add_argument("--coils", type=int, default=256, help="Number of coils to expose")
    parser.add_argument("--discrete", type=int, default=256, help="Number of discrete inputs")
    parser.add_argument("--holding", type=int, default=512, help="Number of holding registers")
    parser.add_argument("--input", type=int, default=512, help="Number of input registers")
    parser.add_argument("--log-level", default="INFO", help="Logging level (default: %(default)s)")
    return parser.parse_args(argv)


def main(argv=None):
    opts = parse_args(argv or sys.argv[1:])
    logging.basicConfig(level=getattr(logging, opts.log_level.upper(), logging.INFO))

    context = build_context(opts.coils, opts.discrete, opts.holding, opts.input)

    # Populate a few demo registers so reads return meaningful values.
    slave = context[0x00 if context.single else opts.unit_id]
    slave.setValues(3, 0x00, [10, 20, 30, 40])  # holding registers
    slave.setValues(4, 0x00, [1, 2, 3, 4])      # input registers

    should_stop = False

    def handle_signal(_signum, _frame):
        nonlocal should_stop
        logging.info("Stopping Modbus ASCII simulator")
        should_stop = True

    signal.signal(signal.SIGINT, handle_signal)
    signal.signal(signal.SIGTERM, handle_signal)

    logging.info(
        "Starting Modbus ASCII server on %s (baud=%d parity=%s data=%d stop=%d unit=%d)",
        opts.port,
        opts.baud,
        opts.parity,
        opts.data_bits,
        opts.stop_bits,
        opts.unit_id,
    )

    StartSerialServer(
        context,
        framer=ModbusAsciiFramer,
        port=opts.port,
        timeout=opts.timeout_ms / 1000.0,
        baudrate=opts.baud,
        parity=opts.parity,
        bytesize=opts.data_bits,
        stopbits=opts.stop_bits,
        handle_stop=lambda: should_stop,
    )


if __name__ == "__main__":
    main()
