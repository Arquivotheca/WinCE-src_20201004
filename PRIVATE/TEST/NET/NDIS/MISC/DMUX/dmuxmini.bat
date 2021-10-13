@REM
@REM Copyright (c) Microsoft Corporation.  All rights reserved.
@REM
@REM
@REM Use of this source code is subject to the terms of the Microsoft shared
@REM source or premium shared source license agreement under which you licensed
@REM this source code. If you did not accept the terms of the license agreement,
@REM you are not authorized to use this source code. For the terms of the license,
@REM please see the license agreement between you and Microsoft or, if applicable,
@REM see the SOURCE.RTF on your install media or the root of your tools installation.
@REM THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
@REM
regset -kHLM -xNEW_KEY -pComm -nDMUXMini
regset -kHLM -pComm\DMUXMini -nGroup -tREG_SZ -v"NDIS"
regset -kHLM -pComm\DMUXMini -nImagePath -tREG_SZ -v"dmuxmini.dll"
regset -kHLM -pComm\DMUXMini -nNoDeviceCreate -tREG_DWORD -v1
regset -kHLM -pComm\DMUXMini -nInstances -tREG_DWORD -v16
regset -kHLM -pComm\DMUXMini -nMacSeed -tREG_DWORD -v4

regset -kHLM -pComm\DMUXMini -nbindto -tREG_SZ -v"PcCard\Ne20001"
