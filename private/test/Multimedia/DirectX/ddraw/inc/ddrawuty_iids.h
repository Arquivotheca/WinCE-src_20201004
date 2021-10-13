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
#pragma once

#ifndef __DDRAWUTY_IIDS_H__
#define __DDRAWUTY_IIDS_H__

#ifndef ONLY_DECLARE_GUIDS

// External Dependencies
#include <ddraw.h>
#include <dvp.h>
#include <uuids.h>
#include <com_utilities.h>

// these specializations are required since no GUIDs are 
// associated with these interfaces (see com_utilities)
DECLARE_STD_UUIDOF_SPECIALIZATION(IDirectDraw);
DECLARE_STD_UUIDOF_SPECIALIZATION(IDirectDrawSurface);
DECLARE_STD_UUIDOF_SPECIALIZATION(IDirectDrawPalette);
DECLARE_STD_UUIDOF_SPECIALIZATION(IDirectDrawClipper);
DECLARE_STD_UUIDOF_SPECIALIZATION(IDirectDrawVideoPort);
DECLARE_STD_UUIDOF_SPECIALIZATION(IDDVideoPortContainer);

// Add overloads of function InterfaceName (see com_utilities)
STD_INTERFACE_NAME(IDirectDraw);
STD_INTERFACE_NAME(IDirectDrawSurface);
STD_INTERFACE_NAME(IDirectDrawPalette);
STD_INTERFACE_NAME(IDirectDrawClipper);
STD_INTERFACE_NAME(IDirectDrawVideoPort);
STD_INTERFACE_NAME(IDDVideoPortContainer);

#endif // #ifndef ONLY_DECLARE_GUIDS

// Stolen from D3D.h, since these are good test cases
DEFINE_GUID( IID_IDirect3D,             0x3BBA0080, 0x2421, 0x11CF, 0xA3, 0x1A, 0x00, 0xAA, 0x00, 0xB9, 0x33, 0x56);
DEFINE_GUID( IID_IDirect3D2,            0x6aae1ec1, 0x662a, 0x11d0, 0x88, 0x9d, 0x00, 0xaa, 0x00, 0xbb, 0xb7, 0x6a);
DEFINE_GUID( IID_IDirect3D3,            0xbb223240, 0xe72b, 0x11d0, 0xa9, 0xb4, 0x00, 0xaa, 0x00, 0xc0, 0x99, 0x3e);
DEFINE_GUID( IID_IDirect3DDevice,       0x64108800, 0x957d, 0X11d0, 0x89, 0xab, 0x00, 0xa0, 0xc9, 0x05, 0x41, 0x29);
DEFINE_GUID( IID_IDirect3DDevice2,      0x93281501, 0x8cf8, 0x11d0, 0x89, 0xab, 0x00, 0xa0, 0xc9, 0x05, 0x41, 0x29);
DEFINE_GUID( IID_IDirect3DDevice3,      0xb0ab3b60, 0x33d7, 0x11d1, 0xa9, 0x81, 0x00, 0xc0, 0x4f, 0xd7, 0xb1, 0x74);
DEFINE_GUID( IID_IDirect3DTexture,      0x2CDCD9E0, 0x25A0, 0x11CF, 0xA3, 0x1A, 0x00, 0xAA, 0x00, 0xB9, 0x33, 0x56);
DEFINE_GUID( IID_IDirect3DTexture2,     0x93281502, 0x8cf8, 0x11d0, 0x89, 0xab, 0x00, 0xa0, 0xc9, 0x05, 0x41, 0x29);

#endif // #ifndef __DDRAWUTY_IIDS_H__



