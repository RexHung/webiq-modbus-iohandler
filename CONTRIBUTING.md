# Contributing Guide

## PR Workflow
1. Fork this repo and create a feature branch.
2. Follow C++14 standard and CMake build.
3. Run `tests/` before submitting a PR.
4. Use commit message convention:
  - `feat:` new feature  
  - `fix:` bug fix  
  - `docs:` documentation  
  - `test:` unit or integration tests  
  - `chore:` build system or config changes

## CI Pipeline
GitHub Actions (`.github/workflows/CI.yml`) runs on every push.


## Code Style
- Use clang-format (LLVM style).
- Each public header must have include guards and brief Doxygen comments.
