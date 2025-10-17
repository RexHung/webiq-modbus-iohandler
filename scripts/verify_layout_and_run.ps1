#Requires -Version 5.1
Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# ===== config =====
$PORT = $env:PORT
if (-not $PORT) { $PORT = 1602 }

$RepoRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$BuildDir = Join-Path $RepoRoot "build"
$ConfDir  = Join-Path $RepoRoot "config"
$ConfFile = Join-Path $ConfDir  "ci.modbus.json"

Write-Host "== Repo root: $RepoRoot"
Write-Host "== Using port: $PORT"

# 1) 檢查基本結構
Write-Host "== Checking tree..."
if (-not (Test-Path (Join-Path $RepoRoot "CMakeLists.txt")) -or
    -not (Test-Path (Join-Path $RepoRoot "tests/integration/test_e2e.cpp")) -or
    -not (Test-Path (Join-Path $RepoRoot "tests/integration/modbus_sim.py"))) {
  Write-Error "❌ tree mismatch"
}

# 2) 單元/組件建置（stub）
# ---- Locate cmake & ctest safely ----
function Resolve-Tool([string]$name, [string[]]$extraPaths) {
  $cmd = Get-Command $name -ErrorAction SilentlyContinue
  if ($cmd) { return $cmd.Path }
  foreach ($p in $extraPaths) {
    if (Test-Path $p) { return $p }
  }
  return $null
}

$cmake = Resolve-Tool "cmake" @(
  "C:\Program Files\CMake\bin\cmake.exe",
  "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
  "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
  "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
)
$ctest = Resolve-Tool "ctest" @(
  "C:\Program Files\CMake\bin\ctest.exe",
  "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe",
  "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe",
  "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe"
)

if (-not $cmake) { throw "cmake not found. Install CMake (winget/choco) or add VS CMake to PATH." }
if (-not $ctest) { throw "ctest not found. Ensure CMake is installed correctly." }

Write-Host "== Using cmake: $cmake"
Write-Host "== Using ctest: $ctest"

Write-Host "== Building (stub)..."
& $cmake -S $RepoRoot -B $BuildDir -DWITH_LIBMODBUS=OFF -DWITH_TESTS=ON -DCMAKE_BUILD_TYPE=Release
& $cmake --build $BuildDir --config Release

# ...
& $ctest --test-dir $BuildDir --output-on-failure

# ...
& $cmake -S $RepoRoot -B $BuildDir -DWITH_LIBMODBUS=ON -DWITH_TESTS=ON -DCMAKE_BUILD_TYPE=Release
& $cmake --build $BuildDir --config Release


# 3) 啟動模擬器（背景）
# 找可用的 Python 啟動方式
$pythonCmd = "python"
if (-not (Get-Command $pythonCmd -ErrorAction SilentlyContinue)) {
  if (Get-Command "py" -ErrorAction SilentlyContinue) { $pythonCmd = "py -3" }
}
# 啟動模擬器
Write-Host "== Starting simulator on port $PORT..."
$env:PORT = "$PORT"
$psi = New-Object System.Diagnostics.ProcessStartInfo
$psi.FileName = "powershell.exe"
$psi.Arguments = "-NoProfile -WindowStyle Hidden -Command `$env:PORT=$env:PORT; $pythonCmd tests/integration/modbus_sim.py"
$psi.WorkingDirectory = $RepoRoot
$psi.UseShellExecute = $true
$psi.WindowStyle = "Hidden"
$proc = [System.Diagnostics.Process]::Start($psi)
Start-Sleep -Seconds 1

try {
  # 4) 產生 CI 設定
  Write-Host "== Generating $ConfFile..."
  New-Item -ItemType Directory -Force -Path $ConfDir | Out-Null
  @"
{
  "transport": "tcp",
  "tcp": { "host": "127.0.0.1", "port": $PORT, "timeout_ms": 1000 },
  "items": [
    { "name": "coil.run",      "unit_id": 1, "function": 1,  "address": 10,  "type": "bool",  "poll_ms": 100 },
    { "name": "holding.temp",  "unit_id": 1, "function": 3,  "address": 0,   "type": "int16", "scale": 0.1, "poll_ms": 100 },
    { "name": "ai.flow",       "unit_id": 1, "function": 4,  "address": 100, "type": "float", "poll_ms": 100, "swap_words": false }
  ]
}
"@ | Set-Content -Path $ConfFile -Encoding UTF8

  # 5) 整合建置（libmodbus + 測試）
  Write-Host "== Building (libmodbus + tests)..."
  cmake -S $RepoRoot -B $BuildDir -DWITH_LIBMODBUS=ON -DWITH_TESTS=ON -DCMAKE_BUILD_TYPE=Release
  cmake --build $BuildDir --config Release

  # 6) 尋找與執行 test_e2e
  Write-Host "== Running E2E..."
  $e2e = Join-Path $BuildDir "tests/integration/test_e2e.exe"
  if (-not (Test-Path $e2e)) {
    $found = Get-ChildItem -Path $BuildDir -Filter "test_e2e*.exe" -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($found) { $e2e = $found.FullName }
  }
  if (-not (Test-Path $e2e)) {
    throw "test_e2e executable not found"
  }
  & $e2e --config $ConfFile

  Write-Host "✅ All checks passed."
}
finally {
  Write-Host "== Cleanup..."
  if ($proc -and -not $proc.HasExited) {
    $proc.Kill()
    $proc.WaitForExit()
  }
}
