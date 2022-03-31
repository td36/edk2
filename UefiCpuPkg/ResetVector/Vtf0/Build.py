## @file
#  Automate the process of building the various reset vector types
#
#  Copyright (c) 2009 - 2022, Intel Corporation. All rights reserved.<BR>
#
#  SPDX-License-Identifier: BSD-2-Clause-Patent
#

import os
import subprocess
import sys

FILE_FORMAT    = '.raw'
IA32           = 'IA32'
X64            = 'X64'
P64            = 'P64'

# Pre-Define a Macros for Page Table
X64_PAGE_TABLES = {
    "PageTable2M" : "PAGE_TABLE_2M",
    "PageTable1G" : "PAGE_TABLE_1G"
}

def RunCommand(commandLine):
    return subprocess.call(commandLine)

# Check for all raw binaries and delete them
for root, dirs, files in os.walk('Bin'):
    for file in files:
        if file.endswith(FILE_FORMAT):
            os.remove(os.path.join(root, file))

for arch in (IA32, X64, P64):

    pageTables = {None: None}
    if arch == X64:
        pageTables = X64_PAGE_TABLES

    for pageTable in pageTables.keys():
        for debugType in (None, 'PORT80', 'SERIAL'):

            # Pattern of directory:
            #   X64:      Bin/X64/(PageTable2M|PageTable1G)
            #   IA32/P64: Bin/(IA32|P64)
            directory = os.path.join('Bin', arch)
            if pageTable is not None:
                directory = os.path.join(directory, pageTable)

            # Pattern of fileName:
            #   ResetVector.(ia32|x64|p64)[.(port80|serial)].raw
            fileName = 'ResetVector' + '.' + arch.lower()
            if debugType is not None:
                fileName += '.' + debugType.lower()
            fileName += FILE_FORMAT

            output = os.path.join(directory, fileName)

            # if the directory not exists then create it
            if not os.path.isdir(directory):
                os.makedirs(directory)

            # Prepare the command to execute the nasmb
            commandLine = f'nasm -D ARCH_{arch}'
            if debugType is not None:
                commandLine += f' -D DEBUG_{debugType}'
            if pageTable is not None:
                commandLine += f' -D {pageTables[pageTable]}'
            commandLine += f' -o {output} Vtf0.nasmb'

            print(f"Command : {commandLine}")

            try:
                ret = RunCommand(commandLine.split())
            except FileNotFoundError:
                print("NASM not found")
            except:
                pass

            if ret != 0:
                print(f"something went wrong while executing {commandLine[-1]}")
                sys.exit()
            print('\tNASM\t' + output)

            commandLine = (
                'python',
                'Tools/FixupForRawSection.py',
                output,
                )
            print('\tFIXUP\t' + output)
            ret = RunCommand(commandLine)
            if ret != 0: sys.exit(ret)
