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
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES.
//
#ifndef __MESSAGES_H
#define __MESSAGES_H

//------------------------------------------------------------------------------

#define NDT_XXX(a, b, c)      ((a << 24)|(b << 16)|c)

//------------------------------------------------------------------------------

#define NDT_ERR               0x02
#define NDT_WRN               0x03
#define NDT_INF               0x0A
#define NDT_DBG               0x0C
#define NDT_CMT               0x0E
#define NDT_VRB               0x0F

//------------------------------------------------------------------------------

#define NDT_INF_TEST_TIME                          NDT_XXX(NDT_INF, 0, 0)
#define NDT_INF_TEST_RESULT                        NDT_XXX(NDT_INF, 0, 1)

//------------------------------------------------------------------------------

#define NDT_ERR_DRIVER_REGISTER                    NDT_XXX(NDT_ERR, 1, 0)
#define NDT_ERR_DRIVER_OPEN                        NDT_XXX(NDT_ERR, 1, 1)
#define NDT_ERR_DRIVER_CLOSE                       NDT_XXX(NDT_ERR, 1, 2)
#define NDT_ERR_DRIVER_DEREGISTER                  NDT_XXX(NDT_ERR, 1, 3)
#define NDT_ERR_DEVICE_OVERLAPPED_ALLOC            NDT_XXX(NDT_ERR, 1, 4)
#define NDT_ERR_DEVICE_CREATEFILEMAPPING           NDT_XXX(NDT_ERR, 1, 5)
#define NDT_ERR_DEVICE_MAPFILEVIEW                 NDT_XXX(NDT_ERR, 1, 6)
#define NDT_ERR_DEVICE_CREATEEVENT                 NDT_XXX(NDT_ERR, 1, 7)
#define NDT_ERR_DEVICE_IOCONTROL                   NDT_XXX(NDT_ERR, 1, 8)
#define NDT_ERR_DEVICE_REQUEST                     NDT_XXX(NDT_ERR, 1, 9)

//------------------------------------------------------------------------------

extern LPCTSTR* g_NDTLogMessageTables[];

//------------------------------------------------------------------------------

void NDTLog(DWORD dwId, ...);

//------------------------------------------------------------------------------

#endif
