##
 #######################################################################################################################
 #
 #  Copyright (c) 2020-2021 Advanced Micro Devices, Inc. All Rights Reserved.
 #
 #  Permission is hereby granted, free of charge, to any person obtaining a copy
 #  of this software and associated documentation files (the "Software"), to deal
 #  in the Software without restriction, including without limitation the rights
 #  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 #  copies of the Software, and to permit persons to whom the Software is
 #  furnished to do so, subject to the following conditions:
 #
 #  The above copyright notice and this permission notice shall be included in all
 #  copies or substantial portions of the Software.
 #
 #  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 #  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 #  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 #  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 #  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 #  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 #  SOFTWARE.
 #
 #######################################################################################################################

import os
import sys

genDir = ''
# if genDir was specified by the user
if len(sys.argv) == 3:
    genDir = sys.argv[2]

if genDir != "":
    CopyrightFilePath = genDir + "/xgl-copyright-template.txt"
    AppProfileHeaderFilePath = genDir + "/api/include/app_profile.h"
else:
    CopyrightFilePath = os.path.dirname(os.path.realpath(__file__)) + "/../xgl-copyright-template.txt"
    AppProfileHeaderFilePath = os.path.dirname(os.path.realpath(__file__)) + "/../../api/include/app_profile.h"

FileHeaderCopyright = open(CopyrightFilePath).read()

FileHeaderWarning = "\
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////\n\
//\n\
// WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!\n\
//\n\
// This code has been generated automatically. Do not hand-modify this code.\n\
//\n\
// When changes are needed, modify the tools generating this module in the tools\\generate directory\n\
//\n\
// WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING!  WARNING! WARNING!  WARNING!  WARNING!  WARNING!\n\
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////\n\
\n"

CopyrightAndWarning = FileHeaderCopyright + FileHeaderWarning

HeaderFileDoxComment = "\n\
/**\n\
************************************************************************************************************************\n\
* @file  %FileName%.h\n\
* @brief auto-generated file.\n\
*        Contains the definition for structs related to shader tuning and ShaderProfile class\n\
************************************************************************************************************************\n\
*/\n\
#pragma once\n"

HeaderIncludes = """
#include <sstream>
#include <iomanip>

#include \"include/app_profile.h\"
#include "include/vk_shader_code.h"
#include \"palDevice.h\"

#include \"utils/json_writer.h\"
#include \"palJsonWriter.h\"

#if ICD_RUNTIME_APP_PROFILE
#include \"utils/json_reader.h\"
#endif
"""

CppInclude = """
#include \"g_shader_profile.h\"

%Includes%

using namespace Util;
"""

InitializedVarTemplate   = """%DataType% %VarName% = %DefaultValue%;\n"""
UninitializedVarTemplate = """%DataType% %VarName%;\n"""
BitFieldVarTemplate      = """%DataType% %VarName% : %DefaultValue%;\n"""
InitializedArrTemplate   = """%DataType% %VarName%[%ArrSize%] = %ArrValue%;\n"""
UnInitializedArrTemplate = """%DataType% %VarName%[%ArrSize%];\n"""

ClassTemplate = """
class %ClassName%
{
public:
%ClassPublicDefs%

private:
%ClassPrivateDefs%
};
"""

StructTemplate = """
struct%StructName%
{
%StructDefs%
}%StructObj%;
"""

UnionTemplate = """\
union%UnionName%
{
%UnionDefs%
}%UnionObj%;
"""

NamespaceVK = """
namespace vk
{
%NamespaceDefs%
}
"""

NamespaceUtils = """
namespace utils
{
class JsonOutputStream;
}
"""

FuncDecClassShaderProfilePublic = """\
void PipelineProfileToJson(PipelineProfile pipelineProfile, const char* pFilePath);
"""

FuncDecSetAppProfile = """\
void SetAppProfile%FuncName%(PipelineProfile* pPipelineProfile);
"""

SetAppProfileFunc = """
void ShaderProfile::SetAppProfile%FuncName%(PipelineProfile* pPipelineProfile)
{
%FuncDefs%
}
"""

GenericGfxIpAppProfile = """
// The shader profile in this function will apply to all GfxIps and AsicRevisions in general
SetAppProfile%FuncName%(pPipelineProfile);
"""

FuncDecBuildAppProfileLlpc = """\
void BuildAppProfileLlpc(const AppProfile appProfile,
                         const Pal::GfxIpLevel gfxIpLevel,
                         PipelineProfile* pPipelineProfile);\n"""

BuildAppProfileLlpcFunc = """\
void ShaderProfile::BuildAppProfileLlpc(const AppProfile appProfile,
                                        const Pal::GfxIpLevel gfxIpLevel,
                                        PipelineProfile* pPipelineProfile)
{
%FuncDefs%
}
"""

FuncDecJsonWriter = """\
// Methods specific to json writer
std::string getShaderStageName(uint32_t i);

template <typename T>
std::string int_to_hex(T upper, T lower);

template <typename T>
std::string int_to_hex(T value);

void ProfileEntryActionToJson(PipelineProfileEntry entry, Util::JsonWriter* pWriter);
void ProfileEntryPatternToJson(PipelineProfilePattern pattern, Util::JsonWriter* pWriter);
"""

ProfileEntryActionToJsonFunc = """\
void ShaderProfile::ProfileEntryActionToJson(PipelineProfileEntry entry, Util::JsonWriter* pWriter)
{
    PipelineProfilePattern pattern = entry.pattern;
    PipelineProfileAction  action  = entry.action;
    pWriter->BeginMap(false);

%CreateInfoApply%

    uint32_t i = 0;
    for (auto shader : action.shaders)
    {
        // We don't want empty objects in JSON dump
        if (
%Condition%
           )
        {
            std::string shaderStageName = getShaderStageName(i);
            VK_ASSERT(shaderStageName.compare("unknown") != 0);

            pWriter->Key(shaderStageName.c_str());
            pWriter->BeginMap(false);

%ShaderCreateApply%

            pWriter->EndMap();
        }
        i++;
    }
    pWriter->EndMap();
}
"""

ProfileEntryPatternToJsonFunc = """\
void ShaderProfile::ProfileEntryPatternToJson(PipelineProfilePattern pattern, Util::JsonWriter* pWriter)
{
    pWriter->BeginMap(false);
    if (pattern.match.always)
    {
        pWriter->Key("always");
        pWriter->Value(true);
    }

    uint32_t i = 0;
    for (auto shader : pattern.shaders)
    {
        // We don't want empty objects in JSON dump
        if (
%Condition%
           )
        {
            std::string shaderStageName = getShaderStageName(i);
            VK_ASSERT(shaderStageName.compare("unknown") != 0);

            pWriter->Key(shaderStageName.c_str());
            pWriter->BeginMap(false);

%Defs%
            pWriter->EndMap();
        }
        i++;
    }
    pWriter->EndMap();
}
"""

FuncDecParseJsonProfile = \
"""
bool ParseJsonProfile(
    utils::Json*                 pJson,
    PipelineProfile*             pProfile,
    const VkAllocationCallbacks* pAllocator);
"""

FuncDecJsonReader = \
"""
// Methods specific to json reader
bool CheckValidKeys(
    utils::Json* pObject,
    size_t       numKeys,
    const char** pKeys);

bool ParseJsonProfileEntry(
    utils::Json*          pPatterns,
    utils::Json*          pActions,
    utils::Json*          pEntry,
    PipelineProfileEntry* pProfileEntry);

bool ParseJsonProfileEntryPattern(
    utils::Json*            pJson,
    PipelineProfilePattern* pPattern);

bool ParseJsonProfileEntryAction(
    utils::Json*           pJson,
    PipelineProfileAction* pAction);

bool ParseJsonProfilePatternShader(
    utils::Json*          pJson,
    ShaderStage           shaderStage,
    ShaderProfilePattern* pPattern);

bool ParseJsonProfileActionShader(
    utils::Json*         pJson,
    ShaderStage          shaderStage,
    ShaderProfileAction* pActions);
"""

ParseJsonShaderTuningFlagsFunc = \
"""
    bool success = true;

    if (pJson->type == utils::JsonValueType::Number)
    {
        pFlags->u32All = static_cast<uint32_t>(pJson->integerValue);
    }
    else if (pJson->type == utils::JsonValueType::Object)
    {
        static const char* ValidKeys[] =
        {
%ValidKeys%
        };

        success &= CheckValidKeys(pJson, VK_ARRAY_SIZE(ValidKeys), ValidKeys);

        utils::Json* pItem = nullptr;
        pFlags->u32All = 0;

%Defs%
    }
    else
    {
        success = false;
    }

    return success;\
"""

ParseJsonShaderTuningOptionsFunc = \
"""
    bool success = true;

    if (pJson->type == utils::JsonValueType::Number)
    {
        pOptions->u32All = static_cast<uint32_t>(pJson->integerValue);
    }
    else if (pJson->type == utils::JsonValueType::Object)
    {
        static const char* ValidKeys[] =
        {
%ValidKeys%
        };

        success &= CheckValidKeys(pJson, VK_ARRAY_SIZE(ValidKeys), ValidKeys);

        utils::Json* pItem = nullptr;
        pOptions->u32All = 0;

%Defs%
    }
    else
    {
        success = false;
    }

    return success;\
"""

ParseJsonProfileEntryRuntimeFunc = \
"""
    bool success = true;

    static const char* ValidKeys[] =
    {
%ValidKeys%
    };

    success &= CheckValidKeys(pJson, VK_ARRAY_SIZE(ValidKeys), ValidKeys);

    utils::Json* pItem = nullptr;

%Defs%

    return success;"""

ParseJsonProfileEntryPatternFunc = """\
bool ShaderProfile::ParseJsonProfileEntryPattern(
    utils::Json*            pJson,
    PipelineProfilePattern* pPattern)
{
%FuncDefs%
}
"""

ParseJsonProfileEntryActionFunc = """\
bool ShaderProfile::ParseJsonProfileEntryAction(
    utils::Json*           pJson,
    PipelineProfileAction* pAction)
{
%FuncDefs%
}
"""

ParseJsonProfilePatternShaderFunc = """\
bool ShaderProfile::ParseJsonProfilePatternShader(
    utils::Json*          pJson,
    ShaderStage           shaderStage,
    ShaderProfilePattern* pPattern)
{
%FuncDefs%
}
"""

ParseJsonProfileActionShaderFunc = """\
bool ShaderProfile::ParseJsonProfileActionShader(
    utils::Json*         pJson,
    ShaderStage          shaderStage,
    ShaderProfileAction* pActions)
{
%FuncDefs%
}
"""

ConditionGameTitle = """if (appProfile == AppProfile::%GameTitle%)
{
%Defs%\
}
"""

ConditionGfxIp = """if (gfxIpLevel == Pal::GfxIpLevel::%Gfxip%)
{
    SetAppProfile%FuncName%(pPipelineProfile);
}
"""

ConditionCreateInfoApply = """\
if (action.createInfo.apply.%Flag%)
{
%Defs%
}
"""

ConditionShaderCreateApply = """\
if (shader.shaderCreate.apply.%Flag%)
{
%Defs%
}
"""

ConditionDynamicShaderInfoApply = """\
if (shader.dynamicShaderInfo.apply.%Flag%)
{
%Defs%
}
"""

ConditionShaderCreateTuningOptions = """\
if (shader.shaderCreate.tuningOptions.%Flag%)
{
%Defs%
}
"""

ConditionShaderMatchPattern = """\
if (shader.match.%Pattern%)
{
%Defs%
}
"""

ConditionParseJsonProfileEntryRuntime = """\
if ((pItem = utils::JsonGetValue(pJson, "%Key%")) != nullptr)
{
%Defs%
}
"""

EntryCountTemplate = """pPipelineProfile->entryCount = %entryCount%;\n\n"""

IncrementEntryTemplate = """i = (pPipelineProfile->entryCount)++;\n"""

shaderCreateApplyTemplate = """\
    pWriter->Key("%Flag%");
    pWriter->Value(shader.shaderCreate.apply.%Flag%);"""

shaderCreateTuningOptionsTemplate = """\
    pWriter->Key("%Flag%");
    pWriter->Value(shader.shaderCreate.tuningOptions.%Flag%);"""

shaderCreateDynamicShaderInfoTemplate = """\
    pWriter->Key("%Flag%");
    pWriter->Value(shader.dynamicShaderInfo.%Flag%);"""

actionCreateInfoTemplate = """\
    pWriter->Key("%Flag%");
    pWriter->Value(action.createInfo.%Flag%);"""

ShaderCreateApplyTuningOptionsRuntimeTemplate = """\
    pActions->shaderCreate.apply.%Action%         = true;
    pActions->shaderCreate.tuningOptions.%Action% = %Value%;"""

ShaderCreateTuningOptionsRuntimeTemplate = """\
    if (pItem->%ValueType% != 0)
    {
        pActions->shaderCreate.tuningOptions.%Action% = %Value%;
    }"""

ShaderCreateApplyRuntimeTemplate = """\
    if (pItem->%ValueType% != 0)
    {
        pActions->shaderCreate.apply.%Action% = 1;
    }"""

ShaderCreateApplyDynamicShaderInfoRuntimeTemplate = """\
    pActions->dynamicShaderInfo.apply.%Action% = true;
    pActions->dynamicShaderInfo.%Action%       = %Value%;"""

parseJsonProfileEntryPatternTemplate = """\
   success &= ParseJsonProfilePatternShader(pItem, %ShaderStage%, &pPattern->shaders[%ShaderStage%]);"""

parseJsonProfileEntryActionTemplate = """\
   success &= ParseJsonProfileActionShader(pItem, %ShaderStage%, &pAction->shaders[%ShaderStage%]);"""

TypeValues = {
    "integerValue": "static_cast<uint32_t>(pItem->integerValue)",
    "booleanValue": "true"
    }

BRANCHES = [
    ]

def jsonEnumWriterTemplate(values, prefix=""):
    template = ""
    elseValue = ""
    for value in values:
        valueTemplate = \
            "    %Else%if (shader.shaderCreate.tuningOptions.%Flag% == %Prefix%%Value%)\n" + \
            "    {\n" + \
            "        pWriter->Key(\"%Flag%\");\n" + \
            "        pWriter->Value(\"%Value%\");\n" + \
            "    }\n"
        template += valueTemplate \
          .replace("%Else%", elseValue) \
          .replace("%Prefix%", prefix) \
          .replace("%Value%", value)
        elseValue = "else "
    return template

def jsonEnumReaderTemplate(values, prefix=""):
    template = \
      "    if (pItem->pStringValue != nullptr)\n" + \
      "    {\n"
    elseValue = ""
    for value in values:
        valueTemplate = \
          "        %Else%if (strcmp(pItem->pStringValue, \"%Value%\") == 0)\n" + \
          "        {\n" + \
          "            pActions->shaderCreate.apply.%Action%         = true;\n" + \
          "            pActions->shaderCreate.tuningOptions.%Action% = %Prefix%%Value%;\n" + \
          "        }\n"
        template += valueTemplate \
          .replace("%Else%", elseValue) \
          .replace("%Prefix%", prefix) \
          .replace("%Value%", value)
        elseValue = "else "
    template += \
      "        else\n" + \
      "        {\n" + \
      "            success = false;\n" + \
      "        }\n"
    template += \
      "    }\n" + \
      "    else\n" + \
      "    {\n"  + \
      "        success = false;\n" + \
      "    }"
    return template

###################################################################################################################
# The following schema is used for both json parsing as well as defining structures for
###################################################################################################################

SHADER_ACTION = {
    "optStrategyFlags": {
        "type": [int, dict],
        "jsonReadable": True,
        "entityInfo": [
            {
                "parent": "shaderCreate.anonStruct",
                "entity": "bitField",
                "varName": "optStrategyFlags",
                "dataType": "uint32_t",
                "defaultValue": 1,
                "jsonWritable": True,
                "buildTypes": {},
            }
        ],
    },

    "vgprLimit": {
        "type": [int],
        "jsonReadable": True,
        "entityInfo": [
            {
                "parent": "shaderCreate.anonStruct",
                "entity": "bitField",
                "varName": "vgprLimit",
                "dataType": "uint32_t",
                "defaultValue": 1,
                "jsonWritable": True,
                "buildTypes": {},
            },
            {
                "parent": "ShaderTuningOptions",
                "entity": "var",
                "varName": "vgprLimit",
                "dataType": "uint32_t",
                "defaultValue": "",
                "buildTypes": {},
            },
        ],
        "buildTypes": {},
        "codeTemplate": """\
            pPipelineProfile->pEntries[%EntryNum%].action.shaders[%ShaderStage%].shaderCreate.apply.%FieldName% = true;
            pPipelineProfile->pEntries[%EntryNum%].action.shaders[%ShaderStage%].shaderCreate.tuningOptions.%FieldName% = %IntValue%u;\n""",
        "jsonWriterTemplate": shaderCreateTuningOptionsTemplate,
        "jsonReaderTemplate": ShaderCreateApplyTuningOptionsRuntimeTemplate
    },

    "sgprLimit": {
        "type": [int],
        "jsonReadable": True,
        "entityInfo": [
            {
                "parent": "shaderCreate.anonStruct",
                "entity": "bitField",
                "varName": "sgprLimit",
                "dataType": "uint32_t",
                "defaultValue": 1,
                "jsonWritable": True,
                "buildTypes": {},
            },
            {
                "parent": "ShaderTuningOptions",
                "entity": "var",
                "varName": "sgprLimit",
                "dataType": "uint32_t",
                "defaultValue": "",
                "buildTypes": {},
            },
        ],
        "buildTypes": {},
        "codeTemplate": """\
            pPipelineProfile->pEntries[%EntryNum%].action.shaders[%ShaderStage%].shaderCreate.apply.%FieldName% = true;
            pPipelineProfile->pEntries[%EntryNum%].action.shaders[%ShaderStage%].shaderCreate.tuningOptions.%FieldName% = %IntValue%u;\n""",
        "jsonWriterTemplate": shaderCreateTuningOptionsTemplate,
        "jsonReaderTemplate": ShaderCreateApplyTuningOptionsRuntimeTemplate
    },

    "ldsSpillLimitDwords": {
        "type": [int],
        "jsonReadable": True,
        "entityInfo": [
            {
                "parent": "shaderCreate.anonStruct",
                "entity": "bitField",
                "varName": "ldsSpillLimitDwords",
                "dataType": "uint32_t",
                "defaultValue": 1,
                "jsonWritable": True,
                "buildTypes": {},
            },
            {
                "parent": "ShaderTuningOptions",
                "entity": "var",
                "varName": "ldsSpillLimitDwords",
                "dataType": "uint32_t",
                "defaultValue": "",
                "buildTypes": {},
            },

        ],
        "buildTypes": {},
        "codeTemplate": """\
            pPipelineProfile->pEntries[%EntryNum%].action.shaders[%ShaderStage%].shaderCreate.apply.%FieldName% = true;
            pPipelineProfile->pEntries[%EntryNum%].action.shaders[%ShaderStage%].shaderCreate.tuningOptions.%FieldName% = %IntValue%u;\n""",
        "jsonWriterTemplate": shaderCreateTuningOptionsTemplate,
        "jsonReaderTemplate": ShaderCreateApplyTuningOptionsRuntimeTemplate
    },

    "maxArraySizeForFastDynamicIndexing": {
        "type": [int],
        "jsonReadable": True,
        "entityInfo": [
            {
                "parent": "shaderCreate.anonStruct",
                "entity": "bitField",
                "varName": "maxArraySizeForFastDynamicIndexing",
                "dataType": "uint32_t",
                "defaultValue": 1,
                "jsonWritable": True,
                "buildTypes": {},
            },
            {
                "parent": "ShaderTuningOptions",
                "entity": "var",
                "varName": "maxArraySizeForFastDynamicIndexing",
                "dataType": "uint32_t",
                "defaultValue": "",
                "buildTypes": {},
            },
        ],
        "buildTypes": {},
        "codeTemplate": """\
            pPipelineProfile->pEntries[%EntryNum%].action.shaders[%ShaderStage%].shaderCreate.apply.%FieldName% = true;
            pPipelineProfile->pEntries[%EntryNum%].action.shaders[%ShaderStage%].shaderCreate.tuningOptions.%FieldName% = %IntValue%u;\n""",
        "jsonWriterTemplate": shaderCreateTuningOptionsTemplate,
        "jsonReaderTemplate": ShaderCreateApplyTuningOptionsRuntimeTemplate
    },

    "userDataSpillThreshold": {
        "type": [int],
        "jsonReadable": True,
        "entityInfo": [
            {
                "parent": "shaderCreate.anonStruct",
                "entity": "bitField",
                "varName": "userDataSpillThreshold",
                "dataType": "uint32_t",
                "defaultValue": 1,
                "jsonWritable": True,
                "buildTypes": {},
            },
            {
                "parent": "ShaderTuningOptions",
                "entity": "var",
                "varName": "userDataSpillThreshold",
                "dataType": "uint32_t",
                "defaultValue": "",
                "buildTypes": {},
            },
        ],
        "buildTypes": {},
        "codeTemplate": """\
            pPipelineProfile->pEntries[%EntryNum%].action.shaders[%ShaderStage%].shaderCreate.apply.%FieldName% = true;
            pPipelineProfile->pEntries[%EntryNum%].action.shaders[%ShaderStage%].shaderCreate.tuningOptions.%FieldName% = %IntValue%u;\n""",
        "jsonWriterTemplate": shaderCreateTuningOptionsTemplate,
        "jsonReaderTemplate": ShaderCreateApplyTuningOptionsRuntimeTemplate
    },

    "maxThreadGroupsPerComputeUnit": {
        "type": [int],
        "jsonReadable": True,
        "entityInfo": [
            {
                "parent": "shaderCreate.anonStruct",
                "entity": "bitField",
                "varName": "maxThreadGroupsPerComputeUnit",
                "dataType": "uint32_t",
                "defaultValue": 1,
                "jsonWritable": True,
                "buildTypes": {},
            },
            {
                "parent": "ShaderTuningOptions",
                "entity": "var",
                "varName": "maxThreadGroupsPerComputeUnit",
                "dataType": "uint32_t",
                "defaultValue": "",
                "buildTypes": {},
            },
        ],
        "buildTypes": {},
        "codeTemplate": """\
            pPipelineProfile->pEntries[%EntryNum%].action.shaders[%ShaderStage%].shaderCreate.apply.%FieldName% = true;
            pPipelineProfile->pEntries[%EntryNum%].action.shaders[%ShaderStage%].shaderCreate.tuningOptions.%FieldName% = %IntValue%u;\n""",
        "jsonWriterTemplate": shaderCreateTuningOptionsTemplate,
        "jsonReaderTemplate": ShaderCreateApplyTuningOptionsRuntimeTemplate
    },

    "scOptions": {
        "type": [list],
        "jsonReadable": True,
        "entityInfo": [
            {
                "parent": "shaderCreate.anonStruct",
                "entity": "bitField",
                "varName": "scOptions",
                "dataType": "uint32_t",
                "defaultValue": 1,
                "jsonWritable": True,
                "buildTypes": {},
            },
        ],
    },

    "scOptionsMask": {
        "type": [list],
        "jsonReadable": True,
        "entityInfo": [
            {
                "parent": "shaderCreate.anonStruct",
                "entity": "bitField",
                "varName": "scOptionsMask",
                "dataType": "uint32_t",
                "defaultValue": 1,
                "jsonWritable": True,
                "buildTypes": {},
            },
        ],
    },

    "trapPresent": {
        "type": [int],
        "jsonReadable": True,
        "entityInfo": [
            {
                "parent": "shaderCreate.anonStruct",
                "entity": "bitField",
                "varName": "trapPresent",
                "dataType": "uint32_t",
                "defaultValue": 1,
                "jsonWritable": True,
                "buildTypes": {},
            },
        ],
        "buildTypes": {},
        "codeTemplate": """\
            pPipelineProfile->pEntries[%EntryNum%].action.shaders[%ShaderStage%].shaderCreate.apply.%FieldName% = %IntValue%;\n""",
        "jsonWriterTemplate": shaderCreateApplyTemplate,
        "jsonReaderTemplate": ShaderCreateApplyRuntimeTemplate
    },

    "debugMode": {
        "type": [int],
        "jsonReadable": True,
        "entityInfo": [
            {
                "parent": "shaderCreate.anonStruct",
                "entity": "bitField",
                "varName": "debugMode",
                "dataType": "uint32_t",
                "defaultValue": 1,
                "jsonWritable": True,
                "buildTypes": {},
            },
        ],
        "buildTypes": {},
        "codeTemplate": """\
            pPipelineProfile->pEntries[%EntryNum%].action.shaders[%ShaderStage%].shaderCreate.apply.%FieldName% = %IntValue%;\n""",
        "jsonWriterTemplate": shaderCreateApplyTemplate,
        "jsonReaderTemplate": ShaderCreateApplyRuntimeTemplate
    },

    "allowReZ": {
        "type": [int],
        "jsonReadable": True,
        "entityInfo": [
            {
                "parent": "shaderCreate.anonStruct",
                "entity": "bitField",
                "varName": "allowReZ",
                "dataType": "uint32_t",
                "defaultValue": 1,
                "jsonWritable": True,
                "buildTypes": {},
            },
        ],
        "buildTypes": {},
        "codeTemplate": """\
            pPipelineProfile->pEntries[%EntryNum%].action.shaders[%ShaderStage%].shaderCreate.apply.%FieldName% = %IntValue%;\n""",
        "jsonWriterTemplate": shaderCreateApplyTemplate,
        "jsonReaderTemplate": ShaderCreateApplyRuntimeTemplate
    },

    "shaderReplaceEnabled": {
        "type": [str],
        "jsonReadable": True,
        "entityInfo": [
            {
                "parent": "shaderCreate.anonStruct",
                "entity": "bitField",
                "varName": "shaderReplaceEnabled",
                "dataType": "uint32_t",
                "defaultValue": 1,
                "jsonWritable": True,
                "buildTypes": {},
            },
        ],
    },

    "fpControlFlags": {
        "type": [int, dict],
        "entityInfo": [
            {
                "parent": "shaderCreate.anonStruct",
                "entity": "bitField",
                "varName": "fpControlFlags",
                "dataType": "uint32_t",
                "defaultValue": 1,
                "jsonWritable": True,
                "buildTypes": {},
            },
        ],
    },

    "optimizationIntent": {
        "type": [int, dict],
        "entityInfo": [
            {
                "parent": "shaderCreate.anonStruct",
                "entity": "bitField",
                "varName": "optimizationIntent",
                "dataType": "uint32_t",
                "defaultValue": 1,
                "jsonWritable": True,
                "buildTypes": {},
            },
        ],
    },

    "disableLoopUnrolls": {
        "type": [int],
        "jsonReadable": True,
        "entityInfo": [
            {
                "parent": "shaderCreate.anonStruct",
                "entity": "bitField",
                "varName": "disableLoopUnrolls",
                "dataType": "uint32_t",
                "defaultValue": 1,
                "jsonWritable": True,
                "buildTypes": {},
            },
        ],
        "buildTypes": {},
        "codeTemplate": """\
            pPipelineProfile->pEntries[%EntryNum%].action.shaders[%ShaderStage%].shaderCreate.apply.%FieldName% = %IntValue%;\n""",
        "jsonWriterTemplate": shaderCreateApplyTemplate,
        "jsonReaderTemplate": ShaderCreateApplyRuntimeTemplate
    },

    "enableSelectiveInline": {
        "type": [int],
        "jsonReadable": True,
        "entityInfo": [
            {
                "parent": "shaderCreate.anonStruct",
                "entity": "bitField",
                "varName": "enableSelectiveInline",
                "dataType": "uint32_t",
                "defaultValue": 1,
                "jsonWritable": True,
                "buildTypes": {},
            },
        ],
        "buildTypes": {},
        "codeTemplate": """\
            pPipelineProfile->pEntries[%EntryNum%].action.shaders[%ShaderStage%].shaderCreate.apply.%FieldName% = %IntValue%;\n""",
        "jsonWriterTemplate": shaderCreateApplyTemplate,
        "jsonReaderTemplate": ShaderCreateApplyRuntimeTemplate
    },

    "maxOccupancyOptions": {
        "type": [int, dict],
        "entityInfo": [
        ],
    },

    "lowLatencyOptions": {
        "type": [int, dict],
        "entityInfo": [
        ],
    },

    "waveSize": {
        "type": [int],
        "jsonReadable": True,
        "entityInfo": [
            {
                "parent": "shaderCreate.anonStruct",
                "entity": "bitField",
                "varName": "waveSize",
                "dataType": "uint32_t",
                "defaultValue": 1,
                "jsonWritable": True,
                "buildTypes": {},
            },
            {
                "parent": "ShaderTuningOptions",
                "entity": "var",
                "varName": "waveSize",
                "dataType": "uint32_t",
                "defaultValue": "",
                "buildTypes": {},
            },
        ],
        "buildTypes": {},
        "codeTemplate": """\
            pPipelineProfile->pEntries[%EntryNum%].action.shaders[%ShaderStage%].shaderCreate.apply.%FieldName% = true;
            pPipelineProfile->pEntries[%EntryNum%].action.shaders[%ShaderStage%].shaderCreate.tuningOptions.%FieldName% = %IntValue%u;\n""",
        "jsonWriterTemplate": shaderCreateTuningOptionsTemplate,
        "jsonReaderTemplate": ShaderCreateApplyTuningOptionsRuntimeTemplate
    },

    "wgpMode": {
        "type": [int],
        "jsonReadable": True,
        "entityInfo": [
            {
                "parent": "shaderCreate.anonStruct",
                "entity": "bitField",
                "varName": "wgpMode",
                "dataType": "uint32_t",
                "defaultValue": 1,
                "jsonWritable": True,
                "buildTypes": {},
            },
        ],
        "buildTypes": {},
        "codeTemplate": """\
            pPipelineProfile->pEntries[%EntryNum%].action.shaders[%ShaderStage%].shaderCreate.apply.%FieldName% = %IntValue%;\n""",
        "jsonWriterTemplate": shaderCreateApplyTemplate,
        "jsonReaderTemplate": ShaderCreateApplyRuntimeTemplate
    },

    "waveBreakSize": {
        "type": [int],
        "jsonReadable": True,
        "entityInfo": [
            {
                "parent": "shaderCreate.anonStruct",
                "entity": "bitField",
                "varName": "waveBreakSize",
                "dataType": "uint32_t",
                "defaultValue": 1,
                "jsonWritable": True,
                "buildTypes": {},
            },
            {
                "parent": "ShaderTuningOptions",
                "entity": "var",
                "varName": "waveBreakSize",
                "dataType": "uint32_t",
                "defaultValue": "",
                "buildTypes": {},
            },
        ],
        "buildTypes": {},
        "codeTemplate": """\
            pPipelineProfile->pEntries[%EntryNum%].action.shaders[%ShaderStage%].shaderCreate.apply.%FieldName% = true;
            pPipelineProfile->pEntries[%EntryNum%].action.shaders[%ShaderStage%].shaderCreate.tuningOptions.%FieldName% = %IntValue%u;\n""",
        "jsonWriterTemplate": shaderCreateTuningOptionsTemplate,
        "jsonReaderTemplate": ShaderCreateApplyTuningOptionsRuntimeTemplate
    },

    "nggDisable": {
        "type": [int],
        "jsonReadable": True,
        "entityInfo": [
            {
                "parent": "shaderCreate.anonStruct",
                "entity": "bitField",
                "varName": "nggDisable",
                "dataType": "uint32_t",
                "defaultValue": 1,
                "jsonWritable": True,
                "buildTypes": {},
            },
        ],
        "buildTypes": {},
        "codeTemplate": """\
            pPipelineProfile->pEntries[%EntryNum%].action.shaders[%ShaderStage%].shaderCreate.apply.%FieldName% = %IntValue%;\n""",
        "jsonWriterTemplate": shaderCreateApplyTemplate,
        "jsonReaderTemplate": ShaderCreateApplyRuntimeTemplate
    },

    "nggFasterLaunchRate": {
        "type": [int],
        "jsonReadable": True,
        "entityInfo": [
            {
                "parent": "shaderCreate.anonStruct",
                "entity": "bitField",
                "varName": "nggFasterLaunchRate",
                "dataType": "uint32_t",
                "defaultValue": 1,
                "jsonWritable": True,
                "buildTypes": {},
            },
        ],
        "buildTypes": {},
        "codeTemplate": """\
            pPipelineProfile->pEntries[%EntryNum%].action.shaders[%ShaderStage%].shaderCreate.apply.%FieldName% = %IntValue%;\n""",
        "jsonWriterTemplate": shaderCreateApplyTemplate,
        "jsonReaderTemplate": ShaderCreateApplyRuntimeTemplate
    },

    "nggVertexReuse": {
        "type": [int],
        "jsonReadable": True,
        "entityInfo": [
            {
                "parent": "shaderCreate.anonStruct",
                "entity": "bitField",
                "varName": "nggVertexReuse",
                "dataType": "uint32_t",
                "defaultValue": 1,
                "jsonWritable": True,
                "buildTypes": {},
            },
        ],
        "buildTypes": {},
        "codeTemplate": """\
            pPipelineProfile->pEntries[%EntryNum%].action.shaders[%ShaderStage%].shaderCreate.apply.%FieldName% = %IntValue%;\n""",
        "jsonWriterTemplate": shaderCreateApplyTemplate,
        "jsonReaderTemplate": ShaderCreateApplyRuntimeTemplate
    },

    "nggEnableFrustumCulling": {
        "type": [int],
        "jsonReadable": True,
        "entityInfo": [
            {
                "parent": "shaderCreate.anonStruct",
                "entity": "bitField",
                "varName": "nggEnableFrustumCulling",
                "dataType": "uint32_t",
                "defaultValue": 1,
                "jsonWritable": True,
                "buildTypes": {},
            },
        ],
        "buildTypes": {},
        "codeTemplate": """\
            pPipelineProfile->pEntries[%EntryNum%].action.shaders[%ShaderStage%].shaderCreate.apply.%FieldName% = %IntValue%;\n""",
        "jsonWriterTemplate": shaderCreateApplyTemplate,
        "jsonReaderTemplate": ShaderCreateApplyRuntimeTemplate
    },

    "nggEnableBoxFilterCulling": {
        "type": [int],
        "jsonReadable": True,
        "entityInfo": [
            {
                "parent": "shaderCreate.anonStruct",
                "entity": "bitField",
                "varName": "nggEnableBoxFilterCulling",
                "dataType": "uint32_t",
                "defaultValue": 1,
                "jsonWritable": True,
                "buildTypes": {},
            },
        ],
        "buildTypes": {},
        "codeTemplate": """\
            pPipelineProfile->pEntries[%EntryNum%].action.shaders[%ShaderStage%].shaderCreate.apply.%FieldName% = %IntValue%;\n""",
        "jsonWriterTemplate": shaderCreateApplyTemplate,
        "jsonReaderTemplate": ShaderCreateApplyRuntimeTemplate
    },

    "nggEnableSphereCulling": {
        "type": [int],
        "jsonReadable": True,
        "entityInfo": [
            {
                "parent": "shaderCreate.anonStruct",
                "entity": "bitField",
                "varName": "nggEnableSphereCulling",
                "dataType": "uint32_t",
                "defaultValue": 1,
                "jsonWritable": True,
                "buildTypes": {},
            },
        ],
        "buildTypes": {},
        "codeTemplate": """\
            pPipelineProfile->pEntries[%EntryNum%].action.shaders[%ShaderStage%].shaderCreate.apply.%FieldName% = %IntValue%;\n""",
        "jsonWriterTemplate": shaderCreateApplyTemplate,
        "jsonReaderTemplate": ShaderCreateApplyRuntimeTemplate
    },

    "nggEnableBackfaceCulling": {
        "type": [int],
        "jsonReadable": True,
        "entityInfo": [
            {
                "parent": "shaderCreate.anonStruct",
                "entity": "bitField",
                "varName": "nggEnableBackfaceCulling",
                "dataType": "uint32_t",
                "defaultValue": 1,
                "jsonWritable": True,
                "buildTypes": {},
            },
        ],
        "buildTypes": {},
        "codeTemplate": """\
            pPipelineProfile->pEntries[%EntryNum%].action.shaders[%ShaderStage%].shaderCreate.apply.%FieldName% = %IntValue%;\n""",
        "jsonWriterTemplate": shaderCreateApplyTemplate,
        "jsonReaderTemplate": ShaderCreateApplyRuntimeTemplate
    },

    "nggEnableSmallPrimFilter": {
        "type": [int],
        "jsonReadable": True,
        "entityInfo": [
            {
                "parent": "shaderCreate.anonStruct",
                "entity": "bitField",
                "varName": "nggEnableSmallPrimFilter",
                "dataType": "uint32_t",
                "defaultValue": 1,
                "jsonWritable": True,
                "buildTypes": {},
            },
        ],
        "buildTypes": {},
        "codeTemplate": """\
            pPipelineProfile->pEntries[%EntryNum%].action.shaders[%ShaderStage%].shaderCreate.apply.%FieldName% = %IntValue%;\n""",
        "jsonWriterTemplate": shaderCreateApplyTemplate,
        "jsonReaderTemplate": ShaderCreateApplyRuntimeTemplate
    },

    "enableSubvector": {
        "type": [int],
        "jsonReadable": True,
        "entityInfo": [
            {
                "parent": "shaderCreate.anonStruct",
                "entity": "bitField",
                "varName": "enableSubvector",
                "dataType": "uint32_t",
                "defaultValue": 1,
                "jsonWritable": True,
                "buildTypes": {},
            },
        ],
        "buildTypes": {},
        "codeTemplate": """\
            pPipelineProfile->pEntries[%EntryNum%].action.shaders[%ShaderStage%].shaderCreate.apply.%FieldName% = %IntValue%;\n""",
        "jsonWriterTemplate": shaderCreateApplyTemplate,
        "jsonReaderTemplate": ShaderCreateApplyRuntimeTemplate
    },

    "enableSubvectorSharedVgprs": {
        "type": [int],
        "jsonReadable": True,
        "entityInfo": [
            {
                "parent": "shaderCreate.anonStruct",
                "entity": "bitField",
                "varName": "enableSubvectorSharedVgprs",
                "dataType": "uint32_t",
                "defaultValue": 1,
                "jsonWritable": True,
                "buildTypes": {},
            },
        ],
        "buildTypes": {},
        "codeTemplate": """\
            pPipelineProfile->pEntries[%EntryNum%].action.shaders[%ShaderStage%].shaderCreate.apply.%FieldName% = %IntValue%;\n""",
        "jsonWriterTemplate": shaderCreateApplyTemplate,
        "jsonReaderTemplate": ShaderCreateApplyRuntimeTemplate
    },

    "maxWavesPerCu": {
        "type": [int],
        "jsonReadable": True,
        "entityInfo": [
            {
                "parent": "dynamicShaderInfo.anonStruct",
                "entity": "bitField",
                "varName": "maxWavesPerCu",
                "dataType": "uint32_t",
                "defaultValue": 1,
                "jsonWritable": True,
                "buildTypes": {},
            },
            {
                "parent": "dynamicShaderInfo",
                "entity": "var",
                "varName": "maxWavesPerCu",
                "dataType": "uint32_t",
                "defaultValue": "",
                "buildTypes": {},
            },
        ],
        "buildTypes": {},
        "codeTemplate": """\
            pPipelineProfile->pEntries[%EntryNum%].action.shaders[%ShaderStage%].dynamicShaderInfo.apply.%FieldName% = true;
            pPipelineProfile->pEntries[%EntryNum%].action.shaders[%ShaderStage%].dynamicShaderInfo.%FieldName% = %IntValue%u;\n""",
        "jsonWriterTemplate": shaderCreateDynamicShaderInfoTemplate,
        "jsonReaderTemplate": ShaderCreateApplyDynamicShaderInfoRuntimeTemplate
    },

    "cuEnableMask": {
        "type": [int],
        "jsonReadable": True,
        "entityInfo": [
            {
                "parent": "dynamicShaderInfo.anonStruct",
                "entity": "bitField",
                "varName": "cuEnableMask",
                "dataType": "uint32_t",
                "defaultValue": 1,
                "jsonWritable": True,
                "buildTypes": {},
            },
            {
                "parent": "dynamicShaderInfo",
                "entity": "var",
                "varName": "cuEnableMask",
                "dataType": "uint32_t",
                "defaultValue": "",
                "buildTypes": {},
            },
        ],
        "buildTypes": {},
        "codeTemplate": """\
            pPipelineProfile->pEntries[%EntryNum%].action.shaders[%ShaderStage%].dynamicShaderInfo.apply.%FieldName% = true;
            pPipelineProfile->pEntries[%EntryNum%].action.shaders[%ShaderStage%].dynamicShaderInfo.%FieldName% = %IntValue%u;\n""",
        "jsonWriterTemplate": shaderCreateDynamicShaderInfoTemplate,
        "jsonReaderTemplate": \
            "    if (shaderStage != ShaderStage::ShaderStageCompute)\n    {\n" +
            "        pActions->dynamicShaderInfo.apply.%Action% = true;\n" +
            "        pActions->dynamicShaderInfo.%Action%       = %Value%;" +
            "\n    }\n    else\n    {\n" +
            "        success = false;\n    }"
    },

    "maxThreadGroupsPerCu": {
        "type": [int],
        "jsonReadable": True,
        "entityInfo": [
            {
                "parent": "dynamicShaderInfo.anonStruct",
                "entity": "bitField",
                "varName": "maxThreadGroupsPerCu",
                "dataType": "uint32_t",
                "defaultValue": 1,
                "jsonWritable": True,
                "buildTypes": {},
            },
            {
                "parent": "dynamicShaderInfo",
                "entity": "var",
                "varName": "maxThreadGroupsPerCu",
                "dataType": "uint32_t",
                "defaultValue": "",
                "buildTypes": {},
            },
        ],
        "buildTypes": {},
        "codeTemplate": """\
            pPipelineProfile->pEntries[%EntryNum%].action.shaders[%ShaderStage%].dynamicShaderInfo.apply.%FieldName% = true;
            pPipelineProfile->pEntries[%EntryNum%].action.shaders[%ShaderStage%].dynamicShaderInfo.%FieldName% = %IntValue%u;\n""",
        "jsonWriterTemplate": shaderCreateDynamicShaderInfoTemplate,
        "jsonReaderTemplate": \
            "    if (shaderStage == ShaderStage::ShaderStageCompute)\n    {\n" +
            "        pActions->dynamicShaderInfo.apply.%Action% = true;\n" +
            "        pActions->dynamicShaderInfo.%Action%       = %Value%;" +
            "\n    }\n    else\n    {\n" +
            "        success = false;\n    }"
    },

    "useSiScheduler": {
        "type": [int],
        "jsonReadable": True,
        "entityInfo": [
            {
                "parent": "ShaderTuningOptions",
                "entity": "var",
                "varName": "useSiScheduler",
                "dataType": "uint32_t",
                "defaultValue": "",
                "jsonWritable": True,
                "buildTypes": {"andType": ["ICD_BUILD_LLPC"]},
            },
        ],
        "buildTypes": {"andType": ["ICD_BUILD_LLPC"]},
        "codeTemplate": """\
            pPipelineProfile->pEntries[%EntryNum%].action.shaders[%ShaderStage%].shaderCreate.tuningOptions.%FieldName% = true;\n""",
        "jsonWriterTemplate": shaderCreateTuningOptionsTemplate,
        "jsonReaderTemplate": ShaderCreateTuningOptionsRuntimeTemplate
    },

    "reconfigWorkgroupLayout": {
        "type": [int],
        "jsonReadable": True,
        "entityInfo": [
            {
                "parent": "ShaderTuningOptions",
                "entity": "var",
                "varName": "reconfigWorkgroupLayout",
                "dataType": "uint32_t",
                "defaultValue": "",
                "jsonWritable": True,
                "buildTypes": {"andType": ["ICD_BUILD_LLPC"]},
            },
        ],
        "buildTypes": {"andType": ["ICD_BUILD_LLPC"]},
        "codeTemplate": """\
            pPipelineProfile->pEntries[%EntryNum%].action.shaders[%ShaderStage%].shaderCreate.tuningOptions.%FieldName% = true;\n""",
        "jsonWriterTemplate": shaderCreateTuningOptionsTemplate,
        "jsonReaderTemplate": ShaderCreateTuningOptionsRuntimeTemplate
    },

    "forceLoopUnrollCount": {
        "type": [int],
        "jsonReadable": True,
        "entityInfo": [
            {
                "parent": "ShaderTuningOptions",
                "entity": "var",
                "varName": "forceLoopUnrollCount",
                "dataType": "uint32_t",
                "defaultValue": "",
                "jsonWritable": True,
                "buildTypes": {"andType": ["ICD_BUILD_LLPC"]},
            },
        ],
        "buildTypes": {"andType": ["ICD_BUILD_LLPC"]},
        "codeTemplate": """\
            pPipelineProfile->pEntries[%EntryNum%].action.shaders[%ShaderStage%].shaderCreate.tuningOptions.%FieldName% = %IntValue%u;\n""",
        "jsonWriterTemplate": shaderCreateTuningOptionsTemplate,
        "jsonReaderTemplate": ShaderCreateTuningOptionsRuntimeTemplate
    },

    "enableLoadScalarizer": {
        "type": [bool],
        "jsonReadable": True,
        "entityInfo": [
            {
                "parent": "ShaderTuningOptions",
                "entity": "var",
                "varName": "enableLoadScalarizer",
                "dataType": "bool",
                "defaultValue": "",
                "jsonWritable": True,
                "buildTypes": {"andType": ["ICD_BUILD_LLPC"]},
            },
        ],
        "buildTypes": {"andType": ["ICD_BUILD_LLPC"]},
        "codeTemplate": """\
            pPipelineProfile->pEntries[%EntryNum%].action.shaders[%ShaderStage%].shaderCreate.tuningOptions.%FieldName% = %BoolValue%;\n""",
        "jsonWriterTemplate": shaderCreateTuningOptionsTemplate,
        "jsonReaderTemplate": ShaderCreateTuningOptionsRuntimeTemplate
    },

    "disableLicm": {
        "type": [bool],
        "jsonReadable": True,
        "entityInfo": [
            {
                "parent": "ShaderTuningOptions",
                "entity": "var",
                "varName": "disableLicm",
                "dataType": "bool",
                "defaultValue": "",
                "jsonWritable": True,
                "buildTypes": {"andType": ["ICD_BUILD_LLPC"]},
            },
        ],
        "buildTypes": {"andType": ["ICD_BUILD_LLPC"]},
        "codeTemplate": """\
            pPipelineProfile->pEntries[%EntryNum%].action.shaders[%ShaderStage%].shaderCreate.tuningOptions.%FieldName% = %BoolValue%;\n""",
        "jsonWriterTemplate": shaderCreateTuningOptionsTemplate,
        "jsonReaderTemplate": ShaderCreateTuningOptionsRuntimeTemplate
    },

    "unrollThreshold": {
        "type": [int],
        "jsonReadable": True,
        "entityInfo": [
            {
                "parent": "ShaderTuningOptions",
                "entity": "var",
                "varName": "unrollThreshold",
                "dataType": "uint32_t",
                "defaultValue": "",
                "jsonWritable": True,
                "buildTypes": {"andType": ["ICD_BUILD_LLPC"]},
            },
        ],
        "buildTypes": {"andType": ["ICD_BUILD_LLPC"]},
        "codeTemplate": """\
            pPipelineProfile->pEntries[%EntryNum%].action.shaders[%ShaderStage%].shaderCreate.tuningOptions.%FieldName% = %IntValue%u;\n""",
        "jsonWriterTemplate": shaderCreateTuningOptionsTemplate,
        "jsonReaderTemplate": ShaderCreateTuningOptionsRuntimeTemplate
    },

    "fp32DenormalMode": {
        "type": [int],
        "jsonReadable": True,
        "entityInfo": [
            {
                "parent": "shaderCreate.anonStruct",
                "entity": "bitField",
                "varName": "fp32DenormalMode",
                "dataType": "uint32_t",
                "defaultValue": 1,
                "jsonWritable": True,
                "buildTypes": {},
            },
            {
                "parent": "ShaderTuningOptions",
                "entity": "var",
                "varName": "fp32DenormalMode",
                "dataType": "Vkgc::DenormalMode",
                "defaultValue": "",
                "jsonWritable": True,
                "buildTypes": {"andType": ["ICD_BUILD_LLPC"]},
            },
        ],
        "validValues": {
            0: "Auto",
            1: "FlushToZero",
            2: "Preserve"
        },
        "buildTypes": {"andType": ["ICD_BUILD_LLPC"]},
        "codeTemplate": """\
            pPipelineProfile->pEntries[%EntryNum%].action.shaders[%ShaderStage%].shaderCreate.apply.%FieldName% = true;
            pPipelineProfile->pEntries[%EntryNum%].action.shaders[%ShaderStage%].shaderCreate.tuningOptions.
            %FieldName% = Vkgc::DenormalMode::%EnumValue%;\n""",
        "jsonWriterTemplate": jsonEnumWriterTemplate(["Auto", "FlushToZero", "Preserve"], prefix="Vkgc::DenormalMode::"),
        "jsonReaderTemplate": jsonEnumReaderTemplate(["Auto", "FlushToZero", "Preserve"], prefix="Vkgc::DenormalMode::")
    },
}

SHADER_PATTERN = {
    "stageActive": {
        "type": [bool],
        "entityInfo": [
            {
                "parent": "shaderProfilePattern.anonStruct",
                "entity": "bitField",
                "description": "Stage needs to be active",
                "varName": "stageActive",
                "dataType": "uint32_t",
                "defaultValue": 1,
                "buildTypes": {},
            }
        ],
        "codeTemplate": """\
            pPipelineProfile->pEntries[%EntryNum%].pattern.shaders[%ShaderStage%].match.stageActive = %Value%;\n""",
        "jsonWriterTemplate": \
            """    pWriter->Key("stageActive");\n""" +
            "    pWriter->Value(true);",
        "jsonReaderTemplate": "    pPattern->match.stageActive = pItem->%Value%;"
    },
    "stageInactive": {
        "type": [bool],
        "entityInfo": [
            {
                "parent": "shaderProfilePattern.anonStruct",
                "entity": "bitField",
                "description": "Stage needs to be inactive",
                "varName": "stageInactive",
                "dataType": "uint32_t",
                "defaultValue": 1,
                "buildTypes": {},
            }
        ],
        "codeTemplate": """\
            pPipelineProfile->pEntries[%EntryNum%].pattern.shaders[%ShaderStage%].match.stageInactive = %Value%;\n""",
        "jsonWriterTemplate": \
            """    pWriter->Key("stageInactive");\n""" +
            "    pWriter->Value(true);",
        "jsonReaderTemplate": "    pPattern->match.stageInactive = pItem->%Value%;"
    },
    "codeHash": {
        "type": [str],
        "entityInfo": [
            {
                "parent": "shaderProfilePattern.anonStruct",
                "entity": "bitField",
                "description": "Test code hash (128-bit)",
                "varName": "codeHash",
                "dataType": "uint32_t",
                "defaultValue": 1,
                "buildTypes": {},
            },
            {
                "parent": "ShaderProfilePattern",
                "entity": "var",
                "varName": "codeHash",
                "dataType": "Pal::ShaderHash",
                "defaultValue": "",
                "buildTypes": {},
            },
        ],
        "codeTemplate": """\
            pPipelineProfile->pEntries[%EntryNum%].pattern.shaders[%ShaderStage%].match.codeHash = true;
            pPipelineProfile->pEntries[%EntryNum%].pattern.shaders[%ShaderStage%].codeHash.lower = 0x%valueLower%;
            pPipelineProfile->pEntries[%EntryNum%].pattern.shaders[%ShaderStage%].codeHash.upper = 0x%valueUpper%;\n""",
        "jsonWriterTemplate": \
            """    pWriter->Key("codeHash");\n""" +
            "    std::string codeHash = int_to_hex(shader.codeHash.upper, shader.codeHash.lower);\n" +
            "    pWriter->Value(codeHash.c_str());",
        "jsonReaderTemplate": \
            "    // The hash is a 128-bit value interpreted from a JSON hex string.  It should be split by a space into two\n" +
            """    // 64-bit sections, e.g.: { "codeHash" : "0x1234567812345678 1234567812345678" }.\n""" +
            "    char* pLower64 = nullptr;\n" +
            "    pPattern->match.codeHash = true;\n" +
            "    pPattern->codeHash.upper = strtoull(pItem->%Value%, &pLower64, 16);\n" +
            "    pPattern->codeHash.lower = strtoull(pLower64, nullptr, 16);"
    },
    "codeSizeLessThan": {
        "type": [int],
        "entityInfo": [
            {
                "parent": "shaderProfilePattern.anonStruct",
                "entity": "bitField",
                "description": "Test code size less than codeSizeLessThanValue",
                "varName": "codeSizeLessThan",
                "dataType": "uint32_t",
                "defaultValue": 1,
                "buildTypes": {},
            },
            {
                "parent": "ShaderProfilePattern",
                "entity": "var",
                "varName": "codeSizeLessThanValue",
                "dataType": "size_t",
                "defaultValue": "",
                "buildTypes": {},
            }
        ],
        "codeTemplate": """\
                pPipelineProfile->pEntries[%EntryNum%].pattern.shaders[%ShaderStage%].match.codeSizeLessThan = true;
                pPipelineProfile->pEntries[%EntryNum%].pattern.shaders[%ShaderStage%].codeSizeLessThanValue = %Value%;\n""",
        "jsonWriterTemplate": \
            """    pWriter->Key("codeSizeLessThan");\n""" +
            "    size_t codeSizeLessThanValue = static_cast<size_t>(shader.codeSizeLessThanValue);\n" +
            "    pWriter->Value(codeSizeLessThanValue);",
        "jsonReaderTemplate": \
            "    pPattern->match.codeSizeLessThan = true;\n" +
            "    pPattern->codeSizeLessThanValue  = static_cast<size_t>(pItem->%Value%);"
    },
}

PIPELINE_ACTION = {
    "binningOverride": {
        "type": [int],
        "entityInfo": [
            {
                "parent": "createInfo.anonStruct",
                "entity": "bitField",
                "varName": "binningOverride",
                "dataType": "uint32_t",
                "defaultValue": 1,
                "buildTypes": {},
            },
            {
                "parent": "createInfo",
                "entity": "var",
                "varName": "binningOverride",
                "dataType": "Pal::BinningOverride",
                "defaultValue": "",
                "buildTypes": {},
            },
        ],
        "validValues": {
            0: "Pal::BinningOverride::Default",
            1: "Pal::BinningOverride::Disable",
            2: "Pal::BinningOverride::Enable"
        },
        "codeTemplate": """\
            pPipelineProfile->pEntries[%EntryNum%].action.createInfo.apply.binningOverride = true;
            pPipelineProfile->pEntries[%EntryNum%].action.createInfo.binningOverride = %EnumValue%;\n""",
        "jsonWriterTemplate": \
            """    pWriter->Key("binningOverride");\n""" +
            "    if (action.createInfo.binningOverride == Pal::BinningOverride::Default)\n    {\n" +
            "        pWriter->Value(0u);\n    }\n" +
            "    else if (action.createInfo.binningOverride == Pal::BinningOverride::Disable)\n    {\n" +
            "        pWriter->Value(1u);\n    }\n" +
            "    else if (action.createInfo.binningOverride == Pal::BinningOverride::Enable)\n    {\n" +
            "        pWriter->Value(2u);\n    }",
        "jsonReaderTemplate": \
            "    pAction->createInfo.apply.binningOverride = true;\n" +
            "    uint32_t binningOverride                  = static_cast<uint32_t>(pItem->integerValue);\n"
            "    if (binningOverride == 0u)\n    {\n" +
            "        pAction->createInfo.binningOverride = Pal::BinningOverride::Default;\n    }\n" +
            "    else if (binningOverride == 1u)\n    {\n" +
            "        pAction->createInfo.binningOverride = Pal::BinningOverride::Disable;\n    }\n" +
            "    else if (binningOverride == 2u)\n    {\n" +
            "        pAction->createInfo.binningOverride = Pal::BinningOverride::Enable;\n    }",
    },

    "lateAllocVsLimit": {
        "type": [int],
        "entityInfo": [
            {
                "parent": "createInfo.anonStruct",
                "entity": "bitField",
                "varName": "lateAllocVsLimit",
                "dataType": "uint32_t",
                "defaultValue": 1,
                "buildTypes": {},
            },
            {
                "parent": "createInfo",
                "entity": "var",
                "varName": "lateAllocVsLimit",
                "dataType": "uint32_t",
                "defaultValue": "",
                "buildTypes": {},
            },
        ],
        "codeTemplate": """\
            pPipelineProfile->pEntries[%EntryNum%].action.createInfo.apply.lateAllocVsLimit = true;
            pPipelineProfile->pEntries[%EntryNum%].action.createInfo.lateAllocVsLimit = %Value%;\n""",
        "jsonWriterTemplate": actionCreateInfoTemplate,
        "jsonReaderTemplate": \
            "    pAction->createInfo.apply.lateAllocVsLimit = true;\n" +
            "    pAction->createInfo.lateAllocVsLimit       = static_cast<uint32_t>(pItem->integerValue);"
    }
}

ENTRIES_TEMPLATE = {
    "entries":
        {
            "pattern": {
                "always": {
                    "type": [bool],
                    "entityInfo": [
                        {
                            "parent": "pipelineProfilePattern.anonStruct",
                            "description": "Pattern always hits",
                            "entity": "bitField",
                            "varName": "always",
                            "dataType": "uint32_t",
                            "defaultValue": 1,
                            "buildTypes": {},
                        }
                    ],
                    "codeTemplate": """\
                        pPipelineProfile->pEntries[%EntryNum%].pattern.match.always = %Value%;\n""",
                    "jsonReaderTemplate": \
                        """    pPattern->match.always = pItem->%Value%;"""
                    },
                "vs": {
                    "type": [dict],
                    "branch": SHADER_PATTERN,
                    "shaderStage": "ShaderStage::ShaderStageVertex"
                },
                "hs": {
                    "type": [dict],
                    "branch": SHADER_PATTERN,
                    "shaderStage": "ShaderStage::ShaderStageTessControl"
                },
                "ds": {
                    "type": [dict],
                    "branch": SHADER_PATTERN,
                    "shaderStage": "ShaderStage::ShaderStageTessEvaluation"
                },
                "gs": {
                    "type": [dict],
                    "branch": SHADER_PATTERN,
                    "shaderStage": "ShaderStage::ShaderStageGeometry"
                },
                "ps": {
                    "type": [dict],
                    "branch": SHADER_PATTERN,
                    "shaderStage": "ShaderStage::ShaderStageFragment"
                },
                "cs": {
                    "type": [dict],
                    "branch": SHADER_PATTERN,
                    "shaderStage": "ShaderStage::ShaderStageCompute"
                },
            },
            "action": {
                "vs": {
                    "type": [dict],
                    "branch": SHADER_ACTION,
                    "shaderStage": "ShaderStage::ShaderStageVertex"
                },
                "hs": {
                    "type": [dict],
                    "branch": SHADER_ACTION,
                    "shaderStage": "ShaderStage::ShaderStageTessControl"
                },
                "ds": {
                    "type": [dict],
                    "branch": SHADER_ACTION,
                    "shaderStage": "ShaderStage::ShaderStageTessEvaluation"
                },
                "gs": {
                    "type": [dict],
                    "branch": SHADER_ACTION,
                    "shaderStage": "ShaderStage::ShaderStageGeometry"
                },
                "ps": {
                    "type": [dict],
                    "branch": SHADER_ACTION,
                    "shaderStage": "ShaderStage::ShaderStageFragment"
                },
                "cs": {
                    "type": [dict],
                    "branch": SHADER_ACTION,
                    "shaderStage": "ShaderStage::ShaderStageCompute"
                }
            },
            # The "BuildTypes" key is not used by the genShaderProfile script in any way. It is included here simply to
            # mark this key as a valid key that can be a part of each entry in the entries list in profile.json files.
            "BuildTypes": []
        }
}

ShaderTuningStructsAndVars = {
    "ShaderProfilePattern": {
        "entity": "struct",
        "structName": "ShaderProfilePattern",
        "objectName": "",
        "buildTypes": {},
        "child": [
            {
                "match": {
                    "entity": "union",
                    "description": "Defines which pattern tests are enabled",
                    "unionName": "",
                    "objectName": "match",
                    "buildTypes": {},
                    "child": {
                        "shaderProfilePattern.anonStruct": {
                            "entity": "struct",
                            "structName": "",
                            "objectName": "",
                            "buildTypes": {},
                            "child": [
                                SHADER_PATTERN,
                                {
                                    "reserved": {
                                        "entity": "bitField",
                                        "varName": "reserved",
                                        "dataType": "uint32_t",
                                        "defaultValue": 27,
                                        "buildTypes": {},
                                    }
                                }
                            ]
                        },
                        "u32All": {
                            "entity": "var",
                            "varName": "u32All",
                            "dataType": "uint32_t",
                            "defaultValue": "",
                            "buildTypes": {},
                        },
                    }
                }
            },
            SHADER_PATTERN
        ]
    },

    "PipelineProfilePattern": {
            "entity": "struct",
            "description": "Defines which pattern tests are enabled",
            "structName": "PipelineProfilePattern",
            "objectName": "",
            "buildTypes": {},
            "child": [
                {
                    "match": {
                        "entity": "union",
                        "unionName": "",
                        "objectName": "match",
                        "buildTypes": {},
                        "child": {
                            "pipelineProfilePattern.anonStruct": {
                                "entity": "struct",
                                "structName": "",
                                "objectName": "",
                                "buildTypes": {},
                                "child": [
                                    {
                                        "always": ENTRIES_TEMPLATE["entries"]["pattern"]["always"]
                                    },
                                    {
                                        "reserved": {
                                            "entity": "bitField",
                                            "varName": "reserved",
                                            "dataType": "uint32_t",
                                            "defaultValue": 31,
                                            "buildTypes": {},
                                        }
                                    }
                                ]
                            },
                            "u32All": {
                                "entity": "var",
                                "varName": "u32All",
                                "dataType": "uint32_t",
                                "defaultValue": "",
                                "buildTypes": {},
                            },
                        }
                    }
                },
                {
                    "shaders": {
                        "entity": "array",
                        "varName": "shaders",
                        "arraySize": "ShaderStageCount",
                        "arrayValue": "",
                        "dataType": "ShaderProfilePattern",
                        "buildTypes": {},
                    },
                },
            ]
        },

    "ShaderTuningOptions": {
        "entity": "struct",
        "structName": "ShaderTuningOptions",
        "buildTypes": {"andType": [], "orType": []},
        "objectName": "",
        "child": [
            SHADER_ACTION,
        ]
    },

    "ShaderProfileAction": {
        "entity": "struct",
        "structName": "ShaderProfileAction",
        "buildTypes": {"andType": [], "orType": []},
        "objectName": "",
        "child": {
            "shaderCreate": {
                "entity": "struct",
                "structName": "",
                "buildTypes": {},
                "objectName": "shaderCreate",
                "child": {
                    "apply": {
                        "entity": "union",
                        "description": "Defines which values are applied",
                        "unionName": "",
                        "objectName": "apply",
                        "buildTypes": {},
                        "child": {
                            "shaderCreate.anonStruct": {
                                "entity": "struct",
                                "structName": "",
                                "objectName": "",
                                "buildTypes": {},
                                "child": [
                                    SHADER_ACTION,
                                    {
                                        "reserved1": {
                                            "entity": "bitField",
                                            "varName": "reserved",
                                            "dataType": "uint32_t",
                                            "defaultValue": 1,
                                            "buildTypes": {},
                                        },
                                    }
                                ]
                            },
                            "u32All": {
                                "entity": "var",
                                "varName": "u32All",
                                "dataType": "uint32_t",
                                "defaultValue": "",
                                "buildTypes": {},
                            },
                        }
                    },
                    "tuningOptions": {
                        "entity": "var",
                        "varName": "tuningOptions",
                        "dataType": "ShaderTuningOptions",
                        "defaultValue": "",
                        "buildTypes": {},
                    },
                }
            },
            "pipelineShader": {
                "entity": "struct",
                "structName": "",
                "objectName": "pipelineShader",
                "buildTypes": {},
                "child": {
                }
            },
            "dynamicShaderInfo": {
                "entity": "struct",
                "description": "Applied to DynamicXShaderInfo",
                "structName": "",
                "objectName": "dynamicShaderInfo",
                "buildTypes": {},
                "child": [
                    {
                        "apply": {
                            "entity": "union",
                            "description": "Defines which values are applied",
                            "unionName": "",
                            "objectName": "apply",
                            "buildTypes": {},
                            "child": {
                                "dynamicShaderInfo.anonStruct": {
                                    "entity": "struct",
                                    "structName": "",
                                    "objectName": "",
                                    "buildTypes": {},
                                    "child": [
                                        SHADER_ACTION,
                                        {
                                            "reserved": {
                                                "entity": "bitField",
                                                "varName": "reserved",
                                                "dataType": "uint32_t",
                                                "defaultValue": 29,
                                                "buildTypes": {},
                                            },
                                        }
                                    ]
                                },
                                "u32All": {
                                    "entity": "var",
                                    "varName": "u32All",
                                    "dataType": "uint32_t",
                                    "defaultValue": "",
                                    "buildTypes": {},
                                },
                            }
                        },
                    },
                    SHADER_ACTION
                ]
            }
        }

    },

    "PipelineProfileAction": {
        "entity": "struct",
        "structName": "PipelineProfileAction",
        "objectName": "",
        "buildTypes": {},
        "child": {
            "shaders": {
                "entity": "array",
                "description": "Applied to ShaderCreateInfo/PipelineShaderInfo/DynamicXShaderInfo:",
                "varName": "shaders",
                "arraySize": "ShaderStageCount",
                "arrayValue": "",
                "dataType": "ShaderProfileAction",
                "buildTypes": {},
            },
            "createInfo": {
                "entity": "struct",
                "description": "Applied to Graphics/ComputePipelineCreateInfo:",
                "structName": "",
                "objectName": "createInfo",
                "buildTypes": {},
                "child": [
                    {
                        "apply": {
                            "entity": "union",
                            "unionName": "",
                            "objectName": "apply",
                            "buildTypes": {},
                            "child": {
                                "createInfo.anonStruct": {
                                    "entity": "struct",
                                    "structName": "",
                                    "objectName": "",
                                    "buildTypes": {},
                                    "child": [
                                        PIPELINE_ACTION,
                                        ENTRIES_TEMPLATE["entries"]["action"],
                                        {
                                            "reserved": {
                                                "entity": "bitField",
                                                "varName": "reserved",
                                                "dataType": "uint32_t",
                                                "defaultValue": 30,
                                                "buildTypes": {},
                                            }
                                        }
                                    ]
                                },
                                "u32All": {
                                    "entity": "var",
                                    "varName": "u32All",
                                    "defaultValue": "",
                                    "dataType": "uint32_t",
                                    "buildTypes": {},
                                },
                            }
                        }
                    },
                    PIPELINE_ACTION,
                    ENTRIES_TEMPLATE["entries"]["action"]
                ]
            }
        }
    },

    "PipelineProfileEntry": {
        "entity": "struct",
        "description": "This struct describes a single entry in a per-application profile of shader compilation "
                       "parameter tweaks. Each entry describes a pair of match patterns and actions. "
                       "For a given shader in a given pipeline, if all patterns defined by this entry match, "
                       "then all actions are applied to that shader prior to compilation.",
        "structName": "PipelineProfileEntry",
        "objectName": "",
        "buildTypes": {},
        "child": {
            "pattern": {
                "entity": "var",
                "varName": "pattern",
                "defaultValue": "",
                "dataType": "PipelineProfilePattern",
                "buildTypes": {},
            },
            "action": {
                "entity": "var",
                "varName": "action",
                "defaultValue": "",
                "dataType": "PipelineProfileAction",
                "buildTypes": {},
            },
        }
    },

    "InitialPipelineProfileEntries": {
        "entity": "var",
        "varName": "InitialPipelineProfileEntries",
        "dataType": "constexpr uint32_t",
        "defaultValue": "32",
        "buildTypes": {},
    },

    "PipelineProfile": {
        "entity": "struct",
        "description": "Describes a collection of entries that can be used to apply application-specific "
                       "shader compilation tuning to different classes of shaders.",
        "structName": "PipelineProfile",
        "objectName": "",
        "buildTypes": {},
        "child": {
            "entryCount": {
                "entity": "var",
                "varName": "entryCount",
                "defaultValue": "",
                "dataType": "uint32_t",
                "buildTypes": {},
            },
            "entryCapacity": {
                "entity": "var",
                "varName": "entryCapacity",
                "defaultValue": "",
                "dataType": "uint32_t",
                "buildTypes": {},
            },
            "pEntries": {
                "entity": "var",
                "varName": "pEntries",
                "defaultValue": "",
                "dataType": "PipelineProfileEntry*",
                "buildTypes": {},
            },
        }
    }
}

ValidKeysForEntity = {
    "struct": ["structName", "objectName", "buildTypes", "child"],
    "union": ["unionName", "objectName", "buildTypes", "child"],
    "array": ["varName", "arraySize", "arrayValue", "dataType", "buildTypes"],
    "var": ["varName", "defaultValue", "dataType", "buildTypes"],
    "bitField": ["varName", "defaultValue", "dataType", "buildTypes"]
}

BuildTypesTemplate = {
    "llpc": "ICD_BUILD_LLPC",
    "icdRuntimeAppProfile": "ICD_RUNTIME_APP_PROFILE"
}

###################################################################################################################
# The following template includes definitions of generic functions. These are copied to source code as is.
###################################################################################################################

JsonWriterGenericDef = """\
// =====================================================================================================================
// Generic code for generating Json output stream and writing to Json dump file using Pal::JsonWriter
std::string ShaderProfile::getShaderStageName(uint32_t i)
{
    //returns shader stage name
    switch (i)
    {
    case ShaderStage::ShaderStageVertex:
        return "vs";
    case ShaderStage::ShaderStageTessControl:
        return "hs";
    case ShaderStage::ShaderStageTessEvaluation:
        return "ds";
    case ShaderStage::ShaderStageGeometry:
        return "gs";
    case ShaderStage::ShaderStageFragment:
        return "ps";
    case ShaderStage::ShaderStageCompute:
        return "cs";
    default:
        return "unknown";
    }
}

template< typename T >
std::string ShaderProfile::int_to_hex(T upper, T lower)
{
    std::ostringstream stream;
    stream << "0x" << std::setfill('0') << std::setw(sizeof(T) * 2)
        << std::hex << upper << " " << lower;
    return stream.str();
}

template< typename T >
std::string ShaderProfile::int_to_hex(T value)
{
    std::ostringstream stream;
    stream << "0x" << std::setfill('0') << std::setw(sizeof(T) * 2)
        << std::hex << value;
    return stream.str();
}

void ShaderProfile::PipelineProfileToJson(PipelineProfile pipelineProfile, const char* pFilePath)
{
    utils::JsonOutputStream jsonStream(pFilePath);
    Util::JsonWriter writer(&jsonStream);
    uint32_t num_entries = pipelineProfile.entryCount;

    writer.BeginMap(false);
    writer.Key("entries");
    writer.BeginList(false);
    for (uint32_t i = 0; i < num_entries; i++)
    {
        writer.BeginMap(false);
        writer.Key("pattern");
        ProfileEntryPatternToJson(pipelineProfile.pEntries[i].pattern, &writer);

        writer.Key("action");
        ProfileEntryActionToJson(pipelineProfile.pEntries[i], &writer);
        writer.EndMap();
    }

    writer.EndList();
    writer.EndMap();
}
"""

JsonReaderGenericDef = \
"""
// =====================================================================================================================
// Generic code for parsing Json dump file using json_reader.

// Tests that each key in the given JSON object matches at least one of the keys in the array.
bool ShaderProfile::CheckValidKeys(
    utils::Json* pObject,
    size_t       numKeys,
    const char** pKeys)
{
    bool success = true;

    if ((pObject != nullptr) && (pObject->type == utils::JsonValueType::Object))
    {
        for (utils::Json* pChild = pObject->pChild; success && (pChild != nullptr); pChild = pChild->pNext)
        {
            if (pChild->pKey != nullptr)
            {
                bool found = false;

                for (size_t i = 0; (found == false) && (i < numKeys); ++i)
                {
                    found |= (strcmp(pKeys[i], pChild->pKey) == 0);
                }

                success &= found;
            }
        }
    }
    else
    {
        success = false;
    }

    return success;
}

bool ShaderProfile::ParseJsonProfile(
    utils::Json*                 pJson,
    PipelineProfile*             pProfile,
    const VkAllocationCallbacks* pAllocator)
{
/*  Example of the run-time profile:
    {
      "entries": [
        {
          "pattern": {
            "always": false,
            "vs": {
              "stageActive": true,
              "codeHash": "0x0 7B9BFA968C24EB11"
            }
          },
          "action": {
            "lateAllocVsLimit": 1000000,
            "vs": {
              "maxThreadGroupsPerComputeUnit": 10
            }
          }
        }
      ]
    }
*/

    bool success = true;

    if (pJson != nullptr)
    {
        utils::Json* pEntries  = utils::JsonGetValue(pJson, "entries");
        utils::Json* pPatterns = utils::JsonGetValue(pJson, "patterns");
        utils::Json* pActions  = utils::JsonGetValue(pJson, "actions");

        if (pEntries != nullptr)
        {
            for (utils::Json* pEntry = pEntries->pChild; (pEntry != nullptr) && success; pEntry = pEntry->pNext)
            {
                if (pProfile->entryCount < pProfile->entryCapacity)
                {
                    success &= ParseJsonProfileEntry(pPatterns, pActions, pEntry, &pProfile->pEntries[pProfile->entryCount++]);
                }
                else
                {
                    uint32_t newCapacity = pProfile->entryCapacity * 2;

                    size_t newSize = newCapacity * sizeof(PipelineProfileEntry);
                    void* pMemory = pAllocator->pfnAllocation(pAllocator->pUserData,
                                                              newSize,
                                                              VK_DEFAULT_MEM_ALIGN,
                                                              VK_SYSTEM_ALLOCATION_SCOPE_OBJECT);
                    if (pMemory != nullptr)
                    {
                        std::memset(pMemory, 0, newSize);

                        PipelineProfileEntry* pNewEntries = static_cast<PipelineProfileEntry*>(pMemory);
                        std::memcpy(pNewEntries, pProfile->pEntries, pProfile->entryCount * sizeof(PipelineProfileEntry));
                        pProfile->entryCapacity = newCapacity;

                        pAllocator->pfnFree(pAllocator->pUserData, pProfile->pEntries);
                        pProfile->pEntries = pNewEntries;

                        success &= ParseJsonProfileEntry(pPatterns, pActions, pEntry, &pProfile->pEntries[pProfile->entryCount++]);
                    }
                    else
                    {
                        success = false;
                    }
                }
            }
        }
    }
    else
    {
        success = false;
    }

    return success;
}

bool ShaderProfile::ParseJsonProfileEntry(
    utils::Json*          pPatterns,
    utils::Json*          pActions,
    utils::Json*          pEntry,
    PipelineProfileEntry* pProfileEntry)
{
    bool success = true;

    static const char* ValidKeys[] =
    {
        "pattern",
        "action",
        // BuildTypes key is added here only to maintain consistency. The value against this key (if any in JSON) is not read at runtime.
        "BuildTypes"
    };

    success &= CheckValidKeys(pEntry, VK_ARRAY_SIZE(ValidKeys), ValidKeys);

    PipelineProfileEntry entry = {};

    utils::Json* pPattern = utils::JsonGetValue(pEntry, "pattern");

    if (pPattern != nullptr)
    {
        if (pPattern->type == utils::JsonValueType::String)
        {
            pPattern = (pPatterns != nullptr) ? utils::JsonGetValue(pPatterns, pPattern->pStringValue) : nullptr;
        }
    }

    if ((pPattern != nullptr) && (pPattern->type != utils::JsonValueType::Object))
    {
        pPattern = nullptr;
    }

    utils::Json* pAction = utils::JsonGetValue(pEntry, "action");

    if (pAction != nullptr)
    {
        if (pAction->type == utils::JsonValueType::String)
        {
            pAction = (pActions != nullptr) ? utils::JsonGetValue(pActions, pAction->pStringValue) : nullptr;
        }
    }

    if ((pAction != nullptr) && (pAction->type != utils::JsonValueType::Object))
    {
        pAction = nullptr;
    }

    if ((pPattern != nullptr) && (pAction != nullptr))
    {
        success &= ParseJsonProfileEntryPattern(pPattern, &pProfileEntry->pattern);
        success &= ParseJsonProfileEntryAction(pAction, &pProfileEntry->action);
    }
    else
    {
        success = false;
    }

    return success;
}
"""

ParseDwordArrayFunc = """
void ShaderProfile::ParseDwordArray(
    utils::Json* pItem,
    uint32_t     maxCount,
    uint32_t     defaultValue,
    uint32_t*    pArray)
{
    for (uint32_t i = 0; i < maxCount; ++i)
    {
        utils::Json* pElement = utils::JsonArrayElement(pItem, i);

        if (pElement != nullptr)
        {
            pArray[i] = static_cast<uint32_t>(pElement->integerValue);
        }
        else
        {
            pArray[i] = defaultValue;
        }
    }
}
"""
