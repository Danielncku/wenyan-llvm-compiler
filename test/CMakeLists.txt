include(${CMAKE_SOURCE_DIR}/cmake/wenyan-toolchain.cmake)

add_wenyan_executable(program_input "${CMAKE_CURRENT_SOURCE_DIR}/input.wy")

# gen: generate .verbose and .expected for all test .wy files
add_custom_target(gen
    COMMAND ${CMAKE_SOURCE_DIR}/scripts/gen-test-files.sh
            --build-dir ${CMAKE_BINARY_DIR}
            --force
            ${CMAKE_CURRENT_SOURCE_DIR}/策問
            ${CMAKE_CURRENT_SOURCE_DIR}/殿試
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    DEPENDS wy wyc wenyan_runtime
    COMMENT "Generating .verbose and .expected test reference files"
    VERBATIM
)
