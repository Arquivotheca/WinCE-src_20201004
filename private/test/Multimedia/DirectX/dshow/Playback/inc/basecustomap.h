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
#ifndef _VMR_CUSTOM_AP_H
#define _VMR_CUSTOM_AP_H

#ifdef UNDER_CE
#include <streams.h>
#include <atlbase.h>
#include <dshow.h>
#include "videostamp.h"
#include "VideoUtil.h"
#include "alloclib.h"

const DWORD_PTR DW_COMPOSITED_ID = 0xDEADBEEF;
const DWORD_PTR DW_STREAM1_ID = DW_COMPOSITED_ID + 1;
const DWORD_PTR DW_STREAM2_ID = DW_COMPOSITED_ID + 2;


typedef HRESULT (*CustomAPCallback)( IDirectDrawSurface *pSurface, void *pUserData );

// 
// SetBorderColor is in both the surfaceallocatorNotify & WindowlessControl,
// so we need a separate class...
//
class CCustomWindowless : public CUnknown,
                          public IVMRWindowlessControl
{
public:
    CCustomWindowless();
    ~CCustomWindowless();

    void Deactivate(void);

    DECLARE_IUNKNOWN
    STDMETHODIMP NonDelegatingQueryInterface(REFIID, void**);

    // IVMRWindowlessControl
    STDMETHODIMP GetNativeVideoSize(LONG* lWidth, LONG* lHeight,
                                    LONG* lARWidth, LONG* lARHeight);
    STDMETHODIMP GetMinIdealVideoSize(LONG* lWidth, LONG* lHeight);
    STDMETHODIMP GetMaxIdealVideoSize(LONG* lWidth, LONG* lHeight);
    STDMETHODIMP SetVideoPosition(const LPRECT lpSRCRect, const LPRECT lpDSTRect);
    STDMETHODIMP GetVideoPosition(LPRECT lpSRCRect,LPRECT lpDSTRect);
    STDMETHODIMP GetAspectRatioMode(DWORD* lpAspectRatioMode);
    STDMETHODIMP SetAspectRatioMode(DWORD AspectRatioMode);

    STDMETHODIMP SetVideoClippingWindow(HWND hwnd);
    STDMETHODIMP RepaintVideo(HWND hwnd, HDC hdc);
    STDMETHODIMP DisplayModeChanged();
    STDMETHODIMP GetCurrentImage(BYTE** lpDib);

    STDMETHODIMP SetBorderColor(COLORREF Clr);
    STDMETHODIMP GetBorderColor(COLORREF* lpClr);
    STDMETHODIMP SetColorKey(COLORREF Clr);
    STDMETHODIMP GetColorKey(COLORREF* lpClr);

    // helpers
    void SetWindowless(CComPtr<IVMRWindowlessControl> pWindowlessControl);

private:
    CComPtr<IVMRWindowlessControl> m_pDefaultWindowlessControl;
};

struct VMRStreamData
{
    DWORD_PTR    vmrId;
    FLOAT        fAlpha;
    INT32        iZorder;
    NORMALIZEDRECT    nrRect;
    DWORD        dwWidth;
    DWORD        dwHeight;        
    VMRStreamData()
    {
        vmrId = 0;
        dwWidth = dwHeight = 0;
        fAlpha = -1.0;
        iZorder = -1;
        nrRect.left = -1.0f;
        nrRect.top = -1.0f;
        nrRect.right = -1.0f;
        nrRect.bottom = -1.0f;
    }
};


class CBaseCustomAP : public CUnknown,
                  public IVMRSurfaceAllocator,
                  public IVMRImagePresenter,
                  public IVMRSurfaceAllocatorNotify,
                  public IVMRImagePresenterConfig
{
public:
    CBaseCustomAP( HRESULT *phr );
    virtual ~CBaseCustomAP();

    virtual HRESULT Activate( CComPtr<IBaseFilter> pVMR, CustomAPCallback pCallbackFn, VMRStreamData *pData );
    virtual void Deactivate();

    DECLARE_IUNKNOWN
    STDMETHODIMP NonDelegatingQueryInterface(REFIID, void**);

    // IVMRSurfaceAllocator
    STDMETHODIMP AllocateSurface(DWORD_PTR dwUserID,
                                VMRALLOCATIONINFO* lpAllocInfo,
                                 DWORD* lpdwActualBackBuffers,
                                 LPDIRECTDRAWSURFACE* lplpSurface);
    STDMETHODIMP FreeSurface(DWORD_PTR dwUserID);
    STDMETHODIMP PrepareSurface(DWORD_PTR dwUserID, LPDIRECTDRAWSURFACE lplpSurface, DWORD dwSurfaceFlags);
    STDMETHODIMP AdviseNotify(IVMRSurfaceAllocatorNotify* lpIVMRSurfAllocNotify);

    // IVMRSurfaceAllocatorNotify
    STDMETHODIMP AdviseSurfaceAllocator(DWORD_PTR dwUserID, IVMRSurfaceAllocator* lpIVRMSurfaceAllocator);
    STDMETHODIMP SetDDrawDevice(LPDIRECTDRAW lpDDrawDevice,HMONITOR hMonitor);
    STDMETHODIMP ChangeDDrawDevice(LPDIRECTDRAW lpDDrawDevice,HMONITOR hMonitor);
    STDMETHODIMP RestoreDDrawSurfaces();
    STDMETHODIMP NotifyEvent(LONG EventCode, LONG_PTR lp1, LONG_PTR lp2);
    STDMETHODIMP SetBorderColor(COLORREF Clr);

    // IVMRImagePresenter
    STDMETHODIMP StartPresenting(DWORD_PTR dwUserID);
    STDMETHODIMP StopPresenting(DWORD_PTR dwUserID);
    STDMETHODIMP PresentImage(DWORD_PTR dwUserID, VMRPRESENTATIONINFO* lpPresInfo);

    // IVMRImagePresenterConfig
    STDMETHODIMP SetRenderingPrefs(DWORD dwPrefs);
    STDMETHODIMP GetRenderingPrefs(DWORD *pdwPrefs);

    // Update stream data;
    VOID UpdateStreamData( DWORD_PTR vmrId, VMRStreamData *pData );

    VOID SetCompositedSize( DWORD dwWidth, DWORD dwHeight );
    DWORD  CompareSavedImages(TCHAR *tszSrcImagesPath );
    HRESULT InitializeMediaType(float scale, DWORD angle);

    // Set the verification sampling frequency
    VOID VerifyStream( BOOL bVerify ) { m_bVerifyStream = bVerify; }
    VOID SetWndHandle( HWND   hwndApp ) { m_hwndApp = hwndApp; }
    VOID SaveImage( BOOL bSave ) { m_bImageSavingMode= bSave; }
    VOID CaptureImage( BOOL bCapture ) { m_bCaptureMode= bCapture; }
    VOID SetImagePath( TCHAR *pszPath ) { m_szPath = pszPath; }
    VOID SetRotation( DWORD dwRotation ) { m_dwRotation= dwRotation; }
    VOID SetVerificationRate( DWORD dwRate ) { m_dwVerificationRate = dwRate; }
    VOID SetDiffTolerance( float fDiffTolerance ) { m_fDiffTolerance = fDiffTolerance; }
    DWORD GetMatchCount() { return m_dwMatchCount;}
    BOOL IsUsingOverlay() { return m_bUsingOverlay;}
    static DWORD WINAPI VerifyThreadProc(LPVOID lpParameter);

protected:
    // customized operation on the image before and after IVMRImagePresenter::PresentImage
    virtual HRESULT PreProcessImage( DWORD_PTR dwUserID, VMRPRESENTATIONINFO* lpPresInfo );
    virtual HRESULT PostProcessImage( DWORD_PTR dwUserID, VMRPRESENTATIONINFO* lpPresInfo );

    // GDI functions to construct a composited image
    virtual BOOL InitCompositedDC() { return TRUE; }

    // Blending verification methods
    virtual DWORD VerifyThread();
    HRESULT CompareSavedImageWithStream();
    HRESULT SaveImage(DWORD dwFrameID);
    HRESULT LoadSavedImage(TCHAR *tszBitmapName, BYTE** ppImageBuffer,  BITMAPINFO *pImageBmi, GUID subtype);
    HRESULT SaveImageBuffer( DWORD_PTR dwUserID, IDirectDrawSurface *pSurface );
    HRESULT GetFrameID(DWORD* dwFrameID);
    HRESULT ProcessMediaSample(VMRPRESENTATIONINFO* lpPresInfo, DWORD_PTR dwUserID);
    HRESULT CheckOverlay(VMRPRESENTATIONINFO* lpPresInfo);
    
protected:
    // BUGBUG: the AP only holds one IVMRSurfaceAllocatorNotify.  We have multiple VMRs (multiply notifies)
    // BUGBUG: visit it later to use one single AP.
    CComPtr<IVMRSurfaceAllocator>       m_pDefaultSurfaceAllocator[3];
    CComPtr<IVMRImagePresenter>         m_pDefaultImagePresenter;
    CComPtr<IVMRImagePresenterConfig>   m_pDefaultImagePresenterConfig;
    CComPtr<IVMRWindowlessControl>      m_pDefaultWindowlessControl;
    CComPtr<IVMRSurfaceAllocatorNotify> m_pDefaultSurfaceAllocatorNotify[3];
    VMRStreamData                       m_StreamData[3];
    HWND                                m_hwndApp;
    LPDIRECTDRAW m_lpDDrawDevice;
    HMONITOR m_hMonitor;

    BOOL     m_bVerifyStream;
    BOOL     m_bImageSavingMode;
    BOOL     m_bMediaTypeChanged;
    BOOL     m_bCaptureMode;
    BOOL     m_bUsingOverlay;
    BOOL     m_bOverlaySupported;
    AM_MEDIA_TYPE m_mt;
    AM_MEDIA_TYPE *m_pMediaType;
    VideoStampReader m_Recognizer;
    BYTE    *m_pStream1;
    BYTE    *m_pStream2;
    BYTE    *m_pCompStream;
    BYTE    *m_pLoadedBitmap;
    TCHAR *m_szPath;
    DWORD m_dwMatchCount;
    DWORD m_dwRotation;
    DDSURFACEDESC m_ddsd;

    DWORD m_bActive;
    DWORD m_dwFrameNo;
    CCustomWindowless   m_CustomWindowless;

    CustomAPCallback    m_pfnCallback;
    void    *m_pUserData;
    float      m_fDiffTolerance;
    
    // control the verification thread
    DWORD            m_dwThreadID;
    HANDLE           m_hThread;
    HANDLE           m_hQuitEvent;

    CCritSec         m_csLock;

    // GDI parameters for using AlphaBlend.
    DWORD    m_dwVerificationRate;
    DWORD    m_dwCompWidth;
    DWORD    m_dwCompHeight;
    DWORD    m_dwBufferSize;
    int            m_iStride;
};

class CVMRRSCustomAP : public CBaseCustomAP
{
public:
    CVMRRSCustomAP( HRESULT *phr );
    virtual ~CVMRRSCustomAP();
private:
};

class CVMRE2ECustomAP : public CBaseCustomAP
{
public:
    CVMRE2ECustomAP( HRESULT *phr );
    virtual ~CVMRE2ECustomAP();

    VOID    RuntimeVerify( BOOL data ) { m_bRuntimeVerify = data; }
    BOOL    RuntimeVerify() { return m_bRuntimeVerify; }
protected:
    // overriden functions
    virtual HRESULT PreProcessImage( DWORD_PTR dwUserID, VMRPRESENTATIONINFO* lpPresInfo );
    virtual HRESULT PostProcessImage( DWORD_PTR dwUserID, VMRPRESENTATIONINFO* lpPresInfo );
    virtual BOOL InitCompositedDC();
    virtual DWORD VerifyThread();

private:
    BOOL IsFrameNumberMatched( DWORD_PTR dwUserID);
    HRESULT BlendStreamToCompImage( DWORD_PTR dwUserID );
    BOOL RepaintCompositedMemDC();
    HRESULT CompareImages();

private:
    BOOL    m_bRuntimeVerify;

    // Using GDI to construct a blended image with alpha information
    HBITMAP  m_hDIB;
    HBITMAP  m_hOldDIB;
    HDC      m_hdcComp;
    BYTE     *m_pDIBBits;
};

#endif

#endif
