#!/usr/bin/env python

import sys, os, glob

HTTPSERVER = os.path.realpath(__file__ + '/../') + '/'
HTML_DIRECTORY = HTTPSERVER + 'web/'
HTML_FILES = glob.glob(HTML_DIRECTORY + '*.html') + glob.glob(HTML_DIRECTORY + '*.js')
NEW_FS_HEADER = HTML_DIRECTORY + 'includes/web_files.h'

def new_header():
    includes = ""
    for html in HTML_FILES:
        path, header = os.path.split(html)
        new_name = header.replace('.', '_')
        header_file = path + '/includes/' + new_name + '.h'
        if os.path.isfile(header_file):
            includes += "#include <%s.h>\n" % new_name

    with open(NEW_FS_HEADER, 'w+') as fs_header:
        fs_header.write("/* WARNING: Automatically Generated File */\n")
        fs_header.write("%s" % includes)

def clean_directory():
    if os.path.isfile(NEW_FS_HEADER):
        os.remove(NEW_FS_HEADER)
    for html in HTML_FILES:
        path, header = os.path.split(html)
        header_file = path + '/includes/' + header.replace('.', '_') + '.h'
        if os.path.isfile(header_file):
            os.remove(header_file)

#
# Convert all the html and js files in the 'web/' directory to header files.
# something.html gets converted to an unsigned char data_something_html[] = ...
#
# To add different file extensions, add a line to the HTML_FILES with the proper extension.
def files_to_array():
    # TODO: Refactor this function to handle single file in directory w/o so many loops
    html_arrays = []
    clean_directory()
    for html in HTML_FILES:
        with open(html) as temp_file:
            html_file = ''.join(temp_file.readlines()).encode('hex')
            path, header = os.path.split(html)
            name = header.replace('.', '_')
            if not os.path.isdir(path + '/includes/'):
                os.makedirs(path + '/includes/')
            header_file = path + '/includes/' + name + '.h'
            with open(header_file, 'w+') as html_header:
                html_header.write("/* WARNING: Automatically Generated File */\n")
                # Create an array. Right now, file is in hex stream (012ac45e1) which needs to be converted
                # to individual values (0x12, 0xac, 0x45, 0xe1), then format and close the array
                html_header.write("unsigned char data_%s[] = {\n" % name)
                hex_codes = ', '.join(['0x' + html_file[i:i+2] for i in range(0, len(html_file), 2)]) + ','
                hex_codes = hex_codes.replace('0x0a,', '0x0d, 0x0a,')
                hex_codes = '\n'.join([hex_codes[i:i+96].rstrip() for i in range(0, len(hex_codes), 96)])
                html_header.write("%s\n};\n" % str(hex_codes))

if __name__ == "__main__":
    if ''.join(sys.argv[1:]) == "-c":
        clean_directory()
        sys.exit(0)
    files_to_array()
    new_header()
