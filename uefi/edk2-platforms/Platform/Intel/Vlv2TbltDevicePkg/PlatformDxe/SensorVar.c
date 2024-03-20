/** @file

  Copyright (c) 2004  - 2014, Intel Corporation. All rights reserved.<BR>
                                                                                   
  SPDX-License-Identifier: BSD-2-Clause-Patent

                                                                                   

Module Name:


  SensorVar.c

Abstract:

  Initialization for the Sensor Info variable.

Revision History

--*/

#include "PlatformDxe.h"
#include "Guid/SensorInfoVariable.h"

//
// Sensor Information (board specific)
//

#define TEMPERATURE_SENSORS_COUNT       4
#define VOLTAGE_SENSORS_COUNT           6
#define FAN_SENSORS_COUNT               4
#define FAN_CONTROLLERS_COUNT           3

TYPEDEF_TEMP_SENSOR_SECTION(TEMPERATURE_SENSORS_COUNT);
TYPEDEF_VOLT_SENSOR_SECTION(VOLTAGE_SENSORS_COUNT);
TYPEDEF_FAN_SENSOR_SECTION(FAN_SENSORS_COUNT);
TYPEDEF_FAN_CONTROLLER_SECTION(FAN_CONTROLLERS_COUNT);
TYPEDEF_SENSOR_INFO_VAR;

SENSOR_INFO_VAR               mSensorInfoData =
{
	  //
    // Temperature Sensors
    //
    TEMPERATURE_SENSORS_COUNT,
    {
        { 0, 3, CPU_CORE_TEMPERATURE,            TRUE  },
        { 0, 1, MOTHERBOARD_AMBIENT_TEMPERATURE, FALSE },
        { 0, 2, VR_TEMPERATURE,                  FALSE },
        { 0, 0, IOH_TEMPERATURE,                 FALSE }
    },

    //
    // Voltage Sensors
    //
    VOLTAGE_SENSORS_COUNT,
    {
        { 0, 0, PLUS_12_VOLTS       },
        { 0, 1, PLUS_5_VOLTS        },
        { 0, 2, PLUS_3P3_VOLTS      },
        { 0, 3, MCH_VCC_VOLTAGE     },
        { 0, 4, CPU_1_VCCP_VOLTAGE  },
        { 0, 5, CPU_VTT_VOLTAGE     }
    },

    //
    // Fan Speed Sensors
    //
    FAN_SENSORS_COUNT,
    {
        { 0, 0, CPU_COOLING_FAN,    FAN_4WIRE,         0 },
        { 0, 1, AUX_COOLING_FAN,    FAN_4WIRE,         1 },
        { 0, 2, CHASSIS_INLET_FAN,  FAN_3WIRE_VOLTAGE, 1 },
        { 0, 3, CHASSIS_OUTLET_FAN, FAN_3WIRE_VOLTAGE, 2 }
    },

    //
    // Fan Speed Controllers
    //
    FAN_CONTROLLERS_COUNT,
    {
        { 0, 0, CPU_COOLING_FAN,     { 0, 0xff, 0xff, 0xff } },
        { 0, 1, CHASSIS_COOLING_FAN, { 1,    2, 0xff, 0xff } },
        { 0, 2, CHASSIS_COOLING_FAN, { 3, 0xff, 0xff, 0xff } }
    }
};

#if defined(SENSOR_INFO_VAR_SUPPORT) && SENSOR_INFO_VAR_SUPPORT != 0
/**

  Write the Sensor Info variable if it does not already exist.

**/
VOID
InitializeSensorInfoVariable (
  )
{
  //
  // Set the Sensor Info variable.  If it already exists and the data matches,
  // the variable driver will simply return without writing; otherwise, the
  // driver will write the variable.
  //
  gRT->SetVariable (
         gEfiSensorInfoVarNameWithPassword,
         &gEfiSensorInfoVarGuid,
         EFI_VARIABLE_NON_VOLATILE |
         EFI_VARIABLE_BOOTSERVICE_ACCESS |
         EFI_VARIABLE_RUNTIME_ACCESS,
         sizeof (SENSOR_INFO_VAR),
         &mSensorInfoData
         );
}
#endif

