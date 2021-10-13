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

// AVTransport.cpp : Implementation of CAVTransport

#include "stdafx.h"


// Bit positions of DLNA.ORG_FLAGS
enum {
    DLNAFLAGS_SENDER_PACED         = 31,
    DLNAFLAGS_LOP_NPT              = 30,
    DLNAFLAGS_LOP_BYTES            = 29,
    DLNAFLAGS_PLAY_CONTAINER       = 28,
    DLNAFLAGS_S0_INCREAING         = 27,
    DLNAFLAGS_SN_INCREAING         = 26,
    DLNAFLAGS_RTSP_PAUSE           = 25,
    DLNAFLAGS_TRANSFER_STREAMING   = 24,
    DLNAFLAGS_TRANSFER_INTERACTIVE = 23,
    DLNAFLAGS_TRANSFER_BACKGROUND  = 22,
    DLNAFLAGS_HTTP_STALLING        = 21,
    DLNAFLAGS_DLNA_V1_5            = 20,
    DLNAFLAGS_BIT_19               = 19,
    DLNAFLAGS_BIT_18               = 18,
    DLNAFLAGS_BIT_17               = 17,
    DLNAFLAGS_LINK_PROTECTED       = 16,
    DLNAFLAGS_CLEARTEXT_FULL_SEEK  = 15,
    DLNAFLAGS_CLEARTEXT_LMTD_SEEK  = 14,
};

/////////////////////////////////////////////////////////////////////////////
// CAVTransport - continued



// FILE EXTENSION MANAGEMENT
//

HRESULT 
CAVTransport::ProcessURISuffix(void)
{
    HRESULT hr = S_OK;
    LPCWSTR pStart = m_MediaInfo.strCurrentURI;
    LPCWSTR pStop = NULL;
    LPCWSTR pSeek = NULL;
    DWORD length = m_MediaInfo.strCurrentURI.length();
    WCHAR regkey[0x40];  // enough space for registry string
    HKEY k = NULL;

    // Default: no file extension in URI
    m_URIFileExtension.clear();
    m_MediaClass = MediaClass_Unknown;

    // Find the main part of the URI.  Terminated by '?' or end of string
    pStop = wcschr(pStart, L'?');
    if (pStop == NULL)
    {
        pStop = pStart + length;
    }

    // Scan backwards for '.' or '/'.  period is success. slash is failure.
    for (pSeek = pStop-1; pSeek > pStart; pSeek--)
    {
        if (*pSeek == L'.' || *pSeek == L'/')
        {
            break;
        }
    }
    ChkBool(pSeek > pStart && pSeek < pStop-1 && *pSeek == L'.', S_FALSE);

    // Skip the dot and extract the suffix
    pSeek++;
    ChkBool(m_URIFileExtension.append(pSeek, pStop - pSeek), S_FALSE);

    // Generate and find the registry key for this media type 
    Chk(StringCchPrintfW(regkey, _countof(regkey), 
              L"COMM\\UPnPDevices\\MediaRenderer\\SupportedFiles\\%s", m_URIFileExtension));
    ChkBool(ERROR_SUCCESS == RegOpenKey(HKEY_LOCAL_MACHINE, regkey, &k), E_FAIL);
    RegCloseKey(k);

    // Translate from file extension to media class
    size_t extensionLength = (m_URIFileExtension.length() + 1) * sizeof(WCHAR);
    for (FileExtensionElement *pEntry = FileExtensionTable; pEntry->FileExtension != NULL; pEntry++)
    {
        LPCWSTR pStr = m_URIFileExtension;
        if (memcmp(pStr, pEntry->FileExtension, extensionLength) == 0)
        {
            m_MediaClass = pEntry->MediaClass;
            break;
        }
    }

Cleanup:
    return hr;
}




// METADATA MANAGEMENT
//

void 
CAVTransport::ClearMetadata(void)
{
    // Clear out all old information
    m_MediaClass = MediaClass_Unknown;
    m_bHasURIExtension = false;
    m_fCanSeek = false;

    m_PositionInfo.strTrackURI.clear();
    m_PositionInfo.strTrackDuration.assign(L"0:00:00");
    m_PositionInfo.strTrackMetaData.clear();
    m_PositionInfo.nTrack = 0;

    m_MediaInfo.strCurrentURI.clear();
    m_MediaInfo.strCurrentURIMetaData.clear();
    m_MediaInfo.strMediaDuration.assign(L"0:00:00");
    m_MediaInfo.nNrTracks = 0;

    m_URIFileExtension.clear();
    m_MetaDataFileExtension.clear();
    m_MetaDataForNotify.clear(); 
    m_MetaDataResource.clear();
    m_MetaDataTitle.clear();
    m_MetaDataProtocolInfo.clear(); 
    m_MetaDataDate.clear();
    m_MetaDataOrgFlags.clear();
    m_MetaDataOrgPN.clear();
    m_MetaDataOrgOp.clear();
    m_MetaDataTrackSize.clear();
    m_MetaDataRate.clear();
    m_MetaDataChannels.clear();
    m_NewURI.clear();
    m_MediaDurationMs.clear();
    m_MetaDataResolution.clear();

    m_pIMM = NULL;
}

DWORD 
CAVTransport::SetMetadata(LPCWSTR pszCurrentURI, LPCWSTR pszCurrentURIMetaData)
{
    DWORD hr = av::SUCCESS_AV;

    ChkBool(pszCurrentURI != NULL && pszCurrentURIMetaData != NULL, av::ERROR_AV_POINTER);

    // Clear out all old information
    ClearMetadata();

    // Set the received strings
    ChkBool( m_MediaInfo.strCurrentURI.assign(pszCurrentURI), av::ERROR_AV_OOM );
    ChkBool( m_MediaInfo.strCurrentURIMetaData.assign(pszCurrentURIMetaData), av::ERROR_AV_OOM );
    ChkBool( m_PositionInfo.strTrackURI.assign(pszCurrentURI), av::ERROR_AV_OOM );
    ChkBool( m_PositionInfo.strTrackMetaData.assign(m_MediaInfo.strCurrentURIMetaData), av::ERROR_AV_OOM );
    ChkUPnP( ProcessURISuffix(), av::ERROR_AV_OOM );

    // Parse and set all information
    m_MediaInfo.nNrTracks  = 1;
    m_PositionInfo.nTrack  = 1;
    ChkUPnP( ProcessMetaData(), av::ERROR_AV_UPNP_AVT_UNSUPPORTED_PLAY_FORMAT );

Cleanup:
    return hr;
}



HRESULT 
CAVTransport::ProcessMetaData(void)
{
    HRESULT hr = S_OK;
    IMediaMetadata *pIMM = NULL;

    // First locate the resource info for this URL
    Chk(ProcessMetaDataResource());

    // Acquire the easy data
    Chk(GetMetaDataTitle());
    Chk(GetMetaDataDate());
    Chk(GetMetaDataProtocolInfo());
    Chk(GetMetaDataFlags());
    Chk(GetMetaDataPN());
    Chk(GetMetaDataOp());
    Chk(GetMetaDataTrackSize());
    Chk(GetMetaDataDuration());
    Chk(GetMetaDataRate());
    Chk(GetMetaDataChannels());
    Chk(GetMetaDataResolution());

    // Acquire the hard data
    Chk(ProcessMetaDataFileExtension());
    Chk(ProcessMetaDataSeekability());
    Chk(ProcessMetaDataTrackDuration());
    Chk(ProcessMetaDataNotify());
    Chk(ProcessMetaDataFlags());

    RETAILMSG(1, (L"SetAVTransportURI called on file '%s'", m_MetaDataTitle));

    // Make a property bag
    CMediaMetadata *pMM = new CMediaMetadata;
    ChkBool(pMM != NULL, E_OUTOFMEMORY);
    Chk(pMM->Add(L"Application", L"DMR"));
    Chk(pMM->Add(L"dc:title", m_MetaDataTitle));
    Chk(pMM->Add(L"res", m_MetaDataResource));
    Chk(pMM->Add(L"dc:date", m_MetaDataDate));
    Chk(pMM->Add(L"DLNA.ORG_FLAGS", m_MetaDataOrgFlags));
    Chk(pMM->Add(L"DLNA.ORG_OP", m_MetaDataOrgOp));
    Chk(pMM->Add(L"DLNA.ORG_PN", m_MetaDataOrgPN));
    Chk(pMM->Add(L"protocolInfo", m_MetaDataProtocolInfo));
    Chk(pMM->Add(L"trackSize", m_MetaDataTrackSize));
    Chk(pMM->Add(L"duration", m_MediaInfo.strMediaDuration));
    Chk(pMM->Add(L"seekable", m_fCanSeek ? L"Seekable" : L"NotSeekable"));
    Chk(pMM->Add(L"fileExtension", m_MetaDataFileExtension));
    Chk(pMM->Add(L"rate", m_MetaDataRate));
    Chk(pMM->Add(L"channels", m_MetaDataChannels));
    Chk(pMM->Add(L"durationMs", m_MediaDurationMs));
    Chk(pMM->Add(L"LimitedRangeSeekable", m_bLimitedRange ? L"TRUE" : L"FALSE"));
    Chk(pMM->Add(L"Container", m_bIsContainer ? L"TRUE" : L"FALSE"));
    Chk(pMM->Add(L"resolution", m_MetaDataResolution));

    // add image scaling info to property bag
    WCHAR Screen[10];
    Chk(StringCchPrintfW(Screen, _countof(Screen), L"%u", GetSystemMetrics(SM_CXSCREEN)));
    Chk(pMM->Add(L"TargetWidth", Screen));
    Chk(StringCchPrintfW(Screen, _countof(Screen), L"%u", GetSystemMetrics(SM_CYSCREEN)));
    Chk(pMM->Add(L"TargetHeight", Screen));
    Chk(pMM->Add(L"ScaleOrClip", L"Scale"));

    // add media class to property bag
    WCHAR *pMediaClass = L"Unknown";
    switch(m_MediaClass)
    {
        case MediaClass_Image:   pMediaClass = L"Image"; break;
        case MediaClass_Audio:   pMediaClass = L"Audio"; break;
        case MediaClass_Video:   pMediaClass = L"Video"; break;
        case MediaClass_Unknown: pMediaClass = L"Unknown"; break;
        default:  ASSERT(0);
    }
    Chk(pMM->Add(L"MediaClass", pMediaClass));

    // Store the property bag's interface until later
    Chk(pMM->QueryInterface(IID_IMediaMetadata, (void**)&m_pIMM));
    Chk(ProcessMetaDataURI());
    
Cleanup:
    if (FAILED(hr))
    {
        delete pMM;
        return av::ERROR_AV_UPNP_ACTION_FAILED;
    }
    return S_OK;
}

HRESULT 
CAVTransport::ParseMetaItemXML(LPCWSTR pSource, LPCWSTR pItem, 
                       ce::wstring *pAttr, ce::wstring *pXML, LPCWSTR *ppNext)
{
    HRESULT hr = S_OK;
    WCHAR pattern1[0x30];
    LPCWSTR pTemp1 = NULL;
    LPCWSTR pTemp2 = NULL;
    LPCWSTR pTemp3 = NULL;
    WCHAR dummy;

    ChkBool(pSource != NULL && pItem != NULL, E_POINTER);
    if (pAttr != NULL)
    {
        pAttr->clear();
    }
    if (pXML != NULL)
    {
        pXML->clear();
    }

    // Prepare the search strings
    Chk(StringCchPrintfW(pattern1, _countof(pattern1), L"< %s %%c", pItem));

    // Search.  repeatedly if necessary
    while (1)
    {
        // Advance to next candidate substring
        pSource = wcschr(pSource, L'<');
        ChkBool(pSource != NULL, E_FAIL);

        // Parse this candidate substring for the Item
        if (swscanf_s(pSource, pattern1, &dummy, 1) != 1)
        {
            pSource++;
            continue;
        }

        // Find the start of the Attributes  after '<'
        pTemp1 = pSource + 1;
        ChkBool(*pTemp1 != 0, E_FAIL);

        // Find the end of the Attributes  '>'
        pTemp2 = wcschr(pSource, L'>');
        ChkBool(pTemp2 != NULL, E_FAIL);

        // Find the end of the payload  "</"
        pTemp3 = wcsstr(pTemp2, L"</");
        ChkBool(pTemp3 != NULL, E_FAIL);

        // Load the attributes
        if (pAttr != NULL)
        {
            Chk(pAttr->append(pTemp1, pTemp2-pTemp1));
        }

        // Load the XML payload
        if (pXML != NULL)
        {
            Chk(pXML->append(pTemp2+1, pTemp3 - (pTemp2 + 1)));
        }
        hr = S_OK;
        goto Cleanup;
    }

    // Not found
    hr = E_FAIL;

Cleanup:
    if (ppNext != NULL)
    {
        *ppNext = pTemp3;
    }
    return hr;
}

HRESULT 
CAVTransport::ParseMetaItemXML(LPCWSTR pItem, ce::wstring *pResult)
{
    return ParseMetaItemXML(m_MediaInfo.strCurrentURIMetaData, pItem, NULL, pResult, NULL);
}

HRESULT 
CAVTransport::ParseMetaItemAttr(LPCWSTR pSource, LPCWSTR pItem, ce::wstring *pResult)
{
    HRESULT hr = S_OK;
    LPCWSTR pTemp1 = NULL;
    LPCWSTR pTemp2 = NULL;
    LPCWSTR pTemp3 = NULL;

    ChkBool(pItem != NULL && pResult != NULL, E_POINTER);
    pResult->clear();

    // Look for the item name
    pTemp1 = wcsstr(pSource, pItem);
    ChkBool(pTemp1 != NULL, E_FAIL);

    // Look for and skip over the first '"' sign
    pTemp2 = wcschr(pTemp1, L'"');
    ChkBool(pTemp2 != NULL, E_FAIL);
    ++pTemp2;
    ChkBool(*pTemp2 != 0, E_FAIL);

    // Look for the second '"' sign
    pTemp3 = wcschr(pTemp2, L'"');
    ChkBool(pTemp3 != NULL, E_FAIL);

    // Copy out the found string
    Chk(pResult->append(pTemp2, pTemp3 - pTemp2));

Cleanup:
    return hr;
}

HRESULT 
CAVTransport::ParseMetaItemAttr(LPCWSTR pItem, ce::wstring *pResult)
{
    // First try the res information
    if (SUCCEEDED(ParseMetaItemAttr(m_MetaDataResource, pItem, pResult)))
    {
        return S_OK;
    }

    // Then try the whole metadata
    return ParseMetaItemAttr(m_MediaInfo.strCurrentURIMetaData, pItem, pResult);
}

HRESULT 
CAVTransport::ParseMetaItemSubAttr(LPCWSTR pSource, LPCWSTR pItem, ce::wstring *pResult)
{
    HRESULT hr = S_OK;
    LPCWSTR pTemp1 = NULL;
    LPCWSTR pTemp2 = NULL;
    LPCWSTR pTemp3 = NULL;

    ChkBool(pItem != NULL && pResult != NULL, E_POINTER);
    pResult->clear();

    // Look for the item name
    pTemp1 = wcsstr(pSource, pItem);
    ChkBool(pTemp1 != NULL, E_FAIL);

    // Look for and skip over the equal '=' sign
    pTemp2 = wcschr(pTemp1, L'=');
    ChkBool(pTemp2 != NULL, E_FAIL);
    pTemp2++;
    ChkBool(*pTemp2 != 0, E_FAIL);

    // Look for the terminating ';' or '"'
    pTemp3 = wcschr(pTemp2, L';');
    if (pTemp3 == NULL)
    {
        pTemp3 = wcschr(pTemp2, L'"');
    }
    ChkBool(pTemp3 != NULL, E_FAIL);

    // copy out the data
    Chk(pResult->append(pTemp2, pTemp3 - pTemp2));

Cleanup:
    return hr;
}

HRESULT 
CAVTransport::ParseMetaItemSubAttr(LPCWSTR pItem, ce::wstring *pResult)
{
    // First try the res information
    if (SUCCEEDED(ParseMetaItemSubAttr(m_MetaDataResource, pItem, pResult)))
    {
        return S_OK;
    }

    // Then try the whole metadata
    return ParseMetaItemSubAttr(m_MediaInfo.strCurrentURIMetaData, pItem, pResult);
}



HRESULT  inline
CAVTransport::GetMetaDataTitle(void)
{
    HRESULT hr = ParseMetaItemXML(L"dc:title", &m_MetaDataTitle);
    return FAILED(hr) ? S_FALSE : S_OK;
}

HRESULT  inline
CAVTransport::GetMetaDataDate(void)
{
    HRESULT hr = ParseMetaItemXML(L"dc:date", &m_MetaDataDate);
    return FAILED(hr) ? S_FALSE : S_OK;
}

HRESULT  inline
CAVTransport::GetMetaDataFlags(void)
{
    HRESULT hr = ParseMetaItemSubAttr(L"DLNA.ORG_FLAGS", &m_MetaDataOrgFlags);
    return FAILED(hr) ? S_FALSE : S_OK;
}

HRESULT  inline
CAVTransport::GetMetaDataPN(void)
{
    HRESULT hr = ParseMetaItemSubAttr(L"DLNA.ORG_PN", &m_MetaDataOrgPN);
    return FAILED(hr) ? S_FALSE : S_OK;
}

HRESULT  inline
CAVTransport::GetMetaDataOp(void)
{
    HRESULT hr = ParseMetaItemSubAttr(L"DLNA.ORG_OP", &m_MetaDataOrgOp);
    return FAILED(hr) ? S_FALSE : S_OK;
}

HRESULT  inline
CAVTransport::GetMetaDataProtocolInfo( void )
{  
    HRESULT hr = ParseMetaItemAttr(L"protocolInfo", &m_MetaDataProtocolInfo);
    return FAILED(hr) ? S_FALSE : S_OK;
}

HRESULT inline
CAVTransport::GetMetaDataTrackSize(void)
{
    HRESULT hr = ParseMetaItemAttr(L"size", &m_MetaDataTrackSize);
    return FAILED(hr) ? S_FALSE : S_OK;
}

HRESULT inline
CAVTransport::GetMetaDataDuration(void)
{
    HRESULT hr = ParseMetaItemAttr(L"duration", &m_MediaInfo.strMediaDuration);
    return FAILED(hr) ? S_FALSE : S_OK;
}

HRESULT inline
CAVTransport::GetMetaDataRate(void)
{
    HRESULT hr = ParseMetaItemAttr(L"rate", &m_MetaDataRate);
    return FAILED(hr) ? S_FALSE : S_OK;
}

HRESULT inline
CAVTransport::GetMetaDataChannels(void)
{
    HRESULT hr = ParseMetaItemAttr(L"channels", &m_MetaDataChannels);
    return FAILED(hr) ? S_FALSE : S_OK;
}

HRESULT inline
CAVTransport::GetMetaDataResolution(void)
{
    HRESULT hr = ParseMetaItemAttr(L"resolution", &m_MetaDataResolution);
    return FAILED(hr) ? S_FALSE : S_OK;
}




HRESULT 
CAVTransport::ProcessMetaDataResource(void)
{
    HRESULT hr = S_OK;
    LPCWSTR pCursor = m_MediaInfo.strCurrentURIMetaData;
    ce::wstring URL;

    // Seek through the file for resource info.  There may be multiple instances
    do
    {
        // find the start and stop of a resource
        Chk(ParseMetaItemXML(pCursor, L"res", &m_MetaDataResource, &URL, &pCursor));
        ChkBool(wcsstr(URL, m_MediaInfo.strCurrentURI) == NULL, S_OK);
    }
    while (pCursor != NULL);

Cleanup:
    return FAILED(hr) ? S_FALSE : S_OK;
}

HRESULT 
CAVTransport::ProcessMetaDataFileExtension(void)
{
    // Translate from DLNA Name to file extension
    for (MediaProfileElement *pEntry = MediaProfileTable; pEntry->DlnaName != NULL; pEntry++)
    {
        // Calculate the size of the prefix string
        int DlnaLen = wcslen(pEntry->DlnaName);
        if (DlnaLen <= m_MetaDataOrgPN.length())
        {
            LPCWSTR pStr = m_MetaDataOrgPN;
            if (memcmp(pStr, pEntry->DlnaName, DlnaLen * sizeof(WCHAR)) == 0)
            {
                m_MetaDataFileExtension.assign(pEntry->FileExtension);
                m_MediaClass = pEntry->MediaClass;
                return S_OK;
            }
        }
    }

    return S_FALSE;
}

HRESULT
CAVTransport::ProcessMetaDataSeekability()
{
    HRESULT hr = S_OK;
            
    // The default.  
    m_fCanSeek = FALSE;

    // Seek can only be performed on audio or video
    ChkBool(m_MediaClass == MediaClass_Audio || m_MediaClass == MediaClass_Video, S_FALSE)

    // DLNA.ORG_OP=AB, if present, gives the permission to seek
    // The 'B' value represents permission to seek by byte.
    // a value '1' means that seek is allowed.   
    // A value '0' means that seek is not allowed
    ChkBool(m_MetaDataOrgOp.length() >= 2, S_FALSE);
    m_fCanSeek = (m_MetaDataOrgOp[1] == L'1') ? TRUE : FALSE;

Cleanup:
    return hr;
}

HRESULT
CAVTransport::ProcessMetaDataTrackDuration(void)
{
    HRESULT hr = S_OK;

    // Zero out the other trackduration variable
    m_PositionInfo.strTrackDuration.clear();

    // if no duration, make it the default
    if (m_MediaInfo.strMediaDuration.length() == 0)
    {
        Chk(m_MediaInfo.strMediaDuration.assign(L"0:00:00"));
        Chk(m_PositionInfo.strTrackDuration.assign(m_MediaInfo.strMediaDuration));
    }

    // Now copy it into the other duration string
    Chk(m_PositionInfo.strTrackDuration.assign(m_MediaInfo.strMediaDuration));

    // Parse the information into a dshow timestamp
    if (FAILED(ParseMediaTime( m_MediaInfo.strMediaDuration, &m_llDuration)))
    {
        m_llDuration = 0;
    }

    // Convert to a string
    WCHAR temp[0x20];
    StringCchPrintfW(temp, _countof(temp), L"%llu", m_llDuration);
    m_MediaDurationMs.assign(temp);

Cleanup:
    return hr;
}

// convert the MetaData to a format to be embedded in LastChange.
HRESULT
CAVTransport::ProcessMetaDataNotify(void)
{
    HRESULT hr = S_OK;
    WCHAR   aCharacters[]       = {L'<', L'>', L'"'};
    WCHAR*  aEntityReferences[] = {L"&amp;lt;", L"&amp;gt;", L"&amp;quot;"};
    bool    bReplaced;
    
    // In case of failure
    m_MetaDataForNotify.clear();
    ChkBool( m_MetaDataForNotify.reserve( 2 * m_MediaInfo.strCurrentURIMetaData.length()), av::ERROR_AV_OOM );
    
    for( const WCHAR* pwch = m_MediaInfo.strCurrentURIMetaData; *pwch; pwch++ )
    {
        bReplaced = false;

        for(int i = 0; i < sizeof(aCharacters)/sizeof(*aCharacters); ++i)
        {
            if(*pwch == aCharacters[i])
            {
                ChkBool( m_MetaDataForNotify.append( aEntityReferences[ i ] ), av::ERROR_AV_OOM );
                bReplaced = true;
                break;
            }
        }

        if( !bReplaced )
        {
            ChkBool( m_MetaDataForNotify.append( pwch, 1 ), av::ERROR_AV_OOM );
        }
    }

Cleanup:
    return hr;
}


HRESULT
CAVTransport::ProcessMetaDataFlags(void)
{
    // Read the bits.  return 1 is set, 0 is clear, -1 is absent
    int limitedRange = ReadFlagsBit_V1_5(DLNAFLAGS_LOP_BYTES);
    int isContainer = ReadFlagsBit_V1_5(DLNAFLAGS_PLAY_CONTAINER);

    // set the flags.  1 is true, 0 and -1 are false
    m_bLimitedRange = (limitedRange == 1) ? true : false;
    m_bIsContainer = (isContainer == 1) ? true : false;

    return S_OK;
}


// Generate a URI with embedded metadata
static const DWORD dwEstimatedSize = 0x100;
HRESULT
CAVTransport::ProcessMetaDataURI(void)
{
    HRESULT hr = S_OK;

    // Generate a new URL just like the old one
    m_NewURI.clear();
    ChkBool(m_NewURI.reserve(dwEstimatedSize), av::ERROR_AV_OOM);
    ChkBool(m_NewURI.append(m_MediaInfo.strCurrentURI), av::ERROR_AV_OOM);

    // Add Metadata Indicator extension 
    ChkBool(m_NewURI.append(m_bHasURIExtension ? L"&" : L"?"), av::ERROR_AV_OOM);
    ChkBool(m_NewURI.append(L"WinCEDLNADMRMetaDataInfoStart"), av::ERROR_AV_OOM);

    // Add file extension information
    if (m_MetaDataFileExtension.length() != 0)
    {
        ChkBool(m_NewURI.append(L"&MediaType="), av::ERROR_AV_OOM);
        ChkBool(m_NewURI.append(m_MetaDataFileExtension), av::ERROR_AV_OOM);
    }
    else if (m_URIFileExtension.length() != 0)
    {
        ChkBool(m_NewURI.append(L"&MediaType="), av::ERROR_AV_OOM);
        ChkBool(m_NewURI.append(m_URIFileExtension), av::ERROR_AV_OOM);
    }
    else
    {
        ChkBool(0, av::ERROR_AV_UPNP_AVT_UNSUPPORTED_PLAY_FORMAT);
    }

    // Add no-seekability indicator as needed
    if (!m_fCanSeek)
    {
        ChkBool(m_NewURI.append(L"&CanSeek=FALSE"), av::ERROR_AV_OOM);
    }

    // End of additions
    ChkBool(m_NewURI.append(L"&EndOfInfo"), av::ERROR_AV_OOM);

Cleanup:
    return hr;
}



int
CAVTransport::ReadFlagsBit_V1_5(DWORD bitnum)
{
    WORD bytenum = (31 - bitnum) / 4;
    WORD bitmask = 1 << (bitnum % 4);
    WORD value = 0;

    // Check limits of bitnumber and bytenumber
    if (bitnum > 31)
    {
        return -1;
    }
    if (bytenum >= m_MetaDataOrgFlags.length())
    {
        return -1;
    }

    // Convert character to value
    WORD w = m_MetaDataOrgFlags[bytenum];
    if (L'0' <= w && w <= L'9')
    {
        value = w - L'0';
    }
    else if (L'A' <= w && w <= L'F')
    {
        value = w - L'A' + 10;
    }
    else if (L'a' <= w && w <= L'f')
    {
        value = w - L'a' + 10;
    }
    else
    {
        return -1;
    }

    // Return the result
    return (value & bitmask) ? 1 : 0;
}
