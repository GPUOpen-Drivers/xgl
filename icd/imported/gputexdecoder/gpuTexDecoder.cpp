/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2021-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  gpuTexDecoder.cpp
 * @brief Contains implementation of gpuTexDecoder object.
 ***********************************************************************************************************************
 */
#include "gpuTexDecoder.h"
#include "shaders.h"
#include "palMetroHash.h"
#include "palHashMapImpl.h"
#include "palMemTrackerImpl.h"

namespace GpuTexDecoder
{
struct TritsQuintsTable
{
    const int TritsOfInteger[256][5] =
    {
        {0, 0, 0, 0, 0}, {1, 0, 0, 0, 0}, {2, 0, 0, 0, 0}, {0, 0, 2, 0, 0},
        {0, 1, 0, 0, 0}, {1, 1, 0, 0, 0}, {2, 1, 0, 0, 0}, {1, 0, 2, 0, 0},
        {0, 2, 0, 0, 0}, {1, 2, 0, 0, 0}, {2, 2, 0, 0, 0}, {2, 0, 2, 0, 0},
        {0, 2, 2, 0, 0}, {1, 2, 2, 0, 0}, {2, 2, 2, 0, 0}, {2, 0, 2, 0, 0},
        {0, 0, 1, 0, 0}, {1, 0, 1, 0, 0}, {2, 0, 1, 0, 0}, {0, 1, 2, 0, 0},
        {0, 1, 1, 0, 0}, {1, 1, 1, 0, 0}, {2, 1, 1, 0, 0}, {1, 1, 2, 0, 0},
        {0, 2, 1, 0, 0}, {1, 2, 1, 0, 0}, {2, 2, 1, 0, 0}, {2, 1, 2, 0, 0},
        {0, 0, 0, 2, 2}, {1, 0, 0, 2, 2}, {2, 0, 0, 2, 2}, {0, 0, 2, 2, 2},
        {0, 0, 0, 1, 0}, {1, 0, 0, 1, 0}, {2, 0, 0, 1, 0}, {0, 0, 2, 1, 0},
        {0, 1, 0, 1, 0}, {1, 1, 0, 1, 0}, {2, 1, 0, 1, 0}, {1, 0, 2, 1, 0},
        {0, 2, 0, 1, 0}, {1, 2, 0, 1, 0}, {2, 2, 0, 1, 0}, {2, 0, 2, 1, 0},
        {0, 2, 2, 1, 0}, {1, 2, 2, 1, 0}, {2, 2, 2, 1, 0}, {2, 0, 2, 1, 0},
        {0, 0, 1, 1, 0}, {1, 0, 1, 1, 0}, {2, 0, 1, 1, 0}, {0, 1, 2, 1, 0},
        {0, 1, 1, 1, 0}, {1, 1, 1, 1, 0}, {2, 1, 1, 1, 0}, {1, 1, 2, 1, 0},
        {0, 2, 1, 1, 0}, {1, 2, 1, 1, 0}, {2, 2, 1, 1, 0}, {2, 1, 2, 1, 0},
        {0, 1, 0, 2, 2}, {1, 1, 0, 2, 2}, {2, 1, 0, 2, 2}, {1, 0, 2, 2, 2},
        {0, 0, 0, 2, 0}, {1, 0, 0, 2, 0}, {2, 0, 0, 2, 0}, {0, 0, 2, 2, 0},
        {0, 1, 0, 2, 0}, {1, 1, 0, 2, 0}, {2, 1, 0, 2, 0}, {1, 0, 2, 2, 0},
        {0, 2, 0, 2, 0}, {1, 2, 0, 2, 0}, {2, 2, 0, 2, 0}, {2, 0, 2, 2, 0},
        {0, 2, 2, 2, 0}, {1, 2, 2, 2, 0}, {2, 2, 2, 2, 0}, {2, 0, 2, 2, 0},
        {0, 0, 1, 2, 0}, {1, 0, 1, 2, 0}, {2, 0, 1, 2, 0}, {0, 1, 2, 2, 0},
        {0, 1, 1, 2, 0}, {1, 1, 1, 2, 0}, {2, 1, 1, 2, 0}, {1, 1, 2, 2, 0},
        {0, 2, 1, 2, 0}, {1, 2, 1, 2, 0}, {2, 2, 1, 2, 0}, {2, 1, 2, 2, 0},
        {0, 2, 0, 2, 2}, {1, 2, 0, 2, 2}, {2, 2, 0, 2, 2}, {2, 0, 2, 2, 2},
        {0, 0, 0, 0, 2}, {1, 0, 0, 0, 2}, {2, 0, 0, 0, 2}, {0, 0, 2, 0, 2},
        {0, 1, 0, 0, 2}, {1, 1, 0, 0, 2}, {2, 1, 0, 0, 2}, {1, 0, 2, 0, 2},
        {0, 2, 0, 0, 2}, {1, 2, 0, 0, 2}, {2, 2, 0, 0, 2}, {2, 0, 2, 0, 2},
        {0, 2, 2, 0, 2}, {1, 2, 2, 0, 2}, {2, 2, 2, 0, 2}, {2, 0, 2, 0, 2},
        {0, 0, 1, 0, 2}, {1, 0, 1, 0, 2}, {2, 0, 1, 0, 2}, {0, 1, 2, 0, 2},
        {0, 1, 1, 0, 2}, {1, 1, 1, 0, 2}, {2, 1, 1, 0, 2}, {1, 1, 2, 0, 2},
        {0, 2, 1, 0, 2}, {1, 2, 1, 0, 2}, {2, 2, 1, 0, 2}, {2, 1, 2, 0, 2},
        {0, 2, 2, 2, 2}, {1, 2, 2, 2, 2}, {2, 2, 2, 2, 2}, {2, 0, 2, 2, 2},
        {0, 0, 0, 0, 1}, {1, 0, 0, 0, 1}, {2, 0, 0, 0, 1}, {0, 0, 2, 0, 1},
        {0, 1, 0, 0, 1}, {1, 1, 0, 0, 1}, {2, 1, 0, 0, 1}, {1, 0, 2, 0, 1},
        {0, 2, 0, 0, 1}, {1, 2, 0, 0, 1}, {2, 2, 0, 0, 1}, {2, 0, 2, 0, 1},
        {0, 2, 2, 0, 1}, {1, 2, 2, 0, 1}, {2, 2, 2, 0, 1}, {2, 0, 2, 0, 1},
        {0, 0, 1, 0, 1}, {1, 0, 1, 0, 1}, {2, 0, 1, 0, 1}, {0, 1, 2, 0, 1},
        {0, 1, 1, 0, 1}, {1, 1, 1, 0, 1}, {2, 1, 1, 0, 1}, {1, 1, 2, 0, 1},
        {0, 2, 1, 0, 1}, {1, 2, 1, 0, 1}, {2, 2, 1, 0, 1}, {2, 1, 2, 0, 1},
        {0, 0, 1, 2, 2}, {1, 0, 1, 2, 2}, {2, 0, 1, 2, 2}, {0, 1, 2, 2, 2},
        {0, 0, 0, 1, 1}, {1, 0, 0, 1, 1}, {2, 0, 0, 1, 1}, {0, 0, 2, 1, 1},
        {0, 1, 0, 1, 1}, {1, 1, 0, 1, 1}, {2, 1, 0, 1, 1}, {1, 0, 2, 1, 1},
        {0, 2, 0, 1, 1}, {1, 2, 0, 1, 1}, {2, 2, 0, 1, 1}, {2, 0, 2, 1, 1},
        {0, 2, 2, 1, 1}, {1, 2, 2, 1, 1}, {2, 2, 2, 1, 1}, {2, 0, 2, 1, 1},
        {0, 0, 1, 1, 1}, {1, 0, 1, 1, 1}, {2, 0, 1, 1, 1}, {0, 1, 2, 1, 1},
        {0, 1, 1, 1, 1}, {1, 1, 1, 1, 1}, {2, 1, 1, 1, 1}, {1, 1, 2, 1, 1},
        {0, 2, 1, 1, 1}, {1, 2, 1, 1, 1}, {2, 2, 1, 1, 1}, {2, 1, 2, 1, 1},
        {0, 1, 1, 2, 2}, {1, 1, 1, 2, 2}, {2, 1, 1, 2, 2}, {1, 1, 2, 2, 2},
        {0, 0, 0, 2, 1}, {1, 0, 0, 2, 1}, {2, 0, 0, 2, 1}, {0, 0, 2, 2, 1},
        {0, 1, 0, 2, 1}, {1, 1, 0, 2, 1}, {2, 1, 0, 2, 1}, {1, 0, 2, 2, 1},
        {0, 2, 0, 2, 1}, {1, 2, 0, 2, 1}, {2, 2, 0, 2, 1}, {2, 0, 2, 2, 1},
        {0, 2, 2, 2, 1}, {1, 2, 2, 2, 1}, {2, 2, 2, 2, 1}, {2, 0, 2, 2, 1},
        {0, 0, 1, 2, 1}, {1, 0, 1, 2, 1}, {2, 0, 1, 2, 1}, {0, 1, 2, 2, 1},
        {0, 1, 1, 2, 1}, {1, 1, 1, 2, 1}, {2, 1, 1, 2, 1}, {1, 1, 2, 2, 1},
        {0, 2, 1, 2, 1}, {1, 2, 1, 2, 1}, {2, 2, 1, 2, 1}, {2, 1, 2, 2, 1},
        {0, 2, 1, 2, 2}, {1, 2, 1, 2, 2}, {2, 2, 1, 2, 2}, {2, 1, 2, 2, 2},
        {0, 0, 0, 1, 2}, {1, 0, 0, 1, 2}, {2, 0, 0, 1, 2}, {0, 0, 2, 1, 2},
        {0, 1, 0, 1, 2}, {1, 1, 0, 1, 2}, {2, 1, 0, 1, 2}, {1, 0, 2, 1, 2},
        {0, 2, 0, 1, 2}, {1, 2, 0, 1, 2}, {2, 2, 0, 1, 2}, {2, 0, 2, 1, 2},
        {0, 2, 2, 1, 2}, {1, 2, 2, 1, 2}, {2, 2, 2, 1, 2}, {2, 0, 2, 1, 2},
        {0, 0, 1, 1, 2}, {1, 0, 1, 1, 2}, {2, 0, 1, 1, 2}, {0, 1, 2, 1, 2},
        {0, 1, 1, 1, 2}, {1, 1, 1, 1, 2}, {2, 1, 1, 1, 2}, {1, 1, 2, 1, 2},
        {0, 2, 1, 1, 2}, {1, 2, 1, 1, 2}, {2, 2, 1, 1, 2}, {2, 1, 2, 1, 2},
        {0, 2, 2, 2, 2}, {1, 2, 2, 2, 2}, {2, 2, 2, 2, 2}, {2, 1, 2, 2, 2}
    };
    const int QuintsOfInteger[128][3] =
    {
        {0, 0, 0},	{1, 0, 0},	{2, 0, 0},	{3, 0, 0},
        {4, 0, 0},	{0, 4, 0},	{4, 4, 0},	{4, 4, 4},
        {0, 1, 0},	{1, 1, 0},	{2, 1, 0},	{3, 1, 0},
        {4, 1, 0},	{1, 4, 0},	{4, 4, 1},	{4, 4, 4},
        {0, 2, 0},	{1, 2, 0},	{2, 2, 0},	{3, 2, 0},
        {4, 2, 0},	{2, 4, 0},	{4, 4, 2},	{4, 4, 4},
        {0, 3, 0},	{1, 3, 0},	{2, 3, 0},	{3, 3, 0},
        {4, 3, 0},	{3, 4, 0},	{4, 4, 3},	{4, 4, 4},
        {0, 0, 1},	{1, 0, 1},	{2, 0, 1},	{3, 0, 1},
        {4, 0, 1},	{0, 4, 1},	{4, 0, 4},	{0, 4, 4},
        {0, 1, 1},	{1, 1, 1},	{2, 1, 1},	{3, 1, 1},
        {4, 1, 1},	{1, 4, 1},	{4, 1, 4},	{1, 4, 4},
        {0, 2, 1},	{1, 2, 1},	{2, 2, 1},	{3, 2, 1},
        {4, 2, 1},	{2, 4, 1},	{4, 2, 4},	{2, 4, 4},
        {0, 3, 1},	{1, 3, 1},	{2, 3, 1},	{3, 3, 1},
        {4, 3, 1},	{3, 4, 1},	{4, 3, 4},	{3, 4, 4},
        {0, 0, 2},	{1, 0, 2},	{2, 0, 2},	{3, 0, 2},
        {4, 0, 2},	{0, 4, 2},	{2, 0, 4},	{3, 0, 4},
        {0, 1, 2},	{1, 1, 2},	{2, 1, 2},	{3, 1, 2},
        {4, 1, 2},	{1, 4, 2},	{2, 1, 4},	{3, 1, 4},
        {0, 2, 2},	{1, 2, 2},	{2, 2, 2},	{3, 2, 2},
        {4, 2, 2},	{2, 4, 2},	{2, 2, 4},	{3, 2, 4},
        {0, 3, 2},	{1, 3, 2},	{2, 3, 2},	{3, 3, 2},
        {4, 3, 2},	{3, 4, 2},	{2, 3, 4},	{3, 3, 4},
        {0, 0, 3},	{1, 0, 3},	{2, 0, 3},	{3, 0, 3},
        {4, 0, 3},	{0, 4, 3},	{0, 0, 4},	{1, 0, 4},
        {0, 1, 3},	{1, 1, 3},	{2, 1, 3},	{3, 1, 3},
        {4, 1, 3},	{1, 4, 3},	{0, 1, 4},	{1, 1, 4},
        {0, 2, 3},	{1, 2, 3},	{2, 2, 3},	{3, 2, 3},
        {4, 2, 3},	{2, 4, 3},	{0, 2, 4},	{1, 2, 4},
        {0, 3, 3},	{1, 3, 3},	{2, 3, 3},	{3, 3, 3},
        {4, 3, 3},	{3, 4, 3},	{0, 3, 4},	{1, 3, 4}
    };
};

struct ColorQuantizationModeInfo
{
    const uint32 ColorUnquantizationTables[1206] =
    {

        0, 255 //2
        ,
        0, 128, 255 //3

        ,
        0, 85, 170, 255 //4

        ,
        0, 64, 128, 192, 255 //5

        ,
        0, 255, 51, 204, 102, 153 //6

        ,
        0, 36, 73, 109, 146, 182, 219, 255 //8

        ,
        0, 255, 28, 227, 56, 199, 84, 171, 113, 142 //10

        ,
        0, 255, 69, 186, 23, 232, 92, 163, 46, 209, 116, 139 //12

        ,
        0, 17, 34, 51, 68, 85, 102, 119, 136, 153, 170, 187, 204, 221, 238, 255  //16

        ,
        0, 255, 67, 188, 13, 242, 80, 175, 27, 228, 94, 161, 40, 215, 107, 148,  //20
        54, 201, 121, 134

        ,
        0, 255, 33, 222, 66, 189, 99, 156, 11, 244, 44, 211, 77, 178, 110, 145, //24
        22, 233, 55, 200, 88, 167, 121, 134

        ,
        0, 8, 16, 24, 33, 41, 49, 57, 66, 74, 82, 90, 99, 107, 115, 123,
        132, 140, 148, 156, 165, 173, 181, 189, 198, 206, 214, 222, 231, 239, 247, 255 //32

        ,
        0, 255, 32, 223, 65, 190, 97, 158, 6, 249, 39, 216, 71, 184, 104, 151,
        13, 242, 45, 210, 78, 177, 110, 145, 19, 236, 52, 203, 84, 171, 117, 138, //40
        26, 229, 58, 197, 91, 164, 123, 132

        ,
        0, 255, 16, 239, 32, 223, 48, 207, 65, 190, 81, 174, 97, 158, 113, 142,
        5, 250, 21, 234, 38, 217, 54, 201, 70, 185, 86, 169, 103, 152, 119, 136, //48
        11, 244, 27, 228, 43, 212, 59, 196, 76, 179, 92, 163, 108, 147, 124, 131

        ,
        0, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 44, 48, 52, 56, 60,
        65, 69, 73, 77, 81, 85, 89, 93, 97, 101, 105, 109, 113, 117, 121, 125,   //64
        130, 134, 138, 142, 146, 150, 154, 158, 162, 166, 170, 174, 178, 182, 186, 190,
        195, 199, 203, 207, 211, 215, 219, 223, 227, 231, 235, 239, 243, 247, 251, 255

        ,
        0, 255, 16, 239, 32, 223, 48, 207, 64, 191, 80, 175, 96, 159, 112, 143, //80
        3, 252, 19, 236, 35, 220, 51, 204, 67, 188, 83, 172, 100, 155, 116, 139,
        6, 249, 22, 233, 38, 217, 54, 201, 71, 184, 87, 168, 103, 152, 119, 136,
        9, 246, 25, 230, 42, 213, 58, 197, 74, 181, 90, 165, 106, 149, 122, 133,
        13, 242, 29, 226, 45, 210, 61, 194, 77, 178, 93, 162, 109, 146, 125, 130

        ,
        0, 255, 8, 247, 16, 239, 24, 231, 32, 223, 40, 215, 48, 207, 56, 199,  //96
        64, 191, 72, 183, 80, 175, 88, 167, 96, 159, 104, 151, 112, 143, 120, 135,
        2, 253, 10, 245, 18, 237, 26, 229, 35, 220, 43, 212, 51, 204, 59, 196,
        67, 188, 75, 180, 83, 172, 91, 164, 99, 156, 107, 148, 115, 140, 123, 132,
        5, 250, 13, 242, 21, 234, 29, 226, 37, 218, 45, 210, 53, 202, 61, 194,
        70, 185, 78, 177, 86, 169, 94, 161, 102, 153, 110, 145, 118, 137, 126, 129

        ,
        0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30,  //128
        32, 34, 36, 38, 40, 42, 44, 46, 48, 50, 52, 54, 56, 58, 60, 62,
        64, 66, 68, 70, 72, 74, 76, 78, 80, 82, 84, 86, 88, 90, 92, 94,
        96, 98, 100, 102, 104, 106, 108, 110, 112, 114, 116, 118, 120, 122, 124, 126,
        129, 131, 133, 135, 137, 139, 141, 143, 145, 147, 149, 151, 153, 155, 157, 159,
        161, 163, 165, 167, 169, 171, 173, 175, 177, 179, 181, 183, 185, 187, 189, 191,
        193, 195, 197, 199, 201, 203, 205, 207, 209, 211, 213, 215, 217, 219, 221, 223,
        225, 227, 229, 231, 233, 235, 237, 239, 241, 243, 245, 247, 249, 251, 253, 255

        ,
        0, 255, 8, 247, 16, 239, 24, 231, 32, 223, 40, 215, 48, 207, 56, 199, //160
        64, 191, 72, 183, 80, 175, 88, 167, 96, 159, 104, 151, 112, 143, 120, 135,
        1, 254, 9, 246, 17, 238, 25, 230, 33, 222, 41, 214, 49, 206, 57, 198,
        65, 190, 73, 182, 81, 174, 89, 166, 97, 158, 105, 150, 113, 142, 121, 134,
        3, 252, 11, 244, 19, 236, 27, 228, 35, 220, 43, 212, 51, 204, 59, 196,
        67, 188, 75, 180, 83, 172, 91, 164, 99, 156, 107, 148, 115, 140, 123, 132,
        4, 251, 12, 243, 20, 235, 28, 227, 36, 219, 44, 211, 52, 203, 60, 195,
        68, 187, 76, 179, 84, 171, 92, 163, 100, 155, 108, 147, 116, 139, 124, 131,
        6, 249, 14, 241, 22, 233, 30, 225, 38, 217, 46, 209, 54, 201, 62, 193,
        70, 185, 78, 177, 86, 169, 94, 161, 102, 153, 110, 145, 118, 137, 126, 129

        ,
        0, 255, 4, 251, 8, 247, 12, 243, 16, 239, 20, 235, 24, 231, 28, 227, //192
        32, 223, 36, 219, 40, 215, 44, 211, 48, 207, 52, 203, 56, 199, 60, 195,
        64, 191, 68, 187, 72, 183, 76, 179, 80, 175, 84, 171, 88, 167, 92, 163,
        96, 159, 100, 155, 104, 151, 108, 147, 112, 143, 116, 139, 120, 135, 124, 131,
        1, 254, 5, 250, 9, 246, 13, 242, 17, 238, 21, 234, 25, 230, 29, 226,
        33, 222, 37, 218, 41, 214, 45, 210, 49, 206, 53, 202, 57, 198, 61, 194,
        65, 190, 69, 186, 73, 182, 77, 178, 81, 174, 85, 170, 89, 166, 93, 162,
        97, 158, 101, 154, 105, 150, 109, 146, 113, 142, 117, 138, 121, 134, 125, 130,
        2, 253, 6, 249, 10, 245, 14, 241, 18, 237, 22, 233, 26, 229, 30, 225,
        34, 221, 38, 217, 42, 213, 46, 209, 50, 205, 54, 201, 58, 197, 62, 193,
        66, 189, 70, 185, 74, 181, 78, 177, 82, 173, 86, 169, 90, 165, 94, 161,
        98, 157, 102, 153, 106, 149, 110, 145, 114, 141, 118, 137, 122, 133, 126, 129

        ,                                                         //256
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
        16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
        32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
        48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
        64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
        80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95,
        96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111,
        112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127,
        128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143,
        144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159,
        160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175,
        176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191,
        192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207,
        208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223,
        224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239,
        240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255,

    };
    const int QuantizationModeTable[2176] =
    {
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, 0, 0, 2, 3, 5, 6, 8, 9, 11, 12, 14, 15, 17, 18, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
        -1, -1, -1, -1, 0, 0, 0, 1, 2, 2, 3, 4, 5, 5, 6, 7, 8, 8, 9, 10, 11, 11, 12, 13, 14, 14, 15, 16, 17, 17, 18, 19, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
        -1, -1, -1, -1, -1, -1, 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13, 14, 14, 15, 15, 16, 16, 17, 17, 18, 18, 19, 19, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
        -1, -1, -1, -1, -1, -1, -1, -1, 0, 0, 0, 0, 0, 1, 1, 1, 2, 2, 2, 3, 3, 4, 4, 4, 5, 5, 5, 6, 6, 7, 7, 7, 8, 8, 8, 9, 9, 10, 10, 10, 11, 11, 11, 12, 12, 13, 13, 13, 14, 14, 14, 15, 15, 16, 16, 16, 17, 17, 17, 18, 18, 19, 19, 19, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 7, 7, 7, 7, 8, 8, 8, 8, 9, 9, 10, 10, 10, 10, 11, 11, 11, 11, 12, 12, 13, 13, 13, 13, 14, 14, 14, 14, 15, 15, 16, 16, 16, 16, 17, 17, 17, 17, 18, 18, 19, 19, 19, 19, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 6, 6, 7, 7, 7, 7, 8, 8, 8, 8, 9, 9, 9, 9, 10, 10, 10, 10, 11, 11, 11, 11, 12, 12, 12, 12, 13, 13, 13, 13, 14, 14, 14, 14, 15, 15, 15, 15, 16, 16, 16, 16, 17, 17, 17, 17, 18, 18, 18, 18, 19, 19, 19, 19, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 6, 6, 6, 6, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 9, 9, 9, 9, 10, 10, 10, 10, 10, 11, 11, 11, 11, 11, 12, 12, 12, 12, 13, 13, 13, 13, 13, 14, 14, 14, 14, 14, 15, 15, 15, 15, 16, 16, 16, 16, 16, 17, 17, 17, 17, 17, 18, 18, 18, 18, 19, 19, 19, 19, 19, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 9, 9, 9, 9, 10, 10, 10, 10, 10, 10, 11, 11, 11, 11, 11, 11, 12, 12, 12, 12, 13, 13, 13, 13, 13, 13, 14, 14, 14, 14, 14, 14, 15, 15, 15, 15, 16, 16, 16, 16, 16, 16, 17, 17, 17, 17, 17, 17, 18, 18, 18, 18, 19, 19, 19, 19, 19, 19,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 9, 9, 9, 9, 9, 10, 10, 10, 10, 10, 10, 10, 11, 11, 11, 11, 11, 11, 12, 12, 12, 12, 12, 13, 13, 13, 13, 13, 13, 13, 14, 14, 14, 14, 14, 14, 15, 15, 15, 15, 15, 16, 16, 16, 16, 16, 16, 16, 17, 17,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 9, 9, 9, 9, 9, 10, 10, 10, 10, 10, 10, 10, 10, 11, 11, 11, 11, 11, 11, 11, 12, 12, 12, 12, 12, 13, 13, 13, 13, 13, 13, 13, 13, 14, 14, 14, 14, 14, 14, 14, 15,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8, 9, 9, 9, 9, 9, 9, 10, 10, 10, 10, 10, 10, 10, 10, 11, 11, 11, 11, 11, 11, 11, 11, 12, 12, 12, 12, 12, 12, 13, 13, 13, 13,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8, 9, 9, 9, 9, 9, 9, 9, 10, 10, 10, 10, 10, 10, 10, 10, 10, 11, 11, 11, 11, 11, 11, 11, 11,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8, 8, 9, 9, 9, 9, 9, 9, 9, 10, 10, 10, 10, 10, 10, 10, 10,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 9, 9, 9, 9, 9, 9,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    };
};

const int QuantAndXferTables[142] =
{
        0, 64,
        0, 32, 64,
        0, 21, 43, 64,
        0, 16, 32, 48, 64,
        0, 64, 12, 52, 25, 39,
        0, 9, 18, 27, 37, 46, 55, 64,
        0, 64, 7, 57, 14, 50, 21, 43, 28, 36,
        0, 64, 17, 47, 5, 59, 23, 41, 11, 53, 28, 36,
        0, 4, 8, 12, 17, 21, 25, 29, 35, 39, 43, 47, 52, 56, 60, 64,
        0, 64, 16, 48, 3, 61, 19, 45, 6, 58, 23, 41, 9, 55, 26, 38, 13, 51, 29, 35,
        0, 64, 8, 56, 16, 48, 24, 40, 2, 62, 11, 53, 19, 45, 27, 37, 5, 59, 13, 51, 22, 42, 30, 34,
        0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30, 34, 36, 38, 40, 42, 44, 46, 48, 50, 52, 54, 56, 58, 60, 62, 64,
};

static const ColorQuantizationModeInfo g_ColorQuantizationInfo;
static const TritsQuintsTable g_TritsQuintsTbl;

// =====================================================================================================================
static void GetSpvCode(
    InternalTexConvertCsType type,
    const void**             pCode,
    uint32*                  pSize)
{
    if (type == InternalTexConvertCsType::ConvertASTCToRGBA8)
    {
        *pCode = AstcDecoder;
        *pSize = sizeof(AstcDecoder);
    }
    else if (type == InternalTexConvertCsType::ConvertETC2ToRGBA8)
    {
        *pCode = Etc2Decoder;
        *pSize = sizeof(Etc2Decoder);
    }
    else if (type == InternalTexConvertCsType::ConvertETC2ToBc3)
    {
        *pCode = Etc2ToBc3;
        *pSize = sizeof(Etc2ToBc3);
    }
    else
    {
        PAL_ASSERT(type == InternalTexConvertCsType::ConvertRGBA8ToBc3);
        *pCode = Bc3Encoder;
        *pSize = sizeof(Bc3Encoder);
    }
}

// =====================================================================================================================
Device::Device()
    :
    m_info({}),
    m_pTableMemory(nullptr),
    m_pPalCmdBuffer (nullptr),
    m_pipelineMap(64, &m_allocator)
{

}

// =====================================================================================================================
Device::~Device()
{
    if (m_pTableMemory != nullptr)
    {
        m_info.pPalDevice->RemoveGpuMemoryReferences(1, &m_pTableMemory, nullptr);
        m_pTableMemory->Destroy();
        PAL_SAFE_FREE(m_pTableMemory, m_info.pPlatform);
    }

    for (InternalPipelineMap::Iterator itr = m_pipelineMap.Begin(); itr.Get(); itr.Next())
    {
        ClientDestroyInternalComputePipeline(m_info, itr.Get()->value.pPipeline, itr.Get()->value.pMemory);
    }

    if (m_etc2ToRgba8PipeLine.pPipeline != nullptr)
    {
        ClientDestroyInternalComputePipeline(m_info, m_etc2ToRgba8PipeLine.pPipeline, m_etc2ToRgba8PipeLine.pMemory);
    }

    if (m_rgba8ToBc3PipeLine.pPipeline != nullptr)
    {
        ClientDestroyInternalComputePipeline(m_info, m_rgba8ToBc3PipeLine.pPipeline, m_rgba8ToBc3PipeLine.pMemory);
    }

    if (m_etc2ToBc3Pipeline.pPipeline != nullptr)
    {
        ClientDestroyInternalComputePipeline(m_info, m_etc2ToBc3Pipeline.pPipeline, m_etc2ToBc3Pipeline.pMemory);
    }

}

// =====================================================================================================================
void Device::Init(
    const DeviceInitInfo& info)
{
    m_info = info;
    m_imageViewSizeInDwords = m_info.pDeviceProperties->gfxipProperties.srdSizes.imageView / sizeof(uint32);
    m_bufferViewSizeInDwords = m_info.pDeviceProperties->gfxipProperties.srdSizes.bufferView / sizeof(uint32);

    // 3 Table and 1 TexBuffer, and 2 Image resource
    m_srdDwords[static_cast<uint32>(InternalTexConvertCsType::ConvertASTCToRGBA8)]
        = (3 + 1) * m_bufferViewSizeInDwords + 2 * m_imageViewSizeInDwords;

    // 1 Image resource for output 1 image resource for image input and 1 TexBuffer for buffer Input
    m_srdDwords[static_cast<uint32>(InternalTexConvertCsType::ConvertETC2ToRGBA8)]
        =  2 * m_imageViewSizeInDwords + m_bufferViewSizeInDwords;

    // 2 Image resources for output and Input
    m_srdDwords[static_cast<uint32>(InternalTexConvertCsType::ConvertRGBA8ToBc3)]
        =  2 * m_imageViewSizeInDwords;

    // 1 Image resource for output 1 image resource for image input and 1 TexBuffer for buffer Input
    m_srdDwords[static_cast<uint32>(InternalTexConvertCsType::ConvertETC2ToBc3)]
        =  2 * m_imageViewSizeInDwords + m_bufferViewSizeInDwords;

    Pal::Result result              = m_pipelineMap.Init();
    m_etc2ToRgba8PipeLine.pMemory   = nullptr;
    m_etc2ToRgba8PipeLine.pPipeline = nullptr;
    m_rgba8ToBc3PipeLine.pMemory    = nullptr;
    m_rgba8ToBc3PipeLine.pPipeline  = nullptr;
    m_etc2ToBc3Pipeline.pMemory     = nullptr;
    m_etc2ToBc3Pipeline.pPipeline   = nullptr;

    PAL_ASSERT(result == Pal::Result::Success);
}

// =====================================================================================================================
static void InsertBarrier(Pal::ICmdBuffer*& cmdBuffer)
{
    PAL_ASSERT(cmdBuffer != nullptr);

    Pal::BarrierInfo barrierInfo = {};
    barrierInfo.waitPoint = Pal::HwPipePreCs;

    const Pal::HwPipePoint pipePoint = Pal::HwPipePostCs;
    barrierInfo.pipePointWaitCount = 1;
    barrierInfo.pPipePoints = &pipePoint;

    Pal::BarrierTransition transition = {};
    transition.srcCacheMask = Pal::CoherShader;
    transition.dstCacheMask = Pal::CoherShader;

    barrierInfo.transitionCount = 1;
    barrierInfo.pTransitions = &transition;
    barrierInfo.reason = 1;
    cmdBuffer->CmdBarrier(barrierInfo);
}

// =====================================================================================================================
Pal::Result Device::GpuDecodeImage(
    InternalTexConvertCsType    type,
    Pal::ICmdBuffer*            pCmdBuffer,
    const Pal::IImage*          pSrcImage,
    const Pal::IImage*          pDstImage,
    uint32                      regionCount,
    Pal::ImageCopyRegion*       pPalImageRegions,
    const CompileTimeConstants& constInfo)
{
    m_pPalCmdBuffer = pCmdBuffer;
    m_pPalCmdBuffer->CmdSaveComputeState(Pal::ComputeStateAll);
    uint32* pUserData = nullptr;
    BindPipeline(type, constInfo);

    if (type == InternalTexConvertCsType::ConvertASTCToRGBA8)
    {
        uint32 pushConstant[PushConstASTCToRGBA];
        for (uint32 idx = 0; idx < regionCount; ++idx)
        {
            pUserData = nullptr;
            CreateUserData(
                type,
                &pUserData,
                m_srdDwords[static_cast<uint32>(type)]);

            // Skip the texture buffer view
            pUserData += 4;

            Pal::ImageCopyRegion copyRegion = pPalImageRegions[idx];
            Pal::SubresId palSrcSubResId    = copyRegion.srcSubres;
            Pal::SubresId palDstSubResId    = copyRegion.dstSubres;
            Pal::SwizzledFormat dstFormat   = pDstImage->GetImageCreateInfo().swizzledFormat;
            Pal::SwizzledFormat srcFormat   = pSrcImage->GetImageCreateInfo().swizzledFormat;

            Pal::ImageViewInfo imageView[2] = {};

            BuildImageViewInfo(&imageView[0], pDstImage, palDstSubResId, copyRegion.numSlices, dstFormat, true);
            BuildImageViewInfo(&imageView[1], pSrcImage, palSrcSubResId, copyRegion.numSlices, srcFormat, false);

            m_info.pPalDevice->CreateImageViewSrds(2, imageView, pUserData);

            pUserData += 2 * m_imageViewSizeInDwords;

            uint32_t copyData[12] = {
                static_cast<uint32_t>(pPalImageRegions[idx].srcOffset.x),
                static_cast<uint32_t>(pPalImageRegions[idx].srcOffset.y),
                static_cast<uint32_t>(pPalImageRegions[idx].srcOffset.z),
                0,
                pPalImageRegions[idx].dstOffset.x * constInfo.pConstants[0],
                pPalImageRegions[idx].dstOffset.y * constInfo.pConstants[1],
                static_cast<uint32_t>(pPalImageRegions[idx].dstOffset.z),
                0,
                pPalImageRegions[idx].extent.width,
                pPalImageRegions[idx].extent.height,
                pPalImageRegions[idx].extent.depth,
                0
            };

            // skip buffer copyData
            memcpy(&pushConstant[12], copyData, sizeof(uint32_t)*12);

            // isSrgb
            pushConstant[24] = constInfo.pConstants[2];
            // isCopyBuffer
            pushConstant[25] = 0;

            m_pPalCmdBuffer->CmdSetUserData(Pal::PipelineBindPoint::Compute, 1, PushConstASTCToRGBA , &pushConstant[0]);

            // extent in block
            const uint32 threadGroupsX = pPalImageRegions[idx].extent.width;
            const uint32 threadGroupsY = pPalImageRegions[idx].extent.height;
            const uint32 threadGroupsZ = Util::Max(pPalImageRegions[idx].extent.depth,
                pPalImageRegions[idx].numSlices);

            m_pPalCmdBuffer->CmdDispatch({ threadGroupsX, threadGroupsY, threadGroupsZ });
        }
    }
    else if ((type == InternalTexConvertCsType::ConvertETC2ToRGBA8) ||
        (type == InternalTexConvertCsType::ConvertETC2ToBc3))
    {
        uint32 pushConstant[PushConstETC2ToRGBA];

        for (uint32 idx = 0; idx < regionCount; ++idx)
        {
            pUserData = nullptr;
            CreateUserData(
                type,
                &pUserData,
                m_srdDwords[static_cast<uint32>(type)]);

            Pal::ImageCopyRegion copyRegion = pPalImageRegions[idx];
            Pal::SubresId palSrcSubResId    = copyRegion.srcSubres;
            Pal::SubresId palDstSubResId    = copyRegion.dstSubres;
            Pal::SwizzledFormat dstFormat   = pDstImage->GetImageCreateInfo().swizzledFormat;
            Pal::SwizzledFormat srcFormat   = pSrcImage->GetImageCreateInfo().swizzledFormat;

            if (type == InternalTexConvertCsType::ConvertETC2ToBc3)
            {
                dstFormat.format    = Pal::ChNumFormat::X32Y32Z32W32_Uint;
                dstFormat.swizzle.r = Pal::ChannelSwizzle::X;
                dstFormat.swizzle.g = Pal::ChannelSwizzle::Y;
                dstFormat.swizzle.b = Pal::ChannelSwizzle::Z;
                dstFormat.swizzle.a = Pal::ChannelSwizzle::W;
            }

            Pal::ImageViewInfo imageView[2] = {};

            BuildImageViewInfo(&imageView[0], pDstImage, palDstSubResId, copyRegion.numSlices, dstFormat, true);
            BuildImageViewInfo(&imageView[1], pSrcImage, palSrcSubResId, copyRegion.numSlices, srcFormat, false);

            m_info.pPalDevice->CreateImageViewSrds(2, imageView, pUserData);

            pUserData += (2 * m_imageViewSizeInDwords) + m_bufferViewSizeInDwords;

            uint32_t copyData[12] = {
                static_cast<uint32_t>(pPalImageRegions[idx].srcOffset.x),
                static_cast<uint32_t>(pPalImageRegions[idx].srcOffset.y),
                static_cast<uint32_t>(pPalImageRegions[idx].srcOffset.z),
                0,
                static_cast<uint32_t>(pPalImageRegions[idx].dstOffset.x * 4),
                static_cast<uint32_t>(pPalImageRegions[idx].dstOffset.y * 4),
                static_cast<uint32_t>(pPalImageRegions[idx].dstOffset.z),
                0,
                pPalImageRegions[idx].extent.width,
                pPalImageRegions[idx].extent.height,
                pPalImageRegions[idx].extent.depth,
                0
            };

            // skip buffer copyData
            memcpy(&pushConstant[12], copyData, sizeof(uint32_t)*12);

            // alphaBits
            pushConstant[24] = constInfo.pConstants[0];
            // components
            pushConstant[25] = constInfo.pConstants[1];
            // signedFlags
            pushConstant[26] = constInfo.pConstants[2];
            // isBufferSrc
            pushConstant[27] = 0;

            m_pPalCmdBuffer->CmdSetUserData(Pal::PipelineBindPoint::Compute, 1, PushConstETC2ToRGBA , &pushConstant[0]);

            const uint32 threadGroupsX = (pPalImageRegions[idx].extent.width + 1) / 2;
            const uint32 threadGroupsY = (pPalImageRegions[idx].extent.height + 1) / 2;
            const uint32 threadGroupsZ = Util::Max(pPalImageRegions[idx].extent.depth,
                pPalImageRegions[idx].numSlices);

            m_pPalCmdBuffer->CmdDispatch({ threadGroupsX, threadGroupsY, threadGroupsZ });
        }
    }
    else
    {
        PAL_ASSERT(type == InternalTexConvertCsType::ConvertRGBA8ToBc3);

        uint32 pushConstant[PushConstRGBAToBC3];

        for (uint32 idx = 0; idx < regionCount; ++idx)
        {
            pUserData = nullptr;
            CreateUserData(
                type,
                &pUserData,
                m_srdDwords[static_cast<uint32>(type)]);

            Pal::ImageCopyRegion copyRegion = pPalImageRegions[idx];
            Pal::SubresId palSrcSubResId    = copyRegion.srcSubres;
            Pal::SubresId palDstSubResId    = copyRegion.dstSubres;
            Pal::SwizzledFormat srcFormat   = pSrcImage->GetImageCreateInfo().swizzledFormat;

            Pal::SwizzledFormat dstFormat;
            dstFormat.format    = Pal::ChNumFormat::X32Y32Z32W32_Uint;
            dstFormat.swizzle.r = Pal::ChannelSwizzle::X;
            dstFormat.swizzle.g = Pal::ChannelSwizzle::Y;
            dstFormat.swizzle.b = Pal::ChannelSwizzle::Z;
            dstFormat.swizzle.a = Pal::ChannelSwizzle::W;

            Pal::ImageViewInfo imageView[2] = {};
            BuildImageViewInfo(&imageView[0], pSrcImage, palSrcSubResId, copyRegion.numSlices, srcFormat, false);
            BuildImageViewInfo(&imageView[1], pDstImage, palSrcSubResId, copyRegion.numSlices, dstFormat, true);

            m_info.pPalDevice->CreateImageViewSrds(2, imageView, pUserData);

            pUserData += (2 * m_imageViewSizeInDwords);

            uint32_t cbParams[4] = {
                pPalImageRegions[idx].extent.width,
                constInfo.pConstants[0],
                constInfo.pConstants[1],
                0
            };

            memcpy(&pushConstant[0], cbParams, sizeof(uint32_t)*4);

            m_pPalCmdBuffer->CmdSetUserData(Pal::PipelineBindPoint::Compute, 1, PushConstRGBAToBC3 , &pushConstant[0]);

            uint32 width               = pPalImageRegions[idx].extent.width * 4;
            uint32 height              = pPalImageRegions[idx].extent.height * 4;
            const uint32 threadGroupsX = (width * height + 63) / 64;

            m_pPalCmdBuffer->CmdDispatch({ threadGroupsX, 1, 1 });
        }
    }

    InsertBarrier(m_pPalCmdBuffer);

    m_pPalCmdBuffer->CmdRestoreComputeState(Pal::ComputeStateAll);

    return Pal::Result::Success;
}

// =====================================================================================================================
Pal::Result Device::GpuDecodeBuffer(
    InternalTexConvertCsType    type,
    Pal::ICmdBuffer*            pCmdBuffer,
    const Pal::IGpuMemory*      pSrcBufferMem,
    Pal::IImage*                pDstImage,
    uint32                      regionCount,
    Pal::MemoryImageCopyRegion* pPalBufferRegionsIn,
    const CompileTimeConstants& constInfo,
    Pal::SwizzledFormat         sourceViewFormat)
{
    m_pPalCmdBuffer = pCmdBuffer;
    m_pPalCmdBuffer->CmdSaveComputeState(Pal::ComputeStateAll);

    uint32* pUserData = nullptr;
    BindPipeline(type, constInfo);

    if (type == InternalTexConvertCsType::ConvertASTCToRGBA8)
    {
        uint32 pushConstant[PushConstASTCToRGBA];
        // fixed 128 bits for one ASTC block
        uint32_t viewBpp = 16;
        for (uint32 idx = 0; idx < regionCount; ++idx)
        {
            pUserData = nullptr;
            CreateUserData(
                type,
                &pUserData,
                m_srdDwords[static_cast<uint32>(type)]);

            Pal::MemoryImageCopyRegion copyRegion = pPalBufferRegionsIn[idx];
            Pal::SubresId palDstSubResId          = copyRegion.imageSubres;
            Pal::SwizzledFormat dstFormat         = pDstImage->GetImageCreateInfo().swizzledFormat;

            Pal::gpusize range = ((pPalBufferRegionsIn[idx].imageExtent.depth - 1) * pPalBufferRegionsIn[idx].gpuMemoryDepthPitch) +
                ((pPalBufferRegionsIn[idx].imageExtent.height - 1) * pPalBufferRegionsIn[idx].gpuMemoryRowPitch) +
                (pPalBufferRegionsIn[idx].imageExtent.width * viewBpp);

            BuildTypedBufferViewInfo(
                pUserData,
                1,
                (pSrcBufferMem->Desc().gpuVirtAddr + pPalBufferRegionsIn[idx].gpuMemoryOffset),
                range,
                viewBpp,
                sourceViewFormat);

            pUserData += m_bufferViewSizeInDwords;

            Pal::ImageViewInfo imageView = {};

            BuildImageViewInfo(&imageView, pDstImage, palDstSubResId, copyRegion.numSlices, dstFormat, true);

            m_info.pPalDevice->CreateImageViewSrds(1, &imageView, pUserData);

            pUserData += m_imageViewSizeInDwords;

            // skip source image
            pUserData += m_imageViewSizeInDwords;

            PAL_ASSERT((pPalBufferRegionsIn[idx].gpuMemoryRowPitch / viewBpp) >= 1);

            uint32_t copyData[12] = {
                uint32_t(pPalBufferRegionsIn[idx].imageOffset.x * constInfo.pConstants[0]),
                uint32_t(pPalBufferRegionsIn[idx].imageOffset.y * constInfo.pConstants[1]),
                uint32_t(pPalBufferRegionsIn[idx].imageOffset.z),
                0, // unused
                pPalBufferRegionsIn[idx].imageExtent.width,
                pPalBufferRegionsIn[idx].imageExtent.height,
                pPalBufferRegionsIn[idx].imageExtent.depth,
                0, // unused
                static_cast<uint32_t>(pPalBufferRegionsIn[idx].gpuMemoryRowPitch / viewBpp),
                static_cast<uint32_t>(pPalBufferRegionsIn[idx].gpuMemoryDepthPitch / viewBpp),
                0, // unused
                0  // unused
            };

            memcpy(&pushConstant[0], copyData, sizeof(uint32_t)*12);

            // isSrgb
            pushConstant[24] = constInfo.pConstants[2];

            // isBufferCpy
            pushConstant[25] = 1;

            m_pPalCmdBuffer->CmdSetUserData(Pal::PipelineBindPoint::Compute, 1, PushConstASTCToRGBA , &pushConstant[0]);

            const uint32 threadGroupsX = pPalBufferRegionsIn[idx].imageExtent.width;
            const uint32 threadGroupsY = pPalBufferRegionsIn[idx].imageExtent.height;
            const uint32 threadGroupsZ = Util::Max(pPalBufferRegionsIn[idx].imageExtent.depth,
                pPalBufferRegionsIn[idx].numSlices);

            m_pPalCmdBuffer->CmdDispatch({ threadGroupsX, threadGroupsY, threadGroupsZ });
        }
    }
    else
    {
        PAL_ASSERT((type == InternalTexConvertCsType::ConvertETC2ToRGBA8) || (type == InternalTexConvertCsType::ConvertETC2ToBc3));

        uint32 viewBpp = 0;
        switch (sourceViewFormat.format)
        {
        case Pal::ChNumFormat::X32Y32Z32W32_Uint:
            viewBpp = 16;
            break;
        case Pal::ChNumFormat::X32Y32_Uint:
            viewBpp = 8;
            break;
        default:
            PAL_ASSERT(0); // should not happen
            break;
        }

        uint32 pushConstant[PushConstETC2ToBC3];

        for (uint32 idx = 0; idx < regionCount; ++idx)
        {
            pUserData = nullptr;
            CreateUserData(
                type,
                &pUserData,
                m_srdDwords[static_cast<uint32>(type)]);

            Pal::MemoryImageCopyRegion copyRegion = pPalBufferRegionsIn[idx];
            Pal::SubresId palDstSubResId          = copyRegion.imageSubres;
            Pal::SwizzledFormat dstFormat         = pDstImage->GetImageCreateInfo().swizzledFormat;
            Pal::ImageViewInfo imageView          = {};

            if (type == InternalTexConvertCsType::ConvertETC2ToBc3)
            {
                dstFormat.format = Pal::ChNumFormat::X32Y32Z32W32_Uint;
                dstFormat.swizzle.r = Pal::ChannelSwizzle::X;
                dstFormat.swizzle.g = Pal::ChannelSwizzle::Y;
                dstFormat.swizzle.b = Pal::ChannelSwizzle::Z;
                dstFormat.swizzle.a = Pal::ChannelSwizzle::W;
            }

            BuildImageViewInfo(&imageView, pDstImage, palDstSubResId, copyRegion.numSlices, dstFormat, true);

            m_info.pPalDevice->CreateImageViewSrds(1, &imageView, pUserData);

            pUserData += (2 * m_imageViewSizeInDwords);

            Pal::gpusize range = ((pPalBufferRegionsIn[idx].imageExtent.depth - 1) * pPalBufferRegionsIn[idx].gpuMemoryDepthPitch) +
                ((pPalBufferRegionsIn[idx].imageExtent.height - 1) * pPalBufferRegionsIn[idx].gpuMemoryRowPitch) +
                (pPalBufferRegionsIn[idx].imageExtent.width * viewBpp);

            BuildTypedBufferViewInfo(
                pUserData,
                1,
                (pSrcBufferMem->Desc().gpuVirtAddr + pPalBufferRegionsIn[idx].gpuMemoryOffset),
                range,
                viewBpp,
                sourceViewFormat);

            pUserData += m_bufferViewSizeInDwords;

            PAL_ASSERT((pPalBufferRegionsIn[idx].gpuMemoryRowPitch / viewBpp) >= 1);

            uint32_t copyData[12] = {
                uint32_t(pPalBufferRegionsIn[idx].imageOffset.x),
                uint32_t(pPalBufferRegionsIn[idx].imageOffset.y),
                uint32_t(pPalBufferRegionsIn[idx].imageOffset.z),
                0, // unused
                pPalBufferRegionsIn[idx].imageExtent.width,
                pPalBufferRegionsIn[idx].imageExtent.height,
                pPalBufferRegionsIn[idx].imageExtent.depth,
                0, // unused
                static_cast<uint32_t>(pPalBufferRegionsIn[idx].gpuMemoryRowPitch / viewBpp),
                static_cast<uint32_t>(pPalBufferRegionsIn[idx].gpuMemoryDepthPitch / viewBpp),
                0, // unused
                0  // unused
            };

            memcpy(&pushConstant[0], copyData, sizeof(uint32_t)*12);

             // alphaBits
            pushConstant[24] = constInfo.pConstants[0];
            // components
            pushConstant[25] = constInfo.pConstants[1];
            // signedFlags
            pushConstant[26] = constInfo.pConstants[2];
            // isBufferSrc
            pushConstant[27] = 1;

            m_pPalCmdBuffer->CmdSetUserData(Pal::PipelineBindPoint::Compute, 1, PushConstETC2ToBC3 , &pushConstant[0]);

            const uint32 threadGroupsX = (pPalBufferRegionsIn[idx].imageExtent.width + 1) / 2;
            const uint32 threadGroupsY = (pPalBufferRegionsIn[idx].imageExtent.height + 1) / 2;
            const uint32 threadGroupsZ = Util::Max(pPalBufferRegionsIn[idx].imageExtent.depth,
                pPalBufferRegionsIn[idx].numSlices);

            m_pPalCmdBuffer->CmdDispatch({ threadGroupsX, threadGroupsY, threadGroupsZ });
        }
    }

    InsertBarrier(m_pPalCmdBuffer);

    m_pPalCmdBuffer->CmdRestoreComputeState(Pal::ComputeStateAll);

    return Pal::Result::Success;
}

// =====================================================================================================================
void Device::CreateUserData(
    InternalTexConvertCsType    type,
    uint32**                    ppUserData,
    uint32                      srdDwords)
{
    *ppUserData = CreateAndBindEmbeddedUserData(m_pPalCmdBuffer, srdDwords, 0, 1);
    memset(*ppUserData, 0, srdDwords * sizeof(uint32));

    if (type == InternalTexConvertCsType::ConvertASTCToRGBA8)
    {
        if (m_pTableMemory == nullptr)
        {
            CreateTableMemory();
        }
        SetupInternalTables(type, ppUserData);
    }
}

// =====================================================================================================================
void Device::BindPipeline(
    InternalTexConvertCsType    type,
    const CompileTimeConstants& constInfo)
{
    const Pal::IPipeline* pPipeline = GetInternalPipeline(type, constInfo);

    Pal::PipelineBindParams bindParam = {};
    bindParam.pipelineBindPoint       = Pal::PipelineBindPoint::Compute;
    bindParam.pPipeline               = pPipeline;
    bindParam.apiPsoHash              = Pal::InternalApiPsoHash;

    m_pPalCmdBuffer->CmdBindPipeline(bindParam);
}

// =====================================================================================================================
Pal::Result Device::CreateGpuMemory(
    Pal::GpuMemoryRequirements* pMemReqs,
    Pal::IGpuMemory**           ppGpuMemory,
    Pal::gpusize*               pOffset)
{
    Pal::GpuMemoryCreateInfo createInfo = {};
    createInfo.size      = pMemReqs->size;
    createInfo.alignment = pMemReqs->alignment;
    createInfo.vaRange   = Pal::VaRange::Default;
    createInfo.priority  = Pal::GpuMemPriority::VeryLow;
    createInfo.heapCount = pMemReqs->heapCount;

    for (uint32 i = 0; i < createInfo.heapCount; i++)
    {
        createInfo.heaps[i] = pMemReqs->heaps[i];
    }

    Pal::Result  result = Pal::Result::Success;
    const size_t objectSize = m_info.pPalDevice->GetGpuMemorySize(createInfo, &result);

    if (result == Pal::Result::Success)
    {
        void* pMemory = PAL_MALLOC(objectSize, m_info.pPlatform, Util::SystemAllocType::AllocInternal);
        if (pMemory != nullptr)
        {
            result = m_info.pPalDevice->CreateGpuMemory(createInfo, pMemory, reinterpret_cast<Pal::IGpuMemory**>(ppGpuMemory));
            if (result != Pal::Result::Success)
            {
                PAL_SAFE_FREE(pMemory, m_info.pPlatform);
            }
        }
        else
        {
            result = Pal::Result::ErrorOutOfMemory;
        }
    }
    return result;
}

// =====================================================================================================================
void Device::CreateMemoryReqs(
    uint32                      bytesSize,
    uint32                      alignment,
    Pal::GpuMemoryRequirements* pMemReqs)
{
    pMemReqs->size      = bytesSize;
    pMemReqs->alignment = alignment;
    pMemReqs->heapCount = 2;
    pMemReqs->heaps[0]  = Pal::GpuHeapLocal;
    pMemReqs->heaps[1]  = Pal::GpuHeapGartUswc;
}

// =====================================================================================================================
uint32* Device::CreateAndBindEmbeddedUserData(
    Pal::ICmdBuffer*  pCmdBuffer,
    uint32            sizeInDwords,
    uint32            entryToBind,
    uint32            bindNum) const
{
    Pal::gpusize gpuVirtAddr = 0;
    uint32* const pCmdSpace  = pCmdBuffer->CmdAllocateEmbeddedData(sizeInDwords, 8, &gpuVirtAddr);
    PAL_ASSERT(pCmdSpace != nullptr);

    const uint32 gpuVirtAddrLo = Util::LowPart(gpuVirtAddr);
    pCmdBuffer->CmdSetUserData(Pal::PipelineBindPoint::Compute, entryToBind, bindNum, &gpuVirtAddrLo);
    return pCmdSpace;
}

// =====================================================================================================================
Pal::IPipeline* Device::GetInternalPipeline(
    InternalTexConvertCsType    type,
    const CompileTimeConstants& constInfo)
{
    void* pMemory                             = nullptr;
    PipelineBuildInfo buildInfo               = {};
    InternalPipelineMemoryPair* pPipelinePair = nullptr;
    Pal::Result result                        = Pal::Result::Success;
    GpuDecodeMappingNode resourceNodes[AstcInternalPipelineNodes];

    if ((type == InternalTexConvertCsType::ConvertETC2ToRGBA8) || (type == InternalTexConvertCsType::ConvertETC2ToBc3))
    {
        InternalPipelineMemoryPair* pPipeline = (type == InternalTexConvertCsType::ConvertETC2ToRGBA8) ?
            &m_etc2ToRgba8PipeLine : &m_etc2ToBc3Pipeline;

        if (pPipeline->pPipeline == nullptr)
        {
            // 1. output
            resourceNodes[0].nodeType       = NodeType::Image;
            resourceNodes[0].sizeInDwords   = m_imageViewSizeInDwords;
            resourceNodes[0].offsetInDwords = 0;
            resourceNodes[0].binding        = 0;
            resourceNodes[0].set            = 0;

            // 2. input image
            resourceNodes[1].nodeType       = NodeType::Image;
            resourceNodes[1].sizeInDwords   = m_imageViewSizeInDwords;
            resourceNodes[1].offsetInDwords = 1 * m_imageViewSizeInDwords;
            resourceNodes[1].binding        = 1;
            resourceNodes[1].set            = 0;

            // 3. input buffer
            resourceNodes[2].nodeType       = NodeType::TexBuffer;
            resourceNodes[2].sizeInDwords   = m_bufferViewSizeInDwords;
            resourceNodes[2].offsetInDwords = 2 * m_imageViewSizeInDwords;
            resourceNodes[2].binding        = 2;
            resourceNodes[2].set            = 0;

            // 4. push constants
            resourceNodes[3].nodeType       = NodeType::PushConstant;
            resourceNodes[3].sizeInDwords   = 28; // bufferData, imageData, alphabits, eacComps, signedflags, isBufferSrc
            resourceNodes[3].offsetInDwords = 2 * m_imageViewSizeInDwords + m_bufferViewSizeInDwords;
            resourceNodes[3].binding        = 0;
            resourceNodes[3].set            = static_cast<unsigned>(-1); // Vkgc::InternalDescriptorSetId

            buildInfo.nodeCount      = GpuTexDecoder::Etc2InternalPipelineNodes;
            buildInfo.shaderType     = type;
            buildInfo.pUserDataNodes = resourceNodes;

            GetSpvCode(buildInfo.shaderType, &(buildInfo.code.pSpvCode), &(buildInfo.code.spvSize));

            result = ClientCreateInternalComputePipeline(
                m_info,
                constInfo,
                buildInfo,
                &pPipeline->pPipeline,
                &pPipeline->pMemory);

            PAL_ASSERT(result == Pal::Result::Success);
        }

        pPipelinePair = pPipeline;
    }
    else if (type == InternalTexConvertCsType::ConvertRGBA8ToBc3)
    {
        if (m_rgba8ToBc3PipeLine.pPipeline == nullptr)
        {
            // 1. input
            resourceNodes[0].nodeType       = NodeType::Image;
            resourceNodes[0].sizeInDwords   = m_imageViewSizeInDwords;
            resourceNodes[0].offsetInDwords = 0;
            resourceNodes[0].binding        = 0;
            resourceNodes[0].set            = 0;

            // 2. output
            resourceNodes[1].nodeType       = NodeType::Image;
            resourceNodes[1].sizeInDwords   = m_imageViewSizeInDwords;
            resourceNodes[1].offsetInDwords = 1 * m_imageViewSizeInDwords;
            resourceNodes[1].binding        = 1;
            resourceNodes[1].set            = 0;

            resourceNodes[2].nodeType       = NodeType::PushConstant;
            resourceNodes[2].sizeInDwords   = 4; // width, start_block, quality, aligned to 4 DWORDs
            resourceNodes[2].offsetInDwords = 2 * m_imageViewSizeInDwords;
            resourceNodes[2].binding        = 0;
            resourceNodes[2].set            = static_cast<unsigned>(-1); // Vkgc::InternalDescriptorSetId

            buildInfo.nodeCount      = GpuTexDecoder::Bc3InternalPipelineNodes;
            buildInfo.shaderType     = type;
            buildInfo.pUserDataNodes = resourceNodes;

            GetSpvCode(buildInfo.shaderType, &(buildInfo.code.pSpvCode), &(buildInfo.code.spvSize));

            result = ClientCreateInternalComputePipeline(
                m_info,
                constInfo,
                buildInfo,
                &m_rgba8ToBc3PipeLine.pPipeline,
                &m_rgba8ToBc3PipeLine.pMemory);

            PAL_ASSERT(result == Pal::Result::Success);
        }
        pPipelinePair = &m_rgba8ToBc3PipeLine;
    }
    else
    {
        Util::RWLock* pPipelineLock = const_cast<Util::RWLock *>(&m_internalPipelineLock);

        Util::MetroHash::Hash hash = {};
        Util::MetroHash64::Hash((uint8 *)(constInfo.pConstants), sizeof(uint32) * constInfo.numConstants, &hash.bytes[0]);

        InternalPipelineKey key = {};
        key.shaderType          = type;
        key.constInfoHash       = Util::MetroHash::Compact32(&hash);

        {
            Util::RWLockAuto<Util::RWLock::LockType::ReadOnly> lock(pPipelineLock);
            pPipelinePair = m_pipelineMap.FindKey(key);
        }

        if (pPipelinePair == nullptr)
        {
            Util::RWLockAuto<Util::RWLock::LockType::ReadWrite> lock(pPipelineLock);

            bool existed = false;
            result       = m_pipelineMap.FindAllocate(key, &existed, &pPipelinePair);

            if ((existed == false) && (result == Pal::Result::Success) && (pPipelinePair != nullptr))
            {
                if (type == InternalTexConvertCsType::ConvertASTCToRGBA8)
                {
                    // 1.Color UnQuantization Buffer View
                    resourceNodes[0].nodeType       = NodeType::Buffer;
                    resourceNodes[0].sizeInDwords   = m_bufferViewSizeInDwords;
                    resourceNodes[0].offsetInDwords = 0;
                    resourceNodes[0].binding        = 0;
                    resourceNodes[0].set            = 0;

                    // 2.Trits Quints Buffer View
                    resourceNodes[1].nodeType       = NodeType::Buffer;
                    resourceNodes[1].sizeInDwords   = m_bufferViewSizeInDwords;
                    resourceNodes[1].offsetInDwords = 1 * m_bufferViewSizeInDwords;
                    resourceNodes[1].binding        = 1;
                    resourceNodes[1].set            = 0;

                    // 3.Quant and Transfer Buffer View
                    resourceNodes[2].nodeType       = NodeType::Buffer;
                    resourceNodes[2].sizeInDwords   = m_bufferViewSizeInDwords;
                    resourceNodes[2].offsetInDwords = 2 * m_bufferViewSizeInDwords;
                    resourceNodes[2].binding        = 2;
                    resourceNodes[2].set            = 0;

                    // 4. TexBuffer View for Src Image Buffer
                    resourceNodes[3].nodeType       = NodeType::TexBuffer;
                    resourceNodes[3].sizeInDwords   = m_bufferViewSizeInDwords;
                    resourceNodes[3].offsetInDwords = 3 * m_bufferViewSizeInDwords;
                    resourceNodes[3].binding        = 3;
                    resourceNodes[3].set            = 0;

                    // 5. Image View for Src Image
                    resourceNodes[4].nodeType       = NodeType::Image;
                    resourceNodes[4].sizeInDwords   = m_imageViewSizeInDwords;
                    resourceNodes[4].offsetInDwords = 4 * m_bufferViewSizeInDwords;
                    resourceNodes[4].binding        = 4;
                    resourceNodes[4].set            = 0;

                    // 6. Image View for Dst Image
                    resourceNodes[5].nodeType       = NodeType::Image;
                    resourceNodes[5].sizeInDwords   = m_imageViewSizeInDwords;
                    resourceNodes[5].offsetInDwords = 4 * m_bufferViewSizeInDwords + m_imageViewSizeInDwords;
                    resourceNodes[5].binding        = 5;
                    resourceNodes[5].set            = 0;

                    resourceNodes[6].nodeType       = NodeType::PushConstant;
                    resourceNodes[6].sizeInDwords   = 26; // bufferData, imageData, isSrgb, isBufferCpy
                    resourceNodes[6].offsetInDwords = 4 * m_bufferViewSizeInDwords + 2 * m_imageViewSizeInDwords;
                    resourceNodes[6].binding        = 0;
                    resourceNodes[6].set            = static_cast<unsigned>(-1); // Vkgc::InternalDescriptorSetId

                    buildInfo.nodeCount      = GpuTexDecoder::AstcInternalPipelineNodes;
                    buildInfo.shaderType     = type;
                    buildInfo.pUserDataNodes = resourceNodes;
                }

                GetSpvCode(buildInfo.shaderType, &(buildInfo.code.pSpvCode), &(buildInfo.code.spvSize));

                result = ClientCreateInternalComputePipeline(
                    m_info,
                    constInfo,
                    buildInfo,
                    &pPipelinePair->pPipeline,
                    &pPipelinePair->pMemory);

                PAL_ASSERT(result == Pal::Result::Success);
            }
        }
    }

    PAL_ASSERT(pPipelinePair->pPipeline);
    return pPipelinePair->pPipeline;
}

// =====================================================================================================================
void Device::BuildBufferViewInfo(
    uint32*                   pData,
    uint8                     count,
    Pal::gpusize              addr,
    Pal::gpusize              dataBytes,
    uint8                     stride,
    Pal::SwizzledFormat       swizzleFormat) const
{
    Pal::BufferViewInfo tableDataView = {};
    tableDataView.gpuAddr             = addr;
    tableDataView.range               = dataBytes;
    tableDataView.stride              = stride;
    tableDataView.swizzledFormat      = swizzleFormat;
    m_info.pPalDevice->CreateUntypedBufferViewSrds(count, &tableDataView, pData);
}

// =====================================================================================================================
void Device::BuildTypedBufferViewInfo(
    uint32*                   pData,
    uint8                     count,
    Pal::gpusize              addr,
    Pal::gpusize              dataBytes,
    uint8                     stride,
    Pal::SwizzledFormat       swizzleFormat) const
{
    Pal::BufferViewInfo tableDataView = {};
    tableDataView.gpuAddr             = addr;
    tableDataView.range               = dataBytes;
    tableDataView.stride              = stride;
    tableDataView.swizzledFormat      = swizzleFormat;
    m_info.pPalDevice->CreateTypedBufferViewSrds(count, &tableDataView, pData);
}

// =====================================================================================================================
void Device::BuildImageViewInfo(
    Pal::ImageViewInfo*  pInfo,
    const Pal::IImage*   pImage,
    const Pal::SubresId& subresId,
    uint32               numSlices,
    Pal::SwizzledFormat  swizzledFormat,
    bool                 isShaderWriteable) const
{
    const Pal::ImageType imageType = pImage->GetImageCreateInfo().imageType;
    pInfo->pImage                  = pImage;
    pInfo->viewType                = static_cast<Pal::ImageViewType>(imageType);
    pInfo->subresRange.startSubres = subresId;
#if PAL_CLIENT_INTERFACE_MAJOR_VERSION >= 642
    pInfo->subresRange.numPlanes = 1;
#endif
    pInfo->subresRange.numMips   = 1;
    pInfo->subresRange.numSlices = numSlices;
    pInfo->swizzledFormat        = swizzledFormat;

    // ASTC/ETC only uses compute shaders, where the write-out surface is assumed to be write-only.
    pInfo->possibleLayouts = { (isShaderWriteable ? Pal::LayoutShaderWrite : Pal::LayoutShaderRead),
                               Pal::EngineTypeUniversal | Pal::EngineTypeCompute };

}

// =====================================================================================================================
Pal::Result Device::CreateTableMemory()
{
    Pal::Result result         = Pal::Result::Success;
    uint32_t colorUnquantiSize = sizeof(ColorQuantizationModeInfo);
    uint32_t triSize           = sizeof(TritsQuintsTable);
    uint32_t quantiModesize    = sizeof(QuantAndXferTables);
    uint32_t totalSize         = colorUnquantiSize + triSize + quantiModesize;

    // Create gpu memory for all table
    Pal::GpuMemoryRequirements memReqs = {};
    CreateMemoryReqs(totalSize, sizeof(uint32_t), &memReqs);
    Pal::GpuMemoryRef memRef;

    Pal::gpusize offset = 0;
    uint8* pData        = nullptr;
    result              = CreateGpuMemory(&memReqs, &m_pTableMemory, &offset);

    if (result == Pal::Result::Success)
    {
        memRef.pGpuMemory = m_pTableMemory;
        result = m_info.pPalDevice->AddGpuMemoryReferences(1, &memRef, nullptr, Pal::GpuMemoryRefCantTrim);
    }

    if (result == Pal::Result::Success)
    {
        result = m_pTableMemory->Map(reinterpret_cast<void**>(&pData));
    }

    if ((result == Pal::Result::Success) && (pData != nullptr))
    {
        // 1.Color UnQuantization Buffer View
        memcpy(pData, &g_ColorQuantizationInfo, colorUnquantiSize);
        offset += colorUnquantiSize;

        // 2.Trits Quints Buffer View
        memcpy(pData + offset, &g_TritsQuintsTbl, triSize);
        offset += triSize;

        // 3.Quant and Transfer Buffer View
        memcpy(pData + offset, &QuantAndXferTables, quantiModesize);
    }

    if (result == Pal::Result::Success)
    {
        m_pTableMemory->Unmap();
    }

    return result;
}

// =====================================================================================================================
Pal::Result Device::SetupInternalTables(
    InternalTexConvertCsType type,
    uint32**                 ppUserData)
{
    // Only Astc need table data
    Pal::Result result = Pal::Result::Success;

    if (type == InternalTexConvertCsType::ConvertASTCToRGBA8)
    {
        if (result == Pal::Result::Success)
        {
            uint32 offset            = 0;
            uint32 colorUnquantiSize = sizeof(ColorQuantizationModeInfo);
            uint32 triSize           = sizeof(TritsQuintsTable);
            uint32 quantiModesize    = sizeof(QuantAndXferTables);

            // 1.Color UnQuantization
            BuildBufferViewInfo(*ppUserData, 1, m_pTableMemory->Desc().gpuVirtAddr, colorUnquantiSize, 1, Pal::UndefinedSwizzledFormat);
            offset += colorUnquantiSize;
            *ppUserData += m_bufferViewSizeInDwords;

            // 2.Trits Quints
            BuildBufferViewInfo(*ppUserData, 1, m_pTableMemory->Desc().gpuVirtAddr + offset, triSize, 1, Pal::UndefinedSwizzledFormat);
            offset += triSize;
            *ppUserData += m_bufferViewSizeInDwords;

            // 3.Quant and Transfer
            BuildBufferViewInfo(*ppUserData, 1, m_pTableMemory->Desc().gpuVirtAddr + offset, quantiModesize, 1, Pal::UndefinedSwizzledFormat);
            *ppUserData += m_bufferViewSizeInDwords;
        }
    }
    return result;
}

}
