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
#ifndef __DEBUGSTREAMUTY_H__
#define __DEBUGSTREAMUTY_H__

// external dependencies
#ifndef UNDER_CE
#include <tchar.h>
#endif
#include <kato.h>
#include <hqalog.h>
#include <string>
#include <vector>

// get rid of HQALOG's Debug macro aliasing
// ========================================
// IF YOU'RE GOING TO USE THE DEBUG STREAM
// UTILITY YOU CANNOT USE THE HQALOG.H MACROS
#undef Debug
#undef DebugV
#undef DebugBeginLevel
#undef DebugBeginLevelV
#undef DebugEndLevel
#undef DebugEndLevelV

#define BUFFERSIZE 1000

typedef KATOVERBOSITY eVerbosityLevel;

namespace DebugStreamUty
{

    class CWDebugStream
    {
    public:
        typedef CWDebugStream& (*manipulator)(CWDebugStream&);

        CWDebugStream(DWORD dwBufferSize = 2048, LPCTSTR pszStreamName = NULL, bool fUseOutputDebugStream=true);
        ~CWDebugStream();

        CWDebugStream& SetVerbosity(eVerbosityLevel v);
        CWDebugStream& operator () (eVerbosityLevel v);
        CWDebugStream& flush();
        CWDebugStream& endl();
        CWDebugStream& push_indent();
        CWDebugStream& pop_indent();
        
        // manipulator support
        // -------------------
        CWDebugStream& operator << (manipulator pfnManipulator);

        // primitives
        // ----------
        CWDebugStream& output_hex(unsigned long l);
        CWDebugStream& output_hex(unsigned short s);
        CWDebugStream& output_hex(unsigned int i);
        CWDebugStream& output_hex(unsigned char b);
        CWDebugStream& output_HEX(unsigned long l);
        CWDebugStream& output_HEX(unsigned short s);
        CWDebugStream& output_HEX(unsigned int i);
        CWDebugStream& output_HEX(unsigned char b);
        CWDebugStream& output_string(const wchar_t* psz);
        CWDebugStream& output_string(const char* psz);
        CWDebugStream& output_decimal(unsigned long l);
        CWDebugStream& output_decimal(long l);
        CWDebugStream& output_decimal(unsigned short i);
        CWDebugStream& output_decimal(unsigned int i);
        CWDebugStream& output_decimal(short i);
        CWDebugStream& output_decimal(int i);
        CWDebugStream& output_decimal(unsigned __int64 i);
        CWDebugStream& output_decimal(signed __int64 i);
        CWDebugStream& output_character(wchar_t c);
        CWDebugStream& output_character(char c);
        CWDebugStream& output_double(double d);
        CWDebugStream& output_pointer(void * pv);
        CWDebugStream& voutput_formatted(LPCTSTR pszFormat, va_list args);
        CWDebugStream& voutput_formatted(eVerbosityLevel v, LPCTSTR pszFormat, va_list args);
        CWDebugStream& output_formatted(LPCTSTR pszFormat, ...);
        CWDebugStream& output_formatted(eVerbosityLevel v, LPCTSTR pszFormat, ...);

        CWDebugStream& operator () (eVerbosityLevel v, LPCTSTR pszFormat, ...);

        CWDebugStream& BeginLevel(DWORD dwLevelID=NULL);
        CWDebugStream& EndLevel();

        bool UseKato(bool fEnable);
        inline bool UsingKato() { return m_fUseKato; }

        // Toggles
        // =======
        bool m_fUseOutputDebugString;  // enable/disable OutputDebugString (only used if not using kato)
        bool m_fPrefixFailures;        // turn on to prefix failures with ### (true by default)

        UINT GetCurrentKatoLevel();

        void SetPrefixText(LPCWSTR pszPrefixText);

    protected:
        enum eKatoLogState
        {
            klsLog, klsBeginLevel, klsEndLevel
        };

        bool m_fUseKato;
        eVerbosityLevel m_CurrentVerbosity;
        std::vector<TCHAR> m_szBuffer;
        TCHAR *m_pszPosition;
        LPCTSTR m_pszStreamName;
        int m_iIndention;
        DWORD m_dwBufferSize;
        eKatoLogState m_KatoLogState;
        UINT m_CurrentKatoLevel;
        LPCWSTR m_pszPrefixText;

        int AvailableBufferSpace();

        void _flush();
        void reset();
    };

    // ==============
    // Inline members
    // ==============
    inline CWDebugStream& CWDebugStream::operator () (eVerbosityLevel v)
    {
        return SetVerbosity(v);
    }

    inline CWDebugStream& CWDebugStream::endl()
    {
        *(m_pszPosition++) = TEXT('\n');
        *m_pszPosition=0;
        flush();
        return *this;
    }

    inline CWDebugStream& CWDebugStream::operator << (CWDebugStream::manipulator pfnManipulator)
    {
        return pfnManipulator(*this);
    }

    // Primitives
    // ----------
    inline CWDebugStream& CWDebugStream::output_hex(unsigned long l)
    {
        m_pszPosition+=_stprintf_s(m_pszPosition,BUFFERSIZE , TEXT("0x%08lx"), l);
        return *this;
    }

    inline CWDebugStream& CWDebugStream::output_hex(unsigned short s)
    {
        m_pszPosition+=_stprintf_s(m_pszPosition,BUFFERSIZE , TEXT("0x%04x"), s);
        return *this;
    }

    inline CWDebugStream& CWDebugStream::output_hex(unsigned int i)
    {
        m_pszPosition+=_stprintf_s(m_pszPosition, BUFFERSIZE , TEXT("0x%08lx"), (unsigned long) i);
        return *this;
    }

    inline CWDebugStream& CWDebugStream::output_hex(unsigned char b)
    {
        m_pszPosition+=_stprintf_s(m_pszPosition, BUFFERSIZE  , TEXT("0x%02hx"), (unsigned short) b);
        return *this;
    }

    inline CWDebugStream& CWDebugStream::output_HEX(unsigned long l)
    {
        m_pszPosition+=_stprintf_s(m_pszPosition, BUFFERSIZE  , TEXT("0x%08lX"), l);
        return *this;
    }

    inline CWDebugStream& CWDebugStream::output_HEX(unsigned short s)
    {
        m_pszPosition+=_stprintf_s(m_pszPosition, BUFFERSIZE  , TEXT("0x%04X"), s);
        return *this;
    }

    inline CWDebugStream& CWDebugStream::output_HEX(unsigned int i)
    {
        m_pszPosition+=_stprintf_s(m_pszPosition, BUFFERSIZE  , TEXT("0x%08lX"), (unsigned long) i);
        return *this;
    }

    inline CWDebugStream& CWDebugStream::output_HEX(unsigned char b)
    {
        m_pszPosition+=_stprintf_s(m_pszPosition, BUFFERSIZE  , TEXT("0x%02hX"), (unsigned short) b);
        return *this;
    }

    inline CWDebugStream& CWDebugStream::output_string(const wchar_t* psz)
    {
        m_pszPosition+=_stprintf_s(m_pszPosition, BUFFERSIZE  , psz ? TEXT("%ls") : TEXT("NULL"), psz);
        return *this;
    }

    inline CWDebugStream& CWDebugStream::output_string(const char* psz)
    {
        // this doesn't work for really long strings
        m_pszPosition+=_stprintf_s(m_pszPosition, BUFFERSIZE  , psz ? TEXT("%hs") : TEXT("NULL"), psz);
        return *this;
    }

    inline CWDebugStream& CWDebugStream::output_decimal(unsigned long l)
    {
        m_pszPosition+=_stprintf_s(m_pszPosition, BUFFERSIZE  , TEXT("%lu"), l);
        return *this;
    }

    inline CWDebugStream& CWDebugStream::output_decimal(long l)
    {
        m_pszPosition+=_stprintf_s(m_pszPosition, BUFFERSIZE  , TEXT("%ld"), l);
        return *this;
    }

    inline CWDebugStream& CWDebugStream::output_decimal(unsigned short i)
    {
        m_pszPosition+=_stprintf_s(m_pszPosition, BUFFERSIZE  , TEXT("%u"), i);
        return *this;
    }

    inline CWDebugStream& CWDebugStream::output_decimal(unsigned int i)
    {
        m_pszPosition+=_stprintf_s(m_pszPosition, BUFFERSIZE  , TEXT("%lu"), (unsigned long) i);
        return *this;
    }

    inline CWDebugStream& CWDebugStream::output_decimal(short i)
    {
        m_pszPosition+=_stprintf_s(m_pszPosition, BUFFERSIZE  , TEXT("%d"), i);
        return *this;
    }

    inline CWDebugStream& CWDebugStream::output_decimal(int i)
    {
        m_pszPosition+=_stprintf_s(m_pszPosition, BUFFERSIZE  , TEXT("%ld"), (long) i);
        return *this;
    }

    inline CWDebugStream& CWDebugStream::output_decimal(unsigned __int64 i)
    {
        m_pszPosition+=_stprintf_s(m_pszPosition, BUFFERSIZE  , TEXT("%I64u"), i);
        return *this;
    }

    inline CWDebugStream& CWDebugStream::output_decimal(signed __int64 i)
    {
        m_pszPosition+=_stprintf_s(m_pszPosition, BUFFERSIZE  , TEXT("%I64d"), i);
        return *this;
    }

    inline CWDebugStream& CWDebugStream::output_character(wchar_t c)
    {
        m_pszPosition+=_stprintf_s(m_pszPosition, BUFFERSIZE  , TEXT("%lc"), c);
        return *this;
    }

    inline CWDebugStream& CWDebugStream::output_character(char c)
    {
        m_pszPosition+=_stprintf_s(m_pszPosition, BUFFERSIZE  , TEXT("%hc"), c);
        return *this;
    }

    inline CWDebugStream& CWDebugStream::output_double(double d)
    {
        m_pszPosition+=_stprintf_s(m_pszPosition, BUFFERSIZE  , TEXT("%g"), d);
        return *this;
    }

    inline CWDebugStream& CWDebugStream::output_pointer(void * pv)
    {
        m_pszPosition+=_stprintf_s(m_pszPosition, BUFFERSIZE  , TEXT("%p"), pv);
        return *this;
    }


    // ============
    // Manipulators
    // ============

    // endline manipulator
    inline CWDebugStream& endl(CWDebugStream& s)
    {
        return s.endl();
    }

    // flush manipulator
    inline CWDebugStream& flush(CWDebugStream& s)
    {
        return s.flush();
    }

    // indent manipulator
    inline CWDebugStream& indent(CWDebugStream& s)
    {
        return s.push_indent();
    }

    inline CWDebugStream& unindent(CWDebugStream& s)
    {
        return s.pop_indent();
    }

    // unary stream manipulator
    template <class TArg>
    struct unary_stream_manipulator
    {
        typedef CWDebugStream& (*function_type)(CWDebugStream&, TArg);
        function_type m_f;
        TArg m_v;

        unary_stream_manipulator(function_type f, TArg v) : m_f(f), m_v(v) {}

        CWDebugStream& call(CWDebugStream& stream) const { return m_f(stream, m_v); }
    };

    template <class TArg>
    CWDebugStream& operator << ( CWDebugStream& stream, const unary_stream_manipulator<TArg>& manip )
    {
        return manip.call(stream);
    }

    // binary stream manipulator
    template <class TArg1, class TArg2>
    struct binary_stream_manipulator
    {
        typedef CWDebugStream& (*function_type)(CWDebugStream&, TArg1, TArg2);
        function_type m_f;
        TArg1 m_v0;
        TArg2 m_v1;

        binary_stream_manipulator(function_type f, TArg1 v0, TArg2 v1) : m_f(f), m_v0(v0), m_v1(v1) {}

        CWDebugStream& call(CWDebugStream& stream) const { return m_f(stream, m_v0, m_v1); }
    };

    template <class TArg1, class TArg2>
    CWDebugStream& operator << ( CWDebugStream& stream, const binary_stream_manipulator<TArg1, TArg2>& manip )
    {
        return manip.call(stream);
    }

    // ternary stream manipulator
    template <class TArg1, class TArg2, class TArg3>
    struct ternary_stream_manipulator
    {
        typedef CWDebugStream& (*function_type)(CWDebugStream&, TArg1, TArg2, TArg3);
        function_type m_f;
        TArg1 m_v0;
        TArg2 m_v1;
        TArg3 m_v2;

        ternary_stream_manipulator(function_type f, TArg1 v0, TArg2 v1, TArg3 v2) : m_f(f), m_v0(v0), m_v1(v1), m_v2(v2) {}

        CWDebugStream& call(CWDebugStream& stream) const { return m_f(stream, m_v0, m_v1, m_v2); }
    };

    template <class TArg1, class TArg2, class TArg3>
    CWDebugStream& operator << ( CWDebugStream& stream, const ternary_stream_manipulator<TArg1, TArg2, TArg3>& manip )
    {
        return manip.call(stream);
    }

    // LOG VERBOSITY manipulator
    inline CWDebugStream& __log(CWDebugStream& s, eVerbosityLevel v)
    {
        return s.SetVerbosity(v);
    }

    inline unary_stream_manipulator<eVerbosityLevel> log(eVerbosityLevel v)
    {
        return unary_stream_manipulator<eVerbosityLevel>(__log, v);
    }

    // BeginLevel manipulator
    inline CWDebugStream& __BeginLevel(CWDebugStream& s, DWORD dwLevelID)
    {
        return s.BeginLevel(dwLevelID);
    }

    inline unary_stream_manipulator<DWORD> BeginLevel(DWORD dwLevelID)
    {
        return unary_stream_manipulator<DWORD>(__BeginLevel, dwLevelID);
    }

    // EndLevel manipulator
    inline CWDebugStream& EndLevel(CWDebugStream& s)
    {
        return s.EndLevel();
    }

    // hex output manipulators
    inline CWDebugStream& __hex1(CWDebugStream& s, unsigned long l)
    {
        return s.output_hex(l);
    }

    inline unary_stream_manipulator<unsigned long> hex(unsigned long l)
    {
        return unary_stream_manipulator<unsigned long>(__hex1, l);
    }

    inline CWDebugStream& __hex2(CWDebugStream& s, unsigned short v)
    {
        return s.output_hex(v);
    }

    inline CWDebugStream& __hex3(CWDebugStream& s, unsigned int i)
    {
        return s.output_hex(i);
    }

    inline CWDebugStream& __hex4(CWDebugStream& s, unsigned char b)
    {
        return s.output_hex(b);
    }

    inline unary_stream_manipulator<unsigned short> hex(unsigned short s)
    {
        return unary_stream_manipulator<unsigned short>(__hex2, s);
    }

    inline unary_stream_manipulator<unsigned int> hex(unsigned int i)
    {
        return unary_stream_manipulator<unsigned int>(__hex3, i);
    }

    inline unary_stream_manipulator<unsigned char> hex(unsigned char b)
    {
        return unary_stream_manipulator<unsigned char>(__hex4, b);
    }

    inline CWDebugStream& __HEX1(CWDebugStream& s, unsigned long l)
    {
        return s.output_HEX(l);
    }

    inline unary_stream_manipulator<unsigned long> HEX(unsigned long l)
    {
        return unary_stream_manipulator<unsigned long>(__HEX1, l);
    }

    inline CWDebugStream& __HEX2(CWDebugStream& s, unsigned short v)
    {
        return s.output_HEX(v);
    }

    inline CWDebugStream& __HEX3(CWDebugStream& s, unsigned int i)
    {
        return s.output_HEX(i);
    }

    inline CWDebugStream& __HEX4(CWDebugStream& s, unsigned char b)
    {
        return s.output_HEX(b);
    }

    inline unary_stream_manipulator<unsigned short> HEX(unsigned short s)
    {
        return unary_stream_manipulator<unsigned short>(__HEX2, s);
    }

    inline unary_stream_manipulator<unsigned int> HEX(unsigned int i)
    {
        return unary_stream_manipulator<unsigned int>(__HEX3, i);
    }

    inline unary_stream_manipulator<unsigned char> HEX(unsigned char b)
    {
        return unary_stream_manipulator<unsigned char>(__HEX4, b);
    }

    inline CWDebugStream& __wchr(CWDebugStream& s, wchar_t c)
    {
        return s.output_character(c);
    }

    inline unary_stream_manipulator<wchar_t> chr(wchar_t c)
    {
        return unary_stream_manipulator<wchar_t>(__wchr, c);
    }

    inline CWDebugStream& __chr(CWDebugStream& s, char c)
    {
        return s.output_character(c);
    }

    inline unary_stream_manipulator<char> chr(char c)
    {
        return unary_stream_manipulator<char>(__chr, c);
    }

    // =======================
    // stream output operators
    // =======================
    inline CWDebugStream& operator << (CWDebugStream& stream, const wchar_t* psz)
    {
        return stream.output_string(psz);
    }

    inline CWDebugStream& operator << (CWDebugStream& stream, const std::wstring& str)
    {
        return stream.output_string(str.c_str());
    }

    inline CWDebugStream& operator << (CWDebugStream& stream, const char* psz)
    {
        return stream.output_string(psz);
    }

    inline CWDebugStream& operator << (CWDebugStream& stream, const std::string& str)
    {
        return stream.output_string(str.c_str());
    }

    template <long TN>
    inline CWDebugStream& operator << (CWDebugStream& stream, const char rc[TN])
    {
        return stream.output_formatted(TEXT("%.*s"), TN, (LPCSTR)rc);
    }

    template <long TN>
    inline CWDebugStream& operator << (CWDebugStream& stream, const wchar_t rc[TN])
    {
        return stream.output_formatted(TEXT("%.*s"), TN, (LPCWSTR)rc);
    }

    inline CWDebugStream& operator << (CWDebugStream& stream, unsigned long l) 
    {
        return stream.output_decimal(l);
    }

    inline CWDebugStream& operator << (CWDebugStream& stream, long l)
    {
        return stream.output_decimal(l);
    }

    inline CWDebugStream& operator << (CWDebugStream& stream, unsigned __int64 i) 
    {
        return stream.output_decimal(i);
    }

    inline CWDebugStream& operator << (CWDebugStream& stream, signed __int64 i) 
    {
        return stream.output_decimal(i);
    }

    inline CWDebugStream& operator << (CWDebugStream& stream, unsigned short i)
    {
        return stream.output_decimal(i);
    }

    inline CWDebugStream& operator << (CWDebugStream& stream, short i)
    {
        return stream.output_decimal(i);
    }

    inline CWDebugStream& operator << (CWDebugStream& stream, unsigned int i)
    {
        return stream.output_decimal(i);
    }

    inline CWDebugStream& operator << (CWDebugStream& stream, int i)
    {
        return stream.output_decimal(i);
    }

    inline CWDebugStream& operator << (CWDebugStream& stream, double d)
    {
        return stream.output_double(d);
    }
    
    inline CWDebugStream& operator << (CWDebugStream& stream, void* pv)
    {
        return stream.output_pointer(pv);
    }

    inline CWDebugStream& operator << (CWDebugStream& stream, const RECT &rc)
    {
        return stream.output_formatted(TEXT("{ %d, %d, %d, %d }"), rc.left, rc.top, rc.right, rc.bottom);
    }
    
    inline CWDebugStream& operator << (CWDebugStream& stream, const GUID &uid)
    { 
        return stream.output_formatted(TEXT("{ %08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x }"),
              uid.Data1, uid.Data2, uid.Data3, uid.Data4[0], uid.Data4[1], uid.Data4[2], uid.Data4[3], 
              uid.Data4[4], uid.Data4[5], uid.Data4[6], uid.Data4[7]);   
    }

    typedef CWDebugStream CDebugStream;

    // extern __declspec( thread ) CDebugStream dbgout;
    extern CDebugStream dbgout;
} // namespace DebugStreamUty

// for backwards compatability with hqalog stuff
extern void Debug(eVerbosityLevel nVerbosity, LPCTSTR pszFormat, ...);
extern void DebugBeginLevel(DWORD dwID, LPCTSTR pszFormat, ...);
extern void DebugEndLevel(LPCTSTR pszFormat, ...);

#endif
