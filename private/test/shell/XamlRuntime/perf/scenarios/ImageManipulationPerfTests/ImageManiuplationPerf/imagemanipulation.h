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

#ifndef __XRIMAGEPERF_H__
#define __XRIMAGEPERF_H__

#include <XRCommon.h>
#include <xamlruntime.h>
#include <XRPtr.h>
#include <perftestutils.h>
#include <XRCustomControl.h>

#define CEPERF_ENABLE
#include <ceperf.h>
#undef TRACE //windowsqa.h defines TRACE

#define IM_PERF_LOG_NAME                L"XRImageManipulationPerfResults.xml"

//Test cases:
#define TC_GRAPH_PAN                    1
#define TC_GRAPH_FLICK                  2
#define TC_GRAPH_ZOOM                   3
#define TC_GRAPH_H_SCROLL               4
#define TC_GRAPH_V_SCROLL               5
#define TC_PHOTO_PAN                    11
#define TC_PHOTO_FLICK                  12
#define TC_PHOTO_ZOOM                   13
#define TC_PHOTO_H_SCROLL               14
#define TC_PHOTO_V_SCROLL               15

#define PAN_SAMPLES                     100
#define MY_MAX_PATH                     MAX_PATH/2
#define MY_OFFSET                       50
#define MS_DELAY                        20

enum IM_DIRECTIONS {
    IM_DOWN,
    IM_UP,
    IM_RIGHT,
    IM_LEFT
};

class __declspec(uuid("{8425a778-0c63-4700-a22b-757e7b2477e6}")) ImageUC:public XRCustomUserControlImpl<ImageUC>
{
   QI_IDENTITY_MAPPING(ImageUC, XRCustomUserControlImpl)

public:
    static HRESULT GetXamlSource(XRXamlSource* pXamlSource);
    static HRESULT Register();
};

class ImageManipulationEventHandler
{
public:

    int m_Result;
    bool m_bStateNormal;
    bool m_bNextStatePinch;
    ImageUC* m_pImageUC;

    ImageManipulationEventHandler(bool bStateNormal, bool bNextStatePinch, ImageUC* pImageUC)
    {
        m_Result = TPR_FAIL;
        m_bStateNormal = bStateNormal;
        m_bNextStatePinch = bNextStatePinch;
        m_pImageUC = pImageUC;
    }

    ~ImageManipulationEventHandler()
    {}

    HRESULT OnEventVisualStateChanged(IXRDependencyObject* pSender, XRValueChangedEventArgs<IXRVisualState*>* pArgs);
};


#endif // __IMAGEPERF_H__