##
 #######################################################################################################################
 #
 #  Copyright (c) 2019-2023 Advanced Micro Devices, Inc. All Rights Reserved.
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

"""This is the xgl settings helper script for the generation of settings code by a script in PAL.

This script lives in the generate/ directory, while the other generation files (e.g. template files)
reside in pal/tools/generate/ directory.
"""

import os
import sys

GENERATE_DIR_PATH = os.path.dirname(sys.argv[0])
if GENERATE_DIR_PATH == "":
    GENERATE_DIR_PATH = "."

PYTHON_EXECUTABLE = sys.executable + " "

PalDepth = sys.argv[1] + "/"

GenSettingsStr = (PYTHON_EXECUTABLE + PalDepth +
                  "tools/generate/genSettingsCode.py --settingsFile %SettingsFile% --codeTemplateFile "
                  "%CodeTemplateFile% --outFilename %OutFilename% --classNameOverride %ClassNameOverride% "
                  "%GenRegistryCode%")

CLASS_NAME_OVERRIDE = "%ClassName%"

SettingsFileBasePath = GENERATE_DIR_PATH + "/../../settings/"

# If <genDir> was specified
if len(sys.argv) == 4:
    GENERATE_DIR_PATH = sys.argv[3]
    SettingsFileBasePath = GENERATE_DIR_PATH + "/settings/"

GENERATE_REGISTRY_CODE_ENABLED = " --genRegistryCode"
GENERATE_REGISTRY_CODE_DISABLED = ""

DefaultSettingsTemplateFile = os.path.dirname(os.path.abspath(__file__)) + "/vulkanSettingsCodeTemplates.py"

settingsArgData = {
    "vulkan": {
        "SettingsFile": SettingsFileBasePath + "settings_xgl.json",
        "CodeTemplateFile": DefaultSettingsTemplateFile,
        "OutFilename": "g_settings",
        "ClassNameOverride": "VulkanSettingsLoader",
        "GenRegistryCode": GENERATE_REGISTRY_CODE_ENABLED,
    }
}

def gen_settings(arg_data):
    """Invoke generate settings script."""
    # Build the command string
    command_str = GenSettingsStr
    command_str = command_str.replace("%SettingsFile%", arg_data["SettingsFile"])
    command_str = command_str.replace("%CodeTemplateFile%", arg_data["CodeTemplateFile"])
    command_str = command_str.replace("%OutFilename%", arg_data["OutFilename"])
    command_str = command_str.replace("%ClassNameOverride%", arg_data["ClassNameOverride"])
    command_str = command_str.replace("%GenRegistryCode%", arg_data["GenRegistryCode"])
    print(command_str)

    return os.system(command_str)

USAGE = "\
*****************************************************************************************************\n\
  Helper script to generate settings files. User can provide components to be generated as arguments.\n\
  The current list of supported arguments/components is:\n\
      [mandatory] <palDir> - path to PAL sources\n\
      Vulkan\n\
      [optional <genDir>]\n\
  User can instead pass \'-all\' to generate all components\' settings files.\n\
    Example Usage: python genSettingsCode.py <palDir> vulkan [optional: <genDir> - path to output]\n\
*****************************************************************************************************"

if len(sys.argv) not in (3, 4):
    print(USAGE)
    sys.exit(1)

if sys.argv[1] == "-all":
    # Generate all the settings files
    for key, value in settingsArgData.items():
        print("Generating settings code for " + key)
        result = gen_settings(value)
        if result != 0:
            print("Error generating settings for " + key)
            sys.exit(1)
else:
    component = sys.argv[2]
    if component in settingsArgData:
        print("Generating settings code for " + component)
        result = gen_settings(settingsArgData[component])
        if result != 0:
            print("Error generating settings for " + component + ". Did you forget to check out the g_ files?")
            sys.exit(1)
    else:
        print("Unknown component argument: " + component)
        sys.exit(1)

sys.exit(0)
