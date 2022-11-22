# @file
# Script to Build OVMF UEFI firmware
#
# Copyright (c) Microsoft Corporation.
# SPDX-License-Identifier: BSD-2-Clause-Patent
##
import os
import sys
import shutil
import logging
from edk2toolext.environment import shell_environment
from edk2toolext.environment.multiple_workspace import MultipleWorkspace

sys.path.append(os.path.dirname(os.path.abspath(__file__)))
from PlatformBuildLib import SettingsManager
from PlatformBuildLib import PlatformBuilder

    # ####################################################################################### #
    #                                Common Configuration                                     #
    # ####################################################################################### #
class CommonPlatform():
    ''' Common settings for this platform.  Define static data here and use
        for the different parts of stuart
    '''
    PackagesSupported = ("OvmfPkg",)
    ArchSupported = ("IA32", "X64")
    TargetsSupported = ("DEBUG", "RELEASE", "NOOPT")
    Scopes = ('ovmf', 'edk2-build')
    WorkspaceRoot = os.path.realpath(os.path.join(
        os.path.dirname(os.path.abspath(__file__)), "..", ".."))
    # Support build and run Shell Unit Test modules
    UnitTestModuleList = {}
    RunShellUnitTest   = False
    ShellUnitTestLog   = ''

    @classmethod
    def GetDscName(cls, ArchCsv: str) -> str:
        ''' return the DSC given the architectures requested.

        ArchCsv: csv string containing all architectures to build
        '''
        dsc = "OvmfPkg"
        if "IA32" in ArchCsv.upper().split(","):
            dsc += "Ia32"
        if "X64" in ArchCsv.upper().split(","):
            dsc += "X64"
        dsc += ".dsc"
        return dsc

    @classmethod
    def UpdatePackagesSupported(cls, ShellUnitTestList):
        ''' Update PackagesSupported by -u ShellUnitTestList from cmd line. '''
        if not ShellUnitTestList:
            return
        UnitTestModuleList = ','.join(ShellUnitTestList).split(",")
        PackagesSupported = []
        for KeyValue in UnitTestModuleList:
            PkgName = KeyValue.split("Pkg")[0] + 'Pkg'
            if PkgName not in PackagesSupported:
                PackagesSupported.append(PkgName)
        cls.PackagesSupported = tuple(PackagesSupported)
        print('PackagesSupported for UnitTest is {}'.format(cls.PackagesSupported))

    @classmethod
    def UpdateUnitTestConfig(cls, args):
        ''' Update UnitTest config by -u ShellUnitTestList and -p PkgsToBuildForUT from cmd line.
            ShellUnitTestList is in this format: {module1:dsc1, module2:dsc2, module3:dsc2...}.
            Only the modules which are in the PkgsToBuildForUT list are added into self.UnitTestModuleList.
        '''
        if not args.ShellUnitTestList:
            return
        UnitTestModuleList = ','.join(args.ShellUnitTestList).split(",")
        if args.PkgsToBuildForUT is None or all(['Pkg' not in Pkg for Pkg in args.PkgsToBuildForUT]):
            # No invalid Pkgs from input. Build all modules in -u UnitTestModuleList.
            for KeyValue in UnitTestModuleList:
                UnitTestPath = os.path.normpath(KeyValue.split(":")[0])
                DscPath      = os.path.normpath(KeyValue.split(":")[1])
                cls.UnitTestModuleList[UnitTestPath] = DscPath
        else:
            PkgsToBuildForUT = ','.join(args.PkgsToBuildForUT).split(',')
            for KeyValue in UnitTestModuleList:
                UnitTestPath = os.path.normpath(KeyValue.split(":")[0])
                DscPath      = os.path.normpath(KeyValue.split(":")[1])
                PkgName      = UnitTestPath.split("Pkg")[0] + 'Pkg'
                if PkgName in PkgsToBuildForUT:
                    cls.UnitTestModuleList[UnitTestPath] = DscPath
        if len(cls.UnitTestModuleList) > 0:
            cls.RunShellUnitTest = True
            cls.ShellUnitTestLog = os.path.join(cls.WorkspaceRoot, 'Build', "BUILDLOG_UnitTest.txt")
            print('UnitTestModuleList is {}'.format(cls.UnitTestModuleList))

    def BuildUnitTest(self):
        ''' Build specific DSC for modules in UnitTestModuleList '''
        self.env = shell_environment.GetBuildVars()
        self.ws  = PlatformBuilder.GetWorkspaceRoot(self)
        self.mws = MultipleWorkspace()
        self.pp  = ''
        VirtualDrive = os.path.join(self.env.GetValue("BUILD_OUTPUT_BASE"), "VirtualDrive")
        os.makedirs(VirtualDrive, exist_ok=True)

        # DSC by self.GetDscName() should have been built in BUILD process.
        BuiltDsc = [CommonPlatform.GetDscName(",".join(self.env.GetValue("TARGET_ARCH").split(' ')))]
        for UnitTestPath, DscPath in CommonPlatform.UnitTestModuleList.items():
            if DscPath not in BuiltDsc:
                ModuleName = UnitTestPath.split('.inf')[0].rsplit('\\')[-1]
                logging.info('Build {0} for {1}'.format(DscPath, ModuleName))
                BuiltDsc.append(DscPath)
                Arch = self.env.GetValue("TARGET_ARCH").split(" ")
                if 'X64' in Arch:
                    UTArch = 'X64'
                else:
                    UTArch = 'IA32'
                self.env.AllowOverride("ACTIVE_PLATFORM")
                self.env.SetValue("ACTIVE_PLATFORM", DscPath, "Test")
                self.env.AllowOverride("TARGET_ARCH")
                self.env.SetValue("TARGET_ARCH", UTArch, "Test") # Set UnitTest arch the same as Ovmf Shell module.
                ret = PlatformBuilder.Build(self)                # Build specific dsc for UnitTest modules
                if (ret != 0):
                    return ret
            ret = PlatformBuilder.ParseDscFile(self)             # Parse OUTPUT_DIRECTORY from dsc files
            if(ret != 0):
                return ret
            OutputPath = os.path.normpath(os.path.join(self.ws, self.env.GetValue("OUTPUT_DIRECTORY")))
            EfiPath    = os.path.join(OutputPath, self.env.GetValue("TARGET") + "_" + self.env.GetValue("TOOL_CHAIN_TAG"),
                         UTArch, UnitTestPath.split('.inf')[0], "OUTPUT", ModuleName + '.efi')
            print('Copy {0}.efi from:{1}'.format(ModuleName, EfiPath))
            shutil.copy(EfiPath, VirtualDrive)
        return 0

    @staticmethod
    def WriteEfiToStartup(EfiFolder, FileObj):
        ''' Write all the .efi files' name in VirtualDrive into Startup.nsh '''
        for root,dirs,files in os.walk(EfiFolder):
            for file in files:
                if os.path.splitext(file)[1] == '.efi':
                    FileObj.write("{0} \n".format(file))

    @classmethod
    def CheckUnitTestLog(cls):
        ''' Check the boot log for UnitTest '''
        file        = open(cls.ShellUnitTestLog, "r")
        fileContent = file.readlines()
        logging.info('Check the UnitTest boot log:{0}'.format(cls.ShellUnitTestLog))
        for Index in range(len(fileContent)):
            if 'FAILURE MESSAGE:' in fileContent[Index]:
                if fileContent[Index + 1].strip() != '':
                    FailureMessage = fileContent[Index + 1] + fileContent[Index + 2]
                    return FailureMessage
        return 0

import PlatformBuildLib
PlatformBuildLib.CommonPlatform = CommonPlatform
