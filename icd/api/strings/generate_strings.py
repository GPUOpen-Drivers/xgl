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
xgl helper script to generate string and struct lists.
"""

import os
import sys
import re
from optparse import OptionParser
import func_table_template

OPEN_COPYRIGHT = '''/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2022 Advanced Micro Devices, Inc. All Rights Reserved.
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
'''

WORK_DIR = os.getcwd()
OUTPUT_DIR = os.getcwd()
OPEN_SOURCE = True

def get_opt():
    """Parse command line options into global variables"""
    global WORK_DIR
    global OUTPUT_DIR
    global OPEN_SOURCE

    parser = OptionParser()

    parser.add_option("-w", "--workdir", action="store",
                  type="string",
                  dest="workdir",
                  help="the work directory")
    parser.add_option("-d", "--output_dir", action="store",
                  type="string",
                  dest="output_dir",
                  help="the output directory")

    options, _ = parser.parse_args()

    if options.workdir:
        print(f"The work directory is {options.workdir}")
        WORK_DIR = options.workdir
    else:
        print(f"The work directroy is not specified, using default: {WORK_DIR}")

    WORK_DIR = os.path.abspath(os.path.realpath(WORK_DIR))
    print(f"The work directory is {WORK_DIR}")

    if (not os.path.exists(WORK_DIR)) or (not os.path.exists(os.path.join(WORK_DIR, "extensions.txt"))):
        print(f"Work directory is not correct: {WORK_DIR}")
        sys.exit(1)

    if options.output_dir:
        OUTPUT_DIR = options.output_dir
    else:
        print("The output directory is not specified; using current directory")
    OUTPUT_DIR = os.path.abspath(os.path.realpath(OUTPUT_DIR))
    print(f"The output directory is {OUTPUT_DIR}")

def generate_string(f, name, suffix, value, gentype):
    """Generate a string declaration or constant"""
    value = f"{value}\0"
    prefix = f"const char {name}{suffix}[]"

    if gentype == 'decl':
        f.write(f"extern {prefix};\n")
    else:
        if OPEN_SOURCE:
            f.write(f"{prefix} = \"{name}\";\n")

    if gentype == 'decl':
        if name != name.upper():
            f.write(f"static const char* {name.upper()}{suffix} = {name}{suffix};\n")

def generate_entry_point_index(f, name, epidx):
    """Generate entry point index define"""
    f.write(f"#define {name}_index {epidx}\n")

def generate_entry_point_type(f, name, eptype):
    """Generate entry point type define"""
    f.write(f"#define {name}_type vk::EntryPoint::Type::{eptype}\n")

def generate_entry_point_condition(f, name, cond):
    """Generate entry point condition define"""
    cond = cond.upper()
    cond = cond.replace('@NONE', 'true')
    cond = cond.replace('@WIN32', 'true')

    core = re.compile(r'@(CORE|IEXT|DEXT)_BUILD_ONLY\( [^\)]* \)', re.VERBOSE)
    cond = core.sub(r'true', cond)

    core = re.compile(r'@CORE\( ( [^\.]* ) \. ( [^\)]* ) \)', re.VERBOSE)
    cond = core.sub(r'CheckAPIVersion(VK_MAKE_API_VERSION(0,\1,\2,0))', cond)

    iext = re.compile(r'@IEXT\( ( [^\)]* ) \)', re.VERBOSE)
    cond = iext.sub(r'CheckInstanceExtension(InstanceExtensions::ExtensionId::\1)', cond)

    dext = re.compile(r'@DEXT\( ( [^\)]* ) \)', re.VERBOSE)
    cond = dext.sub(r'CheckDeviceExtension(DeviceExtensions::ExtensionId::\1)', cond)

    f.write(f"#define {name}_condition {cond}\n")

def get_compile_condition(cond):
    """Assemble condition macro name"""
    cond = cond.replace('@none', '')
    cond = cond.replace('@win32', '_WIN32')

    core = re.compile(r'@core(?:_build_only)?\( ( [^\.]* ) \. ( [^\)]* ) \)', re.VERBOSE)
    cond = core.sub(r'VK_VERSION_\1_\2', cond)

    iext = re.compile(r'@iext(?:_build_only)?\( ( [^\)]* ) \)', re.VERBOSE)
    cond = iext.sub(r'VK_\1', cond)

    dext = re.compile(r'@dext(?:_build_only)?\( ( [^\)]* ) \)', re.VERBOSE)
    cond = dext.sub(r'VK_\1', cond)

    return cond

def make_version(version):
    """Create VK_MAKE_API_VERSION invocation"""
    tokens = version.split(".", 1)
    return f"VK_MAKE_API_VERSION(0,{tokens[0]}, {tokens[1]}, 0)"

def generate_string_file_pass(string_file_prefix, header_file_prefix, gentype):
    """Generate a single header or implementation file"""
    string_file = f"{string_file_prefix}.txt"
    header_file = os.path.join(OUTPUT_DIR, f"g_{header_file_prefix}_{gentype}.h")

    print(f"Generating {os.path.basename(header_file)} from {string_file} ...")

    with open(string_file) as f:
        lines = f.readlines()

    epidx = 0

    with open(header_file, 'w') as f:
        f.write(f"// do not edit by hand; generated from source file \"{string_file}\"\n")
        for line in lines:
            original = line.rstrip().lstrip()
            if original == "" or original[0] == '#':
                continue

            if '=' in original:
                # Generate key-value pair (e.g. for environment variable values controlling features)
                tokens = original.split('=', 1)
                name   = tokens[0].rstrip()
                value  = tokens[1].lstrip()
                generate_string(f, name, "", value, gentype)
            elif '@' in original:
                # Generate extension conditioned string (e.g. for entry points having conditions)
                tokens = original.split('@', 2)
                name   = tokens[0].rstrip()
                eptype = tokens[1].rstrip()
                cond   = '@' + tokens[2].rstrip()
                generate_string(f, name, "_name", name, gentype)
                if gentype == 'decl':
                    generate_entry_point_index(f, name, epidx)
                    epidx += 1
                    generate_entry_point_type(f, name, eptype.upper())
                    generate_entry_point_condition(f, name, cond)
            else:
                # Generate regular string (e.g. for extension strings)
                generate_string(f, original, "_name", original, gentype)

        if epidx > 0:
            f.write(f"#define VKI_ENTRY_POINT_COUNT {epidx}\n")

def generate_string_file(string_file_prefix):
    """Generate a pair of header and implementation file"""
    generate_string_file_pass(string_file_prefix, string_file_prefix, 'decl')
    generate_string_file_pass(string_file_prefix, string_file_prefix, 'impl')

def generate_func_table(entry_file, header_file):
    """Generate function table"""
    print(f"Generating {header_file} from {entry_file} ...")

    with open(entry_file) as f:
        lines = f.readlines()

    header_path = os.path.join(OUTPUT_DIR, header_file)
    with open(header_path, 'w') as header:
        entry_point_members = ''
        prev_ext = ''
        prev_ext_ep_count = 0
        reserved_ep_count = 0

        for line in lines:
            original = line.rstrip().lstrip()
            if original == "" or original[0] == '#':
                continue

            # Figure out if this entry point depends on an extension to be enabled by the compiler.  This is really
            # only relevant for platform-specific extensions e.g. win32 or xcb WSI extensions as most normal extensions
            # are always compiled in.
            cur_ext = ''

            if '@' in original:
                tokens = original.split('@', 2)
                cur_ext = get_compile_condition('@' + tokens[2])

            if cur_ext != prev_ext:
                preprocessor = ''
                if len(prev_ext) > 0:
                    preprocessor += '#else\n'
                    for _ in range(0, prev_ext_ep_count):
                        preprocessor += '    PFN_vkVoidFunction reserved' + str(reserved_ep_count) + ';\n'
                        reserved_ep_count += 1
                    preprocessor += '#endif\n'

                if len(cur_ext) > 0:
                    preprocessor += f'#if {cur_ext}\n'

                prev_ext = cur_ext
                prev_ext_ep_count = 0
                entry_point_members = entry_point_members + preprocessor

            func_name = line.split(' ')[0]

            prev_ext_ep_count += 1
            entry_point_members = entry_point_members + func_table_template.entry_point_member.replace('$func_name$', func_name) + '\n'

        if len(prev_ext) > 0:
            preprocessor = '#else\n'
            for _ in range(0, prev_ext_ep_count):
                preprocessor += '    PFN_vkVoidFunction reserved' + str(reserved_ep_count) + ';\n'
                reserved_ep_count += 1
            preprocessor += '#endif\n'
            entry_point_members = entry_point_members + preprocessor

        if OPEN_SOURCE:
            header_template = func_table_template.header_template.replace('$copyright_string$', OPEN_COPYRIGHT)
        header_template = header_template.replace('$entry_file$', entry_file)
        header_template = header_template.replace('$entry_point_members$', entry_point_members)

        header.write(header_template)

get_opt()
os.chdir(WORK_DIR)
os.makedirs(OUTPUT_DIR, exist_ok=True)

generate_string_file("extensions")
generate_string_file("entry_points")
generate_func_table("entry_points.txt", "g_func_table.h")
