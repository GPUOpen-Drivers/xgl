#!/usr/bin/env python3
##
 #######################################################################################################################
 #
 #  Copyright (c) 2020-2022 Advanced Micro Devices, Inc. All Rights Reserved.
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

from queue import Empty
import sys
import os
import json
import argparse
import warnings
import textwrap
from itertools import chain
from shaderProfileTemplate import *

outputFile     = "g_shader_profile"
configFileName = "profile.json"
headerFileName = outputFile + ".h"
sourceFileName = outputFile + ".cpp"

###################################################################################################################
# Functions to parse app profiles from JSON files and convert to C++ structures and functions (AT COMPILE TIME)
###################################################################################################################

# Parses stage patterns from the input json file and fetches code template from shaderProfileTemplate.py
def parseJsonProfilePatternShader(shaderPatterns):
    success = checkValidKeys(shaderPatterns, SHADER_PATTERN)
    codeShaderPattern = ""

    if success:
        for shaderPatternKey, shaderPatternValue in shaderPatterns.items():

            if type(shaderPatternValue) in SHADER_PATTERN[shaderPatternKey]["type"]:
                success &= True

            else:
                success &= False
                warnings.warn("********** Warning: Type Mismatch for shader_pattern **********\n"
                              "Parsed Stage Pattern key: {0}\n"
                              "Parsed Stage Pattern value: {1}\n"
                              "Parsed Stage Pattern value type: {2}\n"
                              "Expected value type: {3}\n".format(shaderPatternKey,
                                                                  shaderPatternValue,
                                                                  type(shaderPatternValue),
                                                                  ENTRIES_TEMPLATE["entries"]["pattern"][shaderPatternKey]["type"]))

            cppCode = SHADER_PATTERN[shaderPatternKey]["codeTemplate"]
            if shaderPatternKey == "stageActive":
                cppCode = cppCode.replace("%Value%", str(shaderPatternValue).lower())

            elif shaderPatternKey == "stageInactive":
                cppCode = cppCode.replace("%Value%", str(shaderPatternValue).lower())

            elif shaderPatternKey == "codeHash":
                codeHash = str(shaderPatternValue).split(' ')
                valueUpper = (codeHash[0][2:]).zfill(16).upper()
                valueLower = codeHash[1].zfill(16).upper()
                cppCode = cppCode.replace("%valueLower%", valueLower)
                cppCode = cppCode.replace("%valueUpper%", valueUpper)

            elif shaderPatternKey == "codeSizeLessThan":
                cppCode = cppCode.replace("%Value%", str(shaderPatternValue).lower())

            codeShaderPattern = codeShaderPattern + cppCode

        return success, codeShaderPattern

    else:
        print("************Parsing failed****************")
        return success, codeShaderPattern

# Parses patterns from the input json file and fetches code template from shaderProfileTemplate.py
def parseJsonProfileEntryPattern(pattern):
    success = checkValidKeys(pattern, ENTRIES_TEMPLATE["entries"]["pattern"])
    codePattern = ""
    if success:
        for patternKey, patternValue in pattern.items():
            cppCode = ""
            if type(patternValue) in ENTRIES_TEMPLATE["entries"]["pattern"][patternKey]["type"]:
                success &= True
            else:
                success &= False
                warnings.warn("********** Warning: Type Mismatch for pattern **********\n"
                              "Parsed Pattern key: {0}\n"
                              "Parsed Pattern value: {1}\n"
                              "Parsed Pattern value type: {2}\n"
                              "Expected value type: {3}\n".format(patternKey,
                                                                  patternValue,
                                                                  type(patternValue),
                                                                  ENTRIES_TEMPLATE["entries"]["pattern"][patternKey]["type"]))

            if patternKey == "always":

                cppCode = ENTRIES_TEMPLATE["entries"]["pattern"][patternKey]["codeTemplate"]
                cppCode = cppCode.replace("%Value%", str(patternValue).lower())

            elif patternKey in ["vs",
                                "hs",
                                "ds",
                                "gs",
                                "ps",
                                "cs"]:

                success, cppCode = parseJsonProfilePatternShader(patternValue)
                shaderStage = ENTRIES_TEMPLATE["entries"]["pattern"][patternKey]["shaderStage"]
                cppCode = cppCode.replace("%ShaderStage%", shaderStage)

            codePattern = codePattern + cppCode

        return success, codePattern

    else:
        print("************ Parsing failed ****************")
        return success, codePattern

def parseJsonFlags(key, flags):
    cppCode = ""
    success = False
    return success, cppCode

# Parses stage actions from the input json file and fetches code template from shaderProfileTemplate.py.
# Includes parsing options for
# [
#  'optStrategyFlags', 'optStrategyFlags2', 'vgprLimit', 'sgprLimit', 'ldsSpillLimitDwords',
#  'maxArraySizeForFastDynamicIndexing',
#  'userDataSpillThreshold', 'maxThreadGroupsPerComputeUnit', 'scOptions', 'scOptionsMask', 'trapPresent',
#  'debugMode', 'allowReZ', 'shaderReplaceEnabled', 'fpControlFlags', 'optimizationIntent', 'disableLoopUnrolls',
#  'enableSelectiveInline', 'maxOccupancyOptions', 'lowLatencyOptions', 'waveSize', 'wgpMode', 'waveBreakSize',
#  'nggDisable', 'nggFasterLaunchRate', 'nggVertexReuse', 'nggEnableFrustumCulling', 'nggEnableBoxFilterCulling',
#  'nggEnableSphereCulling', 'nggEnableBackfaceCulling', 'nggEnableSmallPrimFilter', 'enableSubvector',
#  'enableSubvectorSharedVgprs', 'maxWavesPerCu', 'cuEnableMask', 'maxThreadGroupsPerCu', 'useSiScheduler',
#  'reconfigWorkgroupLayout', 'forceLoopUnrollCount', 'enableLoadScalarizer', 'disableLicm', 'unrollThreshold'
# ]
def parseJsonProfileActionShader(shaderActions):
    success = checkValidKeys(shaderActions, SHADER_ACTION)

    result = {}
    result['success'] = success

    for branch in BRANCHES:
        if branch not in result:
            result[branch] = False

    codeShaderAction = ""

    if success:
        for shaderActionKey, shaderActionValue in shaderActions.items():
            cppCode = ""

            if type(shaderActionValue) in SHADER_ACTION[shaderActionKey]["type"]:
                success &= True

            else:
                success &= False
                warnings.warn("********** Warning: Type Mismatch for shader action **********\n"
                              "Parsed Stage Action Key: {0}\n"
                              "Parsed Stage Action value: {1}\n"
                              "Parsed Stage Action Value type: {2}\n"
                              "Expected value type: {3}".format(shaderActionKey,
                                                                shaderActionValue,
                                                                type(shaderActionValue),
                                                                SHADER_ACTION[shaderActionKey]["type"]))
            result['success'] |= success

            if shaderActionKey in BRANCHES:
                if shaderActionKey == 'optStrategyFlags':
                    result["optStrategyFlags"] = True
                elif shaderActionKey == 'optStrategyFlags2':
                    result["optStrategyFlags2"] = True
                elif shaderActionKey == 'fpControlFlags':
                    result["fpControlFlags"] = True
                elif shaderActionKey == 'maxOccupancyOptions':
                    result["maxOccupancyOptions"] = True
                elif shaderActionKey == 'lowLatencyOptions':
                    result["lowLatencyOptions"] = True

            if (isinstance(shaderActionValue, int)  or
                isinstance(shaderActionValue, list) or
                isinstance(shaderActionValue, str)  or
                isinstance(shaderActionValue, bool)):
                if "codeTemplate" in SHADER_ACTION[shaderActionKey]:
                    cppCode = SHADER_ACTION[shaderActionKey]["codeTemplate"]
                else:
                    cppCode = ""
                    continue

                if "%FieldName%" in cppCode:
                    cppCode = cppCode.replace("%FieldName%", str(shaderActionKey))
                if "%IntValue%" in cppCode:
                    cppCode = cppCode.replace("%IntValue%", str(shaderActionValue).lower())
                if "%EnumValue%" in cppCode:
                    cppCode = cppCode.replace("%EnumValue%", str(SHADER_ACTION[shaderActionKey]["validValues"][shaderActionValue]))
                if "%ListValue%" in cppCode:
                    cppCode = cppCode.replace("%ListValue%", convertToArray(str(shaderActionValue)))
                if "%StrValue%" in cppCode:
                    cppCode = cppCode.replace("%StrValue%", str(shaderActionValue))
                if "%BoolValue%" in cppCode:
                    cppCode = cppCode.replace("%BoolValue%", str(shaderActionValue).lower())
            else:
                # should be a dictionary type
                success, cppCode = parseJsonFlags(shaderActionKey, shaderActionValue)
                result['success'] |= success

            # wrap with directive only if the buildType dictionary does not contain only a compiler related build type
            if "buildTypes" in SHADER_ACTION[shaderActionKey] \
                and len(SHADER_ACTION[shaderActionKey]["buildTypes"]) != 0 \
                and not isCompilerOnlyBuildType(SHADER_ACTION[shaderActionKey]["buildTypes"]):
                cppCode = wrapWithDirective(cppCode, SHADER_ACTION[shaderActionKey]["buildTypes"])

            codeShaderAction = codeShaderAction + cppCode
        return result, codeShaderAction

    else:
        print("************Parsing failed****************")
        return result, codeShaderAction

# Parses actions from the input json file and fetches code template from shaderProfileTemplate.py
def parseJsonProfileEntryAction(action):
    result = {}
    result['success'] = False

    for branch in BRANCHES:
        if branch not in result:
            result[branch] = False

    success = True
    for actionKey in action:
        if actionKey in ENTRIES_TEMPLATE["entries"]["action"]:
            success &= True
        elif actionKey in PIPELINE_ACTION:
            success &= True
        else:
            success = False

    codeAction = ""
    if success:
        for actionKey, actionValue in action.items():
            cppCode = ""

            if actionKey in ENTRIES_TEMPLATE["entries"]["action"]:
                if type(actionValue) in ENTRIES_TEMPLATE["entries"]["action"][actionKey]["type"]:
                    success &= True
            elif actionKey in PIPELINE_ACTION:
                if type(actionValue) in PIPELINE_ACTION[actionKey]["type"]:
                    success &= True
            else:
                success &= False
                warnings.warn("********** Warning: Type Mismatch for action **********\n")

            result['success'] |= success

            if actionKey in [ "vs",
                              "hs",
                              "ds",
                              "gs",
                              "ps",
                              "cs"]:
                actionResult, cppCode = parseJsonProfileActionShader(actionValue)
                success = actionResult['success']
                result = actionResult
                shaderStage = ENTRIES_TEMPLATE["entries"]["action"][actionKey]["shaderStage"]
                cppCode = cppCode.replace("%ShaderStage%", shaderStage)

            else:
                if actionKey in PIPELINE_ACTION:
                    cppCode = PIPELINE_ACTION[actionKey]["codeTemplate"]
                    if "validValues" in PIPELINE_ACTION[actionKey]:
                        value = PIPELINE_ACTION[actionKey]["validValues"][actionValue]
                        cppCode = cppCode.replace("%EnumValue%", value)
                    else:
                        cppCode = cppCode.replace("%Value%", str(actionValue))

            codeAction = codeAction + cppCode
        return result, codeAction
    else:
        print("************ Parsing failed ****************")
        return result, codeAction

# Takes the entire json object as input, fetches corresponding code template from shaderProfileTemplate.py, manipulates
# it according to tuning parameters present in the json file and finally returns a block of code that is going to reside
# inside g_shader_profile.cpp . The block of code that is returned builds the shader profile in g_shader_profile.cpp
def genProfile(dict, compiler, gfxip):
    entries = dict["entries"]
    entryCount = 0
    cppCode = ""
    result = {}
    result['success'] = False

    for branch in BRANCHES:
        if branch not in result:
            result[branch] = False

    if len(entries) != 0:
        for entry in entries:
            if checkValidKeys(entry, ENTRIES_TEMPLATE["entries"]):
                pattern = entry["pattern"]
                action = entry["action"]

                success, cppPattern = parseJsonProfileEntryPattern(pattern)
                actionResult, cppAction = parseJsonProfileEntryAction(action)
                for branch in actionResult:
                    if actionResult[branch]:
                        result[branch] = True

                if gfxip == "generic":
                    cppCode = cppCode + IncrementEntryTemplate + cppPattern + cppAction + "\n"
                    cppCode = cppCode.replace("%EntryNum%", 'i')
                else:
                    cppCode = cppCode + cppPattern + cppAction + "\n"
                    cppCode = cppCode.replace("%EntryNum%", str(entryCount))
                    entryCount = entryCount + 1
            else:
                print("************ Parsing failed ****************")

        if gfxip == "generic":
            entryCountTemplate = ""
        else:
            entryCountTemplate = EntryCountTemplate.replace("%entryCount%", str(entryCount))

        var = ""
        varTemplate = ""

        if gfxip == "generic":
            var = InitializedVarTemplate.replace("%DataType%", 'uint32_t')
            var = var.replace("%VarName%", 'i')
            var = var.replace("%DefaultValue%", str(0))
            varTemplate = varTemplate + var + "\n"

        cppCode = varTemplate + entryCountTemplate + cppCode

    return dedentAll(cppCode.rstrip("\n"))

# recursive function
def createStructAndVarDefinitions(dictObjects, parent = None):
    contentAll = ''
    if not isinstance(dictObjects, list):
       dictObjects = [dictObjects]
    for dictObject in dictObjects:
        content = ''
        for key, value in dictObject.items():
            if "entityInfo" in value:
                # fetch entityInfo with the given parent name
                value = retrieveEntityInfo(value, parent)
                if not value:
                    continue

            if "entity" in value:
                success = checkValidKeys(ValidKeysForEntity[value["entity"]], value)
                template = ''
                if success:
                    if value["entity"] == "struct":
                        if value["structName"] != "":
                            template = StructTemplate.replace("%StructName%", " " + value["structName"])
                        else:
                            template = StructTemplate.replace("%StructName%", value["structName"])

                        template = template.replace("%StructObj%", value["objectName"])
                        if value["buildTypes"]:
                            template = wrapWithDirective(template, value["buildTypes"])
                        if value["child"]:
                            structBody = createStructAndVarDefinitions(value["child"], parent=key)
                        else:
                            structBody = ''

                        template = template.replace("%StructDefs%", indent(structBody))

                    if value["entity"] == "union":
                        if value["unionName"] != "":
                            template = StructTemplate.replace("%UnionName%", " " + value["unionName"])
                        else:
                            template = UnionTemplate.replace("%UnionName%", value["unionName"])

                        template = template.replace("%UnionObj%", value["objectName"])
                        if value["buildTypes"]:
                            template = wrapWithDirective(template, value["buildTypes"])
                        if value["child"]:
                            unionBody = createStructAndVarDefinitions(value["child"], parent=key)
                        else:
                            unionBody = ''

                        template = template.replace("%UnionDefs%", indent(unionBody))

                    if value["entity"] == "var":
                        # Initialized Variable
                        if value["defaultValue"]:
                            template = InitializedVarTemplate.replace("%DataType%", value["dataType"])
                            template = template.replace("%VarName%", value["varName"])
                            template = template.replace("%DefaultValue%", str(value["defaultValue"]))
                        # Uninitialized variable
                        else:
                            template = UninitializedVarTemplate.replace("%DataType%", value["dataType"])
                            template = template.replace("%VarName%", value["varName"])

                        if value["buildTypes"]:
                            template = wrapWithDirective(template, value["buildTypes"])

                    if value["entity"] == "bitField":
                        template = BitFieldVarTemplate.replace("%DataType%", value["dataType"])
                        template = template.replace("%VarName%", value["varName"])
                        template = template.replace("%DefaultValue%", str(value["defaultValue"]))

                        if value["buildTypes"]:
                            template = wrapWithDirective(template, value["buildTypes"])

                    if value["entity"] == "array":
                        # initialized array
                        if value["arrayValue"]:
                            template = InitializedArrTemplate.replace("%DataType%", value["dataType"])
                            template = template.replace("%VarName%", value["varName"])
                            template = template.replace("%ArrSize%", value["arraySize"])
                            template = template.replace("%ArrValue%", value["arrayValue"])

                        # Uninitialized array
                        else:
                            template = UnInitializedArrTemplate.replace("%DataType%", value["dataType"])
                            template = template.replace("%VarName%", value["varName"])
                            template = template.replace("%ArrSize%", value["arraySize"])

                        if value["buildTypes"]:
                            template = wrapWithDirective(template, value["buildTypes"])

                    if "description" in value:
                        if "secured" in value:
                            template = wrapWithComment(template, value["description"], value["secured"])
                        else:
                            template = wrapWithComment(template, value["description"], "false")

                    content = content + template
                else:
                    print("************ Parsing failed ****************")
        contentAll = contentAll + content

    return contentAll.strip("\n")

# Reads app_profile.h from its location in the driver, and parses the AppProfile Class to retrieve the names of game
# titles and their build type. Returns a dictionary of the form
#  {
#   "released": {
#     "gameTitles": [],
#     "buildTypes": {"andType": []}
#    },
#  }
def getGameTitles(filePath):
    content = open(filePath).readlines()
    tmpContent = None
    appProfileContent = None

    start = -1
    for i, line in enumerate(content):
        if line.startswith("enum class AppProfile"):
            start = i + 1
        elif ("};" in line) and (start >= 0):
            appProfileContent = content[start:i]
            break

    if appProfileContent is not None:
        gameTitleInfo = {
            "released": {
                "gameTitles": [],
                "buildTypes": {"andType": []}
            }
        }

        hasBuildType = False
        directive = ""
        for i, title in enumerate(appProfileContent):
            title = title.replace("\n", "")
            title = title.replace(" ", "")
            title = title.replace(",", "")
            # removes comments
            if "//" in title:
                title = title.split("//")[0]
            if title.startswith("Default"):
                continue
            if title.startswith("{"):
                continue
            if title == "":
                continue

            if "#if" in title:
                hasBuildType = True
                directive = title.strip("#if ")
                gameTitleInfo[directive] = {
                    "gameTitles": [],
                    "buildTypes": {"andType": [directive]}
                }
                continue

            elif ("#end" in title) or ("#else" in title):
                hasBuildType = False
                continue

            if hasBuildType:
                gameTitleInfo[directive]["gameTitles"].append(title)
            else:
                gameTitleInfo["released"]["gameTitles"].append(title)

        return gameTitleInfo

    else:
        return {}

###################################################################################################################
# Build methods to dump Pipeline profile of a specific (currently running) app to a JSON file
###################################################################################################################

def buildProfileEntryPatternToJson():
    cppCode = ""
    conditionStr = ""
    defs = ""
    patternCount = 0
    for pattern in SHADER_PATTERN:
        if (patternCount < len(SHADER_PATTERN) - 1):
            conditionStr = conditionStr + "shader.match." + pattern + " ||\n"
        else:
            conditionStr = conditionStr + "shader.match." + pattern

        defs = defs + ConditionShaderMatchPattern.replace("%Pattern%", pattern)
        defs = defs.replace("%Defs%", SHADER_PATTERN[pattern]["jsonWriterTemplate"])
        patternCount += 1

    cppCode = ProfileEntryPatternToJsonFunc.replace("%Condition%", indent(conditionStr,times=3))
    cppCode = cppCode.replace("%Defs%", indent(defs, times=3))
    return cppCode

# Iterates over SHADER_ACTION but dumps only the keys/actions that are declared in ShaderTuningOptions, dynamicShaderInfo
# and shaderCreate structures. This essentially means that this key in SHADER_ACTION should have at least one of these in
# in the parent field in entityInfo
def buildProfileEntryActionToJson():
    cppCode = ""
    for action in PIPELINE_ACTION:
        conditionStr = ""
        for entity in PIPELINE_ACTION[action]["entityInfo"]:
            if entity["parent"] == "createInfo.anonStruct":
                conditionStr = ConditionCreateInfoApply.replace("%Defs%", PIPELINE_ACTION[action]["jsonWriterTemplate"])
                conditionStr = conditionStr.replace("%Flag%", action)
                cppCode = cppCode + conditionStr

    funcDef = ProfileEntryActionToJsonFunc.replace("%CreateInfoApply%", indent(cppCode.strip("\n")))

    cppCode = ""
    for action in SHADER_ACTION:
        conditionStr = ""
        if "jsonWriterTemplate" in SHADER_ACTION[action]:
            for entity in SHADER_ACTION[action]["entityInfo"]:
                if "jsonWritable" in entity and entity["jsonWritable"]:
                    if entity["parent"] == "shaderCreate.anonStruct":
                        conditionStr = ConditionShaderCreateApply.replace("%Defs%", SHADER_ACTION[action]["jsonWriterTemplate"])

                        if action == "optStrategyFlags":
                            optStrategyStr = ""
                            for key in OPT_STRATEGY_FLAGS.keys():
                                optFlagCondStr = ConditionShaderCreateTuningOptions
                                optFlagCondStr = optFlagCondStr.replace("%Flag%", "flags" + '.' + key)
                                optFlagCondStr = optFlagCondStr.replace("%Defs%", shaderCreateTuningOptionsBooleanTemplate.replace("%Flag%", key))
                                optStrategyStr += indent(optFlagCondStr)
                            conditionStr = conditionStr.replace("%OptStrategyEntry%", optStrategyStr)
                        elif action == "optStrategyFlags2":
                            optStrategy2Str = ""
                            for key in OPT_STRATEGY_FLAGS2.keys():
                                optFlag2CondStr = ConditionShaderCreateTuningOptions
                                optFlag2CondStr = optFlag2CondStr.replace("%Flag%", "flags2" + '.' + key)
                                optFlag2CondStr = optFlag2CondStr.replace("%Defs%", shaderCreateTuningOptionsBooleanTemplate.replace("%Flag%", key))
                                optStrategy2Str += indent(optFlag2CondStr)
                            conditionStr = conditionStr.replace("%OptStrategy2Entry%", optStrategy2Str)

                        elif action == "optimizationIntent":
                            maxOccupancyStr = ""
                            lowLatencyStr   = ""

                            for key in MAX_OCCUPANCY_OPTIONS.keys():
                                maxOccupancyCondStr = ConditionShaderCreateTuningOptions
                                maxOccupancyCondStr = maxOccupancyCondStr.replace("%Flag%", "maxOccupancyOptions" + '.' + key)
                                maxOccupancyCondStr = maxOccupancyCondStr.replace("%Defs%", shaderCreateTuningOptionsBooleanTemplate.replace("%Flag%", key))
                                maxOccupancyStr += indent(maxOccupancyCondStr, times=2)
                            conditionStr = conditionStr.replace("%MaxOccupancyOptionsEntry%", maxOccupancyStr)

                            for key in LOW_LATENCY_OPTIONS.keys():
                                lowLatencyCondStr = ConditionShaderCreateTuningOptions
                                lowLatencyCondStr = lowLatencyCondStr.replace("%Flag%", "lowLatencyOptions" + '.' + key)
                                lowLatencyCondStr = lowLatencyCondStr.replace("%Defs%", shaderCreateTuningOptionsBooleanTemplate.replace("%Flag%", key))
                                lowLatencyStr += indent(lowLatencyCondStr, times=2)
                            conditionStr = conditionStr.replace("%LowLatencyOptionsEntry%", lowLatencyStr)
                        elif action == "fpControlFlags":
                            fpControlStr = ""
                            for key in FP_CONTROL_FLAGS.keys():
                                fpControlCondStr = ConditionShaderCreateTuningOptions
                                fpControlCondStr = fpControlCondStr.replace("%Flag%", action + '.' + key)
                                fpControlCondStr = fpControlCondStr.replace("%Defs%", shaderCreateTuningOptionsBooleanTemplate.replace("%Flag%", key))
                                fpControlStr += indent(fpControlCondStr)
                            conditionStr = conditionStr.replace("%fpControlOptionsEntry%", fpControlStr)

                        conditionStr = conditionStr.replace("%Flag%", action)
                        cppCode = cppCode + wrapWithDirective(conditionStr, SHADER_ACTION[action]["buildTypes"])
                        break
                    elif entity["parent"] == "dynamicShaderInfo.anonStruct":
                        conditionStr = ConditionDynamicShaderInfoApply.replace("%Defs%", SHADER_ACTION[action]["jsonWriterTemplate"])
                        conditionStr = conditionStr.replace("%Flag%", action)
                        cppCode = cppCode + wrapWithDirective(conditionStr, SHADER_ACTION[action]["buildTypes"])
                        break
                    elif entity["parent"] == "ShaderTuningOptions":
                        conditionStr = ConditionShaderCreateTuningOptions.replace("%Defs%", SHADER_ACTION[action]["jsonWriterTemplate"])
                        conditionStr = conditionStr.replace("%Flag%", action)
                        cppCode = cppCode + wrapWithDirective(conditionStr, entity["buildTypes"])
                        break

    conditionStr = ""
    patternCount = 0
    for pattern in SHADER_PATTERN:
        if (patternCount < len(SHADER_PATTERN) - 1):
            conditionStr = conditionStr + "pattern.shaders[i].match." + pattern + " ||\n"
        else:
            conditionStr = conditionStr + "pattern.shaders[i].match." + pattern
        patternCount += 1

    funcDef = funcDef.replace("%Condition%", indent(conditionStr,times=3))
    funcDef = funcDef.replace("%ShaderCreateApply%", indent(cppCode.strip("\n"), times=3))
    return funcDef

###################################################################################################################
# Build methods to parse a JSON file and apply the read app profile to driver (AT RUNTIME)
###################################################################################################################

def parseJsonProfileEntryPatternRuntime():
    cppCode = ""
    validKeys = ""
    defs = ""
    for key in ENTRIES_TEMPLATE["entries"]["pattern"]:
        validKeys = validKeys + '"' + key + '",\n'
        defs = defs + ConditionParseJsonProfileEntryRuntime.replace("%Key%", key)
        strValue = convertTypeToStrValue(ENTRIES_TEMPLATE["entries"]["pattern"][key]["type"][0])
        if strValue == "dictValue":
            shaderStage = ENTRIES_TEMPLATE["entries"]["pattern"][key]["shaderStage"]
            parseJsonProfileEntryPatternTemplateCode = parseJsonProfileEntryPatternTemplate.replace("%ShaderStage%", shaderStage)
            defs = defs.replace("%Defs%", parseJsonProfileEntryPatternTemplateCode)
        else:
            defs = defs.replace("%Defs%", ENTRIES_TEMPLATE["entries"]["pattern"][key]["jsonReaderTemplate"])
            defs = defs.replace("%Value%", strValue)
    cppCode = ParseJsonProfileEntryPatternFunc.replace("%FuncDefs%", ParseJsonProfileEntryRuntimeFunc)
    cppCode = cppCode.replace("%ValidKeys%", indent(validKeys.rstrip("\n"), times=2))
    cppCode = cppCode.replace("%Defs%", indent(defs.rstrip("\n")))
    return cppCode

def parseJsonProfileEntryActionRuntime():
    cppCode = ""
    validKeys = ""
    defs = ""
    for key in chain(PIPELINE_ACTION, ENTRIES_TEMPLATE["entries"]["action"]):
        validKeys = validKeys + '"' + key + '",\n'
        defs = defs + ConditionParseJsonProfileEntryRuntime.replace("%Key%", key)
        if key in PIPELINE_ACTION:
            strValue = convertTypeToStrValue(PIPELINE_ACTION[key]["type"][0])
            if strValue != "unknownValue":
                defs = defs.replace("%Defs%", PIPELINE_ACTION[key]["jsonReaderTemplate"])

        elif key in ENTRIES_TEMPLATE["entries"]["action"]:
            strValue = convertTypeToStrValue(ENTRIES_TEMPLATE["entries"]["action"][key]["type"][0])
            if strValue == "dictValue":
                shaderStage = ENTRIES_TEMPLATE["entries"]["action"][key]["shaderStage"]
                parseJsonProfileEntryActionTemplateCode = parseJsonProfileEntryActionTemplate.replace("%ShaderStage%", shaderStage)
                defs = defs.replace("%Defs%", parseJsonProfileEntryActionTemplateCode)
            else:
                defs = defs.replace("%Defs%", ENTRIES_TEMPLATE["entries"]["action"][key]["jsonReaderTemplate"])
                defs = defs.replace("%Value%", strValue)

    cppCode = ParseJsonProfileEntryActionFunc.replace("%FuncDefs%", ParseJsonProfileEntryRuntimeFunc)
    cppCode = cppCode.replace("%ValidKeys%", indent(validKeys.rstrip("\n"), times=2))
    cppCode = cppCode.replace("%Defs%", indent(defs.rstrip("\n")))
    return cppCode

def parseJsonProfilePatternShaderRuntime():
    cppCode = ""
    validKeys = ""
    defs = ""
    for pattern in SHADER_PATTERN:
        validKeys = validKeys + '"' + pattern + '",\n'
        defs = defs + ConditionParseJsonProfileEntryRuntime.replace("%Key%", pattern)
        strValue = convertTypeToStrValue(SHADER_PATTERN[pattern]["type"][0])
        conditionBody = SHADER_PATTERN[pattern]["jsonReaderTemplate"]
        conditionBody = conditionBody.replace("%Value%", strValue)
        defs = defs.replace("%Defs%", conditionBody)

    cppCode = ParseJsonProfilePatternShaderFunc.replace("%FuncDefs%", ParseJsonProfileEntryRuntimeFunc)
    cppCode = cppCode.replace("%ValidKeys%", indent(validKeys.rstrip("\n"), times=2))
    cppCode = cppCode.replace("%Defs%", indent(defs.rstrip("\n")))
    return cppCode

def parseJsonProfileActionShaderRuntime():
    cppCode = ""
    validKeys = ""
    defs = ""
    for action in SHADER_ACTION:
        if "jsonReaderTemplate" in SHADER_ACTION[action] and SHADER_ACTION[action]["jsonReadable"]:
            validKeys = validKeys + '"' + action + '",\n'
            conditionBlock = ConditionParseJsonProfileEntryRuntime.replace("%Key%", action)
            strValue = convertTypeToStrValue(SHADER_ACTION[action]["type"][0])
            conditionBody = SHADER_ACTION[action]["jsonReaderTemplate"]
            conditionBody = conditionBody.replace("%Action%", action).replace("%ValueType%", strValue)
            if strValue in TypeValues:
                conditionBody = conditionBody.replace("%Value%", TypeValues[strValue])
            conditionBlock = conditionBlock.replace("%Defs%", conditionBody)
            defs = defs + wrapWithDirective(conditionBlock, SHADER_ACTION[action]["buildTypes"])

    cppCode = ParseJsonProfileActionShaderFunc.replace("%FuncDefs%", ParseJsonProfileEntryRuntimeFunc)
    cppCode = cppCode.replace("%ValidKeys%", indent(validKeys.rstrip("\n"), times=2))
    cppCode = cppCode.replace("%Defs%", indent(defs.rstrip("\n")))
    return cppCode

###################################################################################################################
# Generic functions
###################################################################################################################

def writeToFile(text, filePath):
    open(filePath, 'w').write(text)

def readFromFile(fileToRead):
    try:
        with open(fileToRead, 'r') as file:
            content = file.read()
            dictObj = json.loads(content)
            return dictObj, True

    except Exception as e:
        print("\nException Occurred:\n{0} \nCould not read from file: {1}\n".format(e, fileToRead))
        return "", False

def dedentAll(text):
    tempText = ""
    for line in text.splitlines(True):
        tempText += textwrap.dedent(line)
    return tempText

def convertTypeToStrValue(valType):
    if valType == int:
        return "integerValue"
    elif valType == bool:
        return "booleanValue"
    elif valType == str:
        return "pStringValue"
    elif valType == dict:
        return "dictValue"
    elif valType == list:
        return "listValue"
    else:
        warnings.warn("********** Warning: Type unknown for action. Check 'type' key for action **********\n")
        return "unknownValue"

# Checks if the keys in obj1 are also present in obj2 (list or dict)
def checkValidKeys(obj1, obj2):
    if isinstance(obj1, dict) and isinstance(obj2, dict):
        for key in [*obj1]:
            if key in [*obj2]:
                pass
            else:
                return False
        return True

    elif isinstance(obj1, dict) and isinstance(obj2, list):
        for key in [*obj1]:
            if key in obj2:
                pass
            else:
                return False
        return True

    elif isinstance(obj1, list) and isinstance(obj2, dict):
        for key in obj1:
            if key in [*obj2]:
                pass
            else:
                return False
        return True

def indent(text, **kwargs):
    ch = ' '

    if "n_spaces" in kwargs:
        n_spaces = kwargs["n_spaces"]
    else:
        n_spaces = 4

    if "times" in kwargs:
        times = kwargs["times"]
    else:
        times = 1

    if "width" in kwargs:
        wrapper = textwrap.TextWrapper()
        wrapper.width = kwargs["width"]
        wrapper.initial_indent = n_spaces * times * ch
        wrapper.subsequent_indent = n_spaces * times * ch
        contentList = wrapper.wrap(text)

        for i, line in enumerate(contentList):
            dedentedLine = dedentAll(line)
            if dedentedLine.startswith("#if") or dedentedLine.startswith("#else") or dedentedLine.startswith("#end"):
                contentList[i] = dedentedLine
        return '\n'.join(contentList)

    else:
        padding = n_spaces * times * ch
        content = ''
        for line in text.splitlines(True):
            if line.startswith("#if") or line.startswith("#else") or line.startswith("#end") or line.isspace():
                content = content + dedentAll(line)
            else:
                content = content + padding + line

        return content

def convertToArray(txt):
    txt = txt.replace("[", "{")
    txt = txt.replace("]", "}")
    return txt

def isCompilerOnlyBuildType(buildObj):
    if isinstance(buildObj, dict):
        if len(buildObj) == 1:
            if "andType" in buildObj \
                and len(buildObj["andType"]) == 1 \
                and buildObj["andType"][0] == BuildTypesTemplate["llpc"]:
                return True
    return False

def wrapWithDirective(content, buildObj):
    if isinstance(buildObj, str):
        if buildObj:
            content = "#if "+ buildObj + "\n" + content.strip("\n") + "\n#endif\n"

    elif isinstance(buildObj, dict):
        if "andType" in buildObj:
            if buildObj["andType"]:
                valueIfDefTmp = ""
                valueEndDefTmp = ""
                for directive in buildObj["andType"]:
                    valueIfDefTmp += "#if " + directive + "\n"
                    valueEndDefTmp += "#endif" + "\n"

                content = valueIfDefTmp + content + valueEndDefTmp

        if "orType" in buildObj:
            if buildObj["orType"]:
                valueIfDefTmp = "#if "
                valueEndDefTmp = "#endif\n"
                numOfBuildTypes = len(buildObj["orType"])
                for i in range(numOfBuildTypes):
                    type = buildObj["orType"][i]
                    valueIfDefTmp += type
                    if i < (numOfBuildTypes) -1:
                        valueIfDefTmp += " || "
                    else:
                        valueIfDefTmp += "\n"

                content = valueIfDefTmp + content + valueEndDefTmp

        if "custom" in buildObj:
            if buildObj["custom"]:
                if "startWith" in buildObj["custom"]:
                    startWith = buildObj["custom"]["startWith"]
                    content = startWith + "\n" + content.strip("\n")

                if "endWith" in buildObj["custom"]:
                    endWith = buildObj["custom"]["endWith"]
                    content = content + "\n" + endWith + "\n"
    return content

def retrieveEntityInfo(value, parent):
    success = False
    entityInfo = {}
    listOfEntityInfoObjs = value["entityInfo"]
    if not isinstance(listOfEntityInfoObjs, list):
        listOfEntityInfoObjs = [listOfEntityInfoObjs]
    for entityInfo in listOfEntityInfoObjs:
        if entityInfo["parent"] == parent:
            success = True
            break

    if success:
        return entityInfo
    else:
        return {}

def wrapWithComment(content, comment, secured):
    comment = indent(comment, n_spaces=0, width=110)
    if secured == "true":
        comment = ''.join("//" + "# " + line for line in comment.splitlines(True))
    else:
        comment = ''.join("// " + line for line in comment.splitlines(True))

    if content.startswith("\n"):
        content = comment + content
    else:
        content = comment + "\n" + content
    return content

###################################################################################################################
# Parse all files and generate code
###################################################################################################################

def main():
    shaderProfileDir = ''
    outputDir = ''
    genDir = ''

    if len(sys.argv) >= 2:
        shaderProfileDir = sys.argv[1]
        # if genDir was specified by the user
        if len(sys.argv) == 3:
            genDir = sys.argv[2]
    else:
        print("Error: include directory path in the argument \n"
                "usage: python3 genshaderprofile.py <vulkancodebase>\\xgl\\icd\\api\\appopt\\shader_profiles\\ genDir [optional]")
        return -1

    if not os.path.isabs(shaderProfileDir):
        shaderProfileDir = os.path.abspath(shaderProfileDir)

    splitShaderProfileDir = os.path.split(shaderProfileDir)
    if splitShaderProfileDir[1] == '':
        outputDir = os.path.split(splitShaderProfileDir[0])[0]
    else:
        outputDir = splitShaderProfileDir[0]
    if genDir != "":
        outputDir = genDir

    headerDoxComment = HeaderFileDoxComment.replace("%FileName%", outputFile)

    compilers = os.listdir(shaderProfileDir)

    gameTitleInfo = getGameTitles(AppProfileHeaderFilePath)
    if not gameTitleInfo:
        print("Could Not read 'enum class AppProfile' from app_profile.h. Exiting Program")
        return -1

    funcSetAppProfileGroup = ""
    classShaderProfileBodyDict = {}
    ifGameTitleGroupDict = {}

    for compiler in compilers:
        compilerDir = os.path.join(shaderProfileDir, compiler)
        gfxips = os.listdir(compilerDir)

        gameTitlesList = []
        ifGfxipGroupDict = {}
        ifGenericDict = {}

        if compiler not in classShaderProfileBodyDict:
            classShaderProfileBodyDict[compiler] = ""

        if compiler not in ifGameTitleGroupDict:
            ifGameTitleGroupDict[compiler] = ""

        for gfxip in gfxips:
            gfxipDir = os.path.join(compilerDir, gfxip)

            gameTitlesGfxList = []
            ifAsicGroupDict = {}
            ifAsicGenericDict = {}

            if gfxip != "generic":
                asics = os.listdir(os.path.join(gfxipDir))
            else:
                asics = [gfxip]

            for asic in asics:
                if gfxip != "generic":
                    asicDir = os.path.join(gfxipDir, asic)
                else:
                    asicDir = gfxipDir

                print("Parsing " + asicDir)
                gameTitles = os.listdir(os.path.join(asicDir))
                for title in gameTitles:
                    gameTitleDir = os.path.join(asicDir, title)
                    fileToRead = os.path.join(gfxip, gameTitleDir, configFileName)
                    content, readSuccess = readFromFile(fileToRead)

                    if readSuccess:
                        if title not in gameTitlesList:
                            gameTitlesList.append(title)
                        if title not in gameTitlesGfxList:
                            gameTitlesGfxList.append(title)
                        if title not in ifGfxipGroupDict:
                            ifGfxipGroupDict[title] = ""
                        if title not in ifGenericDict:
                            ifGenericDict[title] = ""
                        if title not in ifAsicGroupDict:
                            ifAsicGroupDict[title] = ""
                        if title not in ifAsicGenericDict:
                            ifAsicGenericDict[title] = ""

                        # for header file: g_shader_profile.h********************************************************
                        funcName = compiler.title() + title + gfxip[0].upper() + gfxip[1:]

                        if gfxip != "generic":
                            if asic != "generic":
                                funcName = compiler.title() + title + asic[0].upper() + asic[1:]
                            else:
                                funcName += asic[0].upper() + asic[1:]

                        funcCompGameGfxAsic = FuncDecSetAppProfile.replace("%FuncName%", funcName)

                        for buildType, obj in gameTitleInfo.items():
                            if title in obj["gameTitles"]:
                                funcCompGameGfxAsic = wrapWithDirective(funcCompGameGfxAsic, obj["buildTypes"])

                        if asic in BuildTypesTemplate:
                            funcCompGameGfxAsic = wrapWithDirective(funcCompGameGfxAsic, BuildTypesTemplate[asic])

                        if gfxip in BuildTypesTemplate:
                            funcCompGameGfxAsic = wrapWithDirective(funcCompGameGfxAsic, BuildTypesTemplate[gfxip])

                        classShaderProfileBodyDict[compiler] += funcCompGameGfxAsic
                        # ********************************************************************************************

                        # for cpp file: g_shader_profile.cpp *********************************************************
                        if asic == "generic":
                            ifAsicGeneric = GenericAsicAppProfile.replace("%FuncName%", funcName)
                            ifAsicGenericDict[title] = ifAsicGeneric
                        else:
                            ifAsic = ConditionAsic.replace("%Asic%", asic[0].upper() + asic[1:])
                            ifAsic = ifAsic.replace("%FuncName%", funcName)
                            if asic in BuildTypesTemplate:
                                ifAsic = wrapWithDirective(ifAsic, BuildTypesTemplate[asic])
                            ifAsicGroupDict[title] = ifAsicGroupDict[title] + ifAsic

                        if gfxip == "generic":
                            ifGeneric = GenericGfxIpAppProfile.replace("%FuncName%", funcName)
                            ifGenericDict[title] = ifGeneric

                        appProfile = genProfile(content, compiler, gfxip)
                        funcSetAppProfile = SetAppProfileFunc.replace("%FuncName%", funcName)
                        funcSetAppProfile = funcSetAppProfile.replace("%FuncDefs%", indent(appProfile))
                        if compiler in BuildTypesTemplate:
                            funcSetAppProfile = wrapWithDirective(funcSetAppProfile, BuildTypesTemplate[compiler])

                        for buildType, obj in gameTitleInfo.items():
                            if title in obj["gameTitles"]:
                                funcSetAppProfile = wrapWithDirective(funcSetAppProfile, obj["buildTypes"])

                        if asic in BuildTypesTemplate:
                            funcSetAppProfile = wrapWithDirective(funcSetAppProfile, BuildTypesTemplate[asic])

                        if gfxip in BuildTypesTemplate:
                            funcSetAppProfile = wrapWithDirective(funcSetAppProfile, BuildTypesTemplate[gfxip])

                        funcSetAppProfileGroup = funcSetAppProfileGroup + funcSetAppProfile
                        # ********************************************************************************************

            # for cpp file: g_shader_profile.cpp******************************************************************
            for title in gameTitlesGfxList:
                if gfxip != "generic":
                    if ifAsicGenericDict[title]:
                        ifGfxipBody = indent(ifAsicGroupDict[title] + ifAsicGenericDict[title])
                    else:
                        ifGfxipBody = indent(ifAsicGroupDict[title])
                    ifGfxip = ConditionGfxIp.replace("%Gfxip%", gfxip[0].upper() + gfxip[1:])
                    ifGfxip = ifGfxip.replace("%Defs%", ifGfxipBody)
                    if gfxip in BuildTypesTemplate:
                        ifGfxip = wrapWithDirective(ifGfxip, BuildTypesTemplate[gfxip])
                    ifGfxipGroupDict[title] = ifGfxipGroupDict[title] + ifGfxip
            # ****************************************************************************************************

        # for cpp file: g_shader_profile.cpp******************************************************************
        for title in gameTitlesList:
            if ifGenericDict[title]:
                ifGameTitleBody = indent(ifGfxipGroupDict[title] + ifGenericDict[title])
            else:
                ifGameTitleBody = indent(ifGfxipGroupDict[title])
            ifGameTitle = ConditionGameTitle.replace("%GameTitle%", title)
            ifGameTitle = ifGameTitle.replace("%Defs%", ifGameTitleBody)
            for buildType, obj in gameTitleInfo.items():
                if title in obj["gameTitles"]:
                    ifGameTitle = wrapWithDirective(ifGameTitle, obj["buildTypes"])
            ifGameTitleGroupDict[compiler] += ifGameTitle
        # ****************************************************************************************************

    ###################################################################################################################
    # Build the Header File
    ###################################################################################################################

    classShaderProfilePrivateDefs = ""
    for compiler in classShaderProfileBodyDict:
        if compiler in BuildTypesTemplate:
            if classShaderProfileBodyDict[compiler] != "":
                classShaderProfilePrivateDefs = classShaderProfilePrivateDefs + "\n" + \
                                    wrapWithDirective(classShaderProfileBodyDict[compiler], BuildTypesTemplate[compiler])
        else:
            classShaderProfilePrivateDefs = classShaderProfilePrivateDefs + classShaderProfileBodyDict[compiler]

    funcDecJsonReader = (
                         FuncDecJsonReader + "\n"
                         )

    classShaderProfilePrivateBody = FuncDecJsonWriter + "\n" + \
                                    wrapWithDirective(funcDecJsonReader, BuildTypesTemplate["icdRuntimeAppProfile"]) + \
                                    classShaderProfilePrivateDefs

    classShaderProfilePublicBody = ( FuncDecClassShaderProfilePublic + "\n" +
                                     wrapWithDirective(FuncDecParseJsonProfile, BuildTypesTemplate["icdRuntimeAppProfile"]) + "\n" +
                                     wrapWithDirective(FuncDecBuildAppProfileLlpc, BuildTypesTemplate["llpc"])
                                   )

    classShaderProfile = ClassTemplate.replace("%ClassName%", "ShaderProfile")
    classShaderProfile = classShaderProfile.replace("%ClassPublicDefs%", indent(classShaderProfilePublicBody))
    classShaderProfile = classShaderProfile.replace("%ClassPrivateDefs%", indent(classShaderProfilePrivateBody))

    content = createStructAndVarDefinitions(ShaderTuningStructsAndVars)
    namespaceBody = content + "\n" + classShaderProfile
    headerBody = NamespaceVK.replace("%NamespaceDefs%", namespaceBody)

    headerContent = CopyrightAndWarning + headerDoxComment + HeaderIncludes + "\n" + headerBody
    headerFilePath = os.path.join(outputDir, headerFileName)
    writeToFile(headerContent, headerFilePath)

    ###################################################################################################################
    # Build the Source File
    ###################################################################################################################

    if "llpc" in ifGameTitleGroupDict:
        funcBuildAppProfileLlpc = BuildAppProfileLlpcFunc.replace("%FuncDefs%",
                                                                        indent(ifGameTitleGroupDict["llpc"].rstrip("\n")))
        funcBuildAppProfileLlpc = wrapWithDirective(funcBuildAppProfileLlpc, BuildTypesTemplate["llpc"])
    else:
        funcBuildAppProfileLlpc = ""

    funcProfileEntryActionToJson = buildProfileEntryActionToJson()
    funcProfileEntryPatternToJson = buildProfileEntryPatternToJson()
    funcJsonWriter = JsonWriterGenericDef + \
                            "\n" + \
                            funcProfileEntryPatternToJson + \
                            "\n" + \
                            funcProfileEntryActionToJson

    funcJsonReader = (JsonReaderGenericDef + "\n" +
                      parseJsonProfileEntryPatternRuntime() + "\n" +
                      parseJsonProfileEntryActionRuntime() + "\n" +
                      parseJsonProfilePatternShaderRuntime() + "\n" +
                      parseJsonProfileActionShaderRuntime() + "\n"
                    )

    cppBody = NamespaceVK.replace("%NamespaceDefs%", funcBuildAppProfileLlpc
                                                     + "\n"
                                                     + funcSetAppProfileGroup
                                                     + "\n"
                                                     + funcJsonWriter
                                                     + "\n"
                                                     + wrapWithDirective(funcJsonReader,
                                                                         BuildTypesTemplate["icdRuntimeAppProfile"])
                                                     )

    includeStr = ""

    CppIncludes = CppInclude.replace("%Includes%", includeStr)

    cppContent = CopyrightAndWarning + CppIncludes + cppBody
    cppFilePath = os.path.join(outputDir, sourceFileName)
    writeToFile(cppContent, cppFilePath)
    return 0

if __name__ == '__main__':
    if sys.version_info[:2] < (3, 6):
        raise Exception("Python 3.6 (CPython) or a more recent python version is required.")

    print("Generating shader profiles code ")

    result = main()

    if not result:
        print("Finished generating " + headerFileName + " and " + sourceFileName)
    else:
        print("Error: Exiting without code generation. Driver code compilation will fail.")
        exit(1)
