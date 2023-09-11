##
 #######################################################################################################################
 #
 #  Copyright (c) 2017-2023 Advanced Micro Devices, Inc. All Rights Reserved.
 #
 #  Permission is hereby granted, free of charge, to any person obtaining a copy
 #  of this software and associated documentation files (the "Software"), to deal
 #  in the Software without restriction, including without limitation the rights
 #  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 #  copies of the Software, and to permit persons to whom the Software is
 #  furnished to do so, subject to the following conditions:
 #
 #  The above copyright notice and this permission notice shall be included in all
 #  copies or substantial portions of the Software.
 #
 #  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 #  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 #  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 #  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 #  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 #  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 #  SOFTWARE.
 #
 #######################################################################################################################

include_guard()

macro(xgl_use_clang_compiler)
    if(UNIX)
        set(CMAKE_CXX_COMPILER_ID "Clang")
        set(CMAKE_C_COMPILER_ID "Clang")

        foreach(major RANGE 6 20)
            find_program(CLANG_VER clang-${major})
            if(CLANG_VER)
                set(CLANG_VER ${major})
                # llvm-ar named llvm-ar-6.0 on ubuntu18.04 for version 6
                if(CLANG_VER EQUAL 6)
                    set(CLANG_VER "6.0")
                endif()
                break()
            endif()
        endforeach()

        find_program(CLANG_C_COMPILER clang)
        if (CLANG_C_COMPILER)
            set(CMAKE_C_COMPILER ${CLANG_C_COMPILER} CACHE FILEPATH "" FORCE)
            EXECUTE_PROCESS(COMMAND ${CLANG_C_COMPILER} --version OUTPUT_VARIABLE clang_full_version_string)
            string(REGEX REPLACE ".*clang version ([0-9]+).*" "\\1" CLANG_VER ${clang_full_version_string})
            # llvm-ar named llvm-ar-6.0 on ubuntu18.04 for version 6
            if(CLANG_VER EQUAL 6)
                set(CLANG_VER "6.0")
            endif()
        else()
            find_program(CLANG_C_COMPILER clang-${CLANG_VER})
            if (CLANG_C_COMPILER)
                set(CMAKE_C_COMPILER ${CLANG_C_COMPILER} CACHE FILEPATH "" FORCE)
            else()
                message(FATAL_ERROR "clang cannot be found!")
            endif()
        endif()

        find_program(CLANG_CXX_COMPILER clang++)
        if (CLANG_CXX_COMPILER)
            set(CMAKE_CXX_COMPILER ${CLANG_CXX_COMPILER} CACHE FILEPATH "" FORCE)
        else()
            find_program(CLANG_CXX_COMPILER clang++-${CLANG_VER})
            if (CLANG_CXX_COMPILER)
                set(CMAKE_CXX_COMPILER ${CLANG_CXX_COMPILER} CACHE FILEPATH "" FORCE)
            else()
                message(FATAL_ERROR "clang++ cannot be found!")
            endif()
        endif()

        find_program(CLANG_AR llvm-ar)
        if (CLANG_AR)
            set(CMAKE_AR ${CLANG_AR} CACHE FILEPATH "" FORCE)
        else()
            find_program(CLANG_AR llvm-ar-${CLANG_VER})
            if (CLANG_AR)
                set(CMAKE_AR ${CLANG_AR} CACHE FILEPATH "" FORCE)
            else()
                message(FATAL_ERROR "llvm-ar cannot be found!")
            endif()
        endif()

        find_program(CLANG_LINKER llvm-link)
        if (CLANG_LINKER)
            set(CMAKE_LINKER ${CLANG_LINKER} CACHE FILEPATH "" FORCE)
        else()
            find_program(CLANG_LINKER llvm-link-${CLANG_VER})
            if (CLANG_LINKER)
                set(CMAKE_LINKER ${CLANG_LINKER} CACHE FILEPATH "" FORCE)
            else()
                message(FATAL_ERROR "llvm-link cannot be found!")
            endif()
        endif()

        find_program(CLANG_NM llvm-nm)
        if (CLANG_NM)
            set(CMAKE_NM ${CLANG_NM} CACHE FILEPATH "" FORCE)
        else()
            find_program(CLANG_NM llvm-nm-${CLANG_VER})
            if (CLANG_NM)
                set(CMAKE_NM ${CLANG_NM} CACHE FILEPATH "" FORCE)
            else()
                message(FATAL_ERROR "llvm-nm cannot be found!")
            endif()
        endif()

        find_program(CLANG_OBJDUMP llvm-objdump)
        if (CLANG_OBJDUMP)
            set(CMAKE_OBJDUMP ${CLANG_OBJDUMP} CACHE FILEPATH "" FORCE)
        else()
            find_program(CLANG_OBJDUMP llvm-objdump-${CLANG_VER})
            if (CLANG_OBJDUMP)
                set(CMAKE_OBJDUMP ${CLANG_OBJDUMP} CACHE FILEPATH "" FORCE)
            else()
                message(FATAL_ERROR "llvm-objdump cannot be found!")
            endif()
        endif()

        find_program(CLANG_RANLIB llvm-ranlib)
        if (CLANG_RANLIB)
            set(CMAKE_RANLIB ${CLANG_RANLIB} CACHE FILEPATH "" FORCE)
        else()
            find_program(CLANG_RANLIB llvm-ranlib-${CLANG_VER})
            if (CLANG_RANLIB)
                set(CMAKE_RANLIB ${CLANG_RANLIB} CACHE FILEPATH "" FORCE)
            else()
                message(FATAL_ERROR "llvm-ranlib cannot be found!")
            endif()
        endif()
    endif()
endmacro()

macro(xgl_set_compiler)
    # Before GCC7, when LTO is enabled, undefined reference error was observed when linking static libraries.
    # Use the gcc-ar wrapper instead of ar, this invokes ar with the right plugin arguments
    # --plugin /usr/lib/gcc/.../liblto_plugin.so
    if(UNIX)
        if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
            find_program(GCC_AR gcc-ar)
            if (GCC_AR)
                set(CMAKE_AR ${GCC_AR})
            else()
                message(FATAL_ERROR "gcc-ar cannot be found!")
            endif()
            find_program(GCC_RANLIB gcc-ranlib)
            if (GCC_RANLIB)
                set(CMAKE_RANLIB ${GCC_RANLIB})
            else()
                message(FATAL_ERROR "gcc-ranlib cannot be found!")
            endif()
        endif()
    endif()

    # Assertions
    if(XGL_ENABLE_ASSERTIONS)
        # MSVC doesn't like _DEBUG on release builds.
        if(NOT MSVC)
            add_definitions(-D_DEBUG)
        endif()
        # On non-Debug builds CMake automatically defines NDEBUG, so we explicitly undefine it:
        if(NOT CMAKE_BUILD_TYPE_DEBUG)
            add_definitions(-UNDEBUG)

            # Also remove /D NDEBUG to avoid MSVC warnings about conflicting defines.
            foreach(flags_var_to_scrub
                CMAKE_CXX_FLAGS_RELEASE
                CMAKE_CXX_FLAGS_RELWITHDEBINFO
                CMAKE_CXX_FLAGS_MINSIZEREL
                CMAKE_C_FLAGS_RELEASE
                CMAKE_C_FLAGS_RELWITHDEBINFO
                CMAKE_C_FLAGS_MINSIZEREL)
                string(REGEX REPLACE "(^| )[/-]D *NDEBUG($| )" " "
                    "${flags_var_to_scrub}" "${${flags_var_to_scrub}}")
            endforeach()
        endif()
    endif()

endmacro()

function(xgl_compiler_options TARGET)
    # Set the C++ standard
    set_target_properties(${TARGET} PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        POSITION_INDEPENDENT_CODE ON
    )

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${TARGET} PRIVATE
            -Wall
            -Wextra

            # Don't warn if a structure’s initializer has some fields missing.
            -Wno-missing-field-initializers

            # Disable warnings about bad/undefined pointer arithmetic
            -Wno-pointer-arith

            # Don't warn whenever a switch statement has an index of enumerated type and
            # lacks a case for one or more of the named codes of that enumeration.
            -Wno-switch

            # This turns off a lot of warnings related to unused code
            # -Wunused-but-set-parameter
            # -Wunused-but-set-variable
            # -Wunused-function
            # -Wunused-label
            # -Wunused-local-typedefs
            # -Wunused-parameter
            # -Wno-unused-result
            # -Wunused-variable
            # -Wunused-const-variable
            # -Wunused-value
            -Wno-unused
            # This is part of -Wextra in clang, so we need to disable it explicitly
            -Wno-unused-parameter
        )

        if(ICD_ANALYSIS_WARNINGS_AS_ERRORS)
            target_compile_options(${TARGET} PRIVATE
                -Werror
                -Wno-error=comment
                -Wno-error=ignored-qualifiers
                -Wno-error=missing-braces
                -Wno-error=pointer-arith
                -Wno-error=unused-parameter
            )
            if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
                target_compile_options(${TARGET} PRIVATE
                    -Wno-error=delete-non-abstract-non-virtual-dtor
                )
            endif()
        endif()

        target_compile_options(${TARGET} PRIVATE
            -pthread

            # Disables exception handling
            -fno-exceptions

            # Disable optimizations that assume strict aliasing rules
            -fno-strict-aliasing

            # Doesn’t guarantee the frame pointer is used in all functions.
            -fno-omit-frame-pointer

            -ffunction-sections

            -fdata-sections

            # Having simple optimization on results in dramatically smaller debug builds (and they actually build faster).
            # This is mostly due to constant-folding and dead-code-elimination of registers.
            $<$<CONFIG:Debug>:
                -Og
            >
        )

        target_link_options(${TARGET} PRIVATE LINKER:--gc-sections)

        target_compile_options(${TARGET} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:
            # Disable run time type information
            # This means breaking dynamic_cast and typeid
            -fno-rtti

            # Do not set errno after calling math functions that are executed with a single instruction
            -fno-math-errno

            -fms-extensions
        >)

        if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            target_compile_options(${TARGET} PRIVATE
                # Output with color if in terminal: https://github.com/ninja-build/ninja/wiki/FAQ
                -fdiagnostics-color=always
                -fno-threadsafe-statics
                -fmerge-all-constants
            )
        elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            target_compile_options(${TARGET} PRIVATE
                # Output with color if in terminal: https://github.com/ninja-build/ninja/wiki/FAQ
                -fcolor-diagnostics

                -Wthread-safety
            )
        endif()

        if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
                OR (CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 7))
            target_compile_options(${TARGET} PRIVATE
                -fno-delete-null-pointer-checks
            )
        endif()

        if(TARGET_ARCHITECTURE_BITS EQUAL 32)
            if(NOT (CMAKE_CXX_COMPILER MATCHES ".*arm-linux-gnueabi.*"))
                target_compile_options(${TARGET} PRIVATE -msse -msse2)
            endif()
        endif()

        if(CMAKE_BUILD_TYPE_RELEASE)
            target_compile_options(${TARGET} PRIVATE -O3)
            if(XGL_ENABLE_LTO)
                if(${CMAKE_CXX_COMPILER_ID} MATCHES "GNU")
                    execute_process(COMMAND ${CMAKE_C_COMPILER} -dumpversion OUTPUT_VARIABLE GCC_VERSION)
                    if(GCC_VERSION VERSION_GREATER 5.3 OR GCC_VERSION VERSION_EQUAL 5.3)
                        # add global definition to enable LTO here since some components have no option
                        # to enable it.
                        add_definitions("-flto -fuse-linker-plugin -Wno-odr")
                        message(WARNING "LTO enabled for ${TARGET}")
                    endif()
                elseif(${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
                    # add global definition to enable LTO here since some components have no option
                    # to enable it.
                    add_definitions("-flto=thin")
                    add_link_options("-flto=thin")
                    message(WARNING "LTO enabled for ${TARGET}")
                endif()
            endif()
        endif()
    else()
        message(FATAL_ERROR "Using unknown compiler")
    endif()

endfunction()
