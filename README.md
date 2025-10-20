# WebIQ Modbus ioHandler Starter

[![Sanitizers (Linux)](https://github.com/RexHung/webiq-modbus-iohandler/actions/workflows/sanitizers.yml/badge.svg)](https://github.com/RexHung/webiq-modbus-iohandler/actions/workflows/sanitizers.yml)

Custom ioHandler for WebIQ supporting Modbus TCP and RTU.

## Overview
Implements the WebIQ ioHandler SDK interface (`CreateIoInstance`, `SubscribeItems`, `ReadItem`, etc.) and adds Modbus TCP/RTU connectivity.

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
