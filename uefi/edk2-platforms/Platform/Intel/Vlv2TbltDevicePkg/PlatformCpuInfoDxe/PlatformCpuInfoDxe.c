/** @file

  Copyright (c) 2004  - 2019, Intel Corporation. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

Module Name:

  PlatformCpuInfoDxe.c

Abstract:
  Platform Cpu Info driver to public platform related HOB data

--*/

#include "PlatformCpuInfoDxe.h"

CHAR16    EfiPlatformCpuInfoVariable[] = L"PlatformCpuInfo";

EFI_STATUS
EFIAPI
PlatformCpuInfoInit (
  IN EFI_HANDLE                         ImageHandle,
  IN EFI_SYSTEM_TABLE                   *SystemTable
  )
{
  EFI_STATUS                  Status;
  EFI_PLATFORM_CPU_INFO       *PlatformCpuInfoPtr;
  EFI_PEI_HOB_POINTERS        GuidHob;

  //
  // Get Platform Cpu Info HOB
  //
  GuidHob.Raw = GetHobList ();
  while ((GuidHob.Raw = GetNextGuidHob (&gEfiPlatformCpuInfoGuid, GuidHob.Raw)) != NULL) {
    PlatformCpuInfoPtr = GET_GUID_HOB_DATA (GuidHob.Guid);
    GuidHob.Raw = GET_NEXT_HOB (GuidHob);

      //
      // Write the Platform CPU Info to volatile memory for runtime purposes.
      // This must be done in its own driver because SetVariable protocol is dependent on chipset,
      // which is dependent on CpuIo2, PlatformInfo, and Metronome.
      //
      Status = gRT->SetVariable(
                      EfiPlatformCpuInfoVariable,
                      &gEfiVlv2VariableGuid,
                      EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS,
                      sizeof(EFI_PLATFORM_CPU_INFO),
                      PlatformCpuInfoPtr
                      );
      if (EFI_ERROR(Status)) {
        return Status;
      }
  }

   return EFI_SUCCESS;
}

