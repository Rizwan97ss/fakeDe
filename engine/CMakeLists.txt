cmake_minimum_required(VERSION 3.21)

# Auto-detect the locally bootstrapped vcpkg (engine/vcpkg, see scripts/bootstrap-vcpkg.ps1)
# unless the caller already passed a toolchain file explicitly.
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(_local_vcpkg "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake")
    if(EXISTS "${_local_vcpkg}")
        set(CMAKE_TOOLCHAIN_FILE "${_local_vcpkg}" CACHE STRING "vcpkg toolchain file")
    endif()
endif()

project(fakede_engine CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT DEFINED VCPKG_TARGET_TRIPLET)
    set(VCPKG_TARGET_TRIPLET "x64-windows")
endif()

if(WIN32)
    # windows.h's min/max macros (pulled in transitively via Drogon/OpenCV/ONNX Runtime
    # headers) would otherwise shadow std::min/std::max/std::clamp used throughout the
    # analyzers.
    add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

find_package(Drogon CONFIG REQUIRED)
find_package(OpenCV CONFIG REQUIRED COMPONENTS core imgproc imgcodecs)
find_package(exiv2 CONFIG REQUIRED)
find_package(unofficial-sqlite3 CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

# ONNX Runtime: official prebuilt distribution (not vcpkg's from-source port — see
# docs/ARCHITECTURE.md for why). Fetched by scripts/fetch_onnxruntime.ps1 into
# engine/onnxruntime/. Point ONNXRUNTIME_ROOT_DIR elsewhere to use a different copy.
if(NOT DEFINED ONNXRUNTIME_ROOT_DIR)
    set(ONNXRUNTIME_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/onnxruntime")
endif()
if(NOT EXISTS "${ONNXRUNTIME_ROOT_DIR}/include/onnxruntime_cxx_api.h")
    message(WARNING
        "ONNX Runtime not found at ${ONNXRUNTIME_ROOT_DIR}. "
        "Run scripts/fetch_onnxruntime.ps1 first, or set -DONNXRUNTIME_ROOT_DIR=<path>.")
endif()
add_library(onnxruntime SHARED IMPORTED)
set_target_properties(onnxruntime PROPERTIES
    IMPORTED_LOCATION "${ONNXRUNTIME_ROOT_DIR}/lib/onnxruntime.dll"
    IMPORTED_IMPLIB "${ONNXRUNTIME_ROOT_DIR}/lib/onnxruntime.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${ONNXRUNTIME_ROOT_DIR}/include"
)

add_executable(fakede_engine
    src/main.cpp
    src/core/Types.cpp
    src/core/AnalyzerRegistry.cpp
    src/core/FusionEngine.cpp
    src/core/JobStore.cpp
    src/onnx/OnnxSession.cpp
    src/util/FileTypeSniffer.cpp
    src/util/Base64.cpp
    src/api/AnalysisController.cpp
    src/analyzers/image/MetadataAnalyzer.cpp
    src/analyzers/image/ElaAnalyzer.cpp
    src/analyzers/image/FrequencyAnalyzer.cpp
    src/analyzers/image/NoiseResidualAnalyzer.cpp
    src/analyzers/image/AiGeneratedModelAnalyzer.cpp
    src/analyzers/text/TextStylometryAnalyzer.cpp
    src/analyzers/text/Gpt2Tokenizer.cpp
    src/analyzers/text/TextPerplexityAnalyzer.cpp
    src/analyzers/document/PdfForensicsAnalyzer.cpp
)

target_include_directories(fakede_engine PRIVATE src)

# Bake the source-tree models/ path in as the default so the engine finds its models
# regardless of the process's working directory (Visual Studio's multi-config layout
# runs the exe from build/Release/, not engine/) - FAKEDE_MODELS_DIR still overrides
# this for real deployments where the exe ships without the source tree alongside it.
target_compile_definitions(fakede_engine PRIVATE
    FAKEDE_DEFAULT_MODELS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/models")

target_link_libraries(fakede_engine PRIVATE
    Drogon::Drogon
    opencv_core opencv_imgproc opencv_imgcodecs
    Exiv2::exiv2lib
    unofficial::sqlite3::sqlite3
    nlohmann_json::nlohmann_json
    onnxruntime
)

# Copy the ONNX Runtime DLL next to the built executable so it's found at runtime.
add_custom_command(TARGET fakede_engine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${ONNXRUNTIME_ROOT_DIR}/lib/onnxruntime.dll"
        "$<TARGET_FILE_DIR:fakede_engine>"
    COMMAND_EXPAND_LISTS
)

enable_testing()
add_subdirectory(tests)
