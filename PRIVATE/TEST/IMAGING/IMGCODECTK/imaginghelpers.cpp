//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#include "ImagingHelpers.h"
#include <stdio.h>
using namespace std;
namespace ImagingHelpers
{
    void VerifyImageParameters(const ImageInfo & ii, const CImageDescriptor & id)
    {
        if (ii.RawDataFormat != *(id.GetImageFormat()))
        {
            info(FAIL, _FT("Image returned is incorrect format: expected (%s), got (%s)"), 
                g_mapIMGFMT[*(id.GetImageFormat())], 
                g_mapIMGFMT[ii.RawDataFormat]);
        }
        
        if (ii.PixelFormat != id.GetPixelFormat())
        {
            info(FAIL, _FT("Image returned is incorrect pixel format: expected (%s), got (%s)"),
                g_mapPIXFMT[(LONG)(id.GetPixelFormat())],
                g_mapPIXFMT[(LONG)(ii.PixelFormat)]);
        }
        
        if (ii.Width != id.GetWidth() || ii.Height != id.GetHeight())
        {
            info(FAIL, _FT("Image size is incorrect: expected (%dx%d), got (%dx%d)"),
                id.GetWidth(),
                id.GetHeight(),
                ii.Width,
                ii.Height);
        }

        if (ii.TileWidth > ii.Width || ii.TileHeight > ii.Height)
        {
            info(FAIL, _FT("Image tile size is out of image range: tile (%d, %d), image (%d, %d)"),
                ii.TileWidth, 
                ii.TileHeight,
                ii.Width,
                ii.Height);
        }

        // Due to conversion between himetric and dpi, cannot expect exact number.
        // Allow 0.1% error.
        if (GetErrorPercentage(ii.Xdpi, id.GetXdpi()) > 1 || GetErrorPercentage(ii.Ydpi, id.GetYdpi()) > 1)
        {
            info(FAIL, _FT("Image DPI is incorrect: expected (%lf, %lf), got (%lf, %lf)"),
                id.GetXdpi(),
                id.GetYdpi(),
                ii.Xdpi,
                ii.Ydpi);                
        }
    }

    namespace priv_GetBufferWrappers
    {
        // I'm using these functions for AppVerifier compatibility. I know
        // that GlobalAlloc actually uses LocalAlloc, and I'm guessing 
        // that CoTaskMemAlloc also uses LocalAlloc, so basically I can't
        // figure out which is being used to allocate memory that's being leaked.
        HGLOBAL privGlobalAlloc(UINT uiFlags, UINT uiBytes)
        {
            return GlobalAlloc(uiFlags, uiBytes);
        }

        void * privCoTaskMemAlloc(ULONG cb)
        {
            return CoTaskMemAlloc(cb);
        }

        void * privMapViewOfFile(
            HANDLE hFileMappingObject, 
            DWORD dwDesiredAccess, 
            DWORD dwFileOffsetHigh, 
            DWORD dwFileOffsetLow,
            DWORD dwNumberOfBytesToMap)
        {
            return MapViewOfFile(
                hFileMappingObject,
                dwDesiredAccess,
                dwFileOffsetHigh,
                dwFileOffsetLow,
                dwNumberOfBytesToMap);
        }
    }

    void * GetBufferFromStream(IStream* pStream, AllocType atType, DWORD * pcbBuffer)
    {
        using namespace priv_GetBufferWrappers;
        // pv is the pointer to the buffer.
        void * pv = NULL;

        // stats is used to determine the filesize (i.e. how much memory to allocate)
        STATSTG stats;

        // liOffset is used to seek to the beginning (i.e. reset the stream)
        LARGE_INTEGER liOffset;
        liOffset.QuadPart = 0;

        // temporary handles for the buffers.
        HGLOBAL hGlobal = NULL;
        HANDLE hFileMapping = NULL;

        // Grab the stat to determine the size needed for the buffer.
        pStream->Stat(&stats, STATFLAG_NONAME);
        // Reset the stream
        pStream->Seek(liOffset, SEEK_SET, NULL);
        
        switch(atType)
        {
            case atGLOBAL:
            {
                hGlobal = privGlobalAlloc(GMEM_FIXED, (size_t) stats.cbSize.LowPart);
                pv = GlobalLock(hGlobal);
                if (!pv)
                {
                    info(DETAIL, _FT("Could not allocate buffer, gle=%d"),
                        GetLastError());
                    GlobalFree(hGlobal);
                    return NULL;
                }
                pStream->Read(pv, stats.cbSize.LowPart, NULL);
                GlobalUnlock(hGlobal);
                pv = (void *) hGlobal;
                break;
            }
            case atCTM:
            {
                pv = privCoTaskMemAlloc(stats.cbSize.LowPart);
                if (!pv)
                {
                    info(DETAIL, _FT("Could not allocate buffer, gle=%d"),
                        GetLastError());
                    return NULL;
                }
                pStream->Read(pv, stats.cbSize.LowPart, NULL);
                break;
            }
            case atFM:
            {
                hFileMapping = CreateFileMapping(
                    INVALID_HANDLE_VALUE, 
                    NULL, 
                    PAGE_READWRITE, 
                    stats.cbSize.HighPart, 
                    stats.cbSize.LowPart, 
                    NULL);
                if (!hFileMapping)
                {
                    info(DETAIL, _FT("Could not create file mapping, gle=%d"),
                        GetLastError());
                    return NULL;
                }

                pv = privMapViewOfFile(hFileMapping, FILE_MAP_ALL_ACCESS, 0, 0, 0);
                if (!pv)
                {
                    info(DETAIL, _FT("Could not map view of file, gle=%d"),
                        GetLastError());
                }

                // The CloseHandle and UnmapViewOfFile calls can come in any order.
                // We need to close the file mapping handle here, since otherwise,
                // it would be leaked.
                CloseHandle(hFileMapping);

                pStream->Read(pv, stats.cbSize.LowPart, NULL);
                break;
            }
            default:
                return NULL;
        }
        

        // If the caller wanted the size of the buffer allocated, return it.
        if (pcbBuffer)
        {
            *pcbBuffer = stats.cbSize.LowPart;
        }
        
        return pv;
    }

    // Generate a random integer between iMin and iMax, inclusive: 
    // [iMin, iMax] instead of [iMin, iMax)
    // The maximum range of generated numbers is 2^31 - 1
    int RandomInt(int iMin, int iMax)
    {
        int iRand;
        int iRange;
        iMax++;
        iRange = iMax - iMin;
        if (iRange <= 0) 
            return 0;

        // Since RAND_MAX is 32767 (0x7fff), in order to get a full range we need to
        // do some shifting and oring.
        iRand = rand();
        if (iRange > RAND_MAX)
            iRand |= rand() << 15;
        if (iRange > ((RAND_MAX << 15) | RAND_MAX))
        {
            iRand |= rand() << 30;
            // Can't allow negative numbers.
            iRand &= ~0x80000000;
        }

        iRand %= iRange;
        iRand += iMin;
        return iRand;
    }

    int RandomGaussianInt(int iMin, int iMax, int iCenter, int iIterations)
    {
        int iMaxValid = iMax - iCenter;
        int iTries = 0;
        if (iCenter - iMin > iMaxValid)
        {
            iMaxValid = iCenter - iMin;
        }
        int iRand = 0;
        int iRollSize = 1 + ((iMaxValid) * 2 + iIterations - 1)/ iIterations;

        do 
        {
            iRand = 0;
            for (int iCntr = 0; iCntr < iIterations; iCntr++)
            {
                iRand += RandomInt(0, iRollSize - 1);
            }
            iRand = iRand + iCenter - iMaxValid;
        } while ((iRand < iMin || iRand > iMax) && ++iTries < iIterations);

        if (iTries == iIterations)
        {
            iRand = RandomInt(iMin, iMax);
        }

        return iRand;
    }

    bool IsValidPixelFormat(PixelFormat pf)
    {
        // The map class returns NULL if indexed with an unknown (i.e. invalid) value.
        if (g_mapPIXFMT[pf])
            return true;
        return false;
    }

    bool IsValidInterpHint(InterpolationHint ih)
    {
        if (g_mapINTERP[ih])
            return true;
        return false;
    }

    void CreateRandomGuid(GUID* pguid)
    {
        if (IsBadWritePtr(pguid, sizeof(GUID)))
            return;

        // rand only returns between 0 and 0x7fff, so to get a better guid,
        // I need to do some shifting and oring.
        pguid->Data1 = rand() | (rand() << 15) | (rand() << 30);
        pguid->Data2 = (WORD) (rand() | (rand() << 15));
        pguid->Data3 = (WORD) (rand() | (rand() << 15));
        pguid->Data4[0] = (BYTE) rand();
        pguid->Data4[1] = (BYTE) rand();
        pguid->Data4[2] = (BYTE) rand();
        pguid->Data4[3] = (BYTE) rand();
        pguid->Data4[4] = (BYTE) rand();
        pguid->Data4[5] = (BYTE) rand();
        pguid->Data4[6] = (BYTE) rand();
        pguid->Data4[7] = (BYTE) rand();
        //info(DETAIL, TEXT("Random GUID created: ") _GUIDT, _GUIDEXP(*pguid));
    }

    
    HRESULT ParseGuidList(TCHAR * tszFilename, std::list<const GUID *> & lGuidList, CMapClass mcAvail)
    {
        using namespace std;
        FILE * pFile;
        TCHAR tszLine[128];

        pFile = _tfopen(tszFilename, TEXT("r"));
        if (!pFile)
        {
            return E_FAIL;
        }

        tszLine[1] = 0;
        while (_fgetts(tszLine + 1, countof(tszLine) - 2, pFile))
        {
            const GUID * pguid;
            int iStrLen;

            // ignore commented lines
            if (TEXT('#') == tszLine[1])
                continue;
            tszLine[0] = TEXT('&');
            
            tszLine[countof(tszLine) - 1] = 0;

            iStrLen = _tcslen(tszLine);
            if (TEXT('\n') == tszLine[iStrLen - 1])
            {
                tszLine[iStrLen - 1] = 0;
            }

            pguid = (const GUID *)mcAvail[tszLine];
            if (pguid)
            {
                lGuidList.push_back(pguid);
            }
            
        }
        return S_OK;
    }

    // returns true for equivalent codec info structures.
    bool ImageCodecInfoCompare(const ImageCodecInfo & ici1, const ImageCodecInfo & ici2)
    {
        if (ici1.Clsid != ici2.Clsid)
            return false;
        
        if (ici1.FormatID != ici2.FormatID)
            return false;

        if (wcscmp(ici1.CodecName, ici2.CodecName))
            return false;

        if (wcscmp(ici1.DllName, ici2.DllName))
            return false;

        if (wcscmp(ici1.FormatDescription, ici2.FormatDescription))
            return false;

        if (wcscmp(ici1.FilenameExtension, ici2.FilenameExtension))
            return false;

        if (wcscmp(ici1.MimeType, ici2.MimeType))
            return false;

        if (ici1.Flags != ici2.Flags)
            return false;

        if (ici1.Version != ici2.Version)
            return false;

        // The signature is only valid for a decoder.
        if (ici1.Flags & ImageCodecFlagsDecoder)
        {
            if (ici1.SigCount != ici2.SigCount)
                return false;

            if (ici1.SigSize != ici2.SigSize)
                return false;

            if (memcmp(ici1.SigPattern, ici2.SigPattern, ici1.SigCount * ici1.SigSize))
                return false;

            if (memcmp(ici1.SigMask, ici2.SigMask, ici1.SigCount * ici1.SigSize))
                return false;
        }
        
        return true;
    }

    void DisplayCodecInfo(const ImageCodecInfo & ici)
    {
        info(DETAIL, TEXT("  Clsid:             ") _GUIDT, _GUIDEXP(ici.Clsid));
        info(DETAIL, TEXT("  FormatID:          ") _GUIDT, _GUIDEXP(ici.FormatID));
        info(DETAIL, TEXT("  CodecName:         %ls"), ici.CodecName);
        info(DETAIL, TEXT("  DllName:           %ls"), ici.DllName);
        info(DETAIL, TEXT("  FormatDescription: %ls"), ici.FormatDescription);
        info(DETAIL, TEXT("  FilenameExtension: %ls"), ici.FilenameExtension);
        info(DETAIL, TEXT("  MimeType:          %ls"), ici.MimeType);
        info(DETAIL, TEXT("  Flags:             0x%08x"), ici.Flags);
        info(DETAIL, TEXT("  Version:           %d"), ici.Version);
        info(DETAIL, TEXT("  SigCount:          %d"), ici.SigCount);
        info(DETAIL, TEXT("  SigSize:           %d"), ici.SigSize);
        info(DETAIL, TEXT("  SigPattern, SigMask"));
        for (UINT i = 0; i < ici.SigCount * ici.SigSize; i++)
        {
            info(DETAIL, TEXT("        0x%02x   0x%02x"), ici.SigPattern[i], ici.SigMask[i]);
        }
    }

    double GetErrorPercentage(double dfActual, double dfExpected)
    {
        return fabs(dfActual - dfExpected)/dfExpected * 100.0;
    }

    const double c_dfhmPerInch = 2540;
    
    LONG HiMetricFromPixel(UINT uiPixel, double dfDpi)
    {
        double dfInches;
        LONG lHM;
        dfInches = ((double)uiPixel)/((double)dfDpi);
        lHM = (int) (dfInches * c_dfhmPerInch + 0.5);
        return lHM;
    }
    UINT PixelFromHiMetric(LONG lHM, double dfDpi)
    {
        double dfInches;
        dfInches = ((double)lHM)/(c_dfhmPerInch);
        return (UINT) (dfInches * dfDpi + 0.5);
    }
    
    void GenerateRandomRect(RECT * prcBoundary, RECT * prc)
    {
        if (!prcBoundary || !prc)
            return;

        if (prcBoundary->left >= prcBoundary->right 
            || prcBoundary->top >= prcBoundary->bottom)
        {
            CopyRect(prc, prcBoundary);
            return;
        }
         
        prc->left = RandomInt(prcBoundary->left, prcBoundary->right);
        do {
            prc->right = RandomInt(prcBoundary->left, prcBoundary->right);
        } while(prc->right == prc->left);

        if (prc->right < prc->left)
        {
            prc->right ^= prc->left;
            prc->left  ^= prc->right;
            prc->right ^= prc->left;
        }
        
        prc->top = RandomInt(prcBoundary->top, prcBoundary->bottom);
        do {
            prc->bottom = RandomInt(prcBoundary->top, prcBoundary->bottom);
        } while(prc->bottom == prc->top);

        if (prc->bottom < prc->top)
        {
            prc->bottom ^= prc->top;
            prc->top    ^= prc->bottom;
            prc->bottom ^= prc->top;
        }
    }

    void GenerateRandomInvalidRect(RECT * prcBoundary, RECT * prc)
    {
        RECT rcTemp;
        
        if (!prcBoundary || !prc)
            return;

        GenerateRandomRect(prcBoundary, prc);
        
        do {
            InflateRect(prc, RandomInt(0, 5), RandomInt(0, 5));
            OffsetRect(prc, RandomInt(-20, 20), RandomInt(-20, 20));
            IntersectRect(&rcTemp, prcBoundary, prc);
        } while (EqualRect(&rcTemp, prc) || IsRectEmpty(prc));
    }

    // This function currently assumes 24 bpp bitmap.
    // This function should not be used to open possibly insecure bitmaps,
    // since the parameter validation is minimal at best.
    HBITMAP LoadDIBitmap(const TCHAR * tszName)
    {
        HANDLE hFile = NULL;
        HBITMAP hBmp = NULL;
        BITMAPFILEHEADER bmfh;
        BITMAPINFOHEADER bmi;
        DWORD dwBytesRead;
        BYTE * pbPixelData = NULL;
        bool fSuccess = false;
        hFile = CreateFile(tszName, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (!ReadFile(hFile, (void*)&bmfh, sizeof(bmfh), &dwBytesRead, NULL))
        {
            goto LPFinish;
        }
        if (dwBytesRead != sizeof(bmfh) || bmfh.bfType != 0x4d42)
        {
            goto LPFinish;
        }

        if (!ReadFile(hFile, (void*)&bmi, sizeof(bmi), &dwBytesRead, NULL))
        {   
            goto LPFinish;
        }
        if (dwBytesRead != sizeof(bmi) || bmi.biSize != sizeof(bmi))
        {
            goto LPFinish;
        }
        if (bmi.biBitCount != 24 || bmi.biCompression != BI_RGB)
        {
            goto LPFinish;
        }

        hBmp = CreateDIBSection(NULL, (BITMAPINFO*)&bmi, DIB_RGB_COLORS, (void**) &pbPixelData, NULL, 0);
        if (!hBmp)
        {
            goto LPFinish;
        }

        if (0xffffffff == SetFilePointer(hFile, bmfh.bfOffBits, NULL, FILE_BEGIN) && GetLastError() != NO_ERROR)
        {
            goto LPFinish;
        }

        if (!ReadFile(hFile, (void*) pbPixelData, bmi.biSizeImage, &dwBytesRead, NULL))
        {
            goto LPFinish;
        }

        fSuccess = true;

LPFinish:
        if (hFile)
            CloseHandle(hFile);
        if (!fSuccess && hBmp)
        {
            DeleteObject(hBmp);
            hBmp = NULL;
        }
        return hBmp;
    }

    
    UINT GetValidHeightFromWidth(UINT uiWidth, PixelFormat pf)
    {
        // Calculate a random height that keeps the total image
        // size within available memory resource limits.
        
        // Start with a maximum size of 24 MB, to keep image
        // size within 32MB virtual memory limits.
        int iMaxMem;
        int iStride;
        MEMORYSTATUS ms;
        memset(&ms, 0x00, sizeof(ms));
        ms.dwLength = sizeof(ms);
        GlobalMemoryStatus(&ms);
        iMaxMem = ms.dwAvailPhys - 10 * 4096;
        
        // Set the stride equal to the number of DWORDs needed per pixel, rounding up
        iStride = (uiWidth * GetPixelFormatSize(pf) + 31) / 32;
        // Convert the stride to bytes
        iStride = iStride * 4;
        // Choose height so that total image size is not beyond
        // memory resources.
        return RandomInt(1, iMaxMem / iStride - 1);
    }
    
    HRESULT GetImageCodec(
        IImagingFactory * pImagingFactory, 
        const GUID * pguidImgFmt, 
        const TCHAR * tszMimeType,
        bool fBuiltin, 
        bool fEncoder,
        /*out*/ ImageCodecInfo * pici, 
        /*out*/ tstring & tsExtension)
    {
        typedef pair<UINT, bool> EncoderIndex;
        ImageCodecInfo * pCodecInfo = NULL;
        UINT uiCount = 0;
        int iFoundIndex = -1;
        HRESULT hr;
        bool fFound = false;
        list<EncoderIndex> lstMatchingEncoders;

        if (fEncoder)
            hr = pImagingFactory->GetInstalledEncoders(&uiCount, &pCodecInfo);
        else
            hr = pImagingFactory->GetInstalledDecoders(&uiCount, &pCodecInfo);
        if (FAILED(hr))
        {
            return hr;
        }
    
        // Go through all the encoders returned (or none at all)
        for (UINT uiIndex = 0; uiIndex < uiCount; uiIndex++)
        {
            if ((pguidImgFmt && !memcmp(&(pCodecInfo[uiIndex].FormatID), pguidImgFmt, sizeof(GUID)))
                || (tszMimeType && !_tcscmp(tszMimeType, pCodecInfo[uiIndex].MimeType)))
            {
                // if builtin was wanted, and this is a builtin encoder, choose it.
                if (fBuiltin && (pCodecInfo[uiIndex].Flags & ImageCodecFlagsBuiltin))
                {
                    break;
                }
                // return first non-builtin encoder otherwise
                else if (!fBuiltin && !(pCodecInfo[uiIndex].Flags & ImageCodecFlagsBuiltin))
                {
                    break;
                }
            }
        }
    
        if (uiIndex != uiCount)
        {
            WCHAR wszExtensions[10];
            wcsncpy(wszExtensions, pCodecInfo[uiIndex].FilenameExtension, countof(wszExtensions) - 1);
            wszExtensions[countof(wszExtensions) - 1] = 0;
            WCHAR * wszToken = wcstok(wszExtensions, L";");
            memcpy(pici, &(pCodecInfo[uiIndex]), sizeof(ImageCodecInfo));
            if (wszToken)
            {
                if (L'*' == wszToken[0])
                    wszToken++;
                    
#ifdef UNICODE
                tsExtension = wszToken;
#else
#error Add code to support non-unicode
#endif
            }
            else
            {
                tsExtension = TEXT(".foo");
            }
            fFound = true;
        }
        CoTaskMemFree(pCodecInfo);
    
        if (fFound)
            return S_OK;
        else
            return S_FALSE;
    }

    HRESULT GetSupportedCodecs(
        IImagingFactory* pImagingFactory,
        std::list<const GUID *> & lstDecoders, 
        std::list<const GUID *> & lstEncoders)
    {
        using namespace std;
        UINT uiCodecCount = 0;
        ImageCodecInfo *pCodecInfo = NULL;
        
        if (!pImagingFactory)
            return E_POINTER;

        pImagingFactory->GetInstalledDecoders(&uiCodecCount, &pCodecInfo);
        for (;uiCodecCount; uiCodecCount--)
        {
            // Through the joys of operator overloading, I first get the string
            // identifying the IMGFMT, then use that string to get the GUID*
            TCHAR * tszIMGFMT = g_mapIMGFMT[pCodecInfo[uiCodecCount - 1].FormatID];
            if (tszIMGFMT)
            {
                lstDecoders.push_back((const GUID *)g_mapIMGFMT[tszIMGFMT]);
            }
        }
        if (pCodecInfo)
        {
            CoTaskMemFree(pCodecInfo);
            pCodecInfo = NULL;
        }

        pImagingFactory->GetInstalledEncoders(&uiCodecCount, &pCodecInfo);
        for(; uiCodecCount; uiCodecCount--)
        {
            TCHAR * tszIMGFMT = g_mapIMGFMT[pCodecInfo[uiCodecCount - 1].FormatID];
            if (tszIMGFMT)
            {
                lstEncoders.push_back((const GUID *)g_mapIMGFMT[tszIMGFMT]);
            }
        }
        if (pCodecInfo)
        {
            CoTaskMemFree(pCodecInfo);
            pCodecInfo = NULL;
        }
        return S_OK;
    }

    int GetCorrectStride(UINT uiWidth, PixelFormat pf)
    {
        // First, determine how many bytes are needed for the correct stride.
        // Then round that up to the next DWORD boundary.
        return ((uiWidth * GetPixelFormatSize(pf) + 7) / 8 + 3) & ~3;
    }

    void GetRandomData(void * pvBuffer, int cBytes)
    {
        if (IsBadWritePtr(pvBuffer, cBytes))
        {
            return;
        }
        while(cBytes--)
        {
            ((BYTE*) pvBuffer)[cBytes] = rand();
        }
    }

    HRESULT SetBitmapPixel(UINT uix, UINT uiy, BitmapData * pbmd, unsigned __int64 ui64Value)
    {
        int iBPP = GetPixelFormatSize(pbmd->PixelFormat);
        UINT64 ui64Temp;
        
        if (uiy > pbmd->Height || uix > pbmd->Width)
        {
            return E_INVALIDARG;
        }

        int iBytesPerPixel = (iBPP + 7) / 8;
        int iPixelsPerI64 = 64 / iBPP;
        int iShift = iBPP * ((iPixelsPerI64 - 1) - uix % iPixelsPerI64) % 8;
        UINT64 ui64ValueMask = (((UINT64)1) << iBPP) - 1;
        ui64Temp = 0;
        memcpy((BYTE*)&ui64Temp, ((BYTE*)pbmd->Scan0) + (uiy * pbmd->Stride) + uix * iBPP / 8, iBytesPerPixel);

        ui64Temp &= ~(ui64ValueMask << iShift);
        ui64Temp |= (ui64Value & ui64ValueMask) << iShift;
        memcpy(((BYTE*)pbmd->Scan0) + (uiy * pbmd->Stride) + uix * iBPP / 8, (BYTE*)&ui64Temp, iBytesPerPixel);

        return S_OK;
    }

    HRESULT GetBitmapPixel(UINT uix, UINT uiy, BitmapData * pbmd, unsigned __int64 * pui64Value)
    {
        int iBPP = GetPixelFormatSize(pbmd->PixelFormat);

        if (uiy > pbmd->Height || uix > pbmd->Width)
        {
            return E_INVALIDARG;
        }

        int iBytesPerPixel = (iBPP + 7) / 8;
        int iPixelsPerI64 = 64 / iBPP;
        int iShift = iBPP * ((iPixelsPerI64 - 1) - uix % iPixelsPerI64) % 8;
        UINT64 ui64ValueMask = (((UINT64)1) << iBPP) - 1;
        *pui64Value = 0;
        memcpy((BYTE*)pui64Value, ((BYTE*)pbmd->Scan0) + (uiy * pbmd->Stride) + uix * iBPP / 8, iBytesPerPixel);

        *pui64Value = (*pui64Value >> iShift);
        *pui64Value &= ui64ValueMask;

        return S_OK;
    }
    
    BOOL RecursiveCreateDirectory(tstring tsPath)
    {
        if (!CreateDirectory(tsPath.c_str(), NULL) && GetLastError() == ERROR_PATH_NOT_FOUND)
        {
            if (tsPath.size() == 0)
            {
                return false;
            }
            RecursiveCreateDirectory(tsPath.substr(0, tsPath.rfind(TEXT("\\"))));
        }
        if (CreateDirectory(tsPath.c_str(), NULL) 
            || (GetLastError() == ERROR_FILE_EXISTS
                || GetLastError() == ERROR_ALREADY_EXISTS)
            )
        {
            return true;
        }
        return false;
    }

    HRESULT SaveStreamToFile(IStream * pStream, tstring tsFile)
    {
        tstring tsPath;
        BYTE rgbBuffer[2000];
        ULONG cbRead = 0;
        HANDLE hFile = NULL;
        LARGE_INTEGER liZero = {0};
        tsPath = tsFile.substr(0, tsFile.rfind(TEXT("\\")));
        RecursiveCreateDirectory(tsPath);

        hFile = CreateFile(tsFile.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
        if (INVALID_HANDLE_VALUE == hFile)
        {
            return HRESULT_FROM_WIN32(GetLastError());
        }

        pStream->Seek(liZero, STREAM_SEEK_SET, NULL);

        do
        {
            pStream->Read(rgbBuffer, sizeof(rgbBuffer), &cbRead);
            if (cbRead)
            {
                WriteFile(hFile, rgbBuffer, cbRead, &cbRead, NULL);
            }
        } while (cbRead);
        CloseHandle(hFile);
        return S_OK;
    }

    const int iFULLBRIGHT32 = 255;
    const int iFULLBRIGHT64 = 8191;

    bool GetPixelFormatInformation(
        const PixelFormat     pf,
        unsigned __int64    & i64AlphaMask,
        unsigned __int64    & i64RedMask,
        unsigned __int64    & i64GreenMask,
        unsigned __int64    & i64BlueMask,
        int                 & iAlphaOffset,
        int                 & iRedOffset,
        int                 & iGreenOffset,
        int                 & iBlueOffset,
        int                 & iColorErrorThreshold,
        bool                & fPremult)
    {
        i64AlphaMask = 0;
        i64RedMask = 0;
        i64GreenMask = 0;
        i64BlueMask = 0;
        iAlphaOffset = 0;
        iRedOffset = 0;
        iGreenOffset = 0;
        iBlueOffset = 0;
        iColorErrorThreshold = 0;
        fPremult = 0;
        switch(pf)
        {
        // 32 bpp w/ alpha colors (including the indexed ones)
        case PixelFormat32bppPARGB:
            fPremult = true;
            i64AlphaMask = 0xff000000;
            iAlphaOffset = 24;
            i64RedMask = 0xff0000;
            i64GreenMask = 0xff00;
            i64BlueMask = 0xff;
            iRedOffset = 16;
            iGreenOffset = 8;
            iBlueOffset = 0;
            iColorErrorThreshold = 2;
            break;
        case PixelFormat1bppIndexed:
        case PixelFormat4bppIndexed:
        case PixelFormat8bppIndexed:
        case PixelFormat32bppARGB:
            i64AlphaMask = 0xff000000;
            iAlphaOffset = 24;
            // Fall through to the masks and offsets associated with 32bpp rgb
        case PixelFormat32bppRGB:
            i64RedMask = 0xff0000;
            i64GreenMask = 0xff00;
            i64BlueMask = 0xff;
            iRedOffset = 16;
            iGreenOffset = 8;
            iBlueOffset = 0;
            iColorErrorThreshold = 1;
            break;
        case PixelFormat24bppRGB:
            // What about BGR?
            i64BlueMask = 0xff;
            i64GreenMask = 0xff00;
            i64RedMask = 0xff0000;
            iBlueOffset = 0;
            iGreenOffset = 8;
            iRedOffset = 16;
            iColorErrorThreshold = 1;
            break;
        case PixelFormat16bppARGB1555:
            i64AlphaMask = 0x8000;
            iAlphaOffset = 15;
            // Fall through to the masks and offsets for 16bpp rgb555
        case PixelFormat16bppRGB555:
            i64RedMask = 0x7c00;
            i64GreenMask = 0x3e0;
            i64BlueMask = 0x1f;
            iRedOffset = 10;
            iGreenOffset = 5;
            iBlueOffset = 0;
            iColorErrorThreshold = 2;
            break;
        case PixelFormat16bppRGB565:
            i64RedMask = 0xf800;
            i64GreenMask = 0x7e0;
            i64BlueMask = 0x1f;
            iRedOffset = 11;
            iGreenOffset = 5;
            iBlueOffset = 0;
            iColorErrorThreshold = 2;
            break;
        case PixelFormat64bppPARGB:
            fPremult = true;
        case PixelFormat64bppARGB:
            i64AlphaMask = 0x1fff000000000000;
            iAlphaOffset = 48;
            // Fall through to the masks and offsets for r, g, and b
        case PixelFormat48bppRGB:
            i64RedMask = 0x3fff00000000;
            i64GreenMask = 0x3fff0000;
            i64BlueMask = 0x3fff;
            iRedOffset = 32;
            iGreenOffset = 16;
            iBlueOffset = 0;
            iColorErrorThreshold = 105;
            break;
        default:
            return false;
        }
        return true;
    }

    HRESULT FillBitmapGradient(IBitmapImage* pBitmapImage)
    {
        BitmapData bmd;
        int iRedMax;
        int iGreenMax;
        int iBlueMax;
        int iAlphaMax;
    
        // handle 24 bpp and greater:
        
        unsigned __int64 ui64AlphaMask = 0;
        unsigned __int64 ui64RedMask = 0;
        unsigned __int64 ui64GreenMask = 0;
        unsigned __int64 ui64BlueMask = 0;
        int iAlphaOffset = 0;
        int iRedOffset = 0;
        int iGreenOffset = 0;
        int iBlueOffset = 0;
    
        int iColorErrorThreshold = 0;
    
        bool fPremult = false;
    
        ColorPalette * pcp = NULL;
    
        VerifyHRSuccess(pBitmapImage->LockBits(NULL, ImageLockModeRead, PixelFormatDontCare, &bmd));
        
        GetPixelFormatInformation(
            bmd.PixelFormat,
            ui64AlphaMask,
            ui64RedMask,
            ui64GreenMask,
            ui64BlueMask,
            iAlphaOffset,
            iRedOffset,
            iGreenOffset,
            iBlueOffset,
            iColorErrorThreshold,
            fPremult);
    
        iRedMax = ui64RedMask >> iRedOffset;
        iGreenMax = ui64GreenMask >> iGreenOffset;
        iBlueMax = ui64BlueMask >> iBlueOffset;
        iAlphaMax = ui64AlphaMask >> iAlphaOffset;
    
        if (IsExtendedPixelFormat(bmd.PixelFormat))
        {
            for (int iY = 0; iY < bmd.Height; iY++)
            {
                for (int iX = 0; iX < bmd.Width; iX++)
                {
                    UINT64 ui64Pixel;
                    UINT64 ui64Red = iX * iFULLBRIGHT64 / bmd.Width;
                    UINT64 ui64Green = iY * iFULLBRIGHT64 / bmd.Height;
                    UINT64 ui64Blue = iFULLBRIGHT64 - (iX * iFULLBRIGHT64 / bmd.Width);
                    UINT64 ui64Alpha = ui64AlphaMask >> iAlphaOffset;
                    ui64Pixel = 
                        (ui64Alpha << iAlphaOffset) | 
                        ((ui64Red) << iRedOffset) | 
                        ((ui64Green) << iGreenOffset) | 
                        ((ui64Blue) << iBlueOffset);
                    SetBitmapPixel(iX, iY, &bmd, ui64Pixel);
                }
            }
            VerifyHRSuccess(pBitmapImage->UnlockBits(&bmd));
        }
        else if (!IsIndexedPixelFormat(bmd.PixelFormat))
        {
            for (int iY = 0; iY < bmd.Height; iY++)
            {
                for (int iX = 0; iX < bmd.Width; iX++)
                {
                    UINT64 ui64Pixel;
                    int iRed = iX * iRedMax / bmd.Width;
                    int iGreen = iY * iGreenMax / bmd.Height;
                    int iBlue = iBlueMax - (iX * iBlueMax / bmd.Width);
                    int iAlpha = iAlphaMax;
                    if (fPremult)
                    {
                        iRed = iRed * iAlpha / iAlphaMax;
                        iGreen = iGreen * iAlpha / iAlphaMax;
                        iBlue = iBlue * iAlpha / iAlphaMax;
                    }
                    ui64Pixel = 
                        (iAlpha << iAlphaOffset) | 
                        (iRed << iRedOffset) | 
                        (iGreen << iGreenOffset) | 
                        (iBlue << iBlueOffset);
                    SetBitmapPixel(iX, iY, &bmd, ui64Pixel);
                }
            }
            VerifyHRSuccess(pBitmapImage->UnlockBits(&bmd));
        }
        else if (PixelFormat1bppIndexed == bmd.PixelFormat)
        {
            for (int iY = 0; iY < bmd.Height; iY++)
            {
                for (int iX = 0; iX < bmd.Width; iX++)
                {
                    if ((iX < bmd.Width * 2 / 3 && iY < bmd.Height * 2 / 3) ||
                        (iX >= bmd.Width * 2 / 3 && iY >= bmd.Height * 2 / 3))
                        SetBitmapPixel(iX, iY, &bmd, 0);
                    else
                        SetBitmapPixel(iX, iY, &bmd, 1);
                }
            }
            VerifyHRSuccess(pBitmapImage->UnlockBits(&bmd));
            VerifyHRSuccess(pBitmapImage->GetPalette(&pcp));
            pcp->Entries[0] = MAKEARGB(iFULLBRIGHT32, iFULLBRIGHT32, 0, 0);
            pcp->Entries[1] = MAKEARGB(iFULLBRIGHT32, 0, 0, iFULLBRIGHT32);
            VerifyHRSuccess(pBitmapImage->SetPalette(pcp));
            CoTaskMemFree(pcp);
        }
        else
        {
            for (int iY = 0; iY < bmd.Height; iY++)
            {
                for (int iX = 0; iX < bmd.Width; iX++)
                {
                    if (iX < bmd.Width / 2 && iY < bmd.Height / 2)
                    {
                        SetBitmapPixel(iX, iY, &bmd, 0);
                    }
                    else if (iX < bmd.Width / 2 && iY >= bmd.Height / 2)
                    {
                        SetBitmapPixel(iX, iY, &bmd, 1);
                    }
                    else if (iX >= bmd.Width / 2 && iY < bmd.Height / 2)
                    {
                        SetBitmapPixel(iX, iY, &bmd, 2);
                    }
                    else
                    {
                        SetBitmapPixel(iX, iY, &bmd, 3);
                    }
                }
                
                VerifyHRSuccess(pBitmapImage->GetPalette(&pcp));
                pcp->Entries[0] = MAKEARGB(iFULLBRIGHT32, iFULLBRIGHT32, iFULLBRIGHT32/2, iFULLBRIGHT32/2);
                pcp->Entries[1] = MAKEARGB(iFULLBRIGHT32, iFULLBRIGHT32/2, iFULLBRIGHT32, iFULLBRIGHT32/2);
                pcp->Entries[2] = MAKEARGB(iFULLBRIGHT32, iFULLBRIGHT32/2, iFULLBRIGHT32/2, iFULLBRIGHT32);
                pcp->Entries[3] = MAKEARGB(iFULLBRIGHT32, iFULLBRIGHT32, iFULLBRIGHT32, iFULLBRIGHT32);
                VerifyHRSuccess(pBitmapImage->SetPalette(pcp));
                CoTaskMemFree(pcp);
            }
            VerifyHRSuccess(pBitmapImage->UnlockBits(&bmd));
    
        }
        // iVerify should be set in the debugger if visual verification of 
        // the image is required.
        IImage * pImage;
        HDC hdc;
        RECT rc = {0, 0, bmd.Width, bmd.Height};
        pBitmapImage->QueryInterface(IID_IImage, (void**)&pImage);
        hdc = GetDC(NULL);
        PatBlt(hdc, 0, 0, 640, 480, WHITENESS);
        pImage->Draw(hdc, &rc, NULL);
        pImage->Release();
        ReleaseDC(NULL, hdc);
        return S_OK;
    }

    bool IsDesiredCodecType(IImagingFactory * pImagingFactory, const CImageDescriptor & id)
    {
        if (!g_tsCodecType.empty())
        {
            tstring tsExtension;
            ImageCodecInfo ici; 
            HRESULT hr = S_FALSE;
            if (g_fBuiltinFirst)
            {
                hr = ImagingHelpers::GetImageCodec(
                    pImagingFactory, 
                    NULL, 
                    g_tsCodecType.c_str(), 
                    true,
                    false,
                    &ici,
                    tsExtension);
            }
            if (S_FALSE == hr)
            {
                hr = ImagingHelpers::GetImageCodec(
                    pImagingFactory, 
                    NULL, 
                    g_tsCodecType.c_str(), 
                    false,
                    false,
                    &ici,
                    tsExtension);
            }
            if (S_FALSE == hr)
            {
                hr = ImagingHelpers::GetImageCodec(
                    pImagingFactory, 
                    NULL, 
                    g_tsCodecType.c_str(), 
                    true,
                    false,
                    &ici,
                    tsExtension);
            }
            if (S_FALSE == hr)
            {
                info(ECHO, _T("Ignoring unknown MimeType \"%s\" specified."),
                    g_tsCodecType.c_str());
            }
            else if (memcmp(&(ici.FormatID), id.GetImageFormat(), sizeof(GUID)))
            {
                info(ECHO, _T("Image is not of type \"%s\" specified by user, skipping."),
                    g_tsCodecType.c_str());
                return false;
            }
        }
        return true;
    }
}
