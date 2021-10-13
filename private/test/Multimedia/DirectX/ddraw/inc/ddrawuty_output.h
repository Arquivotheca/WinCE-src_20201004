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
#ifndef __DDRAWUTY_OUTPUT_H__
#define __DDRAWUTY_OUTPUT_H__

// External Dependencies
#include <QATestUty/LocaleUty.h>
#include <QATestUty/DebugUty.h>
#include <DebugStreamUty.h>

// Overloads of << operator for CDebugStrem for
// Direct Draw Structures
extern DebugStreamUty::CDebugStream& operator << (DebugStreamUty::CDebugStream&, const DDCAPS&);
extern DebugStreamUty::CDebugStream& operator << (DebugStreamUty::CDebugStream&, const DDCOLORCONTROL&);
extern DebugStreamUty::CDebugStream& operator << (DebugStreamUty::CDebugStream&, const DDCOLORKEY&);
extern DebugStreamUty::CDebugStream& operator << (DebugStreamUty::CDebugStream&, const DDPIXELFORMAT&);
extern DebugStreamUty::CDebugStream& operator << (DebugStreamUty::CDebugStream&, const DDSCAPS&);
extern DebugStreamUty::CDebugStream& operator << (DebugStreamUty::CDebugStream&, const DDSURFACEDESC&);
extern DebugStreamUty::CDebugStream& operator << (DebugStreamUty::CDebugStream&, const DDVIDEOPORTBANDWIDTH&);
extern DebugStreamUty::CDebugStream& operator << (DebugStreamUty::CDebugStream&, const DDVIDEOPORTCAPS&);
extern DebugStreamUty::CDebugStream& operator << (DebugStreamUty::CDebugStream&, const DDVIDEOPORTCONNECT&);
extern DebugStreamUty::CDebugStream& operator << (DebugStreamUty::CDebugStream&, const DDVIDEOPORTDESC&);
extern DebugStreamUty::CDebugStream& operator << (DebugStreamUty::CDebugStream&, const LPDDPIXELFORMAT&);
extern DebugStreamUty::CDebugStream& operator << (DebugStreamUty::CDebugStream&, const DDVIDEOPORTINFO&);
extern DebugStreamUty::CDebugStream& operator << (DebugStreamUty::CDebugStream&, const DDVIDEOPORTSTATUS&);
extern DebugStreamUty::CDebugStream& operator << (DebugStreamUty::CDebugStream&, const DEVMODE&);

#endif

