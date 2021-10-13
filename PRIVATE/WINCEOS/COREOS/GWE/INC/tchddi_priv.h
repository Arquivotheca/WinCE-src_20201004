//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of the Microsoft
// premium shared source license agreement under which you licensed
// this source code. If you did not accept the terms of the license
// agreement, you are not authorized to use this source code.
// For the terms of the license, please see the license agreement
// signed by you and Microsoft.
// THE SOURCE CODE IS PROVIDED "AS IS", WITH NO WARRANTIES OR INDEMNITIES.
//
/*++
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.

Module Name:

tchddi.h

Abstract:

    This module contains all the definitions for the DDI (Device Driver
    Interface) for the touch panel device.

Notes:


--*/
#pragma once


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
    
    // Offset to an additional property structure which is associated
    // with this specific touch point. The offset is in bytes from
    // the begining of this CETOUCHINPUT structure.
    DWORD dwPropertyOffset;
    
    // Size in bytes of the additional property structure.
    DWORD cbProperty;
} CETOUCHINPUT, *PCETOUCHINPUT;
typedef CETOUCHINPUT const * PCCETOUCHINPUT;


// Touch input flag values (CETOUCHINPUT.dwFlags)

// Specifies that movement occurred. Cannot be combined with TOUCHEVENTF_DOWN.
#define TOUCHEVENTF_MOVE            0x0001

// Specifies that the corresponding touch point was established through a 
// new contact. Cannot be combined with TOUCHEVENTF_MOVE or TOUCHEVENTF_UP.
#define TOUCHEVENTF_DOWN            0x0002

// Specifies that a touch point was removed.
#define TOUCHEVENTF_UP              0x0004

// Specifies that a touch point is in range. Must be set with all 
// TOUCHEVENTF_DOWN and TOUCHEVENTF_MOVE events.
#define TOUCHEVENTF_INRANGE         0x0008

// Specifies that a touch point is designated as primary. Reserved for use
// by the touch proxy.
#define TOUCHEVENTF_PRIMARY         0x0010

// Specifies that this input should not be coalesced in the input stack.
#define TOUCHEVENTF_NOCOALESCE      0x0020

// Specifies that the touch point is associated with a pen contact. 
#define TOUCHEVENTF_PEN             0x0040

// Specifies to the input system that it is not necessary to further 
// calibrate the x and y coordinates of the sample.
#define TOUCHEVENTF_CALIBRATED      0x1000

// Specifies that the touch point is associated with a symmetric Touch 
// H/W controller like the capacitive technology where the driver cannot 
// accurately pair the X's and the Y's recorded to provide the actual X,Y 
// touch contact points. 
#define TOUCHEVENTF_SYMMETRIC       0x2000


// Touch input mask values (CETOUCHINPUT.dwMask)

// The dwTime field contains a system generated value. This mask should only
// be set by the system.
#define TOUCHINPUTMASKF_TIMEFROMSYSTEM  0x0001

// The cxContact and cyContact fields are valid
#define TOUCHINPUTMASKF_CONTACTAREA     0x0004

// The dwPropertyOffset and cbProperty fields are valid
#define TOUCHINPUTMASKF_PROPERTY        0x1000


#define TOUCHEVENTF_VALID           (TOUCHEVENTF_MOVE        | \
                                     TOUCHEVENTF_DOWN        | \
                                     TOUCHEVENTF_UP          | \
                                     TOUCHEVENTF_INRANGE     | \
                                     TOUCHEVENTF_PRIMARY     | \
                                     TOUCHEVENTF_NOCOALESCE  | \
                                     TOUCHEVENTF_PEN         | \
                                     TOUCHEVENTF_SYMMETRIC   | \
                                     TOUCHEVENTF_CALIBRATED)

#define TOUCHEVENTF_STATEMASK       (TOUCHEVENTF_MOVE        | \
                                     TOUCHEVENTF_DOWN        | \
                                     TOUCHEVENTF_UP          | \
                                     TOUCHEVENTF_INRANGE)

#define TOUCHINPUTMASKF_VALID           (TOUCHINPUTMASKF_TIMEFROMSYSTEM | \
                                         TOUCHINPUTMASKF_PROPERTY       | \
                                         TOUCHINPUTMASKF_CONTACTAREA)

#define TOUCHINPUTMASKF_VALIDONINJECT   (TOUCHINPUTMASKF_PROPERTY       | \
                                         TOUCHINPUTMASKF_CONTACTAREA)


// The maximum number of simultaneous touch contacts
#define CETOUCHINPUT_MAX_SIMULTANEOUS     16


// The scaling factor for converting between screen and touch coordinates
#define TOUCH_SCALING_FACTOR              4
 

// enum TouchType - Specifies the overall touch controller type
typedef enum
{
    TOUCHTYPE_NOTOUCH        = 0, // Denotes the absence of a touch h/w
                                  // controller
    TOUCHTYPE_SINGLETOUCH    = 1, // Denotes a touch h/w controller that can
                                  // only provide one x/y sample pair at a
                                  // time, regardless of the number of touch
                                  // contact points
    TOUCHTYPE_SYMMETRICTOUCH = 2, // Denotes a touch h/w controller that
                                  // provides scan-curves for the x and y axis
                                  // separately and hence makes it difficult to
                                  // accurately pair up the obtained X's and
                                  // Y's during multi touch sessions
    TOUCHTYPE_MULTITOUCH     = 3, // Denotes a h/w controller that is able to
                                  // distinctly provide x/y pairs for every
                                  // touch contact point
} TouchType;


