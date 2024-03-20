/** @file
  PCH SPI Common Driver implements the SPI Host Controller Compatibility Interface.

  Copyright (c) 2019 Intel Corporation. All rights reserved. <BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi/UefiBaseType.h>
#include <Library/IoLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <IndustryStandard/Pci30.h>
#include <PchAccess.h>
#include <Protocol/Spi.h>
#include <IncludePrivate/Library/PchSpiCommonLib.h>
#include <Register/X58Ich10.h>

/**
  Initialize an SPI protocol instance.

  @param[in] SpiInstance          Pointer to SpiInstance to initialize

  @retval EFI_SUCCESS             The protocol instance was properly initialized
  @exception EFI_UNSUPPORTED      The PCH is not supported by this module
**/
EFI_STATUS
SpiProtocolConstructor (
  IN     SPI_INSTANCE       *SpiInstance
  )
{
  UINTN           PchSpiBar0;

  //
  // Initialize the SPI protocol instance
  //
  SpiInstance->Signature                    = PCH_SPI_PRIVATE_DATA_SIGNATURE;
  SpiInstance->Handle                       = NULL;
  SpiInstance->SpiProtocol.Revision         = PCH_SPI_SERVICES_REVISION;
  SpiInstance->SpiProtocol.FlashRead        = SpiProtocolFlashRead;
  SpiInstance->SpiProtocol.FlashWrite       = SpiProtocolFlashWrite;
  SpiInstance->SpiProtocol.FlashErase       = SpiProtocolFlashErase;
  SpiInstance->SpiProtocol.FlashReadSfdp    = SpiProtocolFlashReadSfdp;
  SpiInstance->SpiProtocol.FlashReadJedecId = SpiProtocolFlashReadJedecId;
  SpiInstance->SpiProtocol.FlashWriteStatus = SpiProtocolFlashWriteStatus;
  SpiInstance->SpiProtocol.FlashReadStatus  = SpiProtocolFlashReadStatus;
  SpiInstance->SpiProtocol.GetRegionAddress = SpiProtocolGetRegionAddress;
  SpiInstance->SpiProtocol.ReadPchSoftStrap = SpiProtocolReadPchSoftStrap;
  SpiInstance->SpiProtocol.ReadCpuSoftStrap = SpiProtocolReadCpuSoftStrap;

  SpiInstance->PchAcpiBase = ICH10_PMBASE_IO;
  ASSERT (SpiInstance->PchAcpiBase != 0);

  PchSpiBar0 = RCRB + SPIBAR;

  if (PchSpiBar0 == 0) {
    DEBUG ((DEBUG_ERROR, "ERROR : PchSpiBar0 is invalid!\n"));
    ASSERT (FALSE);
  }

  if ((MmioRead32 (PchSpiBar0 + R_PCH_SPI_HSFSC) & B_PCH_SPI_HSFSC_FDV) == 0) {
    DEBUG ((DEBUG_ERROR, "ERROR : SPI Flash Signature invalid, cannot use the Hardware Sequencing registers!\n"));
    ASSERT (FALSE);
  }
  SpiInstance->ReadPermission = 0xffff;
  SpiInstance->WritePermission = 0xffff;
  DEBUG ((DEBUG_INFO, "Flash Region Permission: Read- 0x%04x; Write= 0x%04x\n",
                                                SpiInstance->ReadPermission,
                                                SpiInstance->WritePermission));

  //
  SpiInstance->TotalFlashSize = PcdGet32(PcdFlashAreaSize);
  DEBUG ((DEBUG_INFO, "Total Flash Size : %0x\n", SpiInstance->TotalFlashSize));
  return EFI_SUCCESS;
}

/**
  Delay for at least the request number of microseconds for Runtime usage.

  @param[in] ABase                Acpi base address
  @param[in] Microseconds         Number of microseconds to delay.

**/
VOID
EFIAPI
PchPmTimerStallRuntimeSafe (
  IN  UINT16  ABase,
  IN  UINTN   Microseconds
  )
{
  UINTN   Ticks;
  UINTN   Counts;
  UINTN   CurrentTick;
  UINTN   OriginalTick;
  UINTN   RemainingTick;

  if (Microseconds == 0) {
    return;
  }

  OriginalTick   = IoRead32 ((UINTN) (ABase + R_PCH_ACPI_PM1_TMR)) & B_PCH_ACPI_PM1_TMR_VAL;
  CurrentTick    = OriginalTick;

  //
  // The timer frequency is 3.579545 MHz, so 1 ms corresponds 3.58 clocks
  //
  Ticks = Microseconds * 358 / 100 + OriginalTick + 1;

  //
  // The loops needed by timer overflow
  //
  Counts = Ticks / V_PCH_ACPI_PM1_TMR_MAX_VAL;

  //
  // Remaining clocks within one loop
  //
  RemainingTick = Ticks % V_PCH_ACPI_PM1_TMR_MAX_VAL;

  //
  // not intend to use TMROF_STS bit of register PM1_STS, because this adds extra
  // one I/O operation, and maybe generate SMI
  //
  while ((Counts != 0) || (RemainingTick > CurrentTick)) {
    CurrentTick = IoRead32 ((UINTN) (ABase + R_PCH_ACPI_PM1_TMR)) & B_PCH_ACPI_PM1_TMR_VAL;
    //
    // Check if timer overflow
    //
    if ((CurrentTick < OriginalTick)) {
      if (Counts != 0) {
        Counts--;
      } else {
        //
        // If timer overflow and Counts equ to 0, that means we already stalled more than
        // RemainingTick, break the loop here
        //
        break;
      }
    }

    OriginalTick = CurrentTick;
  }
}

/**
  Read data from the flash part.

  @param[in] This                 Pointer to the PCH_SPI_PROTOCOL instance.
  @param[in] FlashRegionType      The Flash Region type for flash cycle which is listed in the Descriptor.
  @param[in] Address              The Flash Linear Address must fall within a region for which BIOS has access permissions.
  @param[in] ByteCount            Number of bytes in the data portion of the SPI cycle.
  @param[out] Buffer              The Pointer to caller-allocated buffer containing the dada received.
                                  It is the caller's responsibility to make sure Buffer is large enough for the total number of bytes read.

  @retval EFI_SUCCESS             Command succeed.
  @retval EFI_INVALID_PARAMETER   The parameters specified are not valid.
  @retval EFI_DEVICE_ERROR        Device error, command aborts abnormally.
**/
EFI_STATUS
EFIAPI
SpiProtocolFlashRead (
  IN     EFI_SPI_PROTOCOL   *This,
  IN     FLASH_REGION_TYPE  FlashRegionType,
  IN     UINT32             Address,
  IN     UINT32             ByteCount,
  OUT    UINT8              *Buffer
  )
{
  EFI_STATUS        Status;

  //
  // Sends the command to the SPI interface to execute.
  //
  Status = SendSpiCmd (
             This,
             FlashRegionType,
             FlashCycleRead,
             Address,
             ByteCount,
             Buffer
             );
  return Status;
}

/**
  Write data to the flash part.

  @param[in] This                 Pointer to the PCH_SPI_PROTOCOL instance.
  @param[in] FlashRegionType      The Flash Region type for flash cycle which is listed in the Descriptor.
  @param[in] Address              The Flash Linear Address must fall within a region for which BIOS has access permissions.
  @param[in] ByteCount            Number of bytes in the data portion of the SPI cycle.
  @param[in] Buffer               Pointer to caller-allocated buffer containing the data sent during the SPI cycle.

  @retval EFI_SUCCESS             Command succeed.
  @retval EFI_INVALID_PARAMETER   The parameters specified are not valid.
  @retval EFI_DEVICE_ERROR        Device error, command aborts abnormally.
**/
EFI_STATUS
EFIAPI
SpiProtocolFlashWrite (
  IN     EFI_SPI_PROTOCOL   *This,
  IN     FLASH_REGION_TYPE  FlashRegionType,
  IN     UINT32             Address,
  IN     UINT32             ByteCount,
  IN     UINT8              *Buffer
  )
{
  EFI_STATUS        Status;

  //
  // Sends the command to the SPI interface to execute.
  //
  Status = SendSpiCmd (
             This,
             FlashRegionType,
             FlashCycleWrite,
             Address,
             ByteCount,
             Buffer
             );
  return Status;
}

/**
  Erase some area on the flash part.

  @param[in] This                 Pointer to the PCH_SPI_PROTOCOL instance.
  @param[in] FlashRegionType      The Flash Region type for flash cycle which is listed in the Descriptor.
  @param[in] Address              The Flash Linear Address must fall within a region for which BIOS has access permissions.
  @param[in] ByteCount            Number of bytes in the data portion of the SPI cycle.

  @retval EFI_SUCCESS             Command succeed.
  @retval EFI_INVALID_PARAMETER   The parameters specified are not valid.
  @retval EFI_DEVICE_ERROR        Device error, command aborts abnormally.
**/
EFI_STATUS
EFIAPI
SpiProtocolFlashErase (
  IN     EFI_SPI_PROTOCOL   *This,
  IN     FLASH_REGION_TYPE  FlashRegionType,
  IN     UINT32             Address,
  IN     UINT32             ByteCount
  )
{
  EFI_STATUS        Status;

  //
  // Sends the command to the SPI interface to execute.
  //
  Status = SendSpiCmd (
             This,
             FlashRegionType,
             FlashCycleErase,
             Address,
             ByteCount,
             NULL
             );
  return Status;
}

/**
  Read SFDP data from the flash part.

  @param[in] This                 Pointer to the PCH_SPI_PROTOCOL instance.
  @param[in] ComponentNumber      The Componen Number for chip select
  @param[in] Address              The starting byte address for SFDP data read.
  @param[in] ByteCount            Number of bytes in SFDP data portion of the SPI cycle
  @param[out] SfdpData            The Pointer to caller-allocated buffer containing the SFDP data received
                                  It is the caller's responsibility to make sure Buffer is large enough for the total number of bytes read

  @retval EFI_SUCCESS             Command succeed.
  @retval EFI_INVALID_PARAMETER   The parameters specified are not valid.
  @retval EFI_DEVICE_ERROR        Device error, command aborts abnormally.
**/
EFI_STATUS
EFIAPI
SpiProtocolFlashReadSfdp (
  IN     EFI_SPI_PROTOCOL   *This,
  IN     UINT8              ComponentNumber,
  IN     UINT32             Address,
  IN     UINT32             ByteCount,
  OUT    UINT8              *SfdpData
  )
{
  SPI_INSTANCE      *SpiInstance;
  EFI_STATUS        Status;
  UINT32            FlashAddress;

  SpiInstance       = SPI_INSTANCE_FROM_SPIPROTOCOL (This);
  Status            = EFI_SUCCESS;

  if (ComponentNumber > SpiInstance->NumberOfComponents) {
    ASSERT (FALSE);
    return EFI_INVALID_PARAMETER;
  }

  FlashAddress = 0;
  if (ComponentNumber == FlashComponent1) {
    FlashAddress = SpiInstance->Component1StartAddr;
  }
  FlashAddress += Address;
  //
  // Sends the command to the SPI interface to execute.
  //
  Status = SendSpiCmd (
             This,
             FlashRegionAll,
             FlashCycleReadSfdp,
             FlashAddress,
             ByteCount,
             SfdpData
             );
  return Status;
}

/**
  Read Jedec Id from the flash part.

  @param[in] This                 Pointer to the PCH_SPI_PROTOCOL instance.
  @param[in] ComponentNumber      The Componen Number for chip select
  @param[in] ByteCount            Number of bytes in JedecId data portion of the SPI cycle, the data size is 3 typically
  @param[out] JedecId             The Pointer to caller-allocated buffer containing JEDEC ID received
                                  It is the caller's responsibility to make sure Buffer is large enough for the total number of bytes read.

  @retval EFI_SUCCESS             Command succeed.
  @retval EFI_INVALID_PARAMETER   The parameters specified are not valid.
  @retval EFI_DEVICE_ERROR        Device error, command aborts abnormally.
**/
EFI_STATUS
EFIAPI
SpiProtocolFlashReadJedecId (
  IN     EFI_SPI_PROTOCOL   *This,
  IN     UINT8              ComponentNumber,
  IN     UINT32             ByteCount,
  OUT    UINT8              *JedecId
  )
{
  SPI_INSTANCE      *SpiInstance;
  EFI_STATUS        Status;
  UINT32            Address;

  SpiInstance       = SPI_INSTANCE_FROM_SPIPROTOCOL (This);
  Status            = EFI_SUCCESS;

  if (ComponentNumber > SpiInstance->NumberOfComponents) {
    ASSERT (FALSE);
    return EFI_INVALID_PARAMETER;
  }

  Address = 0;
  if (ComponentNumber == FlashComponent1) {
    Address = SpiInstance->Component1StartAddr;
  }

  //
  // Sends the command to the SPI interface to execute.
  //
  Status = SendSpiCmd (
             This,
             FlashRegionAll,
             FlashCycleReadJedecId,
             Address,
             ByteCount,
             JedecId
             );
  return Status;
}

/**
  Write the status register in the flash part.

  @param[in] This                 Pointer to the PCH_SPI_PROTOCOL instance.
  @param[in] ByteCount            Number of bytes in Status data portion of the SPI cycle, the data size is 1 typically
  @param[in] StatusValue          The Pointer to caller-allocated buffer containing the value of Status register writing

  @retval EFI_SUCCESS             Command succeed.
  @retval EFI_INVALID_PARAMETER   The parameters specified are not valid.
  @retval EFI_DEVICE_ERROR        Device error, command aborts abnormally.
**/
EFI_STATUS
EFIAPI
SpiProtocolFlashWriteStatus (
  IN     EFI_SPI_PROTOCOL   *This,
  IN     UINT32             ByteCount,
  IN     UINT8              *StatusValue
  )
{
  EFI_STATUS        Status;

  //
  // Sends the command to the SPI interface to execute.
  //
  Status = SendSpiCmd (
             This,
             FlashRegionAll,
             FlashCycleWriteStatus,
             0,
             ByteCount,
             StatusValue
             );
  return Status;
}

/**
  Read status register in the flash part.

  @param[in] This                 Pointer to the PCH_SPI_PROTOCOL instance.
  @param[in] ByteCount            Number of bytes in Status data portion of the SPI cycle, the data size is 1 typically
  @param[out] StatusValue         The Pointer to caller-allocated buffer containing the value of Status register received.

  @retval EFI_SUCCESS             Command succeed.
  @retval EFI_INVALID_PARAMETER   The parameters specified are not valid.
  @retval EFI_DEVICE_ERROR        Device error, command aborts abnormally.
**/
EFI_STATUS
EFIAPI
SpiProtocolFlashReadStatus (
  IN     EFI_SPI_PROTOCOL   *This,
  IN     UINT32             ByteCount,
  OUT    UINT8              *StatusValue
  )
{
  EFI_STATUS        Status;

  //
  // Sends the command to the SPI interface to execute.
  //
  Status = SendSpiCmd (
             This,
             FlashRegionAll,
             FlashCycleReadStatus,
             0,
             ByteCount,
             StatusValue
             );
  return Status;
}

/**
  Get the SPI region base and size, based on the enum type

  @param[in] This                 Pointer to the PCH_SPI_PROTOCOL instance.
  @param[in] FlashRegionType      The Flash Region type for for the base address which is listed in the Descriptor.
  @param[out] BaseAddress         The Flash Linear Address for the Region 'n' Base
  @param[out] RegionSize          The size for the Region 'n'

  @retval EFI_SUCCESS             Read success
  @retval EFI_INVALID_PARAMETER   Invalid region type given
  @retval EFI_DEVICE_ERROR        The region is not used
**/
EFI_STATUS
EFIAPI
SpiProtocolGetRegionAddress (
  IN     EFI_SPI_PROTOCOL   *This,
  IN     FLASH_REGION_TYPE  FlashRegionType,
  OUT    UINT32             *BaseAddress,
  OUT    UINT32             *RegionSize
  )
{
  SPI_INSTANCE    *SpiInstance;
  UINTN           PchSpiBar0;
  UINT32          ReadValue;

  SpiInstance     = SPI_INSTANCE_FROM_SPIPROTOCOL (This);

  if (FlashRegionType >= FlashRegionMax) {
    return EFI_INVALID_PARAMETER;
  }

  if (FlashRegionType == FlashRegionAll) {
    *BaseAddress  = 0;
    *RegionSize   = SpiInstance->TotalFlashSize;
    return EFI_SUCCESS;
  }

  PchSpiBar0      = AcquireSpiBar0 (SpiInstance);
  ReadValue = MmioRead32 (PchSpiBar0 + (R_PCH_SPI_FREG0_FLASHD + (S_PCH_SPI_FREGX * ((UINT32) FlashRegionType))));
  ReleaseSpiBar0 (SpiInstance);

  //
  // If the region is not used, the Region Base is 7FFFh and Region Limit is 0000h
  //
  if (ReadValue == B_PCH_SPI_FREGX_BASE_MASK) {
    return EFI_DEVICE_ERROR;
  }
  *BaseAddress = ((ReadValue & B_PCH_SPI_FREGX_BASE_MASK) >> N_PCH_SPI_FREGX_BASE) <<
                   N_PCH_SPI_FREGX_BASE_REPR;
  //
  // Region limit address Bits[11:0] are assumed to be FFFh
  //
  *RegionSize = ((((ReadValue & B_PCH_SPI_FREGX_LIMIT_MASK) >> N_PCH_SPI_FREGX_LIMIT) + 1) <<
                  N_PCH_SPI_FREGX_LIMIT_REPR) - *BaseAddress;

  return EFI_SUCCESS;
}

/**
  Read PCH Soft Strap Values

  @param[in] This                 Pointer to the PCH_SPI_PROTOCOL instance.
  @param[in] SoftStrapAddr        PCH Soft Strap address offset from FPSBA.
  @param[in] ByteCount            Number of bytes in SoftStrap data portion of the SPI cycle
  @param[out] SoftStrapValue      The Pointer to caller-allocated buffer containing PCH Soft Strap Value.
                                  If the value of ByteCount is 0, the data type of SoftStrapValue should be UINT16 and SoftStrapValue will be PCH Soft Strap Length
                                  It is the caller's responsibility to make sure Buffer is large enough for the total number of bytes read.

  @retval EFI_SUCCESS             Command succeed.
  @retval EFI_INVALID_PARAMETER   The parameters specified are not valid.
  @retval EFI_DEVICE_ERROR        Device error, command aborts abnormally.
**/
EFI_STATUS
EFIAPI
SpiProtocolReadPchSoftStrap (
  IN     EFI_SPI_PROTOCOL   *This,
  IN     UINT32             SoftStrapAddr,
  IN     UINT32             ByteCount,
  OUT    VOID               *SoftStrapValue
  )
{
  SPI_INSTANCE      *SpiInstance;
  UINT32            StrapFlashAddr;
  EFI_STATUS        Status;

  SpiInstance     = SPI_INSTANCE_FROM_SPIPROTOCOL (This);

  if (ByteCount == 0) {
    *(UINT16 *) SoftStrapValue = SpiInstance->PchStrapSize;
    return EFI_SUCCESS;
  }

  if ((SoftStrapAddr + ByteCount) > (UINT32) SpiInstance->PchStrapSize) {
    ASSERT (FALSE);
    return EFI_INVALID_PARAMETER;
  }

  //
  // PCH Strap Flash Address = FPSBA + RamAddr
  //
  StrapFlashAddr = SpiInstance->PchStrapBaseAddr + SoftStrapAddr;

  //
  // Read PCH Soft straps from using execute command
  //
  Status = SendSpiCmd (
             This,
             FlashRegionDescriptor,
             FlashCycleRead,
             StrapFlashAddr,
             ByteCount,
             SoftStrapValue
             );
  return Status;
}

/**
  Read CPU Soft Strap Values

  @param[in] This                 Pointer to the PCH_SPI_PROTOCOL instance.
  @param[in] SoftStrapAddr        CPU Soft Strap address offset from FCPUSBA.
  @param[in] ByteCount            Number of bytes in SoftStrap data portion of the SPI cycle.
  @param[out] SoftStrapValue      The Pointer to caller-allocated buffer containing CPU Soft Strap Value.
                                  If the value of ByteCount is 0, the data type of SoftStrapValue should be UINT16 and SoftStrapValue will be PCH Soft Strap Length
                                  It is the caller's responsibility to make sure Buffer is large enough for the total number of bytes read.

  @retval EFI_SUCCESS             Command succeed.
  @retval EFI_INVALID_PARAMETER   The parameters specified are not valid.
  @retval EFI_DEVICE_ERROR        Device error, command aborts abnormally.
**/
EFI_STATUS
EFIAPI
SpiProtocolReadCpuSoftStrap (
  IN     EFI_SPI_PROTOCOL   *This,
  IN     UINT32             SoftStrapAddr,
  IN     UINT32             ByteCount,
  OUT    VOID               *SoftStrapValue
  )
{
  SPI_INSTANCE      *SpiInstance;
  UINT32            StrapFlashAddr;
  EFI_STATUS        Status;

  SpiInstance     = SPI_INSTANCE_FROM_SPIPROTOCOL (This);

  if (ByteCount == 0) {
    *(UINT16 *) SoftStrapValue = SpiInstance->CpuStrapSize;
    return EFI_SUCCESS;
  }

  if ((SoftStrapAddr + ByteCount) > (UINT32) SpiInstance->CpuStrapSize) {
    ASSERT (FALSE);
    return EFI_INVALID_PARAMETER;
  }

  //
  // CPU Strap Flash Address = FCPUSBA + RamAddr
  //
  StrapFlashAddr = SpiInstance->CpuStrapBaseAddr + SoftStrapAddr;

  //
  // Read Cpu Soft straps from using execute command
  //
  Status = SendSpiCmd (
             This,
             FlashRegionDescriptor,
             FlashCycleRead,
             StrapFlashAddr,
             ByteCount,
             SoftStrapValue
             );
  return Status;
}

/**
  This function sends the programmed SPI command to the slave device.

  @param[in] This                 Pointer to the PCH_SPI_PROTOCOL instance.
  @param[in] SpiRegionType        The SPI Region type for flash cycle which is listed in the Descriptor
  @param[in] FlashCycleType       The Flash SPI cycle type list in HSFC (Hardware Sequencing Flash Control Register) register
  @param[in] Address              The Flash Linear Address must fall within a region for which BIOS has access permissions.
  @param[in] ByteCount            Number of bytes in the data portion of the SPI cycle.
  @param[in,out] Buffer           Pointer to caller-allocated buffer containing the dada received or sent during the SPI cycle.

  @retval EFI_SUCCESS             SPI command completes successfully.
  @retval EFI_DEVICE_ERROR        Device error, the command aborts abnormally.
  @retval EFI_ACCESS_DENIED       Some unrecognized command encountered in hardware sequencing mode
  @retval EFI_INVALID_PARAMETER   The parameters specified are not valid.
**/
EFI_STATUS
SendSpiCmd (
  IN     EFI_SPI_PROTOCOL   *This,
  IN     FLASH_REGION_TYPE  FlashRegionType,
  IN     FLASH_CYCLE_TYPE   FlashCycleType,
  IN     UINT32             Address,
  IN     UINT32             ByteCount,
  IN OUT UINT8              *Buffer
  )
{
  EFI_STATUS      Status;
  UINT32          Index;
  SPI_INSTANCE    *SpiInstance;
  UINTN           PchSpiBar0;
  UINT32          HardwareSpiAddr;
  UINT32          FlashRegionSize;
  UINT32          SpiDataCount;
  UINT32          FlashCycle;
  UINT32          SmiEnSave;
  UINT16          ABase;

  Status            = EFI_SUCCESS;
  SpiInstance       = SPI_INSTANCE_FROM_SPIPROTOCOL (This);
  PchSpiBar0        = AcquireSpiBar0 (SpiInstance);
  ABase             = SpiInstance->PchAcpiBase;

  //
  // Disable SMIs to make sure normal mode flash access is not interrupted by an SMI
  // whose SMI handler accesses flash (e.g. for error logging)
  //
  // *** NOTE: if the SMI_LOCK bit is set (i.e., PMC PCI Offset A0h [4]='1'),
  // clearing B_GBL_SMI_EN will not have effect. In this situation, some other
  // synchronization methods must be applied here or in the consumer of the
  // SendSpiCmd. An example method is disabling the specific SMI sources
  // whose SMI handlers access flash before flash cycle and re-enabling the SMI
  // sources after the flash cycle .
  //
  SmiEnSave   = IoRead32 ((UINTN) (ABase + R_PCH_SMI_EN));
  IoWrite32 ((UINTN) (ABase + R_PCH_SMI_EN), SmiEnSave & (UINT32) (~B_PCH_SMI_EN_GBL_SMI));

  //
  // If it's write cycle, disable Prefetching, Caching and disable BIOS Write Protect
  //
  if ((FlashCycleType == FlashCycleWrite) ||
      (FlashCycleType == FlashCycleErase)) {
    Status = DisableBiosWriteProtect ();
    if (EFI_ERROR (Status)) {
      goto SendSpiCmdEnd;
    }
  }
  //
  // Make sure it's safe to program the command.
  //
  if (!WaitForSpiCycleComplete (This, PchSpiBar0, FALSE)) {
    Status = EFI_DEVICE_ERROR;
    goto SendSpiCmdEnd;
  }

  Status = SpiProtocolGetRegionAddress (This, FlashRegionType, &HardwareSpiAddr, &FlashRegionSize);
  if (EFI_ERROR (Status)) {
    goto SendSpiCmdEnd;
  }
  HardwareSpiAddr += Address;
  if ((Address + ByteCount) > FlashRegionSize) {
    Status = EFI_INVALID_PARAMETER;
    goto SendSpiCmdEnd;
  }

  //
  // Check for PCH SPI hardware sequencing required commands
  //
  FlashCycle = 0;
  switch (FlashCycleType) {
    case FlashCycleRead:
      FlashCycle = (UINT32) (V_PCH_SPI_HSFSC_CYCLE_READ << N_PCH_SPI_HSFSC_CYCLE);
      break;
    case FlashCycleWrite:
      FlashCycle = (UINT32) (V_PCH_SPI_HSFSC_CYCLE_WRITE << N_PCH_SPI_HSFSC_CYCLE);
      break;
    case FlashCycleErase:
      if (((ByteCount % SIZE_4KB) != 0) ||
          ((HardwareSpiAddr % SIZE_4KB) != 0)) {
        ASSERT (FALSE);
        Status = EFI_INVALID_PARAMETER;
        goto SendSpiCmdEnd;
      }
      break;
    case FlashCycleReadSfdp:
      FlashCycle = (UINT32) (V_PCH_SPI_HSFSC_CYCLE_READ_SFDP << N_PCH_SPI_HSFSC_CYCLE);
      break;
    case FlashCycleReadJedecId:
      FlashCycle = (UINT32) (V_PCH_SPI_HSFSC_CYCLE_READ_JEDEC_ID << N_PCH_SPI_HSFSC_CYCLE);
      break;
    case FlashCycleWriteStatus:
      FlashCycle = (UINT32) (V_PCH_SPI_HSFSC_CYCLE_WRITE_STATUS << N_PCH_SPI_HSFSC_CYCLE);
      break;
    case FlashCycleReadStatus:
      FlashCycle = (UINT32) (V_PCH_SPI_HSFSC_CYCLE_READ_STATUS << N_PCH_SPI_HSFSC_CYCLE);
      break;
    default:
      //
      // Unrecognized Operation
      //
      ASSERT (FALSE);
      Status = EFI_INVALID_PARAMETER;
      goto SendSpiCmdEnd;
      break;
  }

  do {
    SpiDataCount = ByteCount;
    if ((FlashCycleType == FlashCycleRead) ||
        (FlashCycleType == FlashCycleWrite) ||
        (FlashCycleType == FlashCycleReadSfdp)) {
      //
      // Trim at 256 byte boundary per operation,
      // - PCH SPI controller requires trimming at 4KB boundary
      // - Some SPI chips require trimming at 256 byte boundary for write operation
      // - Trimming has limited performance impact as we can read / write atmost 64 byte
      //   per operation
      //
      if (HardwareSpiAddr + ByteCount > ((HardwareSpiAddr + BIT8) &~(BIT8 - 1))) {
        SpiDataCount = (((UINT32) (HardwareSpiAddr) + BIT8) &~(BIT8 - 1)) - (UINT32) (HardwareSpiAddr);
      }
      //
      // Calculate the number of bytes to shift in/out during the SPI data cycle.
      // Valid settings for the number of bytes duing each data portion of the
      // PCH SPI cycles are: 0, 1, 2, 3, 4, 5, 6, 7, 8, 16, 24, 32, 40, 48, 56, 64
      //
      if (SpiDataCount >= 64) {
        SpiDataCount = 64;
      } else if ((SpiDataCount &~0x07) != 0) {
        SpiDataCount = SpiDataCount &~0x07;
      }
    }
    if (FlashCycleType == FlashCycleErase) {
      if (((ByteCount / SIZE_64KB) != 0) &&
          ((ByteCount % SIZE_64KB) == 0) &&
          ((HardwareSpiAddr % SIZE_64KB) == 0)) {
        if (HardwareSpiAddr < SpiInstance->Component1StartAddr) {
          //
          // Check whether Component0 support 64k Erase
          //
          if ((SpiInstance->SfdpVscc0Value & B_PCH_SPI_SFDPX_VSCCX_EO_64K) != 0) {
            SpiDataCount = SIZE_64KB;
          } else {
            SpiDataCount = SIZE_4KB;
          }
        } else {
          //
          // Check whether Component1 support 64k Erase
          //
          if ((SpiInstance->SfdpVscc1Value & B_PCH_SPI_SFDPX_VSCCX_EO_64K) != 0) {
            SpiDataCount = SIZE_64KB;
          } else {
            SpiDataCount = SIZE_4KB;
          }
        }
      } else {
        SpiDataCount = SIZE_4KB;
      }
      if (SpiDataCount == SIZE_4KB) {
        FlashCycle = (UINT32) (V_PCH_SPI_HSFSC_CYCLE_4K_ERASE << N_PCH_SPI_HSFSC_CYCLE);
      } else {
        FlashCycle = (UINT32) (V_PCH_SPI_HSFSC_CYCLE_64K_ERASE << N_PCH_SPI_HSFSC_CYCLE);
      }
    }
    //
    // If it's write cycle, load data into the SPI data buffer.
    //
    if ((FlashCycleType == FlashCycleWrite) || (FlashCycleType == FlashCycleWriteStatus)) {
      if ((SpiDataCount & 0x07) != 0) {
        //
        // Use Byte write if Data Count is 0, 1, 2, 3, 4, 5, 6, 7
        //
        for (Index = 0; Index < SpiDataCount; Index++) {
          MmioWrite8 (PchSpiBar0 + R_PCH_SPI_FDATA00 + Index, Buffer[Index]);
        }
      } else {
        //
        // Use Dword write if Data Count is 8, 16, 24, 32, 40, 48, 56, 64
        //
        for (Index = 0; Index < SpiDataCount; Index += sizeof (UINT32)) {
          MmioWrite32 (PchSpiBar0 + R_PCH_SPI_FDATA00 + Index, *(UINT32 *) (Buffer + Index));
        }
      }
    }

    //
    // Set the Flash Address
    //
    MmioWrite32 (
      (PchSpiBar0 + R_PCH_SPI_FADDR),
      (UINT32) (HardwareSpiAddr & B_PCH_SPI_FADDR_MASK)
      );

    //
    // Set Data count, Flash cycle, and Set Go bit to start a cycle
    //
    MmioAndThenOr32 (
      PchSpiBar0 + R_PCH_SPI_HSFSC,
      (UINT32) (~(B_PCH_SPI_HSFSC_FDBC_MASK | B_PCH_SPI_HSFSC_CYCLE_MASK)),
      (UINT32) ((((SpiDataCount - 1) << N_PCH_SPI_HSFSC_FDBC) & B_PCH_SPI_HSFSC_FDBC_MASK) | FlashCycle | B_PCH_SPI_HSFSC_CYCLE_FGO)
      );
    //
    // end of command execution
    //
    // Wait the SPI cycle to complete.
    //
    if (!WaitForSpiCycleComplete (This, PchSpiBar0, TRUE)) {
      ASSERT (FALSE);
      Status = EFI_DEVICE_ERROR;
      goto SendSpiCmdEnd;
    }
    //
    // If it's read cycle, load data into the call's buffer.
    //
    if ((FlashCycleType == FlashCycleRead) ||
        (FlashCycleType == FlashCycleReadSfdp) ||
        (FlashCycleType == FlashCycleReadJedecId) ||
        (FlashCycleType == FlashCycleReadStatus)) {
      if ((SpiDataCount & 0x07) != 0) {
        //
        // Use Byte read if Data Count is 0, 1, 2, 3, 4, 5, 6, 7
        //
        for (Index = 0; Index < SpiDataCount; Index++) {
          Buffer[Index] = MmioRead8 (PchSpiBar0 + R_PCH_SPI_FDATA00 + Index);
        }
      } else {
        //
        // Use Dword read if Data Count is 8, 16, 24, 32, 40, 48, 56, 64
        //
        for (Index = 0; Index < SpiDataCount; Index += sizeof (UINT32)) {
          *(UINT32 *) (Buffer + Index) = MmioRead32 (PchSpiBar0 + R_PCH_SPI_FDATA00 + Index);
        }
      }
    }

    HardwareSpiAddr += SpiDataCount;
    Buffer += SpiDataCount;
    ByteCount -= SpiDataCount;
  } while (ByteCount > 0);

SendSpiCmdEnd:
  //
  // Restore the settings for SPI Prefetching and Caching and enable BIOS Write Protect
  //
  if ((FlashCycleType == FlashCycleWrite) ||
      (FlashCycleType == FlashCycleErase)) {
    EnableBiosWriteProtect ();
  }
  //
  // Restore SMIs.
  //
  IoWrite32 ((UINTN) (ABase + R_PCH_SMI_EN), SmiEnSave);

  ReleaseSpiBar0 (SpiInstance);
  return Status;
}

/**
  Wait execution cycle to complete on the SPI interface.

  @param[in] This                 The SPI protocol instance
  @param[in] PchSpiBar0           Spi MMIO base address
  @param[in] ErrorCheck           TRUE if the SpiCycle needs to do the error check

  @retval TRUE                    SPI cycle completed on the interface.
  @retval FALSE                   Time out while waiting the SPI cycle to complete.
                                  It's not safe to program the next command on the SPI interface.
**/
BOOLEAN
WaitForSpiCycleComplete (
  IN     EFI_SPI_PROTOCOL   *This,
  IN     UINTN              PchSpiBar0,
  IN     BOOLEAN            ErrorCheck
  )
{
  UINT64        WaitTicks;
  UINT64        WaitCount;
  UINT32        Data32;
  SPI_INSTANCE  *SpiInstance;

  SpiInstance       = SPI_INSTANCE_FROM_SPIPROTOCOL (This);

  //
  // Convert the wait period allowed into to tick count
  //
  WaitCount = SPI_WAIT_TIME / SPI_WAIT_PERIOD;
  //
  // Wait for the SPI cycle to complete.
  //
  for (WaitTicks = 0; WaitTicks < WaitCount; WaitTicks++) {
    Data32 = MmioRead32 (PchSpiBar0 + R_PCH_SPI_HSFSC);
    if ((Data32 & B_PCH_SPI_HSFSC_SCIP) == 0) {
      MmioWrite32 (PchSpiBar0 + R_PCH_SPI_HSFSC, B_PCH_SPI_HSFSC_FCERR | B_PCH_SPI_HSFSC_FDONE);
      if (((Data32 & B_PCH_SPI_HSFSC_FCERR) != 0) && (ErrorCheck == TRUE)) {
        return FALSE;
      } else {
        return TRUE;
      }
    }
    PchPmTimerStallRuntimeSafe (SpiInstance->PchAcpiBase, SPI_WAIT_PERIOD);
  }
  return FALSE;
}
