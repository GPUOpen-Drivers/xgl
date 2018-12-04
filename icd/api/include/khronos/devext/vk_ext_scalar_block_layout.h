/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2018 Advanced Micro Devices, Inc. All Rights Reserved.
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
 **********************************************************************************************************************
 * @file  vk_ext_scalar_block_layout.h
 * @brief
 **********************************************************************************************************************
 */
#ifndef VK_EXT_SCALAR_BLOCK_LAYOUT_H_
#define VK_EXT_SCALAR_BLOCK_LAYOUT_H_

#define VK_EXT_scalar_block_layout 1
#define VK_EXT_SCALAR_BLOCK_LAYOUT_SPEC_VERSION        1
#define VK_EXT_SCALAR_BLOCK_LAYOUT_EXTENSION_NAME      "VK_EXT_scalar_block_layout"
#define VK_EXT_SCALAR_BLOCK_LAYOUT_EXTENSION_NUMBER    222

#define VK_EXT_SCALAR_BLOCK_LAYOUT_ENUM(type, offset) \
    VK_EXTENSION_ENUM(VK_EXT_SCALAR_BLOCK_LAYOUT_EXTENSION_NUMBER, type, offset)

#define VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES_EXT \
    VK_EXT_SCALAR_BLOCK_LAYOUT_ENUM(VkStructureType, 0)

typedef struct VkPhysicalDeviceScalarBlockLayoutFeaturesEXT {
    VkStructureType    sType;
    void*              pNext;
    VkBool32           scalarBlockLayout;
} VkPhysicalDeviceScalarBlockLayoutFeaturesEXT;

#endif /* VK_EXT_SCALAR_BLOCK_LAYOUT_H_ */
