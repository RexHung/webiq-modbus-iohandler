cmake_minimum_required(VERSION 3.14)
project(webiq_modbus_iohandler LANGUAGES CXX)

# ----------------
# Options
# ----------------
option(WITH_LIBMODBUS "Build with libmodbus backend" OFF)
option(WITH_TESTS     "Build test targets" ON)
option(COVERAGE       "Build with coverage flags (GNU/Clang)" OFF)

# Keep CMake's standard flag aligned so ctest/CTest behave consistently
set(BUILD_TESTING ${WITH_TESTS} CACHE BOOL "" FORCE)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Optional coverage flags (Linux/GNU/Clang)
if(COVERAGE)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    message(STATUS "COVERAGE=ON: enabling --coverage flags (-O0 -g)")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0 -g --coverage")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -g --coverage")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} --coverage")
  else()
    message(WARNING "COVERAGE requested but compiler ${CMAKE_CXX_COMPILER_ID} is not supported. Skipping.")
  endif()
endif()

# ----------------
# Main library
# ----------------
add_library(ioh_modbus SHARED
  src/Export.cpp
  src/ModbusIoHandler.cpp
  src/modbus/ModbusClient.cpp
)
target_include_directories(ioh_modbus PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
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
  # Help CMake locate libmodbus when using vcpkg toolchain on Windows
  set(_VCPKG_INC "$ENV{VCPKG_ROOT}/installed/${VCPKG_TARGET_TRIPLET}/include")
  set(_VCPKG_LIB "$ENV{VCPKG_ROOT}/installed/${VCPKG_TARGET_TRIPLET}/lib")
  message(STATUS "WITH_LIBMODBUS=ON; VCPKG_ROOT='$ENV{VCPKG_ROOT}', TRIPLET='${VCPKG_TARGET_TRIPLET}'")
  find_path(LIBMODBUS_INCLUDE_DIR NAMES modbus/modbus.h modbus.h
            HINTS ${_VCPKG_INC})
  find_library(LIBMODBUS_LIBRARY NAMES modbus
               HINTS ${_VCPKG_LIB})
  message(STATUS "LIBMODBUS_INCLUDE_DIR='${LIBMODBUS_INCLUDE_DIR}'")
  message(STATUS "LIBMODBUS_LIBRARY='${LIBMODBUS_LIBRARY}'")
  if(LIBMODBUS_INCLUDE_DIR AND LIBMODBUS_LIBRARY)
    target_include_directories(ioh_modbus PRIVATE ${LIBMODBUS_INCLUDE_DIR} ${LIBMODBUS_INCLUDE_DIR}/modbus)
    target_compile_definitions(ioh_modbus PRIVATE WITH_LIBMODBUS=1)
    target_link_libraries(ioh_modbus PRIVATE ${LIBMODBUS_LIBRARY})
    if(WIN32)
      target_link_libraries(ioh_modbus PRIVATE ws2_32)
    endif()
    message(STATUS "libmodbus enabled")
  else()
    message(FATAL_ERROR "WITH_LIBMODBUS requested but libmodbus not found.\nInclude='${LIBMODBUS_INCLUDE_DIR}' lib='${LIBMODBUS_LIBRARY}'.\nVCPKG_ROOT='$ENV{VCPKG_ROOT}' TRIPLET='${VCPKG_TARGET_TRIPLET}'.")
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

  # Helper: set runtime library path for shared libs via test ENVIRONMENT

  # Unit tests (stub, no external deps)
  add_executable(test_stub tests/unit/test_stub.cpp)
  target_include_directories(test_stub PRIVATE include)
  target_link_libraries(test_stub PRIVATE ioh_modbus)
  add_test(NAME unit_stub COMMAND $<TARGET_FILE:test_stub>)

  add_executable(test_codec tests/unit/test_codec.cpp)
  add_test(NAME unit_codec COMMAND $<TARGET_FILE:test_codec>)

  add_executable(test_scale_zero tests/unit/test_scale_zero.cpp)
  target_link_libraries(test_scale_zero PRIVATE ioh_modbus)
  add_test(NAME unit_scale_zero COMMAND $<TARGET_FILE:test_scale_zero>)

  add_executable(test_api_float_swap tests/unit/test_api_float_swap.cpp)
  target_link_libraries(test_api_float_swap PRIVATE ioh_modbus)
  add_test(NAME unit_api_float_swap COMMAND $<TARGET_FILE:test_api_float_swap>)

  add_executable(test_api_coils_array tests/unit/test_api_coils_array.cpp)
  target_link_libraries(test_api_coils_array PRIVATE ioh_modbus)
  add_test(NAME unit_api_coils_array COMMAND $<TARGET_FILE:test_api_coils_array>)

  add_executable(test_config_invalid tests/unit/test_config_invalid.cpp)
  target_link_libraries(test_config_invalid PRIVATE ioh_modbus)
  add_test(NAME unit_config_invalid COMMAND $<TARGET_FILE:test_config_invalid>)

  add_executable(test_api_fc2 tests/unit/test_api_fc2.cpp)
  target_link_libraries(test_api_fc2 PRIVATE ioh_modbus)
  add_test(NAME unit_api_fc2 COMMAND $<TARGET_FILE:test_api_fc2>)

  add_executable(test_api_fc3_array tests/unit/test_api_fc3_array.cpp)
  target_link_libraries(test_api_fc3_array PRIVATE ioh_modbus)
  add_test(NAME unit_api_fc3_array COMMAND $<TARGET_FILE:test_api_fc3_array>)

  add_executable(test_api_fc15_bad_array tests/unit/test_api_fc15_bad_array.cpp)
  target_link_libraries(test_api_fc15_bad_array PRIVATE ioh_modbus)
  add_test(NAME unit_api_fc15_bad_array COMMAND $<TARGET_FILE:test_api_fc15_bad_array>)

  add_executable(test_api_fc16_bad_array tests/unit/test_api_fc16_bad_array.cpp)
  target_link_libraries(test_api_fc16_bad_array PRIVATE ioh_modbus)
  add_test(NAME unit_api_fc16_bad_array COMMAND $<TARGET_FILE:test_api_fc16_bad_array>)

  add_executable(test_api_int16_scale_bounds tests/unit/test_api_int16_scale_bounds.cpp)
  target_link_libraries(test_api_int16_scale_bounds PRIVATE ioh_modbus)
  add_test(NAME unit_api_int16_scale_bounds COMMAND $<TARGET_FILE:test_api_int16_scale_bounds>)

  add_executable(test_api_float_nonfinite tests/unit/test_api_float_nonfinite.cpp)
  target_link_libraries(test_api_float_nonfinite PRIVATE ioh_modbus)
  add_test(NAME unit_api_float_nonfinite COMMAND $<TARGET_FILE:test_api_float_nonfinite>)

  add_executable(test_api_fc4_array tests/unit/test_api_fc4_array.cpp)
  target_link_libraries(test_api_fc4_array PRIVATE ioh_modbus)
  add_test(NAME unit_api_fc4_array COMMAND $<TARGET_FILE:test_api_fc4_array>)

  add_executable(test_api_double_read_array tests/unit/test_api_double_read_array.cpp)
  target_link_libraries(test_api_double_read_array PRIVATE ioh_modbus)
  add_test(NAME unit_api_double_read_array COMMAND $<TARGET_FILE:test_api_double_read_array>)

  add_executable(test_api_double_write_number tests/unit/test_api_double_write_number.cpp)
  target_link_libraries(test_api_double_write_number PRIVATE ioh_modbus)
  add_test(NAME unit_api_double_write_number COMMAND $<TARGET_FILE:test_api_double_write_number>)

  add_executable(test_api_double_word_order_dcba tests/unit/test_api_double_word_order_dcba.cpp)
  target_link_libraries(test_api_double_word_order_dcba PRIVATE ioh_modbus)
  add_test(NAME unit_api_double_word_order_dcba COMMAND $<TARGET_FILE:test_api_double_word_order_dcba>)

  add_executable(test_api_double_word_order_abcd tests/unit/test_api_double_word_order_abcd.cpp)
  target_link_libraries(test_api_double_word_order_abcd PRIVATE ioh_modbus)
  add_test(NAME unit_api_double_word_order_abcd COMMAND $<TARGET_FILE:test_api_double_word_order_abcd>)

  add_executable(test_api_double_word_order_badc tests/unit/test_api_double_word_order_badc.cpp)
  target_link_libraries(test_api_double_word_order_badc PRIVATE ioh_modbus)
  add_test(NAME unit_api_double_word_order_badc COMMAND $<TARGET_FILE:test_api_double_word_order_badc>)

  add_executable(test_api_double_word_order_cdab tests/unit/test_api_double_word_order_cdab.cpp)
  target_link_libraries(test_api_double_word_order_cdab PRIVATE ioh_modbus)
  add_test(NAME unit_api_double_word_order_cdab COMMAND $<TARGET_FILE:test_api_double_word_order_cdab>)

  add_executable(test_api_fc3_array_2 tests/unit/test_api_fc3_array_2.cpp)
  target_link_libraries(test_api_fc3_array_2 PRIVATE ioh_modbus)
  add_test(NAME unit_api_fc3_array_2 COMMAND $<TARGET_FILE:test_api_fc3_array_2>)

  add_executable(test_api_fc4_array_2 tests/unit/test_api_fc4_array_2.cpp)
  target_link_libraries(test_api_fc4_array_2 PRIVATE ioh_modbus)
  add_test(NAME unit_api_fc4_array_2 COMMAND $<TARGET_FILE:test_api_fc4_array_2>)

  add_executable(test_config_invalid_float_count tests/unit/test_config_invalid_float_count.cpp)
  target_link_libraries(test_config_invalid_float_count PRIVATE ioh_modbus)
  add_test(NAME unit_config_invalid_float_count COMMAND $<TARGET_FILE:test_config_invalid_float_count>)

  add_executable(test_config_invalid_double tests/unit/test_config_invalid_double.cpp)
  target_link_libraries(test_config_invalid_double PRIVATE ioh_modbus)
  add_test(NAME unit_config_invalid_double COMMAND $<TARGET_FILE:test_config_invalid_double>)

  # E2E integration test binary
  add_executable(test_e2e tests/integration/test_e2e.cpp)
  target_include_directories(test_e2e PRIVATE include)
  target_link_libraries(test_e2e PRIVATE ioh_modbus)
  add_test(NAME e2e
           COMMAND $<TARGET_FILE:test_e2e> --config ${CMAKE_SOURCE_DIR}/config/ci.modbus.json)

  # Set runtime environment for all tests
  if(UNIX)
    set(_LD "LD_LIBRARY_PATH=$<TARGET_FILE_DIR:ioh_modbus>:$ENV{LD_LIBRARY_PATH}")
    set_tests_properties(
      unit_stub unit_codec unit_scale_zero unit_api_float_swap unit_api_coils_array
      unit_config_invalid unit_api_fc2 unit_api_fc3_array unit_api_fc15_bad_array
      unit_api_fc16_bad_array unit_api_int16_scale_bounds unit_api_float_nonfinite
      unit_api_fc4_array unit_api_fc3_array_2 unit_api_fc4_array_2
      unit_api_double_read_array unit_api_double_write_number
      unit_api_double_word_order_dcba unit_api_double_word_order_abcd unit_api_double_word_order_badc unit_api_double_word_order_cdab
      unit_config_invalid_float_count unit_config_invalid_double e2e
      PROPERTIES ENVIRONMENT "${_LD}"
    )
  elseif(WIN32)
    set(_PATH "PATH=$<TARGET_FILE_DIR:ioh_modbus>;$ENV{PATH}")
    set_tests_properties(
      unit_stub unit_codec unit_scale_zero unit_api_float_swap unit_api_coils_array
      unit_config_invalid unit_api_fc2 unit_api_fc3_array unit_api_fc15_bad_array
      unit_api_fc16_bad_array unit_api_int16_scale_bounds unit_api_float_nonfinite
      unit_api_fc4_array unit_api_fc3_array_2 unit_api_fc4_array_2
      unit_api_double_read_array unit_api_double_write_number
      unit_api_double_word_order_dcba unit_api_double_word_order_abcd unit_api_double_word_order_badc unit_api_double_word_order_cdab
      unit_config_invalid_float_count unit_config_invalid_double e2e
      PROPERTIES ENVIRONMENT "${_PATH}"
    )
  endif()
endif()

# ----------------
# Install and packaging (CPack)
# ----------------
include(CMakePackageConfigHelpers)

option(PACKAGE_PDB "Include MSVC PDB files in install/package" OFF)

install(TARGETS ioh_modbus EXPORT WebIQModbusIoHandlerTargets
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib)

if(MSVC AND PACKAGE_PDB)
  # Install MSVC program database files (if generated) to aid debugging
  install(TARGETS ioh_modbus PDB DESTINATION bin)
endif()

install(DIRECTORY include/ DESTINATION include)

# Install top-level docs into package root (best-effort)
if(EXISTS "${CMAKE_SOURCE_DIR}/LICENSE")
  install(FILES "${CMAKE_SOURCE_DIR}/LICENSE" DESTINATION .)
endif()
if(EXISTS "${CMAKE_SOURCE_DIR}/README.md")
  install(FILES "${CMAKE_SOURCE_DIR}/README.md" DESTINATION .)
elseif(EXISTS "${CMAKE_SOURCE_DIR}/doc/README.md")
  install(FILES "${CMAKE_SOURCE_DIR}/doc/README.md" DESTINATION . RENAME README.md)
endif()

# Generate and install CMake package config to support find_package()
set(PKG_NAME WebIQModbusIoHandler)
set(INSTALL_CONFIG_DIR lib/cmake/${PKG_NAME})

install(EXPORT WebIQModbusIoHandlerTargets
  FILE ${PKG_NAME}Targets.cmake
  NAMESPACE WebIQ::
  DESTINATION ${INSTALL_CONFIG_DIR})

configure_package_config_file(
  cmake/${PKG_NAME}Config.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/${PKG_NAME}Config.cmake
  INSTALL_DESTINATION ${INSTALL_CONFIG_DIR}
)

if(NOT DEFINED CPACK_PACKAGE_VERSION)
  set(CPACK_PACKAGE_VERSION "0.2.2")
endif()

write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/${PKG_NAME}ConfigVersion.cmake
  VERSION ${CPACK_PACKAGE_VERSION}
  COMPATIBILITY SameMajorVersion
)

install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/${PKG_NAME}Config.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/${PKG_NAME}ConfigVersion.cmake
  DESTINATION ${INSTALL_CONFIG_DIR})

# Basic CPack configuration (archives)
set(CPACK_PACKAGE_NAME "webiq-modbus-iohandler")
set(CPACK_PACKAGE_VENDOR "WebIQ Modbus IOHandler")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "WebIQ Modbus IOHandler library and headers")
set(CPACK_PACKAGE_CONTACT "Project Maintainers")
set(CPACK_GENERATOR "ZIP;TGZ")
set(CPACK_PACKAGE_CHECKSUM "SHA256")
set(CPACK_ARCHIVE_THREADS 0)

# Provide README and LICENSE to CPack (for generators that surface these files)
if(EXISTS "${CMAKE_SOURCE_DIR}/LICENSE")
  set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
endif()
if(EXISTS "${CMAKE_SOURCE_DIR}/README.md")
  set(CPACK_RESOURCE_FILE_README "${CMAKE_SOURCE_DIR}/README.md")
elseif(EXISTS "${CMAKE_SOURCE_DIR}/doc/README.md")
  set(CPACK_RESOURCE_FILE_README "${CMAKE_SOURCE_DIR}/doc/README.md")
endif()
include(CPack)
