/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2015-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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
* @file  sqtt_rgp_annotations.h
* @brief Vulkan API SQTT annotation marker structures.  These are added when SQ thread tracing output is enabled
*        e.g. for RGP.  They are based on the RGP SQTT instrumentation specification at //devtools/main/RGP/
*        documentation/RGP-SQTT-Instrumentation-Specification.docx in the devtools depot.
***********************************************************************************************************************
*/
#ifndef __SQTT_SQTT_RGP_ANNOTATIONS_H__
#define __SQTT_SQTT_RGP_ANNOTATIONS_H__
#pragma once

#include <stdint.h>

// RGP SQTT Instrumentation Specification version (API-independent)
constexpr uint32_t RgpSqttInstrumentationSpecVersion = 1;

// RGP SQTT Instrumentation Specification version for Vulkan-specific tables
constexpr uint32_t RgpSqttInstrumentationApiVersion  = 0;

#if defined(BIGENDIAN_CPU) || defined(__BIG_ENDIAN__)
static_assert(false, "The bitfields in this header match the RGP format specification with the assumption that "
    "the CPU is little-endian.  If we ever support big-endian CPUs, we need to update this header.");
#endif

namespace vk
{

// RgpSqttMarkerIdentifier - Identifiers for RGP SQ thread-tracing markers (Table 1)
enum RgpSqttMarkerIdentifier : uint32_t
{
    RgpSqttMarkerIdentifierEvent               = 0x0,
    RgpSqttMarkerIdentifierCbStart             = 0x1,
    RgpSqttMarkerIdentifierCbEnd               = 0x2,
    RgpSqttMarkerIdentifierBarrierStart        = 0x3,
    RgpSqttMarkerIdentifierBarrierEnd          = 0x4,
    RgpSqttMarkerIdentifierUserEvent           = 0x5,
    RgpSqttMarkerIdentifierGeneralApi          = 0x6,
    RgpSqttMarkerIdentifierSync                = 0x7,
    RgpSqttMarkerIdentifierPresent             = 0x8,
    RgpSqttMarkerIdentifierLayoutTransition    = 0x9,
    RgpSqttMarkerIdentifierRenderPass          = 0xA,
    RgpSqttMarkerIdentifierReserved2           = 0xB,
    RgpSqttMarkerIdentifierBindPipeline        = 0xC,
    RgpSqttMarkerIdentifierReserved4           = 0xD,
    RgpSqttMarkerIdentifierReserved5           = 0xE,
    RgpSqttMarkerIdentifierReserved6           = 0xF
};

// RgpSqttMarkerCbID - Command buffer IDs used in RGP SQ thread-tracing markers. Only 20 bits can be used.
union RgpSqttMarkerCbID
{
    struct
    {
        uint32_t perFrame    : 1;  // Must be set 1, indicating a frame-based command buffer ID
        uint32_t frameIndex  : 7;  // Index of the frame
        uint32_t cbIndex     : 12; // Command buffer index within the frame
        uint32_t reserved    : 12; // Unused bits
    } perFrameCbID;

    struct
    {
        uint32_t perFrame    : 1;  // Must be set 0, indicating a global command buffer ID (for a queue type)
        uint32_t cbIndex     : 19; // a global command buffer index (for a queue type)
        uint32_t reserved    : 12; // Unused bits
    } globalCbID;

    uint32_t u32All;               // uint32 value
};

// These values are based on the annotation marker
constexpr uint32_t RgpSqttMaxPerFrameCbIndex = (1UL << 12) - 1;
constexpr uint32_t RgpSqttMaxFrameIndex      = (1UL << 7) - 1;
constexpr uint32_t RgpSqttMaxGlobalCbIndex   = (1UL << 19) - 1;

// RgpSqttMarkerCbStart - RGP SQ thread-tracing marker for the start of a command buffer. (Table 2)
struct RgpSqttMarkerCbStart
{
    union
    {
        struct
        {
            uint32_t identifier  : 4;  // Identifier for this marker
            uint32_t extDwords   : 3;  // Number of extra dwords following this marker
            uint32_t cbID        : 20; // Command buffer ID for this marker
            uint32_t queue       : 5;  // Queue family index
        };

        uint32_t dword01;                             // The first dword
    };

    union
    {
        uint32_t deviceIdLow;   // The 32-bit low-word of device ID (VkDeviceHandle)
        uint32_t dword02;       // The second dword
    };

    union
    {
        uint32_t deviceIdHigh;   // The 32-bit high-word of device ID (VkDeviceHandle)
        uint32_t dword03;        // The third dword
    };

    union
    {
        uint32_t queueFlags; // VkQueueFlags value for the given queue family index
        uint32_t dword04;    // The first extended dword
    };
};

constexpr uint32_t RgpSqttMarkerCbStartWordCount = 4;

// RgpSqttMarkerCbEnd - RGP SQ thread-tracing marker for the end of a command buffer. (Table 3)
struct RgpSqttMarkerCbEnd
{
    union
    {
        struct
        {
            uint32_t identifier  : 4;  // Identifier for this marker
            uint32_t extDwords   : 3;  // Number of extra dwords following this marker
            uint32_t cbID        : 20; // Command buffer ID for this marker
            uint32_t reserved    : 5;  // Unused bits
        };

        uint32_t dword01;              // The first dword
    };

    union
    {
        uint32_t deviceIdLow;          // The 32-bit low-word of device ID (VkDeviceHandle)
        uint32_t dword02;              // The second dword
    };

    union
    {
        uint32_t deviceIdHigh;          // The 32-bit high-word of device ID (VkDeviceHandle)
        uint32_t dword03;               // The third dword
    };
};

constexpr uint32_t RgpSqttMarkerCbEndWordCount = 3;

// RgpSqttMarkerApiType - API types used in RGP SQ thread-tracing markers (Table 16).
//
// Note: Unless explicitly stated by the comment, these do not have a 1:1 relationship with a specific Vulkan API
// function.  Many of these reasons (e.g. clears in particular) may be caused multiple different API functions that
// all map to the same internal clear function.  Therefore, it may be misleading to describe for example all
// events labelled RgpSqttMarkerEventClearColor as for example "vkCmdClearColorImage(...)" because the same event
// may be triggered also by render pass load-op clears.
// Note: to keep backward compatible for RGP, new enum values in this definition must be added to the end and
// existing values can't be changed.
enum class RgpSqttMarkerEventType : uint32_t
{
    CmdDraw                                     = 0,        // vkCmdDraw
    CmdDrawIndexed                              = 1,        // vkCmdDrawIndexed
    CmdDrawIndirect                             = 2,        // vkCmdDrawIndirect
    CmdDrawIndexedIndirect                      = 3,        // vkCmdDrawIndexedIndirect
    CmdDrawIndirectCountAMD                     = 4,        // vkCmdDrawIndirectCountAMD
    CmdDrawIndexedIndirectCountAMD              = 5,        // vkCmdDrawIndexedIndirectCountAMD
    CmdDispatch                                 = 6,        // vkCmdDispatch
    CmdDispatchIndirect                         = 7,        // vkCmdDispatchIndirect
    CmdCopyBuffer                               = 8,        // vkCmdCopyBuffer
    CmdCopyImage                                = 9,        // vkCmdCopyImage
    CmdBlitImage                                = 10,       // vkCmdBlitImage
    CmdCopyBufferToImage                        = 11,       // vkCmdCopyBufferToImage
    CmdCopyImageToBuffer                        = 12,       // vkCmdCopyImageToBuffer
    CmdUpdateBuffer                             = 13,       // vkCmdUpdateBuffer
    CmdFillBuffer                               = 14,       // vkCmdFillBuffer
    CmdClearColorImage                          = 15,       // vkCmdClearColorImage
    CmdClearDepthStencilImage                   = 16,       // vkCmdClearDepthStencilImage
    CmdClearAttachments                         = 17,       // vkCmdClearAttachments
    CmdResolveImage                             = 18,       // vkCmdResolveImage
    CmdWaitEvents                               = 19,       // vkCmdWaitEvents
    CmdPipelineBarrier                          = 20,       // vkCmdPipelineBarrier
    CmdResetQueryPool                           = 21,       // vkCmdResetQueryPool
    CmdCopyQueryPoolResults                     = 22,       // vkCmdCopyQueryPoolResults
    RenderPassColorClear                        = 23,       // Render pass: Color clear triggered by attachment load op
    RenderPassDepthStencilClear                 = 24,       // Render pass: Depth-stencil clear triggered by attachment load op
    RenderPassResolve                           = 25,       // Render pass: Color multisample resolve triggered by resolve attachment
    InternalUnknown                             = 26,       // Draw or dispatch by PAL due to a reason we do not know
    CmdDrawIndirectCountKHR                     = 27,       // vkCmdDrawIndirectCountKHR
    CmdDrawIndexedIndirectCountKHR              = 28,       // vkCmdDrawIndexedIndirectCountKHR
#if VKI_RAY_TRACING
    CmdTraceRaysKHR                             = 30,       // vkCmdTraceRaysKHR
    CmdTraceRaysIndirectKHR                     = 31,       // vkCmdTraceRaysIndirectKHR
    CmdBuildAccelerationStructuresKHR           = 32,       // vkCmdBuildAccelerationStructuresKHR
    CmdBuildAccelerationStructuresIndirectKHR   = 33,       // vkCmdBuildAccelerationStructuresIndirectKHR
    CmdCopyAccelerationStructureKHR             = 34,       // vkCmdCopyAccelerationStructureKHR
    CmdCopyAccelerationStructureToMemoryKHR     = 35,       // vkCmdCopyAccelerationStructureToMemoryKHR
    CmdCopyMemoryToAccelerationStructureKHR     = 36,       // vkCmdCopyMemoryToAccelerationStructureKHR
#endif
    CmdDrawMeshTasksEXT                         = 41,       // vkCmdDrawMeshTasksEXT
    CmdDrawMeshTasksIndirectCountEXT            = 42,       // vkCmdDrawMeshTasksIndirectCountEXT
    CmdDrawMeshTasksIndirectEXT                 = 43,       // vkCmdDrawMeshTasksIndirectEXT
#if VKI_RAY_TRACING
    ShaderIndirectModeMask                      = 0x800000, // Used to mark whether the shader is compiled in indirect mode or not
                                                            // This mask can only be used with CmdTraceRaysKHR and CmdTraceRaysIndirectKHR
#endif
    CmdUnknown                                  = 0x7fff,
    Invalid                                     = 0xffffffff
};

inline RgpSqttMarkerEventType& operator |= (RgpSqttMarkerEventType& lhs, RgpSqttMarkerEventType rhs)
{
    return lhs = static_cast<RgpSqttMarkerEventType>(
        static_cast<std::underlying_type<RgpSqttMarkerEventType>::type>(lhs) |
        static_cast<std::underlying_type<RgpSqttMarkerEventType>::type>(rhs)
    );
}

// RgpSqttMarkerEvent - "Event (Per-draw/dispatch)" RGP SQ thread-tracing marker.  These are generated ahead of
// draws or dispatches for commands that trigger generation of waves i.e. draws/dispatches (Table 4).
struct RgpSqttMarkerEvent
{
    union
    {
        struct
        {
            uint32_t identifier    : 4;  // Identifier for this marker
            uint32_t extDwords     : 3;  // Number of extra dwords following this marker
            uint32_t apiType       : 24; // The API type for this command
            uint32_t hasThreadDims : 1;  // Whether thread dimensions are included
        };

        uint32_t     dword01;            // The first dword
    };

    union
    {
        // Some information about the vertex/instance/draw register indices.  These values are not always valid because
        // they are not available for one reason or another:
        //
        // - If vertex offset index or instance offset index are not (together) valid, they are both equal to 0
        // - If draw index is not valid, it is equal to the vertex offset index
        struct
        {
            uint32_t cbID                 : 20; // Command buffer ID for this marker
            uint32_t vertexOffsetRegIdx   : 4;  // SPI userdata register index for the first vertex offset
            uint32_t instanceOffsetRegIdx : 4;  // SPI userdata register index for the first instance offset
            uint32_t drawIndexRegIdx      : 4;  // SPI userdata register index for the draw index (multi draw indirect)
        };
        uint32_t     dword02; // The second dword
    };

    union
    {
        uint32_t cmdID;      // Command index within the command buffer
        uint32_t dword03;    // The third dword
    };
};

constexpr uint32_t RgpSqttMarkerEventWordCount = 3;

// RgpSqttMarkerEventWithDims - Per-dispatch specific marker where workgroup dims are included
struct RgpSqttMarkerEventWithDims
{
    RgpSqttMarkerEvent event;   // Per-draw/dispatch marker.  API type should be Dispatch, threadDim = 1
    uint32_t           threadX; // Work group count in X
    uint32_t           threadY; // Work group count in Y
    uint32_t           threadZ; // Work group count in Z
};

constexpr uint32_t RgpSqttMarkerEventWithDimsWordCount = RgpSqttMarkerEventWordCount + 3;

// RgpSqttMarkerBarrierStart - "Barrier Start" RGP SQTT instrumentation marker (Table 5)
struct RgpSqttMarkerBarrierStart
{
    union
    {
        struct
        {
            uint32_t identifier    : 4;  // Identifier for this marker
            uint32_t extDwords     : 3;  // Number of extra dwords following this marker
            uint32_t cbId          : 20; // Command buffer ID within queue
            uint32_t reserved      : 5;  // Reserved
        };

        uint32_t     dword01;            // The first dword
    };

    union
    {
        struct
        {
            uint32_t driverReason : 31;
            uint32_t internal     : 1;
        };

        uint32_t     dword02;            // The second dword
    };
};

constexpr uint32_t RgpSqttMarkerBarrierStartWordCount = 2;

// RgpSqttMarkerBarrierEnd - "Barrier End" RGP SQTT instrumentation marker (Table 6)
struct RgpSqttMarkerBarrierEnd
{
    union
    {
        struct
        {
            uint32_t identifier     : 4;  // Identifier for this marker
            uint32_t extDwords      : 3;  // Number of extra dwords following this marker
            uint32_t cbId           : 20; // Command buffer ID within queue
            uint32_t waitOnEopTs    : 1;  // Issued EOP_TS VGT event followed by a WAIT_REG_MEM for that timestamp
                                          // to be written.  Quintessential full pipeline stall.
            uint32_t vsPartialFlush : 1;  // Stall at ME waiting for all prior VS waves to complete.
            uint32_t psPartialFlush : 1;  // Stall at ME waiting for all prior PS waves to complete.
            uint32_t csPartialFlush : 1;  // Stall at ME waiting for all prior CS waves to complete.
            uint32_t pfpSyncMe      : 1;  // Stall PFP until ME is at same point in command stream.
        };

        uint32_t     dword01;             // The first dword
    };

    union
    {
        struct
        {
            uint32_t syncCpDma            : 1;  // Issue dummy CP-DMA command to confirm all prior CP-DMAs have completed.
            uint32_t invalTcp             : 1;  // Invalidate the TCP vector caches.
            uint32_t invalSqI             : 1;  // Invalidate the SQ instruction caches
            uint32_t invalSqK             : 1;  // Invalidate the SQ constant caches.
            uint32_t flushTcc             : 1;  // Flush L2.
            uint32_t invalTcc             : 1;  // Flush L2.
            uint32_t flushCb              : 1;  // Flush CB caches (including DCC, cmask, fmask)
            uint32_t invalCb              : 1;  // Invalidate CB caches (including DCC, cmask, fmask)
            uint32_t flushDb              : 1;  // Flush DB caches (including htile)
            uint32_t invalDb              : 1;  // Invalidate DB caches (including htile)
            uint32_t numLayoutTransitions : 16; // Number of layout transitions following this packet
            uint32_t invalGl1             : 1;  // Invalidate L1.
            uint32_t waitOnTs             : 1;  // Wait on a timestamp event (EOP or EOS) at the ME.
            uint32_t eopTsBottomOfPipe    : 1;  // Barrier issued an end-of-pipe event that can be waited on
            uint32_t eosTsPsDone          : 1;  // Timestamp when PS waves are done.
            uint32_t eosTsCsDone          : 1;  // Timestamp when CS waves are done.
            uint32_t reserved             : 1;  // Reserved for future expansion. Always 0
        };

        uint32_t  dword02;                // The second dword
    };
};

constexpr uint32_t RgpSqttMarkerBarrierEndWordCount = 2;

// RgpSqttMarkerLayoutTransition - "Layout Transition" RGP SQTT instrumentation marker (Table 7)
struct RgpSqttMarkerLayoutTransition
{
    union
    {
        struct
        {
            uint32_t identifier              : 4;    // Identifier for this marker
            uint32_t extDwords               : 3;    // Number of extra dwords following this marker
            uint32_t depthStencilExpand      : 1;    // Depth/stencil decompress blt
            uint32_t htileHiZRangeExpand     : 1;    // Htile is updated for the specified depth/stencil resource so
                                                     // that the Hi-Z range is programmed to maximum.  This is a much
                                                     // faster operation than depthStencilResummarize.
            uint32_t depthStencilResummarize : 1;    // Htile is updated to reflect the new pixel contents of a
                                                     // depth/stencil resource that was modified with shader writes,
                                                     // copies, etc.  This produces better quality data (better
                                                     // compression) than htileHiZRangeExpand.
            uint32_t dccDecompress           : 1;    // DCC decompress blt for color images.
            uint32_t fmaskDecompress         : 1;    // Decompress fmask to make it shader-readable.
            uint32_t fastClearEliminate      : 1;    // Expand latest specified clear color into pixel data for the
                                                     // fast cleared color/depth resource.
            uint32_t fmaskColorExpand        : 1;    // Completely decompresses the specified color resource so that
                                                     // the color value in each sample is fully expanded and directly
                                                     // shader readable.
            uint32_t initMaskRam             : 1;    // Memsets uninitialized memory to prepare it for use as cmask/
                                                     // fmask/DCC/htile.
            uint32_t reserved1               : 17;   // Reserved for future use.  Always 0.
        };

        uint32_t     dword01;                        // The first dword
    };

    union
    {
        struct
        {
            uint32_t reserved2               : 32;   // Reserved for future use.  Always 0.
        };

        uint32_t  dword02;                           // The second dword
    };
};

constexpr uint32_t RgpSqttMarkerLayoutTransitionWordCount = 2;

// RgpSqttMarkeUserEventDataType - Data types used in RGP SQ thread-tracing markers for an user event
enum RgpSqttMarkerUserEventType : uint32_t
{
    RgpSqttMarkerUserEventTrigger      = 0x0,
    RgpSqttMarkerUserEventPop          = 0x1,
    RgpSqttMarkerUserEventPush         = 0x2,
    RgpSqttMarkerUserEventReserved0    = 0x3,
    RgpSqttMarkerUserEventReserved1    = 0x4,
    RgpSqttMarkerUserEventReserved2    = 0x5,
    RgpSqttMarkerUserEventReserved3    = 0x6,
    RgpSqttMarkerUserEventReserved4    = 0x7,
};

// RgpSqttMarkerUserEvent - RGP SQ thread-tracing marker for an user event.
union RgpSqttMarkerUserEvent
{
    struct
    {
        uint32_t identifier : 4;  // Identifier for this marker
        uint32_t extDwords  : 8;  // Number of extra dwords following this marker
        uint32_t dataType   : 8;  // The type for this marker
        uint32_t reserved   : 12; // reserved
    };

    uint32_t dword01;                               // The first dword
};

constexpr uint32_t RgpSqttMarkerUserEventWordCount = 1;

// The max lengths of frame marker strings
static constexpr size_t RgpSqttMaxUserEventStringLengthInDwords = 1024;

// RgpSqttMarkerUserEvent - RGP SQ thread-tracing marker for an user event with a string (push and trigger data types)
struct RgpSqttMarkerUserEventWithString
{
    RgpSqttMarkerUserEvent header;

    uint32_t stringLength;                                        // Length of the string (in characters)
    uint32_t stringData[RgpSqttMaxUserEventStringLengthInDwords]; // String data in UTF-8 format
};

// RgpSqttMarkerGeneralApiType - API types used in RGP SQ thread-tracing markers for the "General API" packet
//
// Unless otherwise stated in the comment, it can be assumed that the name of the entry point that each
// enum label maps to is "vk" appended by the enum label.
enum class RgpSqttMarkerGeneralApiType : uint32_t
{
    // Interesting subset of core Vulkan 1.0:
    CmdBindPipeline                     = 0,
    CmdBindDescriptorSets               = 1,
    CmdBindIndexBuffer                  = 2,
    CmdBindVertexBuffers                = 3,
    CmdDraw                             = 4,
    CmdDrawIndexed                      = 5,
    CmdDrawIndirect                     = 6,
    CmdDrawIndexedIndirect              = 7,
    CmdDrawIndirectCountAMD             = 8,
    CmdDrawIndexedIndirectCountAMD      = 9,
    CmdDispatch                         = 10,
    CmdDispatchIndirect                 = 11,
    CmdCopyBuffer                       = 12,
    CmdCopyImage                        = 13,
    CmdBlitImage                        = 14,
    CmdCopyBufferToImage                = 15,
    CmdCopyImageToBuffer                = 16,
    CmdUpdateBuffer                     = 17,
    CmdFillBuffer                       = 18,
    CmdClearColorImage                  = 19,
    CmdClearDepthStencilImage           = 20,
    CmdClearAttachments                 = 21,
    CmdResolveImage                     = 22,
    CmdWaitEvents                       = 23,
    CmdPipelineBarrier                  = 24,
    CmdBeginQuery                       = 25,
    CmdEndQuery                         = 26,
    CmdResetQueryPool                   = 27,
    CmdWriteTimestamp                   = 28,
    CmdCopyQueryPoolResults             = 29,
    CmdPushConstants                    = 30,
    CmdBeginRenderPass                  = 31,
    CmdNextSubpass                      = 32,
    CmdEndRenderPass                    = 33,
    CmdExecuteCommands                  = 34,
    CmdSetViewport                      = 35,
    CmdSetScissor                       = 36,
    CmdSetLineWidth                     = 37,
    CmdSetDepthBias                     = 38,
    CmdSetBlendConstants                = 39,
    CmdSetDepthBounds                   = 40,
    CmdSetStencilCompareMask            = 41,
    CmdSetStencilWriteMask              = 42,
    CmdSetStencilReference              = 43,
    CmdDrawIndirectCountKHR             = 44,
    CmdDrawIndexedIndirectCountKHR      = 45,
    CmdDrawMeshTasksEXT                 = 47,
    CmdDrawMeshTasksIndirectCountEXT    = 48,
    CmdDrawMeshTasksIndirectEXT         = 49,

    Invalid = 0xffffffff
};

// RgpSqttMarkerGeneralApi - RGP SQ thread-tracing marker for a "General API" instrumentation packet
union RgpSqttMarkerGeneralApi
{
    struct
    {
        uint32_t identifier : 4;  // Identifier for this marker
        uint32_t extDwords  : 3;  // Number of extra dwords following this marker
        uint32_t apiType    : 20; // Value of RgpSqttMarkerGeneralApiType for this function
        uint32_t isEnd      : 1;  // Bit set to denote the end of a sequence of register writes
        uint32_t reserved   : 4;  // Reserved
    };

    uint32_t dword01;             // The first dword
};

constexpr uint32_t RgpSqttMarkerGeneralApiWordCount = 1;

// RgpSqttMarkerPresent - RGP SQ thread-tracing marker for a "Present" instrumentation packet
union RgpSqttMarkerPresent
{
    struct
    {
        uint32_t identifier  : 4;  // Identifier for this marker
        uint32_t extDwords   : 3;  // Number of extra dwords following this marker
        uint32_t bufferIndex : 4;  // Swap chain image index
        uint32_t reserved    : 21; // Reserved
    };

    uint32_t dword01;              // The first dword
};

constexpr uint32_t RgpSqttMarkerPresentWordCount = 1;

// RgpSqttMarkerPipelineBind - RGP SQ thread-tracing marker written whenever a pipeline is bound (Table 12).
struct RgpSqttMarkerPipelineBind
{
    union
    {
        struct
        {
            uint32_t identifier : 4;  // Identifier for this marker
            uint32_t extDwords  : 3;  // Number of extra dwords following this marker
            uint32_t bindPoint  : 1;  // The bind point of the pipeline within a queue
                                      // 0 = graphics bind point
                                      // 1 = compute bind point
            uint32_t cbID       : 20; // A command buffer ID encoded as per Table 13.
            uint32_t reserved   : 4;  // Reserved
        };

        uint32_t     dword01;         // The first dword
    };

    union
    {
        uint32_t apiPsoHash[2];       // The API PSO hash of the pipeline being bound

        struct
        {
            uint32_t dword02;         // The second dword
            uint32_t dword03;         // The third dword
        };
    };
};

constexpr uint32_t RgpSqttMarkerPipelineBindWordCount = 3;

// Table 15: RgpSqttBarrierReason - Value for the reason field of an RGP barrier start marker originating from the
// Vulkan client (does not include PAL-defined values).
//
// *!* Changes to this enum list must be matched by editing the official RGP SQTT Instrumentation Specification as *!*
// *!* well as a bump to RgpSqttInstrumentationApiVersion (a generic "unknown" enum value is provided that can     *!*
// *!* be used temporarily).                                                                                       *!*
enum RgpBarrierReason : uint32_t
{
    // Generic "Unknown" reason.  Use this temporarily if you need to add a new Pal::CmdBarrier() call in the driver
    // but do not have time to update the RGP SQTT spec.  Please do not abuse.
    RgpBarrierUnknownReason                       = 0xFFFFFFFF,

    // External app-generated barrier reasons, i.e. API synchronization commands
    // Range of valid values: [0x00000001 ... 0x7FFFFFFF]
    RgpBarrierExternalCmdPipelineBarrier          = 0x00000001,  // vkCmdPipelineBarrier
    RgpBarrierExternalRenderPassSync              = 0x00000002,  // Renderpass subpass-related synchronization
    RgpBarrierExternalCmdWaitEvents               = 0x00000003,  // vkCmdWaitEvents

    // Internal barrier reasons, i.e. implicit synchronization inserted by the Vulkan driver
    // Range of valid values: [0xC0000000 ... 0xFFFFFFFE]
    RgpBarrierInternalBase                        = 0xC0000000,
    RgpBarrierInternalPreResetQueryPoolSync       = RgpBarrierInternalBase + 0,
    RgpBarrierInternalPostResetQueryPoolSync      = RgpBarrierInternalBase + 1,
    RgpBarrierInternalGpuEventRecycleStall        = RgpBarrierInternalBase + 2,
    RgpBarrierInternalPreCopyQueryPoolResultsSync = RgpBarrierInternalBase + 3,
    RgpBarrierInternalInstructionTraceStall       = RgpBarrierInternalBase + 4
#if VKI_RAY_TRACING
   ,RgpBarrierInternalRayTracingSync              = RgpBarrierInternalBase + 5
#endif
};

};

#endif
