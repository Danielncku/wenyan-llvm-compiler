cmake_minimum_required(VERSION 3.10)
project(NCKU_Compiler_HW2 C)

set(CMAKE_C_STANDARD 11)

find_package(BISON 3.6 REQUIRED)
find_package(FLEX REQUIRED)

if(BISON_VERSION VERSION_GREATER_EQUAL "3.8")
    set(BISON_EXTRA_FLAGS "-v -Wcounterexamples")
else()
    set(BISON_EXTRA_FLAGS "-v")
endif()

# Define Source Files
set(TEST_DIR ${CMAKE_SOURCE_DIR}/test)
set(SRC_DIR ${CMAKE_SOURCE_DIR}/src)
set(LIB_DIR ${CMAKE_SOURCE_DIR}/lib)
set(LEX_SRC ${SRC_DIR}/compiler.l)
set(YAC_SRC ${SRC_DIR}/compiler.y)

# Define Output Locations for Generated Files
set(GENERATED_DIR ${CMAKE_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${GENERATED_DIR})

# Set C flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -O0")

# Bison Target
BISON_TARGET(
        CompilerParser
        ${YAC_SRC}
        ${GENERATED_DIR}/y.tab.c
        DEFINES_FILE ${GENERATED_DIR}/y.tab.h
        COMPILE_FLAGS "${BISON_EXTRA_FLAGS}"
)

# Flex Target
FLEX_TARGET(
        CompilerScanner
        ${LEX_SRC}
        ${GENERATED_DIR}/lex.yy.c
)

add_flex_bison_dependency(CompilerScanner CompilerParser)

# Include Generated Files Directory
include_directories(
        ${GENERATED_DIR}
        ${SRC_DIR}
        ${LIB_DIR}
)

# Runtime library
add_library(wenyan_runtime STATIC ${SRC_DIR}/wy_rt/wy_rt.c ${LIB_DIR}/utf8.c/utf8.c)
set_target_properties(wenyan_runtime PROPERTIES
        OUTPUT_NAME wenyan-runtime
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)

add_subdirectory(src)

# Wenyan compiler
add_executable(
        wy
        ${COMPILER_SOURCES}
        ${LIB_DIR}/utf8.c/utf8.c
        ${BISON_CompilerParser_OUTPUTS}
        ${FLEX_CompilerScanner_OUTPUTS}
)
target_link_libraries(wy m)

# CLI wrapper for end users (requires llc and gcc in PATH at runtime)
# wenyan_runtime is not linked into wyc, but must be built so wyc can pass -lwenyan-runtime to gcc at runtime
add_executable(wyc ${SRC_DIR}/wyc.c)
add_dependencies(wyc wy wenyan_runtime)

# Install rules
include(GNUInstallDirs)

install(TARGETS wy wyc
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(TARGETS wenyan_runtime
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(FILES cmake/wenyan-toolchain.cmake
        DESTINATION ${CMAKE_INSTALL_DATADIR}/wenyan/cmake
)

# CMake package config for downstream find_package(Wenyan)
include(CMakePackageConfigHelpers)

configure_package_config_file(
        ${CMAKE_SOURCE_DIR}/cmake/WenyanConfig.cmake.in
        ${CMAKE_BINARY_DIR}/WenyanConfig.cmake
        INSTALL_DESTINATION ${CMAKE_INSTALL_DATADIR}/wenyan/cmake
)

install(FILES ${CMAKE_BINARY_DIR}/WenyanConfig.cmake
        DESTINATION ${CMAKE_INSTALL_DATADIR}/wenyan/cmake
)

# Tests
if (EXISTS ${TEST_DIR})
    add_subdirectory(test)
endif ()
