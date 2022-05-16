#!/bin/bash
ToolChain="GCC5"
BuildTarget="DEBUG"
Alignment=0
while getopts "hb:t:" arg; do
  case $arg in
    h)
      echo "usage:"
      echo "  -h: help"
      echo "  -b: build target, default is DEBUG"
      echo "  -t: tool chain, default is GCC5"
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
python UefiPayloadPkg/UniversalPayloadBuild.py -t $ToolChain -b $BuildTarget -a IA32 -D CPU_TIMER_LIB_ENABLE=FALSE
