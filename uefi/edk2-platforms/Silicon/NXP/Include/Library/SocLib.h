/** @file

  Copyright 2020 NXP
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef SOC_LIB_H__
#define SOC_LIB_H__

#include <Uefi.h>
#include <Ppi/NxpPlatformGetClock.h>

/**
  Return the input clock frequency to an IP Module.
  This function reads the RCW bits and calculates the  PLL multiplier/divider
  values to be applied to various IP modules.
  If a module is disabled or doesn't exist on platform, then return zero.

  @param[in]  BaseClock  Base clock to which PLL multiplier/divider values is
                         to be applied.
  @param[in]  ClockType  Variable of Type NXP_IP_CLOCK. Indicates which IP clock
                         is to be retrieved.
  @param[in]  Args       Variable argument list which is parsed based on
                         ClockType. e.g. if the ClockType is NXP_I2C_CLOCK, then
                         the second argument will be interpreted as controller
                         number. e.g. if there are four i2c controllers in SOC,
                         then this value can be 0, 1, 2, 3
                         e.g. if ClockType is NXP_CORE_CLOCK, then second
                         argument is interpreted as cluster number and third
                         argument is interpreted as core number (within the
                         cluster)

  @return                Actual Clock Frequency. Return value 0 should be
                         interpreted as clock not being provided to IP.
**/
UINT64
SocGetClock (
  IN  UINT64        BaseClock,
  IN  NXP_IP_CLOCK  ClockType,
  IN  VA_LIST       Args
  );

/**
  Function to initialize SoC specific constructs
 **/
VOID
SocInit (
  VOID
  );

/**
  Function to get System Version Register(SVR) of SoC
**/
UINT32
SocGetSvr (
  VOID
  );
#endif // SOC_LIB_H__
