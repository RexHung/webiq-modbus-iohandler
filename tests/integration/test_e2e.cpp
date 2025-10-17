// tests/integration/test_e2e.cpp
// E2E integration test for the WebIQ Modbus ioHandler.
// - Connects to a Modbus TCP simulator (e.g., tests/integration/modbus_sim.py)
// - Exercises FC1/3/4/5/6/16 via the project's C API
// Build: added by CMakeLists.txt (see README/tests)

#include <cstdio>
#include <cstring>
#include <string>

// ---- C API exported by the ioHandler shared library -----------------
extern "C" {
  using IoHandle = void*;
  IoHandle CreateIoInstance(void* /*param*/, const char* jsonConfigPath);
  void     DestroyIoInstance(IoHandle h);
  int      SubscribeItems(IoHandle h, const char** names, int count);
  int      UnsubscribeItems(IoHandle h, const char** names, int count);
  int      ReadItem(IoHandle h, const char* name, /*out*/char* outBuf, int outSize);
  int      WriteItem(IoHandle h, const char* name, const char* valueJson);
}

// Simple portable file-exists for C++14
static bool fileExists(const std::string& path) {
  FILE* f = std::fopen(path.c_str(), "r");
  if (!f) return false;
  std::fclose(f);
  return true;
}

int main(int argc, char** argv) {
  // ------------------------------------------------------------------
  // Config path handling
  // If --config <path> is not provided, default to ../config/ci.modbus.json
  // This default matches typical build dir layout: build/tests/integration/<exe>
  // ------------------------------------------------------------------
  std::string defaultPath = "../config/ci.modbus.json";
  std::string cfgPath = defaultPath;

  if (argc > 2 && std::string(argv[1]) == "--config") {
    cfgPath = argv[2];
  }

  if (!fileExists(cfgPath)) {
    std::fprintf(stderr,
      "Config file not found: %s\n"
      "Usage: %s [--config path/to/config.json]\n"
      "Default: %s\n",
      cfgPath.c_str(), argv[0], defaultPath.c_str());
    return 1;
  }

  const char* cfg = cfgPath.c_str();

  // ------------------------------------------------------------------
  // Create instance + subscribe three test items
  // ------------------------------------------------------------------
  IoHandle h = CreateIoInstance(nullptr, cfg);
  if (!h) {
    std::fprintf(stderr, "CreateIoInstance failed (cfg=%s)\n", cfg);
    return 2;
  }

  const char* items[] = { "coil.run", "holding.temp", "ai.flow" };
  if (SubscribeItems(h, items, 3) != 0) {
    std::fprintf(stderr, "SubscribeItems failed\n");
    DestroyIoInstance(h);
    return 3;
  }

  // Small helper to read & print an item
  auto readAndPrint = [&](const char* name, int lineTag) -> bool {
    char buf[128] = {0};
    int rc = ReadItem(h, name, buf, static_cast<int>(sizeof(buf)));
    if (rc != 0) {
      std::fprintf(stderr, "ReadItem('%s') failed rc=%d (at line %d)\n", name, rc, lineTag);
      return false;
    }
    std::printf("%s=%s\n", name, buf);
    return true;
  };

  // 1) Read coil.run (expect false initially per simulator default)
  if (!readAndPrint("coil.run", __LINE__)) {
    UnsubscribeItems(h, items, 3);
    DestroyIoInstance(h);
    return 4;
  }

  // 2) Write coil.run = true, then read back
  if (WriteItem(h, "coil.run", "true") != 0) {
    std::fprintf(stderr, "WriteItem('coil.run','true') failed\n");
    UnsubscribeItems(h, items, 3);
    DestroyIoInstance(h);
    return 5;
  }
  std::printf("coil.run written to true\n");

  if (!readAndPrint("coil.run", __LINE__)) {
    UnsubscribeItems(h, items, 3);
    DestroyIoInstance(h);
    return 6;
  }

  // 3) Read holding.temp -> simulator sets holding[0]=123 with scale 0.1 => 12.3
  if (!readAndPrint("holding.temp", __LINE__)) {
    UnsubscribeItems(h, items, 3);
    DestroyIoInstance(h);
    return 7;
  }

  // 4) Read ai.flow -> simulator sets float32 at input[100..101] to 1.5
  if (!readAndPrint("ai.flow", __LINE__)) {
    UnsubscribeItems(h, items, 3);
    DestroyIoInstance(h);
    return 8;
  }

  // Cleanup
  UnsubscribeItems(h, items, 3);
  DestroyIoInstance(h);

  std::puts("E2E test finished successfully.");
  return 0;
}
