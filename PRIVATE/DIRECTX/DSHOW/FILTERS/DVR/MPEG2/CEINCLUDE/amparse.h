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
//------------------------------------------------------------------------------
// File: AMParse.h
//
// Desc: Interface to the parser to get current time.  This is useful for
//       multifile playback.
//
//------------------------------------------------------------------------------


#ifndef __AMPARSE__
#define __AMPARSE__

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus


DEFINE_GUID(IID_IAMParse,
0xc47a3420, 0x005c, 0x11d2, 0x90, 0x38, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x98);

//
//  Parser interface - supported by MPEG-2 splitter filter
//
DECLARE_INTERFACE_(IAMParse, IUnknown) {
    STDMETHOD(GetParseTime) (THIS_
                             REFERENCE_TIME *prtCurrent
                            ) PURE;
    STDMETHOD(SetParseTime) (THIS_
                             REFERENCE_TIME rtCurrent
                            ) PURE;
    STDMETHOD(Flush) (THIS) PURE;
};

#ifdef __cplusplus
}
#endif // __cplusplus
#endif // __AMPARSE__
