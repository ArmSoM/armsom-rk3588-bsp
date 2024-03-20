/*++

  Copyright (c) 2004  - 2019, Intel Corporation. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __COMMON_HEADER_H_
#define __COMMON_HEADER_H_



#include <PiPei.h>

#include <IndustryStandard/SmBus.h>
#include <IndustryStandard/Pci22.h>
#include <Ppi/AtaController.h>
#include <Ppi/MasterBootMode.h>
#include <Guid/MemoryTypeInformation.h>
#include <Guid/RecoveryDevice.h>
#include <Ppi/ReadOnlyVariable2.h>
#include <Ppi/DeviceRecoveryModule.h>
#include <Ppi/Capsule.h>
#include <Ppi/Reset.h>
#include <Ppi/Stall.h>
#include <Ppi/BootInRecoveryMode.h>
#include <Guid/FirmwareFileSystem2.h>
#include <Ppi/MemoryDiscovered.h>
#include <Ppi/RecoveryModule.h>
#include <Ppi/Smbus2.h>
#include <Ppi/FirmwareVolumeInfo.h>
#include <Ppi/EndOfPeiPhase.h>
#include <Library/DebugLib.h>
#include <Library/PeimEntryPoint.h>
#include <Library/BaseLib.h>
#include <Library/PeiServicesLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/HobLib.h>
#include <Library/PciCf8Lib.h>
#include <Library/IoLib.h>
#include <Library/PciLib.h>
#include <Library/ReportStatusCodeLib.h>
#include <Library/PcdLib.h>
#include <Library/TimerLib.h>
#include <Library/PrintLib.h>
#include <Library/ResetSystemLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PerformanceLib.h>
#include <Library/CacheMaintenanceLib.h>
#include <Library/MtrrLib.h>


#endif
