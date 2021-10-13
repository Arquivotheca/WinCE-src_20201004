//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft shared
// source or premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license agreement,
// you are not authorized to use this source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the SOURCE.RTF on your install media or the root of your tools installation.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
#include <sensor.h>
#include <accapi.h>

#include "AccApiFuncWrap.h"
#include "AccHelper.h"

//------------------------------------------------------------------------------
//f_OpenSensor
//
HSENSOR f_OpenSensor(__in_z WCHAR* szLegacyName, __deref_opt_out PLUID pSensorLuid)
{
    LOG_START();
    HSENSOR hAcc = (HSENSOR)AccelerometerOpen(szLegacyName, pSensorLuid);
    LOG_END();
    return hAcc;
}//f_OpenSensor

//------------------------------------------------------------------------------
//f_AccelerometerStart
//
DWORD f_AccelerometerStart( HSENSOR hSensor, HANDLE hMsgQueue )
{
    LOG_START();
    DWORD dwResult = AccelerometerStart( hSensor, hMsgQueue );
    LOG_END();
    return dwResult;
}//f_AccelerometerStart


//------------------------------------------------------------------------------
//f_AccelerometerStop
//
DWORD f_AccelerometerStop( HSENSOR hSensor )
{
    LOG_START();
    DWORD dwResult = AccelerometerStop( hSensor );
    LOG_END();
    return dwResult;
}//f_AccelerometerStop




//------------------------------------------------------------------------------
//f_AccelerometerCreateCallback
//
DWORD f_AccelerometerCreateCallback( HSENSOR hSensor, ACCELEROMETER_CALLBACK* pfnCallback, LPVOID plvCallbackParam )
{
    LOG_START();
    DWORD dwResult = AccelerometerCreateCallback( hSensor, pfnCallback, plvCallbackParam );
    LOG_END();
    return dwResult;
}//f_AccelerometerCreateCallback




//------------------------------------------------------------------------------
//f_AccelerometerCancelCallback
//
DWORD f_AccelerometerCancelCallback( HSENSOR hSensor )
{
    LOG_START();
    DWORD dwResult = AccelerometerCancelCallback( hSensor );
    LOG_END();
    return dwResult;
}//f_AccelerometerCancelCallback



//------------------------------------------------------------------------------
//f_AccelerometerSetMode
//
DWORD f_AccelerometerSetMode( HSENSOR hSensor, ACC_CONFIG_MODE configMode, LPVOID reserved )
{
    LOG_START();
    DWORD dwResult = AccelerometerSetMode( hSensor, configMode, reserved );
    LOG_END();
    return dwResult;
}//f_AccelerometerSetMode

