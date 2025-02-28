/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2024-2025 Advanced Micro Devices, Inc. All Rights Reserved.
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

	// 1011.7.0
	 #pragma once
const uint32_t Bc3Encoder[] = {
	0x07230203,0x00010000,0x0008000a,0x00001ef6,0x00000000,0x00020011,0x00000001,0x0006000b,
	0x00000001,0x4c534c47,0x6474732e,0x3035342e,0x00000000,0x0003000e,0x00000000,0x00000001,
	0x0009000f,0x00000005,0x00000004,0x6f636e45,0x6c426564,0x736b636f,0x00000000,0x00000a38,
	0x00000a3c,0x00060010,0x00000004,0x00000011,0x00000040,0x00000001,0x00000001,0x00050048,
	0x000009a5,0x00000000,0x00000023,0x00000000,0x00050048,0x000009a5,0x00000001,0x00000023,
	0x00000004,0x00050048,0x000009a5,0x00000002,0x00000023,0x00000008,0x00050048,0x000009a5,
	0x00000003,0x00000023,0x0000000c,0x00030047,0x000009a5,0x00000002,0x00040047,0x000009d9,
	0x00000022,0x00000000,0x00040047,0x000009d9,0x00000021,0x00000000,0x00040047,0x00000a32,
	0x00000022,0x00000000,0x00040047,0x00000a32,0x00000021,0x00000001,0x00040047,0x00000a38,
	0x0000000b,0x0000001d,0x00040047,0x00000a3c,0x0000000b,0x0000001a,0x00020013,0x00000002,
	0x00030021,0x00000003,0x00000002,0x00030016,0x00000006,0x00000020,0x00040020,0x00000007,
	0x00000007,0x00000006,0x00040017,0x0000000c,0x00000006,0x00000003,0x00040020,0x0000000d,
	0x00000007,0x0000000c,0x00040015,0x00000012,0x00000020,0x00000000,0x0004002b,0x00000012,
	0x0000002a,0x00000010,0x0004001c,0x0000002b,0x0000000c,0x0000002a,0x00040015,0x0000002c,
	0x00000020,0x00000001,0x00040020,0x0000002d,0x00000007,0x0000002c,0x0004001c,0x00000036,
	0x00000006,0x0000002a,0x00040020,0x00000037,0x00000007,0x00000036,0x00040017,0x0000004d,
	0x00000006,0x00000002,0x00040017,0x00000055,0x00000012,0x00000002,0x00040020,0x0000006b,
	0x00000007,0x0000002b,0x00040017,0x0000008e,0x00000012,0x00000004,0x00040017,0x00000096,
	0x00000012,0x00000003,0x0004002b,0x00000006,0x0000009e,0x00000000,0x00020014,0x0000009f,
	0x0004002b,0x00000006,0x000000a5,0x3f800000,0x0004002b,0x00000012,0x000000be,0x00000000,
	0x0004002b,0x00000012,0x000000c4,0x00000001,0x0004002b,0x00000012,0x000000ca,0x00000002,
	0x0004002b,0x0000002c,0x000000d6,0x00000008,0x0004002b,0x0000002c,0x000000dc,0x00000003,
	0x0004002b,0x0000002c,0x000000e4,0x00000005,0x0004002b,0x00000012,0x00000176,0x00000004,
	0x0004002b,0x0000002c,0x0000017a,0x00000000,0x0004002b,0x0000002c,0x0000017d,0x00000001,
	0x0004002b,0x0000002c,0x00000180,0x00000002,0x0004002b,0x00000006,0x00000185,0x40400000,
	0x0004001c,0x000001b4,0x0000002c,0x00000176,0x00040020,0x000001b5,0x00000007,0x000001b4,
	0x0007002c,0x000001b4,0x000001b7,0x0000017a,0x00000180,0x000000dc,0x0000017d,0x0004002b,
	0x00000012,0x000001cd,0x00000003,0x0004002b,0x00000006,0x00000208,0x40e00000,0x0004002b,
	0x00000006,0x0000027c,0xc0e00000,0x0004002b,0x00000012,0x00000312,0x00000040,0x0004002b,
	0x00000006,0x00000396,0x437f0000,0x0004001c,0x00000496,0x0000002c,0x0000002a,0x00040020,
	0x00000497,0x00000007,0x00000496,0x0004002b,0x00000012,0x000004a6,0x00000005,0x0004002b,
	0x0000002c,0x000004b0,0x00000007,0x0004002b,0x0000002c,0x0000050f,0x0000001f,0x0004002b,
	0x00000012,0x0000051e,0x00000006,0x0004002b,0x00000006,0x0000064c,0x41f80000,0x0004002b,
	0x00000006,0x0000064d,0x427c0000,0x0006002c,0x0000000c,0x0000064e,0x0000064c,0x0000064d,
	0x0000064c,0x0004002b,0x0000002c,0x00000693,0x0000000b,0x0004002b,0x0000002c,0x000006b5,
	0x00000010,0x0006002c,0x0000000c,0x0000071f,0x0000009e,0x0000009e,0x0000009e,0x0006002c,
	0x0000000c,0x000007ad,0x000000a5,0x000000a5,0x000000a5,0x0006001e,0x000009a5,0x00000012,
	0x00000012,0x00000006,0x00000012,0x00040020,0x000009a6,0x00000009,0x000009a5,0x0004003b,
	0x000009a6,0x000009a7,0x00000009,0x00040020,0x000009a8,0x00000009,0x00000012,0x00040017,
	0x000009d2,0x00000006,0x00000004,0x0004001c,0x000009d3,0x000009d2,0x00000312,0x00040020,
	0x000009d4,0x00000004,0x000009d3,0x0004003b,0x000009d4,0x000009d5,0x00000004,0x00090019,
	0x000009d7,0x00000006,0x00000001,0x00000000,0x00000000,0x00000000,0x00000001,0x00000000,
	0x00040020,0x000009d8,0x00000000,0x000009d7,0x0004003b,0x000009d8,0x000009d9,0x00000000,
	0x00040017,0x000009e4,0x0000002c,0x00000003,0x00040017,0x000009e6,0x0000002c,0x00000002,
	0x00040020,0x000009f4,0x00000004,0x000009d2,0x0004002b,0x00000012,0x000009f6,0x00000108,
	0x00040020,0x00000a09,0x00000004,0x00000006,0x00090019,0x00000a30,0x00000012,0x00000001,
	0x00000000,0x00000000,0x00000000,0x00000002,0x0000001e,0x00040020,0x00000a31,0x00000000,
	0x00000a30,0x0004003b,0x00000a31,0x00000a32,0x00000000,0x00040020,0x00000a37,0x00000001,
	0x00000012,0x0004003b,0x00000a37,0x00000a38,0x00000001,0x00040020,0x00000a3b,0x00000001,
	0x00000096,0x0004003b,0x00000a3b,0x00000a3c,0x00000001,0x0004002b,0x00000006,0x00001eb7,
	0x3d042108,0x0004002b,0x00000006,0x00001eb8,0x3c820821,0x0006002c,0x0000000c,0x00001eb9,
	0x00001eb7,0x00001eb8,0x00001eb7,0x00030001,0x0000008e,0x00001edb,0x00030001,0x0000004d,
	0x00001ef4,0x00030001,0x00000055,0x00001ef5,0x00050036,0x00000002,0x00000004,0x00000000,
	0x00000003,0x000200f8,0x00000005,0x0004003b,0x000001b5,0x0000172f,0x00000007,0x0004003b,
	0x0000006b,0x00001732,0x00000007,0x0004003b,0x0000006b,0x000013d8,0x00000007,0x0004003b,
	0x0000006b,0x000013db,0x00000007,0x0004003b,0x00000497,0x00001203,0x00000007,0x0004003b,
	0x00000037,0x00000aff,0x00000007,0x0004003b,0x00000037,0x00000b03,0x00000007,0x0004003b,
	0x0000006b,0x00000a5e,0x00000007,0x0004003b,0x00000037,0x00000a5f,0x00000007,0x0004003d,
	0x00000012,0x00000a39,0x00000a38,0x0004003d,0x00000096,0x00000a3d,0x00000a3c,0x00050086,
	0x00000012,0x00000a67,0x00000a39,0x0000002a,0x00050041,0x000009a8,0x00000a68,0x000009a7,
	0x0000017d,0x0004003d,0x00000012,0x00000a69,0x00000a68,0x00050051,0x00000012,0x00000a6b,
	0x00000a3d,0x00000000,0x00050084,0x00000012,0x00000a6c,0x00000a6b,0x00000176,0x00050080,
	0x00000012,0x00000a6d,0x00000a69,0x00000a6c,0x00050080,0x00000012,0x00000a6f,0x00000a6d,
	0x00000a67,0x00050084,0x00000012,0x00000a71,0x00000a67,0x0000002a,0x00050082,0x00000012,
	0x00000a74,0x00000a39,0x00000a71,0x00050041,0x000009a8,0x00000a76,0x000009a7,0x0000017a,
	0x0004003d,0x00000012,0x00000a77,0x00000a76,0x00050086,0x00000012,0x00000a78,0x00000a6f,
	0x00000a77,0x00050041,0x000009a8,0x00000a7b,0x000009a7,0x0000017a,0x0004003d,0x00000012,
	0x00000a7c,0x00000a7b,0x00050084,0x00000012,0x00000a7d,0x00000a78,0x00000a7c,0x00050082,
	0x00000012,0x00000a7e,0x00000a6f,0x00000a7d,0x00050084,0x00000012,0x00000a80,0x00000a7e,
	0x00000176,0x00050084,0x00000012,0x00000a82,0x00000a78,0x00000176,0x00050050,0x00000055,
	0x00000a85,0x00000a7e,0x00000a78,0x000500b0,0x0000009f,0x00000a87,0x00000a74,0x0000002a,
	0x000300f7,0x00000aa3,0x00000000,0x000400fa,0x00000a87,0x00000a88,0x00000aa3,0x000200f8,
	0x00000a88,0x0004003d,0x000009d7,0x00000a8a,0x000009d9,0x00050089,0x00000012,0x00000a8d,
	0x00000a74,0x00000176,0x00050080,0x00000012,0x00000a8e,0x00000a80,0x00000a8d,0x00050086,
	0x00000012,0x00000a91,0x00000a74,0x00000176,0x00050080,0x00000012,0x00000a92,0x00000a82,
	0x00000a91,0x00060050,0x00000096,0x00000a93,0x00000a8e,0x00000a92,0x000000be,0x0004007c,
	0x000009e4,0x00000a94,0x00000a93,0x0007004f,0x000009e6,0x00000a95,0x00000a94,0x00000a94,
	0x00000000,0x00000001,0x00050089,0x00000012,0x00000a98,0x00000a74,0x00000176,0x00050080,
	0x00000012,0x00000a99,0x00000a80,0x00000a98,0x00050086,0x00000012,0x00000a9c,0x00000a74,
	0x00000176,0x00050080,0x00000012,0x00000a9d,0x00000a82,0x00000a9c,0x00060050,0x00000096,
	0x00000a9e,0x00000a99,0x00000a9d,0x000000be,0x0004007c,0x000009e4,0x00000a9f,0x00000a9e,
	0x00050051,0x0000002c,0x00000aa0,0x00000a9f,0x00000002,0x0007005f,0x000009d2,0x00000aa1,
	0x00000a8a,0x00000a95,0x00000002,0x00000aa0,0x00050041,0x000009f4,0x00000aa2,0x000009d5,
	0x00000a39,0x0003003e,0x00000aa2,0x00000aa1,0x000200f9,0x00000aa3,0x000200f8,0x00000aa3,
	0x000400e0,0x000000ca,0x000000ca,0x000009f6,0x000500aa,0x0000009f,0x00000aa5,0x00000a74,
	0x000000be,0x000300f7,0x00000ad6,0x00000000,0x000400fa,0x00000aa5,0x00000aa6,0x00000ad6,
	0x000200f8,0x00000aa6,0x000200f9,0x00000aa7,0x000200f8,0x00000aa7,0x000700f5,0x0000002c,
	0x00001ec4,0x0000017a,0x00000aa6,0x00000ace,0x00000aab,0x000500b1,0x0000009f,0x00000aaa,
	0x00001ec4,0x000006b5,0x000400f6,0x00000acf,0x00000aab,0x00000000,0x000400fa,0x00000aaa,
	0x00000aab,0x00000acf,0x000200f8,0x00000aab,0x0004007c,0x00000012,0x00000aaf,0x00001ec4,
	0x00050080,0x00000012,0x00000ab0,0x00000a71,0x00000aaf,0x00060041,0x00000a09,0x00000ab1,
	0x000009d5,0x00000ab0,0x000000be,0x0004003d,0x00000006,0x00000ab2,0x00000ab1,0x00060041,
	0x00000007,0x00000ab3,0x00000a5e,0x00001ec4,0x000000be,0x0003003e,0x00000ab3,0x00000ab2,
	0x0004007c,0x00000012,0x00000ab7,0x00001ec4,0x00050080,0x00000012,0x00000ab8,0x00000a71,
	0x00000ab7,0x00060041,0x00000a09,0x00000ab9,0x000009d5,0x00000ab8,0x000000c4,0x0004003d,
	0x00000006,0x00000aba,0x00000ab9,0x00060041,0x00000007,0x00000abb,0x00000a5e,0x00001ec4,
	0x000000c4,0x0003003e,0x00000abb,0x00000aba,0x0004007c,0x00000012,0x00000abf,0x00001ec4,
	0x00050080,0x00000012,0x00000ac0,0x00000a71,0x00000abf,0x00060041,0x00000a09,0x00000ac1,
	0x000009d5,0x00000ac0,0x000000ca,0x0004003d,0x00000006,0x00000ac2,0x00000ac1,0x00060041,
	0x00000007,0x00000ac3,0x00000a5e,0x00001ec4,0x000000ca,0x0003003e,0x00000ac3,0x00000ac2,
	0x0004007c,0x00000012,0x00000ac7,0x00001ec4,0x00050080,0x00000012,0x00000ac8,0x00000a71,
	0x00000ac7,0x00060041,0x00000a09,0x00000ac9,0x000009d5,0x00000ac8,0x000001cd,0x0004003d,
	0x00000006,0x00000aca,0x00000ac9,0x00050041,0x00000007,0x00000acb,0x00000a5f,0x00001ec4,
	0x0003003e,0x00000acb,0x00000aca,0x00050080,0x0000002c,0x00000ace,0x00001ec4,0x0000017d,
	0x000200f9,0x00000aa7,0x000200f8,0x00000acf,0x0004003d,0x0000002b,0x00000ad0,0x00000a5e,
	0x0004003d,0x00000036,0x00000ad1,0x00000a5f,0x0003003e,0x00000aff,0x00000ad1,0x000300f7,
	0x00001107,0x00000000,0x000300fb,0x000000be,0x00000f94,0x000200f8,0x00000f94,0x00050041,
	0x00000007,0x00000f98,0x00000aff,0x0000017a,0x0004003d,0x00000006,0x00000f99,0x00000f98,
	0x00060052,0x0000004d,0x00001dd5,0x00000f99,0x00001ef4,0x00000000,0x00050041,0x00000007,
	0x00000f9b,0x00000aff,0x0000017a,0x0004003d,0x00000006,0x00000f9c,0x00000f9b,0x00060052,
	0x0000004d,0x00001dd7,0x00000f9c,0x00001dd5,0x00000001,0x000200f9,0x00000f9e,0x000200f8,
	0x00000f9e,0x000700f5,0x0000004d,0x00001ec9,0x00001dd7,0x00000f94,0x00001ddd,0x00000fa2,
	0x000700f5,0x00000012,0x00001ec8,0x000000c4,0x00000f94,0x00000fb3,0x00000fa2,0x000500b0,
	0x0000009f,0x00000fa1,0x00001ec8,0x0000002a,0x000400f6,0x00000fb4,0x00000fa2,0x00000000,
	0x000400fa,0x00000fa1,0x00000fa2,0x00000fb4,0x000200f8,0x00000fa2,0x00050051,0x00000006,
	0x00000fa4,0x00001ec9,0x00000000,0x00050041,0x00000007,0x00000fa6,0x00000aff,0x00001ec8,
	0x0004003d,0x00000006,0x00000fa7,0x00000fa6,0x0007000c,0x00000006,0x00000fa8,0x00000001,
	0x00000025,0x00000fa4,0x00000fa7,0x00060052,0x0000004d,0x00001dda,0x00000fa8,0x00001ef4,
	0x00000000,0x00050051,0x00000006,0x00000fab,0x00001ec9,0x00000001,0x00050041,0x00000007,
	0x00000fad,0x00000aff,0x00001ec8,0x0004003d,0x00000006,0x00000fae,0x00000fad,0x0007000c,
	0x00000006,0x00000faf,0x00000001,0x00000028,0x00000fab,0x00000fae,0x00060052,0x0000004d,
	0x00001ddd,0x00000faf,0x00001dda,0x00000001,0x00050080,0x00000012,0x00000fb3,0x00001ec8,
	0x0000017d,0x000200f9,0x00000f9e,0x000200f8,0x00000fb4,0x000200f9,0x00001107,0x000200f8,
	0x00001107,0x0003003e,0x00000b03,0x00000ad1,0x00050051,0x00000006,0x00001214,0x00001ec9,
	0x00000000,0x00050051,0x00000006,0x00001216,0x00001ec9,0x00000001,0x000500b7,0x0000009f,
	0x00001217,0x00001214,0x00001216,0x000300f7,0x0000121f,0x00000000,0x000400fa,0x00001217,
	0x00001218,0x0000121e,0x000200f8,0x0000121e,0x000200f9,0x0000121f,0x000200f8,0x00001218,
	0x00050051,0x00000006,0x0000121a,0x00001ec9,0x00000000,0x00050051,0x00000006,0x0000121c,
	0x00001ec9,0x00000001,0x00050083,0x00000006,0x0000121d,0x0000121a,0x0000121c,0x000200f9,
	0x0000121f,0x000200f8,0x0000121f,0x000700f5,0x00000006,0x00001eca,0x000000a5,0x0000121e,
	0x0000121d,0x00001218,0x00050088,0x00000006,0x00001221,0x00000208,0x00001eca,0x00050088,
	0x00000006,0x00001223,0x0000027c,0x00001eca,0x00050051,0x00000006,0x00001225,0x00001ec9,
	0x00000001,0x00050085,0x00000006,0x00001226,0x00001223,0x00001225,0x000200f9,0x00001227,
	0x000200f8,0x00001227,0x000700f5,0x00000012,0x00001ecc,0x000000be,0x0000121f,0x00001277,
	0x00001275,0x000500b0,0x0000009f,0x0000122a,0x00001ecc,0x0000002a,0x000400f6,0x00001278,
	0x00001275,0x00000000,0x000400fa,0x0000122a,0x0000122b,0x00001278,0x000200f8,0x0000122b,
	0x00050041,0x00000007,0x0000122e,0x00000b03,0x00001ecc,0x0004003d,0x00000006,0x0000122f,
	0x0000122e,0x00050085,0x00000006,0x00001231,0x0000122f,0x00001221,0x00050081,0x00000006,
	0x00001233,0x00001231,0x00001226,0x0006000c,0x00000006,0x00001234,0x00000001,0x00000002,
	0x00001233,0x0004006d,0x00000012,0x00001235,0x00001234,0x0004007c,0x0000002c,0x00001236,
	0x00001235,0x00050041,0x0000002d,0x00001237,0x00001203,0x00001ecc,0x0003003e,0x00001237,
	0x00001236,0x000500b0,0x0000009f,0x00001239,0x00001ecc,0x000004a6,0x000300f7,0x00001274,
	0x00000000,0x000400fa,0x00001239,0x0000123a,0x0000124c,0x000200f8,0x0000124c,0x000500ac,
	0x0000009f,0x0000124e,0x00001ecc,0x000004a6,0x000300f7,0x00001273,0x00000000,0x000400fa,
	0x0000124e,0x0000124f,0x00001261,0x000200f8,0x00001261,0x00050041,0x0000002d,0x00001264,
	0x00001203,0x00001ecc,0x0004003d,0x0000002c,0x00001265,0x00001264,0x000500ad,0x0000009f,
	0x00001266,0x00001265,0x0000017a,0x000600a9,0x0000002c,0x00001267,0x00001266,0x0000017d,
	0x0000017a,0x00050041,0x0000002d,0x00001269,0x00001203,0x00001ecc,0x0004003d,0x0000002c,
	0x0000126a,0x00001269,0x000500aa,0x0000009f,0x0000126b,0x0000126a,0x000004b0,0x000600a9,
	0x0000002c,0x0000126c,0x0000126b,0x0000017d,0x0000017a,0x00050084,0x0000002c,0x0000126d,
	0x000004b0,0x0000126c,0x00050082,0x0000002c,0x0000126e,0x00001267,0x0000126d,0x00050041,
	0x0000002d,0x0000126f,0x00001203,0x00001ecc,0x0004003d,0x0000002c,0x00001270,0x0000126f,
	0x00050080,0x0000002c,0x00001271,0x00001270,0x0000126e,0x00050041,0x0000002d,0x00001272,
	0x00001203,0x00001ecc,0x0003003e,0x00001272,0x00001271,0x000200f9,0x00001273,0x000200f8,
	0x0000124f,0x00050041,0x0000002d,0x00001252,0x00001203,0x00001ecc,0x0004003d,0x0000002c,
	0x00001253,0x00001252,0x000500ad,0x0000009f,0x00001254,0x00001253,0x0000017a,0x000600a9,
	0x0000002c,0x00001255,0x00001254,0x0000017d,0x0000017a,0x00050041,0x0000002d,0x00001257,
	0x00001203,0x00001ecc,0x0004003d,0x0000002c,0x00001258,0x00001257,0x000500aa,0x0000009f,
	0x00001259,0x00001258,0x000004b0,0x000600a9,0x0000002c,0x0000125a,0x00001259,0x0000017d,
	0x0000017a,0x00050084,0x0000002c,0x0000125b,0x000004b0,0x0000125a,0x00050082,0x0000002c,
	0x0000125c,0x00001255,0x0000125b,0x00050041,0x0000002d,0x0000125d,0x00001203,0x00001ecc,
	0x0004003d,0x0000002c,0x0000125e,0x0000125d,0x00050080,0x0000002c,0x0000125f,0x0000125e,
	0x0000125c,0x00050041,0x0000002d,0x00001260,0x00001203,0x00001ecc,0x0003003e,0x00001260,
	0x0000125f,0x000200f9,0x00001273,0x000200f8,0x00001273,0x000200f9,0x00001274,0x000200f8,
	0x0000123a,0x00050041,0x0000002d,0x0000123d,0x00001203,0x00001ecc,0x0004003d,0x0000002c,
	0x0000123e,0x0000123d,0x000500ad,0x0000009f,0x0000123f,0x0000123e,0x0000017a,0x000600a9,
	0x0000002c,0x00001240,0x0000123f,0x0000017d,0x0000017a,0x00050041,0x0000002d,0x00001242,
	0x00001203,0x00001ecc,0x0004003d,0x0000002c,0x00001243,0x00001242,0x000500aa,0x0000009f,
	0x00001244,0x00001243,0x000004b0,0x000600a9,0x0000002c,0x00001245,0x00001244,0x0000017d,
	0x0000017a,0x00050084,0x0000002c,0x00001246,0x000004b0,0x00001245,0x00050082,0x0000002c,
	0x00001247,0x00001240,0x00001246,0x00050041,0x0000002d,0x00001248,0x00001203,0x00001ecc,
	0x0004003d,0x0000002c,0x00001249,0x00001248,0x00050080,0x0000002c,0x0000124a,0x00001249,
	0x00001247,0x00050041,0x0000002d,0x0000124b,0x00001203,0x00001ecc,0x0003003e,0x0000124b,
	0x0000124a,0x000200f9,0x00001274,0x000200f8,0x00001274,0x000200f9,0x00001275,0x000200f8,
	0x00001275,0x00050080,0x00000012,0x00001277,0x00001ecc,0x0000017d,0x000200f9,0x00001227,
	0x000200f8,0x00001278,0x00050051,0x00000006,0x0000127a,0x00001ec9,0x00000000,0x00050085,
	0x00000006,0x0000127b,0x0000127a,0x00000396,0x0006000c,0x00000006,0x0000127c,0x00000001,
	0x00000002,0x0000127b,0x0004006d,0x00000012,0x0000127d,0x0000127c,0x00050051,0x00000006,
	0x0000127f,0x00001ec9,0x00000001,0x00050085,0x00000006,0x00001280,0x0000127f,0x00000396,
	0x0006000c,0x00000006,0x00001281,0x00000001,0x00000002,0x00001280,0x0004006d,0x00000012,
	0x00001282,0x00001281,0x000500c4,0x00000012,0x00001284,0x0000127d,0x000000d6,0x000500c5,
	0x00000012,0x00001286,0x00001284,0x00001282,0x00060052,0x00000055,0x00001dea,0x00001286,
	0x00001ef5,0x00000000,0x00060052,0x00000055,0x00001dec,0x000000be,0x00001dea,0x00000001,
	0x000200f9,0x00001289,0x000200f8,0x00001289,0x000700f5,0x00000055,0x00001ed1,0x00001dec,
	0x00001278,0x00001def,0x0000128d,0x000700f5,0x00000012,0x00001ed0,0x000000be,0x00001278,
	0x0000129c,0x0000128d,0x000500b0,0x0000009f,0x0000128c,0x00001ed0,0x000004a6,0x000400f6,
	0x0000129d,0x0000128d,0x00000000,0x000400fa,0x0000128c,0x0000128d,0x0000129d,0x000200f8,
	0x0000128d,0x00050041,0x0000002d,0x0000128f,0x00001203,0x00001ed0,0x0004003d,0x0000002c,
	0x00001290,0x0000128f,0x00050084,0x00000012,0x00001292,0x00001ed0,0x000001cd,0x00050080,
	0x00000012,0x00001293,0x0000002a,0x00001292,0x000500c4,0x0000002c,0x00001294,0x00001290,
	0x00001293,0x0004007c,0x00000012,0x00001295,0x00001294,0x00050051,0x00000012,0x00001297,
	0x00001ed1,0x00000000,0x000500c5,0x00000012,0x00001298,0x00001297,0x00001295,0x00060052,
	0x00000055,0x00001def,0x00001298,0x00001ed1,0x00000000,0x00050080,0x00000012,0x0000129c,
	0x00001ed0,0x0000017d,0x000200f9,0x00001289,0x000200f8,0x0000129d,0x00050041,0x0000002d,
	0x0000129e,0x00001203,0x000000e4,0x0004003d,0x0000002c,0x0000129f,0x0000129e,0x000500c4,
	0x0000002c,0x000012a0,0x0000129f,0x0000050f,0x0004007c,0x00000012,0x000012a1,0x000012a0,
	0x00050051,0x00000012,0x000012a3,0x00001ed1,0x00000000,0x000500c5,0x00000012,0x000012a4,
	0x000012a3,0x000012a1,0x00060052,0x00000055,0x00001df2,0x000012a4,0x00001ef5,0x00000000,
	0x00050041,0x0000002d,0x000012a6,0x00001203,0x000000e4,0x0004003d,0x0000002c,0x000012a7,
	0x000012a6,0x000500c3,0x0000002c,0x000012a8,0x000012a7,0x0000017d,0x0004007c,0x00000012,
	0x000012a9,0x000012a8,0x00050051,0x00000012,0x000012ab,0x00001ed1,0x00000001,0x000500c5,
	0x00000012,0x000012ac,0x000012ab,0x000012a9,0x00060052,0x00000055,0x00001df5,0x000012ac,
	0x00001df2,0x00000001,0x000200f9,0x000012ae,0x000200f8,0x000012ae,0x000700f5,0x00000055,
	0x00001ed3,0x00001df5,0x0000129d,0x00001df8,0x000012b2,0x000700f5,0x00000012,0x00001ed2,
	0x0000051e,0x0000129d,0x000012c1,0x000012b2,0x000500b0,0x0000009f,0x000012b1,0x00001ed2,
	0x0000002a,0x000400f6,0x000012c2,0x000012b2,0x00000000,0x000400fa,0x000012b1,0x000012b2,
	0x000012c2,0x000200f8,0x000012b2,0x00050041,0x0000002d,0x000012b4,0x00001203,0x00001ed2,
	0x0004003d,0x0000002c,0x000012b5,0x000012b4,0x00050084,0x00000012,0x000012b7,0x00001ed2,
	0x000001cd,0x00050082,0x00000012,0x000012b8,0x000012b7,0x0000002a,0x000500c4,0x0000002c,
	0x000012b9,0x000012b5,0x000012b8,0x0004007c,0x00000012,0x000012ba,0x000012b9,0x00050051,
	0x00000012,0x000012bc,0x00001ed3,0x00000001,0x000500c5,0x00000012,0x000012bd,0x000012bc,
	0x000012ba,0x00060052,0x00000055,0x00001df8,0x000012bd,0x00001ed3,0x00000001,0x00050080,
	0x00000012,0x000012c1,0x00001ed2,0x0000017d,0x000200f9,0x000012ae,0x000200f8,0x000012c2,
	0x00050051,0x00000012,0x00000ae6,0x00001ed3,0x00000000,0x00060052,0x0000008e,0x00001e17,
	0x00000ae6,0x00001edb,0x00000000,0x00050051,0x00000012,0x00000ae9,0x00001ed3,0x00000001,
	0x00060052,0x0000008e,0x00001e1a,0x00000ae9,0x00001e17,0x00000001,0x000300f7,0x00001587,
	0x00000000,0x000300fb,0x000000be,0x00001408,0x000200f8,0x00001408,0x000200f9,0x0000140c,
	0x000200f8,0x0000140c,0x000700f5,0x0000000c,0x00001ede,0x0000071f,0x00001408,0x00001594,
	0x00001410,0x000700f5,0x0000000c,0x00001edd,0x000007ad,0x00001408,0x0000158e,0x00001410,
	0x000700f5,0x0000002c,0x00001edc,0x0000017a,0x00001408,0x0000143b,0x00001410,0x000500b1,
	0x0000009f,0x0000140f,0x00001edc,0x000006b5,0x000400f6,0x0000143c,0x00001410,0x00000000,
	0x000400fa,0x0000140f,0x00001410,0x0000143c,0x000200f8,0x00001410,0x0003003e,0x000013d8,
	0x00000ad0,0x00050041,0x0000000d,0x00001413,0x000013d8,0x00001edc,0x0004003d,0x0000000c,
	0x00001414,0x00001413,0x0007000c,0x0000000c,0x0000158e,0x00000001,0x00000025,0x00001edd,
	0x00001414,0x0003003e,0x000013db,0x00000ad0,0x00050041,0x0000000d,0x00001418,0x000013db,
	0x00001edc,0x0004003d,0x0000000c,0x00001419,0x00001418,0x0007000c,0x0000000c,0x00001594,
	0x00000001,0x00000028,0x00001ede,0x00001419,0x00050080,0x0000002c,0x0000143b,0x00001edc,
	0x0000017d,0x000200f9,0x0000140c,0x000200f8,0x0000143c,0x0008000c,0x0000000c,0x00001720,
	0x00000001,0x0000002b,0x00001edd,0x0000071f,0x000007ad,0x0008000c,0x0000000c,0x00001728,
	0x00000001,0x0000002b,0x00001ede,0x0000071f,0x000007ad,0x00050085,0x0000000c,0x00001624,
	0x00001720,0x0000064e,0x0006000c,0x0000000c,0x00001625,0x00000001,0x00000008,0x00001624,
	0x00050085,0x0000000c,0x00001628,0x00001728,0x0000064e,0x0006000c,0x0000000c,0x00001629,
	0x00000001,0x00000009,0x00001628,0x00050085,0x0000000c,0x0000162c,0x00001625,0x00001eb9,
	0x00050085,0x0000000c,0x0000162f,0x00001629,0x00001eb9,0x00050051,0x00000006,0x00001632,
	0x00001625,0x00000000,0x0004006d,0x00000012,0x00001633,0x00001632,0x0004007c,0x0000002c,
	0x00001634,0x00001633,0x00050051,0x00000006,0x00001636,0x00001625,0x00000001,0x0004006d,
	0x00000012,0x00001637,0x00001636,0x0004007c,0x0000002c,0x00001638,0x00001637,0x00050051,
	0x00000006,0x0000163a,0x00001625,0x00000002,0x0004006d,0x00000012,0x0000163b,0x0000163a,
	0x0004007c,0x0000002c,0x0000163c,0x0000163b,0x000500c4,0x0000002c,0x0000163e,0x00001634,
	0x00000693,0x000500c4,0x0000002c,0x00001640,0x00001638,0x000000e4,0x000500c5,0x0000002c,
	0x00001641,0x0000163e,0x00001640,0x000500c5,0x0000002c,0x00001643,0x00001641,0x0000163c,
	0x0004007c,0x00000012,0x00001644,0x00001643,0x00050051,0x00000006,0x00001646,0x00001629,
	0x00000000,0x0004006d,0x00000012,0x00001647,0x00001646,0x0004007c,0x0000002c,0x00001648,
	0x00001647,0x00050051,0x00000006,0x0000164a,0x00001629,0x00000001,0x0004006d,0x00000012,
	0x0000164b,0x0000164a,0x0004007c,0x0000002c,0x0000164c,0x0000164b,0x00050051,0x00000006,
	0x0000164e,0x00001629,0x00000002,0x0004006d,0x00000012,0x0000164f,0x0000164e,0x0004007c,
	0x0000002c,0x00001650,0x0000164f,0x000500c4,0x0000002c,0x00001652,0x00001648,0x00000693,
	0x000500c4,0x0000002c,0x00001654,0x0000164c,0x000000e4,0x000500c5,0x0000002c,0x00001655,
	0x00001652,0x00001654,0x000500c5,0x0000002c,0x00001657,0x00001655,0x00001650,0x0004007c,
	0x00000012,0x00001658,0x00001657,0x000500b0,0x0000009f,0x0000144c,0x00001644,0x00001658,
	0x000300f7,0x00001463,0x00000000,0x000400fa,0x0000144c,0x0000144d,0x0000145b,0x000200f8,
	0x0000145b,0x000500c4,0x00000012,0x0000145d,0x00001658,0x000006b5,0x000500c5,0x00000012,
	0x0000145f,0x0000145d,0x00001644,0x00060052,0x00000055,0x00001e45,0x0000145f,0x00001ef5,
	0x00000000,0x00060052,0x00000055,0x00001e47,0x000000be,0x00001e45,0x00000001,0x000200f9,
	0x00001587,0x000200f8,0x0000144d,0x000500c4,0x00000012,0x0000144f,0x00001644,0x000006b5,
	0x000500c5,0x00000012,0x00001451,0x0000144f,0x00001658,0x00060052,0x00000055,0x00001e41,
	0x00001451,0x00001ef5,0x00000000,0x00050083,0x0000000c,0x0000175b,0x0000162c,0x0000162f,
	0x00050083,0x0000000c,0x0000175e,0x0000162c,0x0000162f,0x00050094,0x00000006,0x0000175f,
	0x0000175b,0x0000175e,0x00050088,0x00000006,0x00001760,0x00000185,0x0000175f,0x00050083,
	0x0000000c,0x00001763,0x0000162c,0x0000162f,0x0005008e,0x0000000c,0x00001765,0x00001763,
	0x00001760,0x00050094,0x00000006,0x00001768,0x0000162f,0x0000162f,0x00050094,0x00000006,
	0x0000176b,0x0000162f,0x0000162c,0x00050083,0x00000006,0x0000176c,0x00001768,0x0000176b,
	0x00050085,0x00000006,0x0000176e,0x0000176c,0x00001760,0x0003003e,0x0000172f,0x000001b7,
	0x000200f9,0x0000176f,0x000200f8,0x0000176f,0x000700f5,0x00000012,0x00001ee2,0x000000be,
	0x0000144d,0x00001ef1,0x000017a3,0x000700f5,0x00000012,0x00001ee1,0x000000be,0x0000144d,
	0x000017a5,0x000017a3,0x000500b0,0x0000009f,0x00001772,0x00001ee1,0x0000002a,0x000400f6,
	0x000017a6,0x000017a3,0x00000000,0x000400fa,0x00001772,0x00001773,0x000017a6,0x000200f8,
	0x00001773,0x0003003e,0x00001732,0x00000ad0,0x00050041,0x0000000d,0x00001775,0x00001732,
	0x00001ee1,0x0004003d,0x0000000c,0x00001776,0x00001775,0x00050094,0x00000006,0x00001778,
	0x00001776,0x00001765,0x00050081,0x00000006,0x0000177a,0x00001778,0x0000176e,0x0006000c,
	0x00000006,0x0000177c,0x00000001,0x00000002,0x0000177a,0x0004006d,0x00000012,0x0000177d,
	0x0000177c,0x000500c7,0x00000012,0x0000177e,0x0000177d,0x000001cd,0x00050041,0x0000002d,
	0x00001780,0x0000172f,0x0000177e,0x0004003d,0x0000002c,0x00001781,0x00001780,0x0004007c,
	0x00000012,0x00001782,0x00001781,0x000500ab,0x0000009f,0x0000179a,0x00001782,0x000000be,
	0x000300f7,0x000017a2,0x00000000,0x000400fa,0x0000179a,0x0000179b,0x000017a2,0x000200f8,
	0x0000179b,0x00050084,0x00000012,0x0000179e,0x000000ca,0x00001ee1,0x000500c4,0x00000012,
	0x0000179f,0x00001782,0x0000179e,0x000500c5,0x00000012,0x000017a1,0x00001ee2,0x0000179f,
	0x000200f9,0x000017a2,0x000200f8,0x000017a2,0x000700f5,0x00000012,0x00001ef1,0x00001ee2,
	0x00001773,0x000017a1,0x0000179b,0x000200f9,0x000017a3,0x000200f8,0x000017a3,0x00050080,
	0x00000012,0x000017a5,0x00001ee1,0x0000017d,0x000200f9,0x0000176f,0x000200f8,0x000017a6,
	0x00060052,0x00000055,0x00001e43,0x00001ee2,0x00001e41,0x00000001,0x000200f9,0x00001463,
	0x000200f8,0x00001463,0x000200f9,0x00001587,0x000200f8,0x00001587,0x000700f5,0x00000055,
	0x00001ee4,0x00001e47,0x0000145b,0x00001e43,0x00001463,0x00050051,0x00000012,0x00000af0,
	0x00001ee4,0x00000000,0x00060052,0x0000008e,0x00001eaf,0x00000af0,0x00001e1a,0x00000002,
	0x00050051,0x00000012,0x00000af3,0x00001ee4,0x00000001,0x00060052,0x0000008e,0x00001eb2,
	0x00000af3,0x00001eaf,0x00000003,0x0004003d,0x00000a30,0x00000ad3,0x00000a32,0x00040063,
	0x00000ad3,0x00000a85,0x00001eb2,0x000200f9,0x00000ad6,0x000200f8,0x00000ad6,0x000100fd,
	0x00010038
};
