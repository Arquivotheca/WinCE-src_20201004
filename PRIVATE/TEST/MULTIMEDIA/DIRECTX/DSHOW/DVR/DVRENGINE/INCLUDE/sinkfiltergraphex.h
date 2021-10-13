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
#pragma once
#include "Streams.h"
#include "Wxdebug.h"
#include "Dshow.h"
#include "filtergraph.h"
#include "Sinkfiltergraph.h"
#include "dvrinterfaces.h"
#include "Grabber.h"

class CSinkFilterGraphEx :
    public CSinkFilterGraph
{
public:
    CSinkFilterGraphEx();
    ~CSinkFilterGraphEx(void);

    static HRESULT SequentialStamper(IMediaSample *pSample, REFERENCE_TIME *StartTime, REFERENCE_TIME *StopTime, BOOL TypeChanged, AM_MEDIA_TYPE *mtype, LPVOID lpUser);
    static HRESULT SimpleCountDisp(IMediaSample *pSample, REFERENCE_TIME *StartTime, REFERENCE_TIME *StopTime, BOOL TypeChanged, AM_MEDIA_TYPE *mtype, LPVOID lpUser);
    //static HRESULT SequentialTempStampChecker(IMediaSample *pSample, REFERENCE_TIME *StartTime, REFERENCE_TIME *StopTime, BOOL TypeChanged, AM_MEDIA_TYPE *mtype, LPVOID lpUser);
    //static HRESULT SequentialPermStampChecker(IMediaSample *pSample, REFERENCE_TIME *StartTime, REFERENCE_TIME *StopTime, BOOL TypeChanged, AM_MEDIA_TYPE *mtype, LPVOID lpUser);

    HRESULT SetGrabberCallback(SAMPLECALLBACK callback);

    static bool Ex_flag;
    static UINT32 Ex_iCount;
};

