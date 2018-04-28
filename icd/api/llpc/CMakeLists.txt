##
 #######################################################################################################################
 #
 #  Copyright (c) 2017-2018 Advanced Micro Devices, Inc. All Rights Reserved.
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

project(LLPC C CXX)

### Create LLPC Library ################################################################################################
add_library(llpc STATIC "")
add_dependencies(llpc LLVMCore)

### Cached Project Options #############################################################################################
# None

### Compiler Options ###################################################################################################
if(UNIX)
    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        # SEE: https://gcc.gnu.org/onlinedocs/gcc-6.2.0/gcc/Option-Summary.html#Option-Summary
        # for a list of all options and documentation.
        #target_compile_options(llpc PRIVATE option)
        target_compile_options(llpc PRIVATE
            -Wno-unused-parameter
            -Wno-type-limits
            -Wno-switch
            -Wno-parentheses
            -Wno-maybe-uninitialized
            -Wno-delete-non-virtual-dtor
            -Wno-sign-compare
            -Wno-delete-incomplete
        )

        target_compile_options(llpc PRIVATE $<$<COMPILE_LANGUAGE:CXX>:
            -Wno-unused
            -Wno-ignored-qualifiers
            -Wno-missing-field-initializers
        >)

        target_compile_options(llpc PRIVATE -fno-strict-aliasing)

        target_compile_options(llpc PRIVATE $<$<COMPILE_LANGUAGE:CXX>:
            -std=c++0x
            -fno-rtti
            -fPIC
        >)

        message(STATUS "Configured ${PROJECT_NAME} compiler options for GCC.")
    elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
        #message(STATUS "Configured compiler options for Clang.")
        message(WARNING "Clang is untested.")
    else()
        message(FATAL_ERROR "Using unknown compiler.")
    endif()
elseif(WIN32)
    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
        # CMAKE-TODO: These are /W4 (level 4) warnings
        target_compile_options(llpc
            PRIVATE # Warnings in interface and src
                /wd4005 # 'DEBUG' : macro redefinition ??? Defined in toolchain ??? importedllvmincludellvm/Support/Debug.h
                /wd4018 # '<' : signed/unsigned mismatch
                /wd4100 # unreferenced formal parameter
                /wd4127 # conditional expression is constant
                /wd4141 # 'inline' : used more than once
                /wd4146 # unary minus operator applied to unsigned type, result still unsigned
                /wd4189 # local variable is initialized but not referenced
                /wd4201 # nonstandard extension used : nameless struct/union
                /wd4244 # 'X' : conversion from 'Y' to 'Z', possible loss of data
                /wd4245 # 'X' : conversion from 'Y' to 'Z', signed/unsigned mismatch
                /wd4291 # no matching operator delete found; memory will not be freed if initialization throws an exception
                /wd4267 # 'return' : conversion from 'size_t' to 'unsigned int', possible loss of data
                /wd4389 # '==' : signed/unsigned mismatch
                /wd4505 # unreferenced local function has been removed
                /wd4510 # default constructor could not be generated
                /wd4512 # assignment operator could not be generated
                /wd4610 # struct 'X' can never be instantiated - user defined constructor required
                /wd4624 # destructor could not be generated because a base class destructor is inaccessible or deleted
                /wd4702 # unreachable code
                /wd4706 # assignment within conditional expression
                /wd4800 # forcing value to bool 'true' or 'false' (performance warning)
                /wd6246 # Local declaration of 'S' hides declaration of the same name in outer scope
                /wd6323 # Use of arithmetic operator on Boolean type(s)
        )

        target_compile_definitions(llpc PRIVATE _SCL_SECURE_NO_WARNINGS)
        target_compile_definitions(llpc PRIVATE _CRT_SECURE_NO_WARNINGS)

        message(STATUS "Configured ${PROJECT_NAME} compiler options for MSVC.")
    else()
        message(FATAL_ERROR "Using unknown compiler")
    endif()
endif()

### Defines/Includes/Sources ###########################################################################################
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${XGL_LLVM_BUILD_PATH}/lib/cmake/llvm)
include(LLVMConfig)
message(STATUS "LLVM execuables: " ${LLVM_TOOLS_BINARY_DIR})
message(STATUS "LLVM libraries: " ${LLVM_BUILD_LIBRARY_DIR})
execute_process(
    COMMAND ${LLVM_TOOLS_BINARY_DIR}/llvm-config --libs amdgpu analysis bitreader bitwriter codegen irreader linker mc passes support target transformutils
    OUTPUT_VARIABLE LLVM_LINK_FLAGS
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
message(STATUS "LLVM link options:" ${LLVM_LINK_FLAGS})

target_compile_definitions(llpc PRIVATE ${TARGET_ARCHITECTURE_ENDIANESS}ENDIAN_CPU)
target_compile_definitions(llpc PRIVATE _SPIRV_LLVM_API)

if(XGL_LLVM_UPSTREAM)
    target_compile_definitions(llpc PRIVATE XGL_LLVM_UPSTREAM=1)
endif()

if(WIN32)
    target_compile_definitions(llpc PRIVATE
        NOMINMAX    # windows.h defines min/max which conflicts with the use of std::min / max
        UNICODE     # CMAKE-TODO: What is this used for?
        _UNICODE
    )
endif()

target_include_directories(llpc
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
    PRIVATE
        ${PROJECT_SOURCE_DIR}/context
        ${PROJECT_SOURCE_DIR}/imported/metrohash/src
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/lower
        ${PROJECT_SOURCE_DIR}/patch
        ${PROJECT_SOURCE_DIR}/patch/gfx6/chip
        ${PROJECT_SOURCE_DIR}/patch/gfx9/chip
        ${PROJECT_SOURCE_DIR}/patch/gfx9
        ${PROJECT_SOURCE_DIR}/patch/generate
        ${PROJECT_SOURCE_DIR}/translator
        ${PROJECT_SOURCE_DIR}/translator/libSPIRV
        ${PROJECT_SOURCE_DIR}/translator/Mangler
        ${PROJECT_SOURCE_DIR}/util
        ${XGL_PAL_PATH}/src/core/hw/gfxip/gfx6/chip
        ${XGL_PAL_PATH}/src/core/hw/gfxip/gfx9/chip
        ${XGL_PAL_PATH}/inc/core
        ${XGL_PAL_PATH}/inc/util
        ${LLVM_INCLUDE_DIRS}
)

# CMAKE-TODO: Figure out a better way to do this.
target_include_directories(llpc PRIVATE ${PROJECT_SOURCE_DIR}/../include/khronos)

if(WIN32)
    target_compile_definitions(llpc PRIVATE VK_USE_PLATFORM_WIN32_KHR)
    target_compile_definitions(llpc PRIVATE VKI_SDK_1_1=1)
endif()

# llpc/context
target_sources(llpc PRIVATE
    context/llpcCompiler.cpp
    context/llpcContext.cpp
    context/llpcComputeContext.cpp
    context/llpcGraphicsContext.cpp
    context/llpcShaderCache.cpp
    context/llpcCopyShader.cpp
    context/llpcPipelineContext.cpp
    context/llpcShaderCacheManager.cpp
)

# llpc/imported/metrohash/src
target_sources(llpc PRIVATE
    imported/metrohash/src/metrohash128.cpp
    imported/metrohash/src/metrohash64.cpp
)

# llpc/lower
target_sources(llpc PRIVATE
    lower/llpcSpirvLower.cpp
    lower/llpcSpirvLowerAccessChain.cpp
    lower/llpcSpirvLowerAggregateLoadStore.cpp
    lower/llpcSpirvLowerAlgebraTransform.cpp
    lower/llpcSpirvLowerBufferOp.cpp
    lower/llpcSpirvLowerConstImmediateStore.cpp
    lower/llpcSpirvLowerDynIndex.cpp
    lower/llpcSpirvLowerGlobal.cpp
    lower/llpcSpirvLowerImageOp.cpp
    lower/llpcSpirvLowerOpt.cpp
    lower/llpcSpirvLowerResourceCollect.cpp
)

# llpc/patch
target_sources(llpc PRIVATE
    patch/gfx6/chip/llpcGfx6Chip.cpp
    patch/gfx6/chip/llpcGfx6ConfigBuilder.cpp
    patch/gfx9/chip/llpcGfx9Chip.cpp
    patch/gfx9/chip/llpcGfx9ConfigBuilder.cpp
    patch/gfx9/llpcShaderMerger.cpp
    patch/llpcCodeGenManager.cpp
    patch/llpcFragColorExport.cpp
    patch/llpcPatch.cpp
    patch/llpcPatchAddrSpaceMutate.cpp
    patch/llpcPatchDescriptorLoad.cpp
    patch/llpcPatchEntryPointMutate.cpp
    patch/llpcPatchInOutImportExport.cpp
    patch/llpcPatchBufferOp.cpp
    patch/llpcPatchImageOp.cpp
    patch/llpcPatchPushConstOp.cpp
    patch/llpcPatchResourceCollect.cpp
    patch/llpcVertexFetch.cpp
)

# llpc/translator
target_sources(llpc PRIVATE
    translator/OCL20To12.cpp
    translator/OCL20ToSPIRV.cpp
    translator/OCL21ToSPIRV.cpp
    translator/OCLTypeToSPIRV.cpp
    translator/OCLUtil.cpp
    translator/SPIRVLowerBool.cpp
    translator/SPIRVLowerConstExpr.cpp
    translator/SPIRVLowerOCLBlocks.cpp
    translator/SPIRVReader.cpp
    translator/SPIRVRegularizeLLVM.cpp
    translator/SPIRVToOCL20.cpp
    translator/SPIRVUtil.cpp
    translator/SPIRVWriter.cpp
    translator/SPIRVWriterPass.cpp
    translator/TransOCLMD.cpp
)

target_sources(llpc PRIVATE
    translator/libSPIRV/SPIRVBasicBlock.cpp
    translator/libSPIRV/SPIRVDebug.cpp
    translator/libSPIRV/SPIRVDecorate.cpp
    translator/libSPIRV/SPIRVEntry.cpp
    translator/libSPIRV/SPIRVFunction.cpp
    translator/libSPIRV/SPIRVInstruction.cpp
    translator/libSPIRV/SPIRVModule.cpp
    translator/libSPIRV/SPIRVStream.cpp
    translator/libSPIRV/SPIRVType.cpp
    translator/libSPIRV/SPIRVValue.cpp
)

target_sources(llpc PRIVATE
    translator/Mangler/FunctionDescriptor.cpp
    translator/Mangler/Mangler.cpp
    translator/Mangler/ManglingUtils.cpp
    translator/Mangler/ParameterType.cpp
)

# llpc/util
target_sources(llpc PRIVATE
    util/llpcElf.cpp
    util/llpcDebug.cpp
    util/llpcInternal.cpp
    util/llpcFile.cpp
    util/llpcPassDeadFuncRemove.cpp
    util/llpcPassExternalLibLink.cpp
    util/llpcPassNonNativeFuncRemove.cpp
    util/llpcPipelineDumper.cpp
    util/llpcUtil.cpp
)

### LLPC Auto-generated Files ##########################################################################################
set(OP_EMU_LIB_GEN_DIR ${PROJECT_SOURCE_DIR}/patch/generate)
set(GEN_GLSL_OP_EMU_LIB_SCRIPT ${OP_EMU_LIB_GEN_DIR}/script/genGlslEmuLib.py)

if(UNIX)
    set(BUILD_OS lnx)
elseif(WIN32)
    set(BUILD_OS win)
endif()

set(GEN_GLSL_EMU_LIB_DEPS
    ${OP_EMU_LIB_GEN_DIR}/glslArithOpEmu.ll
    ${OP_EMU_LIB_GEN_DIR}/glslArithOpEmuF64.ll
    ${OP_EMU_LIB_GEN_DIR}/glslBufferOpEmu.ll
    ${OP_EMU_LIB_GEN_DIR}/glslBuiltInVarEmu.ll
    ${OP_EMU_LIB_GEN_DIR}/glslCopyShaderEmu.ll
    ${OP_EMU_LIB_GEN_DIR}/glslImageOpEmu.ll
    ${OP_EMU_LIB_GEN_DIR}/glslInlineConstOpEmu.ll
    ${OP_EMU_LIB_GEN_DIR}/glslMatrixOpEmu.ll
    ${OP_EMU_LIB_GEN_DIR}/glslMatrixOpEmuF16.ll
    ${OP_EMU_LIB_GEN_DIR}/glslMatrixOpEmuF64.ll
    ${OP_EMU_LIB_GEN_DIR}/glslPushConstOpEmu.ll
    ${OP_EMU_LIB_GEN_DIR}/glslNullFsEmu.ll
    ${OP_EMU_LIB_GEN_DIR}/glslSharedVarOpEmu.ll
    ${OP_EMU_LIB_GEN_DIR}/glslSpecialOpEmu.ll
    ${OP_EMU_LIB_GEN_DIR}/glslSpecialOpEmuD16.ll
    ${OP_EMU_LIB_GEN_DIR}/gfx8/glslSpecialOpEmu.ll
    ${OP_EMU_LIB_GEN_DIR}/gfx9/glslArithOpEmu.ll
    ${OP_EMU_LIB_GEN_DIR}/script/genGlslArithOpEmuCode.py
    ${OP_EMU_LIB_GEN_DIR}/script/genGlslArithOpEmuCode.txt
    ${OP_EMU_LIB_GEN_DIR}/script/genGlslArithOpEmuCodeF64.txt
    ${OP_EMU_LIB_GEN_DIR}/script/genGlslEmuLib.py
    ${OP_EMU_LIB_GEN_DIR}/script/genGlslImageOpEmuCode.py
    ${OP_EMU_LIB_GEN_DIR}/script/genGlslImageOpEmuCode.txt
    ${OP_EMU_LIB_GEN_DIR}/script/genGlslOpEmuCodeUtil.py
)

if(UNIX)
    set(LLVM_BIN_DIR ${XGL_LLVM_BUILD_PATH}/bin/)
elseif(WIN32)
    set(LLVM_BIN_DIR ${XGL_LLVM_BUILD_PATH}/\$\(Configuration\)/bin/)
endif()

add_custom_target(
    RunHeapGlslOpEmuLibGenerator
    COMMAND ${PYTHON_CMD} ${GEN_GLSL_OP_EMU_LIB_SCRIPT} ${LLVM_BIN_DIR}  ${LLVM_BIN_DIR}  ${BUILD_OS}
    DEPENDS ${GEN_OP_EMU_LIB_FILES}
    COMMENT "Checking if re-generation is required for g_llpcGlslOpEmuLib.h"
)

add_dependencies(RunHeapGlslOpEmuLibGenerator llvm-ar llvm-link llvm-as)
add_dependencies(llpc RunHeapGlslOpEmuLibGenerator)

### Link Libraries #####################################################################################################
# CMAKE-TODO: LLVM_LIB_DIR should probably be set in the op level CMake?
# Maybe add XGL_LLVM_PATH?
# How are these built? Can they be built through CMake?

### Visual Studio Filters ##############################################################################################
target_vs_filters(llpc)

### Create Standalone Compiler ############################################################################################
add_executable(amdllpc tool/amdllpc.cpp)
add_dependencies(amdllpc llpc)

target_compile_definitions(amdllpc PRIVATE ${TARGET_ARCHITECTURE_ENDIANESS}ENDIAN_CPU)
target_compile_definitions(amdllpc PRIVATE _SPIRV_LLVM_API)

target_include_directories(amdllpc
PUBLIC
    ${PROJECT_SOURCE_DIR}/include
PRIVATE
    ${PROJECT_SOURCE_DIR}/context
    ${PROJECT_SOURCE_DIR}/imported/spirv
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/lower
    ${PROJECT_SOURCE_DIR}/patch
    ${PROJECT_SOURCE_DIR}/patch/gfx6/chip
    ${PROJECT_SOURCE_DIR}/patch/gfx9/chip
    ${PROJECT_SOURCE_DIR}/patch/generate
    ${PROJECT_SOURCE_DIR}/translator
    ${PROJECT_SOURCE_DIR}/translator/libSPIRV
    ${PROJECT_SOURCE_DIR}/translator/Mangler
    ${PROJECT_SOURCE_DIR}/util
    ${XGL_PAL_PATH}/src/core/hw/gfxip/gfx6/chip
    ${XGL_PAL_PATH}/src/core/hw/gfxip/gfx9/chip
    ${XGL_PAL_PATH}/inc/core
    ${XGL_PAL_PATH}/inc/util
    ${LLVM_INCLUDE_DIRS}
)

target_include_directories(amdllpc PRIVATE ${PROJECT_SOURCE_DIR}/../include/khronos)

if(UNIX)
    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        target_compile_options(amdllpc PRIVATE -fno-strict-aliasing)
        target_compile_options(amdllpc PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-std=c++0x -fno-rtti>)

        target_compile_options(amdllpc PRIVATE -Wno-unused-parameter -Wno-shift-negative-value -Wno-type-limits -Wno-error=switch -Wno-error=sign-compare -Wno-error=parentheses -Wno-error=maybe-uninitialized -Wno-error=delete-non-virtual-dtor -Wno-sign-compare -Wno-error)
        target_compile_options(amdllpc PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wno-unused -Wno-unused-parameter -Wno-ignored-qualifiers -Wno-missing-field-initializers>)

        message(STATUS "Configured ${PROJECT_NAME} compiler options for GCC.")
    elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
        #message(STATUS "Configured compiler options for Clang.")
        message(WARNING "Clang is untested.")
    else()
        message(FATAL_ERROR "Using unknown compiler.")
    endif()
endif()

if(UNIX)
    target_link_libraries(amdllpc PRIVATE llpc dl stdc++)
elseif(WIN32)
    target_link_libraries(amdllpc PRIVATE llpc)
endif()
    llvm_map_components_to_libnames(llvm_libs amdgpucodegen amdgpuinfo amdgpuasmparser amdgpudisassembler LTO ipo analysis bitreader bitwriter codegen irreader linker mc passes support target transformutils coroutines aggressiveinstcombine)
target_link_libraries(amdllpc PRIVATE ${llvm_libs})
