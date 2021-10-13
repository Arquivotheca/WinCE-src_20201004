//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

#include <algorithm>
#include "av_upnp.h"

using namespace av_upnp;
using namespace av_upnp::details;

/////////////////////////////////////////////////////////////////////////////
// wstring_buffer

wstring_buffer::wstring_buffer(unsigned int nSizeCheckPeriod)
    : m_nMaxSize(0),
      m_nUsesSinceSizeCheck(0),
      m_nSizeCheckPeriod(nSizeCheckPeriod)
{
}


void wstring_buffer::ResetBuffer()
{
    ++m_nUsesSinceSizeCheck;
    const size_t strBuffer_size = strBuffer.size();

    if( m_nUsesSinceSizeCheck >= m_nSizeCheckPeriod )
    {
        /* NOTE:
         * av::wstring does not at time of this code's writing allow shrinking.
         * If this ability is added, here strBuffer should be shrunk to m_nMaxSize.
         */
        /* strBuffer.shrink_allocation(m_nMaxSize); */

        m_nMaxSize            = strBuffer_size;
        m_nUsesSinceSizeCheck = 0;
    }
    else
    {
        // Update m_nMaxSize to take into account the current buffer
        m_nMaxSize = max(m_nMaxSize, strBuffer_size);
    }

    // Ready the buffer for usage again
    // NOTE: assumes resize does not affect the amount of memory strBuffer has allocated internally
    strBuffer.resize(0);
}
