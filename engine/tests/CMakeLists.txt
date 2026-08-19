# Lightweight smoke tests using plain assert() - deliberately no extra test-framework
# dependency for a Phase-1 scaffold. Revisit (Catch2, via vcpkg) once test surface grows.
add_executable(fusion_engine_test unit/fusion_engine_test.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Types.cpp
    ${CMAKE_SOURCE_DIR}/src/core/FusionEngine.cpp
)
target_include_directories(fusion_engine_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(fusion_engine_test PRIVATE nlohmann_json::nlohmann_json)
add_test(NAME fusion_engine_test COMMAND fusion_engine_test)
