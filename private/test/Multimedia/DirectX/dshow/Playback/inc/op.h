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

// declare a private interface shared by url filter, ocx and filtergraph


#ifndef __OP_H__
#define __OP_H__


// {8E1C39A1-DE53-11cf-AA63-0080C744528D}
DEFINE_GUID(IID_IAMOpenProgress,0x8e1c39a1,0xde53,0x11cf,0xaa,0x63,0x0,0x80,0xc7,0x44,0x52,0x8d);


// this interface provides information about current progress through
// the download
#if defined(UNDER_CE)
DECLARE_INTERFACE_(IAMOpenProgress, IUnknown) {
        STDMETHOD(QueryProgress) (THIS_
                                  LONGLONG* pllTotal,   // [out]
                                  LONGLONG* pllCurrent  // [out]
                                 ) PURE;

        //
        // Abort
        //
        // An improvement over the current "periscoping" implementation
        // of abort by callback into the filter graph.
        // The abort implementation is OK to cause an operation such as RenderFile
        // to stop, but we also want to be able to stop when downloading after
        // RenderFile (or whatever) has finished.  The same mechanism of hoding
        // up an abort flag and waiting until it's seen would work, but the
        // problem is that nobody can tell how long to keep holding it up for
        // until everyone has seen it.  This method instructs the exporter of
        // IAMOpenProgress to hold up their own internal abort flag until
        // further notice.
        STDMETHOD(AbortOperation(THIS_)) PURE;
};
#endif

#endif // __OP_H__



