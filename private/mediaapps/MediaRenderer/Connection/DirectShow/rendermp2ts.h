//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this sample source code is subject to the terms of the Microsoft
// license agreement under which you licensed this sample source code. If
// you did not accept the terms of the license agreement, you are not
// authorized to use this sample source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the LICENSE.RTF on your install media or the root of your tools installation.
// THE SAMPLE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//

#pragma once

#include <atlcomcli.h>
#include <streams.h>

namespace RenderMpeg2TransportStream
{

class Graph
{
public:
    Graph( );
    HRESULT RenderFile( IGraphBuilder* pGraph, const wchar_t* wszFile );

private:
    // disable copy ctor and assignment
    Graph( const Graph& );
    Graph& operator=( const Graph& );

    HRESULT GetMpeg2DemuxFilter_( CComPtr< IBaseFilter >& rspMpeg2Demux );
    HRESULT FindMpeg2TransportStreamDemuxOnGraph_( CComPtr< IBaseFilter >& rspFound );

    CComPtr< IGraphBuilder > m_spGraph;
};

}

