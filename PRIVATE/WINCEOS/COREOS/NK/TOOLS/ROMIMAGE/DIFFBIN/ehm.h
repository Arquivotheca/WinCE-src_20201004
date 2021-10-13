


























#ifndef _EHM_H_
#define _EHM_H_

enum eCodeType
    {
    eHRESULT,
    eBOOL,
    ePOINTER,
    eWINDOWS
    };

// Check HRESULT
#define CHR(hResult)\
    do {hr = (hResult); if(FAILED(hr)) goto Error;} while (0,0)

// Check pointer result
#define CPR(p)\
    do {if(!(0,(p))) {hr = E_OUTOFMEMORY; goto Error;} } while (0,0)

// Check boolean result
#define CBR(fResult)\
    do {if (!(0,(fResult))) {hr = E_FAIL; goto Error;}} while(0,0);

// Check windows result.  Exactly like CBR for the non-Asserting case
#define CWR CBR

// The above macros with Asserts when the condition fails
#ifdef DEBUG

#define CHRA(hResult)\
    do\
        {\
        hr = (hResult);\
        if(FAILED(hr))\
            {\
            if(OnAssertionFail(eHRESULT, hr, TEXT(__FILE__), __LINE__, TEXT("CHRA(") TEXT( # hResult ) TEXT(")")))\
                {\
                DebugBreak();\
                }\
            goto Error;\
            }\
        }\
    while (0,0)

#define CPRA(p)\
    do\
        {\
        if(!(0,(p)))\
            {\
            hr = E_OUTOFMEMORY;\
            if(OnAssertionFail(ePOINTER, 0, TEXT(__FILE__), __LINE__, TEXT("CPRA(") TEXT( # p ) TEXT(")")))\
                {\
                DebugBreak();\
                }\
            goto Error;\
            }\
        }\
    while (0,0)

#define CBRA(fResult)\
    do\
        {\
        if(!(0,(fResult)))\
            {\
            hr = E_FAIL;\
            if(OnAssertionFail(eBOOL, 0, TEXT(__FILE__), __LINE__, TEXT("CBRA(") TEXT( # fResult ) TEXT(")")))\
                {\
                DebugBreak();\
                }\
            goto Error;\
            }\
        }\
    while (0,0)

#define CWRA(fResult)\
    do\
        {\
        if(!(0,(fResult)))\
            {\
            hr = E_FAIL;\
            if(OnAssertionFail(eWINDOWS, 0, TEXT(__FILE__), __LINE__, TEXT("CWRA(") TEXT( # fResult ) TEXT(")")))\
                {\
                DebugBreak();\
                }\
            goto Error;\
            }\
        }\
    while (0,0)


#define VBR(fResult)\
    do\
        {\
        if(!(0,(fResult)))\
            {\
            if(OnAssertionFail(eBOOL, 0, TEXT(__FILE__), __LINE__, TEXT("VBR(") TEXT( # fResult ) TEXT(")")))\
                {\
                DebugBreak();\
                }\
            }\
        }\
    while (0,0)

#define VPR(fResult)\
    do\
        {\
        if(!(0,(fResult)))\
            {\
            if(OnAssertionFail(eBOOL, 0, TEXT(__FILE__), __LINE__, TEXT("VPR(") TEXT( # fResult ) TEXT(")")))\
                {\
                DebugBreak();\
                }\
            }\
        }\
    while (0,0)


// Verify Windows Result
#define VWR(fResult)\
    do\
        {\
        if(!(0,NULL != (fResult)))\
            {\
            if(OnAssertionFail(eWINDOWS, 0, TEXT(__FILE__), __LINE__, TEXT("VWR(") TEXT( # fResult ) TEXT(")")))\
                {\
                DebugBreak();\
                }\
            }\
        }\
    while (0,0)

// Verify HRESULT (careful not to modify hr)
#define VHR(hResult)\
    do\
        {\
        HRESULT _EHM_hrTmp = (hResult);\
        if(FAILED(_EHM_hrTmp))\
            {\
            if(OnAssertionFail(eHRESULT, _EHM_hrTmp, TEXT(__FILE__), __LINE__, TEXT("VHR(") TEXT( # hResult ) TEXT(")")))\
                {\
                DebugBreak();\
                }\
            }\
        }\
    while (0,0)

// make sure you keep the xTmp, can only eval arg 1x
// todo: dump GetLastError in DWR
#define DWR(fResult) \
    do { if(!(0,(fResult))) {DEBUGMSG(1, (TEXT("DWR(") TEXT( # fResult ) TEXT(")\r\n") ));}} while (0,0)
#define DHR(hResult) \
    do { HRESULT hrTmp = hResult; if(FAILED(hrTmp)) {DEBUGMSG(1, (TEXT("DHR(") TEXT( # hResult ) TEXT(")=0x%x\r\n"), hrTmp));}} while (0,0)
#define DPR     DWR     // tmp
#define DBR     DWR     // tmp
#else
#define CHRA CHR
#define CPRA CPR
#define CBRA CBR
#define CWRA CWR
#define VHR(x) (x)
#define VPR(x) (x)
#define VBR(x) (x)
#define VWR(x) (x)
#define DHR(x) (x)
#define DPR(x) (x)
#define DBR(x) (x)
#define DWR(x) (x)
#endif

// - work around various pseudo-pblms:
//  partial images, CEPC no-radio, etc.
// - also things that we want to know about in dev, but not in QA/stress:
//  an e.g. is our DoVerb stuff, there are 'good' failures (END when no call,
// or TALK w/ 0 entries) and 'bad' failures (e.g. TAPI returns an error), we
// don't want to int3 during stress but we do want to on our dev machines
//
// eventually (soon!) we'll make these do "if (g_Assert) int3;", then we
// can run w/ g_Assert on for dev and off for stress.
#define xCHRA   CHR     // should be CHRA but...
#define xCPRA   CPR     // should be CPRA but...
#define xCBRA   CBR     // should be CBRA but...
#define xCWRA   CWR     // should be CWRA but...
#define xVHR    DHR     // should be VHR  but...
#define xVPR    DPR     // should be VPR  but...
#define xVBR    DBR     // should be VBR  but...
#define xVWR    DWR     // should be VWR  but...

#if 0
// work around things we're not sure about (todo: review/fix all callers)
// an e.g. is our DoVerb stuff, the ASSERTs are too tight right now but
// we don't want to lose them entirely
#define qCHRA   CHR     // should be CHRA but...
#define qCPRA   CPR     // should be CBRA but...
#define qCBRA   CBR     // should be CBRA but...
#define qCWRA   CWR     // should be CWRA but...
#define qVHR    DHR     // should be VHR  but...
#define qVPR    DPR     // should be VPR  but...
#define qVBR    DBR     // should be VBR  but...
#define qVWR    DWR     // should be VWR  but...
#endif

#endif // _EHM_H_


