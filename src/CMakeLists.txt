cmake_minimum_required(VERSION 3.20)
project(bw2qr)
set(TARGET_EXE "bw2qr-exe")
set(TARGET_NAME "bw2qr")

# set required c++ version
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# set project source-files
set(SOURCE_FILES
  bw2qr.cpp)
set(HEADER_FILES
  html_template.h)
set(RESOURCE_FILES
  resource.h
  Resource.rc)

# compile executable
add_executable(${TARGET_EXE} ${SOURCE_FILES} ${HEADER_FILES} ${RESOURCE_FILES})
set_target_properties(${TARGET_EXE} PROPERTIES OUTPUT_NAME ${TARGET_NAME})

# list of required third-party libraries
find_package(unofficial-nayuki-qr-code-generator CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(httplib CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(winpp CONFIG REQUIRED)

# set project compile definitions
target_compile_definitions(${TARGET_EXE}
  PRIVATE
    FMT_HEADER_ONLY
    CPPHTTPLIB_OPENSSL_SUPPORT
    WIN32_LEAN_AND_MEAN)

# force utf-8 encoding for source-files
add_compile_options($<$<C_COMPILER_ID:MSVC>:/utf-8>)
add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/utf-8>)

# generic configuration
get_filename_component(BUILD_DIRECTORY ${CMAKE_BINARY_DIR} DIRECTORY)
string(CONCAT CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded"
  $<$<CONFIG:Debug>:DebugDLL>
  $<$<NOT:$<CONFIG:Debug>>:DLL>)
string(CONCAT BUILD_PREFIX_PATH
  $<$<CONFIG:Debug>:${PACKAGE_PREFIX_DIR}/debug>
  $<$<NOT:$<CONFIG:Debug>>:${PACKAGE_PREFIX_DIR}>)
target_link_options(${TARGET_EXE}
  PRIVATE
    $<$<NOT:$<CONFIG:Debug>>:/LTCG>
    $<$<NOT:$<CONFIG:Debug>>:/INCREMENTAL:NO>)

# link third-party libraries
target_link_directories(${TARGET_EXE} 
  PRIVATE 
    ${BUILD_PREFIX_PATH}/lib)
target_link_libraries(${TARGET_EXE}
  PRIVATE
    $<$<PLATFORM_ID:Windows>:ws2_32>
    $<$<PLATFORM_ID:Windows>:crypt32>
    unofficial::nayuki-qr-code-generator::nayuki-qr-code-generator
    OpenSSL::Crypto
    OpenSSL::SSL
    httplib::httplib
    nlohmann_json::nlohmann_json
    fmt::fmt-header-only
    winpp::winpp)

# compress executable using upx
if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
  if(EXISTS ${BUILD_DIRECTORY}/upx.exe)
    add_custom_command(
      TARGET ${TARGET_EXE} POST_BUILD 
      COMMAND ${BUILD_DIRECTORY}/upx.exe -9q $<TARGET_FILE:${TARGET_EXE}>)
  endif()
endif()

# install executable
install(TARGETS ${TARGET_EXE} EXPORT ${TARGET_EXE}Config
  RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

# organize files for visual-studio
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
source_group("Headers Files" FILES ${HEADER_FILES})
source_group("Resources Files" FILES ${RESOURCE_FILES})
source_group("Sources Files" FILES ${SOURCE_FILES})