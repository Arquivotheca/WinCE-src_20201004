//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this sample source code is subject to the terms of the Microsoft
// license agreement under which you licensed this sample source code. If
// you did not accept the terms of the license agreement, you are not
// authorized to use this sample source code. For the terms of the license,
// please see the license agreement between you and Microsoft or, if applicable,
// see the LICENSE.RTF on your install media or the root of your tools installation.
// THE SAMPLE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//

#ifndef __GWETOUCH_H__
#define __GWETOUCH_H__

#ifdef __cplusplus
extern "C" {
#endif


// Touch Input defines
typedef struct {
    // Specifies the x coordinate of the touch point in 4ths of a pixel.
    LONG x;
    
    // Specifies the y coordinate of the touch point in 4ths of a pixel.
    LONG y;
    
    // Identifier of the touch input source. Reserved for use by the touch proxy.
    HANDLE hSource;
    
    // Touch point identifier - this is the touch contact index. This ID must be
    // maintained for a contact from the time it goes down until the time it 
    // goes up.
    DWORD dwID;
    
    // A set of bit flags that specify various aspects of touch point press / 
    // release and motion.
    DWORD dwFlags;
    
    // A set of bit flags that specify which of the optional fields in the 
    // structure contain valid values.
    DWORD dwMask;
    
    // Time stamp for the event, in milliseconds. If this parameter is 0, 
    // the sample will be timestamped by GWES enroute to the gesture engine 
    // and the TOUCHEVENTMASKF_TIMEFROMSYSTEM flag will be set in dwMask. 
    DWORD dwTime;
    
    // Specifies the width of the touch contact area in 4ths of a pixel.
    DWORD cxContact;
    
    // Specifies the height of the touch contact area in 4ths of a pixel.
    DWORD cyContact;

    // Specifies the parent window where this touch input occurred.
    HWND hwndOwner;

    // Specifies the window-relative x coordinate of the touch point.
    LONG xWindow;

    // Specifies the window-relative y coordinate of the touch point.
    LONG yWindow;

} GWETOUCHINPUT, *PGWETOUCHINPUT;
typedef GWETOUCHINPUT const * PCGWETOUCHINPUT;


// The maximum number of simultaneous touch contacts for GWE
#define GWETOUCHINPUT_MAX_SIMULTANEOUS 16

#ifdef CETOUCHINPUT_MAX_SIMULTANEOUS
#if GWETOUCHINPUT_MAX_SIMULTANEOUS < CETOUCHINPUT_MAX_SIMULTANEOUS
#error GWETOUCHINPUT_MAX_SIMULTANEOUS is less than CETOUCHINPUT_MAX_SIMULTANEOUS
#endif
#endif


typedef struct {
    DWORD cInputs;          // Count of simultaneous touch points
    PGWETOUCHINPUT pInputs; // Set of simultaneous touch points
} GWE_TOUCH_SAMPLE_SET;


#ifdef __cplusplus
}
#endif
    
#endif
