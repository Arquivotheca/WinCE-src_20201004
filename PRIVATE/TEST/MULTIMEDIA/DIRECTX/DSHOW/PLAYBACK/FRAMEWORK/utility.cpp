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
////////////////////////////////////////////////////////////////////////////////

#include <dshow.h>
#include <urlmon.h>
#include <math.h>
#include <amstream.h>
#include <streams.h>
#include <ddraw.h>
#include "logging.h"
#include "utility.h"

#if 0
class BindStatusCallback : public IBindStatusCallback, public CUnknown
{
public:
	bool m_bDownload;

public:
	DECLARE_IUNKNOWN;
	
	BindStatusCallback() : CUnknown(TEXT("BindStatusCallback"), NULL), m_bDownload (false)
	{
	}

	~BindStatusCallback()
	{
	}

	// implements IBindStatusCallback
	STDMETHODIMP GetBindInfo(DWORD *grfBINDF,BINDINFO *pbindinfo)
	{
		return E_NOTIMPL;
	}

	STDMETHODIMP GetPriority(LONG *pnPriority)
	{
		*pnPriority = THREAD_PRIORITY_NORMAL;
		return S_OK;
	}

	STDMETHODIMP OnDataAvailable(DWORD grfBSCF, DWORD dwSize, FORMATETC *pformatetc, STGMEDIUM *pstgmed)
	{
		return S_OK;
	}

	STDMETHODIMP OnLowResource(DWORD reserved)
	{
		return S_OK;  // we don't care
	}

	STDMETHODIMP OnObjectAvailable(REFIID riid,IUnknown *punk)
	{
		return S_OK;
	}

	STDMETHODIMP OnProgress(ULONG ulProgress,ULONG ulProgressMax,
			ULONG ulStatusCode,LPCWSTR szStatusText)
	{
		return S_OK;  // we don't care
	}

	STDMETHODIMP OnStartBinding(DWORD grfBSCOption,IBinding *pib)
	{
		return S_OK;  // we don't care
	}

	STDMETHODIMP OnStopBinding(HRESULT hresult,LPCWSTR szError)
	{
		m_bDownload = true;
		return S_OK;  // we don't care
	}
};
#endif

HRESULT FindInterfaceOnGraph(IGraphBuilder *pFilterGraph, REFIID riid, void **ppInterface)
{
    HRESULT hr = E_NOINTERFACE;
    IEnumFilters* pEnumFilters = NULL;
    IBaseFilter* pFilter = NULL;
    ULONG ulFetched = 0;
    
    if(!ppInterface)
        return E_FAIL;

    hr = pFilterGraph->EnumFilters(&pEnumFilters);
    if (FAILED(hr))
    {
        return hr;
    }

    *ppInterface = NULL;
    pEnumFilters->Reset();
    
    // find the first filter in the graph that supports riid interface
    while(!*ppInterface && (pEnumFilters->Next(1, &pFilter, NULL) == S_OK))
    {
        hr = pFilter->QueryInterface(riid, ppInterface);
        pFilter->Release();
    }
    
    return hr;
}

HRESULT FindInterfaceOnGraph(IGraphBuilder *pFilterGraph, REFIID riid, void **ppInterface, 
                                    IEnumFilters* pEnumFilter, BOOL begin )
{
    HRESULT hr = E_NOINTERFACE;
    IBaseFilter*   pFilter;
    ULONG ulFetched = 0;
    
    if(!ppInterface)
        return E_FAIL;
    *ppInterface= NULL;
    
    hr = E_NOINTERFACE;
    
    if( begin )
        pEnumFilter->Reset();
    
    // find the first filter in the graph that supports riid interface
    while(!*ppInterface && pEnumFilter->Next(1, &pFilter, NULL) == S_OK)
    {
        hr = pFilter->QueryInterface(riid, ppInterface);
        pFilter->Release();
    }
    
    return hr;
}

HRESULT FindQualProp(REFIID refiid, IGraphBuilder* pGraph, IQualProp** pQualProp)
{
    HRESULT hr = E_NOINTERFACE;
    IBaseFilter *pBF = NULL;
    IEnumFilters* pEnumFilter = NULL;
    
    if (!pGraph)
        return E_INVALIDARG;

    pGraph->EnumFilters( &pEnumFilter );

    if( FindInterfaceOnGraph( pGraph, refiid, (void **)&pBF, pEnumFilter, TRUE ) == NOERROR )
    {
        // Get the QualProp from the renderer which matches our requested type
        if(refiid == IID_IBasicAudio || refiid == IID_IBasicVideo)
        {
            hr = pBF->QueryInterface(IID_IQualProp, (void **)pQualProp);
        }
        pBF->Release();
    }

    if(NULL != pEnumFilter)
        pEnumFilter->Release();

    return hr;
}

HRESULT FindDroppedFrames(REFIID refiid, IGraphBuilder* pGraph, IAMDroppedFrames** pDroppedFrames)
{
    HRESULT hr = E_NOINTERFACE;
    IBaseFilter *pBF = NULL;
    IEnumFilters* pEnumFilter = NULL;
    
    if (!pGraph)
        return E_INVALIDARG;

    pGraph->EnumFilters( &pEnumFilter );

    if( FindInterfaceOnGraph( pGraph, refiid, (void **)&pBF, pEnumFilter, TRUE ) == NOERROR )
    {
        // Get the QualProp from the renderer which matches our requested type
        if(refiid == IID_IAMDroppedFrames)
        {
            hr = pBF->QueryInterface(IID_IAMDroppedFrames, (void **)pDroppedFrames);
        }
        pBF->Release();
    }

    if(NULL != pEnumFilter)
        pEnumFilter->Release();

    return hr;
}


HRESULT FindAudioGlitch(REFIID refiid, IGraphBuilder* pGraph, IAMAudioRendererStats** pDroppedFrames)
{
    HRESULT hr = E_NOINTERFACE;
    IBaseFilter *pBF = NULL;
    IEnumFilters* pEnumFilter = NULL;
    
    if (!pGraph)
        return E_INVALIDARG;

    pGraph->EnumFilters( &pEnumFilter );

    if( FindInterfaceOnGraph( pGraph, refiid, (void **)&pBF, pEnumFilter, TRUE ) == NOERROR )
    {
        // Get the QualProp from the renderer which matches our requested type
        if(refiid == IID_IAMAudioRendererStats)
        {
            hr = pBF->QueryInterface(IID_IAMAudioRendererStats, (void **)pDroppedFrames);
        }
        pBF->Release();
    }

    if(NULL != pEnumFilter)
        pEnumFilter->Release();

    return hr;
}

// Display the event code.
void DisplayECEvent(long lEventCode)
{
    switch (lEventCode)
    {
        case E_ABORT:
            LOG(TEXT("  E_ABORT\r\n"));
            break;
        case EC_COMPLETE:
            LOG(TEXT("  EC_COMPLETE\r\n"));
            break;
        case EC_ACTIVATE:
            LOG(TEXT("  EC_ACTIVATE\r\n"));
            break;
        case EC_BUFFERING_DATA:
            LOG(TEXT("  EC_BUFFERING_DATA\r\n"));
            break;
        case EC_CLOCK_CHANGED:
            LOG(TEXT("  EC_CLOCK_CHANGED\r\n"));
            break;
        case EC_DISPLAY_CHANGED:
            LOG(TEXT("  EC_DISPLAY_CHANGED\r\n"));
            break;
        case EC_END_OF_SEGMENT:
            LOG(TEXT("  EC_END_OF_SEGMENT\r\n"));
            break;
        case EC_ERROR_STILLPLAYING:
            LOG(TEXT("  EC_ERROR_STILLPLAYING\r\n"));
            break;
        case EC_ERRORABORT:
            LOG(TEXT("  EC_ERRORABORT\r\n"));
            break;
        case EC_FULLSCREEN_LOST:
            LOG(TEXT("  EC_FULLSCREEN_LOST\r\n"));
            break;
        case EC_NEED_RESTART:
            LOG(TEXT("  EC_NEED_RESTART\r\n"));
            break;
        case EC_NOTIFY_WINDOW:
            LOG(TEXT("  EC_NOTIFY_WINDOW\r\n"));
            break;
        case EC_OLE_EVENT:
            LOG(TEXT("  EC_OLE_EVENT\r\n"));
            break;
        case EC_OPENING_FILE:
            LOG(TEXT("  EC_OPENING_FILE\r\n"));
            break;
        case EC_PALETTE_CHANGED:
            LOG(TEXT("  EC_PALETTE_CHANGED\r\n"));
            break;
        case EC_QUALITY_CHANGE:
            LOG(TEXT("  EC_QUALITY_CHANGE\r\n"));
            break;
        case EC_REPAINT:
            LOG(TEXT("  EC_REPAINT\r\n"));
            break;
        case EC_SEGMENT_STARTED:
            LOG(TEXT("  EC_SEGMENT_STARTED\r\n"));
            break;
        case EC_SHUTTING_DOWN:
            LOG(TEXT("  EC_SHUTTING_DOWN\r\n"));
            break;
        case EC_STARVATION:
            LOG(TEXT("  EC_STARVATION\r\n"));
            break;
        case EC_STREAM_CONTROL_STARTED:
            LOG(TEXT("  EC_STREAM_CONTROL_STARTED\r\n"));
            break;
        case EC_STREAM_CONTROL_STOPPED:
            LOG(TEXT("  EC_STREAM_CONTROL_STOPPED\r\n"));
            break;
        case EC_STREAM_ERROR_STILLPLAYING:
            LOG(TEXT("  EC_STREAM_ERROR_STILLPLAYING\r\n"));
            break;
        case EC_STREAM_ERROR_STOPPED:
            LOG(TEXT("  EC_STREAM_ERROR_STOPPED\r\n"));
            break;
        case EC_TIME:
            LOG(TEXT("  EC_TIME\r\n"));
            break;
        case EC_USERABORT:
            LOG(TEXT("  EC_USERABORT\r\n"));
            break;
        case EC_VIDEO_SIZE_CHANGED:
            LOG(TEXT("  EC_VIDEO_SIZE_CHANGED\r\n"));
            break;
        case EC_WINDOW_DESTROYED:
            LOG(TEXT("  EC_WINDOW_DESTROYED\r\n"));
            break;

        // Possible zero events caused by failure in GetEvent()
        case 0:
            LOG(TEXT("  Got a ZERO event\r\n"));
            break;

        default:
            LOG(TEXT("  UNKOWN EC_ code 0x%x\r\n"), lEventCode);
            break;
    }
}


// This is a utility function changes a registry key to set the MinRcvBuffer size to some ad-hoc minimum value.
bool SetupNetworkBuffers()
{
    HKEY hKey;
    DWORD disposition;
    
    if (ERROR_SUCCESS != RegCreateKeyEx(HKEY_CURRENT_USER, _T("Software\\Microsoft\\NetShow\\Player\\General"), 
                    0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hKey, &disposition))
    {
        LOG(_T("SetupNetworkBuffers: Open key failed."));
        return false;
    }

    // Increase the min receive buffer to a large enough size for the tests to run.
    DWORD value = 0xc0000;

    if (ERROR_SUCCESS != RegSetValueEx(hKey, _T("MinRcvBuf"), 0, REG_DWORD, (const BYTE*)&value, sizeof(value)))
    {
        LOG(_T("SetupNetworkBuffers: Set key failed."));
        return false;
    }

    RegCloseKey(hKey);
    return true;
}

bool EnableBWSwitch(bool bwswitch)
{
    HKEY hKey;
    DWORD disposition;

    LOG(_T("EnableBWSwitch: bwswitch = %d"), bwswitch);
    
    if (ERROR_SUCCESS != RegCreateKeyEx(HKEY_CURRENT_USER, _T("Software\\Microsoft\\NetShow\\Player"),
                    0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hKey, &disposition))
    {
        LOG(_T("EnableBWSwitch: Open key failed."));
        return false;
    }

    // Increase the min receive buffer to a large enough size for the tests to run.
    DWORD value = !bwswitch;

    if (ERROR_SUCCESS != RegSetValueEx(hKey, _T("disablebandwidthswitch"), 0, REG_DWORD, (const BYTE*)&value, sizeof(value)))
    {
        LOG(_T("EnableBWSwitch: Set key failed."));
        return false;
    }

    RegCloseKey(hKey);
    return 0;
}

long WaitForEvent(IMediaEvent* pMediaEvent, long eventCode, long *pParam1, long* pParam2, DWORD dwTimeout)
{
    HANDLE hEvent;
    if (FAILED(pMediaEvent->GetEventHandle((OAEVENT*)&hEvent)))
        return false;
    while(1)
    {
        DWORD dw = WaitForSingleObject(hEvent, dwTimeout);
        if (dw == WAIT_OBJECT_0)
        {
            long lEventCode, lParam1, lParam2;
            HRESULT hr = pMediaEvent->GetEvent(&lEventCode, &lParam1, &lParam2, 0);
            if (FAILED(hr))
            {
                return -1;
            }
            else {
                if (lEventCode == eventCode)  {
                    if (pParam1)
                        *pParam1 = lParam1;
                    if (pParam2)
                        *pParam2 = lParam2;
                    return lEventCode;
                }
                else if ((lEventCode == EC_COMPLETE) || 
                    (lEventCode == EC_USERABORT) || (lEventCode == EC_ERRORABORT))
                    return lEventCode;
            }
        }
    }
    return false;
}

/**
  * Finds tokens destructively, replacing the delimiter with a '\0'.
  */
TCHAR* GetNextToken(TCHAR* string, int* pos, TCHAR delimiter)
{
    if (!string || !pos)
        return NULL;

    if (*pos == -1)
        return NULL;

    TCHAR* pStart = string + *pos;
    TCHAR* pEnd = _tcschr(pStart, delimiter);
    if (pEnd)
    {
        *pos = pEnd - string + 1;
        *pEnd = TEXT('\0');
    }
    else {
        *pos = -1;
    }
    return pStart;
}

HRESULT UrlDownloadFile(TCHAR* szUrlFile, TCHAR* szLocalFile, HANDLE* pFileHandle)
{
    HRESULT hr = S_OK;

    // First see if copy file works
    BOOL bCopyOk = CopyFile(szUrlFile, szLocalFile, false);
    if (bCopyOk)
        return S_OK;

    // Try using URLMON to download the file
    hr = URLDownloadToFile(NULL, szUrlFile, szLocalFile, 0, NULL);
    if (hr != S_OK)
    {
        LOG(TEXT("Downloading url %s failed: %x"), szUrlFile, hr);
        return E_FAIL;
    }

    HANDLE hFile = CreateFile(szLocalFile,
                            GENERIC_READ,
                            FILE_SHARE_READ | FILE_SHARE_WRITE,
                            NULL,
                            OPEN_EXISTING,
                            FILE_ATTRIBUTE_NORMAL,
                            NULL);

    if (hFile == INVALID_HANDLE_VALUE)
    {
        LOG(TEXT("%S: ERROR %d@%S - failed to open downloaded file %s"), __FUNCTION__, __LINE__, __FILE__, szLocalFile);
        return E_FAIL;
    }

    // If the user asks for the file handle return it, otherwise close the file
    if (pFileHandle)
        *pFileHandle = hFile;
    else CloseHandle(hFile);

    return S_OK;
}

HRESULT SetMaxVRBackBuffers(int dwMaxBackBuffers)
{
    HRESULT hr = S_OK;
    HKEY hKey;
    DWORD disposition;
    
    if (ERROR_SUCCESS != RegCreateKeyEx(HKEY_LOCAL_MACHINE, _T("Software\\Microsoft\\DirectX\\DirectShow\\Video Renderer"), 
                    0, NULL, 0, 0, NULL, &hKey, &disposition))
    {
        LOG(_T("SetMaxVRBackBuffers: opening key failed."));
        return E_FAIL;
    }

    // Set the flag
    if (ERROR_SUCCESS != RegSetValueEx(hKey, _T("MaxBackBuffers"), 0, REG_DWORD, (const BYTE*)&dwMaxBackBuffers, sizeof(dwMaxBackBuffers)))
    {
        LOG(_T("SetMaxVRBackBuffers: setting max back buffers reg key failed."));
        hr = E_FAIL;
    }

    RegCloseKey(hKey);

    return hr;
}

HRESULT SetVRPrimaryFlipping(bool bAllowPrimaryFlipping)
{
    HRESULT hr = S_OK;
    HKEY hKey;
    DWORD disposition;
	DWORD dwAllowPrimaryFlipping = (bAllowPrimaryFlipping) ? true : false;
    
    if (ERROR_SUCCESS != RegCreateKeyEx(HKEY_LOCAL_MACHINE, _T("Software\\Microsoft\\DirectX\\DirectShow\\Video Renderer"), 
                    0, NULL, 0, 0, NULL, &hKey, &disposition))
    {
        LOG(_T("%S: opening VR regkey failed."), __FUNCTION__);
        return E_FAIL;
    }

    // Set the flag
    if (ERROR_SUCCESS != RegSetValueEx(hKey, _T("AllowPrimaryFlipping"), 0, REG_DWORD, (const BYTE*)&dwAllowPrimaryFlipping, sizeof(dwAllowPrimaryFlipping)))
    {
        LOG(_T("%S: setting VR primary flipping regkey failed."), __FUNCTION__);
        hr = E_FAIL;
    }

    RegCloseKey(hKey);

    return hr;
}

HRESULT SetVRScanLineUsage(bool bUseScanLine)
{
    HRESULT hr = S_OK;
    HKEY hKey;
    DWORD disposition;
	DWORD dwUsage = (bUseScanLine) ? 1 : 0;

    if (ERROR_SUCCESS != RegCreateKeyEx(HKEY_LOCAL_MACHINE, _T("Software\\Microsoft\\DirectX\\DirectShow\\Video Renderer"), 
                    0, NULL, 0, 0, NULL, &hKey, &disposition))
    {
        LOG(_T("%S: opening key failed."), __FUNCTION__);
        return E_FAIL;
    }

    // Set the flag
    if (ERROR_SUCCESS != RegSetValueEx(hKey, _T("UseScanLine"), 0, REG_DWORD, (const BYTE*)&dwUsage, sizeof(dwUsage)))
    {
        LOG(_T("%S: setting scanline reg key failed."), __FUNCTION__);
        hr = E_FAIL;
    }

    RegCloseKey(hKey);

    return hr;
}

bool EnableWMFrameDropping(bool bEnable)
{
    HKEY hKey;
    DWORD disposition;
    
    if (ERROR_SUCCESS != RegCreateKeyEx(HKEY_LOCAL_MACHINE, _T("Software\\Microsoft\\DirectX\\DirectShow\\WMVDecoder"), 
                    0, NULL, 0, 0, NULL, &hKey, &disposition))
    {
        LOG(_T("EnableFrameDropping: opening key failed."));
        return false;
    }

    // Set the flag
    DWORD bDoNotDropFrames = !bEnable;

    if (ERROR_SUCCESS != RegSetValueEx(hKey, _T("DoNotDropFrames"), 0, REG_DWORD, (const BYTE*)&bDoNotDropFrames, sizeof(bDoNotDropFrames)))
    {
        LOG(_T("EnableFrameDropping: setting key failed."));
        return false;
    }

    RegCloseKey(hKey);
    return true;
}

bool ToleranceCheck(DWORD expected, DWORD actual, DWORD pctTolerance)
{
    return (fabs((double)expected - (double)actual) <= ((double)pctTolerance*expected)/100);
}

bool ToleranceCheck(LONGLONG expected, LONGLONG actual, DWORD pctTolerance)
{
    return (fabs((double)expected - (double)actual) <= ((double)pctTolerance*expected)/100);
}

bool ToleranceCheckAbs(LONGLONG expected, LONGLONG actual, DWORD tolerance)
{
    return (fabs((double)expected - (double)actual) <= tolerance);
}

bool LimitCheck(DWORD value, DWORD actual, DWORD pctLimit)
{
    return ((double)actual <= ((double)pctLimit*value)/100);
}

void PrintMediaType(AM_MEDIA_TYPE* pMediaType)
{
    // Major Type
    if (pMediaType->majortype == GUID_NULL)
        LOG(TEXT("Major: NULL\n"));
    else if (pMediaType->majortype == MEDIATYPE_Stream)
        LOG(TEXT("Major: MEDIATYPE_Stream\n"));
    else if (pMediaType->majortype == MEDIATYPE_Audio)
        LOG(TEXT("Major: MEDIATYPE_Audio\n"));
    else if (pMediaType->majortype == MEDIATYPE_Video)
        LOG(TEXT("Major: MEDIATYPE_Video\n"));
    else LOG(TEXT("Major: Unknown \n"));

    // Minor Type
    if (pMediaType->subtype == GUID_NULL)
        LOG(TEXT("Minor: NULL\n"));

    // Audio Subtypes
    else if (pMediaType->subtype == MEDIASUBTYPE_PCM)
        LOG(TEXT("Minor: MEDIASUBTYPE_PCM\n"));

    // MPEG-1 Subtypes
    else if (pMediaType->subtype == MEDIASUBTYPE_MPEG1Packet)
        LOG(TEXT("Minor: MEDIASUBTYPE_MPEG1Packet\n"));
    else if (pMediaType->subtype == MEDIASUBTYPE_MPEG1Payload)
        LOG(TEXT("Minor: MEDIASUBTYPE_MPEG1Payload\n"));
    else if (pMediaType->subtype == MEDIASUBTYPE_MPEG1System)
        LOG(TEXT("Minor: MEDIASUBTYPE_MPEG1System\n"));
    else if (pMediaType->subtype == MEDIASUBTYPE_MPEG1Video)
        LOG(TEXT("Minor: MEDIASUBTYPE_MPEG1Video\n"));
    else if (pMediaType->subtype == MEDIASUBTYPE_MPEG1Audio)
        LOG(TEXT("Minor: MEDIASUBTYPE_MPEG1Audio\n"));

    // Stream Subtypes (not including MPEG1 types)
    else if (pMediaType->subtype == MEDIASUBTYPE_AIFF)
        LOG(TEXT("Minor: MEDIASUBTYPE_AIFF\n"));
    else if (pMediaType->subtype == MEDIASUBTYPE_AU)
        LOG(TEXT("Minor: MEDIASUBTYPE_AU\n"));
    else if (pMediaType->subtype == MEDIASUBTYPE_Avi)
        LOG(TEXT("Minor: MEDIASUBTYPE_Avi\n"));
    else if (pMediaType->subtype == MEDIASUBTYPE_Asf)
        LOG(TEXT("Minor: MEDIASUBTYPE_Asf\n"));
    else if (pMediaType->subtype == MEDIASUBTYPE_AIFF)
        LOG(TEXT("Minor: MEDIASUBTYPE_AIFF\n"));
    else if (pMediaType->subtype == MEDIASUBTYPE_MPEG1VideoCD)
        LOG(TEXT("Minor: MEDIASUBTYPE_MPEG1VideoCD\n"));
    else if (pMediaType->subtype == MEDIASUBTYPE_WAVE)
        LOG(TEXT("Minor: MEDIASUBTYPE_WAVE\n"));

    // Video Subtypes
    // RGB
    else if (pMediaType->subtype == MEDIASUBTYPE_RGB1)
        LOG(TEXT("Minor: MEDIASUBTYPE_RGB1\n"));
    else if (pMediaType->subtype == MEDIASUBTYPE_RGB4)
        LOG(TEXT("Minor: MEDIASUBTYPE_RGB4\n"));
    else if (pMediaType->subtype == MEDIASUBTYPE_RGB8)
        LOG(TEXT("Minor: MEDIASUBTYPE_RGB8\n"));
    else if (pMediaType->subtype == MEDIASUBTYPE_RGB565)
        LOG(TEXT("Minor: MEDIASUBTYPE_RGB565\n"));
    else if (pMediaType->subtype == MEDIASUBTYPE_RGB555)
        LOG(TEXT("Minor: MEDIASUBTYPE_RGB555\n"));
    else if (pMediaType->subtype == MEDIASUBTYPE_RGB24)
        LOG(TEXT("Minor: MEDIASUBTYPE_RGB24 for sure\n"));
    else if (pMediaType->subtype == MEDIASUBTYPE_RGB32)
        LOG(TEXT("Minor: MEDIASUBTYPE_RGB32\n"));
    
    // YUV
    else if (pMediaType->subtype == MEDIASUBTYPE_UYVY)
        LOG(TEXT("Minor: MEDIASUBTYPE_UYVY\n"));
    else if (pMediaType->subtype == MEDIASUBTYPE_Y411)
        LOG(TEXT("Minor: MEDIASUBTYPE_Y411\n"));
    else if (pMediaType->subtype == MEDIASUBTYPE_Y41P)
        LOG(TEXT("Minor: MEDIASUBTYPE_Y41P\n"));
    else if (pMediaType->subtype == MEDIASUBTYPE_Y211)
        LOG(TEXT("Minor: MEDIASUBTYPE_Y211\n"));
    else if (pMediaType->subtype == MEDIASUBTYPE_YVYU)
        LOG(TEXT("Minor: MEDIASUBTYPE_YVYU\n"));
    else if (pMediaType->subtype == MEDIASUBTYPE_YUY2)
        LOG(TEXT("Minor: MEDIASUBTYPE_YUY2\n"));
    else LOG(TEXT("SubType: Unknown \n"));
}

#ifdef UNDER_CE
// BUBUG: include the right interface
#include <initguid.h>
DEFINE_GUID( IID_IDirectDrawSurface, 0x6C14DB81,0xA733,0x11CE,0xA5,0x21,0x00,0x20,0xAF,0x0B,0xE5,0x60 );
#endif

bool IsDDrawSample(IMediaSample *pSample)
{
    HRESULT hr = S_OK;
    IDirectDrawMediaSample* pDDSample = NULL;
    
    // Try gettting the IDirectDrawMediaSample interface
    hr = pSample->QueryInterface(IID_IDirectDrawMediaSample, (void **)&pDDSample);
    if (SUCCEEDED(hr)) 
    {
        pDDSample->Release();
        return true;
    }
    
    // If that didn;t work try gettting the IDirectDrawSurface interface
    IDirectDrawSurface* pSurface = NULL;
    hr = pSample->QueryInterface(IID_IDirectDrawSurface, (void **)&pSurface);
    if (SUCCEEDED(hr)) 
    {
        pSurface->Release();
        return true;
    }
    
    return false;
}


HRESULT GetBackBuffers(IMediaSample *pSample, DWORD* pBackBuffers)
{
    HRESULT hr = S_OK;

	if (!pBackBuffers)
		return E_INVALIDARG;

    // Try getting the IDirectDrawSurface interface
    IDirectDrawSurface* pSurface = NULL;
    DDSURFACEDESC ddsd;
    hr = pSample->QueryInterface(IID_IDirectDrawSurface, (void **)&pSurface);
    if (SUCCEEDED(hr)) 
    {
        ddsd.dwSize = sizeof(ddsd);
        // BUGBUG: when can we use the DDLOCK_WAITNOTBUSY flag? This is also version dependent.
        hr = pSurface->Lock(NULL, &ddsd, DDLOCK_WAITNOTBUSY, NULL);
        pSurface->Release();
    }

	if (SUCCEEDED(hr))
		*pBackBuffers = ddsd.dwBackBufferCount;
    
    return hr;
}

HRESULT LockSurface(IMediaSample *pSample, bool bLock)
{
    HRESULT hr = S_OK;
    IDirectDrawMediaSample* pDDSample = NULL;
    
    // Try gettting the IDirectDrawMediaSample interface
    hr = pSample->QueryInterface(IID_IDirectDrawMediaSample, (void **)&pDDSample);
    if (SUCCEEDED(hr)) 
    {
        if (bLock) {
            hr = pDDSample->LockMediaSamplePointer();
        } else {
            hr = pDDSample->GetSurfaceAndReleaseLock(NULL, NULL);
        }
        pDDSample->Release();
        return hr;
    }
    
    // If that didn;t work try gettting the IDirectDrawSurface interface
    IDirectDrawSurface* pSurface = NULL;
    hr = pSample->QueryInterface(IID_IDirectDrawSurface, (void **)&pSurface);
    if (SUCCEEDED(hr)) 
    {
        if (bLock) 
        {
            DDSURFACEDESC ddsd;
            ddsd.dwSize = sizeof(ddsd);
            // BUGBUG: when can we use the DDLOCK_WAITNOTBUSY flag? This is also version dependent.
            hr = pSurface->Lock(NULL, &ddsd, DDLOCK_WAITNOTBUSY, NULL);
            if (FAILED(hr))
                LOG(TEXT("Failed to lock surface (0x%x)"), hr);
        }
        else {
            hr = pSurface->Unlock(NULL);
        }
        pSurface->Release();
        return hr;
    }
    
    return E_NOINTERFACE;
}

void InvertBitmap(BYTE* pBits, int width, int height, DWORD bitcount)
{
    switch(bitcount)
    {
    case 24:
        for(int x = 0; x < width; x++)
        for(int y = 0; y < height/2; y++)
        {
            // Now swap heightwise
            BYTE* ptr = pBits + y*width*3 + x*3;
            BYTE* otherptr = pBits + (height - y - 1)*width*3 + x*3;
            SWAPBYTE(*ptr, *otherptr);
            SWAPBYTE(*(ptr + 1), *(otherptr + 1));
            SWAPBYTE(*(ptr + 2), *(otherptr + 2));
        }

        break;
    
    case 32:
        for(int x = 0; x < width; x++)
        for(int y = 0; y < height/2; y++)
        {
            // Now swap heightwise
            BYTE* ptr = pBits + y*width*4 + x*4;
            BYTE* otherptr = pBits + (height - y - 1)*width*4 + x*4;
            SWAPBYTE(*ptr, *otherptr);
            SWAPBYTE(*(ptr + 1), *(otherptr + 1));
            SWAPBYTE(*(ptr + 2), *(otherptr + 2));
        }
        break;
    }
}

void SwapRedGreen(BYTE* pBits, int width, int height, DWORD bitcount)
{
    switch(bitcount)
    {
    case 24:
        for(int y = 0; y < height; y++)
        for(int x = 0; x < width; x++)
        {
            // swap red and green
            BYTE* rptr = pBits + y*width*3 + x*3;
            BYTE* bptr = rptr + 2;
            SWAPBYTE(*rptr, *bptr);
        }

        break;
    
    case 32:
        for(int y = 0; y < height; y++)
        for(int x = 0; x < width; x++)
        {
            // swap red and green
            BYTE* rptr = pBits + y*width*4 + x*4;
            BYTE* bptr = rptr + 2;
            SWAPBYTE(*rptr, *bptr);
        }
        break;
    }
}

HRESULT GetScreenResolution(DWORD* pWidth, DWORD* pHeight)
{
    DEVMODE devmode;
    memset(&devmode, 0, sizeof(devmode));
    devmode.dmSize = sizeof(devmode);
    BOOL result = ::EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &devmode);

    if (!result)
        return E_FAIL;

    *pWidth = devmode.dmPelsWidth;
    *pHeight = devmode.dmPelsHeight;

    return S_OK;
}

const TCHAR* GetStateName(FILTER_STATE state)
{
    switch(state)
    {
    case State_Stopped:
        return TEXT("Stopped");
        break;
    case State_Paused:
        return TEXT("Paused");
        break;
    case State_Running:
        return TEXT("Running");
        break;
    }
    return NULL;
}

////////////////////////////////////////////////////////////////////////////////
// NearlyInfinite
// Never use INFINITE, use this instead.  This will prevent lock ups.
//
// Return value:
// long - the calculated amount of time in milliseconds
//
long NearlyInfinite( IMediaPosition *pMediaPosition )
{
    REFTIME rtDuration;
    double  Rate;

    if ( FAILED( pMediaPosition->get_Duration(&rtDuration) ) )
		return 0xffffffff; // -1
    if ( FAILED( pMediaPosition->get_Rate(&Rate) ) )
		return 0xffffffff;

	//the longer of 10 seconds or twice the rate adjusted duration.
    return max( (long)( (rtDuration*1000*2)/Rate ), 10000L );
}

