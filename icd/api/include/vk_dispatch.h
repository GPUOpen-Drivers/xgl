/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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
#include "include/vk_extensions.h"
#include "strings/g_func_table.h"

namespace vk
{

class Device;
class Instance;

namespace EntryPoint
{

// Entry point type
enum class Type : uint32_t
{
    GLOBAL,                 // Global entry point
    INSTANCE,               // Instance-level entry point
    DEVICE,                 // Device-level entry point
    PHYSDEVICE,             // Physical-device-level entry point
};

// Entry point metadata
struct Metadata
{
    const char*             pName;
    Type                    type;
};

}

// Dispatch table class
class DispatchTable
{
public:
    // Dispatch table type
    enum class Type : uint32_t
    {
        GLOBAL,             // Global dispatch table
        INSTANCE,           // Instance dispatch table
        DEVICE,             // Device dispatch table
    };

    DispatchTable(
        Type                type        = Type::INSTANCE,
        const Instance*     pInstance   = nullptr,
        const Device*       pDevice     = nullptr);

    void Init();

    VK_FORCEINLINE const Device* GetDevice()
    {
        return m_pDevice;
    }

    VK_FORCEINLINE Type GetType() const
    {
        return m_type;
    }

    VK_FORCEINLINE const EntryPoints& GetEntryPoints() const
    {
        return m_func;
    }

    VK_FORCEINLINE PFN_vkVoidFunction GetEntryPoint(uint32_t index) const
    {
        VK_ASSERT(index < VKI_ENTRY_POINT_COUNT);
        return m_table[index];
    }

    PFN_vkVoidFunction GetEntryPoint(const char* pName) const;

    PFN_vkVoidFunction GetPhysicalDeviceEntryPoint(const char* pName) const;

    VK_FORCEINLINE EntryPoints* OverrideEntryPoints()
    {
        return &m_func;
    }

    VK_FORCEINLINE void OverrideEntryPoint(uint32_t index, PFN_vkVoidFunction func)
    {
        m_table[index] = func;
    }

    bool CheckAPIVersion(uint32_t apiVersion);
    bool CheckInstanceExtension(InstanceExtensions::ExtensionId id);
    bool CheckDeviceExtension(DeviceExtensions::ExtensionId id);

protected:

    union
    {
        EntryPoints             m_func;
        PFN_vkVoidFunction      m_table[VKI_ENTRY_POINT_COUNT];
    };

    Type                        m_type;

    const Instance*             m_pInstance;
    const Device*               m_pDevice;
};

static_assert(sizeof(EntryPoints) == sizeof(PFN_vkVoidFunction) * VKI_ENTRY_POINT_COUNT, "Entry point count mismatch");

extern const DispatchTable          g_GlobalDispatchTable;

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

    static VK_FORCEINLINE uint64_t IntValueFromHandle(Dispatchable<C>* handle)
    {
        return reinterpret_cast<uint64_t>(handle);
    }
};

// Helper macro to define a dispatchable driver object
#define VK_DEFINE_DISPATCHABLE(a) \
    class Api##a : public Dispatchable<a> {};

// Helper function to initialize a dispatchable object
#define VK_INIT_DISPATCHABLE(obj_class, storage, constructor_params) \
    do { VK_PLACEMENT_NEW (storage) Api##obj_class(); \
         VK_PLACEMENT_NEW ((obj_class*)*(Api##obj_class*)storage) obj_class constructor_params; } while (0)

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
#endif

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

    inline static uint64_t IntValueFromHandle(const ApiType& handle)
    {
#ifdef ICD_X64_BUILD
        return reinterpret_cast<uint64_t>(handle);
#else
        return handle;
#endif
    }

private:
};

namespace entry
{

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
