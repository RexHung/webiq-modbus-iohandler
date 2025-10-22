import importlib
import os
import struct
import sys

import pytest


@pytest.fixture()
def simple_modbus_module():
    prev = os.environ.get("SIMPLE_MODBUS")
    os.environ["SIMPLE_MODBUS"] = "1"

    module_name = "tests.integration.modbus_sim"
    if module_name in sys.modules:
        mod = importlib.reload(sys.modules[module_name])
    else:
        mod = importlib.import_module(module_name)

    assert mod.USE_SIMPLE, "simple modbus backend should be selected for tests"

    yield mod

    # Restore shared state for subsequent tests / imports
    if prev is None:
        os.environ.pop("SIMPLE_MODBUS", None)
    else:
        os.environ["SIMPLE_MODBUS"] = prev
    importlib.reload(mod)


def test_read_multiple_holding_registers(simple_modbus_module):
    mod = simple_modbus_module
    request = bytes([3]) + struct.pack(">HH", 0, 2)
    response = mod.build_simple_response(request)
    assert response == b"\x03\x04\x00\x7b\x00\x00"


def test_read_multiple_input_registers(simple_modbus_module):
    mod = simple_modbus_module
    request = bytes([4]) + struct.pack(">HH", 100, 2)
    response = mod.build_simple_response(request)
    expected_registers = struct.pack(">HH", *mod.float_to_regs(1.5))
    assert response[:2] == b"\x04\x04"
    assert response[2:] == expected_registers


def test_write_holding_register(simple_modbus_module):
    mod = simple_modbus_module
    request = bytes([6]) + struct.pack(">HH", 10, 0xABCD)
    response = mod.build_simple_response(request)
    assert response == request  # echo request for FC6
    assert mod.HOLDING[10] == 0xABCD
