# Wenyan Toolchain for CMake
# Include this file to compile Wenyan (.wy) source files within a CMake project.
# Requires: wy, wenyan_runtime targets (from the wenyan-llvm project or an imported install)

include_guard(GLOBAL)

# Locate llc and gcc if not already found
if(NOT LLC)
    find_program(LLC llc)
endif()
if(NOT GCC)
    find_program(GCC gcc)
endif()

# add_wenyan_ir(TARGET_NAME INPUT_WY_FILE)
# Creates a named CMake target that compiles INPUT_WY_FILE (.wy) to LLVM IR (.ll).
# Output: ${CMAKE_BINARY_DIR}/wenyan/<name>.ll
function(add_wenyan_ir TARGET_NAME INPUT_WY_FILE)
    if(NOT TARGET wy)
        message(FATAL_ERROR "'wy' target must exist before calling add_wenyan_ir.")
    endif()

    get_filename_component(_NAME_WE ${INPUT_WY_FILE} NAME_WE)
    set(_LL_FILE "${CMAKE_BINARY_DIR}/wenyan/${_NAME_WE}.ll")
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/wenyan")

    add_custom_command(
        OUTPUT ${_LL_FILE}
        COMMAND wy ${INPUT_WY_FILE} ${_LL_FILE}
        DEPENDS wy ${INPUT_WY_FILE}
        COMMENT "Compiling ${_NAME_WE}.wy -> LLVM IR"
        VERBATIM
    )

    add_custom_target(${TARGET_NAME} DEPENDS ${_LL_FILE})
endfunction()

# add_wenyan_executable(TARGET_NAME INPUT_WY_FILE [OUTPUT_VAR])
# Creates a named CMake target that compiles INPUT_WY_FILE (.wy) to an executable.
# Output: ${CMAKE_BINARY_DIR}/wenyan/<name>[exe suffix]
# Optional OUTPUT_VAR: variable name to receive the output executable path
function(add_wenyan_executable TARGET_NAME INPUT_WY_FILE)
    foreach(_req wy wenyan_runtime)
        if(NOT TARGET ${_req})
            message(FATAL_ERROR "'${_req}' target must exist before calling add_wenyan_executable.")
        endif()
    endforeach()
    if(NOT LLC)
        message(FATAL_ERROR "llc not found. Install llc or set LLC.")
    endif()
    if(NOT GCC)
        message(FATAL_ERROR "gcc not found. Install gcc or set GCC.")
    endif()

    get_filename_component(_NAME_WE ${INPUT_WY_FILE} NAME_WE)
    set(_LL_FILE  "${CMAKE_BINARY_DIR}/wenyan/${_NAME_WE}.ll")
    set(_OBJ_FILE "${CMAKE_BINARY_DIR}/wenyan/${_NAME_WE}.o")
    set(_EXE_FILE "${CMAKE_BINARY_DIR}/wenyan/${_NAME_WE}${CMAKE_EXECUTABLE_SUFFIX}")
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/wenyan")

    # .wy -> .ll
    add_custom_command(
        OUTPUT ${_LL_FILE}
        COMMAND wy ${INPUT_WY_FILE} ${_LL_FILE}
        DEPENDS wy ${INPUT_WY_FILE}
        COMMENT "Compiling ${_NAME_WE}.wy -> LLVM IR"
        VERBATIM
    )

    # .ll -> .o
    add_custom_command(
        OUTPUT ${_OBJ_FILE}
        COMMAND ${LLC} -relocation-model=pic -filetype=obj ${_LL_FILE} -o ${_OBJ_FILE}
        DEPENDS ${_LL_FILE}
        COMMENT "Compiling ${_NAME_WE}.ll -> object"
        VERBATIM
    )

    # .o -> exe
    add_custom_command(
        OUTPUT ${_EXE_FILE}
        COMMAND ${GCC} -fPIE ${_OBJ_FILE} -L$<TARGET_FILE_DIR:wenyan_runtime> -lwenyan-runtime -o ${_EXE_FILE}
        DEPENDS ${_OBJ_FILE} wenyan_runtime
        COMMENT "Linking ${_NAME_WE} -> executable"
        VERBATIM
    )

    add_custom_target(${TARGET_NAME} DEPENDS ${_EXE_FILE})

    if(ARGC GREATER 2)
        set(${ARGV2} ${_EXE_FILE} PARENT_SCOPE)
    endif()
endfunction()
