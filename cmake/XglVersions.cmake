##
 #######################################################################################################################
 #
 #  Copyright (c) 2022-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

# This will become the value of PAL_CLIENT_INTERFACE_MAJOR_VERSION.  It describes the version of the PAL interface
# that the ICD supports.  PAL uses this value to enable backwards-compatibility for older interface versions.
# It must be updated on each PAL promotion after handling all of the interface changes described in palLib.h.
set(ICD_PAL_CLIENT_MAJOR_VERSION "819")

# This will become the value of GPUOPEN_CLIENT_INTERFACE_MAJOR_VERSION if ICD_GPUOPEN_DEVMODE_BUILD=1.
# It describes the interface version of the gpuopen shared module (part of PAL) that the ICD supports.
set(ICD_GPUOPEN_CLIENT_MAJOR_VERSION "42")

#if VKI_RAY_TRACING
# This will become the value of GPURT_CLIENT_INTERFACE_MAJOR_VERSION if VKI_RAY_TRACING=1.
# It describes the interface version of the GpuRT shared module that the ICD supports.
set(ICD_GPURT_CLIENT_MAJOR_VERSION "39")
#endif

# This will become the value of LLPC_CLIENT_INTERFACE_MAJOR_VERSION if ICD_BUILD_LLPC=1.
# It describes the version of the interface version of LLPC that the ICD supports.
set(ICD_LLPC_CLIENT_MAJOR_VERSION "66")
