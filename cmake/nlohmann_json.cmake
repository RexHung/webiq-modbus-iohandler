include(FetchContent)

set(NLOHMANN_JSON_RELEASE v3.11.2)
set(NLOHMANN_JSON_TAR "https://github.com/nlohmann/json/releases/download/${NLOHMANN_JSON_RELEASE}/json.tar.xz")
set(NLOHMANN_JSON_TAR_SHA256 "8c4b26bf4b422252e13f332bc5e388ec0ab5c3443d24399acb675e68278d341f")

FetchContent_Declare(
  nlohmann_json
  URL "${NLOHMANN_JSON_TAR}"
  URL_HASH SHA256=${NLOHMANN_JSON_TAR_SHA256}
  DOWNLOAD_NO_EXTRACT FALSE
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_GetProperties(nlohmann_json)
if(NOT nlohmann_json_POPULATED)
  FetchContent_Populate(nlohmann_json)
endif()

set(_NLOHMANN_JSON_INCLUDE_DIR "${nlohmann_json_SOURCE_DIR}/include")
if(NOT EXISTS "${_NLOHMANN_JSON_INCLUDE_DIR}/nlohmann/json.hpp")
  message(FATAL_ERROR "nlohmann_json headers not found in '${_NLOHMANN_JSON_INCLUDE_DIR}'")
endif()

if(NOT TARGET nlohmann_json_header_only)
  add_library(nlohmann_json_header_only INTERFACE)
  target_include_directories(nlohmann_json_header_only INTERFACE "${_NLOHMANN_JSON_INCLUDE_DIR}")
  add_library(nlohmann_json::nlohmann_json ALIAS nlohmann_json_header_only)
endif()
