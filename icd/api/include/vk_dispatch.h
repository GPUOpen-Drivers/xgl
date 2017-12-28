/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2017 Advanced Micro Devices, Inc. All Rights Reserved.
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 *
 **********************************************************************************************************************/
/**
 ***********************************************************************************************************************
 * @file  vk_dispatch.h
 * @brief Dispatch table hooks for Vulkan.
 ***********************************************************************************************************************
 */

#ifndef __VK_DISPATCH_H__
#define __VK_DISPATCH_H__

#pragma once

#include "include/vk_utils.h"
#include "include/khronos/vk_icd.h"
#include "open_strings/g_func_table.h"

namespace vk
{
class Device;
class Instance;

namespace secure { namespace entry { enum EntryPointCondition : uint32_t; } }
};

namespace vk
{

// =====================================================================================================================
// The Dispatchable<> template class is a wrapper around "dispatchable" Vulkan objects, (e.g. VkInstance, VkDevice,
// VkCommandBuffer) that hides handle to object conversion details from the rest of the driver.
//
// The ICD loader as currently designed expects the first thing in any object to be a pointer to a dispatch table.  When
// the loader is called by the application, it dereferences the object handle as a pointer to pointer to dispatch table
// and calls the appropriate entry, which lands us in the driver - as close as possible to the real object.  If we
// just, say, handed a pointer to a class instance back to the application as an object handle, then every class would
// need to explicitly include the dispatch table, and could not derive from anything else.  Virtual functions would not
// work, nor multiple inheritance.
//
// So, the Dispatchable<C> class wraps an instance of "C" (whatever that happens to be) in something that always has the
// ICD dispatch table first.
template <typename C>
class Dispatchable
{
private:
    VK_LOADER_DATA m_reservedForLoader;
    unsigned char  m_C[sizeof(C)];

protected:
    Dispatchable()
    {
        m_reservedForLoader.loaderMagic = ICD_LOADER_MAGIC;
    }

public:
    VK_FORCEINLINE C* operator->()
    {
        return (C*)&m_C[0];
    }

    VK_FORCEINLINE const C* operator->() const
    {
        return (const C*)&m_C[0];
    }

    VK_FORCEINLINE operator C*()
    {
        return (C*)&m_C[0];
    }

    VK_FORCEINLINE operator const C*() const
    {
        return (const C*)&m_C[0];
    }

    // Given pointer to const C, returns the containing Dispatchable<C>.
    static VK_FORCEINLINE const Dispatchable<C>* FromObject(const C* it)
    {
        return reinterpret_cast<const Dispatchable<C>*>(
            reinterpret_cast<const uint8_t*>(it) - (sizeof(Dispatchable<C>) - sizeof(C))
        );
    }

    // Non-const version of above.
    static VK_FORCEINLINE Dispatchable<C>* FromObject(C* it)
    {
        return reinterpret_cast<Dispatchable<C>*>(
            reinterpret_cast<uint8_t*>(it) - (sizeof(Dispatchable<C>) - sizeof(C))
        );
    }

    // Converts a "Vk*" dispatchable handle to the driver internal object pointer.
    static VK_FORCEINLINE C* ObjectFromHandle(typename C::ApiType handle)
    {
        return reinterpret_cast<C*>(
            reinterpret_cast<Dispatchable<C>*>(handle)->m_C);
    }
};

// Helper macro to define a dispatchable driver object
#define VK_DEFINE_DISPATCHABLE(a) \
    class Dispatchable##a : public Dispatchable<a> {};  \
    typedef Dispatchable##a Api##a;

// Helper function to initialize a dispatchable object
#define VK_INIT_DISPATCHABLE(obj_class, storage, constructor_params) \
    do { VK_PLACEMENT_NEW (storage) Dispatchable##obj_class(); \
         VK_PLACEMENT_NEW ((obj_class*)*(Dispatchable##obj_class*)storage) obj_class constructor_params; } while (0)

// Helper function to initialize a dispatchable object
#define VK_INIT_API_OBJECT(obj_class, storage, constructor_params) \
    VK_INIT_DISPATCHABLE(obj_class, storage, constructor_params)

// =====================================================================================================================
// Template base class for non-dispatchable Vulkan objects (e.g. VkImages, VkBuffers, etc.).  Adds type-safe helper
// functions for translating between handles and objects with the assumption that the handle is a pointer to the object.
// Some simpler objects may use their own handle conversion functions instead of those provided by this class.
//
// To use, e.g.: "class Image : public NonDispatchable<VkImage, Image> { /* driver image state */ };"
template <typename ApiType_, typename ObjectType_>
class NonDispatchable
{
public:
    typedef ApiType_ ApiType;
    typedef ObjectType_ ObjectType;

    inline static ObjectType* ObjectFromHandle(ApiType handle)
    {
        return reinterpret_cast<ObjectType*>(IntValueFromHandle(handle));
    }

#if defined (VK_TYPE_SAFE_COMPATIBLE_HANDLES)
    inline static ObjectType* ObjectFromHandle(VkObject handle)
    {
        return reinterpret_cast<ObjectType*>(handle);
    }
#endif // VK_TYPE_SAFE_COMPATIBLE_HANDLES

    inline static const ApiType HandleFromObject(const ObjectType* object)
    {
        return ApiType(reinterpret_cast<uint64_t>(object));
    }

    inline static ApiType HandleFromObject(ObjectType* object)
    {
        return ApiType(reinterpret_cast<uint64_t>(object));
    }

    inline static const ApiType HandleFromVoidPointer(const void* pData)
    {
        return ApiType(reinterpret_cast<uint64_t>(pData));
    }

    inline static ApiType HandleFromVoidPointer(void* pData)
    {
        return ApiType(reinterpret_cast<uint64_t>(pData));
    }

    inline static bool IsNullHandle(ApiType handle)
    {
        return IntValueFromHandle(handle) == 0;
    }

private:
    inline static uint64_t IntValueFromHandle(const ApiType& handle)
    {
    #ifdef ICD_X64_BUILD
        return reinterpret_cast<uint64_t>(handle);
    #else
        return handle;
    #endif
    }
};

// Entry in a dispatch table of Vulkan entry points that maps a name (a secure string) to a function pointer
// implementation.  An array of these makes up a dispatch table.  A list of arrays makes up a set of dispatch tables
// that represents one or more driver-internal layers.  The Instance class owns the official dispatch table stack,
// and implementations can use GetIcdProcAddr() to resolve a name to a function pointer.
struct DispatchTableEntry
{
    const char*                             pName;
    void*                                   pFunc;
    vk::secure::entry::EntryPointCondition  conditionType;
    uint32_t                                conditionValue;
};

// Helper macro for referencing the secure string for a particular Vulkan entry point
#define VK_SECURE_ENTRY(entry_name) vk::secure::entry::entry_name##_name

// Helper macro used to build entries of DispatchTableEntry arrays.
#define VK_DISPATCH_ENTRY(entry_name, entry_func) \
    { \
        VK_SECURE_ENTRY(entry_name), \
        reinterpret_cast<void *>(static_cast<PFN_##entry_name>(entry_func)), \
        entry_name##_condition_type, \
        entry_name##_condition_value, \
    }

// Helper macro used to build alias entries of DispatchTableEntry arrays.
#define VK_DISPATCH_ALIAS(alias_name, entry_name, entry_func) \
    { \
        VK_SECURE_ENTRY(alias_name), \
        reinterpret_cast<void *>(static_cast<PFN_##entry_name>(entry_func)), \
        alias_name##_condition_type, \
        alias_name##_condition_value, \
    }

// Helper macro to identify the end of a Vulkan dispatch table
#define VK_DISPATCH_TABLE_END() { 0, 0 }

extern void* GetIcdProcAddr(
    const Instance*            pInstance,
    const Device*              pDevice,
    uint32_t                   tableCount,
    const DispatchTableEntry** pTables,
    const char*                pSecureEntryName);

extern void GetNextDeviceLayerTable(
    const Instance*            pInstance,
    const Device*              pDevice,
    const DispatchTableEntry*  pCurLayerTable,
    EntryPointTable*           pNextLayerFuncs);

namespace entry
{

extern const DispatchTableEntry g_GlobalDispatchTable[];
extern const DispatchTableEntry g_StandardDispatchTable[];

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(
    VkInstance                                  instance,
    const char*                                 pName);

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetPhysicalDeviceProcAddr(
    VkInstance                                  instance,
    const char*                                 pName);

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetDeviceProcAddr(
    VkDevice                                    device,
    const char*                                 pName);

}

}

#endif /* __VK_DISPATCH_H__ */
