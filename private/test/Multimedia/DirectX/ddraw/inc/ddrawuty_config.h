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


#ifndef __DDRAWUTY_CONFIG_H__
#define __DDRAWUTY_CONFIG_H__

// External Dependencies
// ---------------------
#include "main.h"

#include <vector>
#include <algorithm>
#include <QATestUty/LocaleUty.h>
#include <QATestUty/ConfigUty.h>
#include <string>

namespace DDrawUty
{
    // Typedef for a Wide Character string.
    typedef std::basic_string<TCHAR>         wstring;

    // Different Cooperative Levels
    enum CfgCooperativeLevel
    {
        clNone = 0,

        clFullScreenExclusive,
        clNormal,

        clCount
    };

    struct DISPLAYMODE
    {
        // Constructors to allow us to build map statically
        DISPLAYMODE()
          : m_dwWidth(0),
            m_dwHeight(0),
            m_dwDepth(0),
            m_dwFrequency(0),
            m_dwFlags(0),
            m_pszName(NULL)
        { }
        DISPLAYMODE(DWORD dwWidth, DWORD dwHeight, DWORD dwDepth, DWORD dwFrequency, BOOL fStandardVGA, LPCTSTR pszName)
          : m_dwWidth(dwWidth),
            m_dwHeight(dwHeight),
            m_dwDepth(dwDepth),
            m_dwFrequency(dwFrequency),
            m_dwFlags(fStandardVGA),
            m_pszName(pszName)
        { }
        DWORD   m_dwWidth,
                m_dwHeight,
                m_dwDepth,
                m_dwFrequency;
        BOOL    m_dwFlags;
        LPCTSTR m_pszName;
    };

    // Different Surface Types
    enum CfgSurfaceType
    {
        stNone = 0,

        stPrimary0Back,
        stPrimary1Back,
        stPrimary2Back,
        stPrimary3Back,
        stPrimary0BackSys,
        stPrimary1BackSys,
        stPrimary2BackSys,
        stPrimary3BackSys,
//        stDCGetDCNull,
//        stDCGetDCHwnd,
//        stDCCreateDC,
//        stDCCompatBmp,
//        stBackbuffer,
        stOffScrVid,
        stOffScrSys,
        stOffScrSysOwnDc,
        stOverlayVid0Back,
        stOverlayVid1Back,
        stOverlayVid2Back,
        stOverlaySys0Back,
        stOverlaySys1Back,
        stOverlaySys2Back,

        stCount
    };

    // Different Pixel Formats
    enum CfgPixelFormat
    {
        pfNone = 0,

        // 8 bits - paletted
        pfPal8,
        // 16 bits - no Alpha
        pfRGB555,
        pfRGB565,
        pfBGR565,
        // 16 bits - Alpha
        pfARGB1555,
        pfARGB4444,
        pfARGB1555pm,
        pfARGB4444pm,
        // 24 bits
        pfRGB888,
        pfBGR888,
        // 32 bits - no Alpha
        pfRGB0888,
        pfBGR0888,
        // 32 bits - Alpha
        pfARGB8888,
        pfABGR8888,
        pfARGB8888pm,
        pfABGR8888pm,
        // YUV Formats
        pfYUVYUYV,
        pfYUVUYVY,
        pfYUVYV12,
        pfYUVNV12,
        pfYUVI420,
        pfYUVYUY2,

        pfCount
    };

    class CDirectDrawConfiguration
    {
    public:
        // type defines
        // ------------
        typedef std::vector<CfgCooperativeLevel> VectCooperativeLevel;
        typedef std::vector<DISPLAYMODE>         VectDisplayMode;
        typedef std::vector<CfgSurfaceType>      VectSurfaceType;
        typedef std::vector<CfgPixelFormat>      VectPixelFormat;
        typedef std::map<TCHAR*, DWORD>          MapSymbolTable;
        typedef std::map<wstring, DWORD>         MapStringTable;

        // Default Surface Sizes
        // ---------------------
        static int   m_nDefaultHeight,
                     m_nDefaultWidth;

        // Constructor/Destructor
        // ----------------------
        CDirectDrawConfiguration();
        virtual ~CDirectDrawConfiguration();

        // Configuration Methods
        // ---------------------
        bool ConfigFromCapsAndCoop(void);
        bool ConfigFromDDraw(IDirectDraw *pDD, DWORD dwCooperativeLevel);
        bool ConfigFromDDSurface(IDirectDrawSurface *pDDS);
        bool ConfigFromFile(LPCTSTR pszConfigFile);

        // Symbol Table Accessors
        // ----------------------
        HRESULT SetSymbol(TCHAR *szSymbolName, DWORD dwSymbolValue);
        HRESULT GetSymbol(TCHAR *szSymbolName, DWORD *pdwSymbolValue);
        HRESULT SetStringValue(const wstring& strName, DWORD dwValue);
        HRESULT GetStringValue(const wstring& strName, DWORD *pdwValue);        

        // Supported Cooperative Levels
        // ----------------------------
        VectCooperativeLevel m_vclSupported;
        bool IsSupportedCooperativeLevel(CfgCooperativeLevel ccl);
        HRESULT GetCooperativeLevelData(CfgCooperativeLevel ccl, DWORD &cooperativelevel, TCHAR *pszDesc, int nCount);

        // Supported Display Modes
        // -----------------------
        static HRESULT WINAPI EnumModesCallback(LPDDSURFACEDESC lpDDSurfaceDesc, LPVOID lpContext);
        bool IsSupportedDisplayMode(DISPLAYMODE dmReq);
        VectDisplayMode m_vdmSupported;

        // Supported Surface Formats
        // -------------------------
        VectSurfaceType m_vstSupportedPrimary;
        VectSurfaceType m_vstSupportedDC;
        VectSurfaceType m_vstSupportedVidMem;
        VectSurfaceType m_vstSupportedSysMem;
        bool IsSupportedSurfaceType(CfgSurfaceType cst);
        HRESULT GetSurfaceDescData(CfgSurfaceType cst, DDSURFACEDESC &ddsd, TCHAR *pszDesc, int nCount);

        // Supported Pixel Formats
        // -----------------------
        VectPixelFormat m_vpfSupportedVidMem;
        VectPixelFormat m_vpfSupportedSysMem;
        VectPixelFormat m_vpfSupportedDC;
        bool IsSupportedPixelFormat(CfgPixelFormat cpf);
        HRESULT GetPixelFormatData(CfgPixelFormat cpf, DDPIXELFORMAT &ddpf,  TCHAR *pszDesc, int nCount);

        // Surface Querying Functions
        // --------------------------
        bool IsPrimarySurfaceType(CfgSurfaceType cst);

        // Surface/Pixel combinations
        // --------------------------
        bool IsValidSurfacePixelCombo(CfgSurfaceType cst, CfgPixelFormat cpf);
        HRESULT GetSurfaceDesc(CfgSurfaceType cst, CfgPixelFormat cpf, DDSURFACEDESC &ddsd, TCHAR *pszDesc, int nCount, bool bForceValid = true);

        // CAPS Accessing Functions
        // ------------------------
        DDCAPS &HALCaps() { return m_ddcapsHAL; }
        DWORD CooperativeLevel() { return m_dwCooperativeLevel; }

        // The driver name
        // ---------------
        bool SetDriverName(const TCHAR *tcIn);
        bool GetDriverName(TCHAR *tcIn, DWORD*dwSize);

        // The GUID used in DirectDrawCreate
        // ---------------------------------
        bool SetDeviceGUID(const GUID & guid);
        bool GetDeviceGUID(GUID & guid);

        // Verify that the Compositor is enabled.
        // ---------------------------------
        bool IsCompositorEnabled(){ return m_bIsCompositorEnabled; }

    private:
        // Supported Cooperative Levels
        // ----------------------------
        DWORD m_rgCooperativeLevel[clCount];
        TCHAR * m_rgszCooperativeLevelDesc[clCount];
        void InitCooperativeLevels(void);

        // Driver Name
        //
        TCHAR m_szDriverName[256];
        bool m_bDriverNameValid;

        // The GUID of the device we're on.
        GUID m_guidDevice;
        bool m_bDeviceGUIDValid;

        // Power Management Handle
        HANDLE m_rghPowerManagement[5];

        // Default Surface Descriptions
        // ----------------------------
        DDSURFACEDESC m_ddsdPrimary,
                      m_ddsdOffScr,
                      m_ddsdOverlay;
        CfgPixelFormat m_pfPrimary;
        TCHAR * m_rgszSurfaceTypeDesc[stCount];
        void InitSurfaceDescriptions(void);

        // Supported Pixel Formats
        // -----------------------
        DDPIXELFORMAT m_rgPixelFormat[pfCount];
        TCHAR * m_rgszPixelFormatDesc[pfCount];
        void InitPixelFormats(void);

        // PixelFormat ID from PixelFormat Structure
        // -----------------------------------------
        CfgPixelFormat GetPixelFormatIDFromStruct(DDPIXELFORMAT& ddpf);

        // Symbol Table
        // ------------
        MapSymbolTable m_mapSymbols;
        MapStringTable m_mapStrings;

        // CAPS Data Structures
        // --------------------
        DDCAPS m_ddcapsHAL;   // Drivare caps
        DWORD m_dwCooperativeLevel;
        std::vector<DWORD> m_vdwFourCCCodes;

        // Configuration Object
        ConfigUty::CConfig m_cVid;
        ConfigUty::CConfig m_cSys;
        ConfigUty::CConfig m_cOverlayVid;
        ConfigUty::CConfig m_cOverlaySys;

        // bool to check if the Compositor is enabled or not.
        bool m_bIsCompositorEnabled;
    };

    // ======================
    // inline implementations
    // ======================
    inline bool CDirectDrawConfiguration::IsSupportedCooperativeLevel(CfgCooperativeLevel ccl)
    {
        return m_vclSupported.end()
               != std::find(m_vclSupported.begin(), m_vclSupported.end(), ccl);
    }

    inline bool CDirectDrawConfiguration::IsSupportedSurfaceType(CfgSurfaceType cst)
    {
        return (m_vstSupportedPrimary.end() != std::find(m_vstSupportedPrimary.begin(), m_vstSupportedPrimary.end(), cst) ||
                m_vstSupportedVidMem.end()  != std::find(m_vstSupportedVidMem.begin(),  m_vstSupportedVidMem.end(),  cst) ||
                m_vstSupportedSysMem.end()  != std::find(m_vstSupportedSysMem.begin(),  m_vstSupportedSysMem.end(),  cst) ||
                m_vstSupportedDC.end()  != std::find(m_vstSupportedDC.begin(),  m_vstSupportedDC.end(),  cst));
    }

    inline bool CDirectDrawConfiguration::IsSupportedPixelFormat(CfgPixelFormat cpf)
    {
        return (m_vpfSupportedVidMem.end() != std::find(m_vpfSupportedVidMem.begin(), m_vpfSupportedVidMem.end(), cpf) ||
                m_vpfSupportedSysMem.end() != std::find(m_vpfSupportedSysMem.begin(), m_vpfSupportedSysMem.end(), cpf) ||
                m_vpfSupportedDC.end() != std::find(m_vpfSupportedDC.begin(), m_vpfSupportedDC.end(), cpf));
    }

    inline bool CDirectDrawConfiguration::IsPrimarySurfaceType(CfgSurfaceType cst)
    {
        return ((stPrimary0Back <= cst) && (cst <= stPrimary3BackSys));
    }

    extern CDirectDrawConfiguration g_DDConfig;
}

#endif

