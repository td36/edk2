# @file
# Script to Build OVMF UEFI firmware
#
# Copyright (c) Microsoft Corporation.
# SPDX-License-Identifier: BSD-2-Clause-Patent
##
from asyncio.windows_events import NULL
import shutil
import os
import sys
import logging
import argparse
from edk2toollib.utility_functions import RunCmd
from edk2toolext.environment import shell_environment

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
from PlatformBuildLib import SettingsManager
from PlatformBuildLib import PlatformBuilder

    # ####################################################################################### #
    #                                Common Configuration                                     #
    # ####################################################################################### #
class UnitTestConfig():
    ''' Common settings for this platform.  Define static data here and use
        for the different parts of stuart
    '''
    UnitTestModule    = {'UefiCpuPkg/Library/CpuExceptionHandlerLib/UnitTest/DxeCpuExceptionHandlerLibUnitTest.inf':'UefiCpuPkg.dsc',
                         'MdeModulePkg/Application/HelloWorld/HelloWorld.inf'                                      :'MdeModulePkg.dsc'
                        }
    PackagesSupported = ("UefiCpuPkg",)
    ArchSupported     = ("IA32", "X64")
    TargetsSupported  = ("DEBUG", "RELEASE", "NOOPT")
    Scopes            = ('ovmf', 'edk2-build')
    WorkspaceRoot     = os.path.realpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
    def __init__(self):
        print('Print2')
        for Value in UnitTestConfig.UnitTestModule.values():
            PkgName = Value.split("Pkg")[0] + 'Pkg'
            PackagesSupported = list(UnitTestConfig.PackagesSupported)
            UnitTestConfig.PackagesSupported = PackagesSupported
            if PkgName not in list(UnitTestConfig.PackagesSupported):
                UnitTestConfig.PackagesSupported.append(PkgName)

    def GetDscName(cls, ArchCsv: str) -> str:
        return "TargetUnitTest/OvmfPkgTargetTest.dsc"
print('Print1')
import PlatformBuildLib
UnitTestUnitTestConfig = UnitTestConfig()
PlatformBuildLib.CommonPlatform = UnitTestUnitTestConfig

class UnitTestSettingsManager(SettingsManager):
    def GetPlatformDscAndConfig(self) -> tuple:
        return None

class UnitTestPlatformBuilder(PlatformBuilder):
    def AddCommandLineOptions(self, parserObj):
        print('Print3')
        ''' Add command line options to the argparser '''
        parserObj.add_argument('-a', "--arch", dest="build_arch", type=str, default="IA32,X64",
            help="Optional - CSV of architecture to build.  IA32 will use IA32 for Pei & Dxe. "
            "X64 will use X64 for both PEI and DXE.  IA32,X64 will use IA32 for PEI and "
            "X64 for DXE. default is IA32,X64")
        parserObj.add_argument('-p', '--pkg', '--pkg-dir', dest='packageList', type=str,
                               help='Optional - A package or folder you want to update (workspace relative).'
                               'Can list multiple by doing -p <pkg1>,<pkg2> or -p <pkg3> -p <pkg4>',
                               action="append", default=[])
        
    def CheckBootLog(self):
        #
        # Find all FAILURE MESSAGE in boot log
        #
        #BuildLog = "BUILDLOG_{0}.txt".format(PlatformBuilder.GetName(self))
        #LogPath  = os.path.join(self.ws, 'Build', BuildLog)
        LogPath = 'E:/code/FutureCoreRpArraw/RomImages/fail.log'
        print('Checking the boot log: {0}'.format(LogPath))
        file        = open(LogPath, "r")
        fileContent = file.readlines()
        for Index in range(len(fileContent)):
            if 'FAILURE MESSAGE:' in fileContent[Index]:
                if fileContent[Index + 1].strip() != '':
                    FailureMessage = fileContent[Index + 1] + fileContent[Index + 2]
                    print(FailureMessage)
                    return FailureMessage
        return 0

    def BuildUnitTest(self, packageList):
        VirtualDrive = os.path.join(self.env.GetValue("BUILD_OUTPUT_BASE"), "VirtualDrive")
        BuiltDsc = []
        for package in packageList:
            print(package)
            for Module, Dsc in UnitTestConfig.UnitTestModule.items():
                if package in Dsc and Dsc not in BuiltDsc:
                    Module = os.path.normpath(Module)
                    ModuleName = Module.split('.inf')[0].rsplit('\\')[-1]
                    BuiltDsc.append(Dsc)
                    DscPath = os.path.join(package, Dsc)

                    # Build specific dsc for UnitTest modules
                    print('Going to build this {0} for {1}'.format(DscPath, ModuleName))
                    Arch = self.env.GetValue("TARGET_ARCH").split(" ")
                    print('Arch is {}'.format(Arch))
                    # Set the Unit Test arch the same as the Shell in Ovmf.
                    if 'X64' in Arch:
                        UTArch = 'X64'
                    else:
                        UTArch = 'IA32'
                    self.env.AllowOverride("ACTIVE_PLATFORM")
                    self.env.SetValue("ACTIVE_PLATFORM", DscPath, "Test")
                    self.env.AllowOverride("TARGET_ARCH")
                    self.env.SetValue("TARGET_ARCH", UTArch, "Test")
                    self.Build()

                    # Copy the UnitTest efi files to VirtualDrive folder
                    EfiPath = os.path.join(self.env.GetValue("BUILD_OUTPUT_BASE"), UTArch, Module.split('.inf')[0], self.env.GetValue("TARGET"), ModuleName + '.efi')
                    print('Copy {0}.efi from:{1}'.format(ModuleName, EfiPath))
                    shutil.copy(EfiPath, VirtualDrive)

    def WriteEfiToStartup(self, Folder):
        ''' Write all the .efi files' name in VirtualDrive into Startup.nsh'''
        if (self.env.GetValue("MAKE_STARTUP_NSH").upper() == "TRUE"):
            f = open(os.path.join(Folder, "startup.nsh"), "w")
            for root,dirs,files in os.walk(Folder):
                for file in files:
                    print(file)
                    print(os.path.splitext(file)[1])
                    if os.path.splitext(file)[1] == '.efi':
                        f.write("{0} \n".format(file))
            f.write("reset -s\n")
            f.close()

    def PlatformPostBuild(self):
        ''' Build specific Pkg in command line for UnitTest modules.
            The build configuration is defined in Dsc of UnitTestConfig.GetDscName.
            Then copy the .efi files into VirtualDrive folder.
        '''
        parserObj = argparse.ArgumentParser(description='For building UnitTest modules')
        self.AddCommandLineOptions(parserObj)
        args, unknown_args = parserObj.parse_known_args()
        print(args.packageList)
        packageListSet = set()
        for item in args.packageList:  # Parse out the individual packages
            item_list = item.split(",")
            print(item_list)
            for individual_item in item_list:
                # in case cmd line caller used Windows folder slashes
                individual_item = individual_item.replace("\\", "/").rstrip("/")
                packageListSet.add(individual_item.strip())

        self.BuildUnitTest(packageListSet)
        VirtualDrive = os.path.join(self.env.GetValue("BUILD_OUTPUT_BASE"), "VirtualDrive")
        self.WriteEfiToStartup(VirtualDrive)
        return 0

    def FlashRomImage(self):
        PlatformBuilder.FlashRomImage(self)
        UnitTestResult = self.CheckBootLog()
        if (UnitTestResult):
            logging.info("UnitTest failed with this FAILURE MESSAGE:\n{}".format(UnitTestResult))
            return UnitTestResult
        return 0
