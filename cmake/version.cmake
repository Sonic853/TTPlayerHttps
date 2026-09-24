set(TTP_HTTPS_BUILD_VERSION "" CACHE STRING "Beijing yyyy.MM.dd[pN] version; empty selects the current date")
find_program(TTP_HTTPS_POWERSHELL NAMES powershell pwsh REQUIRED)
set(_https_version_args)
if(NOT TTP_HTTPS_BUILD_VERSION STREQUAL "")
  list(APPEND _https_version_args -Version "${TTP_HTTPS_BUILD_VERSION}")
endif()
add_custom_target(ttp_https_version
  COMMAND "${TTP_HTTPS_POWERSHELL}" -NoProfile -ExecutionPolicy Bypass
    -File "${CMAKE_CURRENT_SOURCE_DIR}/cmake/write_version.ps1"
    -Template "${CMAKE_CURRENT_SOURCE_DIR}/src/version.rc.in"
    -OutputPath "${CMAKE_CURRENT_BINARY_DIR}/generated/version.rc"
    ${_https_version_args}
  BYPRODUCTS "${CMAKE_CURRENT_BINARY_DIR}/generated/version.rc"
  COMMENT "Updating HTTPS file and product versions"
  VERBATIM)
