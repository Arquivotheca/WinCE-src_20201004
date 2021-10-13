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
#ifndef _CONTROLS_PRIV_H_
#define _CONTROLS_PRIV_H_

// List box

// Enabling gesture handling for list boxes with LBS_OWNERDRAWVARIABLE style.
// wParam: TRUE - enable, FALSE - disable
// lParam: not used
// returns TRUE if success, FALSE if error occured
#define LB_OWNERDRAWVARGESTURES     (WM_USER+2)

#endif // _CONTROLS_PRIV_H_
