/*++

Copyright (c) 2009 - 2020, Intel Corporation. All rights reserved.<BR>
                                                                                   

  SPDX-License-Identifier: BSD-2-Clause-Patent

                                                                                   



Module Name:

  MiscSystemSlotDesignatorFunction.c

Abstract:

  BIOS system slot designator information boot time changes.
  SMBIOS type 9.

--*/

#include "MiscSubclassDriver.h"

/**
  This function makes boot time changes to the contents of the
  MiscSystemSlotDesignator structure (Type 9).

  @param  RecordData                 Pointer to copy of RecordData from the Data Table.

  @retval EFI_SUCCESS                All parameters were valid.
  @retval EFI_UNSUPPORTED            Unexpected RecordType value.
  @retval EFI_INVALID_PARAMETER      Invalid parameter was found.

**/
MISC_SMBIOS_TABLE_FUNCTION(MiscSystemSlotDesignation)
{
  CHAR8                              *OptionalStrStart;
  UINTN                              SlotDesignationStrLen;
  EFI_STATUS                         Status;
  EFI_STRING                         SlotDesignation;
  STRING_REF                         TokenToGet;
  SMBIOS_TABLE_TYPE9                 *SmbiosRecord;
  EFI_SMBIOS_HANDLE                  SmbiosHandle;
  EFI_MISC_SYSTEM_SLOT_DESIGNATION*  ForType9InputData;

  ForType9InputData = (EFI_MISC_SYSTEM_SLOT_DESIGNATION *)RecordData;

  //
  // First check for invalid parameters.
  //
  if (RecordData == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  TokenToGet = 0;
  switch (ForType9InputData->SlotDesignation) {
    case STR_MISC_SYSTEM_SLOT_PCIEX16_1:
      TokenToGet = STRING_TOKEN (STR_MISC_SYSTEM_SLOT_PCIEX16_1);
      break;
    case STR_MISC_SYSTEM_SLOT_PCIEX16_2:
      TokenToGet = STRING_TOKEN (STR_MISC_SYSTEM_SLOT_PCIEX16_2);
      break;
    case STR_MISC_SYSTEM_SLOT_PCIEX4:
      TokenToGet = STRING_TOKEN (STR_MISC_SYSTEM_SLOT_PCIEX4);
      break;
    case STR_MISC_SYSTEM_SLOT_PCIEX1_1:
      TokenToGet = STRING_TOKEN (STR_MISC_SYSTEM_SLOT_PCIEX1_1);
      break;
    case STR_MISC_SYSTEM_SLOT_PCIEX1_2:
      TokenToGet = STRING_TOKEN (STR_MISC_SYSTEM_SLOT_PCIEX1_2);
      break;
    case STR_MISC_SYSTEM_SLOT_PCIEX1_3:
      TokenToGet = STRING_TOKEN (STR_MISC_SYSTEM_SLOT_PCIEX1_3);
      break;
    case STR_MISC_SYSTEM_SLOT_PCI1:
      TokenToGet = STRING_TOKEN (STR_MISC_SYSTEM_SLOT_PCI1);
      break;
    case STR_MISC_SYSTEM_SLOT_PCI2:
      TokenToGet = STRING_TOKEN (STR_MISC_SYSTEM_SLOT_PCI2);
      break;
    case STR_MISC_SYSTEM_SLOT_PCI3:
      TokenToGet = STRING_TOKEN (STR_MISC_SYSTEM_SLOT_PCI3);
      break;
    default:
      break;
  }

  SlotDesignation = SmbiosMiscGetString (TokenToGet);
  SlotDesignationStrLen = StrLen(SlotDesignation);
  if (SlotDesignationStrLen > SMBIOS_STRING_MAX_LENGTH) {
    return EFI_UNSUPPORTED;
  }

  //
  // Two zeros following the last string.
  //
  SmbiosRecord = AllocatePool(sizeof (SMBIOS_TABLE_TYPE9) + SlotDesignationStrLen + 1 + 1);
  ZeroMem(SmbiosRecord, sizeof (SMBIOS_TABLE_TYPE9) +SlotDesignationStrLen + 1 + 1);

  SmbiosRecord->Hdr.Type = EFI_SMBIOS_TYPE_SYSTEM_SLOTS;
  SmbiosRecord->Hdr.Length = sizeof (SMBIOS_TABLE_TYPE9);
  SmbiosRecord->Hdr.Handle = 0;
  SmbiosRecord->SlotDesignation = 1;
  SmbiosRecord->SlotType = (UINT8)ForType9InputData->SlotType;
  SmbiosRecord->SlotDataBusWidth = (UINT8)ForType9InputData->SlotDataBusWidth;
  SmbiosRecord->CurrentUsage = (UINT8)ForType9InputData->SlotUsage;
  SmbiosRecord->SlotLength = (UINT8)ForType9InputData->SlotLength;
  SmbiosRecord->SlotID = ForType9InputData->SlotId;

  //
  // Slot Characteristics
  //
  CopyMem ((UINT8 *) &SmbiosRecord->SlotCharacteristics1,(UINT8 *) &ForType9InputData->SlotCharacteristics,2);
  OptionalStrStart = (CHAR8 *)(SmbiosRecord + 1);
  UnicodeStrToAsciiStrS (SlotDesignation, OptionalStrStart, SlotDesignationStrLen + 1 + 1);
  //
  // Now we have got the full smbios record, call smbios protocol to add this record.
  //
  SmbiosHandle = SMBIOS_HANDLE_PI_RESERVED;
  Status = Smbios-> Add(
                      Smbios,
                      NULL,
                      &SmbiosHandle,
                      (EFI_SMBIOS_TABLE_HEADER *) SmbiosRecord
                      );
  FreePool(SmbiosRecord);
  return Status;
}
