##
 #######################################################################################################################
 #
 #  Copyright (c) 2021 Advanced Micro Devices, Inc. All Rights Reserved.
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
import re
from optparse import OptionParser

open_copyright = '''/*
 ***********************************************************************************************************************
 *
 *  Copyright (c) 2014-2021 Advanced Micro Devices, Inc. All Rights Reserved.
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

workDir = os.getcwd()
outputDir = os.getcwd()
openSource = True;

def GetOpt():
    global workDir;
    global outputDir
    global openSource;

    parser = OptionParser()

    parser.add_option("-w", "--workdir", action="store",
                  type="string",
                  dest="workdir",
                  help="the work directory")
    parser.add_option("-d", "--output_dir", action="store",
                  type="string",
                  dest="output_dir",
                  help="the output directory")

    (options, args) = parser.parse_args()

    if options.workdir:
        print("The work directory is %s" % (options.workdir));
        workDir = options.workdir;
    else:
        print("The work directroy is not specified, using default: " + workDir);

    workDir = os.path.abspath(os.path.realpath(workDir))
    print("The work directory is %s" % (workDir))

    if (os.path.exists(workDir) == False) or (os.path.exists(os.path.join(workDir, "extensions.txt")) == False):
        print("Work directory is not correct: " + workDir);
        exit();

    if options.output_dir:
        outputDir = options.output_dir
    else:
        print("The output directory is not specified; using current directory")
    outputDir = os.path.abspath(os.path.realpath(outputDir))
    print("The output directory is {}".format(outputDir))

def generate_string(f, name, suffix, value, gentype):
    global openSource;

    value = "%s\0" % value
    prefix = "const char %s%s[]" % (name, suffix);

    if gentype == 'decl':
        f.write("extern %s;\n" % prefix);
    else:
        if openSource:
            f.write("%s = \"" % prefix);
            f.write(name);
            f.write("\";\n");

    if gentype == 'decl':
        if name != name.upper():
            f.write("static const char* %s%s = %s%s;\n" % (name.upper(), suffix, name, suffix))

def generate_entry_point_index(f, name, epidx):
    f.write("#define %s_index %d\n" % (name, epidx))

def generate_entry_point_type(f, name, eptype):
    f.write("#define %s_type vk::EntryPoint::Type::%s\n" % (name, eptype))

def generate_entry_point_condition(f, name, cond):
    cond = cond.upper()
    cond = cond.replace('@NONE', 'true')
    cond = cond.replace('@WIN32', 'true')

    core = re.compile('@CORE\( ( [^\.]* ) \. ( [^\)]* ) \)', re.VERBOSE)
    cond = core.sub(r'CheckAPIVersion(VK_MAKE_VERSION(\1,\2,0))', cond)

    iext = re.compile('@IEXT\( ( [^\)]* ) \)', re.VERBOSE)
    cond = iext.sub(r'CheckInstanceExtension(InstanceExtensions::ExtensionId::\1)', cond)

    dext = re.compile('@DEXT\( ( [^\)]* ) \)', re.VERBOSE)
    cond = dext.sub(r'CheckDeviceExtension(DeviceExtensions::ExtensionId::\1)', cond)

    f.write("#define %s_condition %s\n" % (name, cond))

def get_compile_condition(cond):
    cond = cond.replace('@none', '')
    cond = cond.replace('@win32', '_WIN32')

    core = re.compile('@core\( ( [^\.]* ) \. ( [^\)]* ) \)', re.VERBOSE)
    cond = core.sub(r'VK_VERSION_\1_\2', cond)

    iext = re.compile('@iext\( ( [^\)]* ) \)', re.VERBOSE)
    cond = iext.sub(r'VK_\1', cond)

    dext = re.compile('@dext\( ( [^\)]* ) \)', re.VERBOSE)
    cond = dext.sub(r'VK_\1', cond)

    return cond

def make_version(version):
    tokens = version.split(".", 1)
    return "VK_MAKE_VERSION(%s, %s, 0)" % (tokens[0], tokens[1])

def generate_string_file_pass(string_file_prefix, header_file_prefix, gentype):
    global outputDir

    string_file = "%s.txt" % (string_file_prefix);
    header_file = os.path.join(outputDir, "g_{}_{}.h".format(header_file_prefix, gentype))

    print("Generating %s from %s ..." % (os.path.basename(header_file), string_file))

    f = open(string_file)
    lines = f.readlines()
    f.close()

    epidx = 0

    f = open(header_file, 'w')

    f.write("// do not edit by hand; generated from source file \"%s\"\n" % string_file)
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
        f.write("#define VKI_ENTRY_POINT_COUNT %d\n" % epidx)

    f.close()

def generate_string_file(string_file_prefix):
    generate_string_file_pass(string_file_prefix, string_file_prefix, 'decl');
    generate_string_file_pass(string_file_prefix, string_file_prefix, 'impl');

def generate_func_table(entry_file, header_file):
    from func_table_template import header_template
    from func_table_template import entry_point_member

    global open_copyright
    global openSource;
    global outputDir

    print("Generating %s from %s ..." % (header_file, entry_file))

    f = open(entry_file)
    lines = f.readlines()
    f.close()

    header_path = os.path.join(outputDir, header_file)
    header = open(header_path, 'w')

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
                for num in range(0, prev_ext_ep_count):
                    preprocessor += '    PFN_vkVoidFunction reserved' + str(reserved_ep_count) + ';\n'
                    reserved_ep_count += 1
                preprocessor += '#endif\n'

            if len(cur_ext) > 0:
                preprocessor += ('#if %s\n' % cur_ext)

            prev_ext = cur_ext
            prev_ext_ep_count = 0
            entry_point_members = entry_point_members + preprocessor

        func_name = line.split(' ')[0]

        prev_ext_ep_count += 1
        entry_point_members = entry_point_members + entry_point_member.replace('$func_name$', func_name) + '\n'

    if len(prev_ext) > 0:
        preprocessor = '#else\n'
        for num in range(0, prev_ext_ep_count):
            preprocessor += '    PFN_vkVoidFunction reserved' + str(reserved_ep_count) + ';\n'
            reserved_ep_count += 1
        preprocessor += '#endif\n'
        entry_point_members = entry_point_members + preprocessor

    if openSource:
        header_template = header_template.replace('$copyright_string$', open_copyright)
    header_template = header_template.replace('$entry_file$', entry_file)
    header_template = header_template.replace('$entry_point_members$', entry_point_members)

    header.write(header_template)
    header.close()

GetOpt()
os.chdir(workDir)
os.makedirs(outputDir, exist_ok=True)

generate_string_file("extensions")
generate_string_file("entry_points")
generate_func_table("entry_points.txt", "g_func_table.h")
