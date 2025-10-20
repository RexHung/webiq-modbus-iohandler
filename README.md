# WebIQ Modbus ioHandler Starter

[![Sanitizers (Linux)](https://github.com/RexHung/webiq-modbus-iohandler/actions/workflows/sanitizers.yml/badge.svg)](https://github.com/RexHung/webiq-modbus-iohandler/actions/workflows/sanitizers.yml)

Custom ioHandler for WebIQ supporting Modbus TCP and RTU.

## Overview
Implements the WebIQ ioHandler SDK interface (`CreateIoInstance`, `SubscribeItems`, `ReadItem`, etc.) and adds Modbus TCP/RTU connectivity.

## Quick Start

TL;DR (find_package)

```cmake
cmake_minimum_required(VERSION 3.19)
project(consumer LANGUAGES CXX)
find_package(WebIQModbusIoHandler 0.2 CONFIG REQUIRED)
add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE WebIQ::ioh_modbus)
```

Then configure with your package prefix on the path, e.g.:

```bash
export CMAKE_PREFIX_PATH=/opt/webiq${CMAKE_PREFIX_PATH:+:$CMAKE_PREFIX_PATH}
cmake -S . -B build && cmake --build build --config Release
```

- Use prebuilt package (recommended)
  1) Download a release archive (ZIP/TGZ) and extract to a prefix, e.g. `C:/deps/webiq` (Windows) or `/opt/webiq` (Linux).
  2) Point CMake to the prefix via `CMAKE_PREFIX_PATH` and link the target:

```cmake
cmake_minimum_required(VERSION 3.19)
project(consumer LANGUAGES CXX)
find_package(WebIQModbusIoHandler 0.2 CONFIG REQUIRED)
add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE WebIQ::ioh_modbus)
```

```bash
# Linux/macOS
export CMAKE_PREFIX_PATH=/opt/webiq${CMAKE_PREFIX_PATH:+:$CMAKE_PREFIX_PATH}
cmake -S . -B build && cmake --build build --config Release
```

```powershell
# Windows (MSVC)
$env:CMAKE_PREFIX_PATH = "C:\\deps\\webiq" + $(if ($env:CMAKE_PREFIX_PATH) { ";$env:CMAKE_PREFIX_PATH" } else { "" })
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

- Build from source without libmodbus (stub backend)

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DWITH_LIBMODBUS=OFF
cmake --build build --config Release
```

- Build from source with libmodbus
  - Linux (Debian/Ubuntu):

```bash
sudo apt-get update && sudo apt-get install -y libmodbus-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DWITH_LIBMODBUS=ON
cmake --build build --config Release
```

  - Windows (vcpkg):

```powershell
# Install libmodbus via vcpkg
git clone https://github.com/microsoft/vcpkg "$env:TEMP\vcpkg"; & "$env:TEMP\vcpkg\bootstrap-vcpkg.bat"
$env:VCPKG_ROOT = "$env:TEMP\vcpkg"
& "$env:VCPKG_ROOT\vcpkg.exe" install libmodbus:x64-windows

# Configure with vcpkg toolchain and enable libmodbus
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET=x64-windows -DWITH_LIBMODBUS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Notes
- Runtime search path: ensure the built library directory is on `LD_LIBRARY_PATH` (Linux) or `PATH` (Windows) when running consumers.
- Example consumer: see `examples/consumer-cmake` for a minimal `find_package` usage.

## Features:
- CMake cross-platform build
- JSON configuration for Modbus connection and items
- Stub Modbus client (replaceable with libmodbus)

## Build
```bash
mkdir build && cd build
cmake -DWITH_LIBMODBUS=OFF ..
cmake --build . --config Release
```

* CMakeLists.txt
- 說明摘要

| 功能                                    | 狀態                            |
| ------------------------------------- | ----------------------------- |
| `WITH_LIBMODBUS`                      | 控制是否啟用 libmodbus              |
| `WITH_TESTS`                          | 控制是否建測試目標                     |
| `BUILD_TESTING`                       | 與 `WITH_TESTS` 同步（供 CTest 使用） |
| `include(CTest)` + `enable_testing()` | 自動啟用 `ctest` 測試框架             |
| 測試程式 `test_e2e`                       | 只在 `WITH_TESTS=ON` 時建置        |

- 啟用測試（預設）
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build --output-on-failure
```
- 停用測試（加快建置）
```bash
cmake -S . -B build -DWITH_TESTS=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```


## JSON Schema
```json
{
  "transport": "tcp",
  "tcp": { "host": "192.168.0.10", "port": 502 },
  "rtu": { "device": "/dev/ttyUSB0", "baud": 9600 },
  "items": [
    { "name": "coil.run", "function": 1, "address": 10, "type": "bool", "poll_ms": 200 }
  ]
}
```

## Error Codes
| Code | Meaning                           |
| ---- | --------------------------------- |
| 0    | Success                           |
| -1   | Invalid handle                    |
| -2   | Argument error / operation failed |
| -3   | Buffer too small                  |
| -10  | Not supported                     |


## Roadmap
- Add libmodbus backend
- Batch read optimization
- Reconnect back-off logic
- Automated unit tests


## Directory Structure
```bash
<repo-root>/
├─ CMakeLists.txt                  # 內含 WITH_TESTS / BUILD_TESTING 同步 & add_executable(test_e2e …)
├─ include/
├─ src/
├─ config/
│  └─ ci.modbus.json              # CI 會動態產生或你手動放；add_test 也指向此路徑
├─ tests/
│  └─ integration/
│     ├─ modbus_sim.py            # 從 <repo-root> 執行：python3 tests/integration/modbus_sim.py
│     └─ test_e2e.cpp             # 以相對於 <repo-root> 的路徑加入 CMake
└─ .github/workflows/
   └─ ci.yml
```

## Word Order (64-bit double)

64-bit values occupy four 16-bit Modbus registers (words). Devices may store these words in different orders. Use `word_order` to match your device. The letters A, B, C, D denote the four 16-bit words of a single 64‑bit value.

Register-to-word mapping per `word_order`:

- ABCD: R0→A, R1→B, R2→C, R3→D
- BADC: R0→B, R1→A, R2→D, R3→C
- CDAB: R0→C, R1→D, R2→A, R3→B
- DCBA: R0→D, R1→C, R2→B, R3→A

Notes
- `double` in FC3/FC4/FC16 enforces `count: 4`.
- For 32‑bit `float`, use `swap_words: true/false` (two-word swap) and `count: 2`.
- Non-float arrays with `count: 2` are not treated as floats.

## Full Config Example

```json
{
  "transport": "tcp",
  "tcp": { "host": "192.168.1.50", "port": 502, "timeout_ms": 1000 },
  "reconnect": { "retries": 3, "interval_ms": 500, "backoff_multiplier": 2.0, "max_interval_ms": 4000 },
  "items": [
    { "name": "coil.run",      "unit_id": 1, "function": 1,  "address": 10,  "type": "bool",   "poll_ms": 100 },
    { "name": "hr.temp_c",     "unit_id": 1, "function": 3,  "address": 0,   "type": "int16",  "scale": 0.1, "offset": 0.0, "poll_ms": 200 },
    { "name": "ai.flow_f",     "unit_id": 1, "function": 4,  "address": 100, "count": 2, "type": "float",  "swap_words": false, "poll_ms": 250 },
    { "name": "hr.energy_kwh", "unit_id": 1, "function": 3,  "address": 200, "count": 4, "type": "double", "word_order": "BADC", "poll_ms": 500 },
    { "name": "hr.batch",      "unit_id": 1, "function": 16, "address": 300, "count": 4, "type": "uint16" }
  ]
}
```

## Auto‑Reconnect Policy

Control how the library attempts to reconnect when an operation returns NOT_CONNECTED.

- Fields (top‑level `reconnect`):
  - `retries` (int, >=0): number of reconnect attempts after a NOT_CONNECTED. Default: 1.
  - `interval_ms` (int, >=0): base delay before each reconnect attempt. Default: 0 ms.
  - `backoff_multiplier` (number, >=1.0): multiply delay after each attempt (exponential backoff). Default: 1.0.
  - `max_interval_ms` (int, >=0): optional cap for backoff delay (0 = uncapped). Default: 0.

Behavior
- On NOT_CONNECTED, perform up to `retries` reconnect attempts. Before each attempt, sleep `interval_ms` (growing by `backoff_multiplier` and capped at `max_interval_ms`). After calling `connect()`, retry the operation. If it succeeds or fails with another error, return immediately.

Recommended values
- Typical: `{ "retries": 3, "interval_ms": 500, "backoff_multiplier": 2.0, "max_interval_ms": 4000 }`
- Low‑latency devices: lower `interval_ms` (e.g., 100–200 ms) and limit `retries` to 2–3.

## Using as a CMake Package

After installing or extracting a CPack archive into a prefix, consumers can find and link the library via `find_package`.

- Install or extract the package so that `lib/cmake/WebIQModbusIoHandler` exists under your prefix (e.g., `C:/deps/webiq` or `/opt/webiq`).
- Point `CMAKE_PREFIX_PATH` to that prefix, then:

```cmake
cmake_minimum_required(VERSION 3.19)
project(consumer LANGUAGES CXX)

find_package(WebIQModbusIoHandler CONFIG REQUIRED)

add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE WebIQ::ioh_modbus)
```

See `examples/consumer-cmake` for a minimal consumer project and build instructions.

Notes
- Windows: ensure the directory containing the built `ioh_modbus.dll` is on `PATH` when running.
- Version pinning: `find_package(WebIQModbusIoHandler 0.2 CONFIG REQUIRED)` matches the installed `ConfigVersion`.

## Windows PDBs (Optional)

- CMake option `PACKAGE_PDB` controls whether MSVC `.pdb` files are installed and included in packages.
- CI releases enable this (Windows job passes `-DPACKAGE_PDB=ON`).
- Local build enabling PDBs:
  - `cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DPACKAGE_PDB=ON`

## Sanitizers Workflow

- GitHub Actions job: Sanitizers (Linux) runs Address/Undefined sanitizers on unit tests.
- Schedule: twice monthly and manual dispatch.
- Badge at top of this README links to the workflow run.

## Logging

Basic runtime logging is available and controlled via environment variables:

- `WIQ_LOG_LEVEL` (default `info`): one of `trace, debug, info, warn, error, none` (or `0..5`).
- `WIQ_LOG_TS` (default `0`): set to `1/true` to include timestamps.

Example
```bash
WIQ_LOG_LEVEL=debug WIQ_LOG_TS=1 ./your_app
```

Log format
- `[YYYY-MM-DD HH:MM:SS] LEVEL file:line: message` when `WIQ_LOG_TS=1`
- `LEVEL file:line: message` otherwise
