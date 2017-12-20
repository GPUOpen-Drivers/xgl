##
 ###############################################################################
 #
 # Copyright (c) 2017 Advanced Micro Devices, Inc. All rights reserved.
 #
 # Permission is hereby granted, free of charge, to any person obtaining a copy
 # of this software and associated documentation files (the "Software"), to deal
 # in the Software without restriction, including without limitation the rights
 # to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 # copies of the Software, and to permit persons to whom the Software is
 # furnished to do so, subject to the following conditions:
 #
 # The above copyright notice and this permission notice shall be included in
 # all copies or substantial portions of the Software.
 #
 # THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 # IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 # FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 # AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 # LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 # OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 # THE SOFTWARE.
 ##############################################################################/


import os

def make_shader(il_file_prefix):
    il_file = "%s.il" % il_file_prefix;
    header_file = "%s.h" % il_file_prefix;

    print("Making %s from %s..." % (header_file, il_file))

    cmd_line = "..\\..\\imported\\pal\\tools\\generate\\dev -d -Q -ns -X -o -gfx6 -ns -nt %s" % (il_file);

    try:
        os.remove("il_tokens");
    except Exception, e:
        pass

    os.system(cmd_line)

    f = open("il_tokens")
    lines = f.readlines()
    f.close()

    f = open(header_file, 'w')

    f.write("// do not edit by hand; created from source file \"%s.il\" by executing the command \"%s\"\n" % (il_file_prefix, cmd_line));
    for line in lines:
        if line.startswith("0x"):
            f.write("    %s,\n" % (line.strip()))
    f.close();

make_shader("copy_timestamp_query_pool")
