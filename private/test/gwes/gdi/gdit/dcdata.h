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
/**************************************************************************


Module Name:

   dcdata.h 

Abstract:

   Code used by region tests

***************************************************************************/

#define NOCARE 777 

int possCaps[][2] = {
   DRIVERVERSION, NOCARE,
   TECHNOLOGY,    DT_RASDISPLAY, 
   HORZSIZE,      NOCARE,    
   VERTSIZE,      NOCARE, 
   HORZRES,       GetSystemMetrics(SM_CXSCREEN),   
   VERTRES,       GetSystemMetrics(SM_CYSCREEN),   
   BITSPIXEL,     NOCARE,
   PLANES,        NOCARE,
   NUMBRUSHES,    NOCARE,
   NUMPENS,       NOCARE,
   NUMMARKERS,    NOCARE,
   NUMFONTS,      NOCARE,
   NUMCOLORS,     NOCARE,
   CURVECAPS,     NOCARE,  // NT error CC_NONE,   
   LINECAPS,      NOCARE,  // NT error LC_NONE,    
   POLYGONALCAPS, NOCARE,  // NT error PC_NONE,    
   TEXTCAPS,      NOCARE,  // NT error TC_CP_STROKE | TC_RA_ABLE, 
   CLIPCAPS,      NOCARE,
   RASTERCAPS,    NOCARE,  // NT error RC_BITBLT | RC_BITMAP64 | RC_PALETTE | RC_BIGFONT, 
   ASPECTX,       NOCARE,
   ASPECTY,       NOCARE,
   ASPECTXY,      NOCARE,
   LOGPIXELSX,    NOCARE,
   LOGPIXELSY,    NOCARE,
   SIZEPALETTE,   NOCARE,
   NUMRESERVED,   NOCARE,
   COLORRES,      NOCARE,
};     


TCHAR *possCapsStr[] = {
   TEXT("DRIVERVERSION"),
   TEXT("TECHNOLOGY"),   
   TEXT("HORZSIZE"),     
   TEXT("VERTSIZE"),     
   TEXT("HORZRES"),      
   TEXT("VERTRES"),      
   TEXT("BITSPIXEL"),    
   TEXT("PLANES"),       
   TEXT("NUMBRUSHES"),   
   TEXT("NUMPENS"),      
   TEXT("NUMMARKERS"),   
   TEXT("NUMFONTS"),     
   TEXT("NUMCOLORS"),    
   TEXT("CURVECAPS"),    
   TEXT("LINECAPS"),     
   TEXT("POLYGONALCAPS"),
   TEXT("TEXTCAPS"),     
   TEXT("CLIPCAPS"),     
   TEXT("RASTERCAPS"),   
   TEXT("ASPECTX"),      
   TEXT("ASPECTY"),      
   TEXT("ASPECTXY"),     
   TEXT("LOGPIXELSX"),   
   TEXT("LOGPIXELSY"),   
   TEXT("SIZEPALETTE"),  
   TEXT("NUMRESERVED"),  
   TEXT("COLORRES"),
}; 





	