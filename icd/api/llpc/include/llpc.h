/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2016-2017 Advanced Micro Devices, Inc. All Rights Reserved.
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
 * @file  llpc.h
 * @brief LLPC header file: contains LLPC basic definitions (including interfaces and data types).
 ***********************************************************************************************************************
 */
#pragma once

#include "vulkan.h"

// Confliction of Xlib and LLVM headers
#undef True
#undef False
#undef DestroyAll

namespace Llpc
{

static const uint32_t  Version = 3;
static const uint32_t  MaxColorTargets = 8;
static const char      VkIcdName[]     = "amdvlk";

// Forward declarations
class IShaderCache;

/// Enumerates result codes of LLPC operations.
enum class Result : int32_t
{
    /// The operation completed successfully
    Success                         = 0x00000000,
    // The requested operation is delayed
    Delayed                         = 0x00000001,
    // The requested feature is unsupported
    Unsupported                     = 0x00000002,
    /// The requested operation is unavailable at this time
    ErrorUnavailable                = -(0x00000001),
    /// The operation could not complete due to insufficient system memory
    ErrorOutOfMemory                = -(0x00000002),
    /// An invalid shader code was passed to the call
    ErrorInvalidShader               = -(0x00000003),
    /// An invalid value was passed to the call
    ErrorInvalidValue               = -(0x00000004),
    /// A required input pointer passed to the call was invalid (probably null)
    ErrorInvalidPointer             = -(0x00000005),
    /// The operaton encountered an unknown error
    ErrorUnknown                    = -(0x00000006),
};

/// Enumerates LLPC shader stages.
enum ShaderStage : uint32_t
{
    ShaderStageVertex = 0,                          ///< Vertex shader
    ShaderStageTessControl,                         ///< Tessellation control shader
    ShaderStageTessEval,                            ///< Tessellation evaluation shader
    ShaderStageGeometry,                            ///< Geometry shader
    ShaderStageFragment,                            ///< Fragment shader
    ShaderStageCompute,                             ///< Compute shader

    ShaderStageCount = ShaderStageCompute + 1,      ///< Count of shader stages
    ShaderStageGfxCount = ShaderStageFragment + 1,  ///< Count of shader stages for graphics pipeline

    ShaderStageCopyShader = ShaderStageCompute + 1, ///< Copy shader (internal-use)
    ShaderStageCountInternal = ShaderStageCopyShader + 1, ///< Count of shader stages (internal-use)

    ShaderStageInvalid  = ShaderStageCountInternal, ///< Invalid shader stage
};

/// Enumerates the function of a particular node in a shader's resource mapping graph.
enum class ResourceMappingNodeType : uint32_t
{
    Unknown,                        ///< Invalid type
    DescriptorResource,             ///< Generic descriptor: resource, including texture resource, image, input
                                    ///  attachment
    DescriptorSampler,              ///< Generic descriptor: sampler
    DescriptorCombinedTexture,      ///< Generic descriptor: combined texture, combining resource descriptor with
                                    ///  sampler descriptor of the same texture, starting with resource descriptor
    DescriptorTexelBuffer,          ///< Generic descriptor: texel buffer, including texture buffer and image buffer
    DescriptorFmask,                ///< Generic descriptor: F-mask
    DescriptorBuffer,               ///< Generic descriptor: buffer, including uniform buffer and shader storage buffer
    DescriptorTableVaPtr,           ///< Descriptor table VA pointer
    IndirectUserDataVaPtr,          ///< Indirect user data VA pointer
    PushConst,                      ///< Push constant
    DescriptorBufferCompact,        ///< Compact buffer descriptor, only contains the buffer address
    Count,                          ///< Count of resource mapping node types.
};

/// Represents graphics IP version info. See http://confluence.amd.com/display/ASLC/AMDGPU+Target+Names  for more
/// details.
struct GfxIpVersion
{
    uint32_t        major;              ///< Major version
    uint32_t        minor;              ///< Minor version
    uint32_t        stepping;           ///< Stepping info
};

/// Represents shader binary data.
struct BinaryData
{
    size_t          codeSize;           ///< Size of shader binary data
    const void*     pCode;              ///< Shader binary data
};

/// Prototype of allocator for output data buffer, used in shader-specific operations.
typedef void* (VKAPI_CALL *OutputAllocFunc)(void* pInstance, void* pUserData, size_t size);

/// Represents info to build a shader module.
struct ShaderModuleBuildInfo
{
    void*                pInstance;         ///< Vulkan instance object
    void*                pUserData;         ///< User data
    OutputAllocFunc      pfnOutputAlloc;    ///< Output buffer allocator
    BinaryData           shaderBin;         ///< Shader binary data (SPIR-V binary)
};

/// Represents output of building a shader module.
struct ShaderModuleBuildOut
{
    void*                pModuleData;       ///< Output shader module data (opaque)
};

/// Represents one node in a graph defining how the user data bound in a command buffer at draw/dispatch time maps to
/// resources referenced by a shader (t#, u#, etc.).
struct ResourceMappingNode
{
    ResourceMappingNodeType     type;   ///< Type of this node

    uint32_t    sizeInDwords;   ///< Size of this node in DWORD
    uint32_t    offsetInDwords; ///< Offset of this node (from the beginning of the resource mapping table) in DWORD

    union
    {
        /// Info for generic descriptor nodes (DescriptorResource, DescriptorSampler, DescriptorCombinedTexture,
        /// DescriptorTexelBuffer, DescriptorBuffer and DescriptorBufferCompact)
        struct
        {
            uint32_t                    set;         ///< Descriptor set
            uint32_t                    binding;     ///< Descriptor binding
        } srdRange;
        /// Info for hierarchical nodes (DescriptorTableVaPtr)
        struct
        {
            uint32_t                    nodeCount;  ///< Number of entries in the "pNext" array
            const ResourceMappingNode*  pNext;      ///< Array of node structures describing the next hierarchical
                                                    ///  level of mapping
        } tablePtr;
        /// Info for hierarchical nodes (IndirectUserDataVaPtr)
        struct
        {
            uint32_t                    sizeInDwords; ///< Size of the pointed table in DWORDS
        } userDataPtr;
    };
};

/// Represents the info of static descriptor.
struct DescriptorRangeValue
{
    ResourceMappingNodeType type;       ///< Type of this resource mapping node (currently, only sampler is supported)
    uint32_t                set;        ///< ID of descriptor set
    uint32_t                binding;    ///< ID of descriptor binding
    uint32_t                arraySize;  ///< Element count for arrayed binding
    const uint32_t*         pValue;     ///< Static SRDs
};

/// Represents info of a shader attached to a to-be-built pipeline.
struct PipelineShaderInfo
{
    const void*                     pModuleData;            ///< Shader module data used for pipeline building (opaque)
    const VkSpecializationInfo*     pSpecializatonInfo;     ///< Specialization constant info
    const char*                     pEntryTarget;           ///< Name of the target entry point (for multi-entry)

    uint32_t                        descriptorRangeValueCount; ///< Count of static descriptors
    DescriptorRangeValue*           pDescriptorRangeValues;    ///< An array of static descriptors

    uint32_t                        userDataNodeCount;      ///< Count of user data nodes

    /// User data nodes, providing the root-level mapping of descriptors in user-data entries (physical registers or
    /// GPU memory) to resources referenced in this pipeline shader.
    /// NOTE: Normally, this user data will correspond to the GPU's user data registers. However, Compiler needs some
    /// user data registers for internal use, so some user data may spill to internal GPU memory managed by Compiler.
    const ResourceMappingNode*      pUserDataNodes;
};

/// Represents output of building a graphics pipeline.
struct GraphicsPipelineBuildOut
{
    BinaryData          pipelineBin;        ///< Output pipeline binary data
};

/// Represents info to build a graphics pipeline.
struct GraphicsPipelineBuildInfo
{
    void*               pInstance;          ///< Vulkan instance object
    void*               pUserData;          ///< User data
    OutputAllocFunc     pfnOutputAlloc;     ///< Output buffer allocator
    IShaderCache*       pShaderCache;       ///< Shader cache, used to search for the compiled shader data
    PipelineShaderInfo  vs;                 ///< Vertex shader
    PipelineShaderInfo  tcs;                ///< Tessellation control shader
    PipelineShaderInfo  tes;                ///< Tessellation evaluation shader
    PipelineShaderInfo  gs;                 ///< Geometry shader
    PipelineShaderInfo  fs;                 ///< Fragment shader

    /// Create info of vertex input state
    const VkPipelineVertexInputStateCreateInfo*     pVertexInput;

    struct
    {
        VkPrimitiveTopology  topology;           ///< Primitive topology
        uint32_t             patchControlPoints; ///< Number of control points per patch (valid when the topology is
                                                 ///  "patch")
        uint32_t             deviceIndex;        ///< Device index for device group
        bool                 disableVertexReuse; ///< Disable reusing vertex shader output for indexed draws
    } iaState;                                   ///< Input-assembly state

    struct
    {
        bool        depthClipEnable;    ///< Enable clipping based on Z coordinate
    } vpState;                          ///< Viewport state

    struct
    {
        bool    rasterizerDiscardEnable;    ///< Kill all rasterized pixels. This is implicitly true if stream out
                                            ///  is enabled and no streams are rasterized
        bool    perSampleShading;           ///< Enable per sample shading
        uint32_t  numSamples;               ///< Number of coverage samples used when rendering with this pipeline.
        uint32_t  samplePatternIdx;         ///< Index into the currently bound MSAA sample pattern table that
                                            ///  matches the sample pattern used by the rasterizer when rendering
                                            ///  with this pipeline.
        uint8_t   usrClipPlaneMask;         ///< Mask to indicate the enabled user defined clip planes
    } rsState;                              ///< Rasterizer State

    struct
    {
        bool    alphaToCoverageEnable;          ///< Enable alpha to coverage
        bool    dualSourceBlendEnable;          ///< Blend state bound at draw time will use a dual source blend mode
        struct
        {
            bool          blendEnable;          ///< Blend will be enabled for this target at draw time
            bool          blendSrcAlphaToColor; ///< Whether source alpha is blended to color channels for this target
                                                ///  at draw time
           VkFormat       format;               ///< Color attachment format
        } target[MaxColorTargets];              ///< Per-MRT color target info
    } cbState;                                  ///< Color target state
};

/// Represents info to build a compute pipeline.
struct ComputePipelineBuildInfo
{
    void*               pInstance;          ///< Vulkan instance object
    void*               pUserData;          ///< User data
    OutputAllocFunc     pfnOutputAlloc;     ///< Output buffer allocator
    IShaderCache*       pShaderCache;       ///< Shader cache, used to search for the compiled shader data
    uint32_t            deviceIndex;        ///< Device index for device group
    PipelineShaderInfo  cs;                 ///< Compute shader
};

/// Represents output of building a compute pipeline.
struct ComputePipelineBuildOut
{
    BinaryData          pipelineBin;        ///< Output pipeline binary data
};

typedef uint64_t ShaderHash;

/// Defines callback function used to lookup shader cache info in an external cache
typedef Result (*ShaderCacheGetValue)(const void* pClientData, ShaderHash hash, void* pValue, size_t* pValueLen);

/// Defines callback function used to store shader cache info in an external cache
typedef Result (*ShaderCacheStoreValue)(const void* pClientData, ShaderHash hash, const void* pValue, size_t valueLen);

/// Specifies all information necessary to create a shader cache object.
struct ShaderCacheCreateInfo
{
    const void*  pInitialData;      ///< Pointer to a data buffer whose contents should be used to seed the shader
                                    ///  cache. This may be null if no initial data is present.
    size_t       initialDataSize;   ///< Size of the initial data buffer, in bytes.

    // NOTE: The following parameters are all optional, and are only used when the IShaderCache will be used in
    // tandem with an external cache which serves as a backing store for the cached shader data.

    // [optional] Private client-opaque data which will be passed to the pClientData parameters of the Get and
    // Store callback functions.
    const void*            pClientData;
    ShaderCacheGetValue    pfnGetValueFunc;    ///< [Optional] Function to lookup shader cache data in an external cache
    ShaderCacheStoreValue  pfnStoreValueFunc;  ///< [Optional] Function to store shader cache data in an external cache
};

// =====================================================================================================================
/// Represents the interface of a cache for compiled shaders. The shader cache is designed to be optionally passed in at
/// pipeline create time. The compiled binary for the shaders is stored in the cache object to avoid compiling the same
/// shader multiple times. The shader cache also provides a method to serialize its data to be stored to disk.
class IShaderCache
{
public:
    /// Serializes the shader cache data or queries the size required for serialization.
    ///
    /// @param [in]      pBlob  System memory pointer where the serialized data should be placed. This parameter can
    ///                         be null when querying the size of the serialized data. When non-null (and the size is
    ///                         correct/sufficient) then the contents of the shader cache will be placed in this
    ///                         location. The data is an opaque blob which is not intended to be parsed by clients.
    /// @param [in,out]  pSize  Size of the memory pointed to by pBlob. If the value stored in pSize is zero then no
    ///                         data will be copied and instead the size required for serialization will be returned
    ///                         in pSize.
    ///
    /// @returns Success if data was serialized successfully, Unknown if fail to do serialize.
    virtual Result Serialize(
        void*   pBlob,
        size_t* pSize) = 0;

    /// Merges the provided source shader caches' content into this shader cache.
    ///
    /// @param [in]  srcCacheCount  Count of source shader caches to be merged.
    /// @param [in]  ppSrcCaches    Pointer to an array of pointers to shader cache objects.
    ///
    /// @returns Success if data of source shader caches was merged successfully, OutOfMemory if the internal allocator
    ///          memory cannot be allocated.
    virtual Result Merge(
        uint32_t             srcCacheCount,
        const IShaderCache** ppSrcCaches) = 0;

    /// Frees all resources associated with this object.
    virtual void Destroy() = 0;

protected:
    /// @internal Constructor. Prevent use of new operator on this interface.
    IShaderCache() {}

    /// @internal Destructor. Prevent use of delete operator on this interface.
    virtual ~IShaderCache() {}
};

// =====================================================================================================================
/// Represents the interfaces of a pipeline compiler.
class ICompiler
{
public:
    /// Creates pipeline compiler from the specified info.
    ///
    /// @param [in]  optionCount    Count of compilation-option strings
    /// @param [in]  options        An array of compilation-option strings
    /// @param [out] ppCompiler     Pointer to the created pipeline compiler object
    ///
    /// @returns Result::Success if successful. Other return codes indicate failure.
    static Result VKAPI_CALL Create(GfxIpVersion      gfxIp,
                                    uint32_t          optionCount,
                                    const char*const* options,
                                    ICompiler**       ppCompiler);

    /// Checks whether a vertex attribute format is supported by fetch shader.
    ///
    /// @parame [in] format  Vertex attribute format
    ///
    /// @return TRUE if the specified format is supported by fetch shader. Otherwise, FALSE is returned.
    static bool VKAPI_CALL IsVertexFormatSupported(VkFormat format);

    /// Destroys the pipeline compiler.
    virtual void VKAPI_CALL Destroy() = 0;

    /// Build shader module from the specified info.
    ///
    /// @param [in]  pShaderInfo    Info to build this shader module
    /// @param [out] pShaderOut     Output of building this shader module
    ///
    /// @returns Result::Success if successful. Other return codes indicate failure.
    virtual Result BuildShaderModule(const ShaderModuleBuildInfo* pShaderInfo,
                                    ShaderModuleBuildOut*        pShaderOut) const = 0;

    /// Build graphics pipeline from the specified info.
    ///
    /// @param [in]  pPipelineInfo  Info to build this graphics pipeline
    /// @param [out] pPipelineOut   Output of building this graphics pipeline
    ///
    /// @returns Result::Success if successful. Other return codes indicate failure.
    virtual Result BuildGraphicsPipeline(const GraphicsPipelineBuildInfo* pPipelineInfo,
                                         GraphicsPipelineBuildOut*        pPipelineOut) = 0;

    /// Build compute pipeline from the specified info.
    ///
    /// @param [in]  pPipelineInfo  Info to build this compute pipeline
    /// @param [out] pPipelineOut   Output of building this compute pipeline
    ///
    /// @returns Result::Success if successful. Other return codes indicate failure.
    virtual Result BuildComputePipeline(const ComputePipelineBuildInfo* pPipelineInfo,
                                        ComputePipelineBuildOut*        pPipelineOut) = 0;

    /// Calculates graphics pipeline hash code.
    ///
    /// @param [in]  pPipelineInfo  Info to build this graphics pipeline
    ///
    /// @returns Hash code associated this graphics pipeline.
    virtual uint64_t GetGraphicsPipelineHash(const GraphicsPipelineBuildInfo* pPipelineInfo) const = 0;

    /// Calculates compute pipeline hash code.
    ///
    /// @param [in]  pPipelineInfo  Info to build this compute pipeline
    ///
    /// @returns Hash code associated this compute pipeline.
    virtual uint64_t GetComputePipelineHash(const ComputePipelineBuildInfo* pPipelineInfo) const = 0;

    /// Creates a shader cache object with the requested properties.
    ///
    /// @param [in]  pCreateInfo    Create info of the shader cache.
    /// @param [out] ppShaderCache  Constructed shader cache object.
    ///
    /// @returns Success if the shader cache was successfully created. Otherwise, ErrorOutOfMemory is returned.
    virtual Result CreateShaderCache(
        const ShaderCacheCreateInfo* pCreateInfo,
        IShaderCache**               ppShaderCache) = 0;

    /// Dumps graphics pipeline.
    ///
    /// @param [in]  pPipelineInfo  Info to build this graphics pipeline
    virtual void DumpGraphicsPipeline(const GraphicsPipelineBuildInfo* pPipelineInfo) const = 0;

    /// Dumps compute pipeline.
    ///
    /// @param [in]  pPipelineInfo  Info to build this compute pipeline
    virtual void DumpComputePipeline(const ComputePipelineBuildInfo* pPipelineInfo) const = 0;

protected:
    ICompiler() {}
    /// Destructor
    virtual ~ICompiler() {}
};

} // Llpc
