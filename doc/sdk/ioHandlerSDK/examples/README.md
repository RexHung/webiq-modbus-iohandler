# WebIQ ioHandler Example (Reference Extract)

This folder contains excerpts from the official WebIQ ioHandler SDK example.

## Files

| File | Description |
|------|--------------|
| `example.h` | Declares `ioHandlerExample` class inheriting from `shmi::ioHandlerBase`. Demonstrates a full reference implementation of a custom ioHandler. |
| `example.cpp` | (If present) Implements simulation and monitoring threads for the example handler. |
| `SubscriptionGroup.*` | (If present) Shows how items with identical poll intervals are grouped for efficiency. |

## Key Concepts

- `ioHandlerExample` creates 10 simulated variables (`var0`–`var9`), updating periodically.
- Demonstrates usage of:
  - `subscribe()` / `unsubscribe()`
  - `read()` / `write()`
  - `method()` calls with JSON arguments
- Two background threads:
  - **Simulation thread** (`thread_proc_simulation`) updates internal values every 100 ms.
  - **Monitoring thread** (`thread_proc_monitor`) pushes changed values to WebIQ Connect.

## Usage

These example sources are included for **API reference only**.
They must **not be recompiled or redistributed** outside this repository.

© Smart HMI GmbH, Germany — extracted under fair-use for internal development.
