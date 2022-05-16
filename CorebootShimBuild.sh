#!/bin/bash
BuildTarget="DEBUG"
ToolChain="GCC5"
while getopts "hb:t:" arg; do
  case $arg in
    h)
      echo "usage:"
      echo "  -h: help"
      echo "  -b: build target, default is DEBUG"
	    echo "  -t: build toolchain, default is GCC5"
      exit 0
      ;;
    b)
      BuildTarget=$OPTARG
      ;;
    t)
      ToolChain=$OPTARG
      ;;
  esac
done

while [ $# -gt 0 ]; do
  shift
done

source ./edksetup.sh
make -C ./BaseTools
build -p UefiPayloadPkg/UefiPayloadPkg.dsc -b $BuildTarget -a IA32 -a X64 -D BOOTLOADER=COREBOOT -m UefiPayloadPkg/ShimLayer/ShimLayer.inf -t $ToolChain

cp Build/UefiPayloadPkgX64/$BuildTarget"_"$ToolChain/IA32/UefiPayloadPkg/ShimLayer/ShimLayer/DEBUG/ShimLayer.dll Build/UefiPayloadPkgX64/ShimLayer.elf