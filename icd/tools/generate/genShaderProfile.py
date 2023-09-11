#!/usr/bin/env python3
##
 #######################################################################################################################
 #
 #  Copyright (c) 2020-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

"""
xlg helper script to generate shader profiles.
"""

import json
import os
import sys
import textwrap
import warnings
import pathlib
from itertools import chain

from shaderProfileTemplate import SHADER_PATTERN, ENTRIES_TEMPLATE, SHADER_ACTION, BRANCHES, PIPELINE_ACTION, \
    INCREMENT_ENTRY_TEMPLATE, UNINITIALIZED_VAR_TEMPLATE, INITIALIZED_VAR_TEMPLATE, \
    ValidKeysForEntity, STRUCT_TEMPLATE, UNION_TEMPLATE, BIT_FIELD_VAR_TEMPLATE, INITIALIZED_ARR_TEMPLATE, \
    CONDITION_SHADER_MATCH_PATTERN, PROFILE_ENTRY_PATTERN_TO_JSON_FUNC, CONDITION_CREATE_INFO_APPLY, \
    PROFILE_ENTRY_ACTION_TO_JSON_FUNC, CONDITION_SHADER_CREATE_APPLY, CONDITION_SHADER_CREATE_TUNING_OPTIONS, \
    SHADER_CREATE_TUNING_OPTIONS_BOOLEAN_TEMPLATE, UNINITALIZED_ARR_TEMPLATE, PARSE_JSON_SHADER_TUNING_FLAGS_FUNC, \
    PARSE_JSON_SHADER_TUNING_OPTIONS_FUNC, CONDITION_PARSE_JSON_PROFILE_ENTRY_RUNTIME,\
    PARSE_JSON_PROFILE_ENTRY_PATTERN_TEMPLATE, PARSE_JSON_PROFILE_ENTRY_PATTERN_FUNC, \
    PARSE_JSON_PROFILE_ENTRY_RUNTIME_FUNC, PARSE_JSON_PROFILE_ENTRY_ACTION_TEMPLATE, \
    PARSE_JSON_PROFILE_ENTRY_ACTION_FUNC, PARSE_JSON_PROFILE_PATTERN_SHADER_FUNC, TypeValues, \
    PARSE_JSON_PROFILE_ACTION_SHADER_FUNC, BuildTypesTemplate, HEADER_FILE_DOX_COMMENT, AppProfileHeaderFilePath, \
    FUNC_DEC_SET_APP_PROFILE, GENERIC_ASIC_APP_PROFILE, CONDITION_ASIC, GENERIC_GFX_IP_APP_PROFILE, \
    SET_APP_PROFILE_FUNC, CONDITION_GFX_IP, CONDITION_GAME_TITLE, FUNC_DEC_JSON_READER, FUNC_DEC_JSON_WRITER, \
    FUN_DEC_CLASS_SHADER_PROFILE_PUBLIC, FUNC_DEC_PARSE_JSON_PROFILE, FUNC_DEC_BUILD_APP_PROFILE_LLPC, \
    BUILD_APP_PROFILE_LLPC_FUNC, JSON_WRITER_GENERIC_DEF, JSON_READER_GENERIC_DEF, NAMESPACE_VK, CPP_INCLUDE, \
    CopyrightAndWarning, CONDITION_DYNAMIC_SHADER_INFO_APPLY, CLASS_TEMPLATE, ShaderTuningStructsAndVars, \
    HEADER_INCLUDES, PARSE_DWORD_ARRAY_FUNC, CONDITION_SHADER_CREATE_TUNING_OPTION_FLAGS

OUTPUT_FILE = "g_shader_profile"
CONFIG_FILE_NAME = "profile.json"
HEADER_FILE_NAME = OUTPUT_FILE + ".h"
SOURCE_FILE_NAME = OUTPUT_FILE + ".cpp"

###################################################################################################################
# Functions to parse app profiles from JSON files and convert to C++ structures and functions (AT COMPILE TIME)
###################################################################################################################

def parse_json_profile_pattern_shader(shader_patterns: dict) -> (bool, str):
    """
    Parses stage patterns from the input json file and fetches code template from shaderProfileTemplate.py.
    :param shader_patterns: The input json.
    :return: A bool indicating if the parsing succeeded, and the parsed pattern.
    """
    success = check_valid_keys(shader_patterns, SHADER_PATTERN)
    code_shader_pattern = ""

    if success:
        for shader_pattern_key, shader_pattern_value in shader_patterns.items():

            if type(shader_pattern_value) in SHADER_PATTERN[shader_pattern_key]["type"]:
                success &= True

            else:
                success &= False
                # pylint: disable=consider-using-f-string
                warnings.warn("********** Warning: Type Mismatch for shader_pattern **********\n"
                              "Parsed Stage Pattern key: {0}\n"
                              "Parsed Stage Pattern value: {1}\n"
                              "Parsed Stage Pattern value type: {2}\n"
                              "Expected value type: {3}\n".format(shader_pattern_key,
                                                                  shader_pattern_value,
                                                                  type(shader_pattern_value),
                                                                  ENTRIES_TEMPLATE["entries"]["pattern"][
                                                                      shader_pattern_key]["type"]))

            cpp_code = SHADER_PATTERN[shader_pattern_key]["codeTemplate"]
            if shader_pattern_key == "stageActive":
                cpp_code = cpp_code.replace("%Value%", str(shader_pattern_value).lower())

            elif shader_pattern_key == "stageInactive":
                cpp_code = cpp_code.replace("%Value%", str(shader_pattern_value).lower())

            elif shader_pattern_key == "codeHash":
                code_hash = str(shader_pattern_value).split(' ')
                value_upper = (code_hash[0][2:]).zfill(16).upper()
                value_lower = code_hash[1].zfill(16).upper()
                cpp_code = cpp_code.replace("%valueLower%", value_lower)
                cpp_code = cpp_code.replace("%valueUpper%", value_upper)

            elif shader_pattern_key == "codeSizeLessThan":
                cpp_code = cpp_code.replace("%Value%", str(shader_pattern_value).lower())

            code_shader_pattern = code_shader_pattern + cpp_code

        return success, code_shader_pattern

    print("************Parsing failed****************")
    return success, code_shader_pattern

def parse_json_profile_entry_pattern(pattern: dict):
    """
    Parses patterns from the input json file and fetches code template from shaderProfileTemplate.py.
    :param pattern: The input json.
    :return: A bool indicating if the parsing succeeded, and the parsed pattern.
    """
    success = check_valid_keys(pattern, ENTRIES_TEMPLATE["entries"]["pattern"])
    code_pattern = ""
    if success:
        for pattern_key, pattern_value in pattern.items():
            cpp_code = ""
            if type(pattern_value) in ENTRIES_TEMPLATE["entries"]["pattern"][pattern_key]["type"]:
                success &= True
            else:
                success &= False
                # pylint: disable=consider-using-f-string
                warnings.warn("********** Warning: Type Mismatch for pattern **********\n"
                              "Parsed Pattern key: {0}\n"
                              "Parsed Pattern value: {1}\n"
                              "Parsed Pattern value type: {2}\n"
                              "Expected value type: {3}\n".format(pattern_key,
                                                                  pattern_value,
                                                                  type(pattern_value),
                                                                  ENTRIES_TEMPLATE["entries"]["pattern"][pattern_key][
                                                                      "type"]))

            if pattern_key in ["always",
                              "shaderOnly"]:

                cpp_code = ENTRIES_TEMPLATE["entries"]["pattern"][pattern_key]["codeTemplate"]
                cpp_code = cpp_code.replace("%Value%", str(pattern_value).lower())

            elif pattern_key in ["vs",
                                "hs",
                                "ds",
                                "gs",
                                "ps",
                                "cs"]:

                success, cpp_code = parse_json_profile_pattern_shader(pattern_value)
                shader_stage = ENTRIES_TEMPLATE["entries"]["pattern"][pattern_key]["shaderStage"]
                cpp_code = cpp_code.replace("%ShaderStage%", shader_stage)

            if success is False:
                return False, code_pattern
            code_pattern = code_pattern + cpp_code

        return success, code_pattern

    print("************ Parsing failed ****************")
    return success, code_pattern

def parse_json_flags(key, flags):
    """
    TODO.
    :param key: TODO.
    :param flags: TODO.
    :return: TODO.
    """
    cpp_code = ""
    return success, cpp_code

def parse_json_profile_action_shader(shader_actions):
    """
    Parses stage actions from the input json file and fetches code template from shaderProfileTemplate.py.
    Includes parsing options for
    [
        'optStrategyFlags', 'optStrategyFlags2', 'vgprLimit', 'sgprLimit', 'ldsSpillLimitDwords',
        'maxArraySizeForFastDynamicIndexing',
        'userDataSpillThreshold', 'maxThreadGroupsPerComputeUnit', 'scOptions', 'scOptionsMask', 'trapPresent',
        'debugMode', 'allowReZ', 'shaderReplaceEnabled', 'fpControlFlags', 'optimizationIntent', 'disableLoopUnrolls',
        'enableSelectiveInline', 'maxOccupancyOptions', 'lowLatencyOptions', 'waveSize', 'wgpMode', 'waveBreakSize',
        'nggDisable', 'nggFasterLaunchRate', 'nggVertexReuse', 'nggEnableFrustumCulling', 'nggEnableBoxFilterCulling',
        'nggEnableSphereCulling', 'nggEnableBackfaceCulling', 'nggEnableSmallPrimFilter', 'enableSubvector',
        'enableSubvectorSharedVgprs', 'maxWavesPerCu', 'maxThreadGroupsPerCu', 'useSiScheduler',
        'disableCodeSinking', 'favorLatencyHiding', 'reconfigWorkgroupLayout', 'forceLoopUnrollCount',
        'enableLoadScalarizer', 'disableLicm', 'unrollThreshold', 'nsaThreshold', 'aggressiveInvariantLoads',
        'scalarizeWaterfallLoads'
    ]
    :param shader_actions:
    :return:
    """
    result_ret = {'success': check_valid_keys(shader_actions, SHADER_ACTION)}

    for branch in BRANCHES:
        if branch not in result_ret:
            result_ret[branch] = False

    code_shader_action = ""

    if not result_ret['success']:
        print("************Parsing failed****************")

        for key in shader_actions:
            if key not in SHADER_ACTION:
                raise ValueError("Failed to parse shader action : " + str(key))

        return result_ret, code_shader_action

    for shader_action_key, shader_action_value in shader_actions.items():
        if not type(shader_action_value) in SHADER_ACTION[shader_action_key]["type"]:
            result_ret['success'] = False
            # pylint: disable=consider-using-f-string
            warnings.warn("********** Error: Type Mismatch for shader action **********\n"
                          "Parsed Stage Action Key: {0}\n"
                          "Parsed Stage Action value: {1}\n"
                          "Parsed Stage Action Value type: {2}\n"
                          "Expected value type: {3}".format(shader_action_key,
                                                            shader_action_value,
                                                            type(shader_action_value),
                                                            SHADER_ACTION[shader_action_key]["type"]))
            raise ValueError("{0} is not {1} type with {2} value.".format(shader_action_key,
                                                                          type(shader_action_value),
                                                                          shader_action_value))

        if shader_action_key in BRANCHES:
            result_ret[shader_action_key] = True

        if isinstance(shader_action_value, (bool, int, list, str)):
            if "codeTemplate" in SHADER_ACTION[shader_action_key]:
                cpp_code = SHADER_ACTION[shader_action_key]["codeTemplate"]\
                    .replace("%FieldName%", str(shader_action_key)) \
                    .replace("%IntValue%", str(shader_action_value).lower()) \
                    .replace("%ListValue%", convert_to_array(str(shader_action_value))) \
                    .replace("%StrValue%", str(shader_action_value)) \
                    .replace("%BoolValue%", str(shader_action_value).lower())
                # Need to special-case this, as otherwise the validValues field may not be present
                if "%EnumValue%" in cpp_code:
                    cpp_code = cpp_code.replace(
                        "%EnumValue%", str(SHADER_ACTION[shader_action_key]["validValues"][shader_action_value])) \

            else:
                continue
        else:
            # should be a dictionary type
            success, cpp_code = parse_json_flags(shader_action_key, shader_action_value)
            result_ret['success'] |= success

        # wrap with directive only if the buildType dictionary does not contain only a compiler related build type
        if "buildTypes" in SHADER_ACTION[shader_action_key] \
                and len(SHADER_ACTION[shader_action_key]["buildTypes"]) != 0 \
                and not is_compiler_only_build_type(SHADER_ACTION[shader_action_key]["buildTypes"]):
            cpp_code = wrap_with_directive(cpp_code, SHADER_ACTION[shader_action_key]["buildTypes"])

        code_shader_action = code_shader_action + cpp_code
    return result_ret, code_shader_action

def parse_json_profile_entry_action(action):
    """
    Parses actions from the input json file and fetches code template from shaderProfileTemplate.py
    :param action:
    :return:
    """
    result_ret = {'success': False}

    for branch in BRANCHES:
        if branch not in result_ret:
            result_ret[branch] = False

    for action_key in action:
        if not (action_key in ENTRIES_TEMPLATE["entries"]["action"] or action_key in PIPELINE_ACTION):
            print("************ Parsing failed ****************")
            return result_ret, ""

    code_action = ""
    for action_key, action_value in action.items():
        cpp_code = ""

        if not ((action_key in ENTRIES_TEMPLATE["entries"]["action"] and type(action_value) in
                 ENTRIES_TEMPLATE["entries"]["action"][action_key]["type"]) or (
                        action_key in PIPELINE_ACTION and type(action_value) in PIPELINE_ACTION[action_key]["type"])):
            warnings.warn("********** Warning: Type Mismatch for action **********\n")
            return result_ret, code_action

        result_ret['success'] = True

        if action_key in ["vs",
                          "hs",
                          "ds",
                          "gs",
                          "ps",
                          "cs"]:
            action_result, cpp_code = parse_json_profile_action_shader(action_value)
            result_ret = action_result
            shader_stage = ENTRIES_TEMPLATE["entries"]["action"][action_key]["shaderStage"]
            cpp_code = cpp_code.replace("%ShaderStage%", shader_stage)

        else:
            if action_key in PIPELINE_ACTION:
                cpp_code = PIPELINE_ACTION[action_key]["codeTemplate"]
                if "validValues" in PIPELINE_ACTION[action_key]:
                    value = PIPELINE_ACTION[action_key]["validValues"][action_value]
                    cpp_code = cpp_code.replace("%EnumValue%", value)
                else:
                    cpp_code = cpp_code.replace("%Value%", str(action_value))

        code_action = code_action + cpp_code
    return result_ret, code_action

def gen_profile(input_json, compiler):
    """
    Takes the entire json object as input, fetches corresponding code template from shaderProfileTemplate.py,
    manipulates it according to the tuning parameters present in the json file, and finally returns a block of code
    that is going to reside inside g_shader_profile.cpp.
    :param input_json:
    :param compiler:
    :return: Code to build the shader profile in g_shader_profile.cpp.
    """
    entries = input_json["entries"]
    cpp_code = ""
    result_ret = {'success': False}

    for branch in BRANCHES:
        if branch not in result_ret:
            result_ret[branch] = False

    if len(entries) != 0:
        for entry in entries:
            if check_valid_keys(entry, ENTRIES_TEMPLATE["entries"]):
                pattern = entry["pattern"]
                action = entry["action"]

                success, cpp_pattern = parse_json_profile_entry_pattern(pattern)
                if not success:
                    raise ValueError("JSON parsing failed")
                action_result, cpp_action = parse_json_profile_entry_action(action)
                for branch, result in action_result.items():
                    if result:
                        result_ret[branch] = True

                cpp_code = cpp_code + INCREMENT_ENTRY_TEMPLATE + cpp_pattern + cpp_action + "\n"
                cpp_code = cpp_code.replace("%EntryNum%", 'i')
            else:
                print("************ Parsing failed ****************")

        var_template = ""

        var = UNINITIALIZED_VAR_TEMPLATE.replace("%DataType%", 'uint32_t')
        var = var.replace("%VarName%", 'i')
        var_template = var_template + var + "\n"

        cpp_code = var_template + cpp_code

    return dedent_all(cpp_code.rstrip("\n"))

def create_struct_and_var_definitions(dict_objects, parent=None):
    """
    recursive function
    :param dict_objects:
    :param parent:
    :return:
    """
    content_all = ''
    if not isinstance(dict_objects, list):
        dict_objects = [dict_objects]
    for dict_object in dict_objects:
        content = ''
        for key, value in dict_object.items():
            if "entityInfo" in value:
                # fetch entityInfo with the given parent name
                value = retrieve_entity_info(value, parent)
                if not value:
                    continue

            if "entity" in value:
                success = check_valid_keys(ValidKeysForEntity[value["entity"]], value)
                template = ''
                if success:
                    if value["entity"] == "struct":
                        if value["structName"] != "":
                            template = STRUCT_TEMPLATE.replace("%StructName%", " " + value["structName"])
                        else:
                            template = STRUCT_TEMPLATE.replace("%StructName%", value["structName"])

                        template = template.replace("%StructObj%", value["objectName"])
                        if value["buildTypes"]:
                            template = wrap_with_directive(template, value["buildTypes"])
                        if value["child"]:
                            struct_body = create_struct_and_var_definitions(value["child"], parent=key)
                        else:
                            struct_body = ''

                        template = template.replace("%StructDefs%", indent(struct_body))

                    if value["entity"] == "union":
                        if value["unionName"] != "":
                            template = STRUCT_TEMPLATE.replace("%UnionName%", " " + value["unionName"])
                        else:
                            template = UNION_TEMPLATE.replace("%UnionName%", value["unionName"])

                        template = template.replace("%UnionObj%", value["objectName"])
                        if value["buildTypes"]:
                            template = wrap_with_directive(template, value["buildTypes"])
                        if value["child"]:
                            union_body = create_struct_and_var_definitions(value["child"], parent=key)
                        else:
                            union_body = ''

                        template = template.replace("%UnionDefs%", indent(union_body))

                    if value["entity"] == "var":
                        # Initialized Variable
                        if value["defaultValue"]:
                            template = INITIALIZED_VAR_TEMPLATE.replace("%DataType%", value["dataType"])
                            template = template.replace("%VarName%", value["varName"])
                            template = template.replace("%DefaultValue%", str(value["defaultValue"]))
                        # Uninitialized variable
                        else:
                            template = UNINITIALIZED_VAR_TEMPLATE.replace("%DataType%", value["dataType"])
                            template = template.replace("%VarName%", value["varName"])

                        if value["buildTypes"]:
                            template = wrap_with_directive(template, value["buildTypes"])

                    if value["entity"] == "bitField":
                        template = BIT_FIELD_VAR_TEMPLATE.replace("%DataType%", value["dataType"])
                        template = template.replace("%VarName%", value["varName"])
                        template = template.replace("%DefaultValue%", str(value["defaultValue"]))

                        if value["buildTypes"]:
                            template = wrap_with_directive(template, value["buildTypes"])

                    if value["entity"] == "array":
                        # initialized array
                        if value["arrayValue"]:
                            template = INITIALIZED_ARR_TEMPLATE.replace("%DataType%", value["dataType"])
                            template = template.replace("%VarName%", value["varName"])
                            template = template.replace("%ArrSize%", value["arraySize"])
                            template = template.replace("%ArrValue%", value["arrayValue"])

                        # Uninitialized array
                        else:
                            template = UNINITALIZED_ARR_TEMPLATE.replace("%DataType%", value["dataType"])
                            template = template.replace("%VarName%", value["varName"])
                            template = template.replace("%ArrSize%", value["arraySize"])

                        if value["buildTypes"]:
                            template = wrap_with_directive(template, value["buildTypes"])

                    if "description" in value:
                        if "secured" in value:
                            template = wrap_with_comment(template, value["description"], value["secured"])
                        else:
                            template = wrap_with_comment(template, value["description"], "false")

                    content = content + template
                else:
                    print("************ Parsing failed ****************")
        content_all = content_all + content

    return content_all.strip("\n")

def get_game_titles(file_path):
    """
    Reads app_profile.h from its location in the driver, and parses the AppProfile Class to retrieve the names of game
    titles and their build type.
    :param: file_path:
    :return: A dictionary of the form
    {
        "released": {
            "gameTitles": [],
            "buildTypes": {"andType": []}
        },
    }
    """
    with open(file_path, encoding="utf-8") as file:
        content = file.readlines()
        app_profile_content = None

        start = -1
        for i, line in enumerate(content):
            if line.startswith("enum class AppProfile"):
                start = i + 1
            elif ("};" in line) and (start >= 0):
                app_profile_content = content[start:i]
                break

    if app_profile_content is not None:
        game_title_info = {
            "released": {
                "gameTitles": [],
                "buildTypes": {"andType": []}
            }
        }

        has_build_type = False
        directive = ""
        for i, title in enumerate(app_profile_content):
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
                has_build_type = True
                directive = title.strip("#if ")
                game_title_info[directive] = {
                    "gameTitles": [],
                    "buildTypes": {"andType": [directive]}
                }
                continue

            if ("#end" in title) or ("#else" in title):
                has_build_type = False
                continue

            if has_build_type:
                game_title_info[directive]["gameTitles"].append(title)
            else:
                game_title_info["released"]["gameTitles"].append(title)

        return game_title_info

    return {}

###################################################################################################################
# Build methods to dump Pipeline profile of a specific (currently running) app to a JSON file
###################################################################################################################

def build_profile_entry_pattern_to_json():
    """
    TODO.
    :return: TODO.
    """
    condition_str = ""
    defs = ""
    pattern_count = 0
    for pattern, value in SHADER_PATTERN.items():
        if pattern_count < len(SHADER_PATTERN) - 1:
            condition_str = condition_str + "shader.match." + pattern + " ||\n"
        else:
            condition_str = condition_str + "shader.match." + pattern

        defs = defs + CONDITION_SHADER_MATCH_PATTERN.replace("%Pattern%", pattern)
        defs = defs.replace("%Defs%", value["jsonWriterTemplate"])
        pattern_count += 1

    cpp_code = PROFILE_ENTRY_PATTERN_TO_JSON_FUNC.replace("%Condition%", indent(condition_str, times=3))
    cpp_code = cpp_code.replace("%Defs%", indent(defs, times=3))
    return cpp_code

def build_profile_entry_action_to_json():
    """
    Iterates over SHADER_ACTION but dumps only the keys/actions that are declared in ShaderTuningOptions,
    dynamicShaderInfo and shaderCreate structures. This essentially means that this key in SHADER_ACTION should have at
    least one of these in the parent field in entityInfo
    :return:
    """
    cpp_code = ""
    for action, value in PIPELINE_ACTION.items():
        for entity in value["entityInfo"]:
            if entity["parent"] == "createInfo.anonStruct":
                condition_str = CONDITION_CREATE_INFO_APPLY.replace("%Defs%", value["jsonWriterTemplate"])
                condition_str = condition_str.replace("%Flag%", action)
                cpp_code = cpp_code + condition_str

    func_def = PROFILE_ENTRY_ACTION_TO_JSON_FUNC.replace("%CreateInfoApply%", indent(cpp_code.strip("\n")))

    cpp_code = ""
    for action, value in SHADER_ACTION.items():
        if "jsonWriterTemplate" in value:
            for entity in value["entityInfo"]:
                if "jsonWritable" in entity and entity["jsonWritable"]:
                    if entity["parent"] == "shaderCreate.anonStruct":
                        condition_str = CONDITION_SHADER_CREATE_APPLY.replace("%Defs%", value["jsonWriterTemplate"])

                        if action == "optStrategyFlags":
                            opt_strategy_str = ""
                            for key in OPT_STRATEGY_FLAGS:
                                opt_flag_cond_str = CONDITION_SHADER_CREATE_TUNING_OPTION_FLAGS
                                opt_flag_cond_str = opt_flag_cond_str.replace(
                                    "%Flag%", "flags" + " & " + OPT_STRATEGY_FLAGS[key]["flagName"] + ") != 0")
                                opt_flag_cond_str = opt_flag_cond_str.replace(
                                    "%Defs%", SHADER_CREATE_TUNING_OPTIONS_BOOLEAN_TEMPLATE.replace("%Flag%", key))
                                opt_strategy_str += indent(opt_flag_cond_str)
                            condition_str = condition_str.replace("%OptStrategyEntry%", opt_strategy_str)
                        elif action == "optStrategyFlags2":
                            opt_strategy2_str = ""
                            for key in OPT_STRATEGY_FLAGS2:
                                opt_flag2_cond_str = CONDITION_SHADER_CREATE_TUNING_OPTION_FLAGS
                                opt_flag2_cond_str = opt_flag2_cond_str.replace(
                                    "%Flag%", "flags2" + " & " + OPT_STRATEGY_FLAGS2[key]["flagName"] + ") != 0")
                                opt_flag2_cond_str = opt_flag2_cond_str.replace(
                                    "%Defs%", SHADER_CREATE_TUNING_OPTIONS_BOOLEAN_TEMPLATE.replace("%Flag%", key))
                                opt_strategy2_str += indent(opt_flag2_cond_str)
                            condition_str = condition_str.replace("%OptStrategy2Entry%", opt_strategy2_str)

                        elif action == "optimizationIntent":
                            max_occupancy_str = ""
                            low_latency_str = ""

                            for key in MAX_OCCUPANCY_OPTIONS:
                                max_occupancy_cond_str = CONDITION_SHADER_CREATE_TUNING_OPTION_FLAGS
                                max_occupancy_cond_str = max_occupancy_cond_str.replace(
                                    "%Flag%",
                                    "maxOccupancyOptions" + " & " + MAX_OCCUPANCY_OPTIONS[key]["flagName"] + ") != 0")
                                max_occupancy_cond_str = max_occupancy_cond_str.replace(
                                    "%Defs%", SHADER_CREATE_TUNING_OPTIONS_BOOLEAN_TEMPLATE.replace("%Flag%", key))
                                max_occupancy_str += indent(max_occupancy_cond_str, times=2)
                            condition_str = condition_str.replace("%MaxOccupancyOptionsEntry%", max_occupancy_str)

                            for key in LOW_LATENCY_OPTIONS:
                                low_latency_cond_str = CONDITION_SHADER_CREATE_TUNING_OPTION_FLAGS
                                low_latency_cond_str = low_latency_cond_str.replace(
                                    "%Flag%",
                                    "lowLatencyOptions" + " & " + LOW_LATENCY_OPTIONS[key]["flagName"] + ") != 0")
                                low_latency_cond_str = low_latency_cond_str.replace(
                                    "%Defs%", SHADER_CREATE_TUNING_OPTIONS_BOOLEAN_TEMPLATE.replace("%Flag%", key))
                                low_latency_str += indent(low_latency_cond_str, times=2)
                            condition_str = condition_str.replace("%LowLatencyOptionsEntry%", low_latency_str)
                        elif action == "fpControlFlags":
                            fp_control_str = ""
                            for key in FP_CONTROL_FLAGS:
                                fp_control_cond_str = CONDITION_SHADER_CREATE_TUNING_OPTION_FLAGS
                                fp_control_cond_str = fp_control_cond_str.replace(
                                    "%Flag%", action + " & " + FP_CONTROL_FLAGS[key]["flagName"] + ") != 0")
                                fp_control_cond_str = fp_control_cond_str.replace(
                                    "%Defs%", SHADER_CREATE_TUNING_OPTIONS_BOOLEAN_TEMPLATE.replace("%Flag%", key))
                                fp_control_str += indent(fp_control_cond_str)
                            condition_str = condition_str.replace("%fpControlOptionsEntry%", fp_control_str)

                        condition_str = condition_str.replace("%Flag%", action)
                        cpp_code = cpp_code + wrap_with_directive(condition_str, value["buildTypes"])
                        break
                    if entity["parent"] == "dynamicShaderInfo.anonStruct":
                        condition_str = CONDITION_DYNAMIC_SHADER_INFO_APPLY.replace("%Defs%",
                                                                                    value["jsonWriterTemplate"])
                        condition_str = condition_str.replace("%Flag%", action)
                        cpp_code = cpp_code + wrap_with_directive(condition_str, value["buildTypes"])
                        break
                    if entity["parent"] == "ShaderTuningOptions":
                        condition_str = CONDITION_SHADER_CREATE_TUNING_OPTIONS.replace("%Defs%",
                                                                                       value["jsonWriterTemplate"])
                        condition_str = condition_str.replace("%Flag%", action)
                        cpp_code = cpp_code + wrap_with_directive(condition_str, entity["buildTypes"])
                        break

    condition_str = ""
    pattern_count = 0
    for pattern in SHADER_PATTERN:
        if pattern_count < len(SHADER_PATTERN) - 1:
            condition_str = condition_str + "pattern.shaders[i].match." + pattern + " ||\n"
        else:
            condition_str = condition_str + "pattern.shaders[i].match." + pattern
        pattern_count += 1

    func_def = func_def.replace("%Condition%", indent(condition_str, times=3))
    func_def = func_def.replace("%ShaderCreateApply%", indent(cpp_code.strip("\n"), times=3))
    return func_def

def parse_json_profile_entry_pattern_runtime():
    """
    TODO.
    :return: TODO.
    """
    valid_keys = ""
    defs = ""
    for key in ENTRIES_TEMPLATE["entries"]["pattern"]:
        valid_keys = valid_keys + '"' + key + '",\n'
        defs = defs + CONDITION_PARSE_JSON_PROFILE_ENTRY_RUNTIME.replace("%Key%", key)
        str_value = convert_type_to_str_value(ENTRIES_TEMPLATE["entries"]["pattern"][key]["type"][0])
        if str_value == "dictValue":
            shader_stage = ENTRIES_TEMPLATE["entries"]["pattern"][key]["shaderStage"]
            parse_json_profile_entry_pattern_template_code = PARSE_JSON_PROFILE_ENTRY_PATTERN_TEMPLATE.replace(
                "%ShaderStage%", shader_stage)
            defs = defs.replace("%Defs%", parse_json_profile_entry_pattern_template_code)
        else:
            defs = defs.replace("%Defs%", ENTRIES_TEMPLATE["entries"]["pattern"][key]["jsonReaderTemplate"])
            defs = defs.replace("%Value%", str_value)
    cpp_code = PARSE_JSON_PROFILE_ENTRY_PATTERN_FUNC.replace("%FuncDefs%", PARSE_JSON_PROFILE_ENTRY_RUNTIME_FUNC)
    cpp_code = cpp_code.replace("%ValidKeys%", indent(valid_keys.rstrip("\n"), times=2))
    cpp_code = cpp_code.replace("%Defs%", indent(defs.rstrip("\n")))
    return cpp_code

def parse_json_profile_entry_action_runtime():
    """
    TODO.
    :return: TODO.
    """
    valid_keys = ""
    defs = ""
    for key in chain(PIPELINE_ACTION, ENTRIES_TEMPLATE["entries"]["action"]):
        valid_keys = valid_keys + '"' + key + '",\n'
        defs = defs + CONDITION_PARSE_JSON_PROFILE_ENTRY_RUNTIME.replace("%Key%", key)
        if key in PIPELINE_ACTION:
            str_value = convert_type_to_str_value(PIPELINE_ACTION[key]["type"][0])
            if str_value != "unknownValue":
                defs = defs.replace("%Defs%", PIPELINE_ACTION[key]["jsonReaderTemplate"])

        elif key in ENTRIES_TEMPLATE["entries"]["action"]:
            str_value = convert_type_to_str_value(ENTRIES_TEMPLATE["entries"]["action"][key]["type"][0])
            if str_value == "dictValue":
                shader_stage = ENTRIES_TEMPLATE["entries"]["action"][key]["shaderStage"]
                parse_json_profile_entry_action_template_code = PARSE_JSON_PROFILE_ENTRY_ACTION_TEMPLATE.replace(
                    "%ShaderStage%", shader_stage)
                defs = defs.replace("%Defs%", parse_json_profile_entry_action_template_code)
            else:
                defs = defs.replace("%Defs%", ENTRIES_TEMPLATE["entries"]["action"][key]["jsonReaderTemplate"])
                defs = defs.replace("%Value%", str_value)

    cpp_code = PARSE_JSON_PROFILE_ENTRY_ACTION_FUNC.replace("%FuncDefs%", PARSE_JSON_PROFILE_ENTRY_RUNTIME_FUNC)
    cpp_code = cpp_code.replace("%ValidKeys%", indent(valid_keys.rstrip("\n"), times=2))
    cpp_code = cpp_code.replace("%Defs%", indent(defs.rstrip("\n")))
    return cpp_code

def parse_json_profile_pattern_shader_runtime():
    """
    TODO.
    :return: TODO.
    """
    valid_keys = ""
    defs = ""
    for pattern, value in SHADER_PATTERN.items():
        valid_keys = valid_keys + '"' + pattern + '",\n'
        defs = defs + CONDITION_PARSE_JSON_PROFILE_ENTRY_RUNTIME.replace("%Key%", pattern)
        str_value = convert_type_to_str_value(value["type"][0])
        condition_body = value["jsonReaderTemplate"]
        condition_body = condition_body.replace("%Value%", str_value)
        defs = defs.replace("%Defs%", condition_body)

    cpp_code = PARSE_JSON_PROFILE_PATTERN_SHADER_FUNC.replace("%FuncDefs%", PARSE_JSON_PROFILE_ENTRY_RUNTIME_FUNC)
    cpp_code = cpp_code.replace("%ValidKeys%", indent(valid_keys.rstrip("\n"), times=2))
    cpp_code = cpp_code.replace("%Defs%", indent(defs.rstrip("\n")))
    return cpp_code

def parse_json_profile_action_shader_runtime():
    """
    TODO.
    :return: TODO.
    """
    valid_keys = ""
    defs = ""
    for action, value in SHADER_ACTION.items():
        if "jsonReaderTemplate" in value and value["jsonReadable"]:
            valid_keys = valid_keys + '"' + action + '",\n'
            condition_block = CONDITION_PARSE_JSON_PROFILE_ENTRY_RUNTIME.replace("%Key%", action)
            str_value = convert_type_to_str_value(value["type"][0])
            condition_body = value["jsonReaderTemplate"]
            condition_body = condition_body.replace("%Action%", action).replace("%ValueType%", str_value)
            if str_value in TypeValues:
                condition_body = condition_body.replace("%Value%", TypeValues[str_value])
            condition_block = condition_block.replace("%Defs%", condition_body)
            defs = defs + wrap_with_directive(condition_block, value["buildTypes"])

    cpp_code = PARSE_JSON_PROFILE_ACTION_SHADER_FUNC.replace("%FuncDefs%", PARSE_JSON_PROFILE_ENTRY_RUNTIME_FUNC)
    cpp_code = cpp_code.replace("%ValidKeys%", indent(valid_keys.rstrip("\n"), times=2))
    cpp_code = cpp_code.replace("%Defs%", indent(defs.rstrip("\n")))
    return cpp_code

###################################################################################################################
# Generic functions
###################################################################################################################

def write_to_file(text, file_path):
    """
    Utility function that calls open().
    :param text: The text to write to the file.
    :param file_path: The path to the file.
    :return: None.
    """
    parent_dir = pathlib.Path(file_path).parent
    os.makedirs(parent_dir, exist_ok=True)

    with open(file_path, 'w', encoding="utf-8") as file:
        file.write(text)

def read_from_file(file_to_read):
    """
    Utility function that calls open().
    :param file_to_read: The path to the file.
    :return: None.
    """
    try:
        with open(file_to_read, 'r', encoding="utf-8") as file:
            content = file.read()
            dict_obj = json.loads(content)
            return dict_obj, True

    # pylint: disable=broad-except
    except Exception as read_exception:
        print(f"\nException Occurred:\n{read_exception } \nCould not read from file: {file_to_read}\n")
        return "", False

def dedent_all(text):
    """
    Dedents a block of text, line-by-line.
    :param text: The text to dedent.
    :return: The dedented text.
    """
    temp_text = ""
    for line in text.splitlines(True):
        temp_text += textwrap.dedent(line)
    return temp_text

def convert_type_to_str_value(val_type):
    """
    Converts a type to its corresponding string value.
    :param val_type: A Python type.
    :return: The corresponding string, or "unknownValue" if no such string exists.
    """
    if val_type == int:
        return "integerValue"
    if val_type == bool:
        return "booleanValue"
    if val_type == str:
        return "pStringValue"
    if val_type == dict:
        return "dictValue"
    if val_type == list:
        return "listValue"

    warnings.warn("********** Warning: Type unknown for action. Check 'type' key for action **********\n")
    return "unknownValue"

def check_valid_keys(obj1, obj2):
    """
    Checks if the keys in obj1 are also present in obj2 (list or dict)
    :param obj1: A list or a dict.
    :param obj2: A list or dict.
    :return: True if the key is in both, False if not or if the types are incompatible.
    """

    if isinstance(obj1, dict) and isinstance(obj2, dict):
        for key in [*obj1]:
            if key in [*obj2]:
                pass
            else:
                return False
        return True

    if isinstance(obj1, dict) and isinstance(obj2, list):
        for key in [*obj1]:
            if key in obj2:
                pass
            else:
                return False
        return True

    if isinstance(obj1, list) and isinstance(obj2, dict):
        for key in obj1:
            if key in [*obj2]:
                pass
            else:
                return False
        return True

    return False

def indent(text, **kwargs):
    """
    TODO.
    :param text: TODO.
    :param kwargs: TODO.
    :return: TODO.
    """
    indent_char = ' '

    n_spaces = kwargs.get("n_spaces", 4)
    times = kwargs.get("times", 1)

    if "width" in kwargs:
        wrapper = textwrap.TextWrapper()
        wrapper.width = kwargs["width"]
        wrapper.initial_indent = n_spaces * times * indent_char
        wrapper.subsequent_indent = n_spaces * times * indent_char
        content_list = wrapper.wrap(text)

        for i, line in enumerate(content_list):
            dedented_line = dedent_all(line)
            if dedented_line.startswith("#if") or dedented_line.startswith("#else") or dedented_line.startswith("#end"):
                content_list[i] = dedented_line
        return '\n'.join(content_list)

    padding = n_spaces * times * indent_char
    content = ''
    for line in text.splitlines(True):
        if line.startswith("#if") or line.startswith("#else") or line.startswith("#end") or line.isspace():
            content = content + dedent_all(line)
        else:
            content = content + padding + line

    return content

def convert_to_array(txt):
    """
    Replaces square brackets with curly brackets.
    :param: The input text.
    :return: The modified text.
    """
    txt = txt.replace("[", "{")
    txt = txt.replace("]", "}")
    return txt

def is_compiler_only_build_type(build_obj):
    """
    TODO.
    :param build_obj: TODO.
    :return: TODO.
    """
    if isinstance(build_obj, dict):
        if len(build_obj) == 1:
            if "andType" in build_obj \
                    and len(build_obj["andType"]) == 1 \
                    and build_obj["andType"][0] == BuildTypesTemplate["llpc"]:
                return True
    return False

def wrap_with_directive(content, build_obj):
    """
    TODO.
    :param content: TODO.
    :param build_obj: TODO.
    :return: TODO.
    """
    if isinstance(build_obj, str):
        if build_obj:
            content = "#if " + build_obj + "\n" + content.strip("\n") + "\n#endif\n"

    elif isinstance(build_obj, dict):
        if "andType" in build_obj:
            if build_obj["andType"]:
                value_if_def_tmp = ""
                value_end_def_tmp = ""
                for directive in build_obj["andType"]:
                    value_if_def_tmp += "#if " + directive + "\n"
                    value_end_def_tmp += "#endif" + "\n"

                content = value_if_def_tmp + content + value_end_def_tmp

        if "orType" in build_obj:
            if build_obj["orType"]:
                value_if_def_tmp = "#if "
                value_end_def_tmp = "#endif\n"
                num_of_build_types = len(build_obj["orType"])
                for i in range(num_of_build_types):
                    or_type = build_obj["orType"][i]
                    value_if_def_tmp += or_type
                    if i < num_of_build_types - 1:
                        value_if_def_tmp += " || "
                    else:
                        value_if_def_tmp += "\n"

                content = value_if_def_tmp + content + value_end_def_tmp

        if "custom" in build_obj:
            if build_obj["custom"]:
                if "startWith" in build_obj["custom"]:
                    start_with = build_obj["custom"]["startWith"]
                    content = start_with + "\n" + content.strip("\n")

                if "endWith" in build_obj["custom"]:
                    end_with = build_obj["custom"]["endWith"]
                    content = content + "\n" + end_with + "\n"
    return content

def retrieve_entity_info(value, parent):
    """
    TODO.
    :param value: TODO.
    :param parent: TODO.
    :return: TODO.
    """
    success = False
    entity_info = {}
    list_of_entity_info_objs = value["entityInfo"]
    if not isinstance(list_of_entity_info_objs, list):
        list_of_entity_info_objs = [list_of_entity_info_objs]
    for entity_info in list_of_entity_info_objs:
        if entity_info["parent"] == parent:
            success = True
            break

    if success:
        return entity_info
    return {}

def wrap_with_comment(content, comment, secured):
    """
    Puts a comment with a header and footer around a block of content.
    :param content: The content block.
    :param comment: The comment.
    :param secured: If the '#' character should be used to extend the comment.
    :return: The wrapped content.
    """
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

def main():
    """
    Parses all files and generate code.
    :return: Unix-style return code.
    """
    gen_dir = ''

    if len(sys.argv) >= 2:
        shader_profile_dir = sys.argv[1]
        # If genDir was specified by the user
        if len(sys.argv) == 3:
            gen_dir = sys.argv[2]
    else:
        print("Error: include directory path in the argument \n"
              "usage: python3 genshaderprofile.py <vulkancodebase>\\xgl\\icd\\api\\appopt\\shader_profiles\\ genDir ["
              "optional]")
        return -1

    if not os.path.isabs(shader_profile_dir):
        shader_profile_dir = os.path.abspath(shader_profile_dir)

    split_shader_profile_dir = os.path.split(shader_profile_dir)
    if split_shader_profile_dir[1] == '':
        output_dir = os.path.split(split_shader_profile_dir[0])[0]
    else:
        output_dir = split_shader_profile_dir[0]
    if gen_dir != "":
        output_dir = gen_dir

    header_dox_comment = HEADER_FILE_DOX_COMMENT.replace("%FileName%", OUTPUT_FILE)

    compilers = os.listdir(shader_profile_dir)

    game_title_info = get_game_titles(AppProfileHeaderFilePath)
    if not game_title_info:
        print("Could Not read 'enum class AppProfile' from app_profile.h. Exiting Program")
        return -1

    func_set_app_profile_group = ""
    class_shader_profile_body_dict = {}
    if_game_title_group_dict = {}

    for compiler in compilers:
        compiler_dir = os.path.join(shader_profile_dir, compiler)
        gfxips = os.listdir(compiler_dir)

        game_titles_list = []
        if_gfxip_group_dict = {}
        if_generic_dict = {}

        if compiler not in class_shader_profile_body_dict:
            class_shader_profile_body_dict[compiler] = ""

        if compiler not in if_game_title_group_dict:
            if_game_title_group_dict[compiler] = ""

        for gfxip in gfxips:
            gfxip_dir = os.path.join(compiler_dir, gfxip)

            game_titles_gfx_list = []
            if_asic_group_dict = {}
            if_asic_generic_dict = {}

            if gfxip != "generic":
                asics = os.listdir(os.path.join(gfxip_dir))
            else:
                asics = [gfxip]

            for asic in asics:
                if gfxip != "generic":
                    asic_dir = os.path.join(gfxip_dir, asic)
                else:
                    asic_dir = gfxip_dir

                print("Parsing " + asic_dir)
                game_titles = os.listdir(os.path.join(asic_dir))
                for title in game_titles:
                    game_title_dir = os.path.join(asic_dir, title)
                    file_to_read = os.path.join(gfxip, game_title_dir, CONFIG_FILE_NAME)
                    content, read_success = read_from_file(file_to_read)

                    if read_success:
                        if title not in game_titles_list:
                            game_titles_list.append(title)
                        if title not in game_titles_gfx_list:
                            game_titles_gfx_list.append(title)
                        if title not in if_gfxip_group_dict:
                            if_gfxip_group_dict[title] = ""
                        if title not in if_generic_dict:
                            if_generic_dict[title] = ""
                        if title not in if_asic_group_dict:
                            if_asic_group_dict[title] = ""
                        if title not in if_asic_generic_dict:
                            if_asic_generic_dict[title] = ""

                        # for header file: g_shader_profile.h********************************************************
                        func_name = compiler.title() + title + gfxip[0].upper() + gfxip[1:]

                        if gfxip != "generic":
                            if asic != "generic":
                                func_name = compiler.title() + title + asic[0].upper() + asic[1:]
                            else:
                                func_name += asic[0].upper() + asic[1:]

                        func_comp_game_gfx_asic = FUNC_DEC_SET_APP_PROFILE.replace("%FuncName%", func_name)

                        for _, obj in game_title_info.items():
                            if title in obj["gameTitles"]:
                                func_comp_game_gfx_asic = wrap_with_directive(func_comp_game_gfx_asic,
                                                                              obj["buildTypes"])

                        if asic in BuildTypesTemplate:
                            func_comp_game_gfx_asic = wrap_with_directive(func_comp_game_gfx_asic,
                                                                          BuildTypesTemplate[asic])

                        if gfxip in BuildTypesTemplate:
                            func_comp_game_gfx_asic = wrap_with_directive(func_comp_game_gfx_asic,
                                                                          BuildTypesTemplate[gfxip])

                        class_shader_profile_body_dict[compiler] += func_comp_game_gfx_asic
                        # ********************************************************************************************

                        # for cpp file: g_shader_profile.cpp *********************************************************
                        if asic == "generic":
                            if_asic_generic = GENERIC_ASIC_APP_PROFILE.replace("%FuncName%", func_name)
                            if_asic_generic_dict[title] = if_asic_generic
                        else:
                            if_asic = CONDITION_ASIC.replace("%Asic%", asic[0].upper() + asic[1:])
                            if_asic = if_asic.replace("%FuncName%", func_name)
                            if asic in BuildTypesTemplate:
                                if_asic = wrap_with_directive(if_asic, BuildTypesTemplate[asic])
                            if_asic_group_dict[title] = if_asic_group_dict[title] + if_asic

                        if gfxip == "generic":
                            if_generic = GENERIC_GFX_IP_APP_PROFILE.replace("%FuncName%", func_name)
                            if_generic_dict[title] = if_generic

                        app_profile = gen_profile(content, compiler)
                        func_set_app_profile = SET_APP_PROFILE_FUNC.replace("%FuncName%", func_name)
                        func_set_app_profile = func_set_app_profile.replace("%FuncDefs%", indent(app_profile))
                        if compiler in BuildTypesTemplate:
                            func_set_app_profile = wrap_with_directive(func_set_app_profile,
                                                                       BuildTypesTemplate[compiler])

                        for _, obj in game_title_info.items():
                            if title in obj["gameTitles"]:
                                func_set_app_profile = wrap_with_directive(func_set_app_profile, obj["buildTypes"])

                        if asic in BuildTypesTemplate:
                            func_set_app_profile = wrap_with_directive(func_set_app_profile, BuildTypesTemplate[asic])

                        if gfxip in BuildTypesTemplate:
                            func_set_app_profile = wrap_with_directive(func_set_app_profile, BuildTypesTemplate[gfxip])

                        func_set_app_profile_group = func_set_app_profile_group + func_set_app_profile
                        # ********************************************************************************************

            # for cpp file: g_shader_profile.cpp******************************************************************
            for title in game_titles_gfx_list:
                if gfxip != "generic":
                    if if_asic_generic_dict[title]:
                        if_gfxip_body = indent(if_asic_group_dict[title] + if_asic_generic_dict[title])
                    else:
                        if_gfxip_body = indent(if_asic_group_dict[title])
                    if_gfxip = CONDITION_GFX_IP.replace("%Gfxip%", gfxip[0].upper() + gfxip[1:])
                    if_gfxip = if_gfxip.replace("%Defs%", if_gfxip_body)
                    if gfxip in BuildTypesTemplate:
                        if_gfxip = wrap_with_directive(if_gfxip, BuildTypesTemplate[gfxip])
                    if_gfxip_group_dict[title] = if_gfxip_group_dict[title] + if_gfxip
            # ****************************************************************************************************

        # for cpp file: g_shader_profile.cpp******************************************************************
        for title in game_titles_list:
            if if_generic_dict[title]:
                if_game_title_body = indent(if_gfxip_group_dict[title] + if_generic_dict[title])
            else:
                if_game_title_body = indent(if_gfxip_group_dict[title])
            if_game_title = CONDITION_GAME_TITLE.replace("%GameTitle%", title)
            if_game_title = if_game_title.replace("%Defs%", if_game_title_body)
            for _, obj in game_title_info.items():
                if title in obj["gameTitles"]:
                    if_game_title = wrap_with_directive(if_game_title, obj["buildTypes"])
            if_game_title_group_dict[compiler] += if_game_title
        # ****************************************************************************************************

    ###################################################################################################################
    # Build the Header File
    ###################################################################################################################

    class_shader_profile_private_defs = ""
    for compiler, value in class_shader_profile_body_dict.items():
        if compiler in BuildTypesTemplate:
            if value != "":
                class_shader_profile_private_defs = class_shader_profile_private_defs + "\n" + \
                                                    wrap_with_directive(value,
                                                                        BuildTypesTemplate[compiler])
        else:
            class_shader_profile_private_defs = class_shader_profile_private_defs + value

    func_dec_json_reader = (
            FUNC_DEC_JSON_READER + "\n"
    )

    class_shader_profile_private_body = FUNC_DEC_JSON_WRITER + "\n" + wrap_with_directive(
        func_dec_json_reader, BuildTypesTemplate["icdRuntimeAppProfile"]) + class_shader_profile_private_defs

    class_shader_profile_public_body = (FUN_DEC_CLASS_SHADER_PROFILE_PUBLIC + "\n" +
                                        wrap_with_directive(FUNC_DEC_PARSE_JSON_PROFILE,
                                                            BuildTypesTemplate["icdRuntimeAppProfile"]) + "\n" +
                                        wrap_with_directive(FUNC_DEC_BUILD_APP_PROFILE_LLPC, BuildTypesTemplate["llpc"])
                                        )

    class_shader_profile = CLASS_TEMPLATE.replace("%ClassName%", "ShaderProfile")
    class_shader_profile = class_shader_profile.replace("%ClassPublicDefs%", indent(class_shader_profile_public_body))
    class_shader_profile = class_shader_profile.replace("%ClassPrivateDefs%", indent(class_shader_profile_private_body))

    content = create_struct_and_var_definitions(ShaderTuningStructsAndVars)
    namespace_body = content + "\n" + class_shader_profile
    header_body = NAMESPACE_VK.replace("%NamespaceDefs%", namespace_body)

    header_content = CopyrightAndWarning + header_dox_comment + HEADER_INCLUDES + "\n" + header_body
    header_file_path = os.path.join(output_dir, HEADER_FILE_NAME)
    os.makedirs(output_dir, exist_ok=True)
    write_to_file(header_content, header_file_path)

    ###################################################################################################################
    # Build the Source File
    ###################################################################################################################

    if "llpc" in if_game_title_group_dict:
        func_build_app_profile_llpc = BUILD_APP_PROFILE_LLPC_FUNC.replace("%FuncDefs%",
                                                                          indent(
                                                                              if_game_title_group_dict["llpc"].rstrip(
                                                                                  "\n")))
        func_build_app_profile_llpc = wrap_with_directive(func_build_app_profile_llpc, BuildTypesTemplate["llpc"])
    else:
        func_build_app_profile_llpc = ""

    func_profile_entry_action_to_json = build_profile_entry_action_to_json()
    func_profile_entry_pattern_to_json = build_profile_entry_pattern_to_json()
    func_json_writer = (JSON_WRITER_GENERIC_DEF + "\n" +
                        func_profile_entry_pattern_to_json + "\n" +
                        func_profile_entry_action_to_json)

    func_json_reader = (JSON_READER_GENERIC_DEF + "\n" +
                        parse_json_profile_entry_pattern_runtime() + "\n" +
                        parse_json_profile_entry_action_runtime() + "\n" +
                        parse_json_profile_pattern_shader_runtime() + "\n" +
                        parse_json_profile_action_shader_runtime() + "\n"
                        )

    cpp_body = NAMESPACE_VK.replace("%NamespaceDefs%", func_build_app_profile_llpc
                                    + "\n"
                                    + func_set_app_profile_group
                                    + "\n"
                                    + func_json_writer
                                    + "\n"
                                    + wrap_with_directive(func_json_reader,
                                                          BuildTypesTemplate["icdRuntimeAppProfile"])
                                    )

    include_str = ""

    cpp_includes = CPP_INCLUDE.replace("%Includes%", include_str)

    cpp_content = CopyrightAndWarning + cpp_includes + cpp_body
    cpp_file_path = os.path.join(output_dir, SOURCE_FILE_NAME)
    os.makedirs(output_dir, exist_ok=True)
    write_to_file(cpp_content, cpp_file_path)
    return 0

if __name__ == '__main__':
    if sys.version_info[:2] < (3, 6):
        raise Exception("Python 3.6 (CPython) or a more recent python version is required.")

    print("Generating shader profiles code ")

    if not main():
        print("Finished generating " + HEADER_FILE_NAME + " and " + SOURCE_FILE_NAME)
    else:
        print("Error: Exiting without code generation. Driver code compilation will fail.")
        sys.exit(1)
