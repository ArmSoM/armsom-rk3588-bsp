/**@file

Copyright (c) 2006 - 2009, Intel Corporation. All rights reserved.<BR>
Copyright (c) 2015, Hisilicon Limited. All rights reserved.<BR>
Copyright (c) 2015, Linaro Limited. All rights reserved.<BR>
SPDX-License-Identifier: BSD-2-Clause-Patent

Module Name:

  MiscBootInformationData.c

Abstract:

  This driver parses the mMiscSubclassDataTable structure and reports
  any generated data to the DataHub.

Based on files under Nt32Pkg/MiscSubClassPlatformDxe/
**/

#include "SmbiosMisc.h"

//
// Static (possibly build generated) Bios Vendor data.
//
MISC_SMBIOS_TABLE_DATA(SMBIOS_TABLE_TYPE32, MiscBootInformation) = {
  {                                                     // Hdr
    EFI_SMBIOS_TYPE_SYSTEM_BOOT_INFORMATION,              // Type,
    0,                                                    // Length,
    0                                                     // Handle
  },
  {                                                     // Reserved[6]
    0,
    0,
    0,
    0,
    0,
    0
  },
  BootInformationStatusNoError                          // BootInformationStatus
};

/* eof - MiscBootInformationData.c */
