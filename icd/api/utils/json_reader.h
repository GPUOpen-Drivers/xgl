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
**************************************************************************************************
* @file  json_reader.h
* @brief A simple JSON reader.
**************************************************************************************************
*/
#ifndef __JSON_READER_H__
#define __JSON_READER_H__
#pragma once

#include <stdint.h>

typedef struct VkAllocationCallbacks VkAllocationCallbacks;

namespace vk
{

namespace utils
{

// List of valid JSON value types
enum class JsonValueType
{
    Object,
    Array,
    String,
    Number,
    Boolean
};

// Basic JSON node representing either a value or a key:value pair.  JSON data is composed of a tree of these nodes.
struct Json
{
    char*         pKey;         // A string describing the key.  May be empty (e.g. for array elements)
    JsonValueType type;         // Type of value.
    char*         pStringValue; // A string value type.  Valid when type is String.
    double        doubleValue;  // A double-cast value type.  Valid when type is Number or Boolean.
    uint64_t      integerValue; // An integer-cast value type.  Valid when type is Number or Boolean.
    bool          booleanValue; // A boolean value type.  Valid when type is Number or Boolean.
    Json*         pChild;       // List of child key:value pairs.  Valid when type is Object or Array.
    Json*         pNext;        // Next pointer in a list of key:value pairs.
};

// Settings structure for parsing JSON data.
struct JsonSettings
{
    // Allocator function for allocating memory Json nodes.  If nullptr, malloc() is used.
    void* (*pfnAlloc)(const void* pUserData, size_t sz);

    // Free function for freeing memory used by Json nodes.  If nullptr, free() is used.
    void (*pfnFree)(const void* pUserData, void* ptr);

    // A user-provided value to the allocator functions.
    const void* pUserData;
};

// Parse a JSON string from a buffer into a tree of Json nodes.
extern Json* JsonParse(const JsonSettings& settings, const void* pJson, size_t sz);

// Destroy a tree of JSON nodes.
extern void JsonDestroy(const JsonSettings& settings, Json* pJson);

// For JSON arrays, returns the array size.
extern size_t JsonArraySize(Json* pJson);

// For JSON arrays, returns the i-th array element.
extern Json* JsonArrayElement(Json* pJson, size_t index);

// Helper allocator function for Vulkan instances
extern void* JsonInstanceAlloc(const void* pUserData, size_t sz);

// Helper allocator function for Vulkan instances
extern void JsonInstanceFree(const void* pUserData, void* pPtr);

// Returns a JSON settings structure compatible with allocating memory through a Vulkan instance.
extern JsonSettings JsonMakeInstanceSettings(const VkAllocationCallbacks* pAllocCB);

// Finds an object's child value by key
extern Json* JsonGetValue(Json* pObject, const char* pKey, bool deep = false);

};

};
#endif
