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
#pragma once
#ifndef __DDRAWUTY_SINGLETONS_H__
#define __DDRAWUTY_SINGLETONS_H__

// External dependencies
#include <com_utilities.h>
#include <DDrawUty_Types.h>
#include "Singleton.h"

namespace DDrawUty
{
    // ====================
    // CDirectDrawSingleton
    // ====================
    class CDirectDrawSingleton : public singleton< com_utilities::CComPointer<IDirectDraw> >
    {
    public:
        typedef singleton< com_utilities::CComPointer<IDirectDraw> > base_class;
        typedef com_utilities::CComPointer<IDirectDrawSurface> primary_surface_type;

        CDirectDrawSingleton();
        virtual ~CDirectDrawSingleton();

        virtual bool SetCooperativeLevel(DWORD dwCooperativeLevel);
        
        // State members
        // -------------
        GUID * lpGUID;                  // default = NULL
        
        bool m_fSetCooperativeLevel;    // default = true
        DWORD m_dwCooperativeLevel;     // default = DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN
        HWND m_hWnd;
        HINSTANCE m_hInstDDraw;
        
        inline HRESULT GetLastError(void) { return(m_hr); }
    private:
        struct EnumDisplayContext
        {
            int      iDisplayIndex;
            bool     fSuccess;
            GUID     guid;
            HMONITOR hm;
            TCHAR    tszDriverName[MAX_PATH];
        };
        virtual bool CreateObject();
        virtual void DestroyObject();
        static HRESULT WINAPI EnumCallback_ChooseDisplay(
            GUID * lpGUID, 
            LPTSTR tszDescription, 
            LPTSTR tszName,
            LPVOID pvContext,
            HMONITOR hm);
        RECT rcWindowPos;
        HRESULT m_hr;                   // for storage of the last error code
        
    };

    // ============
    // g_DirectDraw
    // ============
    extern CDirectDrawSingleton g_DirectDraw;


    // ==================
    // CDirectDrawSurface
    // ==================
    class CDirectDrawSurface : public singleton< com_utilities::CComPointer<IDirectDrawSurface> >
    {
    public:
        typedef singleton< com_utilities::CComPointer<IDirectDrawSurface> > base_class;

        CDirectDrawSurface();
        
        virtual ~CDirectDrawSurface();

        // Description of the desired surface.  Users should
        // set up this structure before getting the interface.
        CDDSurfaceDesc m_cddsd;

	inline HRESULT GetLastError(void) { return (m_hr); }
        
    protected:
        virtual bool CreateObject();
        virtual void DestroyObject();
        HRESULT m_hr;						// for storage of the last error code
    };

    
    // =========================
    // CDirectDrawSurfacePrimary
    // =========================
    class CDirectDrawSurfacePrimary : public CDirectDrawSurface
    {
    public:
        CDirectDrawSurfacePrimary();
        
    protected:
        virtual bool CreateObject();
    };

    // ============
    // g_DDSPrimary
    // ============
    extern CDirectDrawSurfacePrimary g_DDSPrimary;


    // ============================
    // CDirectDrawSurfaceBackbuffer
    // ============================
    class CDirectDrawSurfaceBackbuffer : public CDirectDrawSurface
    {
    protected:
        virtual bool CreateObject();
        virtual void DestroyObject();
    };

    // ===============
    // g_DDSBackbuffer
    // ===============
    extern CDirectDrawSurfaceBackbuffer g_DDSBackbuffer;

    // ====================
    // CDirectDrawSurfaceDC
    // ====================
    class CDirectDrawSurfaceDC : public CDirectDrawSurface
    {
    public:
        // The surface type tells us which type of DC to use when creating the DC.
        CfgSurfaceType m_stType;

        // Constructor is needed to clear the surface type
        CDirectDrawSurfaceDC();
    protected:
        virtual bool CreateObject();
        virtual void DestroyObject();
        HWND        m_hwnd; // The window that the DC is from.
        HDC         m_hdc;  // The DC that the surface is based on.
        HBITMAP     m_hbmp; // The bitmap associated with the DC (the stock bitmap).
    };

    // =======
    // g_DDSDC
    // =======
    extern CDirectDrawSurfaceDC g_DDSDC;

}

#endif
