## Consumer Example (CMake)

A minimal CMake project that finds and links `WebIQModbusIoHandler` using its exported CMake package config.

## Prerequisites
- CMake 3.19+
- A built and installed (or CPack-extracted) package providing:
  - `include/` headers
  - `bin/` and/or `lib/` with `ioh_modbus`
  - `lib/cmake/WebIQModbusIoHandler/` with `*Config.cmake`

## Build: Linux/macOS
```bash
# Suppose the package is extracted under /opt/webiq
export CMAKE_PREFIX_PATH=/opt/webiq${CMAKE_PREFIX_PATH:+:$CMAKE_PREFIX_PATH}
cmake -S . -B build
cmake --build build --config Release
./build/consumer
```

## Build: Windows (MSVC)
```powershell
# Suppose the package is extracted under C:\\deps\\webiq
$env:CMAKE_PREFIX_PATH = "C:\\deps\\webiq" + $(if ($env:CMAKE_PREFIX_PATH) { ";$env:CMAKE_PREFIX_PATH" } else { "" })
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release

# Ensure the DLL directory (from the package bin/) is on PATH before running
$env:PATH = "C:\\deps\\webiq\\bin;" + $env:PATH
./build/Release/consumer.exe
```

## Notes
- You can pin a minimum version: `find_package(WebIQModbusIoHandler 0.2 CONFIG REQUIRED)`
- The example does not call into the library; it only verifies linkability.
- For debug symbol packages on Windows, the upstream project can enable `PACKAGE_PDB=ON`.
