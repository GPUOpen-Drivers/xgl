##
 #######################################################################################################################
 #
 #  Copyright (c) 2019-2020 Advanced Micro Devices, Inc. All Rights Reserved.
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

# This script lives in the generate/ directory, other generation files (e.g. template files)
# reside in pal/tools/generate/ directory
# This script is the xgl settings helper script for the generate settings code script in PAL

GenerateDirPath = os.path.dirname(sys.argv[0])
if GenerateDirPath == "":
    GenerateDirPath = "."

PythonExecutable = sys.executable + " "

PalDepth = sys.argv[1] + "/"

GenSettingsStr = (PythonExecutable + PalDepth +
                  "tools/generate/genSettingsCode.py --settingsFile %SettingsFile% --codeTemplateFile %CodeTemplateFile% --outFilename %OutFilename% --classNameOverride %ClassNameOverride% %GenRegistryCode%")

ClassNameOverride = "%ClassName%"

SettingsFileBasePath = GenerateDirPath + "/../../settings/"

GenerateRegistryCodeEnabled = " --genRegistryCode"
GenerateRegistryCodeDisabled = ""

DefaultSettingsTemplateFile = os.path.dirname(os.path.abspath(__file__))+"/vulkanSettingsCodeTemplates.py"

settingsArgData = {
    "vulkan": {
        "SettingsFile": SettingsFileBasePath+"settings_xgl.json",
        "CodeTemplateFile": DefaultSettingsTemplateFile,
        "OutFilename": "g_settings",
        "ClassNameOverride": "VulkanSettingsLoader",
        "GenRegistryCode": GenerateRegistryCodeEnabled,
    }
}

def GenSettings(argData):
    # Build the command string
    commandStr = GenSettingsStr
    commandStr = commandStr.replace("%SettingsFile%",      argData["SettingsFile"])
    commandStr = commandStr.replace("%CodeTemplateFile%",  argData["CodeTemplateFile"])
    commandStr = commandStr.replace("%OutFilename%",       argData["OutFilename"])
    commandStr = commandStr.replace("%ClassNameOverride%", argData["ClassNameOverride"])
    commandStr = commandStr.replace("%GenRegistryCode%",   argData["GenRegistryCode"])
    print(commandStr)

    return os.system(commandStr)

usage = "\
*****************************************************************************************************\n\
  Helper script to generate settings files. User can provide components to be generated as arguments.\n\
  The current list of supported arguments/components is:\n\
      Vulkan\n\
  User can instead pass '-all' to generate all components' settings files.\n\
    Example Usage: python genSettingsCode.py vulkan\n\
*****************************************************************************************************"

if len(sys.argv) < 1:
    print(usage)
    sys.exit(1)

if sys.argv[1] == "-all":
    # Generate all the settings files
    for key in settingsArgData:
        print("Generating settings code for " + key)
        result = GenSettings(settingsArgData[key])
        if result != 0:
            print("Error generating settings for " + key)
else:
    for component in sys.argv[2:]:
        if component in settingsArgData:
            print("Generating settings code for " + component)
            result = GenSettings(settingsArgData[component])
            if result != 0:
                print("Error generating settings for " + component + ". Did you forget to check out the g_ files?")
        else:
            print("Unknown component argument: " + component)

sys.exit(0)
