/** @file
  Reset System Library functions for Simics ICH10

  Copyright (c) 2006 - 2019 Intel Corporation. All rights reserved. <BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Base.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/TimerLib.h>
#include <Register/X58Ich10.h>


VOID
AcpiPmControl (
  UINTN SuspendType
  )
{
  ASSERT (SuspendType < 6);
  DEBUG((EFI_D_ERROR, "SuspendType = 0x%x\n", SuspendType));

  IoBitFieldWrite16 (ICH10_PMBASE_IO + 4, 10, 13, (UINT16) SuspendType);
  IoOr16 (ICH10_PMBASE_IO + 0x04, BIT13);
  CpuDeadLoop ();
}

/**
  Calling this function causes a system-wide reset. This sets
  all circuitry within the system to its initial state. This type of reset
  is asynchronous to system operation and operates without regard to
  cycle boundaries.

  System reset should not return, if it returns, it means the system does
  not support cold reset.
**/
VOID
EFIAPI
ResetCold (
  VOID
  )
{
  DEBUG((EFI_D_ERROR, "ResetCold_CF9\n"));
  IoWrite8 (0xCF9, BIT3 | BIT2 | BIT1); // 1st choice: PIIX3 RCR, RCPU|SRST
  MicroSecondDelay (50);

  DEBUG((EFI_D_ERROR, "ResetCold_Port64\n"));
  IoWrite8 (0x64, 0xfe);         // 2nd choice: keyboard controller
  CpuDeadLoop ();
}

/**
  Calling this function causes a system-wide initialization. The processors
  are set to their initial state, and pending cycles are not corrupted.

  System reset should not return, if it returns, it means the system does
  not support warm reset.
**/
VOID
EFIAPI
ResetWarm (
  VOID
  )
{
  DEBUG((EFI_D_ERROR, "ResetWarm\n"));
  //
  //BUGBUG workaround for warm reset
  //
  IoWrite8(0xCF9, BIT2 | BIT1);
  MicroSecondDelay(50);

  IoWrite8 (0x64, 0xfe);
  CpuDeadLoop ();
}

/**
  Calling this function causes the system to enter a power state equivalent
  to the ACPI G2/S5 or G3 states.

  System shutdown should not return, if it returns, it means the system does
  not support shut down reset.
**/
VOID
EFIAPI
ResetShutdown (
  VOID
  )
{
  DEBUG((EFI_D_ERROR, "ResetShutdown\n"));
  AcpiPmControl (0);
  ASSERT (FALSE);
}


/**
  Calling this function causes the system to enter a power state for capsule
  update.

  Reset update should not return, if it returns, it means the system does
  not support capsule update.

**/
VOID
EFIAPI
EnterS3WithImmediateWake (
  VOID
  )
{
  DEBUG((EFI_D_ERROR, "EnterS3WithImmediateWake\n"));
  AcpiPmControl (1);
  ASSERT (FALSE);
}

/**
  This function causes a systemwide reset. The exact type of the reset is
  defined by the EFI_GUID that follows the Null-terminated Unicode string passed
  into ResetData. If the platform does not recognize the EFI_GUID in ResetData
  the platform must pick a supported reset type to perform.The platform may
  optionally log the parameters from any non-normal reset that occurs.

  @param[in]  DataSize   The size, in bytes, of ResetData.
  @param[in]  ResetData  The data buffer starts with a Null-terminated string,
                         followed by the EFI_GUID.
**/
VOID
EFIAPI
ResetPlatformSpecific (
  IN UINTN   DataSize,
  IN VOID    *ResetData
  )
{
  DEBUG((EFI_D_ERROR, "ResetPlatformSpecific\n"));
  ResetCold ();
}
