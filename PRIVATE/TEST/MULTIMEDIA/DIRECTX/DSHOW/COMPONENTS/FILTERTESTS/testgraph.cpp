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
#include <objbase.h>
#include <streams.h>
#include <stdio.h>
#include <tchar.h>
#include <dmodshow.h>

#include <Tux.h>
#include "TestGraph.h"
#include "TestInterfaces.h"
#include "FilterTest.h"
#include "common.h"

// This module contains indirect tests for some methods in IMediaControl, IPin and other interfaces that require a connected filter graph to be exercised.
// These interface contain methods that typically require the whole graph to be built before being called on the filter.
// The tests use the FilterGraph manager to exercise the interfaces, since direct calls require co-ordination between the different filters to which this is connected.

// This method helps find specific interfaces given a connected graph.
HRESULT FindInterfaceOnGraph(IGraphBuilder *pGraph, REFIID riid, void **ppInterface)
{
    HRESULT hr = E_NOINTERFACE;
    IBaseFilter* pFilter;
   	IEnumFilters* pEnumFilter = NULL;    

    if(!ppInterface)
	{
		LOG(TEXT("FindInterfaceOnGraph: null pointer received."));
        return E_FAIL;
	}
    *ppInterface= NULL;
	
	hr = pGraph->EnumFilters( &pEnumFilter );
	if (FAILED(hr))
	{
		LOG(TEXT("FindInterfaceOnGraph: EnumFilters failed."));
		return hr;
	}
    
    hr = E_NOINTERFACE;
    
	pEnumFilter->Reset();
    
    // find the first filter in the graph that supports riid interface
    while(!*ppInterface && pEnumFilter->Next(1, &pFilter, NULL) == S_OK)
    {
        hr = pFilter->QueryInterface(riid, ppInterface);
        pFilter->Release();
    }

	pEnumFilter->Release();
    return hr;
}

CTestGraph::CTestGraph(GUID sourceGuid, GUID filterGuid, GUID sinkGuid, GUID verifierGuid) :
    m_pGraph(NULL),
	m_pSourceFilter(NULL),
	m_pSinkFilter(NULL),
	m_pFilter(NULL),
	m_pTestSource(NULL),
	m_pTestSink(NULL),
	m_pVerifier(NULL),
	m_nSourceOutputPins(0),
	m_ppSourceOutputPinArray(NULL),
	m_nSinkInputPins(0),
	m_ppSinkInputPinArray(NULL),
	m_nFilterInputPins(0),
    m_ppFilterInputPinArray(NULL),
	m_nFilterOutputPins(0),
	m_ppFilterOutputPinArray(NULL)
{
	m_guidSource = sourceGuid;
	m_guidSink = sinkGuid;
	m_guidFilter = filterGuid;
	m_guidVerifier = verifierGuid;

	CoInitialize(NULL);
}

CTestGraph::CTestGraph(const TCHAR* xmlfile)
{
	// Read in GUIDs from xml file
	// Not implemented yet.
}

CTestGraph::~CTestGraph()
{
	Release();
	CoUninitialize();
}

HRESULT CTestGraph::GetPinArray(IBaseFilter* pFilter, IPin*** pppPins, int *pNumPins, PIN_DIRECTION pinDir)
{
	IEnumPins* pEnumPins = NULL;
	HRESULT hr = S_OK;
	int nPins = 0;

	if (FAILED(hr = pFilter->EnumPins(&pEnumPins)))
	{
		LOG(TEXT("GetPinArray: EnumPins failed."));
		return hr;
	}
	
	if (FAILED(hr = pEnumPins->Reset())) 
	{
		LOG(TEXT("GetPinArray: EnumPins::Reset failed."));
		pEnumPins->Release();
		return hr;
	}
		
	PIN_DIRECTION dir;
	IPin *pPin = NULL;
	ULONG cPins = 0;
	while ((hr = pEnumPins->Next(1, &pPin, 0)) == S_OK) {
		pPin->QueryDirection(&dir);
		if (dir == pinDir)
			nPins++;
		pPin->Release();
	}
	
	// Next returns S_FALSE which is a success when it goes the end
	if (nPins && SUCCEEDED(hr) && pppPins) {
		hr = S_OK;
		IPin** ppPins = (IPin**) new PPIN[nPins];
		if (!ppPins)
		{
			LOG(TEXT("GetPinArray: couldn't allocate pin array."));
			hr = E_OUTOFMEMORY;
		}
		else {
			*pppPins = ppPins;
			
			pEnumPins->Reset();
			int i = 0;
			while ((hr = pEnumPins->Next(1, &pPin, 0)) == S_OK) {
				pPin->QueryDirection(&dir);
				if (dir == pinDir) {
					ppPins[i] = pPin;
					i++;
				} else pPin->Release();
			}
			hr = S_OK;
		}
	}
	
	if (pEnumPins)
		pEnumPins->Release();

	*pNumPins = (SUCCEEDED(hr)) ? nPins : 0;

	return hr;
}

HRESULT CTestGraph::CreateVerifier(const GUID clsid, IVerifier** ppVerifier)
{
	// Incase someone doesn't need a verifier.
	//If you have a verifier you need to override this function
	return E_NOTIMPL;
}

HRESULT CTestGraph::CreateFilters()
{
	HRESULT hr = NOERROR;

	if (m_guidFilter == GUID_NULL)
		return E_UNEXPECTED;

	//Create the graph builder object
	IGraphBuilder* pGraphBuilder = NULL;
	if (!m_pGraph) {
		hr = CoCreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER, IID_IGraphBuilder, (void**)&pGraphBuilder);
		if (!pGraphBuilder)	{
			LOG(TEXT("CreateFilters: CoCreateInstance FilterGraph failed."));
		}
	}

	// Create the test source filter if there is one specified
	IBaseFilter* pSourceFilter = NULL;
	if (SUCCEEDED(hr) && !m_pSourceFilter && (m_guidSource != GUID_NULL)) {
		hr = CreateFilter(m_guidSource, IID_IBaseFilter, (void**)&pSourceFilter);	
		if (!pSourceFilter)	{
			LOG(TEXT("CreateFilters: creating the source filter failed."));
		}
	}

	// Create the test sink filter if there is one specified
	IBaseFilter* pSinkFilter = NULL;
	if (SUCCEEDED(hr) && !m_pSinkFilter && (m_guidSink != GUID_NULL)) {
		hr = CreateFilter(m_guidSink, IID_IBaseFilter, (void**)&pSinkFilter);	
		if (!pSinkFilter) {
			LOG(TEXT("CreateFilters: creating the sink filter failed."));
		}
	}

	// Create the filter to be tested - this should always be specified
	IBaseFilter* pFilter = NULL;
	if (SUCCEEDED(hr) && !m_pFilter) {
		hr = CreateFilter(m_guidFilter, IID_IBaseFilter, (void**)&pFilter);
		if (!pFilter) {
			LOG(TEXT("CreateFilters: creating the filter under test failed."));
		}
	}

	// Create the verifier
	IVerifier *pVerifier = NULL;
	if (SUCCEEDED(hr) && !m_pVerifier && (m_guidVerifier!= GUID_NULL)) {
		hr = CreateVerifier(m_guidVerifier, &pVerifier);
		if (!pVerifier) {
			LOG(TEXT("CreateFilters: creating the verifier failed."));
		}
	}

	if ( ((m_guidSource != GUID_NULL) && !m_pSourceFilter && !pSourceFilter) ||
		 ((m_guidSink != GUID_NULL) && !m_pSinkFilter && !pSinkFilter) ||
		 (!m_pFilter && !pFilter) && 
		 ((m_guidVerifier != GUID_NULL) && !m_pVerifier && !pVerifier) )
	{
		LOG(TEXT("CreateFilters: couldn't create one or more specified filters."));
		hr = E_UNEXPECTED;
	}

	// If all filters were created
	if (SUCCEEDED(hr)) {
		if (pGraphBuilder)
			m_pGraph = pGraphBuilder;
		if (pSourceFilter)
			m_pSourceFilter = pSourceFilter;
		if (pSinkFilter)
			m_pSinkFilter = pSinkFilter;
		if (pFilter)
			m_pFilter = pFilter;
		if (pVerifier)
			m_pVerifier = pVerifier;
	}
	else {
		if (pGraphBuilder)
			pGraphBuilder->Release();
		if (pSourceFilter)
			pSourceFilter->Release();
		if (pSinkFilter)
			pSinkFilter->Release();
		if (pFilter)
			pFilter->Release();
		if (pVerifier)
			delete pVerifier;
	}
	
	return hr;
}

HRESULT CTestGraph::AcquirePins()
{
	HRESULT hrPin = NOERROR;
	HRESULT hr = NOERROR;

	if (FAILED(hr = CreateFilters()))
	{
		LOG(TEXT("AcquirePins: CreateFilters failed."));
		return hr;
	}

	if (!m_nSourceOutputPins && m_pSourceFilter)
	{
		hrPin = GetPinArray(m_pSourceFilter, &m_ppSourceOutputPinArray, &m_nSourceOutputPins, PINDIR_OUTPUT);
		if (FAILED(hrPin))
		{
			LOG(TEXT("AcquirePins: GetPinArray for the source filter outputs failed."));
			hr = hrPin;
		}
	}

	if (!m_nFilterInputPins)
	{
		hrPin = GetPinArray(m_pFilter, &m_ppFilterInputPinArray, &m_nFilterInputPins, PINDIR_INPUT);
		if (FAILED(hrPin))
		{
			LOG(TEXT("AcquirePins: GetPinArray for the filter under test inputs failed."));
			hr = hrPin;
		}
	}

	if (!m_nFilterOutputPins)
	{
		hrPin = GetPinArray(m_pFilter, &m_ppFilterOutputPinArray, &m_nFilterOutputPins, PINDIR_OUTPUT);
		if (FAILED(hrPin))
		{
			LOG(TEXT("AcquirePins: GetPinArray for the filter under test outputs failed."));
			hr = hrPin;
		}
	}


	if (!m_nSinkInputPins && m_pSinkFilter)
	{
		hr = GetPinArray(m_pSinkFilter, &m_ppSinkInputPinArray, &m_nSinkInputPins, PINDIR_INPUT);
		if (FAILED(hrPin))
		{
			LOG(TEXT("AcquirePins: GetPinArray for the sink filter inputs failed."));
			hr = hrPin;
		}
	}

	return hr;
}

// This is a utility function that simply tries to connect the graph in the order specified - order has to be InputFirst or OutputFirst
HRESULT CTestGraph::ConnectGraph(ConnectionOrder connorder)
{
	HRESULT hr = NOERROR;

	if (connorder == InputFirst)
	{
		LOG(TEXT("ConnectGraph: InputFirst specified"));
		LOG(TEXT("ConnectGraph: Connecting source."));
		if (FAILED(hr = ConnectSource())) {
			LOG(TEXT("ConnectGraph: ConnectSource failed."));
			LOG(TEXT("ConnectGraph: Disconnecting any connected filters"));
			DisconnectSource();
			return hr;
		}
		LOG(TEXT("ConnectGraph: Connecting sink"));
		if (FAILED(hr = ConnectSink())) {
			LOG(TEXT("ConnectGraph: ConnectSink failed"));
			LOG(TEXT("ConnectGraph: disconnecting any connected filters"));
			DisconnectSink();
			DisconnectSource();
		}
	}
	else {
		if (connorder == OutputFirst)
			LOG(TEXT("ConnectGraph: OutputFirst specified"));
		else
			LOG(TEXT("ConnectGraph: both specified"));
		LOG(TEXT("ConnectGraph: Connecting sink"));
		if (FAILED(hr = ConnectSink())) {
			LOG(TEXT("ConnectGraph: ConnectSink failed"));
			LOG(TEXT("ConnectGraph: disconnecting any connected filters"));
			DisconnectSink();
			return hr;
		}
		LOG(TEXT("ConnectGraph: Connecting source"));
		if (FAILED(hr = ConnectSource())) {
			LOG(TEXT("ConnectGraph: ConnectSource failed"));
			LOG(TEXT("ConnectGraph: disconnecting any connected filters"));
			DisconnectSource();
			DisconnectSink();
		}
	}

	return hr;
}

IBaseFilter* CTestGraph::GetFilter()
{
	if (!m_pFilter)
		CreateFilters();
	// Addref the object before we return
	if (m_pFilter)
	    m_pFilter->AddRef();
	return m_pFilter;
}

IBaseFilter* CTestGraph::GetSourceFilter()
{
	if (!m_pSourceFilter)
		CreateFilters();
	if (m_pSourceFilter)
		m_pSourceFilter->AddRef();
	return m_pSourceFilter;
}

IBaseFilter* CTestGraph::GetSinkFilter()
{
	if (!m_pSinkFilter)
		CreateFilters();
	if (m_pSinkFilter)
		m_pSinkFilter->AddRef();
	return m_pSinkFilter;
}

ITestSource* CTestGraph::GetTestSource()
{
	if (!m_pTestSource)
	{
		// Let's see if the source filter has been created
		if (!m_pSourceFilter)
			CreateFilters();
		
		// If still not, we have a problem
		if (!m_pSourceFilter)
			return NULL;
		
		// Query the source filter for a test source interface
		m_pSourceFilter->QueryInterface(IID_ITestSource, (void**)&m_pTestSource);

		// If thre is a verifier, set it
		if (m_pTestSource && m_pVerifier)
			m_pTestSource->SetVerifier(m_pVerifier);
	}

	if (m_pTestSource)
	{
		// Add an additional referernce
		m_pTestSource->AddRef();
	}
	
	return m_pTestSource;
}

ITestSink* CTestGraph::GetTestSink()
{
	if (!m_pTestSink)
	{
		// Let's see if the Sink filter has been created
		if (!m_pSinkFilter)
			CreateFilters();
		
		// If still not, we have a problem
		if (!m_pSinkFilter)
			return NULL;
		
		// Query the Sink filter for a test Sink interface
		m_pSinkFilter->QueryInterface(IID_ITestSink, (void**)&m_pTestSink);

		// If thre is a verifier, set it
		if (m_pTestSink && m_pVerifier)
			m_pTestSink->SetVerifier(m_pVerifier);
	}

	if (m_pTestSink)
	{
		// Add an additional referernce
		m_pTestSink->AddRef();
	}
	
	return m_pTestSink;
}

HRESULT CTestGraph::AddFiltersToGraph()
{
	HRESULT hr = NOERROR;
	if (FAILED(hr = CreateFilters()))
		return hr;

	IBaseFilter* pFoundFilter = NULL;

	// Add the source filter to the graph - FindFilterByName returns S_OK if it finds the filter and VFW_E_NOT_FOUND if it cant
	if (m_pSourceFilter && FAILED(m_pGraph->FindFilterByName(L"Test Source Filter", &pFoundFilter)))
	{
		hr = m_pGraph->AddFilter(m_pSourceFilter, L"Test Source Filter");
		if (FAILED(hr))
			LOG(TEXT("AddFiltersToGraph: IGraphBuilder::AddFilter failed for test source filter."));
	}
	else {
		if (pFoundFilter) {
			pFoundFilter->Release();
			pFoundFilter = NULL;
		}
	}

	// Add the sink filter to the graph
	if (SUCCEEDED(hr) && m_pSinkFilter && FAILED(m_pGraph->FindFilterByName(L"Test Sink Filter", &pFoundFilter)))
	{
		// Add the sink filter to the graph
		hr = m_pGraph->AddFilter(m_pSinkFilter, L"Test Sink Filter");
		if (FAILED(hr))
			LOG(TEXT("AddFiltersToGraph: IGraphBuilder::AddFilter failed for test sink filter."));
	}
	else {
		if (pFoundFilter) {
			pFoundFilter->Release();
			pFoundFilter = NULL;
		}
	}

	// Add the filter to the graph
	if (SUCCEEDED(hr) && FAILED(m_pGraph->FindFilterByName(L"Filter Under Test", &pFoundFilter)))
	{
		// Add the filter to the graph
		hr = m_pGraph->AddFilter(m_pFilter, L"Filter Under Test");
		if (FAILED(hr))
			LOG(TEXT("AddFiltersToGraph: IGraphBuilder::AddFilter failed for filter under test."));
		
		//Initialize filter if need be
		hr = InitializeFilter();
		if (FAILED(hr))
			LOG(TEXT("AddFiltersToGraph: IGraphBuilder::InitializeFilter failed for filter under test."));
	}
	else {
		if (pFoundFilter) {
			pFoundFilter->Release();
			pFoundFilter = NULL;
		}		
	}

	return hr;
}

HRESULT CTestGraph::InitializeFilter() {
	//default implementation to return S_OK
	return S_OK;
}


IMediaControl* CTestGraph::GetMediaControlGraph()
{
	IMediaControl* pMediaControl = NULL;
	if (!m_pGraph)
		CreateFilters();
	if (m_pGraph)
		m_pGraph->QueryInterface(IID_IMediaControl, (void**) &pMediaControl);
	return pMediaControl;
}

IMediaEvent* CTestGraph::GetMediaEventGraph()
{
	IMediaEvent* pMediaEvent = NULL;
	if (!m_pGraph)
		CreateFilters();
	if (m_pGraph)
		m_pGraph->QueryInterface(IID_IMediaEvent, (void**) &pMediaEvent);
	return pMediaEvent;
}

IGraphBuilder* CTestGraph::GetFilterGraphBuilder()
{
	if (!m_pGraph)
		CreateFilters();
	if (m_pGraph)
		m_pGraph->AddRef();
	return m_pGraph;
}

void CTestGraph::ReleasePins()
{
	if (m_nSourceOutputPins)
	{
		for(int i = 0; i < m_nSourceOutputPins; i++) {
			m_ppSourceOutputPinArray[i]->Release();
			m_ppSourceOutputPinArray[i] = NULL;
		}
		delete [] m_ppSourceOutputPinArray;
		m_nSourceOutputPins = 0;
	}

	if (m_nSinkInputPins)
	{
		for(int i = 0; i < m_nSinkInputPins; i++) {
			m_ppSinkInputPinArray[i]->Release();
			m_ppSinkInputPinArray[i] = NULL;
		}
		delete [] m_ppSinkInputPinArray;
		m_nSinkInputPins;
	}

	if (m_nFilterOutputPins)
	{
		for(int i = 0; i < m_nFilterOutputPins; i++) {
			m_ppFilterOutputPinArray[i]->Release();
			m_ppFilterOutputPinArray[i] = NULL;
		}
		delete [] m_ppFilterOutputPinArray;
		m_nFilterOutputPins = 0;
	}

	if (m_nFilterInputPins)
	{
		for(int i = 0; i < m_nFilterInputPins; i++) {
			m_ppFilterInputPinArray[i]->Release();
			m_ppFilterInputPinArray[i] = NULL;
		}
		delete [] m_ppFilterInputPinArray;
		m_nFilterInputPins;
	}
}

void CTestGraph::ReleaseFilters()
{
	if (m_pTestSource)
	{
		// Release the reference added during QueryInterface
		m_pTestSource->Release();
	}

	if (m_pTestSink)
	{
		// Release the reference added during QueryInterface
		m_pTestSink->Release();
	}

	if (m_pSourceFilter) {
		if (m_pGraph)
			m_pGraph->RemoveFilter(m_pSourceFilter);
		m_pSourceFilter->Release();
		m_pSourceFilter = NULL;
	}

	if (m_pSinkFilter) {
		if (m_pGraph)
			m_pGraph->RemoveFilter(m_pSinkFilter);
		m_pSinkFilter->Release();
		m_pSinkFilter = NULL;
	}

	if (m_pFilter) {
		if (m_pGraph)
			m_pGraph->RemoveFilter(m_pFilter);
		m_pFilter->Release();
		m_pFilter = NULL;
	}

	if (m_pGraph) {
		m_pGraph->Release();
		m_pGraph = NULL;
	}
}

HRESULT CTestGraph::Release()
{
	HRESULT hrRelease;
	HRESULT hr = NOERROR;
	if (FAILED(hrRelease = DisconnectSource()))
		hr = hrRelease;

	if (FAILED(hrRelease = DisconnectSink()))
		hr = hrRelease;

	ReleasePins();

	ReleaseFilters();

	if (m_pVerifier)
	{
		m_pVerifier->Reset();
		delete m_pVerifier;
	}

	return hr;
}

// This method will succeed if called for a graph with no test source.
HRESULT CTestGraph::ConnectSource()
{
	HRESULT hr = NOERROR;

	if (FAILED(hr = CreateFilters()) || FAILED(hr = AcquirePins()))
	{
		LOG(TEXT("ConnectSource: CreateFilters or AcquirePins failed."));
		return hr;
	}

	if (FAILED(hr = AddFiltersToGraph()))
	{
		LOG(TEXT("ConnectSource: AddFiltersToGraph failed."));	
		return hr;
	}

	for(int i = 0; i < m_nSourceOutputPins && i < m_nFilterInputPins; i++) {
		hr = m_pGraph->ConnectDirect(m_ppSourceOutputPinArray[i], m_ppFilterInputPinArray[i], NULL);
		if (FAILED(hr))
		{
			LOG(TEXT("ConnectSource: IGraphBuilder::ConnectDirect failed to connect source output to filter input."));
			break;
		}
	}
	if (FAILED(hr)) {
		LOG(TEXT("ConnectSource: disconnecting any connected pins."));
		for(int i = 0; i < m_nSourceOutputPins; i++) {
			IPin *pConnectedPin = NULL;
			// ConnectedTo results in an outstanding ref count on pConnectedPin
			m_ppSourceOutputPinArray[i]->ConnectedTo(&pConnectedPin);
			if (pConnectedPin) {
				m_pGraph->Disconnect(m_ppSourceOutputPinArray[i]);
				m_pGraph->Disconnect(pConnectedPin);
				pConnectedPin->Release();
			}
		}
	}

	return hr; 
}

HRESULT CTestGraph::DisconnectSource()
{
	HRESULT hr = NOERROR;
	if(m_ppSourceOutputPinArray) {
		for(int i = 0; i < m_nSourceOutputPins; i++) {
			IPin *pConnectedPin = NULL;
			// ConnectedTo results in an outstanding ref count on pConnectedPin
			if(m_ppSourceOutputPinArray[i]) {
				m_ppSourceOutputPinArray[i]->ConnectedTo(&pConnectedPin);
				if (pConnectedPin) {
					HRESULT hrDisconnect = m_pGraph->Disconnect(m_ppSourceOutputPinArray[i]);
					if (FAILED(hrDisconnect))
					{
						LOG(TEXT("DisconnectSource: IGraphBuilder::Disconnect failed for test source filter output."));
						hr = hrDisconnect;
					}
					hrDisconnect = m_pGraph->Disconnect(pConnectedPin);
					if (SUCCEEDED(hr) && FAILED(hrDisconnect))
					{
						LOG(TEXT("DisconnectSource: IGraphBuilder::Disconnect failed for test source filter output's connected pin."));
						hr = hrDisconnect;
					}
					pConnectedPin->Release();
				}
			}
		}
	}
	return hr;
}

// This method will succeed if called for a graph with no test sink.
HRESULT CTestGraph::ConnectSink()
{
	HRESULT hr = NOERROR;

	if (FAILED(hr = CreateFilters()))
	{
		LOG(TEXT("ConnectSink: CreateFiltersfailed."));
		return hr;
	}

	if (FAILED(hr = AddFiltersToGraph()))
	{
		LOG(TEXT("ConnectSink: AddFiltersToGraph failed."));	
		return hr;
	}

	if(FAILED(hr = AcquirePins()))
	{
		LOG(TEXT("ConnectSink: AcquirePins failed."));
		return hr;
	}

	for(int i = 0; i < m_nFilterOutputPins && i < m_nSinkInputPins; i++) {
		hr = m_pGraph->ConnectDirect(m_ppFilterOutputPinArray[i], m_ppSinkInputPinArray[i], NULL);
		if (FAILED(hr))
		{
			LOG(TEXT("ConnectSink: IGraphBuilder::ConnectDirect failed to connect filter output to sink input."));
			break;
		}
	}
	if (FAILED(hr)) {
		LOG(TEXT("ConnectSink: disconnecting any connected pins."));
		for(int i = 0; i < m_nSinkInputPins; i++) {
			IPin *pConnectedPin = NULL;
			// ConnectedTo results in an outstanding ref count on pConnectedPin
			m_ppFilterOutputPinArray[i]->ConnectedTo(&pConnectedPin);
			if (pConnectedPin) {
				m_pGraph->Disconnect(m_ppFilterOutputPinArray[i]);
				m_pGraph->Disconnect(pConnectedPin);
				pConnectedPin->Release();
			}
		}
	}
	
	return hr;
}


HRESULT CTestGraph::DisconnectSink()
{
	HRESULT hr = NOERROR;

	if(m_ppFilterOutputPinArray) {
		for(int i = 0; i < m_nSinkInputPins; i++) {
			IPin *pConnectedPin = NULL;
			// ConnectedTo results in an outstanding ref count on pConnectedPin
			if(m_ppFilterOutputPinArray[i]) {
				m_ppFilterOutputPinArray[i]->ConnectedTo(&pConnectedPin);
				if (pConnectedPin) {
					HRESULT hrDisconnect = m_pGraph->Disconnect(m_ppFilterOutputPinArray[i]);
					if (FAILED(hrDisconnect))
						hr = hrDisconnect;
					hrDisconnect = m_pGraph->Disconnect(pConnectedPin);
					if (SUCCEEDED(hr) && FAILED(hrDisconnect))
						hr = hrDisconnect;
					pConnectedPin->Release();
				}
			}
		}
	}
	return hr;
}

HRESULT CTestGraph::DisconnectGraph()
{
	HRESULT hrSource = NOERROR;
	HRESULT hrSink = NOERROR;
	
	// If we actually had a source filter
	if (m_guidSource != GUID_NULL)
		hrSource = DisconnectSource();

	if (m_guidSink != GUID_NULL)
		hrSink = DisconnectSink();
	
	if (FAILED(hrSource))
		return hrSource;
	else return hrSink;
}
	
ConnectionOrder CTestGraph::GetConnectionOrder()
{
	// Default is to return EitherFirst
	return EitherFirst;
}

int CTestGraph::GetNumFilterInputPins()
{
	// This should be overridden by the derived test graph
	return -1;
}

int CTestGraph::GetNumFilterOutputPins()
{
	// This should be overridden by the derived test graph
	return -1;
}

MediaTypeCombination* CTestGraph::GetMediaTypeCombination(int enumerator, bool &bValidConnection)
{
	// This should be overridden by the derived test graph
	return NULL;
}

ConnectionValidation CTestGraph::ValidateMediaTypeCombination(AM_MEDIA_TYPE** ppSourceMediaType, AM_MEDIA_TYPE** ppSinkMediaType)
{
	// This should be overridden by the derived test graph
	return Unknown;
}

int CTestGraph::GetNumMediaTypeCombinations()
{
	// This should be overridden by the derived test graph
	return 0;
}

int CTestGraph::GetNumStreamingModes()
{
	// This should be overridden by the derived test graph
	return 0;
}

bool CTestGraph::GetStreamingMode(int enumerator, StreamingMode& mode, int& nUnits, StreamingFlags& flags, TCHAR** datasource)
{
	// This should be overridden by the derived test graph
	return false;
}

CDummySink* CTestGraph::GetMediaCombTestSink(HRESULT *phr, int nPins) {
    //this should be overridden by the derived test graph if they have a custom dummy sink
    return new CDummySink(phr, nPins);
}

CDummySource* CTestGraph::GetMediaCombTestSource(HRESULT *phr, int nPins) {
    //this should be overridden by the derived test graph if they have a custom dummy source
    return new CDummySource(phr, nPins);
}
