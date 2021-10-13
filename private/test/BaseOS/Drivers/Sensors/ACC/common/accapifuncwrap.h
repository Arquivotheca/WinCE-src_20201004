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
#ifndef _ACC_API_FUNC_WRAP_
#define _ACC_API_FUNC_WRAP_

#include "Accapi.h"



HSENSOR f_OpenSensor(__in_z WCHAR* szLegacyName, __deref_opt_out PLUID pSensorLuid);

DWORD f_AccelerometerStart( HSENSOR hSensor, HANDLE hMsgQueue );
DWORD f_AccelerometerStop( HSENSOR hSensor );
DWORD f_AccelerometerCreateCallback( HSENSOR hSensor, ACCELEROMETER_CALLBACK* pfnCallback, LPVOID plvCallbackParam );
DWORD f_AccelerometerCancelCallback( HSENSOR hSensor );
DWORD f_AccelerometerSetMode( HSENSOR hSensor, ACC_CONFIG_MODE configMode, LPVOID reserved );


#endif //_ACC_API_FUNC_WRAP_

