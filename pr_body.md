Summary

- Harden the Windows simple Modbus simulator: add heartbeat/signal logging, guard pymodbus helpers behind `USE_SIMPLE`, and ensure the simple server stays alive through CI by running everything in a single PowerShell step.
- Extend CI to run the new simple server pytest and install the required dependency so regressions are caught early.
- Record the investigation progress in `CODEX_CONTEXT_NOTES.md` v0.1.3 for future sessions.

Changes

- tests/integration/modbus_sim.py
  - Added heartbeat startup log, signal tracing, and fixed FC3/FC4 response buffer length.
  - Wrapped pymodbus helpers with `if not USE_SIMPLE` so the Windows default path no longer crashes.
- tests/integration/test_modbus_simple.py
  - New pytest covering FC3/FC4 multi-register reads and FC6 writes against the simple server implementation.
- tests/integration/requirements.txt
  - Added `pytest>=8` for the new unit tests.
- .github/workflows/CI.yml
  - Consolidated Windows integration steps into one PowerShell script and added `python -m pytest tests/integration/test_modbus_simple.py`.
- CODEX_CONTEXT_NOTES.md
  - Bumped to v0.1.3, summarising the fixes, successful CI runs, and remaining ideas.

Verification

- python3 -m pytest tests/integration/test_modbus_simple.py
- GitHub Actions CI run 18719057069 (green)

Next Steps

- Continue to monitor upcoming CI runs to ensure stability.
- Consider adding pymodbus backend smoke tests in a separate follow-up if needed.
