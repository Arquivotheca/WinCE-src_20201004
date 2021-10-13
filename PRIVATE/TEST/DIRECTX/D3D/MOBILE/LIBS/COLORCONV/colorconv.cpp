//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include <ColorConv.h>
#include <tchar.h>

//
// GetByteBitString
//
//   Given a byte of data, and adequate string storage, this function produces a character dump
//   of the binary representation of the byte.
//
// Arguments:
//
//   TCHAR *ptchString:  A string that is a minimum of 9 characters wide (allows for 8 characters
//                       reserved for the bit dump and NULL termination).
//   BYTE Byte:  Data to use as a source for retrieving bit values.
// 
// Return Values:
// 
//   VOID
//
VOID GetByteBitString(WCHAR *pwszString, BYTE Byte)
{
    CONST UCHAR uchLastBitPos = 7; // Zero-based index to last valid char position for bit data
    INT iBitPos;
    wcscpy(pwszString,L"????????");

    for(iBitPos = 0; iBitPos < 8; iBitPos++)
    {   
        if (((UCHAR)0x01 << iBitPos) & Byte)
            pwszString[uchLastBitPos-iBitPos] = L'1';
        else
            pwszString[uchLastBitPos-iBitPos] = L'0';
    }
}

//
// PackPixel
//
//   Accepts a four color components with a pixel format, then shifts all components into position
//   and merges them into one pixel.  Built-in assumptions:
//
//      * No component may exceed eight bits in size (enforced by type).
//      * The format of the pixel has been described in PIXEL_UNPACK prior to this call.
//
// Arguments:
//
//   PIXEL_UNPACK:  Specification describing shifts relavent to individual components.
//   PVOID:  Pointer to storage for this pixel value, of the correct size for the format
//   BYTE Red:  Input byte for red component.
//   BYTE Green:  Input byte for green component.
//   BYTE Blue:  Input byte for blue component.
//   BYTE Alpha:  Input byte for alpha component.
//
// Return Value:
//
//   VOID:  Because this function is likely to be used in "inner loops", return value processing is
//          prohibitively expensive.
//
VOID __forceinline PackPixel(CONST PIXEL_UNPACK *pSpec, VOID *pPixel, BYTE Red, BYTE Green, BYTE Blue, BYTE Alpha)
{
	DWORD dwTempPixel;


    //
    // Pixel data may be 32BPP, 24BPP, 16BPP, or 8BPP
    //
    switch(pSpec->uiTotalBytes)
    {
    case 4:
		*(DWORD*)pPixel = (DWORD)Red   << pSpec->uchRedSHR   |
                          (DWORD)Green << pSpec->uchGreenSHR |
                          (DWORD)Blue  << pSpec->uchBlueSHR  |
                          (DWORD)Alpha << pSpec->uchAlphaSHR;

        break;
    case 3:
		dwTempPixel = (DWORD)Red   << pSpec->uchRedSHR   |
                      (DWORD)Green << pSpec->uchGreenSHR |
                      (DWORD)Blue  << pSpec->uchBlueSHR  |
                      (DWORD)Alpha << pSpec->uchAlphaSHR;

		// Store in dest buffer
		((BYTE*)pPixel)[0] = ((BYTE*)&dwTempPixel)[0];
		((BYTE*)pPixel)[1] = ((BYTE*)&dwTempPixel)[1];
		((BYTE*)pPixel)[2] = ((BYTE*)&dwTempPixel)[2];

        break;
    case 2:
		 *(WORD*)pPixel = (WORD)Red   << pSpec->uchRedSHR   |
						  (WORD)Green << pSpec->uchGreenSHR |
						  (WORD)Blue  << pSpec->uchBlueSHR  |
						  (WORD)Alpha << pSpec->uchAlphaSHR;
		 break;
    case 1:
		 *(BYTE*)pPixel = Red   << pSpec->uchRedSHR   |
						  Green << pSpec->uchGreenSHR |
						  Blue  << pSpec->uchBlueSHR  |
						  Alpha << pSpec->uchAlphaSHR;
        break;
    }
}

//
// UnpackPixel
//
//   Accepts a 32-bit pixel value, and seperates out each component.  Assumptions:
//
//      * No component may exceed eight bits in size.
//      * The format of the pixel has been described in PIXEL_UNPACK prior to this call.
//
// Arguments:
//
//   PIXEL_UNPACK:  Specification describing how to "unpack" the components of this pixel format.
//   BPP32:  Pointer to 32-bit storage for this pixel value
//   BYTE Red:  Output byte for seperated red component.
//   BYTE Green:  Output byte for seperated green component.
//   BYTE Blue:  Output byte for seperated blue component.
//   BYTE Alpha:  Output byte for seperated alpha component.
//
// Return Value:
//
//   VOID:  Because this function is likely to be used in "inner loops", return value processing is
//          prohibitively expensive.
//
VOID __forceinline UnpackPixel(CONST PIXEL_UNPACK *pSpec, CONST BPP32 *pPixel, BYTE *Red, BYTE *Green, BYTE *Blue, BYTE *Alpha)
{
    // Mask & shift to the far right (removes all unwanted bits left/right of the component)
    *Red   = (BYTE)((*(DWORD*)pPixel & pSpec->dwRedMask)   >> pSpec->uchRedSHR);
    *Green = (BYTE)((*(DWORD*)pPixel & pSpec->dwGreenMask) >> pSpec->uchGreenSHR);
    *Blue  = (BYTE)((*(DWORD*)pPixel & pSpec->dwBlueMask)  >> pSpec->uchBlueSHR);
    *Alpha = (BYTE)((*(DWORD*)pPixel & pSpec->dwAlphaMask) >> pSpec->uchAlphaSHR);

    // Shift left to byte boundery
    *Red <<= 8 - pSpec->uchRedBits;
    *Green <<= 8 - pSpec->uchGreenBits;
    *Blue <<= 8 - pSpec->uchBlueBits;
    *Alpha <<= 8 - pSpec->uchAlphaBits;

    // Special handling for formats with no alpha component; assume intent was opaque pixel
    if (0 == pSpec->dwAlphaMask)
        *Alpha = 0xFF;
}

//
// UnpackPixel
//
//   Accepts a 24-bit pixel value, and seperates out each component.  Assumptions:
//
//      * No component may exceed eight bits in size.
//      * The format of the pixel has been described in PIXEL_UNPACK prior to this call.
//
// Arguments:
//
//   PIXEL_UNPACK:  Specification describing how to "unpack" the components of this pixel format.
//   BPP24:  Pointer to 24-bit storage for this pixel value
//   BYTE Red:  Output byte for seperated red component.
//   BYTE Green:  Output byte for seperated green component.
//   BYTE Blue:  Output byte for seperated blue component.
//   BYTE Alpha:  Output byte for seperated alpha component.
//
// Return Value:
//
//   VOID:  Because this function is likely to be used in "inner loops", return value processing is
//          prohibitively expensive.
//
VOID __forceinline UnpackPixel(CONST PIXEL_UNPACK *pSpec, CONST BPP24 *pPixel, BYTE *Red, BYTE *Green, BYTE *Blue, BYTE *Alpha)
{
    BPP32 Pixel32;
    *(DWORD*)(&Pixel32) = ((DWORD)(((BYTE*)pPixel)[2])) << 16;
    *(DWORD*)(&Pixel32) |= ((DWORD)(((BYTE*)pPixel)[1]))<< 8;
    *(DWORD*)(&Pixel32) |= ((DWORD)(((BYTE*)pPixel)[0]))<< 0;
    UnpackPixel(pSpec, &Pixel32, Red, Green, Blue, Alpha);
}


//
// UnpackPixel
//
//   Accepts a 16-bit pixel value, and seperates out each component.  Assumptions:
//
//      * No component may exceed eight bits in size.
//      * The format of the pixel has been described in PIXEL_UNPACK prior to this call.
//
// Arguments:
//
//   PIXEL_UNPACK:  Specification describing how to "unpack" the components of this pixel format.
//   BPP16:  Pointer to 16-bit storage for this pixel value
//   BYTE Red:  Output byte for seperated red component.
//   BYTE Green:  Output byte for seperated green component.
//   BYTE Blue:  Output byte for seperated blue component.
//   BYTE Alpha:  Output byte for seperated alpha component.
//
// Return Value:
//
//   VOID:  Because this function is likely to be used in "inner loops", return value processing is
//          prohibitively expensive.
//
VOID __forceinline UnpackPixel(CONST PIXEL_UNPACK *pSpec, CONST BPP16 *pPixel, BYTE *Red, BYTE *Green, BYTE *Blue, BYTE *Alpha)
{
    BPP32 Pixel32;
    *(DWORD*)(&Pixel32) = ((DWORD)(((BYTE*)pPixel)[1])) << 8;
    *(DWORD*)(&Pixel32) |= ((DWORD)(((BYTE*)pPixel)[0]))<< 0;
    UnpackPixel(pSpec, &Pixel32, Red, Green, Blue, Alpha);
}

//
// UnpackPixel
//
//   Accepts a 8-bit pixel value, and seperates out each component.  Assumptions:
//
//      * The format of the pixel has been described in PIXEL_UNPACK prior to this call.
//
// Arguments:
//
//   PIXEL_UNPACK:  Specification describing how to "unpack" the components of this pixel format.
//   BPP8:  Pointer to 8-bit storage for this pixel value
//   BYTE Red:  Output byte for seperated red component.
//   BYTE Green:  Output byte for seperated green component.
//   BYTE Blue:  Output byte for seperated blue component.
//   BYTE Alpha:  Output byte for seperated alpha component.
//
// Return Value:
//
//   VOID:  Because this function is likely to be used in "inner loops", return value processing is
//          prohibitively expensive.
//
VOID __forceinline UnpackPixel(CONST PIXEL_UNPACK *pSpec, CONST BPP8 *pPixel, BYTE *Red, BYTE *Green, BYTE *Blue, BYTE *Alpha)
{
    BPP32 Pixel32;
    *(DWORD*)(&Pixel32) = ((DWORD)(((BYTE*)pPixel)[0]))<< 0;
    UnpackPixel(pSpec, &Pixel32, Red, Green, Blue, Alpha);
}


//
// UnpackPixel
//
//   Accepts a pixel value of arbitrary format, and seperates out each component.  Assumptions:
//
//      * The format of the pixel has been described in PIXEL_UNPACK prior to this call.
//
// Arguments:
//
//   PIXEL_UNPACK:  Specification describing how to "unpack" the components of this pixel format.
//   PVOID:  Pointer to 8BPP / 16BPP / 24BPP / 32BPP storage for this pixel value
//   BYTE Red:  Output byte for seperated red component.
//   BYTE Green:  Output byte for seperated green component.
//   BYTE Blue:  Output byte for seperated blue component.
//   BYTE Alpha:  Output byte for seperated alpha component.
//
// Return Value:
//
//   VOID:  Because this function is likely to be used in "inner loops", return value processing is
//          prohibitively expensive.
//
VOID __forceinline UnpackPixel(CONST PIXEL_UNPACK *pSpec, CONST VOID *pPixel, BYTE *Red, BYTE *Green, BYTE *Blue, BYTE *Alpha)
{
    //
    // Pixel data is right-aligned in DWORD; left part of DWORD is unused for non 32-bit formats
    //
    switch(pSpec->uiTotalBytes)
    { 
    case 4:
        UnpackPixel(pSpec, (BPP32*)((BYTE*)pPixel), Red, Green, Blue, Alpha);
        break;
    case 3:
        UnpackPixel(pSpec, (BPP24*)((BYTE*)pPixel), Red, Green, Blue, Alpha);
        break;
    case 2:
        UnpackPixel(pSpec, (BPP16*)((BYTE*)pPixel), Red, Green, Blue, Alpha);
        break;
    case 1:
        UnpackPixel(pSpec, (BPP8*)((BYTE*)pPixel),  Red, Green, Blue, Alpha);
        break;
    }
}

//
// ConvertPrecision
//
//   For use with per-component color-conversion code.  Converts a color
//   of arbitrary precision to a color of another arbitrary precision, via
//   the simple shift technique (not the most exact technique).
//
// Arguments:
// 
//   None
//
// Return Value:
//
//   VOID
//
VOID __forceinline ConvertPrecision(UCHAR uchIn, UCHAR uchInBits, BYTE *pOut, UCHAR uchOutBits)
{
    // 
    // Shift in correct direction
    //
    if (uchInBits > uchOutBits)
    {
        *pOut = uchIn >> (uchInBits - uchOutBits);
        return;
    }
    if (uchInBits < uchOutBits)
    {
        *pOut = uchIn << (uchOutBits - uchInBits);
        return;
    }

    //
    // No precision change; simple copy
    //
    *pOut = uchIn; 
}

//
//  ConvertPixel
//
//    Converts a pixel, of specified source format, to a pixel of specified destination format.
//
//  Arguments:
//
//    PIXEL_UNPACK *pSource:  Input format description
//    PIXEL_UNPACK *pDest:  Input format description
//    BYTE* pSourcePixel:  Input pixel
//    BYTE* pDestPixel:  Output Pixel
// 
//  Return Value:
//
//    VOID
//
VOID ConvertPixel(CONST PIXEL_UNPACK *pSource, CONST PIXEL_UNPACK *pDest, CONST BYTE* pSourcePixel, BYTE* pDestPixel)
{
	UCHAR uchRed, uchGreen, uchBlue, uchAlpha;
	UCHAR uchConvRed, uchConvGreen, uchConvBlue, uchConvAlpha;

	UnpackPixel(pSource, pSourcePixel, &uchRed, &uchGreen, &uchBlue, &uchAlpha);

	// Per component, convert to color depth of destination
	ConvertPrecision(uchRed,   8, &uchConvRed,   pDest->uchRedBits);
	ConvertPrecision(uchGreen, 8, &uchConvGreen, pDest->uchGreenBits);
	ConvertPrecision(uchBlue,  8, &uchConvBlue,  pDest->uchBlueBits);
	ConvertPrecision(uchAlpha, 8, &uchConvAlpha, pDest->uchAlphaBits);

	PackPixel(pDest, pDestPixel, uchConvRed, uchConvGreen, uchConvBlue, uchConvAlpha);
}


//
//  ConvertPixelDbg
//
//    Debug version of ConvertPixel.  Converts a pixel, of specified source format, to a pixel of specified
//    destination format.  Passes back all intermediate color values from color conversion.
//
//  Arguments:
//
//    PIXEL_UNPACK *pSource:  Input format description
//    PIXEL_UNPACK *pDest:  Input format description
//    BYTE* pSourcePixel:  Input pixel
//    BYTE* pDestPixel:  Output Pixel
//    Intermediate RGBA color values
// 
//  Return Value:
//
//    VOID
//
VOID __forceinline ConvertPixelDbg(CONST PIXEL_UNPACK *pSource, CONST PIXEL_UNPACK *pDest, CONST BYTE* pSourcePixel, BYTE* pDestPixel,
								   UCHAR *puchRed, UCHAR *puchGreen, UCHAR *puchBlue, UCHAR *puchAlpha,
								   UCHAR *puchConvRed, UCHAR *puchConvGreen, UCHAR *puchConvBlue, UCHAR *puchConvAlpha)
{

	UnpackPixel(pSource, pSourcePixel, puchRed, puchGreen, puchBlue, puchAlpha);

	// Per component, convert to color depth of destination
	ConvertPrecision(*puchRed,   8, puchConvRed,   pDest->uchRedBits);
	ConvertPrecision(*puchGreen, 8, puchConvGreen, pDest->uchGreenBits);
	ConvertPrecision(*puchBlue,  8, puchConvBlue,  pDest->uchBlueBits);
	ConvertPrecision(*puchAlpha, 8, puchConvAlpha, pDest->uchAlphaBits);

	PackPixel(pDest, pDestPixel, *puchConvRed, *puchConvGreen, *puchConvBlue, *puchConvAlpha);
}


/////////////////////////////////////////////////////////////////////
// Unit tests for above color conversion code
/////////////////////////////////////////////////////////////////////

#ifdef D3DQA_BUILD_COLORCONV_UNITTESTS

//
// ConvertTest
//
//   Simple unit test to demonstrate inputs/outputs of conversion, for manual
//   inspection and verification.  Useful for verifying correctness of
//   changes/updates.
//
// Arguments:
// 
//   None
//
// Return Value:
//
//   VOID
//
VOID ConvertTest()
{
    CONST UINT uiDbgOutputMax = 255;
    TCHAR tszDbgOutput[uiDbgOutputMax];
    UINT uiInBits, uiDestPrecision, uiSrcPrecision;
    UCHAR uchOutBits;
    WCHAR wszInBits[QA_BYTE_BIT_LENGTH];
    WCHAR wszOutBits[QA_BYTE_BIT_LENGTH];

    for (uiInBits = 0; uiInBits < 256; uiInBits++)
    {
        // To minimize this to a reasonable set of permutations, skip many of the
        // values in the iterator's range (keeping, among other things, corner cases).
        // The specific skipped ranges are somewhat arbitrary; but functionally
        // mandatory to make the size of the debug output tractable
        if ( 18 == uiInBits) uiInBits = 125;
        if ( 130 == uiInBits) uiInBits = 250;

        // Only test for precisions that correspond to pixel formats with at least
        // 2 bits per component
        for(uiSrcPrecision = 2; uiSrcPrecision <= 8; uiSrcPrecision++)
        {
            //
            // If the input bits have some bits on, that are out of range for this
            // source precision, skip the iteration.
            //
            //  0x01 << Precision  :  Put a one in the first bit past the valid bits
            //                        for this precision
            //
            //  subtract 1 :  Set all bits, valid for this precision, to one.  Set
            //                all other bits to zero.
            //
            //  bitwise NOT :  Set all bits, invalid for this precision, to one.  Set
            //                 all other bits to zero.
            //
            //  bitwise AND :  If any of the proposed source bits are in invalid 
            //                 positions, skip this iteration
            //
            if ((UCHAR)uiInBits & (~(((UCHAR)0x01 << uiSrcPrecision) - 1)))
                continue;

            for(uiDestPrecision = 2; uiDestPrecision <= 8; uiDestPrecision++)
            {
                ConvertPrecision((BYTE)uiInBits, uiSrcPrecision, &uchOutBits, uiDestPrecision);
                GetByteBitString(wszInBits, (BYTE)uiInBits);
                GetByteBitString(wszOutBits, (BYTE)uchOutBits);
                
                _stprintf(tszDbgOutput, _T("INPUT: [%lSb];  OUTPUT: [%lSb];  Precision Conversion: [%lu => %lu]\n"), wszInBits, wszOutBits, uiSrcPrecision, uiDestPrecision);
                OutputDebugString(tszDbgOutput);
            }
        }
    }
}


//
// UnpackPackTest
//
//   Simple unit test to demonstrate that the unpack/pack routines can be used
//   in combination to achieve color conversion.  This unit test is useful for
//   manually inspecting sample cases, to verify correctness of changes/updates.
//
// Arguments:
// 
//   None
//
// Return Value:
//
//   VOID
//
VOID UnpackPackTest()
{
    CONST UINT uiDbgOutputMax = 255;
    TCHAR tszDbgOutput[uiDbgOutputMax];
    CONST UINT uiMaxValues = 6;
    UINT uiInFormat, uiOutFormat, uiValueIter;

    for (uiValueIter = 0; uiValueIter < uiMaxValues; uiValueIter++)
    {
        DWORD dwValue;

        // Sample data
        switch(uiValueIter)
        {
        case 0:
            dwValue = 0xFFFFFFFF; // Repeating bit-pattern: 1111b
            break;
        case 1:
            dwValue = 0x55555555; // Repeating bit-pattern: 0101b
            break;
        case 2:
            dwValue = 0xAAAAAAAA; // Repeating bit-pattern: 1010b
            break;
        case 3:
            dwValue = 0x00000000; // Repeating bit-pattern: 0000b
            break;
        case 4:
            dwValue = 0x99999999; // Repeating bit-pattern: 1001b
            break;
        case 5:
            dwValue = 0x66666666; // Repeating bit-pattern: 0110b
            break;
        }

        for (uiInFormat = 1; uiInFormat < QA_NUM_UNPACK_FORMATS; uiInFormat++)
        {

            for (uiOutFormat = 1; uiOutFormat < QA_NUM_UNPACK_FORMATS; uiOutFormat++)
            {

                WCHAR wszIn1[QA_BYTE_BIT_LENGTH], wszIn2[QA_BYTE_BIT_LENGTH], wszIn3[QA_BYTE_BIT_LENGTH], wszIn4[QA_BYTE_BIT_LENGTH];
                WCHAR wszRed[QA_BYTE_BIT_LENGTH], wszGreen[QA_BYTE_BIT_LENGTH], wszBlue[QA_BYTE_BIT_LENGTH], wszAlpha[QA_BYTE_BIT_LENGTH];
                UCHAR uchRed, uchGreen, uchBlue, uchAlpha;
                UCHAR uchConvRed, uchConvGreen, uchConvBlue, uchConvAlpha;
                DWORD dwResult;
/*
				UnpackPixel(&UnpackFormat[uiInFormat], (VOID*)&dwValue, &uchRed, &uchGreen, &uchBlue, &uchAlpha);

                GetByteBitString(wszIn1, ((BYTE*)&dwValue)[0]);
                GetByteBitString(wszIn2, ((BYTE*)&dwValue)[1]);
                GetByteBitString(wszIn3, ((BYTE*)&dwValue)[2]);
                GetByteBitString(wszIn4, ((BYTE*)&dwValue)[3]);

                GetByteBitString(wszRed, uchRed);
                GetByteBitString(wszGreen, uchGreen);
                GetByteBitString(wszBlue, uchBlue);
                GetByteBitString(wszAlpha, uchAlpha);

                switch(UnpackFormat[uiInFormat].uiTotalBytes)
                {
                case 4:
                    _stprintf(tszDbgOutput, _T("IN FORMAT: %lS;  32BPP INPUT: [%lS %lS %lS %lS];  OUTPUT: [RED: %lSb, GREEN: %lSb, BLUE: %lSb, ALPHA: %lSb]\n"),
                                UnpackFormat[uiInFormat].wszDescription, 
                                wszIn1, wszIn2, wszIn3, wszIn4,
                                wszRed, wszGreen, wszBlue, wszAlpha);
                    break;
                case 3:
                    _stprintf(tszDbgOutput, _T("IN FORMAT: %lS;  24BPP INPUT: [%lS %lS %lS];  OUTPUT: [RED: %lSb, GREEN: %lSb, BLUE: %lSb, ALPHA: %lSb]\n"),
                                UnpackFormat[uiInFormat].wszDescription, 
                                wszIn1, wszIn2, wszIn3,
                                wszRed, wszGreen, wszBlue, wszAlpha);
                    break;
                case 2:
                    _stprintf(tszDbgOutput, _T("IN FORMAT: %lS;  16BPP INPUT: [%lS %lS];  OUTPUT: [RED: %lSb, GREEN: %lSb, BLUE: %lSb, ALPHA: %lSb]\n"),
                                UnpackFormat[uiInFormat].wszDescription, 
                                wszIn1, wszIn2,
                                wszRed, wszGreen, wszBlue, wszAlpha);
                    break;
                case 1:
                    _stprintf(tszDbgOutput, _T("IN FORMAT: %lS;  8BPP INPUT: [%lS];  OUTPUT: [RED: %lSb, GREEN: %lSb, BLUE: %lSb, ALPHA: %lSb]\n"),
                                UnpackFormat[uiInFormat].wszDescription, 
                                wszIn1,
                                wszRed, wszGreen, wszBlue, wszAlpha);
                    break;
                }

                OutputDebugString(tszDbgOutput);

                // Per component, convert to color depth of destination
                ConvertPrecision(uchRed,   8, &uchConvRed,   UnpackFormat[uiOutFormat].uchRedBits);
                ConvertPrecision(uchGreen, 8, &uchConvGreen, UnpackFormat[uiOutFormat].uchGreenBits);
                ConvertPrecision(uchBlue,  8, &uchConvBlue,  UnpackFormat[uiOutFormat].uchBlueBits);
                ConvertPrecision(uchAlpha, 8, &uchConvAlpha, UnpackFormat[uiOutFormat].uchAlphaBits);

				PackPixel(&UnpackFormat[uiOutFormat], ((VOID*)&dwResult), uchConvRed, uchConvGreen, uchConvBlue, uchConvAlpha);
*/
				ConvertPixelDbg(&UnpackFormat[uiInFormat], &UnpackFormat[uiOutFormat], (BYTE*)&dwValue, (BYTE*)&dwResult,
								   &uchRed, &uchGreen,  &uchBlue,  &uchAlpha, 
								   &uchConvRed, &uchConvGreen,  &uchConvBlue,  &uchConvAlpha);

                GetByteBitString(wszIn1, ((BYTE*)&dwValue)[0]);
                GetByteBitString(wszIn2, ((BYTE*)&dwValue)[1]);
                GetByteBitString(wszIn3, ((BYTE*)&dwValue)[2]);
                GetByteBitString(wszIn4, ((BYTE*)&dwValue)[3]);

                GetByteBitString(wszRed, uchRed);
                GetByteBitString(wszGreen, uchGreen);
                GetByteBitString(wszBlue, uchBlue);
                GetByteBitString(wszAlpha, uchAlpha);

                switch(UnpackFormat[uiInFormat].uiTotalBytes)
                {
                case 4:
                    _stprintf(tszDbgOutput, _T("IN FORMAT: %lS;  32BPP INPUT: [%lS %lS %lS %lS];  OUTPUT: [RED: %lSb, GREEN: %lSb, BLUE: %lSb, ALPHA: %lSb]\n"),
                                UnpackFormat[uiInFormat].wszDescription, 
                                wszIn1, wszIn2, wszIn3, wszIn4,
                                wszRed, wszGreen, wszBlue, wszAlpha);
                    break;
                case 3:
                    _stprintf(tszDbgOutput, _T("IN FORMAT: %lS;  24BPP INPUT: [%lS %lS %lS];  OUTPUT: [RED: %lSb, GREEN: %lSb, BLUE: %lSb, ALPHA: %lSb]\n"),
                                UnpackFormat[uiInFormat].wszDescription, 
                                wszIn1, wszIn2, wszIn3,
                                wszRed, wszGreen, wszBlue, wszAlpha);
                    break;
                case 2:
                    _stprintf(tszDbgOutput, _T("IN FORMAT: %lS;  16BPP INPUT: [%lS %lS];  OUTPUT: [RED: %lSb, GREEN: %lSb, BLUE: %lSb, ALPHA: %lSb]\n"),
                                UnpackFormat[uiInFormat].wszDescription, 
                                wszIn1, wszIn2,
                                wszRed, wszGreen, wszBlue, wszAlpha);
                    break;
                case 1:
                    _stprintf(tszDbgOutput, _T("IN FORMAT: %lS;  8BPP INPUT: [%lS];  OUTPUT: [RED: %lSb, GREEN: %lSb, BLUE: %lSb, ALPHA: %lSb]\n"),
                                UnpackFormat[uiInFormat].wszDescription, 
                                wszIn1,
                                wszRed, wszGreen, wszBlue, wszAlpha);
                    break;
                }

                OutputDebugString(tszDbgOutput);

				GetByteBitString(wszIn1, ((BYTE*)&dwResult)[0]);
				GetByteBitString(wszIn2, ((BYTE*)&dwResult)[1]);
				GetByteBitString(wszIn3, ((BYTE*)&dwResult)[2]);
				GetByteBitString(wszIn4, ((BYTE*)&dwResult)[3]);

                switch(UnpackFormat[uiOutFormat].uiTotalBytes)
                {
                case 4:
                    _stprintf(tszDbgOutput, _T("OUT FORMAT: %lS;  32BPP OUPUT: [%lS %lS %lS %lS]\n"),
                                UnpackFormat[uiOutFormat].wszDescription, 
                                wszIn1, wszIn2, wszIn3, wszIn4);
                    break;
                case 3:
                    _stprintf(tszDbgOutput, _T("OUT FORMAT: %lS;  24BPP OUPUT: [%lS %lS %lS]\n"),
                                UnpackFormat[uiOutFormat].wszDescription, 
                                wszIn1, wszIn2, wszIn3);
                    break;
                case 2:
                    _stprintf(tszDbgOutput, _T("OUT FORMAT: %lS;  16BPP OUPUT: [%lS %lS]\n"),
                                UnpackFormat[uiOutFormat].wszDescription, 
                                wszIn1, wszIn3);
                    break;
                case 1:
                    _stprintf(tszDbgOutput, _T("OUT FORMAT: %lS;  8BPP OUPUT: [%lS]\n"),
                                UnpackFormat[uiOutFormat].wszDescription, 
                                wszIn1);
                    break;
                }

                OutputDebugString(tszDbgOutput);
                OutputDebugString(_T("\n"));
            }
        }
    }
}

#endif // #ifdef D3DQA_BUILD_COLORCONV_UNITTESTS
