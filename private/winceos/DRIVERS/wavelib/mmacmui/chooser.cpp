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
#include "acmi.h"
#include "msacmdlg.h"
#include "chooseri.h"
#include "wavelib.h"

enum { ChooseCancel = 0,
       ChooseOk,
       ChooseSubFailure,
       ChooseNoMem };
//
//
//
#define istspace iswspace

//
//  to quickly hack around overlapping and unimaginative defines..
//
#define IDD_CMB_FORMATTAG   IDD_ACMFORMATCHOOSE_CMB_FORMATTAG
#define IDD_CMB_FORMAT      IDD_ACMFORMATCHOOSE_CMB_FORMAT

/*
 * Function Declarations
 */
BOOL FNWCALLBACK NewSndDlgProc(HWND hwnd,
                            unsigned msg,
                            WPARAM wParam,
                            LPARAM lParam);

BOOL FNWCALLBACK NewNameDlgProc(HWND hwnd,
                             unsigned msg,
                             WPARAM wParam,
                             LPARAM lParam);

void FNLOCAL SetName(PInstData pInst);
void FNLOCAL DelName(PInstData pInst);

PNameStore FNLOCAL NewNameStore(UINT cchLen);

LRESULT FNLOCAL InitDialog(HWND hwnd, HWND hwndFocus, LPARAM lParam);
BOOL FNWCALLBACK FormatTagsCallback(HACMDRIVERID hadid,
                                      LPACMFORMATDETAILS paftd,
                                      DWORD dwInstance,
                                      DWORD fdwSupport);

BOOL FNWCALLBACK FormatTagsCallbackSimple
(
    HACMDRIVERID            hadid,
    LPACMFORMATTAGDETAILS   paftd,
    DWORD                   dwInstance,
    DWORD                   fdwSupport
);


BOOL FNWCALLBACK FormatsCallback(HACMDRIVERID hadid,
                                   LPACMFORMATDETAILS pafd,
                                   DWORD dwInstance,
                                   DWORD fdwSupport);

BOOL FNWCALLBACK FilterTagsCallback(HACMDRIVERID hadid,
                                      LPACMFILTERTAGDETAILS paftd,
                                      DWORD dwInstance,
                                      DWORD fdwSupport);

BOOL FNWCALLBACK FiltersCallback(HACMDRIVERID hadid,
                                   LPACMFILTERDETAILS pafd,
                                   DWORD dwInstance,
                                   DWORD fdwSupport);


MMRESULT FNLOCAL RefreshFormatTags(PInstData pInst);
void FNLOCAL RefreshFormats(PInstData pInst);
void FNLOCAL EmptyFormats(PInstData pInst);

PInstData FNLOCAL NewInstance(LPBYTE pbChoose,UINT uType);

LPBYTE FNLOCAL CopyStruct(LPBYTE lpDest,
                       LPBYTE lpByte, UINT uType);

void FNLOCAL SelectFormatTag(PInstData pInst);
void FNLOCAL SelectFormat(PInstData pInst);

BOOL FNLOCAL FindFormat(PInstData pInst,LPWAVEFORMATEX lpwfx,BOOL fExact);
BOOL FNLOCAL FindFilter(PInstData pInst,LPWAVEFILTER lpwf,BOOL fExact);

void FNLOCAL MashNameWithRate(PInstData pInst,
                              PNameStore pnsDest,
                              PNameStore pnsSrc,
                              LPWAVEFORMATEX pwfx);

void FNLOCAL TagUnavailable(PInstData pInst);


BOOL g_bAcmuiInitialized=FALSE;
BOOL acmui_Initialize(VOID)
{
    g_bAcmuiInitialized=TRUE;
    return TRUE;
}

BOOL acmui_Terminate(VOID)
{
    if (g_bAcmuiInitialized)
        {
        waveui_DeInit();
        g_bAcmuiInitialized=FALSE;
        }
    return TRUE;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
PInstData FNLOCAL
NewInstance(
    LPBYTE pbChoose,
    UINT uType
    )
{
    PInstData   pInst = NULL;

    if (!g_bAcmuiInitialized)
        {
        waveui_Init();
        g_bAcmuiInitialized=TRUE;
        }

    FUNC("NewInstance");
    pInst = (PInstData)LocalAlloc(LPTR,sizeof(InstData));
    if (!pInst)
        goto EXIT;

    pInst->pag = pag;

    pInst->pnsTemp = NewNameStore(STRING_LEN);
    if (!pInst->pnsTemp)
        goto exitfail;

    pInst->pnsStrOut = NewNameStore(STRING_LEN);
    if (!pInst->pnsStrOut)
    {
        DeleteNameStore(pInst->pnsTemp);
        goto exitfail;
    }

    switch (uType)
    {
        case FORMAT_CHOOSE:
            pInst->pfmtc = (PACMFORMATCHOOSE_INT)pbChoose;
            pInst->pszName = pInst->pfmtc->pszName;
            pInst->cchName = pInst->pfmtc->cchName;
            pInst->fEnableHook = (pInst->pfmtc->fdwStyle &
                                  ACMFORMATCHOOSE_STYLEF_ENABLEHOOK) != 0;
            pInst->pfnHook = pInst->pfmtc->pfnHook;

            break;
        case FILTER_CHOOSE:
            pInst->pafltrc = (PACMFILTERCHOOSE_INT)pbChoose;
            pInst->pszName = pInst->pafltrc->pszName;
            pInst->cchName = pInst->pafltrc->cchName;
            pInst->fEnableHook = (pInst->pafltrc->fdwStyle &
                                  ACMFILTERCHOOSE_STYLEF_ENABLEHOOK) != 0;
            pInst->pfnHook = pInst->pafltrc->pfnHook;

            break;

        default:
            goto exitfail;
    }

    pInst->mmrSubFailure = MMSYSERR_NOERROR;
    pInst->uType = uType;
    goto EXIT;

exitfail:
    LocalFree((HLOCAL)pInst);
    pInst = NULL;

EXIT:
    FUNC_EXIT();
    return (pInst);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void FNLOCAL
DeleteInstance ( PInstData pInst )
{
    FUNC("DeleteInstance");

    DeleteNameStore(pInst->pnsTemp);
    DeleteNameStore(pInst->pnsStrOut);
    LocalFree((HLOCAL)pInst);

    FUNC_EXIT();
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
LPBYTE FNLOCAL
CopyStruct ( LPBYTE     lpDest,
             LPBYTE     lpSrc,
             UINT       uType )
{
    LPBYTE      lpBuffer;
    DWORD       cbSize;

    if (!lpSrc)
        return (NULL);

    switch (uType)
    {
        case FORMAT_CHOOSE:
        {
            LPWAVEFORMATEX lpwfx = (LPWAVEFORMATEX)lpSrc;
            cbSize = SIZEOF_WAVEFORMATEX(lpwfx);
            // Guard against error
            if (cbSize==0)
            {
                return NULL;
            }
            break;
        }
        case FILTER_CHOOSE:
        {
            LPWAVEFILTER lpwf = (LPWAVEFILTER)lpSrc;
            cbSize = lpwf->cbStruct;
            break;
        }

        default:
        {
            return NULL;
        }
    }

    if (lpDest)
    {
        lpBuffer = (LPBYTE)LocalReAlloc(lpDest,cbSize,LMEM_MOVEABLE);
    }
    else
    {
        lpBuffer = (LPBYTE)LocalAlloc(LPTR,cbSize);
    }

    if (!lpBuffer)
        return (NULL);

    _fmemcpy(lpBuffer, lpSrc, (UINT)cbSize);
    return (lpBuffer);
}




//------------------------------------------------------------------------------
//  @doc INTERNAL ACM_API
//
//  @func PNameStore FNLOCAL | NewNameStore | Allocates a sized string buffer
//
//  @parm UINT | cchLen | Maximum number of characters in string (inc. NULL)
//
//------------------------------------------------------------------------------
PNameStore FNLOCAL
NewNameStore ( UINT cchLen )
{
    UINT        cbSize;
    PNameStore  pName = NULL;

    FUNC("NewNameStore");

    // Only allow allocations up to STRING_LEN (which is currently the only thing we're ever asked for).
    if (cchLen <= STRING_LEN)
        {
        cbSize = cchLen*sizeof(TCHAR) + sizeof(NameStore);

        pName = (PNameStore)LocalAlloc(LPTR,cbSize);
        if (pName)
            pName->cbSize = cbSize;
        }

    FUNC_EXIT();
    return (pName);
}



//------------------------------------------------------------------------------
//
//  @doc    EXTERNAL ACM_API_STRUCTURE
//
//  @struct ACMFILTERCHOOSE | The <t ACMFILTERCHOOSE> structure contains
//          information the Audio Compression Manager (ACM) uses to initialize
//          the system-defined wave filter selection dialog box. After the
//          user closes the dialog box, the system returns information about
//          the user's selection in this structure.
//
//  @field  DWORD | cbStruct | Specifies the size in bytes of the
//          <t ACMFILTERCHOOSE> structure. This member must be initialized
//          before calling the <f acmFilterChoose> function. The size specified
//          in this member must be large enough to contain the base
//          <t ACMFILTERCHOOSE> structure.
//
//  @field  DWORD | fdwStyle | Specifies optional style flags for the
//          <f acmFilterChoose> function. This member must be initialized to
//          a valid combination of the following flags before calling the
//          <f acmFilterChoose> function.
//
//          @flag ACMFILTERCHOOSE_STYLEF_ENABLEHOOK |
//          NOTE! : This flag is invalid for Windows CE.
//
//          @flag ACMFILTERCHOOSE_STYLEF_ENABLETEMPLATE |
//          NOTE! : This flag is invalid for Windows CE.
//
//          @flag ACMFILTERCHOOSE_STYLEF_ENABLETEMPLATEHANDLE |
//          NOTE! : This flag is invalid for Windows CE.
//
//          @flag ACMFILTERCHOOSE_STYLEF_INITTOFILTERSTRUCT | Indicates that the
//          buffer pointed to by <e ACMFILTERCHOOSE.pwfltr> contains a valid
//          <t WAVEFILTER> structure that the dialog will use as the initial
//          selection.
//
//          @flag ACMFILTERCHOOSE_STYLEF_SHOWHELP |
//          NOTE! : This flag is invalid for Windows CE.
//
//  @field  HWND | hwndOwner | Identifies the window that owns the dialog
//          box. This member can be any valid window handle, or NULL if the
//          dialog box has no owner. This member must be initialized before
//          calling the <f acmFilterChoose> function.
//
//  @field  LPWAVEFILTER | pwfltr | Specifies a pointer to a <t WAVEFILTER>
//          structure. If the ACMFILTERCHOOSE_STYLEF_INITTOFILTERSTRUCT flag is
//          specified in the <e ACMFILTERCHOOSE.fdwStyle> member, then this
//          structure must be initialized to a valid filter. When the
//          <f acmFilterChoose> function returns, this buffer contains the
//          selected filter. If the user cancels the dialog, no changes will
//          be made to this buffer.
//
//  @field  DWORD | cbwfltr | Specifies the size in bytes of the buffer pointed
//          to by the <e ACMFILTERCHOOSE.pwfltr> member. The <f acmFilterChoose>
//          function returns ACMERR_NOTPOSSIBLE if the buffer is too small to
//          contain the filter information; also, the ACM copies the required size
//          into this member. An application can use the <f acmMetrics> and
//          <f acmFilterTagDetails> functions to determine the largest size
//          required for this buffer.
//
//  @field  LPCSTR | pszTitle | Points to a string to be placed in the title
//          bar of the dialog box. If this member is NULL, the ACM uses the
//          default title (that is, "Filter Selection").
//
//  @field  char | szFilterTag[ACMFILTERTAGDETAILS_FILTERTAG_CHARS] |
//          When the <f acmFilterChoose> function returns, this buffer contains
//          a NULL-terminated string describing the filter tag of the filter
//          selection. This string is equivalent to the
//          <e ACMFILTERTAGDETAILS.szFilterTag> member of the <t ACMFILTERTAGDETAILS>
//          structure returned by <f acmFilterTagDetails>. If the user cancels
//          the dialog, this member will contain a NULL string.
//
//  @field  char | szFilter[ACMFILTERDETAILS_FILTER_CHARS] | When the
//          <f acmFilterChoose> function returns, this buffer contains a
//          NULL-terminated string describing the filter attributes of the
//          filter selection. This string is equivalent to the
//          <e ACMFILTERDETAILS.szFilter> member of the <t ACMFILTERDETAILS>
//          structure returned by <f acmFilterDetails>. If the user cancels
//          the dialog, this member will contain a NULL string.
//
//  @field  LPSTR | pszName |
//          NOTE! : This parameter is ignored in Windows CE.
//
//  @field  DWORD | cchName |
//          NOTE! : This parameter is ignored in Windows CE.
//
//  @field  DWORD | fdwEnum | Specifies optional flags for restricting the
//          type of filters listed in the dialog. These flags are identical to
//          the <p fdwEnum> flags for the <f acmFilterEnum> function. This
//          member should be zero if <e ACMFILTERCHOOSE.pwfltrEnum> is NULL.
//
//          @flag ACM_FILTERENUMF_DWFILTERTAG | Specifies that the
//          <e WAVEFILTER.dwFilterTag> member of the <t WAVEFILTER> structure
//          referred to by the <e ACMFILTERCHOOSE.pwfltrEnum> member is valid. The
//          enumerator will only enumerate a filter that conforms to this
//          attribute.
//
//  @field  LPWAVEFILTER | pwfltrEnum | Points to a <t WAVEFILTER> structure
//          that will be used to restrict the filters listed in the dialog. The
//          <e ACMFILTERCHOOSE.fdwEnum> member defines which fields from the
//          <e ACMFILTERCHOOSE.pwfltrEnum> structure should be used for the
//          enumeration restrictions. The <e WAVEFILTER.cbStruct> member of this
//          <t WAVEFILTER> structure must be initialized to the size of the
//          <t WAVEFILTER> structure. This member can be NULL if no special
//          restrictions are desired.
//
//  @field  HINSTANCE | hInstance |
//          NOTE! : This parameter is ignored in Windows CE.
//
//  @field  LPCSTR | pszTemplateName |
//          NOTE! : This parameter is ignored in Windows CE.
//
//  @field  LPARAM | lCustData |
//          NOTE! : This parameter is ignored in Windows CE.
//
//  @field  ACMFILTERCHOOSEHOOKPROC | pfnHook |
//          NOTE! : This parameter is ignored in Windows CE.
//
//  @xref   <f acmFilterChoose> <f acmMetrics>
//          <f acmFilterTagDetails> <f acmFilterDetails> <f acmFilterEnum>
//          <f acmFormatChoose>
//
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//  @doc    EXTERNAL ACM_API
//
//  @func   MMRESULT | acmFilterChoose | The <f acmFilterChoose> function creates
//          an Audio Compression Manager (ACM) defined dialog box that enables
//          the user to select a wave filter.
//
//  @parm   LPACMFILTERCHOOSE | pafltrc | Points to an <t ACMFILTERCHOOSE>
//          structure that contains information used to initialize the dialog
//          box. When <f acmFilterChoose> returns, this structure contains
//          information about the user's filter selection.
//
//  @rdesc  Returns <c MMSYSERR_NOERROR> if the function was successful.
//          Otherwise, it returns an error value. Possible error returns are:
//
//          @flag MMSYSERR_INVALHANDLE | Specified handle is invalid.
//
//          @flag MMSYSERR_NODRIVER | A suitable driver is not available to
//          provide valid filter selections.
//
//          @flag MMSYSERR_INVALFLAG | One or more flags are invalid.
//
//          @flag MMSYSERR_INVALPARAM | One or more arguments passed are invalid.
//
//          @flag ACMERR_NOTPOSSIBLE | The buffer identified by the
//          <e ACMFILTERCHOOSE.pwfltr> member of the <t ACMFILTERCHOOSE> structure
//          is too small to contain the selected filter.
//
//          @flag ACMERR_CANCELED | The user chose the Cancel button or the
//          Close command on the System menu to close the dialog box.
//
//  @comm   The <e ACMFILTERCHOOSE.pwfltr> member must be filled in with a valid
//          pointer to a memory location that will contain the returned filter
//          header structure. Moreover, the <e ACMFILTERCHOOSE.cbwfltr> member must
//          be filled in with the size in bytes of this memory buffer.
//
//  @xref   <t ACMFILTERCHOOSE> <f acmFormatChoose>
//
//------------------------------------------------------------------------------
MMRESULT ACMAPI
acmFilterChoose(
    LPACMFILTERCHOOSE pafltrc
    )
{
    int         iRet;
    PInstData   pInst = NULL;
    LPCTSTR     lpDlgTemplate = MAKEINTRESOURCE(DLG_ACMFILTERCHOOSE_ID);
    HINSTANCE   hInstance = NULL;
    MMRESULT    mmRet = MMSYSERR_NOERROR;
    UINT        cbwfltrEnum;
    ACMFILTERCHOOSE_INT afc;
    PACMFILTERCHOOSE_INT pafc;
    HWND        hwndOwner;


    FUNC("wapi_acmFilterChoose");

    if (pafltrc == NULL || ! CeSafeCopyMemory(&afc, pafltrc, sizeof(ACMFILTERCHOOSE))) {
        return MMSYSERR_INVALPARAM;
    }

    pafc = &afc;

    VE_WPOINTER(pafc, sizeof(DWORD));
    VE_WPOINTER(pafc, pafc->cbStruct);
    VE_COND_PARAM(sizeof(*pafc) > pafc->cbStruct);
    VE_FLAGS(pafc->fdwStyle, ACMFILTERCHOOSE_STYLEF_VALID);

    VE_WPOINTER(pafc->pwfltr, pafc->cbwfltr);

    VE_FLAGS(pafc->fdwEnum, ACM_FILTERENUMF_VALID);

    //
    //  validate fdwEnum and pwfltrEnum so the chooser doesn't explode when
    //  an invalid combination is specified.
    //
    cbwfltrEnum = 0L;
    if (0 != (pafc->fdwEnum & ACM_FILTERENUMF_DWFILTERTAG)) {
        VE_COND_PARAM(NULL == pafc->pwfltrEnum);
        VE_RWAVEFILTER(pafc->pwfltrEnum);
        cbwfltrEnum = (UINT)pafc->cbStruct;
    } else {
        VE_COND_PARAM(NULL != pafc->pwfltrEnum);
    }

    pInst = NewInstance((LPBYTE)pafc,FILTER_CHOOSE);
    VE_COND_NOMEM(!pInst);

    pInst->cbwfltrEnum = cbwfltrEnum;

    hInstance = pInst->pag->hinst;

    if (IsWindow(pafc->hwndOwner)) {
        hwndOwner = pafc->hwndOwner;
    } else {
        hwndOwner = NULL;
    }

    //
    // Give the replacable component a chance to edit the style before
    // creating the dialog box.
    //
    waveui_BeforeDialogBox((LPVOID) pafc, FALSE);

    if (0 != (pafc->fdwStyle & ACMFILTERCHOOSE_STYLEF_ENABLETEMPLATE)) {
        lpDlgTemplate = (LPCTSTR)pafc->pszTemplateName;
        hInstance = pafc->hInstance;
    }

    iRet = DialogBoxParamW(hInstance,
                          lpDlgTemplate,
                          hwndOwner,
                          NewSndDlgProc,
                          PTR2LPARAM(pInst));

    //
    // Give the replacable component a chance to clean up after returning
    // from the dialog box.
    //
    waveui_AfterDialogBox((LPVOID) pafc, FALSE);


    switch (iRet)
    {
        case -1:
            mmRet = MMSYSERR_INVALPARAM;
            break;
        case ChooseOk:
            mmRet = MMSYSERR_NOERROR;
            break;
        case ChooseCancel:
            mmRet = ACMERR_CANCELED;
            break;
        case ChooseSubFailure:
            mmRet = pInst->mmrSubFailure;
            break;
        default:
            mmRet = MMSYSERR_NOMEM;
            break;
    }

    if (ChooseOk == iRet)
    {
        DWORD cbSize;
        LPWAVEFILTER lpwfltr = (LPWAVEFILTER)pInst->lpbSel;
        ACMFILTERDETAILS_INT adf;
        ACMFILTERTAGDETAILS_INT adft;

        cbSize = lpwfltr->cbStruct;

        if (pafc->cbwfltr > cbSize)
            pafc->cbwfltr = cbSize;
        else
            VE_COND(cbSize > pafc->cbwfltr, ACMERR_NOTPOSSIBLE);

        memcpy(pafc->pwfltr, lpwfltr, (UINT)pafc->cbwfltr);

        memset(&adft, 0, sizeof(adft));

        adft.cbStruct = sizeof(adft);
        adft.dwFilterTag = lpwfltr->dwFilterTag;
        if (!acmFilterTagDetails(NULL,
                                 (PACMFILTERTAGDETAILS) &adft,
                                 ACM_FILTERTAGDETAILSF_FILTERTAG))
            StringCchCopy(pafc->szFilterTag,_countof(pafc->szFilterTag),adft.szFilterTag);

        adf.cbStruct      = sizeof(adf);
        adf.dwFilterIndex = 0;
        adf.dwFilterTag   = lpwfltr->dwFilterTag;
        adf.fdwSupport    = 0;
        adf.pwfltr        = lpwfltr;
        adf.cbwfltr       = cbSize;

        if (!acmFilterDetails(NULL,
                              (PACMFILTERDETAILS) &adf,
                              ACM_FILTERDETAILSF_FILTER))
            StringCchCopy(pafc->szFilter,_countof(pafc->szFilter),adf.szFilter);

        LocalFree(lpwfltr);
    }

EXIT:
    if (pInst)
        DeleteInstance(pInst);

    CeSafeCopyMemory(pafltrc, &afc, sizeof(ACMFILTERCHOOSE));

    FUNC_EXIT();
    return (mmRet);
}



//------------------------------------------------------------------------------
//
//  @doc    EXTERNAL ACM_API_STRUCTURE
//
//  @struct ACMFORMATCHOOSE | The <t ACMFORMATCHOOSE> structure contains
//          information the Audio Compression Manager (ACM) uses to initialize
//          the system-defined wave format selection dialog box. After the
//          user closes the dialog box, the system returns information about
//          the user's selection in this structure.
//
//  @field  DWORD | cbStruct | Specifies the size in bytes of the
//          <t ACMFORMATCHOOSE> structure. This member must be initialized
//          before calling the <f acmFormatChoose> function. The size specified
//          in this member must be large enough to contain the base
//          <t ACMFORMATCHOOSE> structure.
//
//  @field  DWORD | fdwStyle | Specifies optional style flags for the
//          <f acmFormatChoose> function. This member must be initialized to
//          a valid combination of the following flags before calling the
//          <f acmFormatChoose> function.
//
//          @flag ACMFORMATCHOOSE_STYLEF_ENABLEHOOK |
//          NOTE! : This flag is invalid for Windows CE.
//
//          @flag ACMFORMATCHOOSE_STYLEF_ENABLETEMPLATE |
//          NOTE! : This flag is invalid for Windows CE.
//
//          @flag ACMFORMATCHOOSE_STYLEF_ENABLETEMPLATEHANDLE |
//          NOTE! : This flag is invalid for Windows CE.
//
//          @flag ACMFORMATCHOOSE_STYLEF_INITTOWFXSTRUCT | Indicates that the
//          buffer pointed to by <e ACMFORMATCHOOSE.pwfx> contains a valid
//          <t WAVEFORMATEX> structure that the dialog will use as the initial
//          selection.
//
//          @flag ACMFORMATCHOOSE_STYLEF_SHOWHELP |
//          NOTE! : This flag is invalid for Windows CE.
//
//  @field  HWND | hwndOwner | Identifies the window that owns the dialog
//          box. This member can be any valid window handle, or NULL if the
//          dialog box has no owner. This member must be initialized before
//          calling the <f acmFormatChoose> function.
//
//  @field  LPWAVEFORMATEX | pwfx | Specifies a pointer to a <t WAVEFORMATEX>
//          structure. If the ACMFORMATCHOOSE_STYLEF_INITTOWFXSTRUCT flag is
//          specified in the <e ACMFORMATCHOOSE.fdwStyle> member, then this
//          structure must be initialized to a valid format. When the
//          <f acmFormatChoose> function returns, this buffer contains the
//          selected format. If the user cancels the dialog, no changes will
//          be made to this buffer.
//
//  @field  DWORD | cbwfx | Specifies the size in bytes of the buffer pointed
//          to by the <e ACMFORMATCHOOSE.pwfx> member. The <f acmFormatChoose>
//          function returns ACMERR_NOTPOSSIBLE if the buffer is too small to
//          contain the format information; also, the ACM copies the required size
//          into this member. An application can use the <f acmMetrics> and
//          <f acmFormatTagDetails> functions to determine the largest size
//          required for this buffer.
//
//  @field  LPCSTR | pszTitle | Points to a string to be placed in the title
//          bar of the dialog box. If this member is NULL, the ACM uses the
//          default title (that is, "Sound Selection").
//
//  @field  char | szFormatTag[ACMFORMATTAGDETAILS_FORMATTAG_CHARS] |
//          When the <f acmFormatChoose> function returns, this buffer contains
//          a NULL-terminated string describing the format tag of the format
//          selection. This string is equivalent to the
//          <e ACMFORMATTAGDETAILS.szFormatTag> member of the <t ACMFORMATTAGDETAILS>
//          structure returned by <f acmFormatTagDetails>. If the user cancels
//          the dialog, this member will contain a NULL string.
//
//  @field  char | szFormat[ACMFORMATDETAILS_FORMAT_CHARS] | When the
//          <f acmFormatChoose> function returns, this buffer contains a
//          NULL-terminated string describing the format attributes of the
//          format selection. This string is equivalent to the
//          <e ACMFORMATDETAILS.szFormat> member of the <t ACMFORMATDETAILS>
//          structure returned by <f acmFormatDetails>. If the user cancels
//          the dialog, this member will contain a NULL string.
//
//  @field  LPSTR | pszName |
//          NOTE! : This parameter is ignored in Windows CE.
//
//  @field  DWORD | cchName |
//          NOTE! : This parameter is ignored in Windows CE.
//
//  @field  DWORD | fdwEnum | Specifies optional flags for restricting the
//          type of formats listed in the dialog. These flags are identical to
//          the <p fdwEnum> flags for the <f acmFormatEnum> function. This
//          member should be zero if <e ACMFORMATCHOOSE.pwfxEnum> is NULL.
//
//          @flag ACM_FORMATENUMF_WFORMATTAG | Specifies that the
//          <e WAVEFORMATEX.wFormatTag> member of the <t WAVEFORMATEX> structure
//          referred to by the <e ACMFORMATCHOOSE.pwfxEnum> member is valid. The
//          enumerator will only enumerate a format that conforms to this
//          attribute.
//
//          @flag ACM_FORMATENUMF_NCHANNELS | Specifies that the
//          <e WAVEFORMATEX.nChannels> member of the <t WAVEFORMATEX>
//          structure referred to by the <e ACMFORMATCHOOSE.pwfxEnum> member is
//          valid. The enumerator will only enumerate a format that conforms to
//          this attribute.
//
//          @flag ACM_FORMATENUMF_NSAMPLESPERSEC | Specifies that the
//          <e WAVEFORMATEX.nSamplesPerSec> member of the <t WAVEFORMATEX>
//          structure referred to by the <e ACMFORMATCHOOSE.pwfxEnum> member is
//          valid. The enumerator will only enumerate a format that conforms to
//          this attribute.
//
//          @flag ACM_FORMATENUMF_WBITSPERSAMPLE | Specifies that the
//          <e WAVEFORMATEX.wBitsPerSample> member of the <t WAVEFORMATEX>
//          structure referred to by the <e ACMFORMATCHOOSE.pwfxEnum> member is
//          valid. The enumerator will only enumerate a format that conforms to
//          this attribute.
//
//          @flag ACM_FORMATENUMF_CONVERT | Specifies that the <t WAVEFORMATEX>
//          structure referred to by the <e ACMFORMATCHOOSE.pwfxEnum> member is
//          valid. The enumerator will only enumerate destination formats that
//          can be converted from the given <e ACMFORMATCHOOSE.pwfxEnum> format.
//
//          @flag ACM_FORMATENUMF_SUGGEST | Specifies that the <t WAVEFORMATEX>
//          structure referred to by the <e ACMFORMATCHOOSE.pwfxEnum> member is
//          valid. The enumerator will enumerate all suggested destination
//          formats for the given <e ACMFORMATCHOOSE.pwfxEnum> format.
//
//          @flag ACM_FORMATENUMF_HARDWARE | Specifies that the enumerator should
//          only enumerate formats that are supported in hardware by one or
//          more of the installed wave devices. This provides a way for an
//          application to choose only formats native to an installed wave
//          device.
//
//          @flag ACM_FORMATENUMF_INPUT | Specifies that the enumerator should
//          only enumerate formats that are supported for input (recording).
//
//          @flag ACM_FORMATENUMF_OUTPUT | Specifies that the enumerator should
//          only enumerate formats that are supported for output (playback).
//
//  @field  LPWAVEFORMATEX | pwfxEnum | Points to a <t WAVEFORMATEX> structure
//          that will be used to restrict the formats listed in the dialog. The
//          <e ACMFORMATCHOOSE.fdwEnum> member defines the fields of the
//          <e ACMFORMATCHOOSE.pwfxEnum> structure that should be used for the
//          enumeration restrictions. This member can be NULL if no special
//          restrictions are desired. See the description for <f acmFormatEnum>
//          for other requirements associated with the <e ACMFORMATCHOOSE.pwfxEnum>
//          member.
//
//  @field  HINSTANCE | hInstance |
//          NOTE! : This parameter is ignored in Windows CE.
//
//  @field  LPCSTR | pszTemplateName |
//          NOTE! : This parameter is ignored in Windows CE.
//
//  @field  LPARAM | lCustData |
//          NOTE! : This parameter is ignored in Windows CE.
//
//  @field  ACMFORMATCHOOSEHOOKPROC | pfnHook |
//          NOTE! : This parameter is ignored in Windows CE.
//
//  @xref   <f acmFormatChoose> <f acmMetrics>
//          <f acmFormatTagDetails> <f acmFormatDetails> <f acmFormatEnum>
//          <f acmFilterChoose>
//
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
//
//  @doc    EXTERNAL ACM_API
//
//  @func   MMRESULT | acmFormatChoose | The <f acmFormatChoose> function creates
//          an Audio Compression Manager (ACM) defined dialog box that enables
//          the user to select a wave format.
//
//  @parm   LPACMFORMATCHOOSE | pfmtc | Points to an <t ACMFORMATCHOOSE>
//          structure that contains information used to initialize the dialog
//          box. When <f acmFormatChoose> returns, this structure contains
//          information about the user's format selection.
//
//  @rdesc  Returns <c MMSYSERR_NOERROR> if the function was successful.
//          Otherwise, it returns an error value. Possible error returns are:
//
//          @flag MMSYSERR_INVALHANDLE | Specified handle is invalid.
//
//          @flag MMSYSERR_INVALFLAG | One or more flags are invalid.
//
//          @flag MMSYSERR_NODRIVER | A suitable driver is not available to
//          provide valid format selections.
//
//          @flag MMSYSERR_INVALPARAM | One or more arguments passed are invalid.
//
//          @flag ACMERR_NOTPOSSIBLE | The buffer identified by the
//          <e ACMFORMATCHOOSE.pwfx> member of the <t ACMFORMATCHOOSE> structure
//          is too small to contain the selected format.
//
//          @flag ACMERR_CANCELED | The user chose the Cancel button or the
//          Close command on the System menu to close the dialog box.
//
//  @comm   The <e ACMFORMATCHOOSE.pwfx> member must be filled in with a valid
//          pointer to a memory location that will contain the returned
//          format header structure. Moreover, the <e ACMFORMATCHOOSE.cbwfx>
//          member must be filled in with the size in bytes of this memory buffer.
//
//  @xref   <t ACMFORMATCHOOSE> <f acmFilterChoose>
//
//------------------------------------------------------------------------------
MMRESULT ACMAPI
acmFormatChoose(
    LPACMFORMATCHOOSE pfmtc
    )
{
    int         iRet;
    PInstData   pInst = NULL;
    LPCTSTR     lpDlgTemplate = MAKEINTRESOURCE(DLG_ACMFORMATCHOOSE_ID);
    HINSTANCE   hInstance = NULL;
    MMRESULT    mmRet = MMSYSERR_NOERROR;
    UINT        cbwfxEnum;
    ACMFORMATCHOOSE_INT afc;
    PACMFORMATCHOOSE_INT pafc;
    HWND        hwndOwner;

    FUNC("wapi_acmFormatChoose");

    if (pfmtc == NULL || ! CeSafeCopyMemory(&afc, pfmtc, sizeof(ACMFORMATCHOOSE))) {
        return MMSYSERR_INVALPARAM;
    }
    pafc = &afc;

    VE_WPOINTER(pafc, sizeof(DWORD));
    VE_WPOINTER(pafc, pafc->cbStruct);
    VE_COND_PARAM(sizeof(*pafc) > pafc->cbStruct);
    VE_FLAGS(pafc->fdwStyle, ACMFORMATCHOOSE_STYLEF_VALID);

    VE_WPOINTER(pafc->pwfx, pafc->cbwfx);

    VE_FLAGS(pafc->fdwEnum, ACM_FORMATENUMF_VALID);

    //
    //  validate fdwEnum and pwfxEnum so the chooser doesn't explode when
    //  an invalid combination is specified.
    //
    if (0 != (ACM_FORMATENUMF_HARDWARE & pafc->fdwEnum))
    {
        VE_COND_FLAG(0 == (
            (ACM_FORMATENUMF_INPUT|ACM_FORMATENUMF_OUTPUT) & pafc->fdwEnum));
    }

    cbwfxEnum = 0;
    if (0 != (pafc->fdwEnum & (ACM_FORMATENUMF_WFORMATTAG |
                                ACM_FORMATENUMF_NCHANNELS |
                                ACM_FORMATENUMF_NSAMPLESPERSEC |
                                ACM_FORMATENUMF_WBITSPERSAMPLE |
                                ACM_FORMATENUMF_CONVERT |
                                ACM_FORMATENUMF_SUGGEST)))
    {
        VE_COND_PARAM(NULL == pafc->pwfxEnum);

        if (0 == (pafc->fdwEnum & (ACM_FORMATENUMF_CONVERT |
                                    ACM_FORMATENUMF_SUGGEST)))
        {
            cbwfxEnum = sizeof(PCMWAVEFORMAT);
            VE_RPOINTER(pafc->pwfxEnum, cbwfxEnum);
        }
        else
        {
            cbwfxEnum = SIZEOF_WAVEFORMATEX(pafc->pwfxEnum);
        }
    } else {
        VE_COND_PARAM(NULL != pafc->pwfxEnum);
    }

    //
    // Allocate a chooser Inst structure
    //
    pInst = NewInstance((LPBYTE)pafc,FORMAT_CHOOSE);
    VE_COND_NOMEM(!pInst);

    pInst->cbwfxEnum = cbwfxEnum;

    hInstance = pInst->pag->hinst;

    if (IsWindow(pafc->hwndOwner)) {
        hwndOwner = pafc->hwndOwner;
    } else {
        hwndOwner = NULL;
    }

    //
    // Give the replacable component a chance to edit the style before
    // creating the dialog box.
    //
    waveui_BeforeDialogBox((LPVOID) pafc, TRUE);

    if (0 != (pafc->fdwStyle & ACMFORMATCHOOSE_STYLEF_ENABLETEMPLATE)) {
        lpDlgTemplate = (LPCTSTR)pafc->pszTemplateName;
        hInstance = pafc->hInstance;
    }

    iRet = DialogBoxParam(hInstance,
                          lpDlgTemplate,
                          hwndOwner,
                          NewSndDlgProc,
                          PTR2LPARAM(pInst));

    //
    // Give the replacable component a chance to clean up after returning
    // from the dialog box.
    //
    waveui_AfterDialogBox((LPVOID) pafc, TRUE);


    switch (iRet)
    {
        case -1:
            mmRet = MMSYSERR_INVALPARAM;
            break;
        case ChooseOk:
            mmRet = MMSYSERR_NOERROR;
            break;
        case ChooseCancel:
            mmRet = ACMERR_CANCELED;
            break;
        case ChooseSubFailure:
            mmRet = pInst->mmrSubFailure;
            break;
        default:
            mmRet = MMSYSERR_NOMEM;
            break;
    }

    if (ChooseOk == iRet)
    {
        UINT                cbSize;
        LPWAVEFORMATEX      lpwfx = (LPWAVEFORMATEX)pInst->lpbSel;
        ACMFORMATDETAILS_INT    adf;
        ACMFORMATTAGDETAILS_INT adft;

        cbSize = SIZEOF_WAVEFORMATEX(lpwfx);

        /* pInst has a valid wave format selected */

        if (pafc->cbwfx > cbSize)
            pafc->cbwfx = cbSize;
        else
            VE_COND(cbSize > pafc->cbwfx, ACMERR_NOTPOSSIBLE);

        _fmemcpy(pafc->pwfx, lpwfx, (UINT)pafc->cbwfx);

        _fmemset(&adft, 0, sizeof(adft));

        adft.cbStruct = sizeof(adft);
        adft.dwFormatTag = lpwfx->wFormatTag;
        if (!acmFormatTagDetails(NULL,
                                (PACMFORMATTAGDETAILS) &adft,
                                ACM_FORMATTAGDETAILSF_FORMATTAG))
            StringCchCopy(pafc->szFormatTag,_countof(pafc->szFormatTag),adft.szFormatTag);

        adf.cbStruct      = sizeof(adf);
        adf.dwFormatIndex = 0;
        adf.dwFormatTag   = lpwfx->wFormatTag;
        adf.fdwSupport    = 0;
        adf.pwfx          = lpwfx;
        adf.cbwfx         = cbSize;

        if (!acmFormatDetails(NULL,
                              (PACMFORMATDETAILS) &adf,
                              ACM_FORMATDETAILSF_FORMAT))
            StringCchCopy(pafc->szFormat,_countof(pafc->szFormat),adf.szFormat);

        LocalFree(lpwfx);
    }

EXIT:
    if (pInst)
        DeleteInstance(pInst);

    CeSafeCopyMemory(pfmtc, &afc, sizeof(ACMFORMATCHOOSE));

    FUNC_EXIT();
    return (mmRet);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL FNLOCAL
MeasureItem(
    HWND hwnd,
    MEASUREITEMSTRUCT FAR * lpmis
    )
{
    TEXTMETRIC tm;
    HDC hdc;
    HWND hwndCtrl;

    hwndCtrl = GetDlgItem(hwnd,lpmis->CtlID);

    hdc = GetWindowDC(hwndCtrl);
    GetTextMetrics(hdc,(TEXTMETRIC FAR *)&tm);
    ReleaseDC(hwndCtrl,hdc);
    //bugbug: the "+1" is a fudge.
    lpmis->itemHeight = tm.tmAscent + tm.tmExternalLeading + 1;

    return (TRUE);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL
DoHookCallback(
    PInstData pInst,
    HWND hwnd,
    unsigned msg,
    WPARAM wParam,
    LPARAM lParam
    )
{
    BOOL fRet;
    PBYTE pbOtherProc;
    int cbBuf;
    LPWAVEFORMATEX pwfx = (LPWAVEFORMATEX) lParam;
    LPWAVEFILTER pwfltr = (LPWAVEFILTER) lParam;


    if ((msg == MM_ACM_FORMATCHOOSE) &&
        (wParam == FORMATCHOOSE_FORMAT_VERIFY)) {

        switch (pInst->uType)
        {
            case FORMAT_CHOOSE:
                cbBuf = sizeof(WAVEFORMATEX) + pwfx->cbSize;
                fRet = TRUE;
                break;

            case FILTER_CHOOSE:
                cbBuf = sizeof(WAVEFILTER) + pwfltr->cbStruct;
                fRet = TRUE;
                break;

            default:
                return FALSE;
        }

        if (fRet) {

            pbOtherProc = (PBYTE)LocalAlloc(LPTR, cbBuf);

            if (pbOtherProc != NULL) {

                try
                {
                    memcpy(pbOtherProc, (PVOID) lParam, cbBuf);
                }
                    except (EXCEPTION_EXECUTE_HANDLER)
                {
                    fRet = FALSE;
                }

                if (fRet)
                    fRet = wapi_DoFunctionCallback((LPVOID)pInst->pfnHook, 4,
                                (DWORD) hwnd, (DWORD) msg,
                                (DWORD) wParam, (DWORD) pbOtherProc, 0);

                LocalFree(pbOtherProc);

                if (fRet)
                    return (TRUE);
            }

        }
    } else {
        if (wapi_DoFunctionCallback((LPVOID)pInst->pfnHook, 4,
                        (DWORD) hwnd, (DWORD) msg,
                        (DWORD) wParam, (DWORD) lParam, 0))
            return (TRUE);
    }
    return FALSE;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL FNWCALLBACK
NewSndDlgProc(
    HWND hwnd,
    unsigned msg,
    WPARAM wParam,
    LPARAM lParam
    )
{
    UINT        CmdCommandId;  // WM_COMMAND ID
    UINT        CmdCmd;        // WM_COMMAND Command
    PInstData   pInst;
    DWORD       dwStyle;

    FUNC("NewSndDlgProc");

    if (waveui_AcmDlgProc(hwnd, msg, wParam, lParam)) {
        return (TRUE);
    }

    pInst = GetInstData(hwnd);

    if (pInst) {
        //
        // Pass everything to the hook function first
        //
        if (pInst->fEnableHook && pInst->pfnHook)  {
            if (DoHookCallback(pInst, hwnd, msg, wParam, lParam))
                return(TRUE);
        }
    } else if (msg == WM_INITDIALOG) {
        //
        // Allow HookProc to handle WM_INITDIALOG, but always call ours as well.
        //
        pInst = (PInstData) lParam;
        if (pInst->fEnableHook && pInst->pfnHook)  {
            wapi_DoFunctionCallback((LPVOID)pInst->pfnHook, 4,
                (DWORD) hwnd, (DWORD) msg, (DWORD) wParam, 0, 0);
        }
    }

    switch (msg)
    {

        case MM_ACM_FILTERCHOOSE: // case MM_ACM_FORMATCHOOSE:
            switch (wParam)
            {
                case FORMATCHOOSE_FORMAT_ADD:
                case FORMATCHOOSE_FORMATTAG_ADD:
                    SetWindowLong(hwnd,DWL_MSGRESULT,FALSE);
                    break;

                case FORMATCHOOSE_FORMAT_VERIFY:
                case FORMATCHOOSE_FORMATTAG_VERIFY:
                    SetWindowLong(hwnd,DWL_MSGRESULT,TRUE);
                    break;
                default:
                    return FALSE;
            }
            return (TRUE);

        case WM_INITDIALOG:

            dwStyle = GetWindowLong (hwnd, GWL_EXSTYLE);
            SetWindowLong (hwnd, GWL_EXSTYLE, dwStyle | WS_EX_CAPTIONOKBTN);

            /* Stuff our instance data pointer into the right place */
            if (SetInstData(hwnd,lParam))
            {
                LRESULT     lr;

                lr = HANDLE_WM_INITDIALOG(hwnd, wParam, lParam, InitDialog);

                //
                //  BUGBUG:  Unfortunately, I can't think of the right way
                //  to do this.  It seems as though the IDD_CMB_FORMAT control
                //  does not receive WM_SETFONT before we get WM_MEASUREITEM,
                //  so the WM_MEASUREITEM handler ends up computing a height
                //  that may not be correct.  So, I'll just make height of
                //  our owner draw format combobox the same as the height of
                //  the formattag combobox.
                //
                {
                    int i;

                    i = (int)SendMessage(GetDlgItem(hwnd, IDD_CMB_FORMATTAG), CB_GETITEMHEIGHT, 0, (LPARAM)0);
                    SendMessage(GetDlgItem(hwnd, IDD_CMB_FORMAT), CB_SETITEMHEIGHT, 0, (LPARAM)i);

                    i = (int)SendMessage(GetDlgItem(hwnd, IDD_CMB_FORMATTAG), CB_GETITEMHEIGHT, (WPARAM)(-1), (LPARAM)0);
                    SendMessage(GetDlgItem(hwnd, IDD_CMB_FORMAT), CB_SETITEMHEIGHT, (WPARAM)(-1), (LPARAM)i);
                }

                return (0L != lr);
            }
            else  {
                WARNMSG("Ending with no mem");
                EndDialog(hwnd,ChooseNoMem);
            }
            return (TRUE);

        case WM_DESTROY:
            if (pInst)
            {
                EmptyFormats(pInst);
                RemoveInstData(hwnd);
            }
            /* We don't post quit */
            return (FALSE);

        case WM_MEASUREITEM:
            if ((int)wParam != IDD_CMB_FORMAT)
                return (FALSE);

            MeasureItem(hwnd,(MEASUREITEMSTRUCT FAR *)lParam);
            return (TRUE);

        case WM_CLOSE:
            if (pInst->lpbSel)
            {
                LocalFree(pInst->lpbSel);
                pInst->lpbSel = NULL;
            }
            EndDialog(hwnd,ChooseCancel);
            return (TRUE);

        case WM_HELP:
            {
                HWND hOwner = NULL;
                switch (pInst->uType)
                {
                case FORMAT_CHOOSE:
                    hOwner = pInst->pfmtc->hwndOwner;
                    break;
                case FILTER_CHOOSE:
                    hOwner = pInst->pafltrc->hwndOwner;
                    break;
                default:
                    return FALSE;
                }
                SendMessage( hOwner, pInst->uHelpMsg, 0, 0 );
                SetForegroundWindow(hwnd);
                return (TRUE);
            }

        case WM_COMMAND:
            CmdCommandId = GET_WM_COMMAND_ID(wParam,lParam);
            CmdCmd       = GET_WM_COMMAND_CMD(wParam,lParam);

            switch (CmdCommandId)
            {
                case IDCANCEL:
                {
                    if (pInst->lpbSel)
                    {
                        LocalFree(pInst->lpbSel);
                        pInst->lpbSel = NULL;
                    }
                    EndDialog(hwnd,ChooseCancel);
                    return (TRUE);
                }

                case IDOK:
                {
                    BOOL fOk;

                    fOk = pInst->lpbSel != NULL;
                    if (!fOk)
                    {
                        pInst->mmrSubFailure = MMSYSERR_ERROR;
                        EndDialog(hwnd,ChooseSubFailure);
                    }
                    else
                        EndDialog(hwnd,ChooseOk);
                    return (TRUE);
                }

                case IDD_CMB_FORMATTAG:
                    if (CmdCmd == CBN_SELCHANGE)
                    {
                        int index;
                        index = ComboBox_GetCurSel(pInst->hFormatTags);

                        if (index == pInst->iPrevFormatTagsSel)
                            return (FALSE);

                        if (ComboBox_GetItemData(pInst->hFormatTags,0) == 0)
                        {
                            /* We've inserted an "[unavailable]" so make
                             * sure we remove it and reset the current
                             * selection.
                             */
                            ComboBox_DeleteString(pInst->hFormatTags,0);
                            ComboBox_SetCurSel(pInst->hFormatTags,index-1);
                        }

                        /* CBN_SELCHANGE only comes from the user! */
                        SelectFormatTag(pInst);

                        /* Format == first choice */
                        RefreshFormats(pInst);
                        ComboBox_SetCurSel(pInst->hFormats,0);
                        SelectFormat(pInst);

                        return (TRUE);
                    }
                    return (FALSE);

                case IDD_CMB_FORMAT:
                    if (CmdCmd == CBN_SELCHANGE)
                    {
                        int index;

                        /* CBN_SELCHANGE only comes from the user! */
                        SelectFormat(pInst);

                        /* If we have "unavailable" in list, remove it */
                        index = ComboBox_GetCurSel(pInst->hFormats);
                        if (ComboBox_GetItemData(pInst->hFormats,0) == 0)
                        {
                            int     cFormats;

                            cFormats = ComboBox_GetCount(pInst->hFormats);
                            if (cFormats > 1)
                            {
                                /* We've inserted an "[unavailable]" so make
                                 * sure we remove it and reset the current
                                 * selection.
                                 */
                                if (0 != index)
                                {
                                    ComboBox_DeleteString(pInst->hFormats,0);
                                    ComboBox_SetCurSel(pInst->hFormats,index-1);
                                }
                            }
                        }

                        return (TRUE);
                    }
                    return (FALSE);
            }
    }

    FUNC_EXIT();
    return (FALSE);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void FNLOCAL
SetTitle ( PInstData pInst )
{
    LPCTSTR  pszTitle = NULL;

    switch (pInst->uType)
    {
        case FORMAT_CHOOSE:
            pszTitle = (pInst->pfmtc->pszTitle);
            break;
        case FILTER_CHOOSE:
            pszTitle = (pInst->pafltrc->pszTitle);
            break;
        default:
            return;
    }

    if (pszTitle)
    {
        SendMessage(pInst->hwnd,WM_SETTEXT,0,(LPARAM)pszTitle);
    }
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
LRESULT FNLOCAL
InitDialog(
    HWND                    hwnd,
    HWND                    hwndFocus,
    LPARAM                  lParam
    )
{
    LONG                lStyle;
    RECT                rc;
    BOOL                fReturn;
    BOOL                fInitialized = FALSE;
    PInstData           pInst;
    MMRESULT            mmrEnumStatus;
    BOOL                fShowHelp = FALSE;

    FUNC("InitDialog");
    pInst = GetInstData(hwnd);

    pInst->hwnd = hwnd;

    pInst->hFormatTags = GetDlgItem(hwnd,IDD_CMB_FORMATTAG);
    pInst->hFormats = GetDlgItem(hwnd,IDD_CMB_FORMAT);

    GetWindowRect(pInst->hFormats,(RECT FAR *)&rc);
    pInst->uiFormatTab = ((rc.right - rc.left)*2)/3;

    SetTitle(pInst);
    pInst->uHelpMsg = RegisterWindowMessage(ACMHELPMSGSTRING);

    fReturn = TRUE;


    /*
     * RefreshFormatTags is the first real call to acmFormatEnum, so we
     * need to get out fast if this fails, also pass back the error
     * we got so the user can figure out what went wrong.
     */
    mmrEnumStatus = RefreshFormatTags(pInst);

    if (mmrEnumStatus != MMSYSERR_NOERROR)
    {
        pInst->mmrSubFailure = mmrEnumStatus;
        EndDialog (hwnd,ChooseSubFailure);
        return (fReturn);
    }

    RefreshFormats(pInst);

    switch (pInst->uType) {

        case FORMAT_CHOOSE:
            if (pInst->pfmtc->fdwStyle & ACMFORMATCHOOSE_STYLEF_INITTOWFXSTRUCT) {

                FindFormat(pInst,pInst->pfmtc->pwfx,FALSE);
                fInitialized = TRUE;
            }
            if (pInst->pfmtc->fdwStyle & ACMFORMATCHOOSE_STYLEF_SHOWHELP) {
                fShowHelp = TRUE;
            }
            break;

        case FILTER_CHOOSE:
            if (pInst->pafltrc->fdwStyle & ACMFILTERCHOOSE_STYLEF_INITTOFILTERSTRUCT) {

                FindFilter(pInst,pInst->pafltrc->pwfltr,FALSE);
                fInitialized = TRUE;
            }
            if (pInst->pafltrc->fdwStyle & ACMFILTERCHOOSE_STYLEF_SHOWHELP) {
                fShowHelp = TRUE;
            }
            break;
        default:
            return FALSE;
    }

    if (fShowHelp) {
        lStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
        lStyle |= WS_EX_CONTEXTHELP;
        SetWindowLong(hwnd, GWL_EXSTYLE, lStyle);
    }

    if (!fInitialized) {
        int         cTags;
        int         n;
        DWORD       dwTag;

        cTags = ComboBox_GetCount(pInst->hFormatTags);
        if (0 == cTags)
        {
            TagUnavailable(pInst);
        }

        //
        //  try to default to tag 1 (PCM for format, Volume for filter)
        //
        for (n = cTags; (0 != n); n--)
        {
            dwTag = ComboBox_GetItemData(pInst->hFormatTags, n);
            if (1 == dwTag)
            {
                break;
            }
        }

        ComboBox_SetCurSel(pInst->hFormatTags, n);
        SelectFormatTag(pInst);

        RefreshFormats(pInst);
        ComboBox_SetCurSel(pInst->hFormats,0);
        SelectFormat(pInst);

    }

    FUNC_EXIT();
    return (fReturn);
}




//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void FNLOCAL
SelectFormatTag (
    PInstData pInst
    )
{
    int         index;
    DWORD       dwTag;

    index = ComboBox_GetCurSel(pInst->hFormatTags);
    if (CB_ERR == index)
    {
        pInst->dwTag = 0L;
        return;
    }

    dwTag = ComboBox_GetItemData(pInst->hFormatTags,index);
    pInst->dwTag = dwTag;
    pInst->iPrevFormatTagsSel = index;
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void FNLOCAL
SelectFormat (
    PInstData pInst
    )
{
    int         index;
    LPBYTE      lpbytes;
    LPBYTE      lpSet;

    index = ComboBox_GetCurSel(pInst->hFormats);
    if (CB_ERR == index)
    {
        if (pInst->lpbSel)
            LocalFree(pInst->lpbSel);
        pInst->lpbSel = NULL;
        return;
    }
    lpbytes = (LPBYTE)ComboBox_GetItemData(pInst->hFormats,
                                           index);

    lpSet = CopyStruct(pInst->lpbSel,lpbytes,pInst->uType);
    if (lpSet)
    {
        pInst->lpbSel = lpSet;
    }

}




//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void FNLOCAL
MashNameWithRate (
    PInstData        pInst,
    PNameStore       pnsDest,
    PNameStore       pnsSrc,
    LPWAVEFORMATEX   pwfx
    )
{
    TCHAR   szMashFmt[30];

    pnsDest->achName[0] = TEXT('\0');

    PREFAST_ASSERT( NULL != pInst->pag );
    if( LoadString( pInst->pag->hinst,
                    IDS_FORMAT_MASH,
                    szMashFmt,
                    SIZEOF(szMashFmt)) )
    {
        StringCchPrintf((LPTSTR)pnsDest->achName,NAMELEN(pnsDest),
                (LPTSTR)szMashFmt,
                (LPTSTR)pnsSrc->achName,
                (pwfx->nAvgBytesPerSec + 512) / 1024L);
    }
}






//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void FNLOCAL
TagUnavailable (
    PInstData pInst
    )
{
    int index;
    /* Select [not available] for format tag */
    LoadString(pInst->pag->hinst,
               IDS_TXT_UNAVAILABLE,
               (LPTSTR)pInst->pnsTemp->achName,
               NAMELEN(pInst->pnsTemp));
    index = ComboBox_InsertString(pInst->hFormatTags,
                                   0,
                                   pInst->pnsTemp->achName);
    ComboBox_SetItemData(pInst->hFormatTags,index,NULL);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void FNLOCAL
FormatUnavailable (
    PInstData pInst
    )
{
    int index;
    LoadString(pInst->pag->hinst,
               IDS_TXT_UNAVAILABLE,
               (LPTSTR)pInst->pnsTemp->achName,
               NAMELEN(pInst->pnsTemp));
    index = ComboBox_InsertString(pInst->hFormats,
                                   0,
                                   pInst->pnsTemp->achName);
    ComboBox_SetItemData(pInst->hFormats,index,NULL);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL FNLOCAL
FindFormat(
    PInstData       pInst,
    LPWAVEFORMATEX  pwfx,
    BOOL            fExact
    )
{
    int                 index;
    BOOL                fOk;
    ACMFORMATTAGDETAILS_INT adft;
    MMRESULT            mmr;
    ACMFORMATDETAILS_INT    adf;

    PNameStore pns = pInst->pnsTemp;

    /* Adjust the Format and FormatTag comboboxes to correspond to the
     * Custom Format selection
     */
    _fmemset(&adft, 0, sizeof(adft));

    adft.cbStruct = sizeof(adft);
    adft.dwFormatTag = pwfx->wFormatTag;
    mmr = acmFormatTagDetails(NULL, (PACMFORMATTAGDETAILS) &adft, ACM_FORMATTAGDETAILSF_FORMATTAG);
    fOk = (MMSYSERR_NOERROR == mmr);
    if (fOk)
    {
        index = ComboBox_FindStringExact(pInst->hFormatTags,
                                             -1,
                                             adft.szFormatTag);
        fOk = (CB_ERR != index);
    }

    index = fOk?index:0;

    if (!fOk && fExact && ComboBox_GetItemData(pInst->hFormatTags,0))
        TagUnavailable(pInst);

    ComboBox_SetCurSel(pInst->hFormatTags,index);
    SelectFormatTag((PInstData)pInst);

    RefreshFormats((PInstData)pInst);

    if (fOk)
    {
        //
        //
        //
        adf.cbStruct      = sizeof(adf);
        adf.dwFormatIndex = 0;
        adf.dwFormatTag   = pwfx->wFormatTag;
        adf.fdwSupport    = 0;
        adf.pwfx          = pwfx;
        adf.cbwfx         = SIZEOF_WAVEFORMATEX(pwfx);

        mmr = acmFormatDetails(NULL, (PACMFORMATDETAILS) &adf, ACM_FORMATDETAILSF_FORMAT);

        fOk = (MMSYSERR_NOERROR == mmr);
        if (fOk)
        {
            StringCchCopy(pns->achName, NAMELEN(pns), adf.szFormat);
            MashNameWithRate(pInst,pInst->pnsStrOut,pns,pwfx);
            index = ComboBox_FindStringExact(pInst->hFormats,-1,
                                              pInst->pnsStrOut->achName);

            fOk = (CB_ERR != index);
        }
        index = fOk?index:0;
    }
    if (!fOk && fExact && ComboBox_GetItemData(pInst->hFormats,0))
    {
        FormatUnavailable(pInst);
    }

    ComboBox_SetCurSel(pInst->hFormats,index);
    SelectFormat((PInstData)pInst);

    return (fOk);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL FNLOCAL
FindFilter (
    PInstData      pInst,
    LPWAVEFILTER   pwf,
    BOOL           fExact
    )
{
    int                 index;
    BOOL                fOk;
    ACMFILTERTAGDETAILS_INT adft;
    MMRESULT            mmr;
    ACMFILTERDETAILS_INT    adf;

    /* Adjust the Filter and FilterTag comboboxes to correspond to the
     * Custom Filter selection
     */
    _fmemset(&adft, 0, sizeof(adft));

    adft.cbStruct = sizeof(adft);
    adft.dwFilterTag = pwf->dwFilterTag;
    mmr = acmFilterTagDetails(NULL,
                               (PACMFILTERTAGDETAILS) &adft,
                               ACM_FILTERTAGDETAILSF_FILTERTAG);
    fOk = (MMSYSERR_NOERROR == mmr);
    if (fOk)
    {
        index = ComboBox_FindStringExact(pInst->hFormatTags,
                                             -1,
                                             adft.szFilterTag);
        fOk = (CB_ERR != index);
    }

    index = fOk?index:0;

    if (!fOk && fExact && ComboBox_GetItemData(pInst->hFormatTags,0))
        TagUnavailable(pInst);

    ComboBox_SetCurSel(pInst->hFormatTags,index);
    SelectFormatTag((PInstData)pInst);

    RefreshFormats((PInstData)pInst);

    if (fOk)
    {
        //
        //
        //
        adf.cbStruct      = sizeof(adf);
        adf.dwFilterIndex = 0;
        adf.dwFilterTag   = pwf->dwFilterTag;
        adf.fdwSupport    = 0;
        adf.pwfltr        = pwf;
        adf.cbwfltr       = pwf->cbStruct;

        mmr = acmFilterDetails(NULL, (PACMFILTERDETAILS) &adf, ACM_FILTERDETAILSF_FILTER);
        fOk = (MMSYSERR_NOERROR == mmr);
        if (fOk)
        {
            index = ComboBox_FindStringExact(pInst->hFormats, -1, adf.szFilter);

            fOk = (CB_ERR != index);
        }
        index = fOk?index:0;
    }

    ComboBox_SetCurSel(pInst->hFormats,index);
    SelectFormat((PInstData)pInst);
    return (TRUE);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL FNLOCAL
InEnumSet (PInstData pInst, LPWAVEFORMATEX pwfxCustom, LPWAVEFORMATEX pwfxBuf, DWORD cbSize);




//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
/*
 * N = number of custom formats.
 * K = number of formats in the enumeration.
 */

/* slow method.
 * FOREACH format, is there a matching format in the enumeration?
 * cost? - Many calls to enumeration apis as N increases (linear search).
 * O(N)*O(K)
 * Best case:   All formats hit early in the enumeration. < O(K) multiplier
 * Worst case:  All formats hit late in the enumeration.  Hard O(K)*O(N)
 */
/* alternate method.
 * FOREACH enumerated format, is there a hit in the custom formats?
 * cost? - Call to lookup function for all enumerated types.
 * O(K)*O(N)
 * Best case:   A cheap lookup will mean < O(N) multiplier
 * Worst case:  Hard O(K)*O(N)
 */
typedef struct tResponse {
    LPWAVEFORMATEX pwfx;
    BOOL fHit;
} Response ;

BOOL FNWCALLBACK
CustomCallback ( HACMDRIVERID           hadid,
                 LPACMFORMATDETAILS     pafd,
                 DWORD                  dwInstance,
                 DWORD                  fdwSupport )
{
    Response FAR * presp = (Response FAR *)dwInstance;
    if (_fmemcmp(presp->pwfx,pafd->pwfx,SIZEOF_WAVEFORMATEX(presp->pwfx)) == 0)
    {
        presp->fHit = TRUE;
        return (FALSE);
    }
    return (TRUE);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL FNLOCAL
InEnumSet (PInstData        pInst,
           LPWAVEFORMATEX   pwfxCustom,
           LPWAVEFORMATEX   pwfxBuf,
           DWORD            cbwfx )
{
    ACMFORMATDETAILS_INT    afd;
    DWORD               cbSize;
    DWORD               dwEnumFlags;
    BOOL                fOk;
    Response            resp;
    Response FAR *     presp;

    FUNC(InEnumSet);

    _fmemset(&afd, 0, sizeof(afd));

    afd.cbStruct    = sizeof(afd);
    afd.pwfx        = pwfxBuf;
    afd.cbwfx       = cbwfx;
    dwEnumFlags     = pInst->pfmtc->fdwEnum;

    /* optional filtering for a waveformat template */
    if ( pInst->pfmtc->pwfxEnum )
    {
        cbSize = min (pInst->cbwfxEnum, afd.cbwfx );
        _fmemcpy(afd.pwfx, pInst->pfmtc->pwfxEnum, (UINT)cbSize);
    }

    if (dwEnumFlags & (ACM_FORMATENUMF_CONVERT | ACM_FORMATENUMF_SUGGEST))
    {
        ;
    }
    else
    {
        /* if we don't really need this information, we can use
         * it to restrict the enumeration and hopefully speed things
         * up.
         */
        dwEnumFlags |= ACM_FORMATENUMF_WFORMATTAG;
        afd.pwfx->wFormatTag = pwfxCustom->wFormatTag;
        dwEnumFlags |= ACM_FORMATENUMF_NCHANNELS;
        afd.pwfx->nChannels = pwfxCustom->nChannels;
        dwEnumFlags |= ACM_FORMATENUMF_NSAMPLESPERSEC;
        afd.pwfx->nSamplesPerSec = pwfxCustom->nSamplesPerSec;
        dwEnumFlags |= ACM_FORMATENUMF_WBITSPERSAMPLE;
        afd.pwfx->wBitsPerSample = pwfxCustom->wBitsPerSample;
    }

    resp.fHit = FALSE;
    resp.pwfx = pwfxCustom;

    afd.dwFormatTag = afd.pwfx->wFormatTag;

    presp = &resp;
    fOk = (acmFormatEnum(NULL,
                         (PACMFORMATDETAILS) &afd,
                         CustomCallback,
                         (LPARAM)presp,
                         dwEnumFlags)== 0L);

    FUNC_EXIT();
    return (resp.fHit);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
MMRESULT FNLOCAL
RefreshFormatTags (
    PInstData pInst
    )
{
    MMRESULT    mmr;
    DWORD       dwEnumFlags = 0L;
    MMRESULT    mmrEnumStatus = MMSYSERR_NOERROR;

    FUNC("RefreshFormatTags");

    SetWindowRedraw(pInst->hFormatTags,FALSE);

    ComboBox_ResetContent(pInst->hFormatTags);

    switch (pInst->uType)
    {
        case FORMAT_CHOOSE:
        {
            ACMFORMATDETAILS_INT   afd;
            LPWAVEFORMATEX      pwfx;
            DWORD               cbSize;

            //
            // Enumerate the format tags for the FormatTag combobox.
            // This might seem weird, to call acmFormatEnum, but we have
            // to because it has the functionality to restrict formats and
            // acmFormatTagEnum doesn't.
            //
            _fmemset(&afd, 0, sizeof(afd));

            mmr = IMetricsMaxSizeFormat(NULL, &afd.cbwfx );
            if (MMSYSERR_NOERROR == mmr)
            {
                afd.cbwfx = max(afd.cbwfx, pInst->cbwfxEnum);

                pwfx = (LPWAVEFORMATEX)LocalAlloc(LPTR, (UINT)afd.cbwfx);
                if (!pwfx)
                    break;

                HEXVALUE(pwfx);
                afd.cbStruct    = sizeof(afd);
                afd.pwfx        = pwfx;

                /* optional filtering for a waveformat template */
                if ( pInst->pfmtc->pwfxEnum )
                {
                    cbSize = min (pInst->cbwfxEnum, afd.cbwfx);
                    _fmemcpy(pwfx, pInst->pfmtc->pwfxEnum, (UINT)cbSize);
                    afd.dwFormatTag = pwfx->wFormatTag;
                }

                dwEnumFlags = pInst->pfmtc->fdwEnum;

                if (0 == (dwEnumFlags & (ACM_FORMATENUMF_CONVERT |
                                         ACM_FORMATENUMF_SUGGEST)))
                {
                    ACMFORMATTAGDETAILS_INT aftd;

                    _fmemset(&aftd, 0, sizeof(aftd));

                    /* Enumerate the format tags */
                    aftd.cbStruct = sizeof(aftd);

                    /* Was a format tag specified?
                    * This means they only want one format tag.
                    */
                    pInst->fTagFilter = (pInst->pfmtc->pwfxEnum &&
                                        (pInst->pfmtc->fdwEnum & ACM_FORMATENUMF_WFORMATTAG));

                    pInst->pafdSimple = (PACMFORMATDETAILS) &afd;

                    HEXVALUE(FormatTagsCallbackSimple);
                    HEXVALUE(pInst->pafdSimple);
                    HEXVALUE(pInst);
                    HEXVALUE(&aftd);
                    mmrEnumStatus = acmFormatTagEnum(NULL,
                                                     (PACMFORMATTAGDETAILS) &aftd,
                                                     FormatTagsCallbackSimple,
                                                     PTR2LPARAM(pInst),
                                                     0L);
                    pInst->pafdSimple = NULL;
                }
                else
                {
                    mmrEnumStatus = acmFormatEnum(NULL,
                                                  (PACMFORMATDETAILS) &afd,
                                                  FormatTagsCallback,
                                                  PTR2LPARAM(pInst),
                                                  dwEnumFlags);
                }

                if (MMSYSERR_NOERROR == mmrEnumStatus)
                {
                    //
                    //  add format that we are asked to init to (this has every
                    //  chance of being a 'non-standard' format, so we have to do
                    //  this in the following way..)
                    //
                    if (0 != (ACMFORMATCHOOSE_STYLEF_INITTOWFXSTRUCT & pInst->pfmtc->fdwStyle))
                    {
                        afd.cbStruct    = sizeof(afd);
                        afd.dwFormatTag = pInst->pfmtc->pwfx->wFormatTag;
                        afd.pwfx        = pInst->pfmtc->pwfx;
                        afd.cbwfx       = SIZEOF_WAVEFORMATEX(pInst->pfmtc->pwfx);
                        afd.fdwSupport  = 0L;

                        mmr = acmFormatDetails(NULL, (PACMFORMATDETAILS) &afd, ACM_FORMATDETAILSF_FORMAT);
                        if (MMSYSERR_NOERROR == mmr)
                        {
                            FormatTagsCallback(NULL, (PACMFORMATDETAILS) &afd, PTR2LPARAM(pInst), afd.fdwSupport);
                        }
                    }

                    //
                    //
                    //
                    if (    (0 != (pInst->pfmtc->fdwEnum & (ACM_FORMATENUMF_CONVERT | ACM_FORMATENUMF_SUGGEST)))
                         && (pInst->pfmtc->pwfxEnum != NULL)
                         )
                    {
                        afd.cbStruct    = sizeof(afd);
                        afd.dwFormatTag = pInst->pfmtc->pwfxEnum->wFormatTag;
                        afd.pwfx        = pInst->pfmtc->pwfxEnum;
                        afd.cbwfx       = SIZEOF_WAVEFORMATEX(pInst->pfmtc->pwfxEnum);
                        afd.fdwSupport  = 0L;

                        mmr = acmFormatDetails(NULL, (PACMFORMATDETAILS) &afd, ACM_FORMATDETAILSF_FORMAT);
                        if (MMSYSERR_NOERROR == mmr)
                        {
                            FormatTagsCallback(NULL, (PACMFORMATDETAILS) &afd, PTR2LPARAM(pInst), afd.fdwSupport);
                        }
                    }
                }
                LocalFree(pwfx);
            }
            break;
        }
        case FILTER_CHOOSE:
        {
            ACMFILTERTAGDETAILS_INT aftd;

            // Enumerate the filter tags
            aftd.cbStruct = sizeof(aftd);
            //
            // Was a filter tag specified?
            // This means they only want one filter tag.
            //
            pInst->fTagFilter = (pInst->pafltrc->pwfltrEnum &&
                                 (pInst->pafltrc->fdwEnum & ACM_FILTERENUMF_DWFILTERTAG));

            mmrEnumStatus = acmFilterTagEnum(NULL,
                                              (PACMFILTERTAGDETAILS) &aftd,
                                              FilterTagsCallback,
                                              PTR2LPARAM(pInst),
                                              dwEnumFlags);
            if (MMSYSERR_NOERROR == mmrEnumStatus)
            {
                //
                //  add filter that we are asked to init to (this has every
                //  chance of being a 'non-standard' filter, so we have to do
                //  this in the following way..)
                //
                if (0 != (ACMFILTERCHOOSE_STYLEF_INITTOFILTERSTRUCT & pInst->pafltrc->fdwStyle))
                {
                    _fmemset(&aftd, 0, sizeof(aftd));

                    aftd.cbStruct    = sizeof(aftd);
                    aftd.dwFilterTag = pInst->pafltrc->pwfltr->dwFilterTag;

                    mmr = acmFilterTagDetails(NULL, (PACMFILTERTAGDETAILS) &aftd, ACM_FILTERTAGDETAILSF_FILTERTAG);
                    if (MMSYSERR_NOERROR == mmr)
                    {
                        FilterTagsCallback(NULL, (PACMFILTERTAGDETAILS) &aftd, PTR2LPARAM(pInst), aftd.fdwSupport);
                    }
                }
            }

            break;
        }

        default:
            mmrEnumStatus = MMSYSERR_ERROR;
    }

    if (MMSYSERR_NOERROR == mmrEnumStatus)
    {
        //
        // perhaps we made it through but, darn it, we just didn't find
        // any suitable tags!  Well there must not have been an acceptable
        // driver configuration.  We just quit and tell the caller.
        //
        if (ComboBox_GetCount(pInst->hFormatTags) == 0)
            mmrEnumStatus = MMSYSERR_NODRIVER;
    }

    SetWindowRedraw(pInst->hFormatTags,TRUE);

    FUNC_EXIT();
    return (mmrEnumStatus);
}


//------------------------------------------------------------------------------
//
//  BOOL FormatTagsCallbackSimpleOnlyOne
//
//  Description:
//
//
//  Arguments:
//          HACMDRIVERID hadid:
//
//          LPACMFORMATDETAILS pafd:
//
//          DWORD dwInstance:
//
//          DWORD fdwSupport:
//
//  Return (BOOL):
//
//
//------------------------------------------------------------------------------
BOOL FNWCALLBACK
FormatTagsCallbackSimpleOnlyOne(
    HACMDRIVERID            hadid,
    LPACMFORMATDETAILS      pafd,
    DWORD                   dwInstance,
    DWORD                   fdwSupport
    )
{
    //
    //  only need ONE callback!
    //
    *((LPDWORD)dwInstance) = 1;

    //DPF(1, "FormatTagsCallbackSimpleOnlyOne: %lu, %s", pafd->dwFormatTag, pafd->szFormat);

    return (FALSE);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL FNWCALLBACK
FormatTagsCallbackSimple(
    HACMDRIVERID            hadid,
    LPACMFORMATTAGDETAILS   paftd,
    DWORD                   dwInstance,
    DWORD                   fdwSupport
    )
{
    BOOL                fRet;
    MMRESULT            mmr;
    int                 n;
    PInstData           pInst;
    LPWAVEFORMATEX      pwfxSave;
    DWORD               cbwfxSave;
    DWORD               dw;

    FUNC("FormatTagsCallbackSimple");

    //
    //
    //
    pInst = (PInstData)LPARAM2PTR(dwInstance);

    HEXVALUE(pInst);

    // Explicitly filtering for a tag?
    if (pInst->fTagFilter && (paftd->dwFormatTag != pInst->pfmtc->pwfxEnum->wFormatTag))
        GOTO_EXIT(fRet = TRUE);

    n = ComboBox_FindStringExact(pInst->hFormatTags, -1, paftd->szFormatTag);
    if (CB_ERR != n)
    {
        GOTO_EXIT(fRet = TRUE);
    }

    dw = 0;

    pInst->pafdSimple->dwFormatTag = paftd->dwFormatTag;
    pInst->pafdSimple->fdwSupport  = 0L;
    pInst->pafdSimple->pwfx->wFormatTag = (UINT)paftd->dwFormatTag;

    //
    //
    //
    cbwfxSave = pInst->pafdSimple->cbwfx;
    pwfxSave = (LPWAVEFORMATEX)LocalAlloc(LPTR, cbwfxSave);
    if (NULL == pwfxSave) {
        GOTO_EXIT(fRet = TRUE);
    }

    _fmemcpy(pwfxSave, pInst->pafdSimple->pwfx, (int)cbwfxSave);

    mmr = acmFormatEnum(NULL,
                        pInst->pafdSimple,
                        FormatTagsCallbackSimpleOnlyOne,
                        (DWORD)(LPDWORD)&dw,
                        pInst->pfmtc->fdwEnum | ACM_FORMATENUMF_WFORMATTAG);

    _fmemcpy(pInst->pafdSimple->pwfx, pwfxSave, (int)cbwfxSave);
    LocalFree(pwfxSave);

    //
    //
    //
    if (0 == dw)
        GOTO_EXIT(fRet = TRUE);

    {
        fRet = (BOOL)SendMessage(pInst->hwnd,
                              MM_ACM_FORMATCHOOSE,
                              FORMATCHOOSE_FORMATTAG_VERIFY,
                              (LPARAM)paftd->dwFormatTag);
        if (!fRet) {
            GOTO_EXIT(fRet = TRUE);
        }
    }

    n = ComboBox_AddString(pInst->hFormatTags, paftd->szFormatTag);
    ComboBox_SetItemData(pInst->hFormatTags, n, paftd->dwFormatTag);

    fRet = TRUE;
EXIT:
    FUNC_EXIT();
    return(fRet);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL FNWCALLBACK
FormatTagsCallback (
    HACMDRIVERID           hadid,
    LPACMFORMATDETAILS     pafd,
    DWORD                  dwInstance,
    DWORD                  fdwSupport
    )
{
    int                 index;
    PInstData           pInst = (PInstData)LPARAM2PTR(dwInstance);
    ACMFORMATTAGDETAILS_INT aftd;
    MMRESULT            mmr;
    BOOL                fRet;

    FUNC("FormatTagsCallback");

    //
    // We are being called by acmFormatEnum.  Why not acmFormatTagEnum?
    // because we can't enumerate tags based upon the same restrictions
    // as acmFormatEnum.  So we use the pwfx->wFormatTag and lookup
    // the combobox to determine if we've had a hit.  This is slow, but
    // it only happens once during initialization.
    //
    _fmemset(&aftd, 0, sizeof(aftd));
    aftd.cbStruct = sizeof(aftd);
    aftd.dwFormatTag = pafd->pwfx->wFormatTag;

    mmr = acmFormatTagDetails(NULL,
                              (PACMFORMATTAGDETAILS) &aftd,
                              ACM_FORMATTAGDETAILSF_FORMATTAG);
    if (MMSYSERR_NOERROR != mmr)
        GOTO_EXIT(fRet = TRUE);

    index = ComboBox_FindStringExact(pInst->hFormatTags,
                                         -1,
                                         aftd.szFormatTag);

    // if this isn't there try to add it.
    if (CB_ERR == index)
    {
        if (!SendMessage(pInst->hwnd, MM_ACM_FORMATCHOOSE,
              FORMATCHOOSE_FORMATTAG_VERIFY, (LPARAM)aftd.dwFormatTag))
        {
            GOTO_EXIT(fRet = TRUE);
        }
        index = ComboBox_AddString(pInst->hFormatTags, aftd.szFormatTag);
        ComboBox_SetItemData(pInst->hFormatTags,index, aftd.dwFormatTag);

    }

    // Keep going
    fRet = TRUE;
EXIT:
    FUNC_EXIT();
    return(fRet);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL FNWCALLBACK
FilterTagsCallback (
    HACMDRIVERID           hadid,
    LPACMFILTERTAGDETAILS  paftd,
    DWORD                  dwInstance,
    DWORD                  fdwSupport
    )
{
    int             index;
    PInstData       pInst = (PInstData)LPARAM2PTR(dwInstance);
    BOOL                fRet;

    FUNC("FilterTagsCallback");



    // Explicitly filtering for a tag?
    if (pInst->fTagFilter &&
        paftd->dwFilterTag != pInst->pafltrc->pwfltrEnum->dwFilterTag)
        GOTO_EXIT(fRet = TRUE);

    index = ComboBox_FindStringExact(pInst->hFormatTags, -1, paftd->szFilterTag);

    // if this isn't there try to add it.
    if (CB_ERR == index)
    {
        if (!SendMessage(pInst->hwnd, MM_ACM_FILTERCHOOSE,
            FILTERCHOOSE_FILTERTAG_VERIFY, (LPARAM)paftd->dwFilterTag))
        {
            GOTO_EXIT(fRet = TRUE);
        }
        index = ComboBox_AddString(pInst->hFormatTags, paftd->szFilterTag);
        ComboBox_SetItemData(pInst->hFormatTags,index, paftd->dwFilterTag);
    }

    // Keep going
    fRet = TRUE;
EXIT:
    FUNC_EXIT();
    return(fRet);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void FNLOCAL
RefreshFormats (
    PInstData pInst
    )
{
    BOOL            fOk;
    MMRESULT        mmr;
    DWORD           dwEnumFlags;
    DWORD           cbSize;

    FUNC("RefreshFormats");

    ASSERT( NULL != pInst->pag );

    SetWindowRedraw(pInst->hFormats,FALSE);

    /* Remove all wave formats */
    EmptyFormats(pInst);

    ComboBox_ResetContent(pInst->hFormats);

    /* Brief explanation:
     *  RefreshFormats() updates the Format/Filter combobox.  This
     *  combobox is *THE* selection for the dialog.  This is where we
     *  call the enumeration API's to limit the user's selection.
     *
     *  IF the user has passed in fdwEnum flags to "match", we just copy
     *  the p*Enum add our current tag and OR the ACM_*ENUMF_*TAG flag
     *  to their fdwEnum flags.
     *
     *  IF the user has passed in fdwEnum flags to convert or suggest,
     *  we just let it go untouched through the acmFormatEnum API.
     */

    fOk = (pInst->dwTag != 0L);
    /* If there's a bad tag selected.  Just skip this junk
     */

    if (fOk)  {
        switch (pInst->uType)  {

            case FORMAT_CHOOSE:  {

                ACMFORMATDETAILS_INT    afd;
                LPWAVEFORMATEX      pwfx;

                fOk = FALSE;

                mmr = IMetricsMaxSizeFormat(NULL, &afd.cbwfx );
                if (MMSYSERR_NOERROR == mmr)  {
                    afd.cbwfx = max(afd.cbwfx, pInst->cbwfxEnum);

                    pwfx = (LPWAVEFORMATEX)LocalAlloc(LPTR, (UINT)afd.cbwfx);
                    if (NULL == pwfx)
                        break;

                    afd.cbStruct    = sizeof(afd);
                    afd.pwfx        = pwfx;
                    afd.fdwSupport  = 0L;

                    /* optional filtering for a waveformat template */
                    if ( pInst->pfmtc->pwfxEnum )
                    {
                        cbSize = min(pInst->cbwfxEnum, afd.cbwfx);
                        _fmemcpy(pwfx, pInst->pfmtc->pwfxEnum, (UINT)cbSize);
                    }

                    dwEnumFlags = pInst->pfmtc->fdwEnum;

                    fOk = TRUE;

                    if ( pInst->pfmtc->fdwEnum &
                         (ACM_FORMATENUMF_CONVERT | ACM_FORMATENUMF_SUGGEST))
                    {
                        /* enumerate over all formats and exclude
                         * undesireable ones in the callback.
                         */
                        ;
                    }
                    else
                    {
                        /* enumerate over only ONE format
                         */
                        dwEnumFlags |= ACM_FORMATENUMF_WFORMATTAG;
                        afd.pwfx->wFormatTag = (WORD)pInst->dwTag;
                    }

                    afd.dwFormatTag = pwfx->wFormatTag;

                    fOk = (acmFormatEnum(NULL,
                                        (PACMFORMATDETAILS) &afd,
                                        FormatsCallback,
                                        PTR2LPARAM(pInst),
                                        dwEnumFlags)== 0L);

                    LocalFree(pwfx);
                }

                //
                //  add format that we are asked to init to (this has every
                //  chance of being a 'non-standard' format, so we have to do
                //  this in the following way..)
                //
                if (0 != (ACMFORMATCHOOSE_STYLEF_INITTOWFXSTRUCT & pInst->pfmtc->fdwStyle))
                {
                    if (pInst->pfmtc->pwfx->wFormatTag == (WORD)pInst->dwTag)
                    {
                        afd.cbStruct    = sizeof(afd);
                        afd.dwFormatTag = pInst->dwTag;
                        afd.pwfx        = pInst->pfmtc->pwfx;
                        afd.cbwfx       = SIZEOF_WAVEFORMATEX(pInst->pfmtc->pwfx);
                        afd.fdwSupport  = 0L;

                        mmr = acmFormatDetails(NULL, (PACMFORMATDETAILS) &afd, ACM_FORMATDETAILSF_FORMAT);
                        if (MMSYSERR_NOERROR == mmr)
                        {
                            FormatsCallback(NULL, (PACMFORMATDETAILS) &afd, PTR2LPARAM(pInst), afd.fdwSupport);
                        }
                    }
                }

                //
                //
                //
                if (0 != (pInst->pfmtc->fdwEnum & (ACM_FORMATENUMF_CONVERT |
                                                   ACM_FORMATENUMF_SUGGEST)))
                {
                    PREFAST_ASSERT(pInst->pfmtc->pwfxEnum != NULL);
                    if (pInst->pfmtc->pwfxEnum->wFormatTag == (WORD)pInst->dwTag)
                    {
                        afd.cbStruct    = sizeof(afd);
                        afd.dwFormatTag = pInst->dwTag;
                        afd.pwfx        = pInst->pfmtc->pwfxEnum;
                        afd.cbwfx       = SIZEOF_WAVEFORMATEX(pInst->pfmtc->pwfxEnum);
                        afd.fdwSupport  = 0L;

                        mmr = acmFormatDetails(NULL, (PACMFORMATDETAILS) &afd, ACM_FORMATDETAILSF_FORMAT);
                        if (MMSYSERR_NOERROR == mmr)
                        {
                            FormatsCallback(NULL, (PACMFORMATDETAILS) &afd, PTR2LPARAM(pInst), afd.fdwSupport);
                        }
                    }
                }
                break;
            }

            case FILTER_CHOOSE:  {
                ACMFILTERDETAILS_INT    afd;
                LPWAVEFILTER         pwfltr;

                fOk = FALSE;

                mmr = IMetricsMaxSizeFilter(NULL, &afd.cbwfltr );

                if (MMSYSERR_NOERROR == mmr)  {

                    afd.cbwfltr = max(afd.cbwfltr, pInst->cbwfltrEnum);

                    pwfltr = (LPWAVEFILTER)LocalAlloc(LPTR, (UINT)afd.cbwfltr);
                    if (NULL != pwfltr)
                    {
                        afd.cbStruct   = sizeof(afd);
                        afd.pwfltr     = pwfltr;
                        afd.fdwSupport = 0L;

                        /* optional filtering for a wavefilter template */
                        if ( pInst->pafltrc->pwfltrEnum )
                        {
                            cbSize = pInst->pafltrc->pwfltrEnum->cbStruct;
                            cbSize = min (cbSize, afd.cbwfltr);
                            _fmemcpy(pwfltr, pInst->pafltrc->pwfltrEnum, (UINT)cbSize);
                        }

                        dwEnumFlags = ACM_FILTERENUMF_DWFILTERTAG;
                        afd.pwfltr->dwFilterTag = pInst->dwTag;

                        fOk = (acmFilterEnum(NULL,
                                             (PACMFILTERDETAILS) &afd,
                                             FiltersCallback,
                                             PTR2LPARAM(pInst),
                                             dwEnumFlags) == 0L);
                        LocalFree(pwfltr);
                    }
                }

                //
                //  add filter that we are asked to init to (this has every
                //  chance of being a 'non-standard' filter, so we have to do
                //  this in the following way..)
                //
                if (0 != (ACMFILTERCHOOSE_STYLEF_INITTOFILTERSTRUCT & pInst->pafltrc->fdwStyle))
                {
                    if (pInst->pafltrc->pwfltr->dwFilterTag == pInst->dwTag)
                    {
                        afd.cbStruct    = sizeof(afd);
                        afd.dwFilterTag = pInst->dwTag;
                        afd.pwfltr      = pInst->pafltrc->pwfltr;
                        afd.cbwfltr     = pInst->pafltrc->pwfltr->cbStruct;
                        afd.fdwSupport  = 0L;

                        mmr = acmFilterDetails(NULL, (PACMFILTERDETAILS) &afd, ACM_FILTERDETAILSF_FILTER);
                        if (MMSYSERR_NOERROR == mmr)
                        {
                            FiltersCallback(NULL, (PACMFILTERDETAILS) &afd, PTR2LPARAM(pInst), afd.fdwSupport);
                        }
                    }
                }
                break;
            }

            default:
                return;
        }

    }


    if (fOk)
        fOk = (ComboBox_GetCount(pInst->hFormats) > 0);

    if (!fOk)
    {
        int index;

        // The codec has probably been disabled or there are no supported
        // formats.
        PREFAST_ASSERT(pInst->pag != NULL);
        LoadString(pInst->pag->hinst,
                   IDS_TXT_NONE,
                   (LPTSTR)pInst->pnsTemp->achName,
                   NAMELEN(pInst->pnsTemp));
        index = ComboBox_InsertString(pInst->hFormats,0,
                                       pInst->pnsTemp->achName);
        ComboBox_SetItemData(pInst->hFormats,index,0L);
    }

    // Don't let the user OK or assign name, only cancel

    SetWindowRedraw(pInst->hFormats,TRUE);

    FUNC_EXIT();
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void FNLOCAL
EmptyFormats (
    PInstData pInst
    )
{
    int index;
    LPWAVEFORMATEX lpwfx;
    for (index = ComboBox_GetCount(pInst->hFormats);
        index > 0;
        index--)
    {
        lpwfx = (LPWAVEFORMATEX)ComboBox_GetItemData(pInst->hFormats,index-1);
        if (lpwfx)
            LocalFree(lpwfx);
    }
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL FNWCALLBACK
FormatsCallback (
    HACMDRIVERID hadid,
    LPACMFORMATDETAILS pafd,
    DWORD dwInstance,
    DWORD fdwSupport
    )
{
    PInstData       pInst = (PInstData)LPARAM2PTR(dwInstance);
    PNameStore      pns;
    LPWAVEFORMATEX  lpwfx;
    UINT            index;
    BOOL            fRet;

    FUNC("FormatsCallback");


    pns = pInst->pnsTemp;

    /* Check for the case when something like CONVERT or SUGGEST
     * is used and we get called back for non matching tags.
     */
    if ((WORD)pInst->dwTag != pafd->pwfx->wFormatTag)
        GOTO_EXIT(fRet = TRUE);

    // we get the details from the callback
    STRVALUE(pafd->szFormat);
    StringCchCopy(pns->achName, NAMELEN(pns), pafd->szFormat);

    MashNameWithRate(pInst,pInst->pnsStrOut,pns,(pafd->pwfx));
    index = ComboBox_FindStringExact(pInst->hFormats,-1,
                                      pInst->pnsStrOut->achName);

    //
    //  if already in combobox, don't add another instance
    //
    if (CB_ERR != index)
        GOTO_EXIT(fRet = TRUE);

    {
        LPARAM lParam;

        lParam = (LPARAM) pafd->pwfx;

        if (!SendMessage(pInst->hwnd, MM_ACM_FORMATCHOOSE,
                         FORMATCHOOSE_FORMAT_VERIFY,lParam))
        {
            GOTO_EXIT(fRet = TRUE);
        }
    }

    lpwfx = (LPWAVEFORMATEX)CopyStruct(NULL,(LPBYTE)(pafd->pwfx),FORMAT_CHOOSE);

    if (!lpwfx)
        GOTO_EXIT(fRet = TRUE);

    index = ComboBox_AddString(pInst->hFormats,
                                pInst->pnsStrOut->achName);

    ComboBox_SetItemData(pInst->hFormats,index,(LPARAM)lpwfx);

    // Keep going
    fRet = TRUE;

EXIT:
    FUNC_EXIT();
    return (fRet);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL FNWCALLBACK
FiltersCallback (
    HACMDRIVERID          hadid,
    LPACMFILTERDETAILS    pafd,
    DWORD                 dwInstance,
    DWORD                 fdwSupport
    )
{
    PInstData       pInst = (PInstData)LPARAM2PTR(dwInstance);
    PNameStore      pns;
    UINT            index;
    LPWAVEFILTER    lpwf;
    BOOL            fRet;

    FUNC("FiltersCallback");


    pns = pInst->pnsTemp;

    if (pInst->dwTag != pafd->pwfltr->dwFilterTag)
        GOTO_EXIT(fRet = TRUE);

    index = ComboBox_FindStringExact(pInst->hFormats, -1, pafd->szFilter);

    //
    //  if already in combobox, don't add another instance
    //
    if (CB_ERR != index)
        GOTO_EXIT(fRet = TRUE);

    {
        LPARAM lParam;

        lParam = (LPARAM) pafd->pwfltr;

        if (!SendMessage(pInst->hwnd, MM_ACM_FILTERCHOOSE,
                         FILTERCHOOSE_FILTER_VERIFY,lParam))
        {
            GOTO_EXIT(fRet = TRUE);
        }
    }

    /*
     * Filter depending upon the flags.
     */
    lpwf = (LPWAVEFILTER)CopyStruct(NULL,(LPBYTE)(pafd->pwfltr),FILTER_CHOOSE);

    if (!lpwf)
        GOTO_EXIT(fRet = TRUE);

    // we get the details from the callback
    StringCchCopy(pns->achName, NAMELEN(pns), pafd->szFilter);

    index = ComboBox_AddString(pInst->hFormats, pns->achName);
    ComboBox_SetItemData(pInst->hFormats,index,(LPARAM)lpwf);

    fRet = TRUE;

EXIT:
    FUNC_EXIT();
    return (fRet);
}

