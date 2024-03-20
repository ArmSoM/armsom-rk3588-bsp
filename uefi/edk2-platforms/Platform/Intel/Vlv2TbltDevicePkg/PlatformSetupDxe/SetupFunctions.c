/** @file

  Copyright (c) 2004  - 2019, Intel Corporation. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

Module Name:

    SetupFunctions.c

Abstract:

Revision History

--*/

#include "PlatformSetupDxe.h"

VOID
AsciiToUnicode (
  IN    CHAR8     *AsciiString,
  IN    CHAR16    *UnicodeString
  )
{
  UINT8           Index;

  Index = 0;
  while (AsciiString[Index] != 0) {
    UnicodeString[Index] = (CHAR16)AsciiString[Index];
    Index++;
  }
}

VOID
SwapEntries (
  IN  CHAR8 *Data
  )
{
  UINT16  Index;
  CHAR8   Temp8;

  Index = 0;
  while (Data[Index] != 0 && Data[Index+1] != 0) {
    Temp8 = Data[Index];
    Data[Index] = Data[Index+1];
    Data[Index+1] = Temp8;
    Index +=2;
  }

  return;
}
