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

#include <ctype.h>
#include <assert.h>
#include <malloc.h>
#include <memory.h>
#include <stdlib.h>

#include "json_reader.h"
#include "vk_utils.h"

namespace vk { namespace utils {

// Context for parsing JSON data
struct JsonContext
{
    JsonSettings settings;              // Copy of settings
    const char*  pStr;                  // Next character in buffer
    size_t       sz;                    // Number of bytes left in buffer
    bool         inSingleLineComment;   // If currently parsing a single-line (//) comment
    bool         inMultiLineComment;    // If currently parsing a multi-line (/* */) comment
};

static bool JsonParseObject(JsonContext* pCtx, char prefix, Json* pObject);
static bool JsonParseArray(JsonContext* pCtx, char prefix, Json* pArray);

// =====================================================================================================================
// Default memory allocator using malloc().  Should never be used.
static void* JsonDefaultAlloc(
    const void*  pUserData,
    size_t       sz)
{
    return malloc(sz);
}

// =====================================================================================================================
// Default memory allocator using free().  Should never be used.
static void JsonDefaultFree(
    const void* pUserData,
    void*       ptr)
{
    free(ptr);
}

// =====================================================================================================================
// Returns the next character after offset entries without advancing the buffer.
static char JsonPeek(
    JsonContext* pCtx,
    size_t       offset = 0)
{
    if (offset < pCtx->sz)
    {
        return pCtx->pStr[offset];
    }
    else
    {
        return '\0';
    }
}

// =====================================================================================================================
// Advances the buffer
static void JsonAdvance(JsonContext* pCtx)
{
    if (pCtx->sz > 0)
    {
        pCtx->pStr++;
        pCtx->sz--;
    }
}

// =====================================================================================================================
// Returns the next character after eating white-space and ignoring comments.  Advances the buffer.
static char JsonNextToken(
    JsonContext* pCtx)
{
    while (true)
    {
        char c = JsonPeek(pCtx);

        if (c == '\0')
        {
            return c;
        }
        else if (pCtx->inSingleLineComment)
        {
            JsonAdvance(pCtx);

            if (c == '\n')
            {
                pCtx->inSingleLineComment = false;
            }
        }
        else if (pCtx->inMultiLineComment)
        {
            if (c == '*' && JsonPeek(pCtx, 1) == '/')
            {
                JsonAdvance(pCtx);
                JsonAdvance(pCtx);
                pCtx->inMultiLineComment = false;
            }
            else
            {
                JsonAdvance(pCtx);
            }
        }
        else if (isspace(static_cast<unsigned char>(c)))
        {
            JsonAdvance(pCtx);
        }
        else if (c == '/')
        {
            char c2 = JsonPeek(pCtx, 1);

            if (c2 == '/')
            {
                JsonAdvance(pCtx);
                JsonAdvance(pCtx);
                pCtx->inSingleLineComment = true;
            }
            else if (c2 == '*')
            {
                JsonAdvance(pCtx);
                JsonAdvance(pCtx);
                pCtx->inMultiLineComment = true;
            }
        }
        else
        {
            JsonAdvance(pCtx);

            return c;
        }
    }
}

// =====================================================================================================================
// Destroys a JSON node and recursively its children.
static void JsonFree(
    const JsonSettings& settings,
    Json*               pItem)
{
    if (pItem == nullptr)
    {
        return;
    }

    if (pItem->pKey != nullptr)
    {
        settings.pfnFree(settings.pUserData, pItem->pKey);
    }

    if (pItem->pStringValue != nullptr)
    {
        settings.pfnFree(settings.pUserData, pItem->pStringValue);
    }

    Json* pChild = pItem->pChild;

    while (pChild != nullptr)
    {
        Json* pNext = pChild->pNext;

        JsonFree(settings, pChild);

        pChild = pNext;
    }

    settings.pfnFree(settings.pUserData, pItem);
}

// =====================================================================================================================
// Creates a new empty JSON node.
static Json* JsonNew(const JsonSettings& settings)
{
    Json* pItem = static_cast<Json*>(settings.pfnAlloc(settings.pUserData, sizeof(Json)));

    if (pItem != nullptr)
    {
        pItem->type         = JsonValueType::String;
        pItem->pKey         = nullptr;
        pItem->pStringValue = nullptr;
        pItem->doubleValue  = 0.0;
        pItem->integerValue = 0;
        pItem->booleanValue = false;
        pItem->pChild       = nullptr;
        pItem->pNext        = nullptr;
    }

    return pItem;
}

// =====================================================================================================================
// Parses a string value until a quote is seen.  Prefix is expected to be '"' and the value of JsonPeek(0) is expected
// to be the first character of the string after the quote.
//
// NOTE: Does not handle escape characters i.e. "blah \"air quotes\" blah" won't work.  Hopefully we won't ever need it.
static bool JsonParseStringValue(
    JsonContext* pCtx,
    char         prefix,
    char**       ppString)
{
    assert(prefix == '"');

    const char* pStart = nullptr;
    const char* pEnd   = nullptr;
    char* pString      = nullptr;

    pStart = pCtx->pStr;

    JsonAdvance(pCtx);

    while (JsonPeek(pCtx) != '\0' && pEnd == nullptr)
    {
        if (JsonPeek(pCtx) == '"')
        {
            pEnd = pCtx->pStr;
        }

        JsonAdvance(pCtx);
    }

    if (pStart != nullptr && pEnd != nullptr)
    {
        size_t len = (pEnd - pStart);

        pString = (char*)pCtx->settings.pfnAlloc(pCtx->settings.pUserData, len + 1);

        if (pString != nullptr)
        {
            memcpy(pString, pStart, len);
            pString[len] = '\0';
        }
    }

    if (pString != nullptr)
    {
        *ppString = pString;
        return true;
    }

    return false;
}

// =====================================================================================================================
// Parses the next character and returns true if it matches the expected value.
static bool JsonParseToken(
    JsonContext* pCtx,
    char         token)
{
    char c = JsonNextToken(pCtx);

    return (c == token);
}

// =====================================================================================================================
// Parses a number value.  Prefix is expected to be either '+' or '-' or a digit and the value of JsonPeek(0) should
// be the second character of the number.
static bool JsonParseNumberValue(
    JsonContext* pCtx,
    char         prefix,
    double*      pValueDouble,
    uint64_t*    pValueInteger)
{
    char buf[128];
    size_t count       = 0;
    bool good          = true;
    bool floatingPoint = false;
    int32_t base       = 10;

    enum State { LeadingSign, DecimalPreHex, Decimal, Fraction, PostExpPreSign, PostExpPostSign, End };

    State state = LeadingSign;

    while (good && state != End)
    {
        char c = (count == 0) ? prefix : JsonPeek(pCtx);

        if (count < sizeof(buf) - 1)
        {
            buf[count++] = c;
        }
        else
        {
            good = false;
        }

        if (state == LeadingSign)
        {
            if (c == '+' || c == '-')
            {
                state = Decimal;
            }
            else if (isdigit(static_cast<unsigned char>(c)))
            {
                state = DecimalPreHex;
            }
            else
            {
                good = false;
            }
        }
        else if (state == DecimalPreHex || state == Decimal)
        {
            if (c == 'x')
            {
                if (state == DecimalPreHex && count == 2 && buf[0] == '0')
                {
                    state = Decimal;
                    base = 16;
                }
                else
                {
                    good = false;
                }
            }
            else if (c == '.')
            {
                floatingPoint = true;
                good &= (base == 10);
                state = Fraction;
            }
            else if (isdigit(static_cast<unsigned char>(c)))
            {
                if (c != '0')
                {
                    state = Decimal;
                }
            }
            else if ((c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))
            {
                good &= (base == 16);
                good &= (state == Decimal);
            }
            else
            {
                state = End;
            }
        }
        else if (state == Fraction)
        {
            if (isdigit(static_cast<unsigned char>(c)))
            {
                state = Fraction;
            }
            else if (c == 'e' || c == 'E')
            {
                state = PostExpPreSign;
            }
            else
            {
                state = End;
            }
        }
        else if (state == PostExpPreSign)
        {
            if (c == '+' || c == '-' || isdigit(static_cast<unsigned char>(c)))
            {
                state = PostExpPostSign;
            }
            else
            {
                good = false;
            }
        }
        else if (state == PostExpPostSign)
        {
            if (isdigit(static_cast<unsigned char>(c)))
            {
                state = PostExpPostSign;
            }
            else
            {
                state = End;
            }
        }
        else
        {
            state = End;
            good = false;
        }

        if (count > 1 && state != End)
        {
            JsonAdvance(pCtx);
        }
    }

    if (good)
    {
        buf[count] = '\0';

        if (floatingPoint)
        {
            *pValueDouble  = strtod(buf, nullptr);
            *pValueInteger = uint64_t(*pValueDouble);
        }
        else
        {
            *pValueInteger = strtoull(buf, nullptr, base);
            *pValueDouble  = double(*pValueInteger);
        }
    }

    return good;
}

// =====================================================================================================================
// Parses "true" or "false" value type.
static bool JsonParseBooleanValue(
    JsonContext* pCtx,
    char         prefix,
    bool*        pValueBool)
{
    bool good = true;

    if (prefix == 't')
    {
        good = good && JsonParseToken(pCtx, 'r');
        good = good && JsonParseToken(pCtx, 'u');
        good = good && JsonParseToken(pCtx, 'e');

        if (good)
        {
            *pValueBool = true;
        }
    }
    else if (prefix == 'f')
    {
        good = good && JsonParseToken(pCtx, 'a');
        good = good && JsonParseToken(pCtx, 'l');
        good = good && JsonParseToken(pCtx, 's');
        good = good && JsonParseToken(pCtx, 'e');

        if (good)
        {
            *pValueBool = false;
        }
    }

    return good;
}

// =====================================================================================================================
// Parses one of the valid value types.
static bool JsonParseValue(
    JsonContext* pCtx,
    char         prefix,
    Json*        pItem)
{
    if (prefix == '"')
    {
        pItem->type = JsonValueType::String;
        return JsonParseStringValue(pCtx, prefix, &pItem->pStringValue);
    }
    else if (isdigit(static_cast<unsigned char>(prefix)) || prefix == '+' || prefix == '-')
    {
        pItem->type = JsonValueType::Number;
        bool good = JsonParseNumberValue(pCtx, prefix, &pItem->doubleValue, &pItem->integerValue);

        if (good)
        {
            pItem->booleanValue = (pItem->integerValue == 1);
        }

        return good;
    }
    else if (prefix == '{')
    {
        pItem->type = JsonValueType::Object;

        return JsonParseObject(pCtx, prefix, pItem);
    }
    else if (prefix == '[')
    {
        pItem->type = JsonValueType::Array;

        return JsonParseArray(pCtx, prefix, pItem);
    }
    else if (prefix == 't' || prefix == 'f')
    {
        pItem->type = JsonValueType::Boolean;

        bool good = JsonParseBooleanValue(pCtx, prefix, &pItem->booleanValue);

        if (good)
        {
            pItem->integerValue = static_cast<uint64_t>(pItem->booleanValue);
            pItem->doubleValue  = static_cast<double>(pItem->integerValue);
        }

        return good;
    }

    return false;
}

// =====================================================================================================================
// Parses an object JSON value
static bool JsonParseObject(
    JsonContext* pCtx,
    char         prefix,
    Json*        pObject)
{
    bool good = true;

    Json* pPrevChild = nullptr;

    while (good)
    {
        char c = JsonNextToken(pCtx);

        if (c == '}')
        {
            break;
        }

        Json* pChild = JsonNew(pCtx->settings);

        if (pChild != nullptr)
        {
            if (pPrevChild != nullptr)
            {
                pPrevChild->pNext = pChild;
            }

            if (pObject->pChild == nullptr)
            {
                pObject->pChild = pChild;
            }

            pPrevChild = pChild;
        }
        else
        {
            good = false;
        }

        good = good && JsonParseStringValue(pCtx, c, &pChild->pKey);
        good = good && (pChild->pKey != nullptr);
        good = good && JsonParseToken(pCtx, ':');
        good = good && JsonParseValue(pCtx, JsonNextToken(pCtx), pChild);

        c = JsonNextToken(pCtx);

        if (c == '}')
        {
            break;
        }

        good = (c == ',');
    }

    return good;
}

// =====================================================================================================================
// Parses an array JSON value.
static bool JsonParseArray(
    JsonContext* pCtx,
    char         prefix,
    Json*        pArray)
{
    bool good = true;

    Json* pPrevChild = nullptr;

    while (good)
    {
        char c = JsonNextToken(pCtx);

        if (c == ']')
        {
            break;
        }

        Json* pChild = JsonNew(pCtx->settings);

        if (pChild != nullptr)
        {
            if (pPrevChild != nullptr)
            {
                pPrevChild->pNext = pChild;
            }

            if (pArray->pChild == nullptr)
            {
                pArray->pChild = pChild;
            }

            pPrevChild = pChild;
        }
        else
        {
            good = false;
        }

        good = good && JsonParseValue(pCtx, c, pChild);

        c = JsonNextToken(pCtx);

        if (c == ']')
        {
            break;
        }

        good = (c == ',');
    }

    return good;
}

// =====================================================================================================================
// Fills settings with defaults when missing information.
static JsonSettings JsonFillSettings(
    const JsonSettings* pSettings)
{
    JsonSettings settings = {};

    if (pSettings != nullptr)
    {
        settings = *pSettings;
    }

    if (settings.pfnAlloc == nullptr || settings.pfnFree == nullptr)
    {
        settings.pfnAlloc = &JsonDefaultAlloc;
        settings.pfnFree = &JsonDefaultFree;
    }

    return settings;
}

// =====================================================================================================================
// Parses a buffer of JSON text into a Json* node hierarchy.  If an error is occurred while parsing, nullptr is
// returned.
Json* JsonParse(
    const JsonSettings& settings,
    const void*         pJson,
    size_t              sz)
{
    JsonContext ctx = {};

    ctx.settings = JsonFillSettings(&settings);
    ctx.pStr     = (const char*)pJson;
    ctx.sz       = sz;

    Json* pRoot = JsonNew(ctx.settings);

    char prefix = JsonNextToken(&ctx);

    if (JsonParseValue(&ctx, prefix, pRoot) == false)
    {
        JsonFree(ctx.settings, pRoot);

        pRoot = nullptr;
    }

    return pRoot;
}

// =====================================================================================================================
// Destroys a JSON node hierarchy.
void JsonDestroy(
    const JsonSettings& settings,
    Json*               pJson)
{
    JsonFree(settings, pJson);
}

// =====================================================================================================================
// Returns the array size of a JSON Array value type.
size_t JsonArraySize(Json* pJson)
{
    size_t count = 0;

    if (pJson->type == JsonValueType::Array)
    {
        for (const Json* pElem = pJson->pChild; pElem != nullptr; pElem = pElem->pNext)
        {
            count++;
        }
    }

    return count;
}

// =====================================================================================================================
// Returns the i-th array element value of a JSON Array.
Json* JsonArrayElement(Json* pJson, size_t index)
{
    if (pJson->type == JsonValueType::Array)
    {
        for (Json* pElem = pJson->pChild; pElem != nullptr; pElem = pElem->pNext)
        {
            if (index == 0)
            {
                return pElem;
            }

            index--;
        }
    }

    return nullptr;
}

// =====================================================================================================================
// Helper allocator function for Vulkan instances
void* JsonInstanceAlloc(
    const void*  pUserData,
    size_t       sz)
{
    const VkAllocationCallbacks* pAllocCB = static_cast<const VkAllocationCallbacks*>(pUserData);

    return pAllocCB->pfnAllocation(pAllocCB->pUserData, sz, VK_DEFAULT_MEM_ALIGN,  VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE);
}

// =====================================================================================================================
// Helper allocator function for Vulkan instances
void JsonInstanceFree(
    const void* pUserData,
    void*       pPtr)
{
    const VkAllocationCallbacks* pAllocCB = static_cast<const VkAllocationCallbacks*>(pUserData);

    pAllocCB->pfnFree(pAllocCB->pUserData, pPtr);
}

// =====================================================================================================================
// Returns a JSON settings structure compatible with allocating memory through a Vulkan instance.
JsonSettings JsonMakeInstanceSettings(
    const VkAllocationCallbacks* pAllocCB)
{
    JsonSettings settings = {};

    settings.pfnAlloc  = &JsonInstanceAlloc;
    settings.pfnFree   = &JsonInstanceFree;
    settings.pUserData = pAllocCB;

    return settings;
}

// =====================================================================================================================
// Finds an object's child value by key
Json* JsonGetValue(
    Json*       pObject,
    const char* pKey,
    bool        deep)
{
    Json* pValue = nullptr;

    if (pObject != nullptr && pObject->type == JsonValueType::Object)
    {
        for (Json* pChild = pObject->pChild; pChild != nullptr; pChild = pChild->pNext)
        {
            if (pChild->pKey != nullptr && (strcmp(pKey, pChild->pKey) == 0))
            {
                pValue = pChild;

                break;
            }
        }
    }

    if (deep && pValue == nullptr)
    {
        for (Json* pChild = pObject->pChild; pChild != nullptr; pChild = pChild->pNext)
        {
            pValue = JsonGetValue(pChild, pKey, deep);

            if (pValue != nullptr)
            {
                break;
            }
        }
    }

    return pValue;
}

}; };
