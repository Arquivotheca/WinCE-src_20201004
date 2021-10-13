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

#ifndef __DDRAWUTY_MAPS_H__
#define __DDRAWUTY_MAPS_H__

// External dependencies
#include <const_map.h>
#include <vector>
#include <string>
//#include <QATestUty/DebugUty.h>

namespace DDrawUty
{
    // ==============
    // rop_string_map
    // ==============
    class rop_string_map 
    {
    public:
        typedef std::pair<DWORD, LPCTSTR> value_type;
        typedef std::vector<value_type>::const_iterator const_iterator;
    
        // constructor : initialization is like std::vector
        template<class _Iter>
        rop_string_map(_Iter first, _Iter last) 
            : m_vMap(first, last) {}           
    
        // returns a std::tstring appropriate to the given
        // ROP array.
        // example usage:
        //  printf("dsFlags = %s\n", g_SomeFlagMap[dwFlags].c_str());
        std::tstring operator[] (DWORD dwROP) const;

    private:
        std::vector<value_type> m_vMap;
    };

    // ==================================
    // DDrawUty related IID-->string maps
    // ==================================
    extern const std::static_map<IID, LPCTSTR> g_IIDMap;
    
    // ====================================
    // DDrawUty related value-->string maps
    // ====================================
    extern const std::const_map<HRESULT, LPCTSTR> g_ErrorMap;
    extern const std::const_map<HRESULT, LPCTSTR> g_ErrorMsgMap;

    // ========================================
    // value-->string maps for flags to Methods
    // ========================================
    extern const flag_string_map g_AlphaBltFlagMap;
    extern const flag_string_map g_BltFlagMap;
    extern const flag_string_map g_BltFastFlagMap;
    extern const flag_string_map g_GetSetColorKeyFlagMap;

    // ===========================================
    // value-->string maps for flags in Structures
    // ===========================================
    extern const flag_string_map g_BltBatchFlagMap;
    extern const flag_string_map g_BltFXFlagMap;
    extern const flag_string_map g_CapsCapsFlagMap;
    extern const flag_string_map g_CapsBltCapsFlagMap;
    extern const flag_string_map g_CapsCKeyCapsFlagMap;
    extern const flag_string_map g_CapsPalCapsFlagMap;
    extern const flag_string_map g_CapsAlphaCapsFlagMap;
    extern const flag_string_map g_CapsMiscCapsFlagMap;
    extern const flag_string_map g_ColorControlFlagMap;
    extern const flag_string_map g_PixelFormatFlagMap;
    extern const flag_string_map g_SCapsCapsFlagMap;
    extern const flag_string_map g_SurfaceDescFlagMap;
    extern const flag_string_map g_VideoPortBandwidthCapsMap;
    extern const flag_string_map g_VideoPortCapsFlagMap;
    extern const flag_string_map g_VideoPortCapsCapsMap;
    extern const flag_string_map g_VideoPortCapsFXMap;
    extern const flag_string_map g_VideoPortConnectFlagMap;
    extern const flag_string_map g_VideoPortInfoFlagMap;
    extern const flag_string_map g_VideoPortStatusFlagMap;
    extern const flag_string_map g_DevModeFieldsMap;

    extern const rop_string_map g_RopMap;
}

#endif

