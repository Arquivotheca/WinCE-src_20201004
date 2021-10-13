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
////////////////////////////////////////////////////////////////////////////////

#ifndef GRAPH_TESTDESC_PARSER_H
#define GRAPH_TESTDESC_PARSER_H

#include "DxXML.h"
#include "GraphTestDesc.h"

extern HRESULT ParsePlaybackTestDesc( CXMLElement *hElement, TestDesc** ppTestDesc );
extern HRESULT ParseStateChangeTestDesc( CXMLElement *hElement, TestDesc** ppTestDesc );
extern HRESULT ParseBuildGraphTestDesc( CXMLElement *hElement, TestDesc** ppTestDesc );
extern HRESULT ParseInterfaceTestDesc( CXMLElement *hElement, TestDesc** ppTestDesc );
extern HRESULT ParseMarkerInfo( CXMLElement *hElement, PlaybackInterfaceTestDesc* pPlaybackInterfaceTestDesc );
extern HRESULT ParseAudioControlTestDesc( CXMLElement *hElement, TestDesc** ppTestDesc );

#endif
