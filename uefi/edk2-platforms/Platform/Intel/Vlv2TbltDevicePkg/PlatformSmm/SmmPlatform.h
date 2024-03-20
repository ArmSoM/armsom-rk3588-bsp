/*++

  Copyright (c) 2004  - 2019, Intel Corporation. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

Module Name:

  SmmPlatform.h

Abstract:

  Header file for

++*/

#ifndef _PLATFORM_H
#define _PLATFORM_H

#include <PiSmm.h>



#include <Protocol/SmmBase2.h>
#include <Protocol/SmmPowerButtonDispatch2.h>
#include <Protocol/SmmSxDispatch2.h>
#include <Protocol/SmmSwDispatch2.h>
#include <Protocol/SmmIchnDispatch.h>
#include <Protocol/SmmAccess2.h>
#include <Protocol/SmmVariable.h>
#include <Protocol/PciRootBridgeIo.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/GlobalNvsArea.h>
#include <Guid/SetupVariable.h>
#include <Guid/EfiVpdData.h>
#include <Guid/PciLanInfo.h>
#include <IndustryStandard/Pci22.h>

#include "PchAccess.h"
#include "PlatformBaseAddresses.h"

#include <Library/UefiBootServicesTableLib.h>
#include <Library/SmmServicesTableLib.h>
#include <Library/S3BootScriptLib.h>
#include <Library/IoLib.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DevicePathLib.h>
#include <Library/PcdLib.h>
#include <Library/PchPlatformLib.h>
#include <Library/StallSmmLib.h>



typedef struct {
  UINT8     Register;
  UINT8     Function;
  UINT8     Device;
  UINT8     Bus;
  UINT32    ExtendedRegister;
} SMM_PCI_IO_ADDRESS;

typedef struct {
  CHAR8     BoardAaNumber[7];
  UINTN     BoardFabNumber;
} BOARD_AA_NUMBER_DECODE;

//
// BugBug -- Need to get these two values from acpi.h, but right now, they are
//           declared in platform-specific variants of this file, so no easy
//           way to pick-up the include file and work across platforms.
//           Need these definitions to go into a file like common\acpi.h.
//
#define ACPI_ENABLE                 0xA0
#define ACPI_DISABLE                0xA1

#define APM_12_FUNCS                  0x50
#define SMI_SET_SMMVARIABLE_PROTOCOL  0x51  // this is used in Cpu\Pentium\Smm\Base\SmmBase.c

#define SMI_CMD_GET_MSEG_STATUS     0x70
#define SMI_CMD_UPDATE_MSEG_SIZE    0x71
#define SMI_CMD_LOAD_STM            0x72
#define SMI_CMD_UNLOAD_STM          0x73
#define SMI_CMD_GET_SMRAM_RANGES    0x74


#define PCAT_RTC_ADDRESS_REGISTER   0x74
#define PCAT_RTC_DATA_REGISTER      0x75

#define RTC_ADDRESS_SECOND          0x00
#define RTC_ADDRESS_SECOND_ALARM    0x01
#define RTC_ADDRESS_MINUTE          0x02
#define RTC_ADDRESS_MINUTE_ALARM    0x03
#define RTC_ADDRESS_HOUR            0x04
#define RTC_ADDRESS_HOUR_ALARM      0x05

#define RTC_ADDRESS_REGISTER_A      0x0A
#define RTC_ADDRESS_REGISTER_B      0x0B
#define RTC_ADDRESS_REGISTER_C      0x0C
#define RTC_ADDRESS_REGISTER_D      0x0D

#define B_RTC_ALARM_INT_ENABLE      0x20
#define B_RTC_ALARM_INT_STATUS      0x20

#define B_RTC_DATE_ALARM_MASK       0x3F

#define PCAT_CMOS_2_ADDRESS_REGISTER  0x72
#define PCAT_CMOS_2_DATA_REGISTER     0x73

#define EC_C_PORT                     0x66
#define SMC_SMI_DISABLE               0xBC
#define SMC_ENABLE_ACPI_MODE          0xAA  // Enable ACPI mode

#define IO_MISC 156


#define MAXIMUM_NUMBER_OF_PSTATES           12
#define  ICH_SMM_DATA_PORT                  0xB3

#define EFI_IA32_PMG_CST_CONFIG               0x000000E2
#define   B_EFI_CST_CONTROL_LOCK                BIT15
#define   B_EFI_IO_MWAIT_REDIRECTION_ENABLE     BIT10
#define EFI_IA32_PMG_IO_CAPTURE_ADDR          0x000000E4

extern EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL *mPciRootBridgeIo;

//
// Callback function prototypes
//
EFI_STATUS
EFIAPI
PowerButtonCallback (
  IN EFI_HANDLE  DispatchHandle,
  IN CONST VOID  *Context         OPTIONAL,
  IN OUT VOID    *CommBuffer      OPTIONAL,
  IN OUT UINTN   *CommBufferSize  OPTIONAL
  );

EFI_STATUS
EFIAPI
S5SleepAcLossCallBack (
  IN EFI_HANDLE  DispatchHandle,
  IN CONST VOID  *Context         OPTIONAL,
  IN OUT VOID    *CommBuffer      OPTIONAL,
  IN OUT UINTN   *CommBufferSize  OPTIONAL
  );


EFI_STATUS
EFIAPI    
S4S5CallBack (
  IN EFI_HANDLE  DispatchHandle,
  IN CONST VOID  *Context         OPTIONAL,
  IN OUT VOID    *CommBuffer      OPTIONAL,
  IN OUT UINTN   *CommBufferSize  OPTIONAL
  );

EFI_STATUS
EFIAPI    
EnableAcpiCallback (
  IN EFI_HANDLE  DispatchHandle,
  IN CONST VOID  *Context         OPTIONAL,
  IN OUT VOID    *CommBuffer      OPTIONAL,
  IN OUT UINTN   *CommBufferSize  OPTIONAL
  );

EFI_STATUS
EFIAPI
DisableAcpiCallback (
  IN EFI_HANDLE  DispatchHandle,
  IN CONST VOID  *Context         OPTIONAL,
  IN OUT VOID    *CommBuffer      OPTIONAL,
  IN OUT UINTN   *CommBufferSize  OPTIONAL
  );

EFI_STATUS
EFIAPI
SmmReadyToBootCallback (
  IN EFI_HANDLE  DispatchHandle,
  IN CONST VOID  *Context         OPTIONAL,
  IN OUT VOID    *CommBuffer      OPTIONAL,
  IN OUT UINTN   *CommBufferSize  OPTIONAL
  );

VOID
DummyTco1Callback (
  IN  EFI_HANDLE                              DispatchHandle,
  IN  EFI_SMM_ICHN_DISPATCH_CONTEXT           *DispatchContext
  );

VOID
SetAfterG3On (
  BOOLEAN Enable
  );

EFI_STATUS
EFIAPI    
SxSleepEntryCallBack (
  IN EFI_HANDLE  DispatchHandle,
  IN CONST VOID  *Context         OPTIONAL,
  IN OUT VOID    *CommBuffer      OPTIONAL,
  IN OUT UINTN   *CommBufferSize  OPTIONAL
  );

EFI_STATUS
SaveRuntimeScriptTable (
  VOID
  );

#endif

