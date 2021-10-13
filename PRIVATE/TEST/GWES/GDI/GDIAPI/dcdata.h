//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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





	