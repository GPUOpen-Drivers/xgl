//
// File: vk_platform.h
//
/*
** Copyright (c) 2014-2020 The Khronos Group Inc.
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#ifndef VK_PLATFORM_H_
#define VK_PLATFORM_H_

#ifdef __cplusplus
extern "C"
{
#endif // __cplusplus

/*
***************************************************************************************************
*   Platform-specific directives and type declarations
***************************************************************************************************
*/

/* Platform-specific calling convention macros.
 *
 * Platforms should define these so that Vulkan clients call Vulkan commands
 * with the same calling conventions that the Vulkan implementation expects.
 *
 * VKAPI_ATTR - Placed before the return type in function declarations.
 *              Useful for C++11 and GCC/Clang-style function attribute syntax.
 * VKAPI_CALL - Placed after the return type in function declarations.
 *              Useful for MSVC-style calling convention syntax.
 * VKAPI_PTR  - Placed between the '(' and '*' in function pointer types.
 *
 * Function declaration:  VKAPI_ATTR void VKAPI_CALL vkCommand(void);
 * Function pointer type: typedef void (VKAPI_PTR *PFN_vkCommand)(void);
 */
    // On other platforms, use the default calling convention
    #define VKAPI_ATTR
    #define VKAPI_CALL
    #define VKAPI_PTR

#include <stddef.h>

#if !defined(VK_NO_STDINT_H)
        #include <stdint.h>
#endif // !defined(VK_NO_STDINT_H)

#ifdef __cplusplus
} // extern "C"
#endif // __cplusplus

#endif
