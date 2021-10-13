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
#ifndef _TEST_GRAPH_H
#define _TEST_GRAPH_H

#include <streams.h>
#include "TestInterfaces.h"
#include "DummySink.h"
#include "DummySource.h"

enum ConnectionValidation {
    Supported,
    Unsupported,
    Unknown
};

enum ConnectionOrder
{
    InputFirst,
    OutputFirst,
    EitherFirst
};

class CTestGraph;

typedef IPin* PPIN;

class CTestGraph
{
public:

    // GUIDs of the test source, sink and filter under test. Factory method for the verifier to be used.
    CTestGraph(GUID sourcGUID, GUID filterGUID, GUID sinkGUID, GUID verifierGuid);

    // An xml file specifying the GUIDs and verifier (if any).
    CTestGraph(const TCHAR* xmlfile);

    // Blah.
    virtual ~CTestGraph();

    // Returns a pointer to the filter under test. Addrefs returned filter. Creates if not already created.
    virtual IBaseFilter* GetFilter();

    // Returns a pointer to the source test filter. Addrefs returned filter. Creates if not already created.
    virtual IBaseFilter* GetSourceFilter();

    // Returns a pointer to the sink test test. Addrefs returned filter. Creates if not already created.
    virtual IBaseFilter* GetSinkFilter();

    // Returns a pointer to the test source interface. Addrefs returned interface. Creates if not already created.
    virtual ITestSource* GetTestSource();

    // Returns a pointer to the test sink interface. Addrefs returned interface. Creates if not already created.
    virtual ITestSink* GetTestSink();

    // Adds all filters to the graph.
    virtual HRESULT AddFiltersToGraph();

    // Get the underlying control for the graph
    virtual IMediaControl* GetMediaControlGraph(); 

    // Get the underlying IMediaEvent for the graph
    virtual IMediaEvent* GetMediaEventGraph(); 

    // Get the underlying filter graph builder
    virtual IGraphBuilder* GetFilterGraphBuilder(); 

    // Create all filters needed for test graph and adds them to the graph.
    virtual HRESULT CreateFilters();

    // Enumerate and acquire all pins on the filters.
    virtual HRESULT AcquirePins();

    // Disconnects connected filters and releases all resources.
    virtual HRESULT Release();

    // Connects all output pins on source filter to corresponding input pins on filter under test.
    virtual HRESULT ConnectSource();

    // Disconnects all connected output pins on source filter from corresponding input pins on filter under test.
    virtual HRESULT DisconnectSource();

    // Connects all output pins on filter under test to corresponding input pins on sink filter.
    virtual HRESULT ConnectSink();

    // Disconnects all connected output pins on filter under test from corresponding input pins on sink filter.
    virtual HRESULT DisconnectSink();

    // Connects corresponding pins from source to filter under test first and then from filter under test to sink.
    virtual HRESULT ConnectGraph(ConnectionOrder connorder = InputFirst);

    // Disconnects corresponding pins from filter under test to sink first and then from source to filter under test.
    virtual HRESULT DisconnectGraph();

    // The following methods allow the generic tests to perform more specific tests.

    // Create specific filter needed for this test graph
    virtual HRESULT CreateFilter(const GUID clsid, REFIID riid, void** ppInterface) = 0;

    // Create specific verifier needed for this test graph
    //This is not pure virtual like CreateFilter, because you only need to implement this if your test is using a Verifier
    virtual HRESULT CreateVerifier(const GUID clsid, IVerifier** ppVerifier);

    // Number of input pins on filter under test.
    virtual int GetNumFilterInputPins();

    // Number of output pins on filter under test.
    virtual int GetNumFilterOutputPins();
    
    // This allows the graph to specify if it can be connected from either direction 
    virtual ConnectionOrder GetConnectionOrder();

    // Is called right after filter to be tested is added to graph and before it is connected
    virtual HRESULT  InitializeFilter ();

    // What dummy sink to use for Media Combination test
    virtual CDummySink* GetMediaCombTestSink(HRESULT *phr, int nPins); 

    // What source to use for Media Combination test
    virtual CDummySource* GetMediaCombTestSource(HRESULT *phr, int nPins); 

    // Helper for the tests to determine what needs to be tested. This has to be overridden by the derived test graphs
    virtual MediaTypeCombination* GetMediaTypeCombination(int enumerator, bool &bValidConnection);
    virtual int GetNumMediaTypeCombinations();
    virtual ConnectionValidation ValidateMediaTypeCombination(AM_MEDIA_TYPE** ppSourceMediaType, AM_MEDIA_TYPE** ppSinkMediaType);

    // Helper for the tests to determine valid streaming modes and source of data
    virtual int GetNumStreamingModes();
    virtual bool GetStreamingMode(int enumerator, StreamingMode& mode, int& nUnits, StreamingFlags& flags, TCHAR **datasource);

    //Helper for the tests to determine the filter being tested
    virtual GUID GetFilterGUID();
    
protected:
    HRESULT GetPinArray(IBaseFilter* pFilter, IPin*** pppPins, int *pNumPins, PIN_DIRECTION pinDir);
    void ReleaseFilters();
    void ReleasePins();
    IBaseFilter *m_pFilter;
    IBaseFilter *m_pSourceFilter;
    IBaseFilter *m_pSinkFilter;
    IPin **m_ppFilterOutputPinArray;
    int m_nFilterOutputPins;
    IPin **m_ppFilterInputPinArray;
    int m_nFilterInputPins;

private:
    IGraphBuilder *m_pGraph;
    ITestSource *m_pTestSource;
    ITestSink *m_pTestSink;
    IVerifier *m_pVerifier;

    IPin **m_ppSourceOutputPinArray;
    int m_nSourceOutputPins;

    IPin **m_ppSinkInputPinArray;
    int m_nSinkInputPins;


    GUID m_guidSource;
    GUID m_guidSink;
    GUID m_guidFilter;
    GUID m_guidVerifier;
};
#endif // _TEST_GRAPH_H
