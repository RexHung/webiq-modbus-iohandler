cmake_minimum_required(VERSION 3.19)
project(webiq_modbus_consumer LANGUAGES CXX)

# Consumer-side: discover installed package
find_package(WebIQModbusIoHandler CONFIG REQUIRED)

add_executable(consumer main.cpp)
target_compile_features(consumer PRIVATE cxx_std_14)
target_link_libraries(consumer PRIVATE WebIQ::ioh_modbus)

