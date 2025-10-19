# Changelog

## v0.2.0 (2025-10-19)
- API/Codec
  - Add 64-bit `double` support with configurable four-word ordering via `word_order` (ABCD/BADC/CDAB/DCBA).
  - Enforce `count=4` for `double` (FC3/FC4/FC16) and `count=2` for `float` where applicable.
  - Fix FC3/FC4 behavior: arrays with `count=2` of non-float types are not parsed as float.
  - Add auto-reconnect: on `NOT_CONNECTED`, attempt `connect()` once and retry the operation.
  - Address MSVC narrowing warnings in initialization.
- Config/Schema/Docs
  - Schema: add `word_order` enum; docs updated with examples and guidance.
- Tests
  - Add unit tests for `double` word orders (ABCD/BADC/CDAB/DCBA) and FC3/FC4 `uint16` arrays with `count=2`.
- CI
  - Restore broader unit matrix (Linux: x86_64/i386/armhf/arm64; Windows: win64/win32).
  - Windows integration: robust vcpkg toolchain path handling; PATH/env setup; verbose fallback logging.
  - Linux ARM: multiarch + ports.ubuntu.com sources for foreign-arch runtimes; QEMU-based test execution.
  - Add caches (pip/vcpkg/ccache) and use ccache launcher to speed builds.
  - Harden Ubuntu simulator startup (pymodbus check, nohup logging, readiness probe).

## v0.1.0 (2025-10-17)
- Initial release aligned with Spec Kit  
- Implements stub Modbus client and JSON config parser  
- Supports subscribe/read/write cycle and polling
