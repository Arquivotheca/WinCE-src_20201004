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
#ifndef _VIDCAP_TEST_GRAPH_H
#define _VIDCAP_TEST_GRAPH_H

#include "TestGraph.h"

extern CTestGraph* CreateVidCapTestGraph();

#define NOUTPUTS 3
#define NCOMBINATIONS 3

class CVidCapTestGraph : public CTestGraph
{
public:
	// Override only needed methods
	CVidCapTestGraph();
	virtual ~CVidCapTestGraph();

	// CTestGraph virtual functions
	virtual HRESULT CreateFilter(const GUID clsid, REFIID riid, void** ppInterface);
	virtual HRESULT CreateVerifier(const GUID clsid, IVerifier** ppVerifier);
	virtual int GetNumFilterInputPins();
	virtual int GetNumFilterOutputPins();
	virtual ConnectionOrder GetConnectionOrder();

	virtual MediaTypeCombination* GetMediaTypeCombination(int enumerator, bool &bValidConnection);
	virtual int GetNumMediaTypeCombinations();
	ConnectionValidation ValidateMediaTypeCombination(AM_MEDIA_TYPE** ppSourceMediaType, AM_MEDIA_TYPE** ppSinkMediaType);

	virtual int GetNumStreamingModes();
	virtual bool GetStreamingMode(int enumerator, StreamingMode& mode, int& nUnits, StreamingFlags& flags, TCHAR** datasource);

private:
	// The outputs from the video capture filter could be variable. We need to determine this.
	int m_nOutputs;

	// Media type combinations - both valid and invalid
	MediaTypeCombination m_mtc[NCOMBINATIONS];

private:
	// Create the video cap filter with the right driver
	HRESULT CreateVideoCaptureFilter(REFIID riid, void** ppInterface);

	// Create the vid cap test sink filter
	HRESULT CreateVidCapTestSink(REFIID riid, void** ppInterface);

	// Creat the valid and invalid media type combinations to be used in the tests
	void CreateMediaTypeCombinations();
};

#endif