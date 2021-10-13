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
#ifndef __GSE_H
#define __GSE_H

#if __cplusplus
extern "C" {
#endif


// GestureType
// Enumeration with all available gestures
enum enumGestureType {
    GESTURETYPE_TAP = 0,
    GESTURETYPE_PRESSANDHOLD,
    GESTURETYPE_DBLTAP,
    GESTURETYPE_PAN,
    GESTURETYPE_FLICK,
    GESTURETYPE_PINCHSTRETCH,

    GESTURETYPE__LAST            // this should always be the last item!
};
typedef enum enumGestureType GestureType;


// GestureClass
// Defines the class of the gesture
// Should have one entry for each new class. A class can
// tipically asssociated with a different input driver
enum enumGestureClass {
    GESTURECLASS_TOUCH = 0,

    GESTURECLASS__LAST          // this should always be the last item!
};
typedef enum enumGestureClass GestureClass;

struct structGestureDataPoint {
    POINT point;                        // point in screen coordinates
    ULONG lParam1;                      // extra data for this particular point
    ULONG lParam2;                      // additional extra data
    UINT timeBetweenCurrAndNextPoint;   // time it should take 
                                        // getting from this point to the next,
                                        // in milliseconds  only relevant for multi
                                        // point scenarios, 0 for all other cases
};
typedef struct structGestureDataPoint GestureDataPoint;



// GestureDataMultiPoint
// This structure allows sending gestures which require several simultaneous
// streams. A good example is complex multi-touch gestures
struct structGestureDataMultiPoint {
    UINT nPoints;                // Number of points passed on the points array
    GestureDataPoint* points;   // Points array
};
typedef struct structGestureDataMultiPoint GestureDataMultiPoint;


// OnSendCallback function prototype
// The interpretation of each param will be down to the specialized class
typedef HRESULT (CALLBACK *ONSENDCALLBACK)(LONG lParam1, LONG lParam2, LONG lParam3);
/// C# sample for calling back:
/// delegate int OnSendCallbackDelegate(long lParam1, long lParam2, long lParam3);
/// int OnSendCallback(long lParam1, long lParam2, long lParam3) {}
/// DoGestureWithCallback(..., OnSendCallback)


///=============================================================================
/// Exported function type prototypes
///=============================================================================
typedef HRESULT (__cdecl *SENDRANDOMGESTUREPROC)(void); 
typedef HRESULT (__cdecl *DOGESTUREPROC)(GestureType gestureType, BYTE nGestureDataCount, GestureDataPoint* pGestureData);
typedef HRESULT (__cdecl *DOGESTUREPROCWITHCALLBACK)(GestureType gestureType, BYTE nGestureDataCount, GestureDataPoint* pGestureData, ONSENDCALLBACK fOnSendCallback);
typedef HRESULT (__cdecl *DOFREEFORMGESTUREPROC)(BYTE nGestureDataCount, GestureDataMultiPoint* pGestureData, BYTE bInterpolationMethod);
typedef HRESULT (__cdecl *GESTURECHAINSTARTPROC)(void);
typedef HRESULT (__cdecl *GESTURECHAINADDDELAYPROC)(UINT nDelay);
typedef HRESULT (__cdecl *GESTURECHAINENDPROC)(BOOL bAsync);

typedef HRESULT (__cdecl *FREEFORMSESSIONINITIALIZE)(void);
typedef HRESULT (__cdecl *FREEFORMSESSIONSEND)(void);
typedef HRESULT (__cdecl *FREEFORMSESSIONSENDWITHCALLBACK)(ONSENDCALLBACK fOnSendCallback);
typedef HRESULT (__cdecl *FREEFORMSESSIONCLOSE)(void);
typedef HRESULT (__cdecl *FREEFORMSTREAMCREATE)(OUT DWORD *pdwStreamID, int xPos, int yPos);
typedef HRESULT (__cdecl *FREEFORMSTREAMADDSEGMENT)(DWORD dwStreamID, DWORD dwDurationMs, int xPos, int yPos);
typedef HRESULT (__cdecl *FREEFORMSTREAMADDGAP)(DWORD dwStreamID, DWORD dwDurationMs, int xPos, int yPos);


// GestureChainEntry
// Used as a node for a list (chain) of gestures
struct tagGestureChainEntry {
    void *samples;                  // Pointer to list of driver samples for this gesture
    ULONG lNumSamples;              // The number of samples in the array
    UINT delay;                     // Delay between this gesture and the next one
    GestureClass gestureClass;      // Class of gesture
    tagGestureChainEntry *next;     // Pointer to the next entry on the list
};
typedef struct tagGestureChainEntry GestureChainEntry;

// GestureClassTypeMapping
// Defines the mapping between a given gesture and it's class
__declspec(selectany) int GestureClassTypeMapping[][2] = {
    {GESTURECLASS_TOUCH, GESTURETYPE_TAP},
    {GESTURECLASS_TOUCH, GESTURETYPE_PRESSANDHOLD},
    {GESTURECLASS_TOUCH, GESTURETYPE_DBLTAP},
    {GESTURECLASS_TOUCH, GESTURETYPE_PAN},
    {GESTURECLASS_TOUCH, GESTURETYPE_FLICK},

    {GESTURECLASS__LAST, 0}
};


#if __cplusplus
}
#endif

#endif