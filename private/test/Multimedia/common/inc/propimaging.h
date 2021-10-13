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

#define FACILITY_IMAGING        0x87b
#define MAKE_IMGERR(n)          MAKE_HRESULT(SEVERITY_ERROR, FACILITY_IMAGING, n)
#define IMGERR_OBJECTBUSY       MAKE_IMGERR(1)
#define IMGERR_NOPALETTE        MAKE_IMGERR(2)
#define IMGERR_BADLOCK          MAKE_IMGERR(3)
#define IMGERR_BADUNLOCK        MAKE_IMGERR(4)
#define IMGERR_NOCONVERSION     MAKE_IMGERR(5)
#define IMGERR_CODECNOTFOUND    MAKE_IMGERR(6)
#define IMGERR_NOFRAME          MAKE_IMGERR(7)
#define IMGERR_ABORT            MAKE_IMGERR(8)
#define IMGERR_FAILLOADCODEC    MAKE_IMGERR(9)
#define IMGERR_PROPERTYNOTFOUND MAKE_IMGERR(10)
#define IMGERR_PROPERTYNOTSUPPORTED MAKE_IMGERR(11)

// Imaging Property constants in Imaging.h that are not in GDIPlusImaging.h
#define PropertyTagGpsProcessingMethod			0x001B
#define PropertyTagGpsAreaInformation			0x001C
#define PropertyTagGpsDateStamp					0x001D
#define PropertyTagGpsDifferential				0x001E

// PSeudo ID values for property values returned by WIC from the JFIF segment
#define JFIFVersion					0x00010000
#define JFIFUnits					0x00010001
#define JFIFXDensity				0x00010002
#define JFIFYDensity				0x00010003
#define JFIFXThumbnail				0x00010004
#define JFIFYThumbnail				0x00010005
#define JFIFThumbnailData			0x00010006

// Names for fixed properties (Derived, Test, or accessed via specific WIC function calls)
#define PROPNAME_PIXELFORMAT	_T("PixelFormat")
#define PROPNAME_WIDTH			_T("Width")
#define PROPNAME_HEIGHT			_T("Height")
#define PROPNAME_FILESIZE		_T("FileSize")
#define PROPNAME_DPIX			_T("DPIX")
#define PROPNAME_DPIY			_T("DPIY")
#define PROPNAME_IMAGEFORMAT	_T("ImageFormat")
#define PROPNAME_MEDIATYPE		_T("MediaType")
#define PROPNAME_MEDIASUBTYPE	_T("MediaSubType")
#define PROPNAME_UNKNOWN		_T("Unknown")
#define PROPNAME_UNCPATH		_T("UNCPath")
#define PROPNAME_FILENAME		_T("FileName")
#define PROPNAME_MIMETYPE		_T("MimeType")
#define PROPNAME_THRESHOLD      _T("Threshold")
#define PROPNAME_AVGTHRESHOLD   _T("AvgThreshold")
#define PROPNAME_INVALIDIMAGE _T("InvalidImage")
#define PROPNAME_REFERENCEBMP _T("ReferenceBMP")
#define PROPNAME_PIXELFORMATGDI	_T("PixelFormatGDI")
