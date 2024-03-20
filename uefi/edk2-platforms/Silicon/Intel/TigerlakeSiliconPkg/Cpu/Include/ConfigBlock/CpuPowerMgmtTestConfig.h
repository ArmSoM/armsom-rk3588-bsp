/** @file
  CPU Power Management Test Config Block.

  Copyright (c) 2021, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/
#ifndef _CPU_POWER_MGMT_TEST_CONFIG_H_
#define _CPU_POWER_MGMT_TEST_CONFIG_H_

#define CPU_POWER_MGMT_TEST_CONFIG_REVISION 4

extern EFI_GUID gCpuPowerMgmtTestConfigGuid;

#pragma pack (push,1)

///
/// PPM Package C State Limit
///
typedef enum {
  PkgC0C1                 = 0,
  PkgC2,
  PkgC3,
  PkgC6,
  PkgC7,
  PkgC7s,
  PkgC8,
  PkgC9,
  PkgC10,
  PkgCMax,
  PkgCpuDefault = 254,
  PkgAuto = 255
} MAX_PKG_C_STATE;

///
/// PPM Package C State Time Limit
///
typedef enum {
  TimeUnit1ns             = 0,
  TimeUnit32ns,
  TimeUnit1024ns,
  TimeUnit32768ns,
  TimeUnit1048576ns,
  TimeUnit33554432ns,
  TimeUnitMax
} C_STATE_TIME_UNIT;

///
/// Custom Power Units. User can choose to enter in watts or 125 milliwatt increments.
///
typedef enum {
  PowerUnitWatts = 0,     ///< in Watts.
  PowerUnit125MilliWatts, ///< in 125 milliwatt increments. Example: 90 power units times 125 mW equals 11.250 W.
  PowerUnitMax
} CUSTOM_POWER_UNIT;

///
/// PPM Interrupt Redirection Mode Selection
///
typedef enum {
  PpmIrmFixedPriority     = 0,
  PpmIrmRoundRobin,
  PpmIrmHashVector,
  PpmIrmReserved1,
  PpmIrmReserved2,
  PpmIrmReserved3,
  PpmIrmReserved4,
  PpmIrmNoChange
} PPM_IRM_SETTING;

/**
  CPU Power Management Test Configuration Structure.

  <b>Revision 1</b>:
  - Initial version.
  <b>Revision 2</b>:
  - Add  CstateLatencyControl0TimeUnit for WHL only
  - Add  CstateLatencyControl0Irtl for WHL only
  <b>Revision 3</b>:
  - Change C State LatencyContol to Auto as default.
  <b>Revision 4</b>:
  - Deprecate ConfigTdpLevel. Move to premem.
**/
typedef struct {
  CONFIG_BLOCK_HEADER   Header;                    ///< Offset 0-27  Config Block Header
  UINT32 Eist                          : 1;        ///< Offset 28-31 Enable or Disable Intel SpeedStep Technology. 0: Disable; <b>1: Enable</b>
  UINT32 EnergyEfficientPState         : 1;        ///<              Enable or Disable Energy Efficient P-state will be applied in Turbo mode. Disable; <b>1: Enable</b>
  UINT32 EnergyEfficientTurbo          : 1;        ///<              Enable or Disable Energy Efficient Turbo, will be applied in Turbo mode. Disable; <b>1: Enable</b>
  UINT32 TStates                       : 1;        ///<              Enable or Disable T states; <b>0: Disable</b>; 1: Enable.
  UINT32 BiProcHot                     : 1;        ///<              Enable or Disable Bi-Directional PROCHOT#; 0: Disable; <b>1: Enable</b>.
  UINT32 DisableProcHotOut             : 1;        ///<              Enable or Disable PROCHOT# signal being driven externally; 0: Disable; <b>1: Enable</b>.
  UINT32 ProcHotResponse               : 1;        ///<              Enable or Disable PROCHOT# Response; <b>0: Disable</b>; 1: Enable.
  UINT32 DisableVrThermalAlert         : 1;        ///<              Enable or Disable VR Thermal Alert; <b>0: Disable</b>; 1: Enable.
  UINT32 EnableAllThermalFunctions     : 1;        ///<              Enable or Disable Thermal Reporting through ACPI tables; 0: Disable; <b>1: Enable</b>.
  UINT32 ThermalMonitor                : 1;        ///<              Enable or Disable Thermal Monitor; 0: Disable; <b>1: Enable</b>.
  UINT32 Cx                            : 1;        ///<              Enable or Disable CPU power states (C-states). 0: Disable; <b>1: Enable</b>
  UINT32 PmgCstCfgCtrlLock             : 1;        ///<              If enabled, sets MSR 0xE2[15]; 0: Disable; <b>1: Enable</b>.
  UINT32 C1e                           : 1;        ///<              Enable or Disable Enhanced C-states. 0: Disable; <b>1: Enable</b>
  UINT32 C1AutoDemotion                : 1;        ///<              Enable or Disable C6/C7 auto demotion to C1. 0: Disabled; <b>1: C1 Auto demotion</b>
  UINT32 C1UnDemotion                  : 1;        ///<              Enable or Disable C1UnDemotion. 0: Disabled; <b>1: C1 Auto undemotion</b>
  UINT32 C3AutoDemotion                : 1;        ///<              [CoffeeLake Only] Enable or Disable C6/C7 auto demotion to C3  0: Disabled; <b>1: C3 Auto demotion</b>
  UINT32 C3UnDemotion                  : 1;        ///<              [CoffeeLake Only] Enable or Disable C3UnDemotion. 0: Disabled; <b>1: C3 Auto undemotion</b>
  UINT32 PkgCStateDemotion             : 1;        ///<              Enable or Disable Package Cstate Demotion. Disable; <b>1: Enable</b> [WhiskeyLake] <b>Disable</b>; 1: Enable
  UINT32 PkgCStateUnDemotion           : 1;        ///<              Enable or Disable Package Cstate UnDemotion. Disable; <b>1: Enable</b> [WhiskeyLake] <b>Disable</b>; 1: Enable
  UINT32 CStatePreWake                 : 1;        ///<              Enable or Disable CState-Pre wake. Disable; <b>1: Enable</b>
  UINT32 TimedMwait                    : 1;        ///<              Enable or Disable TimedMwait Support. <b>Disable</b>; 1: Enable
  UINT32 CstCfgCtrIoMwaitRedirection   : 1;        ///<              Enable or Disable IO to MWAIT redirection; <b>0: Disable</b>; 1: Enable.
  UINT32 ProcHotLock                   : 1;        ///<              If enabled, sets MSR 0x1FC[23]; <b>0: Disable</b>; 1: Enable.
  UINT32 RaceToHalt                    : 1;        ///<              Enable or Disable Race To Halt feature; 0: Disable; <b>1: Enable </b>. RTH will dynamically increase CPU frequency in order to enter pkg C-State faster to reduce overall power. (RTH is controlled through MSR 1FC bit 20)
  UINT32 ConfigTdpLevel                : 8;        ///<              @deprecated. Move to premem phase.
  UINT16 CstateLatencyControl1Irtl;                ///< Offset 32-33 Interrupt Response Time Limit of LatencyContol1 MSR 0x60B[9:0].<b>0 is Auto</b>.
  UINT16 CstateLatencyControl2Irtl;                ///< Offset 34-35 Interrupt Response Time Limit of LatencyContol2 MSR 0x60C[9:0].<b>0 is Auto</b>.
  UINT16 CstateLatencyControl3Irtl;                ///< Offset 36-37 Interrupt Response Time Limit of LatencyContol3 MSR 0x633[9:0].<b>0 is Auto</b>.
  UINT16 CstateLatencyControl4Irtl;                ///< Offset 38-39 Interrupt Response Time Limit of LatencyContol4 MSR 0x634[9:0].<b>0 is Auto</b>.
  UINT16 CstateLatencyControl5Irtl;                ///< Offset 40-41 Interrupt Response Time Limit of LatencyContol5 MSR 0x635[9:0].<b>0 is Auto</b>.
  // Due to the removal of CstateLatencyControl0Irtl, PkgCStateLimit is not aligned to 32-bit address.
  UINT8  Rsvd1[2];                                 ///< Offset 42-43 Reserved for config block alignment.
  MAX_PKG_C_STATE   PkgCStateLimit;                ///< Offset 44    This field is used to set the Max Pkg Cstate. Default set to Auto which limits the Max Pkg Cstate to deep C-state.
  /**
     @todo: The following enums have to be replaced with policies.
  **/
  C_STATE_TIME_UNIT Reserved;                      ///< Offset 45    Reserved for config block alignment.
  C_STATE_TIME_UNIT CstateLatencyControl1TimeUnit; ///< Offset 46    TimeUnit for Latency Control1 MSR 0x60B[12:10]; <b>2: 1024ns</b>.
  C_STATE_TIME_UNIT CstateLatencyControl2TimeUnit; ///< Offset 47    TimeUnit for Latency Control2 MSR 0x60C[12:10]; <b>2: 1024ns</b>.
  C_STATE_TIME_UNIT CstateLatencyControl3TimeUnit; ///< Offset 48    TimeUnit for Latency Control3 MSR 0x633[12:10]; <b>2: 1024ns</b>.
  C_STATE_TIME_UNIT CstateLatencyControl4TimeUnit; ///< Offset 49    TimeUnit for Latency Control4 MSR 0x634[12:10]; <b>2: 1024ns</b>.
  C_STATE_TIME_UNIT CstateLatencyControl5TimeUnit; ///< Offset 50    TimeUnit for Latency Control5 MSR 0x635[12:10]; <b>2: 1024ns</b>.
  /**
  Offset 51  Default power unit in watts or in 125 milliwatt increments.
  - 0: PowerUnitWatts.
  - <b>1: PowerUnit125MilliWatts</b>.
  **/
  CUSTOM_POWER_UNIT CustomPowerUnit;
  /**
  Offset 52  Interrupt Redirection Mode Select.
   - 0: Fixed priority.             //Default under CNL.
   - 1: Round robin.
   - 2: Hash vector.
   - 4: PAIR with fixed priority.   //Default under KBL, not available under CNL.
   - 5: PAIR with round robin.      //Not available under CNL.
   - 6: PAIR with hash vector.      //Not available under CNL.
   - 7: No change.
  **/
  PPM_IRM_SETTING      PpmIrmSetting;
  // Move the padding to previous offset to align the structure at 32-bit address.
  UINT8  Rsvd[4];                                 ///< Offset 53-56 Reserved for future use and config block alignment
} CPU_POWER_MGMT_TEST_CONFIG;

#pragma pack (pop)

#endif // _CPU_POWER_MGMT_TEST_CONFIG_H_
