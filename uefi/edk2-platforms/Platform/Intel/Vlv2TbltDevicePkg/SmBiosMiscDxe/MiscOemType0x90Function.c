/*++

Copyright (c) 1999  - 2020, Intel Corporation.  All rights reserved.
                                                                                   

  SPDX-License-Identifier: BSD-2-Clause-Patent

                                                                                   




Module Name:

  MiscOemType0x88Function.c

Abstract:

  The function that processes the Smbios data type 0x88 before they
  are submitted to Data Hub

--*/

#include "CommonHeader.h"

#include "MiscSubclassDriver.h"
#include <Library/PrintLib.h>
#include <Protocol/DxeSmmReadyToLock.h>
#include <Register/Cpuid.h>
#include <Register/Msr.h>


VOID
GetCPUStepping ( )
{
  CHAR16    Buffer[40];

  UINT32                                FamilyId;
  UINT32                                Model;
  UINT32                                SteppingId;
  CPUID_VERSION_INFO_EAX  Eax;
  CPUID_VERSION_INFO_EBX  Ebx;
  CPUID_VERSION_INFO_ECX  Ecx;
  CPUID_VERSION_INFO_EDX  Edx;

  AsmCpuid (CPUID_VERSION_INFO, &Eax.Uint32, &Ebx.Uint32, &Ecx.Uint32, &Edx.Uint32);
  FamilyId = Eax.Bits.FamilyId;
  if (Eax.Bits.FamilyId == 0x0F) {
    FamilyId |= (Eax.Bits.ExtendedFamilyId << 4);
  }
  Model = Eax.Bits.Model;
  if (Eax.Bits.FamilyId == 0x06 || Eax.Bits.FamilyId == 0x0f) {
    Model |= (Eax.Bits.ExtendedModelId << 4);
  }
  SteppingId = Eax.Bits.SteppingId;

  //
  //Family/Model/Step
  //
  UnicodeSPrint (Buffer, sizeof (Buffer), L"%d/%d/%d", FamilyId,  Model, SteppingId);
  HiiSetString(mHiiHandle,STRING_TOKEN(STR_MISC_PROCESSOR_STEPPING), Buffer, NULL);

}

EFI_STATUS
SearchChildHandle(
  EFI_HANDLE Father,
  EFI_HANDLE *Child
  )
{
  EFI_STATUS                                                 Status;
  UINTN                                                          HandleIndex;
  EFI_GUID                                                     **ProtocolGuidArray = NULL;
  UINTN                                                          ArrayCount;
  UINTN                                                          ProtocolIndex;
  UINTN                                                          OpenInfoCount;
  UINTN                                                          OpenInfoIndex;
  EFI_OPEN_PROTOCOL_INFORMATION_ENTRY  *OpenInfo = NULL;
  UINTN                                                          mHandleCount;
  EFI_HANDLE                                                 *mHandleBuffer= NULL;

  //
  // Retrieve the list of all handles from the handle database
  //
  Status = gBS->LocateHandleBuffer (
                  AllHandles,
                  NULL,
                  NULL,
                  &mHandleCount,
                  &mHandleBuffer
                  );

  for (HandleIndex = 0; HandleIndex < mHandleCount; HandleIndex++) {
    //
    // Retrieve the list of all the protocols on each handle
    //
    Status = gBS->ProtocolsPerHandle (
                    mHandleBuffer[HandleIndex],
                    &ProtocolGuidArray,
                    &ArrayCount
                    );
    if (!EFI_ERROR (Status)) {
      for (ProtocolIndex = 0; ProtocolIndex < ArrayCount; ProtocolIndex++) {
        Status = gBS->OpenProtocolInformation (
                        mHandleBuffer[HandleIndex],
                        ProtocolGuidArray[ProtocolIndex],
                        &OpenInfo,
                        &OpenInfoCount
                        );

        if (!EFI_ERROR (Status)) {
          for (OpenInfoIndex = 0; OpenInfoIndex < OpenInfoCount; OpenInfoIndex++) {
            if(OpenInfo[OpenInfoIndex].AgentHandle == Father) {
              if ((OpenInfo[OpenInfoIndex].Attributes & EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER) == EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER) {
                *Child = mHandleBuffer[HandleIndex];
		  Status = EFI_SUCCESS;
		  goto TryReturn;
              }
            }
          }
	   Status = EFI_NOT_FOUND;
       }
      }
      if(OpenInfo != NULL) {
        FreePool(OpenInfo);
	      OpenInfo = NULL;
      }
    }
    if(ProtocolGuidArray != NULL) {
    FreePool (ProtocolGuidArray);
    ProtocolGuidArray = NULL;
    }
  }
TryReturn:
  if(OpenInfo != NULL) {
    FreePool (OpenInfo);
    OpenInfo = NULL;
  }
  if(ProtocolGuidArray != NULL) {
    FreePool(ProtocolGuidArray);
    ProtocolGuidArray = NULL;
  }
  if(mHandleBuffer != NULL) {
    FreePool (mHandleBuffer);
    mHandleBuffer = NULL;
  }
  return Status;
}

EFI_STATUS
JudgeHandleIsPCIDevice(
  EFI_HANDLE    Handle,
  UINT8            Device,
  UINT8            Funs
  )
{
  EFI_STATUS  Status;
  EFI_DEVICE_PATH   *DPath;

  Status = gBS->HandleProtocol (
                  Handle,
                  &gEfiDevicePathProtocolGuid,
                  (VOID **) &DPath
                  );
  if(!EFI_ERROR(Status)) {
    while(!IsDevicePathEnd(DPath)) {
      if((DPath->Type == HARDWARE_DEVICE_PATH) && (DPath->SubType == HW_PCI_DP)) {
        PCI_DEVICE_PATH   *PCIPath;
        PCIPath = (PCI_DEVICE_PATH*) DPath;
        DPath = NextDevicePathNode(DPath);

        if(IsDevicePathEnd(DPath) && (PCIPath->Device == Device) && (PCIPath->Function == Funs)) {
          return EFI_SUCCESS;
        }
      } else {
        DPath = NextDevicePathNode(DPath);
      }
    }
  }
  return EFI_UNSUPPORTED;
}

EFI_STATUS
GetDriverName(
  EFI_HANDLE   Handle
)
{
  EFI_DRIVER_BINDING_PROTOCOL        *BindHandle = NULL;
  EFI_STATUS                         Status;
  UINT32                             Version;
  UINT16                             *Ptr;
  CHAR16                             Buffer[40];
  STRING_REF                  TokenToUpdate;
  Status = gBS->OpenProtocol(
                  Handle,
                  &gEfiDriverBindingProtocolGuid,
                  (VOID**)&BindHandle,
                   NULL,
                   NULL,
                   EFI_OPEN_PROTOCOL_GET_PROTOCOL
                   );

  if (EFI_ERROR(Status)) {
    return EFI_NOT_FOUND;
  }

  Version = BindHandle->Version;
  Ptr = (UINT16*)&Version;
  UnicodeSPrint(Buffer, sizeof (Buffer), L"%d.%d.%d", Version >> 24 , (Version >>16)& 0x0f ,*(Ptr));

  TokenToUpdate = (STRING_REF)STR_MISC_GOP_VERSION;
  HiiSetString(mHiiHandle, TokenToUpdate, Buffer, NULL);

  return EFI_SUCCESS;
}

EFI_STATUS
GetGOPDriverName()
{
  UINTN                         HandleCount;
  EFI_HANDLE                *Handles= NULL;
  UINTN                         Index;
  EFI_STATUS                Status;
  EFI_HANDLE                Child = 0;

  Status = gBS->LocateHandleBuffer(
		              ByProtocol,
		              &gEfiDriverBindingProtocolGuid,
		              NULL,
		              &HandleCount,
		              &Handles
                  );

  for (Index = 0; Index < HandleCount ; Index++) {
    Status = SearchChildHandle(Handles[Index], &Child);
    if(!EFI_ERROR(Status)) {
      Status = JudgeHandleIsPCIDevice(Child, 0x02, 0x00);
      if(!EFI_ERROR(Status)) {
        return GetDriverName(Handles[Index]);
      }
    }
  }
  return EFI_UNSUPPORTED;
}

VOID
GetUcodeVersion()
{
  UINT32                   MicroCodeVersion;
  CHAR16                   Buffer[40];

  //
  // Microcode Revision
  //
  AsmWriteMsr64 (MSR_IA32_BIOS_SIGN_ID, 0);
  AsmCpuid (CPUID_VERSION_INFO, NULL, NULL, NULL, NULL);
  MicroCodeVersion = (UINT32) RShiftU64 (AsmReadMsr64 (MSR_IA32_BIOS_SIGN_ID), 32);
  UnicodeSPrint (Buffer, sizeof (Buffer), L"%x", MicroCodeVersion);
  HiiSetString(mHiiHandle,STRING_TOKEN(STR_MISC_UCODE_VERSION), Buffer, NULL);
}

/**
  Publish the smbios OEM type 0x90.

  @param Event   - Event whose notification function is being invoked (gEfiDxeSmmReadyToLockProtocolGuid).
  @param Context - Pointer to the notification functions context, which is implementation dependent.

  @retval None

**/
EFI_STATUS
EFIAPI
AddSmbiosT0x90Callback (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  EFI_STATUS            Status;
  UINTN                 SECVerStrLen = 0;
  UINTN                 uCodeVerStrLen = 0;
  UINTN                 GOPStrLen = 0;
  UINTN                 SteppingStrLen = 0;
  SMBIOS_TABLE_TYPE90    *SmbiosRecord;
  EFI_SMBIOS_HANDLE     SmbiosHandle;
  CHAR16                *SECVer;
  CHAR16                *uCodeVer;
  CHAR16                *GOPVer;
  CHAR16                *Stepping;
  STRING_REF            TokenToGet;
  CHAR8                 *OptionalStrStart;
  UINTN                 OptionalStrSize;
  EFI_SMBIOS_PROTOCOL               *SmbiosProtocol;

  DEBUG ((EFI_D_INFO, "Executing SMBIOS T0x90 callback.\n"));

  gBS->CloseEvent (Event);    // Unload this event.

  //
  // First check for invalid parameters.
  //
  if (Context == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Status = gBS->LocateProtocol (
                  &gEfiSmbiosProtocolGuid,
                  NULL,
                  (VOID *) &SmbiosProtocol
                  );
  ASSERT_EFI_ERROR (Status);

  GetUcodeVersion();
  GetGOPDriverName();
  GetCPUStepping();

  TokenToGet = STRING_TOKEN (STR_MISC_SEC_VERSION);
  SECVer = SmbiosMiscGetString (TokenToGet);
  SECVerStrLen = StrLen(SECVer);
  if (SECVerStrLen > SMBIOS_STRING_MAX_LENGTH) {
    return EFI_UNSUPPORTED;
  }

  TokenToGet = STRING_TOKEN (STR_MISC_UCODE_VERSION);
  uCodeVer = SmbiosMiscGetString (TokenToGet);
  uCodeVerStrLen = StrLen(uCodeVer);
  if (uCodeVerStrLen > SMBIOS_STRING_MAX_LENGTH) {
    return EFI_UNSUPPORTED;
  }

  TokenToGet = STRING_TOKEN (STR_MISC_GOP_VERSION);
  GOPVer = SmbiosMiscGetString (TokenToGet);
  GOPStrLen = StrLen(GOPVer);
  if (GOPStrLen > SMBIOS_STRING_MAX_LENGTH) {
    return EFI_UNSUPPORTED;
  }

  TokenToGet = STRING_TOKEN (STR_MISC_PROCESSOR_STEPPING);
  Stepping = SmbiosMiscGetString (TokenToGet);
  SteppingStrLen = StrLen(Stepping);


  if (SteppingStrLen > SMBIOS_STRING_MAX_LENGTH) {
    return EFI_UNSUPPORTED;
  }

  OptionalStrSize = SECVerStrLen + 1 + uCodeVerStrLen + 1 + GOPStrLen + 1 + SteppingStrLen + 1 + 1;
  SmbiosRecord = AllocatePool(sizeof (SMBIOS_TABLE_TYPE90) + OptionalStrSize);
  ZeroMem(SmbiosRecord, sizeof (SMBIOS_TABLE_TYPE90) + OptionalStrSize);

  SmbiosRecord->Hdr.Type = EFI_SMBIOS_TYPE_FIRMWARE_VERSION_INFO;
  SmbiosRecord->Hdr.Length = sizeof (SMBIOS_TABLE_TYPE90);

  //
  // Make handle chosen by smbios protocol.add automatically.
  //
  SmbiosRecord->Hdr.Handle = 0;

  //
  // SEC VERSION will be the 1st optional string following the formatted structure.
  //
  SmbiosRecord->SECVersion = 0;

  //
  // Microcode VERSION will be the 2nd optional string following the formatted structure.
  //
  SmbiosRecord->uCodeVersion = 2;

  //
  // GOP VERSION will be the 3rd optional string following the formatted structure.
  //
  SmbiosRecord->GOPVersion = 3;

  //
  // CPU Stepping will be the 4th optional string following the formatted structure.
  //
  SmbiosRecord->CpuStepping = 4;

  OptionalStrStart = (CHAR8 *)(SmbiosRecord + 1);
  UnicodeStrToAsciiStrS (SECVer, OptionalStrStart, OptionalStrSize);
  OptionalStrStart += (SECVerStrLen + 1);
  OptionalStrSize  -= (SECVerStrLen + 1);
  UnicodeStrToAsciiStrS (uCodeVer, OptionalStrStart, OptionalStrSize);
  OptionalStrStart += (uCodeVerStrLen + 1);
  OptionalStrSize  -= (uCodeVerStrLen + 1);
  UnicodeStrToAsciiStrS (GOPVer, OptionalStrStart, OptionalStrSize);
  OptionalStrStart += (GOPStrLen + 1);
  OptionalStrSize  -= (GOPStrLen + 1);
  UnicodeStrToAsciiStrS (Stepping, OptionalStrStart, OptionalStrSize);

  //
  // Now we have got the full smbios record, call smbios protocol to add this record.
  //
  SmbiosHandle = SMBIOS_HANDLE_PI_RESERVED;
  Status = SmbiosProtocol-> Add(
                              SmbiosProtocol,
                              NULL,
                              &SmbiosHandle,
                              (EFI_SMBIOS_TABLE_HEADER *) SmbiosRecord
                              );

  FreePool(SmbiosRecord);
  return Status;
}


/**
  This function makes boot time changes to the contents of the
  MiscOemType0x90 (Type 0x90).

  @param  RecordData                 Pointer to copy of RecordData from the Data Table.

  @retval EFI_SUCCESS                All parameters were valid.
  @retval EFI_UNSUPPORTED            Unexpected RecordType value.
  @retval EFI_INVALID_PARAMETER      Invalid parameter was found.

**/
MISC_SMBIOS_TABLE_FUNCTION(MiscOemType0x90)
{
  EFI_STATUS                    Status;
  static BOOLEAN                CallbackIsInstalledT0x90 = FALSE;
  VOID                           *AddSmbiosT0x90CallbackNotifyReg;
  EFI_EVENT                      AddSmbiosT0x90CallbackEvent;

  //
  // This callback will create a OEM Type 0x90 record.
  //
  if (CallbackIsInstalledT0x90 == FALSE) {
    CallbackIsInstalledT0x90 = TRUE;        	// Prevent more than 1 callback.
    DEBUG ((EFI_D_INFO, "Create Smbios T0x90 callback.\n"));

  //
  // gEfiDxeSmmReadyToLockProtocolGuid is ready
  //
  Status = gBS->CreateEvent (
                  EVT_NOTIFY_SIGNAL,
                  TPL_CALLBACK,
                  (EFI_EVENT_NOTIFY)AddSmbiosT0x90Callback,
                  RecordData,
                  &AddSmbiosT0x90CallbackEvent
                  );

  ASSERT_EFI_ERROR (Status);
  if (EFI_ERROR (Status)) {
    return Status;

  }

  Status = gBS->RegisterProtocolNotify (
                  &gEfiDxeSmmReadyToLockProtocolGuid,
                  AddSmbiosT0x90CallbackEvent,
                  &AddSmbiosT0x90CallbackNotifyReg
                  );

  return Status;
  }

  return EFI_SUCCESS;

}
