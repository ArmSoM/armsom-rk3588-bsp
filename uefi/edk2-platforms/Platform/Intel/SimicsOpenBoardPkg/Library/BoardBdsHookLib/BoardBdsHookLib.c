/** @file
  Board BDS hook Library. Implements board specific BDS hook library

  Copyright (c) 2019 Intel Corporation. All rights reserved. <BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "BoardBdsHook.h"
#include <Guid/RootBridgesConnectedEventGroup.h>
#include <Protocol/FirmwareVolume2.h>

#define LEGACY_8259_MASK_REGISTER_MASTER                  0x21
#define LEGACY_8259_MASK_REGISTER_SLAVE                   0xA1
#define LEGACY_8259_EDGE_LEVEL_TRIGGERED_REGISTER_MASTER  0x4D0
#define LEGACY_8259_EDGE_LEVEL_TRIGGERED_REGISTER_SLAVE   0x4D1

//
// Predefined platform connect sequence
//
EFI_DEVICE_PATH_PROTOCOL    *gPlatformConnectSequence[] = { NULL };


ACPI_HID_DEVICE_PATH       gPnpPs2KeyboardDeviceNode  = gPnpPs2Keyboard;
ACPI_HID_DEVICE_PATH       gPnp16550ComPortDeviceNode = gPnp16550ComPort;
UART_DEVICE_PATH           gUartDeviceNode            = gUart;
VENDOR_DEVICE_PATH         gTerminalTypeDeviceNode    = gPcAnsiTerminal;

//
// Global data
//

VOID          *mEfiDevPathNotifyReg;
EFI_EVENT     mEfiDevPathEvent;
VOID          *mEmuVariableEventReg;
EFI_EVENT     mEmuVariableEvent;
BOOLEAN       mDetectVgaOnly;
UINT16        mHostBridgeDevId;

//
// Table of host IRQs matching PCI IRQs A-D
// (for configuring PCI Interrupt Line register)
//
CONST UINT8 PciHostIrqs[] = {
  0x0a, 0x0a, 0x0b, 0x0b
};

//
// Type definitions
//
typedef
EFI_STATUS
(EFIAPI *PROTOCOL_INSTANCE_CALLBACK)(
  IN EFI_HANDLE           Handle,
  IN VOID                 *Instance,
  IN VOID                 *Context
  );

/**
  @param[in]  Handle - Handle of PCI device instance
  @param[in]  PciIo - PCI IO protocol instance
  @param[in]  Pci - PCI Header register block
**/
typedef
EFI_STATUS
(EFIAPI *VISIT_PCI_INSTANCE_CALLBACK)(
  IN EFI_HANDLE           Handle,
  IN EFI_PCI_IO_PROTOCOL  *PciIo,
  IN PCI_TYPE00           *Pci
  );


//
// Function prototypes
//

EFI_STATUS
VisitAllInstancesOfProtocol (
  IN EFI_GUID                    *Id,
  IN PROTOCOL_INSTANCE_CALLBACK  CallBackFunction,
  IN VOID                        *Context
  );

EFI_STATUS
VisitAllPciInstancesOfProtocol (
  IN VISIT_PCI_INSTANCE_CALLBACK CallBackFunction
  );

VOID
InstallDevicePathCallback (
  VOID
  );

EFI_STATUS
EFIAPI
ConnectRootBridge (
  IN EFI_HANDLE  RootBridgeHandle,
  IN VOID        *Instance,
  IN VOID        *Context
  );


VOID
PlatformRegisterFvBootOption (
  EFI_GUID                         *FileGuid,
  CHAR16                           *Description,
  UINT32                           Attributes
  )
{
  EFI_STATUS                        Status;
  INTN                              OptionIndex;
  EFI_BOOT_MANAGER_LOAD_OPTION      NewOption;
  EFI_BOOT_MANAGER_LOAD_OPTION      *BootOptions;
  UINTN                             BootOptionCount;
  MEDIA_FW_VOL_FILEPATH_DEVICE_PATH FileNode;
  EFI_LOADED_IMAGE_PROTOCOL         *LoadedImage;
  EFI_DEVICE_PATH_PROTOCOL          *DevicePath;

  Status = gBS->HandleProtocol (
                  gImageHandle,
                  &gEfiLoadedImageProtocolGuid,
                  (VOID **) &LoadedImage
                  );
  ASSERT_EFI_ERROR (Status);

  EfiInitializeFwVolDevicepathNode (&FileNode, FileGuid);
  DevicePath = DevicePathFromHandle (LoadedImage->DeviceHandle);
  ASSERT (DevicePath != NULL);
  DevicePath = AppendDevicePathNode (
                 DevicePath,
                 (EFI_DEVICE_PATH_PROTOCOL *) &FileNode
                 );
  ASSERT (DevicePath != NULL);

  Status = EfiBootManagerInitializeLoadOption (
             &NewOption,
             LoadOptionNumberUnassigned,
             LoadOptionTypeBoot,
             Attributes,
             Description,
             DevicePath,
             NULL,
             0
             );
  ASSERT_EFI_ERROR (Status);
  FreePool (DevicePath);

  BootOptions = EfiBootManagerGetLoadOptions (
                  &BootOptionCount, LoadOptionTypeBoot
                  );

  OptionIndex = EfiBootManagerFindLoadOption (
                  &NewOption, BootOptions, BootOptionCount
                  );

  if (OptionIndex == -1) {
    Status = EfiBootManagerAddLoadOptionVariable (&NewOption, MAX_UINTN);
    ASSERT_EFI_ERROR (Status);
  }
  EfiBootManagerFreeLoadOption (&NewOption);
  EfiBootManagerFreeLoadOptions (BootOptions, BootOptionCount);
}

/**
  Remove all MemoryMapped(...)/FvFile(...) and Fv(...)/FvFile(...) boot options
  whose device paths do not resolve exactly to an FvFile in the system.

  This removes any boot options that point to binaries built into the firmware
  and have become stale due to any of the following:
  - DXEFV's base address or size changed (historical),
  - DXEFV's FvNameGuid changed,
  - the FILE_GUID of the pointed-to binary changed,
  - the referenced binary is no longer built into the firmware.

  EfiBootManagerFindLoadOption() used in PlatformRegisterFvBootOption() only
  avoids exact duplicates.
**/
VOID
RemoveStaleFvFileOptions (
  VOID
  )
{
  EFI_BOOT_MANAGER_LOAD_OPTION *BootOptions;
  UINTN                        BootOptionCount;
  UINTN                        Index;

  BootOptions = EfiBootManagerGetLoadOptions (&BootOptionCount,
                  LoadOptionTypeBoot);

  for (Index = 0; Index < BootOptionCount; ++Index) {
    EFI_DEVICE_PATH_PROTOCOL *Node1, *Node2, *SearchNode;
    EFI_STATUS               Status;
    EFI_HANDLE               FvHandle;

    //
    // If the device path starts with neither MemoryMapped(...) nor Fv(...),
    // then keep the boot option.
    //
    Node1 = BootOptions[Index].FilePath;
    if (!(DevicePathType (Node1) == HARDWARE_DEVICE_PATH &&
          DevicePathSubType (Node1) == HW_MEMMAP_DP) &&
        !(DevicePathType (Node1) == MEDIA_DEVICE_PATH &&
          DevicePathSubType (Node1) == MEDIA_PIWG_FW_VOL_DP)) {
      continue;
    }

    //
    // If the second device path node is not FvFile(...), then keep the boot
    // option.
    //
    Node2 = NextDevicePathNode (Node1);
    if (DevicePathType (Node2) != MEDIA_DEVICE_PATH ||
        DevicePathSubType (Node2) != MEDIA_PIWG_FW_FILE_DP) {
      continue;
    }

    //
    // Locate the Firmware Volume2 protocol instance that is denoted by the
    // boot option. If this lookup fails (i.e., the boot option references a
    // firmware volume that doesn't exist), then we'll proceed to delete the
    // boot option.
    //
    SearchNode = Node1;
    Status = gBS->LocateDevicePath (&gEfiFirmwareVolume2ProtocolGuid,
                    &SearchNode, &FvHandle);

    if (!EFI_ERROR (Status)) {
      //
      // The firmware volume was found; now let's see if it contains the FvFile
      // identified by GUID.
      //
      EFI_FIRMWARE_VOLUME2_PROTOCOL     *FvProtocol;
      MEDIA_FW_VOL_FILEPATH_DEVICE_PATH *FvFileNode;
      UINTN                             BufferSize;
      EFI_FV_FILETYPE                   FoundType;
      EFI_FV_FILE_ATTRIBUTES            FileAttributes;
      UINT32                            AuthenticationStatus;

      Status = gBS->HandleProtocol (FvHandle, &gEfiFirmwareVolume2ProtocolGuid,
                      (VOID **)&FvProtocol);
      ASSERT_EFI_ERROR (Status);

      FvFileNode = (MEDIA_FW_VOL_FILEPATH_DEVICE_PATH *)Node2;
      //
      // Buffer==NULL means we request metadata only: BufferSize, FoundType,
      // FileAttributes.
      //
      Status = FvProtocol->ReadFile (
                             FvProtocol,
                             &FvFileNode->FvFileName, // NameGuid
                             NULL,                    // Buffer
                             &BufferSize,
                             &FoundType,
                             &FileAttributes,
                             &AuthenticationStatus
                             );
      if (!EFI_ERROR (Status)) {
        //
        // The FvFile was found. Keep the boot option.
        //
        continue;
      }
    }

    //
    // Delete the boot option.
    //
    Status = EfiBootManagerDeleteLoadOptionVariable (
               BootOptions[Index].OptionNumber, LoadOptionTypeBoot);
    DEBUG_CODE (
      CHAR16 *DevicePathString;

      DevicePathString = ConvertDevicePathToText(BootOptions[Index].FilePath,
                           FALSE, FALSE);
      DEBUG ((
        EFI_ERROR (Status) ? EFI_D_WARN : DEBUG_VERBOSE,
        "%a: removing stale Boot#%04x %s: %r\n",
        __FUNCTION__,
        (UINT32)BootOptions[Index].OptionNumber,
        DevicePathString == NULL ? L"<unavailable>" : DevicePathString,
        Status
        ));
      if (DevicePathString != NULL) {
        FreePool (DevicePathString);
      }
      );
  }

  EfiBootManagerFreeLoadOptions (BootOptions, BootOptionCount);
}

VOID
PlatformRegisterOptionsAndKeys (
  VOID
  )
{
  EFI_STATUS                   Status;
  EFI_INPUT_KEY                Enter;
  EFI_INPUT_KEY                F2;
  EFI_INPUT_KEY                Esc;
  EFI_BOOT_MANAGER_LOAD_OPTION BootOption;

  //
  // Register ENTER as CONTINUE key
  //
  Enter.ScanCode    = SCAN_NULL;
  Enter.UnicodeChar = CHAR_CARRIAGE_RETURN;
  Status = EfiBootManagerRegisterContinueKeyOption (0, &Enter, NULL);
  ASSERT_EFI_ERROR (Status);
  DEBUG ((DEBUG_INFO, "PlatformRegisterOptionsAndKeys\n"));
  if (EFI_ERROR(Status)){
    return;
  }
  //
  // Map F2 to Boot Manager Menu
  //
  F2.ScanCode     = SCAN_F2;
  F2.UnicodeChar  = CHAR_NULL;
  Esc.ScanCode    = SCAN_ESC;
  Esc.UnicodeChar = CHAR_NULL;
  Status = EfiBootManagerGetBootManagerMenu (&BootOption);
  ASSERT_EFI_ERROR (Status);
  Status = EfiBootManagerAddKeyOptionVariable (
             NULL, (UINT16) BootOption.OptionNumber, 0, &F2, NULL
             );
  ASSERT (Status == EFI_SUCCESS || Status == EFI_ALREADY_STARTED);
  Status = EfiBootManagerAddKeyOptionVariable (
             NULL, (UINT16) BootOption.OptionNumber, 0, &Esc, NULL
             );
  ASSERT (Status == EFI_SUCCESS || Status == EFI_ALREADY_STARTED);
}


/**
  Add IsaKeyboard to ConIn; add IsaSerial to ConOut, ConIn, ErrOut.

  @param[in] DeviceHandle  Handle of the LPC Bridge device.

  @retval EFI_SUCCESS  Console devices on the LPC bridge have been added to
                       ConOut, ConIn, and ErrOut.

  @return              Error codes, due to EFI_DEVICE_PATH_PROTOCOL missing
                       from DeviceHandle.
**/
EFI_STATUS
EFIAPI
ConnectRootBridge (
  IN EFI_HANDLE  RootBridgeHandle,
  IN VOID        *Instance,
  IN VOID        *Context
  )
{
  EFI_STATUS Status;

  //
  // Make the PCI bus driver connect the root bridge, non-recursively. This
  // will produce a number of child handles with PciIo on them.
  //
  Status = gBS->ConnectController (
                  RootBridgeHandle, // ControllerHandle
                  NULL,             // DriverImageHandle
                  NULL,             // RemainingDevicePath -- produce all
                                    //   children
                  FALSE             // Recursive
                  );
  return Status;
}


/**
  Add IsaKeyboard to ConIn; add IsaSerial to ConOut, ConIn, ErrOut.

  @param[in] DeviceHandle  Handle of the LPC Bridge device.

  @retval EFI_SUCCESS  Console devices on the LPC bridge have been added to
                       ConOut, ConIn, and ErrOut.

  @return              Error codes, due to EFI_DEVICE_PATH_PROTOCOL missing
                       from DeviceHandle.
**/
EFI_STATUS
PrepareLpcBridgeDevicePath (
  IN EFI_HANDLE                DeviceHandle
  )
{
  EFI_STATUS                Status;
  EFI_DEVICE_PATH_PROTOCOL  *DevicePath;
  EFI_DEVICE_PATH_PROTOCOL  *TempDevicePath;
  CHAR16                    *DevPathStr;

  DevicePath = NULL;
  Status = gBS->HandleProtocol (
                  DeviceHandle,
                  &gEfiDevicePathProtocolGuid,
                  (VOID*)&DevicePath
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }
  TempDevicePath = DevicePath;

  //
  // Register Keyboard
  //
  DevicePath = AppendDevicePathNode (DevicePath, (EFI_DEVICE_PATH_PROTOCOL *)&gPnpPs2KeyboardDeviceNode);

  EfiBootManagerUpdateConsoleVariable (ConIn, DevicePath, NULL);

  //
  // Register COM1
  //
  DevicePath = TempDevicePath;
  gPnp16550ComPortDeviceNode.UID = 0;

  DevicePath = AppendDevicePathNode (DevicePath, (EFI_DEVICE_PATH_PROTOCOL *)&gPnp16550ComPortDeviceNode);
  DevicePath = AppendDevicePathNode (DevicePath, (EFI_DEVICE_PATH_PROTOCOL *)&gUartDeviceNode);
  DevicePath = AppendDevicePathNode (DevicePath, (EFI_DEVICE_PATH_PROTOCOL *)&gTerminalTypeDeviceNode);

  //
  // Print Device Path
  //
  DevPathStr = ConvertDevicePathToText (DevicePath, FALSE, FALSE);
  if (DevPathStr != NULL) {
    DEBUG((
      DEBUG_INFO,
      "BdsPlatform.c+%d: COM%d DevPath: %s\n",
      __LINE__,
      gPnp16550ComPortDeviceNode.UID + 1,
      DevPathStr
      ));
    FreePool(DevPathStr);
  }

  EfiBootManagerUpdateConsoleVariable (ConOut, DevicePath, NULL);
  EfiBootManagerUpdateConsoleVariable (ConIn, DevicePath, NULL);
  EfiBootManagerUpdateConsoleVariable (ErrOut, DevicePath, NULL);

  //
  // Register COM2
  //
  DevicePath = TempDevicePath;
  gPnp16550ComPortDeviceNode.UID = 1;

  DevicePath = AppendDevicePathNode (DevicePath, (EFI_DEVICE_PATH_PROTOCOL *)&gPnp16550ComPortDeviceNode);
  DevicePath = AppendDevicePathNode (DevicePath, (EFI_DEVICE_PATH_PROTOCOL *)&gUartDeviceNode);
  DevicePath = AppendDevicePathNode (DevicePath, (EFI_DEVICE_PATH_PROTOCOL *)&gTerminalTypeDeviceNode);

  //
  // Print Device Path
  //
  DevPathStr = ConvertDevicePathToText (DevicePath, FALSE, FALSE);
  if (DevPathStr != NULL) {
    DEBUG((
      DEBUG_INFO,
      "BdsPlatform.c+%d: COM%d DevPath: %s\n",
      __LINE__,
      gPnp16550ComPortDeviceNode.UID + 1,
      DevPathStr
      ));
    FreePool(DevPathStr);
  }

  EfiBootManagerUpdateConsoleVariable (ConOut, DevicePath, NULL);
  EfiBootManagerUpdateConsoleVariable (ConIn, DevicePath, NULL);
  EfiBootManagerUpdateConsoleVariable (ErrOut, DevicePath, NULL);

  return EFI_SUCCESS;
}

EFI_STATUS
GetGopDevicePath (
   IN  EFI_DEVICE_PATH_PROTOCOL *PciDevicePath,
   OUT EFI_DEVICE_PATH_PROTOCOL **GopDevicePath
   )
{
  UINTN                           Index;
  EFI_STATUS                      Status;
  EFI_HANDLE                      PciDeviceHandle;
  EFI_DEVICE_PATH_PROTOCOL        *TempDevicePath;
  EFI_DEVICE_PATH_PROTOCOL        *TempPciDevicePath;
  UINTN                           GopHandleCount;
  EFI_HANDLE                      *GopHandleBuffer;

  if (PciDevicePath == NULL || GopDevicePath == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // Initialize the GopDevicePath to be PciDevicePath
  //
  *GopDevicePath    = PciDevicePath;
  TempPciDevicePath = PciDevicePath;

  Status = gBS->LocateDevicePath (
                  &gEfiDevicePathProtocolGuid,
                  &TempPciDevicePath,
                  &PciDeviceHandle
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // Try to connect this handle, so that GOP driver could start on this
  // device and create child handles with GraphicsOutput Protocol installed
  // on them, then we get device paths of these child handles and select
  // them as possible console device.
  //
  gBS->ConnectController (PciDeviceHandle, NULL, NULL, FALSE);

  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiGraphicsOutputProtocolGuid,
                  NULL,
                  &GopHandleCount,
                  &GopHandleBuffer
                  );
  if (!EFI_ERROR (Status)) {
    //
    // Add all the child handles as possible Console Device
    //
    for (Index = 0; Index < GopHandleCount; Index++) {
      Status = gBS->HandleProtocol (GopHandleBuffer[Index], &gEfiDevicePathProtocolGuid, (VOID*)&TempDevicePath);
      if (EFI_ERROR (Status)) {
        continue;
      }
      if (CompareMem (
            PciDevicePath,
            TempDevicePath,
            GetDevicePathSize (PciDevicePath) - END_DEVICE_PATH_LENGTH
            ) == 0) {
        //
        // In current implementation, we only enable one of the child handles
        // as console device, i.e. sotre one of the child handle's device
        // path to variable "ConOut"
        // In future, we could select all child handles to be console device
        //

        *GopDevicePath = TempDevicePath;

        //
        // Delete the PCI device's path that added by
        // GetPlugInPciVgaDevicePath(). Add the integrity GOP device path.
        //
        EfiBootManagerUpdateConsoleVariable (ConOutDev, NULL, PciDevicePath);
        EfiBootManagerUpdateConsoleVariable (ConOutDev, TempDevicePath, NULL);
      }
    }
    gBS->FreePool (GopHandleBuffer);
  }

  return EFI_SUCCESS;
}

/**
  Add PCI display to ConOut.

  @param[in] DeviceHandle  Handle of the PCI display device.

  @retval EFI_SUCCESS  The PCI display device has been added to ConOut.

  @return              Error codes, due to EFI_DEVICE_PATH_PROTOCOL missing
                       from DeviceHandle.
**/
EFI_STATUS
PreparePciDisplayDevicePath (
  IN EFI_HANDLE                DeviceHandle
  )
{
  EFI_STATUS                Status;
  EFI_DEVICE_PATH_PROTOCOL  *DevicePath;
  EFI_DEVICE_PATH_PROTOCOL  *GopDevicePath;

  DevicePath    = NULL;
  GopDevicePath = NULL;
  Status = gBS->HandleProtocol (
                  DeviceHandle,
                  &gEfiDevicePathProtocolGuid,
                  (VOID*)&DevicePath
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  GetGopDevicePath (DevicePath, &GopDevicePath);
  DevicePath = GopDevicePath;

  EfiBootManagerUpdateConsoleVariable (ConOut, DevicePath, NULL);

  return EFI_SUCCESS;
}

/**
  Add PCI Serial to ConOut, ConIn, ErrOut.

  @param[in] DeviceHandle  Handle of the PCI serial device.

  @retval EFI_SUCCESS  The PCI serial device has been added to ConOut, ConIn,
                       ErrOut.

  @return              Error codes, due to EFI_DEVICE_PATH_PROTOCOL missing
                       from DeviceHandle.
**/
EFI_STATUS
PreparePciSerialDevicePath (
  IN EFI_HANDLE                DeviceHandle
  )
{
  EFI_STATUS                Status;
  EFI_DEVICE_PATH_PROTOCOL  *DevicePath;

  DevicePath = NULL;
  Status = gBS->HandleProtocol (
                  DeviceHandle,
                  &gEfiDevicePathProtocolGuid,
                  (VOID*)&DevicePath
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  DevicePath = AppendDevicePathNode (DevicePath, (EFI_DEVICE_PATH_PROTOCOL *)&gUartDeviceNode);
  DevicePath = AppendDevicePathNode (DevicePath, (EFI_DEVICE_PATH_PROTOCOL *)&gTerminalTypeDeviceNode);

  EfiBootManagerUpdateConsoleVariable (ConOut, DevicePath, NULL);
  EfiBootManagerUpdateConsoleVariable (ConIn, DevicePath, NULL);
  EfiBootManagerUpdateConsoleVariable (ErrOut, DevicePath, NULL);

  return EFI_SUCCESS;
}

EFI_STATUS
VisitAllInstancesOfProtocol (
  IN EFI_GUID                    *Id,
  IN PROTOCOL_INSTANCE_CALLBACK  CallBackFunction,
  IN VOID                        *Context
  )
{
  EFI_STATUS                Status;
  UINTN                     HandleCount;
  EFI_HANDLE                *HandleBuffer;
  UINTN                     Index;
  VOID                      *Instance;

  //
  // Start to check all the PciIo to find all possible device
  //
  HandleCount = 0;
  HandleBuffer = NULL;
  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  Id,
                  NULL,
                  &HandleCount,
                  &HandleBuffer
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  for (Index = 0; Index < HandleCount; Index++) {
    Status = gBS->HandleProtocol (HandleBuffer[Index], Id, &Instance);
    if (EFI_ERROR (Status)) {
      continue;
    }

    Status = (*CallBackFunction) (
               HandleBuffer[Index],
               Instance,
               Context
               );
  }

  gBS->FreePool (HandleBuffer);

  return EFI_SUCCESS;
}


EFI_STATUS
EFIAPI
VisitingAPciInstance (
  IN EFI_HANDLE  Handle,
  IN VOID        *Instance,
  IN VOID        *Context
  )
{
  EFI_STATUS                Status;
  EFI_PCI_IO_PROTOCOL       *PciIo;
  PCI_TYPE00                Pci;

  PciIo = (EFI_PCI_IO_PROTOCOL*) Instance;

  //
  // Check for all PCI device
  //
  Status = PciIo->Pci.Read (
                    PciIo,
                    EfiPciIoWidthUint32,
                    0,
                    sizeof (Pci) / sizeof (UINT32),
                    &Pci
                    );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return (*(VISIT_PCI_INSTANCE_CALLBACK)(UINTN) Context) (
           Handle,
           PciIo,
           &Pci
           );

}



EFI_STATUS
VisitAllPciInstances (
  IN VISIT_PCI_INSTANCE_CALLBACK CallBackFunction
  )
{
  return VisitAllInstancesOfProtocol (
           &gEfiPciIoProtocolGuid,
           VisitingAPciInstance,
           (VOID*)(UINTN) CallBackFunction
           );
}


/**
  Do platform specific PCI Device check and add them to
  ConOut, ConIn, ErrOut.

  @param[in]  Handle - Handle of PCI device instance
  @param[in]  PciIo - PCI IO protocol instance
  @param[in]  Pci - PCI Header register block

  @retval EFI_SUCCESS - PCI Device check and Console variable update
                        successfully.
  @retval EFI_STATUS - PCI Device check or Console variable update fail.

**/
EFI_STATUS
EFIAPI
DetectAndPreparePlatformPciDevicePath (
  IN EFI_HANDLE           Handle,
  IN EFI_PCI_IO_PROTOCOL  *PciIo,
  IN PCI_TYPE00           *Pci
  )
{
  EFI_STATUS                Status;

  Status = PciIo->Attributes (
    PciIo,
    EfiPciIoAttributeOperationEnable,
    EFI_PCI_DEVICE_ENABLE,
    NULL
    );
  ASSERT_EFI_ERROR (Status);

  if (!mDetectVgaOnly) {
    //
    // Here we decide whether it is LPC Bridge
    //
    if ((IS_PCI_LPC (Pci)) ||
        ((IS_PCI_ISA_PDECODE (Pci)) &&
         (Pci->Hdr.VendorId == 0x8086) &&
         (Pci->Hdr.DeviceId == 0x7000)
        )
       ) {
      //
      // Add IsaKeyboard to ConIn,
      // add IsaSerial to ConOut, ConIn, ErrOut
      //
      DEBUG ((DEBUG_INFO, "Found LPC Bridge device\n"));
      PrepareLpcBridgeDevicePath (Handle);
      return EFI_SUCCESS;
    }
    //
    // Here we decide which Serial device to enable in PCI bus
    //
    if (IS_PCI_16550SERIAL (Pci)) {
      //
      // Add them to ConOut, ConIn, ErrOut.
      //
      DEBUG ((DEBUG_INFO, "Found PCI 16550 SERIAL device\n"));
      PreparePciSerialDevicePath (Handle);
      return EFI_SUCCESS;
    }
  }

  //
  // Here we decide which display device to enable in PCI bus
  //
  if (IS_PCI_DISPLAY (Pci)) {
    //
    // Add them to ConOut.
    //
    DEBUG ((DEBUG_INFO, "Found PCI display device\n"));
    PreparePciDisplayDevicePath (Handle);
    return EFI_SUCCESS;
  }

  return Status;
}


/**
  Do platform specific PCI Device check and add them to ConOut, ConIn, ErrOut

  @param[in]  DetectVgaOnly - Only detect VGA device if it's TRUE.

  @retval EFI_SUCCESS - PCI Device check and Console variable update successfully.
  @retval EFI_STATUS - PCI Device check or Console variable update fail.

**/
EFI_STATUS
DetectAndPreparePlatformPciDevicePaths (
  BOOLEAN DetectVgaOnly
  )
{
  mDetectVgaOnly = DetectVgaOnly;
  return VisitAllPciInstances (DetectAndPreparePlatformPciDevicePath);
}

/**
  Connect the predefined platform default console device.

  Always try to find and enable PCI display devices.

  @param[in] PlatformConsole  Predefined platform default console device array.
**/
VOID
PlatformInitializeConsole (
  IN PLATFORM_CONSOLE_CONNECT_ENTRY   *PlatformConsole
  )
{
  UINTN                              Index;
  EFI_DEVICE_PATH_PROTOCOL           *VarConout;
  EFI_DEVICE_PATH_PROTOCOL           *VarConin;

  //
  // Connect RootBridge
  //
  GetEfiGlobalVariable2 (EFI_CON_OUT_VARIABLE_NAME, (VOID **) &VarConout, NULL);
  GetEfiGlobalVariable2 (EFI_CON_IN_VARIABLE_NAME, (VOID **) &VarConin, NULL);

  if (VarConout == NULL || VarConin == NULL) {
    //
    // Do platform specific PCI Device check and add them to ConOut, ConIn, ErrOut
    //
    DetectAndPreparePlatformPciDevicePaths (FALSE);
    DetectAndPreparePlatformPciDevicePaths(TRUE);
    //
    // Have chance to connect the platform default console,
    // the platform default console is the minimue device group
    // the platform should support
    //
    for (Index = 0; PlatformConsole[Index].DevicePath != NULL; ++Index) {
      //
      // Update the console variable with the connect type
      //
      if ((PlatformConsole[Index].ConnectType & CONSOLE_IN) == CONSOLE_IN) {
        EfiBootManagerUpdateConsoleVariable (ConIn, PlatformConsole[Index].DevicePath, NULL);
      }
      if ((PlatformConsole[Index].ConnectType & CONSOLE_OUT) == CONSOLE_OUT) {
        EfiBootManagerUpdateConsoleVariable (ConOut, PlatformConsole[Index].DevicePath, NULL);
      }
      if ((PlatformConsole[Index].ConnectType & STD_ERROR) == STD_ERROR) {
        EfiBootManagerUpdateConsoleVariable (ErrOut, PlatformConsole[Index].DevicePath, NULL);
      }
    }
  } else {
    //
    // Only detect VGA device and add them to ConOut
    //
    DetectAndPreparePlatformPciDevicePaths (TRUE);
  }
}


/**
  Configure PCI Interrupt Line register for applicable devices
  Ported from SeaBIOS, src/fw/pciinit.c, *_pci_slot_get_irq()

  @param[in]  Handle - Handle of PCI device instance
  @param[in]  PciIo - PCI IO protocol instance
  @param[in]  PciHdr - PCI Header register block

  @retval EFI_SUCCESS - PCI Interrupt Line register configured successfully.

**/
EFI_STATUS
EFIAPI
SetPciIntLine (
  IN EFI_HANDLE           Handle,
  IN EFI_PCI_IO_PROTOCOL  *PciIo,
  IN PCI_TYPE00           *PciHdr
  )
{
  EFI_DEVICE_PATH_PROTOCOL  *DevPathNode;
  EFI_DEVICE_PATH_PROTOCOL  *DevPath;
  UINTN                     RootSlot;
  UINTN                     Idx;
  UINT8                     IrqLine;
  EFI_STATUS                Status;
  UINT32                    RootBusNumber;

  Status = EFI_SUCCESS;

  if (PciHdr->Device.InterruptPin != 0) {

    DevPathNode = DevicePathFromHandle (Handle);
    ASSERT (DevPathNode != NULL);
    DevPath = DevPathNode;

    RootBusNumber = 0;
    if (DevicePathType (DevPathNode) == ACPI_DEVICE_PATH &&
        DevicePathSubType (DevPathNode) == ACPI_DP &&
        ((ACPI_HID_DEVICE_PATH *)DevPathNode)->HID == EISA_PNP_ID(0x0A03)) {
      RootBusNumber = ((ACPI_HID_DEVICE_PATH *)DevPathNode)->UID;
    }

    //
    // Compute index into PciHostIrqs[] table by walking
    // the device path and adding up all device numbers
    //
    Status = EFI_NOT_FOUND;
    RootSlot = 0;
    Idx = PciHdr->Device.InterruptPin - 1;
    while (!IsDevicePathEnd (DevPathNode)) {
      if (DevicePathType (DevPathNode) == HARDWARE_DEVICE_PATH &&
          DevicePathSubType (DevPathNode) == HW_PCI_DP) {

        Idx += ((PCI_DEVICE_PATH *)DevPathNode)->Device;

        //
        // Unlike SeaBIOS, which starts climbing from the leaf device
        // up toward the root, we traverse the device path starting at
        // the root moving toward the leaf node.
        // The slot number of the top-level parent bridge is needed
        // with more than 24 slots on the root bus.
        //
        if (Status != EFI_SUCCESS) {
          Status = EFI_SUCCESS;
          RootSlot = ((PCI_DEVICE_PATH *)DevPathNode)->Device;
        }
      }

      DevPathNode = NextDevicePathNode (DevPathNode);
    }
    if (EFI_ERROR (Status)) {
      return Status;
    }
    if (RootBusNumber == 0 && RootSlot == 0) {
      return Status; //bugbug: workaround; need SIMICS change B0/D0/F0 PCI_IntPin reg(0x3D) = 0X0
//      DEBUG((
//        DEBUG_ERROR,
//       "%a: PCI host bridge (00:00.0) should have no interrupts!\n",
//        __FUNCTION__
//        ));
//      ASSERT (FALSE);
    }

    //
    // Final PciHostIrqs[] index calculation depends on the platform
    // and should match SeaBIOS src/fw/pciinit.c *_pci_slot_get_irq()
    //
    switch (mHostBridgeDevId) {
      case INTEL_82441_DEVICE_ID:
        Idx -= 1;
        break;
      case INTEL_ICH10_DEVICE_ID:
        //
        // SeaBIOS contains the following comment:
        // "Slots 0-24 rotate slot:pin mapping similar to piix above, but
        //  with a different starting index.
        //
        //  Slots 25-31 all use LNKA mapping (or LNKE, but A:D = E:H)"
        //
        if (RootSlot > 24) {
          //
          // in this case, subtract back out RootSlot from Idx
          // (SeaBIOS never adds it to begin with, but that would make our
          //  device path traversal loop above too awkward)
          //
          Idx -= RootSlot;
        }
        break;
      default:
        ASSERT (FALSE); // should never get here
    }
    Idx %= ARRAY_SIZE (PciHostIrqs);
    IrqLine = PciHostIrqs[Idx];

    DEBUG_CODE_BEGIN ();
    {
      CHAR16        *DevPathString;
      STATIC CHAR16 Fallback[] = L"<failed to convert>";
      UINTN         Segment, Bus, Device, Function;

      DevPathString = ConvertDevicePathToText (DevPath, FALSE, FALSE);
      if (DevPathString == NULL) {
        DevPathString = Fallback;
      }
      Status = PciIo->GetLocation (PciIo, &Segment, &Bus, &Device, &Function);
      ASSERT_EFI_ERROR (Status);

      DEBUG ((DEBUG_VERBOSE, "%a: [%02x:%02x.%x] %s -> 0x%02x\n", __FUNCTION__,
        (UINT32)Bus, (UINT32)Device, (UINT32)Function, DevPathString,
        IrqLine));

      if (DevPathString != Fallback) {
        FreePool (DevPathString);
      }
    }
    DEBUG_CODE_END ();

    //
    // Set PCI Interrupt Line register for this device to PciHostIrqs[Idx]
    //
    Status = PciIo->Pci.Write (
               PciIo,
               EfiPciIoWidthUint8,
               PCI_INT_LINE_OFFSET,
               1,
               &IrqLine
               );
  }

  return Status;
}

/**
Write to mask and edge/level triggered registers of master and slave 8259 PICs.

@param[in]  Mask       low byte for master PIC mask register,
high byte for slave PIC mask register.
@param[in]  EdgeLevel  low byte for master PIC edge/level triggered register,
high byte for slave PIC edge/level triggered register.

**/
VOID
Interrupt8259WriteMask(
  IN UINT16  Mask,
  IN UINT16  EdgeLevel
)
{
  IoWrite8(LEGACY_8259_MASK_REGISTER_MASTER, (UINT8)Mask);
  IoWrite8(LEGACY_8259_MASK_REGISTER_SLAVE, (UINT8)(Mask >> 8));
  IoWrite8(LEGACY_8259_EDGE_LEVEL_TRIGGERED_REGISTER_MASTER, (UINT8)EdgeLevel);
  IoWrite8(LEGACY_8259_EDGE_LEVEL_TRIGGERED_REGISTER_SLAVE, (UINT8)(EdgeLevel >> 8));
}

VOID
PciAcpiInitialization (
  VOID
  )
{
  UINTN  Pmba;

  //
  // Query Host Bridge DID to determine platform type
  //
  mHostBridgeDevId = PcdGet16 (PcdSimicsX58HostBridgePciDevId);
  switch (mHostBridgeDevId) {
    case INTEL_82441_DEVICE_ID:
      Pmba = POWER_MGMT_REGISTER_PIIX4 (PIIX4_PMBA);
      //
      // 00:01.0 ISA Bridge (PIIX4) LNK routing targets
      //
      PciWrite8 (PCI_LIB_ADDRESS (0, 1, 0, 0x60), 0x0b); // A
      PciWrite8 (PCI_LIB_ADDRESS (0, 1, 0, 0x61), 0x0b); // B
      PciWrite8 (PCI_LIB_ADDRESS (0, 1, 0, 0x62), 0x0a); // C
      PciWrite8 (PCI_LIB_ADDRESS (0, 1, 0, 0x63), 0x0a); // D
      break;
    case INTEL_ICH10_DEVICE_ID:
      Pmba = POWER_MGMT_REGISTER_ICH10 (ICH10_PMBASE);
      //
      // 00:1f.0 LPC Bridge LNK routing targets
      //
      PciWrite8 (PCI_LIB_ADDRESS (0, 0x1f, 0, 0x60), 0x0a); // A
      PciWrite8 (PCI_LIB_ADDRESS (0, 0x1f, 0, 0x61), 0x0a); // B
      PciWrite8 (PCI_LIB_ADDRESS (0, 0x1f, 0, 0x62), 0x0b); // C
      PciWrite8 (PCI_LIB_ADDRESS (0, 0x1f, 0, 0x63), 0x0b); // D
      PciWrite8 (PCI_LIB_ADDRESS (0, 0x1f, 0, 0x68), 0x0a); // E
      PciWrite8 (PCI_LIB_ADDRESS (0, 0x1f, 0, 0x69), 0x0a); // F
      PciWrite8 (PCI_LIB_ADDRESS (0, 0x1f, 0, 0x6a), 0x0b); // G
      PciWrite8 (PCI_LIB_ADDRESS (0, 0x1f, 0, 0x6b), 0x0b); // H
      break;
    default:
      DEBUG ((DEBUG_ERROR, "%a: Unknown Host Bridge Device ID: 0x%04x\n",
        __FUNCTION__, mHostBridgeDevId));
      ASSERT (FALSE);
      return;
  }

  //
  // Initialize PCI_INTERRUPT_LINE for applicable present PCI devices
  //
  VisitAllPciInstances (SetPciIntLine);

  //
  // Set ACPI SCI_EN bit in PMCNTRL
  //
  IoOr16 ((PciRead32 (Pmba) & ~BIT0) + 4, BIT0);
  //
  // Set all 8259 interrupts to edge triggered and disabled
  //
  Interrupt8259WriteMask(0xFFFF, 0x0000);
}

EFI_STATUS
EFIAPI
ConnectRecursivelyIfPciMassStorage (
  IN EFI_HANDLE           Handle,
  IN EFI_PCI_IO_PROTOCOL  *Instance,
  IN PCI_TYPE00           *PciHeader
  )
{
  EFI_STATUS                Status;
  EFI_DEVICE_PATH_PROTOCOL  *DevicePath;
  CHAR16                    *DevPathStr;

  //
  // Recognize PCI Mass Storage
  //
  if (IS_CLASS1 (PciHeader, PCI_CLASS_MASS_STORAGE)) {
    DevicePath = NULL;
    Status = gBS->HandleProtocol (
                    Handle,
                    &gEfiDevicePathProtocolGuid,
                    (VOID*)&DevicePath
                    );
    if (EFI_ERROR (Status)) {
      return Status;
    }

    //
    // Print Device Path
    //
    DevPathStr = ConvertDevicePathToText (DevicePath, FALSE, FALSE);
    if (DevPathStr != NULL) {
      DEBUG(( DEBUG_INFO, "Found Mass Storage device: %s\n", DevPathStr));
      FreePool(DevPathStr);
    }

    Status = gBS->ConnectController (Handle, NULL, NULL, TRUE);
    if (EFI_ERROR (Status)) {
      return Status;
    }

   }

  return EFI_SUCCESS;
}


/**
  This notification function is invoked when the
  EMU Variable FVB has been changed.

  @param  Event                 The event that occurred
  @param  Context               For EFI compatibility.  Not used.

**/
VOID
EFIAPI
EmuVariablesUpdatedCallback (
  IN  EFI_EVENT Event,
  IN  VOID      *Context
  )
{
  DEBUG ((DEBUG_INFO, "%a called\n", __FUNCTION__));
  UpdateNvVarsOnFileSystem ();
}


EFI_STATUS
EFIAPI
VisitingFileSystemInstance (
  IN EFI_HANDLE  Handle,
  IN VOID        *Instance,
  IN VOID        *Context
  )
{
  EFI_STATUS      Status;
  STATIC BOOLEAN  ConnectedToFileSystem = FALSE;

  if (ConnectedToFileSystem) {
    return EFI_ALREADY_STARTED;
  }

  Status = ConnectNvVarsToFileSystem (Handle);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  ConnectedToFileSystem = TRUE;
  mEmuVariableEvent =
    EfiCreateProtocolNotifyEvent (
      &gEfiDevicePathProtocolGuid,
      TPL_CALLBACK,
      EmuVariablesUpdatedCallback,
      NULL,
      &mEmuVariableEventReg
      );
  PcdSet64S (PcdEmuVariableEvent, (UINT64)(UINTN) mEmuVariableEvent);

  return EFI_SUCCESS;
}


VOID
PlatformBdsRestoreNvVarsFromHardDisk (
  )
{
  VisitAllPciInstances (ConnectRecursivelyIfPciMassStorage);
  VisitAllInstancesOfProtocol (
    &gEfiSimpleFileSystemProtocolGuid,
    VisitingFileSystemInstance,
    NULL
    );
}

/**
  Connect with predefined platform connect sequence.

  The OEM/IBV can customize with their own connect sequence.
**/
VOID
PlatformBdsConnectSequence (
  VOID
  )
{
  UINTN Index;

  DEBUG ((DEBUG_INFO, "%a called\n", __FUNCTION__));

  Index = 0;

  //
  // Here we can get the customized platform connect sequence
  // Notes: we can connect with new variable which record the
  // last time boots connect device path sequence
  //
  while (gPlatformConnectSequence[Index] != NULL) {
    //
    // Build the platform boot option
    //
    EfiBootManagerConnectDevicePath (gPlatformConnectSequence[Index], NULL);
    Index++;
  }

  //
  // Just use the simple policy to connect all devices
  //
  DEBUG ((DEBUG_INFO, "EfiBootManagerConnectAll\n"));
  EfiBootManagerConnectAll ();

  PciAcpiInitialization ();
}

/**
  Do the platform specific action after the console is ready

  Possible things that can be done in PlatformBootManagerAfterConsole:

  > Console post action:
    > Dynamically switch output mode from 100x31 to 80x25 for certain senarino
    > Signal console ready platform customized event
  > Run diagnostics like memory testing
  > Connect certain devices
  > Dispatch aditional option roms
  > Special boot: e.g.: USB boot, enter UI
**/
// VOID
// EFIAPI
// PlatformBootManagerAfterConsole (
//   VOID
//   )
// {

// }

/**
  This notification function is invoked when an instance of the
  EFI_DEVICE_PATH_PROTOCOL is produced.

  @param  Event                 The event that occurred
  @param  Context               For EFI compatibility.  Not used.

**/
VOID
EFIAPI
NotifyDevPath (
  IN  EFI_EVENT Event,
  IN  VOID      *Context
  )
{
  EFI_HANDLE                            Handle;
  EFI_STATUS                            Status;
  UINTN                                 BufferSize;
  EFI_DEVICE_PATH_PROTOCOL             *DevPathNode;
  ATAPI_DEVICE_PATH                    *Atapi;

  //
  // Examine all new handles
  //
  for (;;) {
    //
    // Get the next handle
    //
    BufferSize = sizeof (Handle);
    Status = gBS->LocateHandle (
              ByRegisterNotify,
              NULL,
              mEfiDevPathNotifyReg,
              &BufferSize,
              &Handle
              );

    //
    // If not found, we're done
    //
    if (EFI_NOT_FOUND == Status) {
      break;
    }

    if (EFI_ERROR (Status)) {
      continue;
    }

    //
    // Get the DevicePath protocol on that handle
    //
    Status = gBS->HandleProtocol (Handle, &gEfiDevicePathProtocolGuid, (VOID **)&DevPathNode);
    ASSERT_EFI_ERROR (Status);

    while (!IsDevicePathEnd (DevPathNode)) {
      //
      // Find the handler to dump this device path node
      //
      if (
           (DevicePathType(DevPathNode) == MESSAGING_DEVICE_PATH) &&
           (DevicePathSubType(DevPathNode) == MSG_ATAPI_DP)
         ) {
        Atapi = (ATAPI_DEVICE_PATH*) DevPathNode;
        PciOr16 (
          PCI_LIB_ADDRESS (
            0,
            1,
            1,
            (Atapi->PrimarySecondary == 1) ? 0x42: 0x40
            ),
          BIT15
          );
      }

      //
      // Next device path node
      //
      DevPathNode = NextDevicePathNode (DevPathNode);
    }
  }

  return;
}


VOID
InstallDevicePathCallback (
  VOID
  )
{
  DEBUG ((DEBUG_INFO, "Registered NotifyDevPath Event\n"));
  mEfiDevPathEvent = EfiCreateProtocolNotifyEvent (
                          &gEfiDevicePathProtocolGuid,
                          TPL_CALLBACK,
                          NotifyDevPath,
                          NULL,
                          &mEfiDevPathNotifyReg
                          );
}



/**
  ReadyToBoot callback to set video and text mode for internal shell boot.
  That will not connect USB controller while CSM and FastBoot are disabled, we need to connect them
  before booting to Shell for showing USB devices in Shell.

  When FastBoot is enabled and Windows Console is the chosen Console behavior, input devices will not be connected
  by default. Hence, when booting to EFI shell, connecting input consoles are required.

  @param  Event   Pointer to this event
  @param  Context Event hanlder private data

  @retval None.
**/
VOID
EFIAPI
BdsReadyToBootCallback (
  IN  EFI_EVENT                 Event,
  IN  VOID                      *Context
  )
{
   DEBUG ((DEBUG_INFO, "%a called\n", __FUNCTION__));
}


/**
  This is the callback function for PCI ENUMERATION COMPLETE.

  @param[in] Event      The Event this notify function registered to.
  @param[in] Context    Pointer to the context data registered to the Event.
**/
VOID
EFIAPI
BdsSmmReadyToLockCallback (
  IN EFI_EVENT    Event,
  IN VOID         *Context
  )
{

  VOID                *ProtocolPointer;
  EFI_STATUS          Status;

  //
  // Check if this is first time called by EfiCreateProtocolNotifyEvent() or not,
  // if it is, we will skip it until real event is triggered
  //
  Status = gBS->LocateProtocol (&gEfiDxeSmmReadyToLockProtocolGuid, NULL, (VOID **) &ProtocolPointer);
  if (EFI_SUCCESS != Status) {
    return;
  }

  DEBUG ((DEBUG_INFO, "%a called\n", __FUNCTION__));

  //
  // Dispatch the deferred 3rd party images.
  //
  EfiBootManagerDispatchDeferredImages ();

  //
  // For non-trusted console it must be handled here.
  //
  //UpdateGraphicConOut (FALSE);
}


/**
  This is the callback function for PCI ENUMERATION COMPLETE.

  @param[in] Event      The Event this notify function registered to.
  @param[in] Context    Pointer to the context data registered to the Event.
**/
VOID
EFIAPI
BdsPciEnumCompleteCallback (
  IN EFI_EVENT    Event,
  IN VOID         *Context
  )
{
  VOID                                *ProtocolPointer;
  EFI_STATUS                          Status;
  PLATFORM_CONSOLE_CONNECT_ENTRY      PlatformConsole[3];
  UINTN                               PlatformConsoleCount;
  UINTN                               MaxCount;
  //
  // Check if this is first time called by EfiCreateProtocolNotifyEvent() or not,
  // if it is, we will skip it until real event is triggered
  //
  Status = gBS->LocateProtocol (&gEfiPciEnumerationCompleteProtocolGuid, NULL, (VOID **) &ProtocolPointer);
  if (EFI_SUCCESS != Status) {
    return;
  }

  PlatformConsoleCount = 0;
  MaxCount             = ARRAY_SIZE(PlatformConsole);

  if (PcdGetSize (PcdTrustedConsoleOutputDevicePath) >= sizeof(EFI_DEVICE_PATH_PROTOCOL)) {
    PlatformConsole[PlatformConsoleCount].ConnectType = ConOut;
    PlatformConsole[PlatformConsoleCount].DevicePath = PcdGetPtr (PcdTrustedConsoleOutputDevicePath);
    PlatformConsoleCount++;
  }
  if (PcdGetSize (PcdTrustedConsoleInputDevicePath) >= sizeof(EFI_DEVICE_PATH_PROTOCOL) &&
    PlatformConsoleCount < MaxCount) {
    PlatformConsole[PlatformConsoleCount].ConnectType = ConIn;
    PlatformConsole[PlatformConsoleCount].DevicePath = PcdGetPtr (PcdTrustedConsoleInputDevicePath);
    PlatformConsoleCount++;
  }

  if (PlatformConsoleCount < MaxCount){
    PlatformConsole[PlatformConsoleCount].ConnectType = 0;
    PlatformConsole[PlatformConsoleCount].DevicePath = NULL;
  }else{
    PlatformConsole[MaxCount - 1].ConnectType = 0;
    PlatformConsole[MaxCount - 1].DevicePath = NULL;
  }

  DEBUG ((DEBUG_INFO, "%a called\n", __FUNCTION__));

  PlatformInitializeConsole (PlatformConsole);
}


/**
  Before console after trusted console event callback

  @param[in] Event      The Event this notify function registered to.
  @param[in] Context    Pointer to the context data registered to the Event.
**/
VOID
EFIAPI
BdsBeforeConsoleAfterTrustedConsoleCallback (
  IN EFI_EVENT          Event,
  IN VOID               *Context
  )
{
  EFI_BOOT_MANAGER_LOAD_OPTION  *NvBootOptions;
  UINTN                         NvBootOptionCount;
  UINTN                         Index;
  EFI_STATUS                    Status;

  DEBUG ((DEBUG_INFO, "%a called\n", __FUNCTION__));

  NvBootOptions = EfiBootManagerGetLoadOptions (&NvBootOptionCount, LoadOptionTypeBoot);
  for (Index = 0; Index < NvBootOptionCount; Index++) {
    Status = EfiBootManagerDeleteLoadOptionVariable (NvBootOptions[Index].OptionNumber, LoadOptionTypeBoot);
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: removing Boot#%04x %r\n",
        __FUNCTION__,
        (UINT32) NvBootOptions[Index].OptionNumber,
        Status
        ));
    }
  }

  InstallDevicePathCallback ();

  VisitAllInstancesOfProtocol (&gEfiPciRootBridgeIoProtocolGuid, ConnectRootBridge, NULL);
  //
  // Enable LPC
  //
  PciOr16 (POWER_MGMT_REGISTER_ICH10(0x04), BIT0 | BIT1 | BIT2);

  PlatformRegisterOptionsAndKeys ();
}


/**
  Before console before end of DXE event callback

  @param[in] Event      The Event this notify function registered to.
  @param[in] Context    Pointer to the context data registered to the Event.
**/
VOID
EFIAPI
BdsBeforeConsoleBeforeEndOfDxeGuidCallback (
  IN EFI_EVENT          Event,
  IN VOID               *Context
){
  DEBUG ((DEBUG_INFO, "%a called\n", __FUNCTION__));
}

/**
  After console ready before boot option event callback

  @param[in] Event      The Event this notify function registered to.
  @param[in] Context    Pointer to the context data registered to the Event.
**/
VOID
EFIAPI
BdsAfterConsoleReadyBeforeBootOptionCallback (
  IN EFI_EVENT          Event,
  IN VOID               *Context
  )
{
  EFI_BOOT_MODE                      BootMode;

  DEBUG ((DEBUG_INFO, "%a called\n", __FUNCTION__));

  if (PcdGetBool (PcdOvmfFlashVariablesEnable)) {
    DEBUG ((DEBUG_INFO, "PlatformBdsPolicyBehavior: not restoring NvVars "
      "from disk since flash variables appear to be supported.\n"));
  } else {
    //
    // Try to restore variables from the hard disk early so
    // they can be used for the other BDS connect operations.
    //
    PlatformBdsRestoreNvVarsFromHardDisk ();
  }

  //
  // Get current Boot Mode
  //
  BootMode = GetBootModeHob ();
  DEBUG ((DEBUG_ERROR, "Boot Mode:%x\n", BootMode));

  //
  // Go the different platform policy with different boot mode
  // Notes: this part code can be change with the table policy
  //
  ASSERT (BootMode == BOOT_WITH_FULL_CONFIGURATION);

  //
  // Perform some platform specific connect sequence
  //
  PlatformBdsConnectSequence ();

  //
  // Logo show
  //
  EnableBootLogo(PcdGetPtr(PcdLogoFile));

  EfiBootManagerRefreshAllBootOption ();

  //
  // Register UEFI Shell
  //
  PlatformRegisterFvBootOption (
    PcdGetPtr (PcdShellFile), L"EFI Internal Shell", LOAD_OPTION_ACTIVE
    );

  RemoveStaleFvFileOptions ();
}
