////////////////////////////////////////////////////////////////////////////////
//
//  DirectDraw Utility Library
//  Copyright (c) 1999, Microsoft Corporation
//
//  Module: PixelConverter.h
//          Declares the CPixelConverter class.
//
////////////////////////////////////////////////////////////////////////////////

#ifndef __PIXELCONVERTER_H__
#define __PIXELCONVERTER_H__

namespace DDrawUty
{

////////////////////////////////////////////////////////////////////////////////

    class CPixelConverter
    {
    public:
        CPixelConverter();
        CPixelConverter(const DDPIXELFORMAT &rddpf);
        CPixelConverter(IUnknown *pDDS);

        // Operations
        void    Initialize(const DDPIXELFORMAT &rddpf);
        bool    Initialize(IUnknown *pDDS);

        // Methods
        void    RGBAFromPixel(DWORD &rdwR, DWORD &rdwG, DWORD &rdwB, DWORD &rdwA, DWORD dwPixel) const;
        void    PixelFromRGBA(DWORD &rdwPixel, DWORD dwR, DWORD dwG, DWORD dwB, DWORD dwA) const;
        void    JumpPixels(void **ppvPixel, int nJumpCount) const;
        bool    ReadRGBA(DWORD &rdwR, DWORD &rdwG, DWORD &rdwB, DWORD &rdwA, void **ppvPixel) const;
        bool    WriteRGBA(void **ppvPixel, DWORD dwR, DWORD dwG, DWORD dwB, DWORD dwA) const;
        void    ClampRGBA(DWORD &rdwR, DWORD &rdwG, DWORD &rdwB, DWORD &rdwA) const;
        DWORD   MaxAlpha() const;
        void    RoundRGBA(float fR, float fG, float fB, float fA, DWORD &rdwR, DWORD &rdwG, DWORD &rdwB, DWORD &rdwA) const;
        void    NormalizeRGBA(DWORD dwR, DWORD dwG, DWORD dwB, DWORD dwA, float &rfR, float &rfG, float &rfB, float &rfA) const;
        void    UnNormalizeRGBA(float fR, float fG, float fB, float fA, DWORD &rdwR, DWORD &rdwG, DWORD &rdwB, DWORD &rdwA) const;
        bool    IsEqualRGBA(DWORD dwR1, DWORD dwG1, DWORD dwB1, DWORD dwA1, DWORD dwR2, DWORD dwG2, DWORD dwB2, DWORD dwA2, int nDelta) const;

    private:
        // Helpers
        static void     ShiftFactorFromMask(int &rnShiftFactor, DWORD dwMask);
        static void     StepSizeFromMask(int &rnStepSize, DWORD dwMask);
        static DWORD    ChannelFromPixel(DWORD dwPixel, int nShiftFactor, DWORD dwMask);
        static DWORD    PixelFromChannel(DWORD dwChannel, int nShiftFactor, DWORD dwMask);

        // Data
        DWORD   m_dwPixelFlags;
        DWORD   m_dwRMask,
                m_dwGMask,
                m_dwBMask,
                m_dwAMask,
                m_dwPixelMask;
        int     m_nRShiftFactor,
                m_nGShiftFactor,
                m_nBShiftFactor,
                m_nAShiftFactor;
        int     m_nRStepSize,
                m_nGStepSize,
                m_nBStepSize,
                m_nAStepSize;
        float   m_fRScale,
                m_fGScale,
                m_fBScale,
                m_fAScale;
        UINT    m_nBytesPerPixel;
        
    };

////////////////////////////////////////////////////////////////////////////////

}

#endif // __PIXELCONVERTER_H__
