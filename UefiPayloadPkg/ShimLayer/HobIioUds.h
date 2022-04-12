/**
Copyright (c) 2019-2021, Intel Corporation. All rights reserved.<BR>

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright notice, this
  list of conditions and the following disclaimer in the documentation and/or
  other materials provided with the distribution.
* Neither the name of Intel Corporation nor the names of its contributors may
  be used to endorse or promote products derived from this software without
  specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
  THE POSSIBILITY OF SUCH DAMAGE.

**/

#ifndef _HOB_IIOUDS_H_
#define _HOB_IIOUDS_H_

#include <PiPei.h>

#define IIO_UNIVERSAL_DATA_GUID  { 0x7FF396A1, 0xEE7D, 0x431E, { 0xBA, 0x53, 0x8F, 0xCA, 0x12, 0x7C, 0x44, 0xC0 } }

#define NUMBER_PORTS_PER_SOCKET  21
#define MAX_SOCKET               8  // CONFIG_MAX_SOCKET
#define MaxIIO                   MAX_SOCKET
#define MAX_IIO_STACK            6
#define MAX_IMC                  2
#define MAX_CH                   6
#define MC_MAX_NODE              (MAX_SOCKET * MAX_IMC)
#define MAX_CHA_MAP              4

// Maximum KTI PORTS to be used in structure definition
#if (MAX_SOCKET == 1)
#define MAX_FW_KTI_PORTS  3
#else
#define MAX_FW_KTI_PORTS  6
#endif //(MAX_SOCKET == 1)

#define MAX_LOGIC_IIO_STACK  (MAX_IIO_STACK+2)

// RC version number structure.
typedef struct {
  UINT8     Major;
  UINT8     Minor;
  UINT8     Revision;
  UINT16    BuildNumber;
} RC_VERSION;
// Note: the struct is not packed for a reason: it is not packed in FSP code.
// It is a bug acknowledged by Intel (IPS case 00600003) that has been fixed for SRP
// but won't be fixed for CPX.

#pragma pack(1)

// --------------------------------------------------------------------------------------//
// Structure definitions for Universal Data Store (UDS)
// --------------------------------------------------------------------------------------//

typedef enum {
  TYPE_SCF_BAR = 0,
  TYPE_PCU_BAR,
  TYPE_MEM_BAR0,
  TYPE_MEM_BAR1,
  TYPE_MEM_BAR2,
  TYPE_MEM_BAR3,
  TYPE_MEM_BAR4,
  TYPE_MEM_BAR5,
  TYPE_MEM_BAR6,
  TYPE_MEM_BAR7,
  TYPE_SBREG_BAR,
  TYPE_MAX_MMIO_BAR
} MMIO_BARS;

/**
 IIO PCIe Ports
 **/
typedef enum {
  // IOU0, CSTACK
  PORT_0 = 0,
  // IOU1, PSTACK0
  PORT_1A,
  PORT_1B,
  PORT_1C,
  PORT_1D,
  // IOU2, PSTACK1
  PORT_2A,
  PORT_2B,
  PORT_2C,
  PORT_2D,
  // IOU3, PSTACK2
  PORT_3A,
  PORT_3B,
  PORT_3C,
  PORT_3D,
  MAX_PORTS
} PCIE_PORTS;

/**
 * IIO Stacks
 *
 * Ports    Stack       Stack(HOB)      IioConfigIou
 * =================================================
 * 0        CSTACK      stack 0         IOU0
 * 1A..1D   PSTACK0     stack 1         IOU1
 * 2A..2D   PSTACK1     stack 2         IOU2
 * 3A..3D   PSTACK2     stack 4         IOU3
 */
typedef enum {
  CSTACK = 0,
  PSTACK0,
  PSTACK1,
  PSTACK2 = 4,
  MAX_STACKS
} IIO_STACKS;

typedef struct uint64_t_struct {
  UINT32    lo;
  UINT32    hi;
} UINT64_STRUCT;

typedef struct {
  UINT8    Device;
  UINT8    Function;
} IIO_PORT_INFO;

typedef struct {
  UINT8    Valid;                          // TRUE, if the link is valid (i.e reached normal operation)
  UINT8    PeerSocId;                      // Socket ID
  UINT8    PeerSocType;                    // Socket Type (0 - CPU; 1 - IIO)
  UINT8    PeerPort;                       // Port of the peer socket
} QPI_PEER_DATA;

typedef struct {
  UINT8            Valid;
  UINT32           MmioBar[TYPE_MAX_MMIO_BAR];
  UINT8            PcieSegment;
  UINT64_STRUCT    SegMmcfgBase;
  UINT16           stackPresentBitmap;
  UINT16           M2PciePresentBitmap;
  UINT8            TotM3Kti;
  UINT8            TotCha;
  UINT32           ChaList[MAX_CHA_MAP];
  UINT32           SocId;
  QPI_PEER_DATA    PeerInfo[MAX_FW_KTI_PORTS];               // QPI LEP info
} QPI_CPU_DATA;

typedef struct {
  UINT8            Valid;
  UINT8            SocId;
  QPI_PEER_DATA    PeerInfo[MAX_SOCKET];             // QPI LEP info
} QPI_IIO_DATA;

typedef struct {
  IIO_PORT_INFO    PortInfo[NUMBER_PORTS_PER_SOCKET];
} IIO_DMI_PCIE_INFO;

typedef enum {
  TYPE_UBOX = 0,
  TYPE_UBOX_IIO,
  TYPE_MCP,
  TYPE_FPGA,
  TYPE_HFI,
  TYPE_NAC,
  TYPE_GRAPHICS,
  TYPE_DINO,
  TYPE_RESERVED,
  TYPE_DISABLED,                    // This item must be prior to stack specific disable types
  TYPE_UBOX_IIO_DIS,
  TYPE_MCP_DIS,
  TYPE_FPGA_DIS,
  TYPE_HFI_DIS,
  TYPE_NAC_DIS,
  TYPE_GRAPHICS_DIS,
  TYPE_DINO_DIS,
  TYPE_RESERVED_DIS,
  TYPE_NONE
} STACK_TYPE;

typedef struct _STACK_RES {
  UINT8     Personality;               // see STACK_TYPE for details
  UINT8     BusBase;
  UINT8     BusLimit;
  UINT16    PciResourceIoBase;
  UINT16    PciResourceIoLimit;
  UINT32    IoApicBase;               // Base of IO configured for this stack
  UINT32    IoApicLimit;              // Limit of IO configured for this stack
  UINT32    Mmio32Base;
  UINT32    Mmio32Limit;
  UINT64    Mmio64Base;
  UINT64    Mmio64Limit;
  UINT32    PciResourceMem32Base;
  UINT32    PciResourceMem32Limit;
  UINT64    PciResourceMem64Base;
  UINT64    PciResourceMem64Limit;
  UINT32    VtdBarAddress;
  UINT32    Mmio32MinSize;                       // Minimum required size of MMIO32 resource needed for this stack
} STACK_RES;

typedef struct {
  UINT8                Valid;
  UINT8                SocketID;                 // Socket ID of the IIO (0..3)
  UINT8                BusBase;
  UINT8                BusLimit;
  UINT16               PciResourceIoBase;
  UINT16               PciResourceIoLimit;
  UINT32               IoApicBase;
  UINT32               IoApicLimit;
  UINT32               Mmio32Base;
  UINT32               Mmio32Limit;
  UINT64               Mmio64Base;
  UINT64               Mmio64Limit;
  STACK_RES            StackRes[MAX_LOGIC_IIO_STACK];
  UINT32               RcBaseAddress;
  IIO_DMI_PCIE_INFO    PcieInfo;
  UINT8                DmaDeviceCount;
} IIO_RESOURCE_INSTANCE;

typedef struct {
  UINT16                   PlatGlobalIoBase;        // Global IO Base
  UINT16                   PlatGlobalIoLimit;       // Global IO Limit
  UINT32                   PlatGlobalMmio32Base;    // Global Mmiol base
  UINT32                   PlatGlobalMmio32Limit;   // Global Mmiol limit
  UINT64                   PlatGlobalMmio64Base;    // Global Mmioh Base [43:0]
  UINT64                   PlatGlobalMmio64Limit;   // Global Mmioh Limit [43:0]
  QPI_CPU_DATA             CpuQpiInfo[MAX_SOCKET];  // QPI related info per CPU
  QPI_IIO_DATA             IioQpiInfo[MAX_SOCKET];  // QPI related info per IIO
  UINT32                   MemTsegSize;
  UINT32                   MemIedSize;
  UINT64                   PciExpressBase;
  UINT32                   PciExpressSize;
  UINT32                   MemTolm;
  IIO_RESOURCE_INSTANCE    IIO_resource[MAX_SOCKET];
  UINT8                    numofIIO;
  UINT8                    MaxBusNumber;
  UINT32                   packageBspApicID[MAX_SOCKET];  // This data array is valid only for SBSP, not for non-SBSP CPUs.
  UINT8                    EVMode;
  UINT8                    Pci64BitResourceAllocation;
  UINT8                    SkuPersonality[MAX_SOCKET];
  UINT8                    VMDStackEnable[MaxIIO][MAX_IIO_STACK];
  UINT16                   IoGranularity;
  UINT32                   MmiolGranularity;
  UINT64_STRUCT            MmiohGranularity;
  UINT8                    RemoteRequestThreshold;   // 5370389
  UINT32                   UboxMmioSize;
  UINT32                   MaxAddressBits;
} PLATFORM_DATA;

typedef struct {
  UINT8         CurrentUpiiLinkSpeed;                 // Current programmed UPI Link speed (Slow/Full speed mode)
  UINT8         CurrentUpiLinkFrequency;              // Current requested UPI Link frequency (in GT)
  UINT8         OutKtiCpuSktHotPlugEn;                // 0 - Disabled, 1 - Enabled for PM X2APIC
  UINT32        OutKtiPerLinkL1En[MAX_SOCKET];        // output kti link enabled status for PM
  UINT8         IsocEnable;
  UINT32        meRequestedSize;                 // Size of the memory range requested by ME FW, in MB
  UINT32        ieRequestedSize;                 // Size of the memory range requested by IE FW, in MB
  UINT8         DmiVc1;
  UINT8         DmiVcm;
  UINT32        CpuPCPSInfo;
  UINT8         cpuSubType;
  UINT8         SystemRasType;
  UINT8         numCpus;                  // 1,..4. Total number of CPU packages installed and detected (1..4)by QPI RC
  UINT16        tolmLimit;
  UINT32        tohmLimit;
  RC_VERSION    RcVersion;
  BOOLEAN       MsrTraceEnable;
  UINT8         DdrXoverMode;                 // DDR 2.2 Mode
  // For RAS
  UINT8         bootMode;
  UINT8         OutClusterOnDieEn;                 // Whether RC enabled COD support
  UINT8         OutSncEn;
  UINT8         OutNumOfCluster;
  UINT8         imcEnabled[MAX_SOCKET][MAX_IMC];
  UINT16        LlcSizeReg;
  UINT8         chEnabled[MAX_SOCKET][MAX_CH];
  UINT8         memNode[MC_MAX_NODE];
  UINT8         IoDcMode;
  UINT8         DfxRstCplBitsEn;
} SYSTEM_STATUS;

typedef struct {
  PLATFORM_DATA    PlatformData;
  SYSTEM_STATUS    SystemStatus;
  UINT32           OemValue;
} IIO_UDS;
#pragma pack()

#endif
