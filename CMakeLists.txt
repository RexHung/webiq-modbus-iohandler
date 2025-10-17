cmake_minimum_required(VERSION 3.14)
project(webiq_modbus_iohandler LANGUAGES CXX)

# ----------------
# Options
# ----------------
option(WITH_LIBMODBUS "Build with libmodbus backend" OFF)
option(WITH_TESTS     "Build test targets" ON)

# Keep CMake's standard flag aligned so ctest/CTest behave consistently
set(BUILD_TESTING ${WITH_TESTS} CACHE BOOL "" FORCE)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ----------------
# Main library
# ----------------
add_library(ioh_modbus SHARED
  src/Export.cpp
  src/ModbusIoHandler.cpp
  src/modbus/ModbusClient.cpp
)
target_include_directories(ioh_modbus PUBLIC include)
target_compile_definitions(ioh_modbus PRIVATE WIQ_IOH_BUILD)

# JSON dependency (nlohmann/json)
include(FetchContent)
FetchContent_Declare(nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)
target_link_libraries(ioh_modbus PRIVATE nlohmann_json::nlohmann_json)

# Optional libmodbus backend
if(WITH_LIBMODBUS)
  find_path(LIBMODBUS_INCLUDE_DIR modbus.h)
  find_library(LIBMODBUS_LIBRARY NAMES modbus)
  if(LIBMODBUS_INCLUDE_DIR AND LIBMODBUS_LIBRARY)
    target_include_directories(ioh_modbus PRIVATE ${LIBMODBUS_INCLUDE_DIR})
    target_compile_definitions(ioh_modbus PRIVATE WITH_LIBMODBUS=1)
    target_link_libraries(ioh_modbus PRIVATE ${LIBMODBUS_LIBRARY})
    message(STATUS "libmodbus enabled")
  else()
    message(WARNING "libmodbus requested but not found; building with stub backend")
  endif()
endif()

if(MSVC)
  target_compile_definitions(ioh_modbus PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# ----------------
# Tests (optional)
# ----------------
include(CTest)
if(BUILD_TESTING)
  enable_testing()

  # Unit tests (stub, no external deps)
  add_executable(test_stub tests/unit/test_stub.cpp)
  target_include_directories(test_stub PRIVATE include)
  target_link_libraries(test_stub PRIVATE ioh_modbus)
  add_test(NAME unit_stub COMMAND test_stub)

  add_executable(test_codec tests/unit/test_codec.cpp)
  add_test(NAME unit_codec COMMAND test_codec)

  add_executable(test_scale_zero tests/unit/test_scale_zero.cpp)
  target_link_libraries(test_scale_zero PRIVATE ioh_modbus)
  add_test(NAME unit_scale_zero COMMAND test_scale_zero)

  add_executable(test_api_float_swap tests/unit/test_api_float_swap.cpp)
  target_link_libraries(test_api_float_swap PRIVATE ioh_modbus)
  add_test(NAME unit_api_float_swap COMMAND test_api_float_swap)

  add_executable(test_api_coils_array tests/unit/test_api_coils_array.cpp)
  target_link_libraries(test_api_coils_array PRIVATE ioh_modbus)
  add_test(NAME unit_api_coils_array COMMAND test_api_coils_array)

  add_executable(test_config_invalid tests/unit/test_config_invalid.cpp)
  target_link_libraries(test_config_invalid PRIVATE ioh_modbus)
  add_test(NAME unit_config_invalid COMMAND test_config_invalid)

  # E2E integration test binary
  add_executable(test_e2e tests/integration/test_e2e.cpp)
  target_include_directories(test_e2e PRIVATE include)
  target_link_libraries(test_e2e PRIVATE ioh_modbus)

  add_test(NAME e2e
           COMMAND test_e2e --config ${CMAKE_SOURCE_DIR}/config/ci.modbus.json)
endif()
