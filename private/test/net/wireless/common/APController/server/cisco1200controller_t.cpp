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
// ----------------------------------------------------------------------------
//
// Use of this source code is subject to the terms of the Microsoft end-user
// license agreement (EULA) under which you licensed this SOFTWARE PRODUCT.
// If you did not accept the terms of the EULA, you are not authorized to use
// this source code. For a copy of the EULA, please see the LICENSE.RTF on your
// install media.
//
// ----------------------------------------------------------------------------
//
// Implementation of the Cisco1200Controller_t class.
//
// ----------------------------------------------------------------------------

#include "Cisco1200Controller_t.hpp"

#include "AccessPointState_t.hpp"
#include "CiscoPort_t.hpp"

#include <assert.h>
#include <strsafe.h>

// Turns on/off various levels of debugging:
//

using namespace ce::qa;


/* ========================================================================= */
/* =========================== Local Definitions =========================== */
/* ========================================================================= */

// CLI command/response strings:
//
static const char   s_HdweTypeHeader[]     = "802.11";
static const size_t s_HdweTypeHeaderChars  = COUNTOF(s_HdweTypeHeader)-1;

static const char   s_RadioIntfHeader[]    = "Dot11Radio";
static const size_t s_RadioIntfHeaderChars = COUNTOF(s_RadioIntfHeader)-1;


/* ========================================================================= */
/* ============================ Local Functions ============================ */
/* ========================================================================= */

// ----------------------------------------------------------------------------
//
// Encodes the specified string value into HEX.
//
static DWORD
EncodeHexKey(
    const char *pKey,
    const char *pKeyName,
    char       *pBuffer,
    size_t     *pBufferBytes,
    size_t       KeyBytes = INT_MAX / 2)
{
    static const char *hexmap = "0123456789ABCDEF";

    size_t length = *pBufferBytes - 1;
    if (length >= KeyBytes)
        length  = KeyBytes;
    else
    {
        HRESULT hr = StringCchLengthA(pKey, length, &length);
        if (FAILED(hr))
        {
            LogError(TEXT("Can't encode HEX %hs value \"%.128hs\": %s"),
                     pKeyName, pKey, HRESULTErrorText(hr));
            return HRESULT_CODE(hr);
        }
    }

    for (size_t kx = 0 ; kx < length ; ++kx)
    {
        UINT ch = (UINT)pKey[kx];
        *(pBuffer++) = hexmap[(ch >> 4) & 0xf];
        *(pBuffer++) = hexmap[ ch       & 0xf];
    }

   *pBuffer = '\0';
   *pBufferBytes = length * 2;
    return NO_ERROR;
}

// ----------------------------------------------------------------------------
//
// Decodes the specified HEX key.
//
static DWORD
DecodeHexKey(
    const char *pKey,
    const char *pKeyName,
    char       *pBuffer,
    size_t     *pBufferBytes)
{
    HRESULT hr = S_OK;
    size_t length = 0;

    hr = StringCchLengthA(pKey, (*pBufferBytes - 1) * 2, &length);
    if (FAILED(hr))
    {
        LogError(TEXT("Can't decode HEX %hs value \"%.128hs\": %s"),
                 pKeyName, pKey, HRESULTErrorText(hr));
        return HRESULT_CODE(hr);
    }

    length = (length + 1) / 2;

    const char *kp = pKey;
    for (size_t hx = 0 ; hx < length ; ++hx)
    {
        UINT abyte = 0;
        for (int nx = 0 ; nx < 2 ; ++nx)
        {
            UINT ch = static_cast<UINT>(*(kp++));
            switch (ch)
            {
                case '0': case '1': case '2': case '3': case '4':
                case '5': case '6': case '7': case '8': case '9':
                    ch -= '0';
                    break;
                case 'a': case 'b': case 'c':
                case 'd': case 'e': case 'f':
                    ch -= 'a' - 10;
                    break;
                case 'A': case 'B': case 'C':
                case 'D': case 'E': case 'F':
                    ch -= 'A' - 10;
                    break;
                default:
                    LogError(TEXT("Bad HEX %hs value \"%.128hs\""),
                             pKeyName, pKey);
                    return ERROR_INVALID_PARAMETER;
            }
            abyte <<= 4;
            abyte |= ch;
        }
        *(pBuffer++) = static_cast<BYTE>(abyte);
        if (*kp == '.' || *kp == ':' || *kp == '-')
             kp++;
    }

   *pBuffer = '\0';
   *pBufferBytes = length;
    return NO_ERROR;
}

// ----------------------------------------------------------------------------
//
// Parses an SSID name from the specified sentence.
//
static bool
ParseSsid(
    const TelnetWords_t &Words,
    int                  StartAt,
    char                *pBuffer)
{
    pBuffer[0] = '\0';

    // Locate the SSID header.
    for (; StartAt < Words.Size() ; ++StartAt)
        if (_stricmp(Words[StartAt], "ssid") == 0)
            break;

    // Concatenate the remaining word(s).
    char *pBuff = pBuffer;
    const char *pBuffEnd = &pBuffer[CiscoPort_t::MaxSsidChars];
    while (++StartAt < Words.Size() && pBuff < pBuffEnd)
    {
        const char *pWord = Words[StartAt];
        if (pBuffer[0]) *(pBuff++) = ' ';
        for (; pBuff < pBuffEnd ; ++pBuff, ++pWord)
            if ((*pBuff = *pWord) == '\0')
                break;
    }
   *pBuff = '\0';

    // Remove leading and trailing brackets (if any).
    pBuffEnd = pBuff;
    if (pBuffer[0] == '[')
    {
        int braces = 1;
        for (pBuff = &pBuffer[1] ; pBuff < pBuffEnd ; ++pBuff)
            switch (*pBuff)
            {
            case '[': braces++; break;
            case ']': braces--; break;
            }
        if (braces == 0)
        {
            pBuffEnd--;
            pBuff = pBuffer;
            for (const char *pKeep = &pBuffer[1] ; pKeep < pBuffEnd ;)
                *(pBuff++) = *(pKeep++);
           *pBuff = '\0';
            pBuffEnd--;
        }
    }

    return (pBuffEnd > pBuffer);
}

/* ========================================================================= */
/* ============================ Cisco1200Request =========================== */
/* ========================================================================= */

// ----------------------------------------------------------------------------
//
// Cisco1200Request_t encapsulates all the data and methods required to
// process a single Cisco 1200-series AP interaction.
//
class Cisco1200Request_t
{
private:

    // Cisco device-type controller:
    Cisco1200Controller_t *m_pCtlr;

    // Telnet (CLI) controller:
    CiscoPort_t *m_pCLI;

    // Access Point information:
    const CiscoAPInfo_t &m_APInfo;

    // Device's current configuration:
    AccessPointState_t *m_pState;

    // Name of access point:
    char m_APName[96];

    // Bit-maps indicating which fields need to be updated:
    int m_UpdatesRequested; // Changes requested by the user
    int m_UpdatesRemaining; // Changes remaining in this page

    // True if the SSID is in the global SSID list:
    bool m_bSsidAssociated;

public:

    // Constructor and destructor:
    Cisco1200Request_t(
        Cisco1200Controller_t *pCtlr,
        CiscoPort_t           *pCLI,
        const CiscoAPInfo_t   &APInfo,
        AccessPointState_t    *pState);
   ~Cisco1200Request_t(void)
    {;}

    // Loads all the settings:
    DWORD LoadSettings(void);

    // Updates all the settings:
    DWORD UpdateSettings(const AccessPointState_t &NewState);

private:

   // Private nop assignement operator
   Cisco1200Request_t&  operator = (const Cisco1200Request_t& other);

    // Loads or updates the passphrase:
    DWORD
    LoadPassphrase(void);
    DWORD
    UpdatePassphrase(
        const AccessPointState_t &NewState,
        bool                      Stalled);

    // Loads or updates the radio status:
    DWORD
    LoadRadioStatus(void);
    DWORD
    UpdateRadioStatus(
        const AccessPointState_t &NewState,
        bool                      Stalled);

    // Loads or updates the Radius information:
    DWORD
    LoadRadiusInfo(void);
    DWORD
    UpdateRadiusInfo(
        const AccessPointState_t &NewState,
        bool                      Stalled);

    // Loads or updates the security-mode settings:
    DWORD
    LoadSecurityModes(void);
    DWORD
    UpdateSecurityModes(
        const AccessPointState_t &NewState,
        bool                      Stalled);

    // Loads or updates the SSID:
    DWORD
    LoadSsid(void);
    DWORD
    UpdateSsid(
        const AccessPointState_t &NewState,
        bool                      Stalled);

    // Loads or updates the SSID-broadcast flag:
    DWORD
    LoadSsidBroadcast(void);
    DWORD
    UpdateSsidBroadcast(
        const AccessPointState_t &NewState,
        bool                      Stalled);

    // Loads or updates the WEP keys:
    DWORD
    LoadWepKeys(void);
    DWORD
    UpdateWepKeys(
        const AccessPointState_t &NewState,
        bool                      Stalled);

    // Loads or updates the WMM capability:
    DWORD
    LoadWmmCapability(void);
    DWORD
    UpdateWmmCapability(
        const AccessPointState_t &NewState,
        bool                      Stalled);

    // Sends the specified command and stores the response in m_Response:
    // If necessary, changes to the specified command-mode before sending
    // the command.
    DWORD
    SendCommand(
        CiscoPort_t::CommandMode_e CommandMode,
        const char                *pFormat, ...);

    // Sends the specified command, stores the response in m_Response and
    // and checks it for an error message:
    // (Any response is assumed to be an error message.)
    // If necessary, changes to the specified command-mode before sending
    // the command.
    // If appropriate, issues an error message containing the specified
    // operation name and the error message sent by the server.
    DWORD
    VerifyCommand(
        CiscoPort_t::CommandMode_e CommandMode,
        const char                *pOperation,
        const char                *pFormat, ...);

};

// ----------------------------------------------------------------------------
//
// Constructor.
//
Cisco1200Request_t::
Cisco1200Request_t(
    Cisco1200Controller_t *pCtlr,
    CiscoPort_t           *pCLI,
    const CiscoAPInfo_t   &APInfo,
    AccessPointState_t    *pState)
    : m_pCtlr(pCtlr),
      m_pCLI(pCLI),
      m_APInfo(APInfo),
      m_pState(pState),
      m_UpdatesRequested(0),
      m_UpdatesRemaining(0),
      m_bSsidAssociated(false)
{
    StringCchPrintfA(m_APName, COUNTOF(m_APName),
                    "Cisco %hs at %ls",
                     m_APInfo.ModelName,
                     m_pCLI->GetServerHost());
}

// ----------------------------------------------------------------------------
//
// Loads all the settings.
//
DWORD
Cisco1200Request_t::
LoadSettings(void)
{
    DWORD result;

    // Get the SSID.
    // Loaded first because it's needed for other settings.
    result = LoadSsid();
    if (NO_ERROR != result)
        return result;

    // Get the initial running configuration.
    result = m_pCLI->ReadRunningConfig();
    if (NO_ERROR != result)
        return result;

    // Get the security modes.
    // This, like the SSID needs to be early.
    result = LoadSecurityModes();
    if (NO_ERROR != result)
        return result;

    // Get the passphrase.
    result = LoadPassphrase();
    if (NO_ERROR != result)
        return result;

    // Get the radio state.
    result = LoadRadioStatus();
    if (NO_ERROR != result)
        return result;

    // Get the Radius information.
    result = LoadRadiusInfo();
    if (NO_ERROR != result)
        return result;

    // Get the broadcast-SSID flag.
    result = LoadSsidBroadcast();
    if (NO_ERROR != result)
        return result;

    // Get the WEP keys.
    result = LoadWepKeys();
    if (NO_ERROR != result)
        return result;

    // Get the WMM capability.
    result = LoadWmmCapability();
    if (NO_ERROR != result)
        return result;

    return NO_ERROR;
}

#ifdef DEBUG
// ----------------------------------------------------------------------------
//
// Displays the specified field-mask settings:
//
static void
LogFieldSettings(int Fields)
{
    for (int mask = 1 ; mask <= AccessPointState_t::FieldMaskAll ; mask <<= 1)
    {
        if ((Fields & mask) == 0)
            continue;
        const char *name = "unknown";
        switch (mask)
        {
        case AccessPointState_t::FieldMaskCapabilities:  name = "Capabilities";  break;
        case AccessPointState_t::FieldMaskSsid:          name = "Ssid";          break;
        case AccessPointState_t::FieldMaskBSsid:         name = "BSsid";         break;
        case AccessPointState_t::FieldMaskSsidBroadcast: name = "SsidBroadcast"; break;
        case AccessPointState_t::FieldMaskRadioState:    name = "RadioState";    break;
        case AccessPointState_t::FieldMaskSecurityMode:  name = "SecurityMode";  break;
        case AccessPointState_t::FieldMaskRadius:        name = "Radius";        break;
        case AccessPointState_t::FieldMaskWEPKeys:       name = "WEPKeys";       break;
        case AccessPointState_t::FieldMaskPassphrase:    name = "Passphrase";    break;
        }
        LogDebug(TEXT("[AC]     0x%04X = %hs"), mask, name);
    }
}
#endif

// ----------------------------------------------------------------------------
//
// Updates the configuration settings:
//
DWORD
Cisco1200Request_t::
UpdateSettings(const AccessPointState_t &NewState)
{
    DWORD result = NO_ERROR;

    static const int OldStatesToCompare = 3;
    AccessPointState_t oldStates[OldStatesToCompare];

    // Initialize the fields-to-be-changed flags.
    m_UpdatesRequested = NewState.GetFieldFlags();
    LogDebug(TEXT("[AC] Fields to be updated: 0x%04X"), m_UpdatesRequested);
#ifdef DEBUG
    LogFieldSettings(m_UpdatesRequested);
#endif

    int radioEnableDelays = 0;
    AccessPointState_t updates;
    for (int tries = 0 ; ; ++tries)
    {
        // Load the settings.
        result = LoadSettings();
        if (NO_ERROR != result)
            break;

        // We may change the list of updates required as we proceed so
        // we have to make sure the "new" record containing everything
        // we know about the current state of the AP in case we decide,
        // for example, that we need to update the passphrase and they
        // didn't send us one.
        if (tries == 0)
        {
            updates = *m_pState;
            updates.MergeFields(m_UpdatesRequested, NewState);
            updates.SetFieldFlags(m_UpdatesRequested);

            // Fill in defaults for missing fields.
            if (updates.GetSsid()[0] == TEXT('\0'))
            {
                const TCHAR *pDefSsid = m_pState->GetSsid();
                if (pDefSsid[0] == TEXT('\0'))
                    pDefSsid = m_pState->GetBSsidText();
                updates.SetSsid(pDefSsid);
            }
            if (updates.GetRadiusServer() == 0)
            {
                DWORD defServer = m_pState->GetRadiusServer();
                if (defServer == 0)
                    defServer = (1<<24) | (10<<8) | 10; // 10.10.0.1
                updates.SetRadiusServer(defServer);
            }
            if (updates.GetRadiusPort() == 0)
            {
                DWORD defPort = m_pState->GetRadiusPort();
                if (defPort == 0)
                    defPort = 1812;
                updates.SetRadiusPort(defPort);
            }
            if (updates.GetRadiusSecret()[0] == TEXT('\0'))
            {
                const TCHAR *pDefSecret = m_pState->GetRadiusSecret();
                if (pDefSecret[0] == TEXT('\0'))
                    pDefSecret = TEXT("0123456789");
                updates.SetRadiusSecret(pDefSecret);
            }
        }

        // Store the current state for checking for stalled updates.
        oldStates[tries % OldStatesToCompare] = *m_pState;

        // Determine which fields (if any) still need to be updated.
        m_UpdatesRemaining = m_pState->CompareFields(m_UpdatesRequested, updates);
        if (0 == m_UpdatesRemaining)
            break;

        LogDebug(TEXT("[AC] Updates remaining at pass %d = 0x%04X"),
                 tries+1, m_UpdatesRemaining);
#ifdef DEBUG
        LogFieldSettings(m_UpdatesRemaining);
#endif

        // Make sure we're not endlessly looping with no progress.
        // We do this by comparing the current AP configuration against
        // the AP's state after the last few page-updates. It would seem
        // we could do as well by just comparing against the last state,
        // but that wouldn't detect "cycles" where, for example, changing
        // WEP key #1 causes key #2 to revert to a previous value and
        // going back to fix key #2 causes key #1 to revert and so on.
        // (Believe it or not, this actually happens.)
        bool stalled = false;
        if (tries > 0)
        {
            int first = tries % OldStatesToCompare;
            int prev = first;
            for (int tx = 0 ; tx < tries ; ++tx)
            {
                if (prev == 0)
                     prev = OldStatesToCompare - 1;
                else prev--;
                if (prev == first)
                    break;
                if (memcmp(m_pState, &(oldStates[prev]),
                           sizeof(AccessPointState_t)) == 0)
                {
                    stalled = true;
                    break;
                }
            }
        }

        // SSID:
        // Updated first because it's needed for other settings.
        if (!m_bSsidAssociated
         || (m_UpdatesRemaining & (int)AccessPointState_t::FieldMaskSsid))
        {
            result = UpdateSsid(updates, stalled);
            if (NO_ERROR != result)
                break;
        }

        // Security modes:
        // Likewise for security modes.
        if (m_UpdatesRemaining & (int)AccessPointState_t::FieldMaskSecurityMode)
        {
            result = UpdateSecurityModes(updates, stalled);
            if (NO_ERROR != result)
                break;
        }

        // Passphrase:
        if (m_UpdatesRemaining & (int)AccessPointState_t::FieldMaskPassphrase)
        {
            result = UpdatePassphrase(updates, stalled);
            if (NO_ERROR != result)
                break;
        }

        // Radius server information:
        if (m_UpdatesRemaining & (int)AccessPointState_t::FieldMaskRadius)
        {
            result = UpdateRadiusInfo(updates, stalled);
            if (NO_ERROR != result)
                break;
        }

        // Broadcast-SSID flag:
        if (m_UpdatesRemaining & (int)AccessPointState_t::FieldMaskSsidBroadcast)
        {
            result = UpdateSsidBroadcast(updates, stalled);
            if (NO_ERROR != result)
                break;
        }

        // WEP keys:
        if (m_UpdatesRemaining & (int)AccessPointState_t::FieldMaskWEPKeys)
        {
            result = UpdateWepKeys(updates, stalled);
            if (NO_ERROR != result)
                break;
        }

        // WMM Capability:
        if (m_UpdatesRemaining & (int)AccessPointState_t::FieldMaskCapabilities)
        {
            result = UpdateWmmCapability(updates, stalled);
            if (NO_ERROR != result)
                break;
        }

        // Radio-state:
        if (m_UpdatesRemaining & (int)AccessPointState_t::FieldMaskRadioState)
        {
            // Enabling the radio takes a few seconds, so when we get
            // in the mode where only the radio is holding things up,
            // wait a while before giving up.
            static const int MaxRadioEnableDelays = 30;
            static const long RadioEnableDelayMS = 1000;
            if (stalled
             && m_UpdatesRemaining == (int)AccessPointState_t::FieldMaskRadioState
             && radioEnableDelays++ < MaxRadioEnableDelays)
            {
                Sleep(RadioEnableDelayMS);
                LogDebug(TEXT("[AC] Delayed %lusec to let radio catch up"),
                        (RadioEnableDelayMS * radioEnableDelays) / 1000);
                stalled = false;
            }
            
            result = UpdateRadioStatus(updates, stalled);
            if (NO_ERROR != result)
                break;
        }
    }

    return result;
}

// ----------------------------------------------------------------------------
//
// Loads or updates the passphrase.
//
DWORD
Cisco1200Request_t::
LoadPassphrase(void)
{
    DWORD result = NO_ERROR;

    // If the security modes don't use the passphrase, just return
    // success. This won't represent what's in the AP, but it won't
    // matter until they modify the security modes to something that
    // uses it.
    switch (m_pState->GetAuthMode())
    {
    case APAuthWPA_PSK:
    case APAuthWPA2_PSK:
        break;
    default:
        return NO_ERROR;
    }
    
    // Look for the appropriate global SSID in the running configuration.
    TelnetLines_t *pConfig = m_pCLI->GetRunningConfig();
    assert(NULL != pConfig);
    
    m_pState->SetPassphrase(""); bool inSsid = false;
    for (int lx = 0 ; pConfig && lx < pConfig->Size() ; ++lx)
    {
        const TelnetWords_t &words = pConfig->GetLine(lx);
        if (words.Size() > 2
         && _stricmp(words[0], "Dot11") == 0
         && _stricmp(words[1], "ssid")  == 0
         &&  strcmp (words[2], m_pCLI->GetSsid()) == 0)
             inSsid = true;
        else
        if (!inSsid)
            continue;

        // Dot11 ssid apname
        //     authentication open
        //     authentication key-management wpa
        //     wpa-psk [hex|ascii] 0 0123456789   <==
        // !
        if (words[0][0] == '!')
            break;
        if (words.Size() < 4 || _stricmp(words[0], "wpa-psk") != 0)
            continue;
        
        char   key[AccessPointState_t::MaxPassphraseChars+1];
        size_t keyBytes = COUNTOF(key);
        
        if (_stricmp(words[1], "hex") == 0)
        {
            result = DecodeHexKey(words[3], "passphrase", key, &keyBytes);
        }
        else
        {
            // Merge multiple ASCII words (if necessary).
            char       *pKey    = &key[0];
            const char *pKeyEnd = &key[AccessPointState_t::MaxPassphraseChars];
            for (int wx = 3 ; wx < words.Size() && pKey < pKeyEnd ; ++wx)
            {
                const char *pWord = words[wx];
                if (key[0]) *(pKey++) = ' ';
                for (; pKey < pKeyEnd ; ++pKey, ++pWord)
                    if ((*pKey = *pWord) == '\0')
                        break;
            }
           *pKey = '\0';
        }
        
        if (NO_ERROR == result)
        {
            HRESULT hr = m_pState->SetPassphrase(key);
            result = HRESULT_CODE(hr);
        }
        
        return result;
    }

    LogWarn(TEXT("[AC] No PSK passphrase found for %s authentication"),
            APAuthMode2String(m_pState->GetAuthMode()));

    return NO_ERROR;
}

DWORD
Cisco1200Request_t::
UpdatePassphrase(
    const AccessPointState_t &NewState,
    bool                      Stalled)
{
    DWORD result = NO_ERROR;
    if (Stalled)
        result = ERROR_INVALID_STATE;

    char passphrase[AccessPointState_t::MaxPassphraseChars+1];
    NewState.GetPassphrase(passphrase, COUNTOF(passphrase));

    // If the security modes don't use the passphrase, just transfer
    // the new key to the current state. This won't represent what's
    // in the AP, but it won't matter until they modify the security
    // modes to something that uses it. At that point we can update
    // the AP properly.
    switch (NewState.GetAuthMode())
    {
    case APAuthWPA_PSK:
    case APAuthWPA2_PSK:
        break;
    default:
        m_pState->SetPassphrase(passphrase);
        return NO_ERROR;
    }

    // Set the passphrase.
    if (NO_ERROR == result)
        result = VerifyCommand(CiscoPort_t::CommandModeSsid,
                               "setting PSK passphrase",
                               "wpa-psk ascii 0 %s",
                               passphrase);

    if (NO_ERROR == result)
        LogDebug(TEXT("[AC]   set passphrase to \"%hs\""), passphrase);
    else
    {
        LogError(TEXT("Unable to update Passphrase to \"%hs\" on %hs: %s"),
                 passphrase, m_APName,
                 Win32ErrorText(result));
        return result;
    }

    return NO_ERROR;
}

// ----------------------------------------------------------------------------
//
// Loads or updates the radio status.
//
DWORD
Cisco1200Request_t::
LoadRadioStatus(void)
{
    DWORD result;

    result = SendCommand(CiscoPort_t::CommandModePrivileged,
                        "show interface %s %d | include %s",
                         s_RadioIntfHeader,
                         m_pCLI->GetRadioInterface(),
                         s_RadioIntfHeader);
    if (NO_ERROR != result)
    {
        LogError(TEXT("Can't read interface %d information from %hs: %s"),
                 m_pCLI->GetRadioInterface(),
                 m_APName,
                 Win32ErrorText(result));
        return result;
    }

    // The Dot11Radio line should be around here somewhere...
    TelnetLines_t *pResponse = m_pCLI->GetCommandResponse();
    assert(NULL != pResponse);
    
    for (int lx = 0 ; ; ++lx)
    {
        if (!pResponse || lx >= pResponse->Size())
        {
            LogError(TEXT("Can't determine radio state for %hs"), m_APName);
            return ERROR_INVALID_PARAMETER;
        }

        const TelnetWords_t &words = pResponse->GetLine(lx);
        if (words.Size() > 1
         && _strnicmp(words[0], s_RadioIntfHeader, s_RadioIntfHeaderChars) == 0)
        {
            //    Dot11Radio0 is up
            // || Dot11Radio0 is reset
            m_pState->SetRadioState(_strnicmp(words[2], "up", 2) == 0);
            break;
        }
    }

    return NO_ERROR;
}

DWORD
Cisco1200Request_t::
UpdateRadioStatus(
    const AccessPointState_t &NewState,
    bool                      Stalled)
{
    DWORD result = NO_ERROR;
    if (Stalled)
        result = ERROR_INVALID_STATE;

    const char *pWhich;
    if (NewState.IsRadioOn())
         pWhich = "enabl";
    else pWhich = "disabl";

    if (NO_ERROR == result)
        result = VerifyCommand(CiscoPort_t::CommandModeInterface,
                               "switching radio state",
                               "%sshutdown",
                               NewState.IsRadioOn()? "no " : "");

    if (NO_ERROR == result)
        LogDebug(TEXT("[AC]   %hsed radio"), pWhich);
    else
    {
        LogError(TEXT("Unable to %hse radio interface %d on %hs: %s"),
                 pWhich,
                 m_pCLI->GetRadioInterface(),
                 m_APName,
                 Win32ErrorText(result));
        return result;
    }

    return NO_ERROR;
}

// ----------------------------------------------------------------------------
//
// Loads or updates the Radius information.
//
DWORD
Cisco1200Request_t::
LoadRadiusInfo(void)
{
    // Look for a "radius-server host" line in the running configs.
    TelnetLines_t *pConfig = m_pCLI->GetRunningConfig();
    assert(NULL != pConfig);
    
    for (int lx = 0 ; pConfig && lx < pConfig->Size() ; ++lx)
    {
        const TelnetWords_t &words = pConfig->GetLine(lx);

        if (words.Size() < 3
         || _stricmp(words[0], "radius-server") != 0
         || _stricmp(words[1], "host") != 0)
            continue;

        // Interpret the server address.
        DWORD server = inet_addr(words[2]);
        if (INADDR_NONE != server)
            m_pState->SetRadiusServer(server);
        else
        {
            LogWarn(TEXT("[AC] Radius server address '%hs' is invalid"),
                    words[2]);
            continue;
        }

        // Find and interpret the port (if any).
        m_pState->SetRadiusPort(0);
        for (int px = words.Size() - 1 ; --px >= 3 ;)
            if (_stricmp(words[px], "auth-port") == 0)
            {
                px++;
                long port = strtol(words[px], NULL, 10);
                if (0 <= port && port < 32*1024)
                    m_pState->SetRadiusPort(port);
                else
                {
                    LogWarn(TEXT("[AC] Radius server port '%hs' is invalid"),
                            words[px]);
                }
                break;
            }

        // Find the secret-key (if any).
        m_pState->SetRadiusSecret("");
        for (int kx = words.Size() - 1 ; --kx >= 3 ;)
            if (_stricmp(words[kx], "key") == 0)
            {
                kx++;        
                char   key[AccessPointState_t::MaxRadiusSecretChars+1];
                size_t keyBytes = COUNTOF(key);
        
                // Merge multiple words (if necessary).
                char       *pKey    = &key[0];
                const char *pKeyEnd = &key[AccessPointState_t::MaxPassphraseChars];
                for (; kx < words.Size() && pKey < pKeyEnd ; ++kx)
                {
                    const char *pWord = words[kx];
                    if (key[0]) *(pKey++) = ' ';
                    for (; pKey < pKeyEnd ; ++pKey, ++pWord)
                        if ((*pKey = *pWord) == '\0')
                            break;
                }
               *pKey = '\0';
               
                HRESULT hr = m_pState->SetRadiusSecret(key);
                if (FAILED(hr))
                {
                    LogWarn(TEXT("[AC] Radius key '%hs' invalid"), key);
                }
                break;
            }

        break;
    }

    return NO_ERROR;
}

DWORD
Cisco1200Request_t::
UpdateRadiusInfo(
    const AccessPointState_t &NewState,
    bool                      Stalled)
{
    DWORD result;

    in_addr inaddr;
    char oldServer[96];
    char newServer[96];
    int  oldPort;
    int  newPort;
    char oldSecret[NewState.MaxRadiusSecretChars+1];
    char newSecret[NewState.MaxRadiusSecretChars+1];

    inaddr.s_addr = m_pState->GetRadiusServer();
    StringCchPrintfA(oldServer, COUNTOF(oldServer), "%u.%u.%u.%u",
                     inaddr.s_net, inaddr.s_host,
                     inaddr.s_lh, inaddr.s_impno);

    inaddr.s_addr = NewState.GetRadiusServer();
    StringCchPrintfA(newServer, COUNTOF(newServer), "%u.%u.%u.%u",
                     inaddr.s_net, inaddr.s_host,
                     inaddr.s_lh, inaddr.s_impno);

    oldPort = m_pState->GetRadiusPort();
    newPort = NewState.GetRadiusPort();
    
    m_pState->GetRadiusSecret(oldSecret, COUNTOF(oldSecret));
    NewState.GetRadiusSecret( newSecret, COUNTOF(newSecret));

    if (Stalled)
    {
        LogError(TEXT("Unable to update Radius to \"%hs,%d,%hs\" on %hs"),
                 newServer, newPort, newSecret,
                 m_APName);
        return ERROR_INVALID_PARAMETER;
    }

    // Remove the old radius server assignments.
    result = VerifyCommand(CiscoPort_t::CommandModeRadius,
                           "removing server from radius group",
                           "no server %s auth-port %d",
                            oldServer, oldPort);

    if (NO_ERROR == result)
        result = VerifyCommand(CiscoPort_t::CommandModeConfig,
                               "removing radius server",
                               "no radius-server host %s auth-port %d",
                                oldServer, oldPort);
    if (NO_ERROR != result)
    {
        LogError(TEXT("Error removing old radius-server \"%hs\" from %hs: %s"),
                 oldServer, m_APName,
                 Win32ErrorText(result));
        return result;
    }

    // Configure radius login.
    result = SendCommand(CiscoPort_t::CommandModeConfig,
                         "aaa authentication login eap_methods group rad_eap" "\r\n"
                         "line 0"                           "\r\n"
                         "login authentication eap_methods" "\r\n"
                         "exit");
    if (NO_ERROR == result)
        result = SendCommand(CiscoPort_t::CommandModeConfig,
                             "radius-server attribute 32"
                             " include-in-access-req format %%h");
    if (NO_ERROR != result)
    {
        LogError(TEXT("Error configuring radius login on %hs: %s"),
                 m_APName,
                 Win32ErrorText(result));
        return result;
    }

    // If there's no new server, we're done.
    if (NewState.GetRadiusServer() == 0)
        return NO_ERROR;

    // Add the new radius server assignments.
    result = VerifyCommand(CiscoPort_t::CommandModeRadius,
                           "adding server to radius group",
                           "server %s auth-port %d",
                            newServer, newPort);
    
    if (NO_ERROR == result)
        result = VerifyCommand(CiscoPort_t::CommandModeConfig,
                               "adding radius server",
                               "radius-server host %s auth-port %d %s %s",
                                newServer, newPort,
                               (newSecret[0] == '\0'? "" : "key"),
                               (newSecret[0] == '\0'? "" : newSecret));

    if (NO_ERROR == result)
    {
        LogDebug(TEXT("[AC]   set radius server to \"%hs\""), newServer);
        LogDebug(TEXT("[AC]   set radius port   to  %d"),     newPort);
        LogDebug(TEXT("[AC]   set radius secret to \"%hs\""), newSecret);
    }
    else
    {
        LogError(TEXT("Error adding new radius-server \"%hs,%d,%hs\" to %hs: %s"),
                 newServer, newPort, newSecret,
                 m_APName,
                 Win32ErrorText(result));
        return result;
    }

    return NO_ERROR;
}

// ----------------------------------------------------------------------------
//
// Loads or updates the security-mode settings.
//
DWORD
Cisco1200Request_t::
LoadSecurityModes(void)
{
    bool   openAuth = false;
    bool sharedAuth = false;
    bool    eapAuth = false;
    bool  wepCipher = false;
    bool tkipCipher = false;
    bool  aesCipher = false;
    bool wpaKeyMgmt = false;
    bool wpaPsk     = false;

    // Look up the interface and SSID in the running-config list.
    TelnetLines_t *pConfig = m_pCLI->GetRunningConfig();
    assert(NULL != pConfig);
    
    int currentIntf = -1; bool inSsid = false;
    for (int lx = 0 ; pConfig && lx < pConfig->Size() ; ++lx)
    {
        const TelnetWords_t &words = pConfig->GetLine(lx);
        if (words.Size() >= 2
         && _stricmp(words[0], "interface") == 0)
        {
            if (_strnicmp(words[1], s_RadioIntfHeader, s_RadioIntfHeaderChars) != 0)
                 currentIntf = -1;
            else currentIntf = words[1][s_RadioIntfHeaderChars] - '0';
        }
        else
        if (currentIntf == m_pCLI->GetRadioInterface())
        {
            // interface Dot11Radio0
            //     encryption mode wep mandatory
            //     encryption mode ciphers tkip
            //     encryption mode ciphers aes-ccm
            //
            if (words.Size() > 3
             && _stricmp(words[0], "encryption") == 0
             && _stricmp(words[1], "mode")       == 0)
            {
                if (_stricmp(words[2], "wep")       == 0
                 && _stricmp(words[3], "mandatory") == 0)
                {
                    wepCipher = true;
                }
                else
                if (_stricmp(words[2], "ciphers") == 0)
                {
                    if (_stricmp(words[3], "tkip") == 0)
                        tkipCipher = true;
                    else
                    if (_stricmp(words[3], "aes-ccm") == 0)
                        aesCipher = true;
                }
            }
        }
        else
        if (inSsid)
        {
            // dot11 ssid mySsid
            //     authentication open
            //     authentication open eap eap_methods
            //     authentication shared
            //     authentication network-eap eap_methods
            //     authentication key-management wpa
            //     wpa-psk [hex|ascii] 0 0123456789
            // !
            //
            if (words[0][0] == '!')
                inSsid = false;
            else
            if (words.Size() > 1)
            {
                if (_stricmp(words[0], "wpa-psk") == 0)
                    wpaPsk = true;
                else
                if (_stricmp(words[0], "authentication") == 0)
                {
                    if (_stricmp(words[1], "open") == 0)
                    {
                        if (words.Size() > 2
                         && _stricmp(words[2], "eap") == 0)
                              eapAuth = true;
                        else openAuth = true;
                    }
                    else
                    if (_stricmp(words[1], "shared") == 0)
                        sharedAuth = true;
                    else
                    if (_stricmp(words[1], "network-eap") == 0)
                        eapAuth = true;
                    else
                    if (_stricmp(words[1], "key-management") == 0)
                    {
                        if (words.Size() > 2
                         && _stricmp(words[2], "wpa") == 0)
                            wpaKeyMgmt = true;
                    }
                }
            }
        }
        else
        if (words.Size() > 2
         && _stricmp(words[0], "dot11") == 0
         && _stricmp(words[1], "ssid")  == 0
         && _stricmp(words[2], m_pCLI->GetSsid()) == 0)
        {
            currentIntf = -1;
            inSsid = true;
        }
    }

    // Translate the observed authentication and cipher configuration
    // into security modes.
    if (wpaKeyMgmt || aesCipher || tkipCipher)
    {
        if (aesCipher)
        {
            m_pState->SetCipherMode(APCipherAES);
            if (wpaPsk)
                 m_pState->SetAuthMode(APAuthWPA2_PSK);
            else m_pState->SetAuthMode(APAuthWPA2);
        }
        else
        if (tkipCipher)
        {
            m_pState->SetCipherMode(APCipherTKIP);
            if (wpaPsk)
                 m_pState->SetAuthMode(APAuthWPA_PSK);
            else m_pState->SetAuthMode(APAuthWPA);
        }
        else
        {
            LogWarn(TEXT("[AC] Missing WPA cipher-mode for %hs"), m_APName);
            m_pState->SetAuthMode  (APAuthWPA);
            m_pState->SetCipherMode(APCipherClearText);
        }
    }
    else
    if (eapAuth)
    {
        m_pState->SetAuthMode  (APAuthWEP_802_1X);
        m_pState->SetCipherMode(APCipherWEP);
    }
    else
    {
        if (sharedAuth)
             m_pState->SetAuthMode(APAuthShared);
        else m_pState->SetAuthMode(APAuthOpen);
        if (wepCipher)
             m_pState->SetCipherMode(APCipherWEP);
        else m_pState->SetCipherMode(APCipherClearText);
    }

    return NO_ERROR;
}

DWORD
Cisco1200Request_t::
UpdateSecurityModes(
    const AccessPointState_t &NewState,
    bool                      Stalled)
{
    DWORD result = NO_ERROR;
    if (Stalled || !ValidateSecurityModes(NewState.GetAuthMode(),
                                          NewState.GetCipherMode(),
                                          m_pState->GetCapabilitiesEnabled()))
    {
        result = ERROR_INVALID_STATE;
    }

    // Remove the old encryption and authentication settings.
    if (NO_ERROR == result)
    {
        LogDebug(TEXT("[AC]   removing key-management"));
        result = SendCommand(CiscoPort_t::CommandModeSsid,
                             "no authentication key-management");        
    }
    if (NO_ERROR == result)
    {
        LogDebug(TEXT("[AC]   removing WEP keys(s)"));
        result = SendCommand(CiscoPort_t::CommandModeInterface,
                             "no encryption key 1" "\r\n"
                             "no encryption key 2" "\r\n"
                             "no encryption key 3" "\r\n"
                             "no encryption key 4");
    }
    if (NO_ERROR == result)
    {
        LogDebug(TEXT("[AC]   removing encryption mode"));
        result = SendCommand(CiscoPort_t::CommandModeInterface,
                             "no encryption mode");
    }
    if (NO_ERROR == result)
    {
        LogDebug(TEXT("[AC]   removing WPA passphrase"));
        result = SendCommand(CiscoPort_t::CommandModeSsid,
                             "no wpa-psk");
    }
    if (NO_ERROR == result)
    {
        LogDebug(TEXT("[AC]   removing authentication mode(s)"));
        result = SendCommand(CiscoPort_t::CommandModeSsid,
                             "no authentication open"   "\r\n"
                             "no authentication shared" "\r\n"
                             "no authentication network-eap");
    }
    
    // Turn on the appropriate cipher (if any).
    if (NO_ERROR == result && NewState.GetCipherMode() != APCipherClearText)
    {
        const char *pCiphers;
        const char *pEncrType;
        switch (NewState.GetCipherMode())
        {
        default:           pCiphers = "wep";     pEncrType = "mandatory"; break;
        case APCipherTKIP: pCiphers = "ciphers"; pEncrType = "tkip";      break;
        case APCipherAES:  pCiphers = "ciphers"; pEncrType = "aes-ccm";   break;
        }
        
        LogDebug(TEXT("[AC]   adding \"%hs %hs\" encryption"), pCiphers, pEncrType);
        result = VerifyCommand(CiscoPort_t::CommandModeInterface,
                               "adding encryption mode",
                               "encryption mode %s %s",
                                pCiphers, pEncrType);
    }

    // Turn on the appropriate authentication.
    // Make sure the corresponding keys and so on get updated, too.
    bool needWpaKeyMgmt = false;
    switch (NewState.GetAuthMode())
    {
    case APAuthOpen:
    case APAuthShared:
        
        if (NewState.GetCipherMode() != APCipherClearText
         && (m_UpdatesRemaining & AccessPointState_t::FieldMaskWEPKeys) == 0)
        {
            m_UpdatesRequested |= AccessPointState_t::FieldMaskWEPKeys;
            m_UpdatesRemaining |= AccessPointState_t::FieldMaskWEPKeys;
        }
        
        if (NO_ERROR == result)
        {
            const char *pAuthName;
            if (NewState.GetAuthMode() == APAuthOpen)
                 pAuthName = "open";
            else pAuthName = "shared";
            
            LogDebug(TEXT("[AC]   adding \"%hs\" authentication"), pAuthName);
            result = VerifyCommand(CiscoPort_t::CommandModeSsid,
                                   "adding open/shared authentication",
                                   "authentication %s",
                                    pAuthName);
        }
        break;

    case APAuthWEP_802_1X:
    case APAuthWPA:
    case APAuthWPA2:
        needWpaKeyMgmt = NewState.GetCipherMode() != APCipherWEP;

        if ((m_UpdatesRemaining & AccessPointState_t::FieldMaskRadius) == 0)
        {
            m_UpdatesRequested |= AccessPointState_t::FieldMaskRadius;
            m_UpdatesRemaining |= AccessPointState_t::FieldMaskRadius;
        }

        if (NO_ERROR == result)
        {
            LogDebug(TEXT("[AC]   adding \"open eap\" authentication"));
            result = VerifyCommand(CiscoPort_t::CommandModeSsid,
                                   "adding open EAP authenticaion",
                                   "authentication open eap eap_methods");
        }

        if (NO_ERROR == result)
        {
            LogDebug(TEXT("[AC]   adding \"network-eap\" authentication"));
            result = VerifyCommand(CiscoPort_t::CommandModeSsid,
                                   "adding network EAP authenticaion",
                                   "authentication network-eap eap_methods");
        }
        break;

    case APAuthWPA_PSK:
    case APAuthWPA2_PSK:
        needWpaKeyMgmt = true;

        if ((m_UpdatesRemaining & AccessPointState_t::FieldMaskPassphrase) == 0)
        {
            m_UpdatesRequested |= AccessPointState_t::FieldMaskPassphrase;
            m_UpdatesRemaining |= AccessPointState_t::FieldMaskPassphrase;
        }

        if (NO_ERROR == result)
        {
            LogDebug(TEXT("[AC]   adding \"open\" authentication"));
            result = VerifyCommand(CiscoPort_t::CommandModeSsid,
                                   "adding open authenticaion",
                                   "authentication open");
        }
        break;

    }

    // If necessary, turn on WPA key-management.
    if (NO_ERROR == result && needWpaKeyMgmt)
    {
        LogDebug(TEXT("[AC]   adding WPA key-management"));
        result = VerifyCommand(CiscoPort_t::CommandModeSsid,
                               "adding WPA key-management",
                               "authentication key-management wpa");
    }
    
    if (NO_ERROR == result)
    {
        LogDebug(TEXT("[AC]   set authentication mode to %s"), NewState.GetAuthName());
        LogDebug(TEXT("[AC]   set encryption mode     to %s"), NewState.GetCipherName());
    }
    else
    {
        LogError(TEXT("Unable to update security modes to")
                 TEXT(" auth=%s and cipher=%s on %hs: %s"),
                 NewState.GetAuthName(),
                 NewState.GetCipherName(),
                 m_APName,
                 Win32ErrorText(result));
        return result;
    }

    return NO_ERROR;
}

// ----------------------------------------------------------------------------
//
// Loads or updates the SSID.
//
DWORD
Cisco1200Request_t::
LoadSsid(void)
{
    // Look up the interface in the global SSIDs list.
    for (int gx = m_pCLI->GetNumberGlobalSsids() ; --gx >= 0 ;)
    {
        const CiscoSsid_t *pGlobalSsiid = m_pCLI->GetGlobalSsid(gx);
        if (pGlobalSsiid
         && pGlobalSsiid->IsAssociated(m_pCLI->GetRadioInterface()))
        {
            m_pState->SetSsid(pGlobalSsiid->GetSsid());
            m_pCLI->SetSsid(pGlobalSsiid->GetSsid());
            m_bSsidAssociated = true;
            return NO_ERROR;
        }
    }

    // Not associated with any SSIDs, just warn about it and make sure
    // to add a new SSID next time we update.
    LogWarn(TEXT("%hs not associated to any global SSIDs"), m_APName);
    m_bSsidAssociated = false;
    
    return NO_ERROR;
}

DWORD
Cisco1200Request_t::
UpdateSsid(
    const AccessPointState_t &NewState,
    bool                      Stalled)
{
    HRESULT hr;
    DWORD result;
    CiscoSsid_t *pGlobalSsid;

    if (Stalled)
    {
        LogError(TEXT("Unable to update SSID to \"%ls\" on %hs"),
                 NewState.GetSsid(),
                 m_APName);
        return ERROR_INVALID_STATE;
    }

    char oldSsid[CiscoPort_t::MaxSsidChars+1];
    hr = WiFUtils::ConvertString(oldSsid, m_pState->GetSsid(),
                         COUNTOF(oldSsid), "SSID");
    if (FAILED(hr))
        return HRESULT_CODE(hr);

    char newSsid[CiscoPort_t::MaxSsidChars+1];
    hr = WiFUtils::ConvertString(newSsid, NewState.GetSsid(),
                         COUNTOF(newSsid), "SSID");
    if (FAILED(hr))
        return HRESULT_CODE(hr);

    // Make sure there are no invalid characters in the SSID.
    int valid = 0, invalid = 0, leading = 0, trailing = 0;
    for (const char *pSsid = newSsid ; *pSsid ; ++pSsid)
        switch (*pSsid)
        {
        case '+': case ']': case '$': case '\t':
            invalid++;
            break;
        case ' ':
            if (valid) trailing++;
            else       leading++;
            break;
        default:
            valid++;
            valid += trailing;
            trailing = 0;
            break;
        }
    if (!valid || invalid || leading || trailing)
    {
        LogError(TEXT("Invalid SSID \"%hs\": leading and trailing spaces,")
                 TEXT(" '+', ']', '$' and TAB are not allowed"),
                 newSsid);
        return ERROR_INVALID_PARAMETER;
    }
    
    // Make sure the security modes, radio-state, etc. gets replaced.
    // These modes are all, in some way registered to the global SSID,
    // so when the SSID goes, they go too.
    if ((m_UpdatesRemaining & AccessPointState_t::FieldMaskSecurityMode) == 0)
    {
        m_UpdatesRequested |= AccessPointState_t::FieldMaskSecurityMode;
        m_UpdatesRemaining |= AccessPointState_t::FieldMaskSecurityMode;
    }
    if ((m_UpdatesRemaining & AccessPointState_t::FieldMaskRadioState) == 0)
    {
        m_UpdatesRequested |= AccessPointState_t::FieldMaskRadioState;
        m_UpdatesRemaining |= AccessPointState_t::FieldMaskRadioState;
    }
    if ((m_UpdatesRemaining & AccessPointState_t::FieldMaskSsidBroadcast) == 0)
    {
        m_UpdatesRequested |= AccessPointState_t::FieldMaskSsidBroadcast;
        m_UpdatesRemaining |= AccessPointState_t::FieldMaskSsidBroadcast;
    }
    if ((m_UpdatesRemaining & AccessPointState_t::FieldMaskCapabilities) == 0)
    {
        m_UpdatesRequested |= AccessPointState_t::FieldMaskCapabilities;
        m_UpdatesRemaining |= AccessPointState_t::FieldMaskCapabilities;
    }

    // Create the global SSID.
    m_pCLI->SetSsid(newSsid);
    result = m_pCLI->SetCommandMode(CiscoPort_t::CommandModeSsid);
    if (NO_ERROR == result)
        result = m_pCLI->AddGlobalSsid(newSsid);
    if (NO_ERROR != result)
    {
        LogError(TEXT("Error adding global SSID \"%hs\" to %hs: %s"),
                 newSsid, m_APName,
                 Win32ErrorText(result));
        return result;
    }

    // If we're replacing an existing SSID...
    if (oldSsid[0] && m_bSsidAssociated)
    {
        LogDebug(TEXT("[AC]   disassociating from old SSID \"%hs\""), oldSsid);
        
        // Disassociate the old SSID from this interface.
        int remainingAssociations = 0;
        result = VerifyCommand(CiscoPort_t::CommandModeInterface,
                               "disassociating SSID from interface",
                               "no ssid %s", oldSsid);

        if (NO_ERROR != result)
        {
            LogWarn(TEXT("Error disassociating SSID \"%hs\" from %hs: %s"),
                    oldSsid, m_APName,
                    Win32ErrorText(result));
        }

        // Delete the SSID if it doesn't have any remaining associations.
        if (remainingAssociations == 0)
        {
            LogDebug(TEXT("[AC]   deleting unused global SSID \"%hs\""), oldSsid);
        
            result = VerifyCommand(CiscoPort_t::CommandModeConfig,
                                   "deleting global SSID",
                                   "no dot11 ssid %s", oldSsid);
            if (NO_ERROR == result)
                result = m_pCLI->DeleteGlobalSsid(oldSsid);
            if (NO_ERROR != result)
            {
                LogWarn(TEXT("Error deleting global SSID \"%hs\" from %hs: %s"),
                        oldSsid, m_APName,
                        Win32ErrorText(result));
            }
        }
    }

    // Associate the SSID with this interface.
    LogDebug(TEXT("[AC]   associating with new SSID \"%hs\""), newSsid);
    
    result = VerifyCommand(CiscoPort_t::CommandModeInterface,
                           "associating SSID to interface",
                           "ssid %s", newSsid);
    if (NO_ERROR == result)
    {
        pGlobalSsid = m_pCLI->GetGlobalSsid(newSsid);
        if (pGlobalSsid)
            result = pGlobalSsid->AssociateRadioInterface(m_pCLI->GetRadioInterface());
    }
    
    if (NO_ERROR == result)
        LogDebug(TEXT("[AC]   set SSID to \"%hs\""), newSsid);
    else
    {
        LogError(TEXT("Error associating SSID \"%hs\" with %hs: %s"),
                 newSsid, m_APName,
                 Win32ErrorText(result));
        return result;
    }

    m_pCLI->SetSsid(newSsid);
    return NO_ERROR;
}

// ----------------------------------------------------------------------------
//
// Loads or updates the SSID-broadcast flag.
//
DWORD
Cisco1200Request_t::
LoadSsidBroadcast(void)
{
    // Look for the appropriate global SSID in the running configuration.
    TelnetLines_t *pConfig = m_pCLI->GetRunningConfig();
    assert(NULL != pConfig);
    
    m_pState->SetSsidBroadcast(false);
    for (int lx = 0 ; pConfig && lx < pConfig->Size() ; ++lx)
    {
        const TelnetWords_t &words = pConfig->GetLine(lx);
        if (words.Size() > 2
         && strcmp(words[2], m_pCLI->GetSsid()) == 0
         && _stricmp(words[1], "ssid") == 0
         && _stricmp(words[0], "dot11") == 0)
        {
            // Dot11 ssid apname
            //     authentication open
            //     guest-mode         <==
            // !
            for (; lx < pConfig->Size() ; ++lx)
            {
                const TelnetWords_t &nextWords = pConfig->GetLine(lx);
                if (nextWords.Size() > 0)
                {
                    if (nextWords[0][0] == '!')
                        break;
                    else
                    if (strcmp(nextWords[0], "guest-mode") == 0)
                    {
                        m_pState->SetSsidBroadcast(true);
                        break;
                    }
                }
            }
            break;
        }
    }

    return NO_ERROR;
}

DWORD
Cisco1200Request_t::
UpdateSsidBroadcast(
    const AccessPointState_t &NewState,
    bool                      Stalled)
{
    DWORD result = NO_ERROR;
    if (Stalled)
        result = ERROR_INVALID_STATE;

    if (NO_ERROR == result)
        result = VerifyCommand(CiscoPort_t::CommandModeSsid,
                               "setting SSID guest-mode",
                               "%sguest-mode",
                                NewState.IsSsidBroadcast()? "" : "no ");

    if (NO_ERROR != result)
    {
        LogError(TEXT("Unable to %hs broadcast-SSID on %hs"),
                 NewState.IsSsidBroadcast()? "enable" : "disable",
                 m_APName,
                 Win32ErrorText(result));
        return result;
    }

    return NO_ERROR;
}

// ----------------------------------------------------------------------------
//
// Loads or updates the WEP keys.
//
DWORD
Cisco1200Request_t::
LoadWepKeys(void)
{
    DWORD result;
    char lineBuff[120];

    // If the security modes don't use WEP keys, just return success.
    // This won't represent what's in the AP, but it won't matter until
    // they modify the security modes to something that use WEP keys.
    switch (m_pState->GetAuthMode())
    {
    case APAuthOpen:
        if (m_pState->GetCipherMode() == APCipherClearText)
            return NO_ERROR;
        break;
    case APAuthShared:
        break;
    default:
        return NO_ERROR;
    }

    // Look up the interface in the running-config list.
    TelnetLines_t *pConfig = m_pCLI->GetRunningConfig();
    assert(NULL != pConfig);
    
    WEPKeys_t newKeys;
    int currentIntf = -1;
    for (int lx = 0 ; pConfig && lx < pConfig->Size() ; ++lx)
    {
        const TelnetWords_t &words = pConfig->GetLine(lx);
        if (words.Size() >= 2
         && _stricmp(words[0], "interface") == 0)
        {
            if (_strnicmp(words[1], s_RadioIntfHeader, s_RadioIntfHeaderChars) != 0)
                 currentIntf = -1;
            else currentIntf = words[1][s_RadioIntfHeaderChars] - '0';
        }
        else
        if (currentIntf == m_pCLI->GetRadioInterface())
        {
            // interface Dot11Radio0
            //     encryption key 1 size 40bit 0 0123456789 transmit-key"
            //
            if (words.Size() < 7
             || _stricmp(words[0], "encryption") != 0
             || _stricmp(words[1], "key")        != 0
             || _stricmp(words[3], "size")       != 0)
                continue;

            // Get the key-index.
            int keyIndex = words[2][0] - '1';
            if (0 > keyIndex || keyIndex >= newKeys.KeyCount)
            {
                LogError(TEXT("Invalid key-index in WEP-key line \"%hs\"")
                         TEXT(" from %hs"),
                         words.MergeLine(lineBuff, COUNTOF(lineBuff)),
                         m_APName);
                return ERROR_INVALID_PARAMETER;
            }

            // Get the key-size.
            size_t keySize;
            if (_strnicmp(words[4], "40", 2) == 0) keySize = 5; // <= in bytes
            else
            if (_strnicmp(words[4], "128",3) == 0) keySize = 13;
            else
            {
                LogError(TEXT("Invalid key-size in WEP-key line \"%hs\"")
                         TEXT(" from %hs"),
                         words.MergeLine(lineBuff, COUNTOF(lineBuff)),
                         m_APName);
                return ERROR_INVALID_PARAMETER;
            }
            newKeys.m_Size[keyIndex] = static_cast<BYTE>(keySize);

            // Get the key itself.
            size_t hexSize = COUNTOF(lineBuff);
            result = DecodeHexKey(words[6], "WEP key", lineBuff, &hexSize);
            if (NO_ERROR != result || hexSize != keySize)
            {
                LogError(TEXT("Invalid key-material in WEP-key line \"%hs\"")
                         TEXT(" from %hs"),
                         words.MergeLine(lineBuff, COUNTOF(lineBuff)),
                         m_APName);
                return ERROR_INVALID_PARAMETER;
            }
            memcpy(newKeys.m_Material[keyIndex], lineBuff, keySize);

            // Get the optional "current key-index" indicator.
            if (words.Size() > 7 && _stricmp(words[7], "transmit-key") == 0)
            {
                newKeys.m_KeyIndex = static_cast<BYTE>(keyIndex);
            }
        }
    }

    m_pState->SetWEPKeys(newKeys);
    return NO_ERROR;
}

DWORD
Cisco1200Request_t::
UpdateWepKeys(
    const AccessPointState_t &NewState,
    bool                      Stalled)
{
    DWORD result = NO_ERROR;
    if (Stalled)
        result = ERROR_INVALID_STATE;

    const WEPKeys_t &oldKeys = m_pState->GetWEPKeys();
    const WEPKeys_t &newKeys = NewState.GetWEPKeys();

    // If the security modes don't use WEP keys, just transfer the keys
    // to the current state. This won't represent what's in the AP, but
    // it won't matter until they modify the security modes to something
    // that uses WEP keys. At that point we can update the AP properly.
    switch (NewState.GetAuthMode())
    {
    case APAuthOpen:
        if (NewState.GetCipherMode() == APCipherClearText)
        {
            m_pState->SetWEPKeys(newKeys);
            return NO_ERROR;
        }
        break;
    case APAuthShared:
        break;
    default:
        m_pState->SetWEPKeys(newKeys);
        return NO_ERROR;
    }

    int keyIndex = static_cast<int>(newKeys.m_KeyIndex);
    int kx = keyIndex;
    for (kx = 0 ; kx < newKeys.KeyCount && NO_ERROR == result ; ++kx)
    {
        // Ignore empty and invalid keys.
        int keyLength = static_cast<int>(newKeys.m_Size[kx]);
        if (!newKeys.ValidKeySize(keyLength))
            continue;

        // Encode the key into hex.
        char   hexKey[sizeof(newKeys.m_Material[kx]) * 3];
        size_t hexBytes = COUNTOF(hexKey);
        result = EncodeHexKey((const char *)(newKeys.m_Material[kx]),
                             "WEP key",
                              hexKey, &hexBytes,
                              keyLength);
        if (NO_ERROR != result)
            break;

        // Make sure the length is one Cisco understands.
        int keyBits = keyLength * 8;
        if (keyBits == 104)
            keyBits = 128;
        else
        if (keyBits != 40)
        {
            LogError(TEXT("WEP key #%d \"%hs\" wrong size for")
                     TEXT(" %hs: only 40 and 104 bits allowed"),
                     kx, hexKey,
                     m_APName);
            result = ERROR_INVALID_PARAMETER;
            break;
        }

        // Insert the key.
        result = VerifyCommand(CiscoPort_t::CommandModeInterface,
                              "inserting WEP key",
                              "encryption key %d size %d %hs %hs",
                               kx+1, keyBits, hexKey,
                              (kx == keyIndex)? "transmit-key" : "");
        if (NO_ERROR != result)
            break;

        LogDebug(TEXT("[AC]   set WEP key #%d to \"%hs\" (%d bits) %hs"),
                 kx+1, hexKey, keyBits,
                (kx == keyIndex)? "(transmit-key)" : "");
    }

    if (NO_ERROR != result)
    {
        LogError(TEXT("Unable to add WEP key #%d to %hs"),
                 kx+1,
                 m_APName);
        return result;
    }

    return NO_ERROR;
}

// ----------------------------------------------------------------------------
//
// Loads or updates the radio status.
//
DWORD
Cisco1200Request_t::
LoadWmmCapability(void)
{
    bool dot11PhoneEnabled = false;
    bool  interfaceEnabled = true; // enabled by default

    // Look up the interface in the running-config list.
    TelnetLines_t *pConfig = m_pCLI->GetRunningConfig();
    assert(NULL != pConfig);
    
    int currentIntf = -1;
    for (int lx = 0 ; pConfig && lx < pConfig->Size() ; ++lx)
    {
        const TelnetWords_t &words = pConfig->GetLine(lx);
        if (words.Size() >= 2
         && _stricmp(words[0], "interface") == 0)
        {
            if (_strnicmp(words[1], s_RadioIntfHeader, s_RadioIntfHeaderChars) != 0)
                 currentIntf = -1;
            else currentIntf = words[1][s_RadioIntfHeaderChars] - '0';
        }
        else
        if (currentIntf == m_pCLI->GetRadioInterface())
        {
            // interface Dot11Radio0
            //     no dot11 qos mode <==
            //
            if (words.Size() > 3
             && _stricmp(words[0], "no")    == 0
             && _stricmp(words[1], "dot11") == 0
             && _stricmp(words[2], "qos")   == 0
             && _stricmp(words[3], "mode")  == 0)
            {
                interfaceEnabled = false;
            }
        }
        else
        if (words.Size() > 1
         && _stricmp(words[0], "dot11") == 0
         && _stricmp(words[1], "phone") == 0)
        {
            dot11PhoneEnabled = true;
        }
    }

    if (dot11PhoneEnabled && interfaceEnabled)
         m_pState->EnableCapability(APCapsWMM);
    else m_pState->DisableCapability(APCapsWMM);

    return NO_ERROR;
}

DWORD
Cisco1200Request_t::
UpdateWmmCapability(
    const AccessPointState_t &NewState,
    bool                      Stalled)
{
    DWORD result = NO_ERROR;
    if (Stalled)
        result = ERROR_INVALID_STATE;

    const char *pOperation;
    const char *pCommand;
    const char *pWhich;
    if (NewState.IsCapabilityEnabled(APCapsWMM))
    {
        pOperation = "enabling interface WMM mode";
        pCommand   = "dot11 qos mode wmm";
        pWhich     = "enabl";
    }
    else
    {
        pOperation = "disabling interface WMM mode";
        pCommand   = "no dot11 qos mode wmm";
        pWhich     = "disabl";
    }

    // Always enable the global capability.
    if (NO_ERROR == result)
    {
        LogDebug(TEXT("[AC]   adding global QoS mode \"dot11 phone\""));
        result = VerifyCommand(CiscoPort_t::CommandModeConfig,
                              "enabing global QoS mode",
                              "dot11 phone");
    }
    
    // Enable/disble the interface.
    if (NO_ERROR == result)
    {
        LogDebug(TEXT("[AC]   %hsing interface QoS mode WMM"), pWhich);
        result = VerifyCommand(CiscoPort_t::CommandModeInterface,
                               pOperation, pCommand);
    }
    
    if (NO_ERROR == result)
        LogDebug(TEXT("[AC]   %hsed WMM capability"), pWhich);
    else
    {
        LogError(TEXT("Unable to %hse WMM on %hs"),
                 pWhich,
                 m_APName);
        return result;
    }

    return NO_ERROR;
}

// ----------------------------------------------------------------------------
//
// Sends the specified command and stores the response in m_Response:
// If necessary, changes to the specified command-mode before sending
// the command.
//
DWORD
Cisco1200Request_t::
SendCommand(
    CiscoPort_t::CommandMode_e CommandMode,
    const char                *pFormat, ...)
{
    va_list pArgList;
    va_start(pArgList, pFormat);
    DWORD result = m_pCtlr->SendCommandV(CommandMode,
                                         pFormat,
                                         pArgList);
    va_end(pArgList);
    return result;
}

// ----------------------------------------------------------------------------
//
// Sends the specified command, stores the response in m_Response and
// and checks it for an error message:
// (Any response is assumed to be an error message.)
// If necessary, changes to the specified command-mode before sending
// the command.
// If appropriate, issues an error message containing the specified
// operation name and the error message sent by the server.
//
DWORD
Cisco1200Request_t::
VerifyCommand(
    CiscoPort_t::CommandMode_e CommandMode,
    const char                *pOperation,
    const char                *pFormat, ...)
{
    va_list pArgList;
    va_start(pArgList, pFormat);
    DWORD result = m_pCtlr->VerifyCommandV(CommandMode,
                                           pOperation,
                                           pFormat,
                                           pArgList);
    va_end(pArgList);
    return result;
}


/* ========================================================================= */
/* ========================== Cisco1200Controller ========================== */
/* ========================================================================= */

// ----------------------------------------------------------------------------
//
// Constructor.
//
Cisco1200Controller_t::
Cisco1200Controller_t(
    CiscoPort_t         *pCLI,        // Telnet/CLI interface
    int                  Intf,        // Radio intf or -1 if all intfs
    const CiscoAPInfo_t &APInfo,      // Basic configuration information
    AccessPointState_t  *pState)      // Initial device configuration
  : CiscoAPTypeController_t(pCLI, Intf, APInfo, pState)
{
    // nothing to do
}

// ----------------------------------------------------------------------------
//
// Destructor.
//
Cisco1200Controller_t::
~Cisco1200Controller_t(void)
{
    // nothing to do
}
    
// ----------------------------------------------------------------------------
//
// Loads information about the specified radio interface.
//
static DWORD
GetRadioInterface(
    CiscoPort_t         *pCLI,
    int                  Intf,
    const CiscoAPInfo_t &APInfo,
    DWORD               *pHdweType,
    MACAddr_t           *pMACAddr)
{
    DWORD result;
    
    // Read the device's radio interface info.
    result = pCLI->SendCommand(CiscoPort_t::CommandModePrivileged,
                               "show interface %s %d | include Hardware",
                                s_RadioIntfHeader, Intf);
    if (NO_ERROR != result)
    {
        LogError(TEXT("Can't read interface %d information from")
                 TEXT(" Cisco %hs at %s: %s"),
                 Intf,
                 APInfo.ModelName,
                 pCLI->GetServerHost(),
                 Win32ErrorText(result));
        return result;
    }

    // The hardware line should be around here somewhere...
    TelnetLines_t *pResponse = pCLI->GetCommandResponse();
    assert(NULL != pResponse);
    
    for (int lx = 0 ; ; ++lx)
    {
        if (!pResponse || lx >= pResponse->Size())
        {
            LogError(TEXT("Radio interface %d not supported by ")
                     TEXT(" Cisco %hs at %s"),
                     Intf,
                     APInfo.ModelName,
                     pCLI->GetServerHost());
            return ERROR_INVALID_PARAMETER;
        }

        const TelnetWords_t &words = pResponse->GetLine(lx);
        if (words.Size() < 1
         || _stricmp(words[0], "hardware") != 0)
            continue;

        // Get the hardware type.
        int typeIX;
        for (typeIX = 1 ; typeIX < words.Size() ; ++typeIX)
            if (_strnicmp(words[typeIX], s_HdweTypeHeader, s_HdweTypeHeaderChars) == 0)
                break;
        if (typeIX >= words.Size())
        {
            LogError(TEXT("Missing radio interface type \"%hsX\" in")
                     TEXT(" Cisco %hs at %s"),
                     s_HdweTypeHeader,
                     APInfo.ModelName,
                     pCLI->GetServerHost());
            return ERROR_INVALID_PARAMETER;
        }
        else
        {
            DWORD interfaceType = 0;
            switch (words[typeIX][s_HdweTypeHeaderChars])
            {
            case 'A': interfaceType = CiscoPort_t::InterfaceTypeRadioA; break;
            case 'B': interfaceType = CiscoPort_t::InterfaceTypeRadioB; break;
            case 'G': interfaceType = CiscoPort_t::InterfaceTypeRadioB
                                    | CiscoPort_t::InterfaceTypeRadioG; break;
            }
            if (interfaceType == 0)
            {
                LogError(TEXT("Unrecognized radio interface type \"%hs\" in")
                         TEXT(" Cisco %hs at %s"),
                         words[typeIX],
                         APInfo.ModelName,
                         pCLI->GetServerHost());
                return ERROR_INVALID_PARAMETER;
            }

           *pHdweType = interfaceType;
            LogDebug(TEXT("[AC] Interface %d radio type = %hs"),
                     Intf, words[typeIX]);
        }

        // Get the MAC address.
        int addrIX;
        for (addrIX = words.Size() - 3 ; addrIX > typeIX ; --addrIX)
            if (_stricmp(words[addrIX], "address") == 0)
                break;
        if (addrIX <= typeIX)
        {
            LogError(TEXT("Missing radio MAC address hdr \"address\" in")
                     TEXT(" Cisco %hs at %s"),
                     APInfo.ModelName,
                     pCLI->GetServerHost());
            return ERROR_INVALID_PARAMETER;
        }
        else
        {
            MACAddr_t macAddr;
            HRESULT hr = macAddr.FromString(words[addrIX+2]);
            if (FAILED(hr))
            {
                LogError(TEXT("Bad MAC address \"%hs\" in")
                         TEXT(" Cisco %hs at %s"),
                         words[addrIX+2],
                         APInfo.ModelName,
                         pCLI->GetServerHost());
                return ERROR_INVALID_PARAMETER;
            }

           *pMACAddr = macAddr;
            LogDebug(TEXT("[AC] Interface %d MAC address (BSSID) = %hs"),
                     Intf, words[addrIX+2]);
        }

        break;
    }

    return NO_ERROR;
}

// ----------------------------------------------------------------------------
//
// Initializes the device-controller.
//
DWORD
Cisco1200Controller_t::
InitializeDevice(void)
{
    DWORD result;

    //
    // Turn off password encryption.
    //
    result = m_pCLI->VerifyCommand(CiscoPort_t::CommandModeConfig,
                                   "disabling password-encryption",
                                   "no service password-encryption");
    if (NO_ERROR != result)
    {
        LogError(TEXT("Can't turn off password encryption on")
                 TEXT(" Cisco %hs at %s: %s"),
                 m_APInfo.ModelName,
                 m_pCLI->GetServerHost(),
                 Win32ErrorText(result));
        return result;
    }

    //
    // Read the initial running configuration.
    //
    result = m_pCLI->ReadRunningConfig();
    if (NO_ERROR != result)
        return result;
    
    TelnetLines_t *pConfig = m_pCLI->GetRunningConfig();
    assert(NULL != pConfig);
    
    //
    // Read the device's radio interface list and make sure the one
    // they've selected actually exists.
    //
    bool intfExists = false;
    for (int lx = 0 ; pConfig && lx < pConfig->Size() ; ++lx)
    {
        const TelnetWords_t &words = pConfig->GetLine(lx);
        if (words.Size() < 2
         || _stricmp(words[0], "interface") != 0
         || _strnicmp(words[1], s_RadioIntfHeader, s_RadioIntfHeaderChars) != 0)
         continue;

        int intf = words[1][s_RadioIntfHeaderChars] - '0';
        DWORD interfaceType;
        MACAddr_t macAddr;
        result = GetRadioInterface(m_pCLI, intf, m_APInfo,
                                   &interfaceType, &macAddr);
        if (NO_ERROR != result)
            return result;

        if (m_pCLI->GetRadioInterface() == intf)
        {
            m_pCLI->SetRadioInterfaceType(interfaceType);
            m_pState->SetBSsid(macAddr);
            intfExists = true;
        }

        // If we're supposed to control all the interfaces, create a
        // telnet/CLI interface for each.
        if (m_pIntfCLIs[0] && m_pCLI->GetRadioInterface() != intf)
        {
            CiscoPort_t *pIntfCLI = new CiscoPort_t(m_pCLI->GetServerHost(),
                                                    m_pCLI->GetServerPort(), intf);
            if (NULL == pIntfCLI)
            {
                LogError(TEXT("Can't allocate CLI interface for radio %d")
                         TEXT(" in Cisco %hs at %s"),
                         intf,
                         m_APInfo.ModelName,
                         m_pCLI->GetServerHost());
                return ERROR_OUTOFMEMORY;
            }

            for (int ix = 1 ; ; ++ix)
            {
                if (ix >= CiscoPort_t::NumberRadioInterfaces)
                {
                    LogWarn(TEXT("Too many radio interfaces")
                            TEXT(" in Cisco %hs at %s")
                            TEXT(": can only control %d"),
                            m_APInfo.ModelName,
                            m_pCLI->GetServerHost(),
                            CiscoPort_t::NumberRadioInterfaces);
                    delete pIntfCLI;
                    break;
                }
                else
                if (NULL == m_pIntfCLIs[ix])
                {
                    m_pIntfCLIs[ix] = pIntfCLI;
                    break;
                }
            }
        }
    }

    if (!intfExists)
    {
        LogError(TEXT("Radio interface %d not supported by ")
                 TEXT(" Cisco %hs at %s"),
                 m_pCLI->GetRadioInterface(),
                 m_APInfo.ModelName,
                 m_pCLI->GetServerHost());
        return ERROR_INVALID_PARAMETER;
    }

    //
    // Load the global SSIDs and match them up to the radio interface(s).
    //    

    // Get all the global SSIDs.
    for (int lx = 0 ; lx < pConfig->Size() ; ++lx)
    {
        const TelnetWords_t &words = pConfig->GetLine(lx);
        char ssidBuff[CiscoPort_t::MaxSsidChars+1];

        if (words.Size() >= 3
         && _stricmp(words[0], "dot11") == 0
         && ParseSsid(words, 1, ssidBuff))
        {
            LogDebug(TEXT("[AC] Adding global SSID \"%hs\""), ssidBuff);
            result = m_pCLI->AddGlobalSsid(ssidBuff);
            if (NO_ERROR != result)
                return result;
        }
    }

    // Remove all the current interface associations.
    for (int gx = m_pCLI->GetNumberGlobalSsids() ; --gx >= 0 ;)
    {
        CiscoSsid_t *pGlobalSsid = m_pCLI->GetGlobalSsid(gx);
        if (NULL != pGlobalSsid)
        {
            int numAssociations;
            for (int ix = 0 ; ix < CiscoPort_t::NumberRadioInterfaces ; ++ix)
            {
                result = pGlobalSsid->DisassociateRadioInterface(ix,
                                                                &numAssociations);
                if (NO_ERROR != result)
                    return result;
                if (0 == numAssociations)
                    break;
            }
        }
    }

    // Associate each interface to the corresponding global SSID.
    int currentIntf = -1;
    for (int lx = 0 ; lx < pConfig->Size() ; ++lx)
    {
        const TelnetWords_t &words = pConfig->GetLine(lx);
        if (words.Size() >= 2
         && _stricmp(words[0], "interface") == 0)
        {
            if (_strnicmp(words[1], s_RadioIntfHeader, s_RadioIntfHeaderChars) != 0)
                 currentIntf = -1;
            else currentIntf = words[1][s_RadioIntfHeaderChars] - '0';
        }
        else
        if (0 <= currentIntf && currentIntf < CiscoPort_t::NumberRadioInterfaces
         && words.Size() >= 2)
        {
            char ssidBuff[CiscoPort_t::MaxSsidChars+1];
            if (ParseSsid(words, 0, ssidBuff))
            {
                CiscoSsid_t *pGlobalSsid = m_pCLI->GetGlobalSsid(ssidBuff);
                if (NULL == pGlobalSsid)
                {
                    LogWarn(TEXT("Can't find interface %d SSID \"%hs\"")
                            TEXT(" in global SSIDs list for")
                            TEXT(" Cisco %hs at %s"),
                            currentIntf, ssidBuff,
                            m_APInfo.ModelName,
                            m_pCLI->GetServerHost());
                    continue;
                }

                result = pGlobalSsid->AssociateRadioInterface(currentIntf);
                if (NO_ERROR != result)
                    return result;

                LogDebug(TEXT("[AC] Global SSID \"%hs\" associated with %hs%d"),
                         ssidBuff,
                         s_RadioIntfHeader,
                         currentIntf);

                // Allow only one association per interface.
                currentIntf = -1;
            }
        }
    }

    // Remove global SSIDs with no associations.
    for (int gx = m_pCLI->GetNumberGlobalSsids() ; --gx >= 0 ;)
    {
        CiscoSsid_t *pGlobalSsid = m_pCLI->GetGlobalSsid(gx);
        if (NULL != pGlobalSsid)
        {
            CiscoSsid_t outgoingSsid = *pGlobalSsid;

            int numAssociations = 0;
            for (int ix = 0 ; ix < CiscoPort_t::NumberRadioInterfaces ; ++ix)
                if (pGlobalSsid->IsAssociated(ix))
                    numAssociations++;

            if (0 == numAssociations)
            {
                LogDebug(TEXT("[AC] Deleting unassocated global SSID \"%hs\""),
                         outgoingSsid.GetSsid());

                result = m_pCLI->DeleteGlobalSsid(outgoingSsid.GetSsid());
                if (NO_ERROR == result)
                    result = m_pCLI->VerifyCommand(CiscoPort_t::CommandModeConfig,
                                                   "deleting global SSID",
                                                   "no dot11 ssid %s",
                                                    outgoingSsid.GetSsid());
                if (NO_ERROR != result)
                {
                    LogError(TEXT("Error deleting global SSID \"%hs\" from")
                             TEXT(" Cisco %hs at %s: %s"),
                             outgoingSsid.GetSsid(),
                             m_APInfo.ModelName,
                             m_pCLI->GetServerHost(),
                             Win32ErrorText(result));
                    return result;
                }
            }
        }
    }

    //
    // Fill in the model information.
    //
    m_pState->SetVendorName("Cisco");
    m_pState->SetModelNumber(m_APInfo.ModelName);

    // Fill in the model's capabilities.
    int capsStatic = int(APCapsWEP_802_1X
                       | APCapsWPA  | APCapsWPA_PSK
                       | APCapsWPA2 | APCapsWPA2_PSK);
    int capsDynamic = int(APCapsWMM);

    m_pState->SetCapabilitiesImplemented(capsStatic | capsDynamic);
    m_pState->SetCapabilitiesEnabled(capsStatic);

    return NO_ERROR;
}

#ifdef TEST_COMMAND_MODES
// ----------------------------------------------------------------------------
//
// Cycles through all the command-modes combinations.
//
static const char *
GetModeName(
    CiscoPort_t::CommandMode_e Mode)
{
    const char *pName;
    switch (Mode)
    {
    case CiscoPort_t::CommandModeUserExec:   pName = "UserExec";    break;
    case CiscoPort_t::CommandModePrivileged: pName = "Privileged";  break;
    case CiscoPort_t::CommandModeConfig:     pName = "Global";      break;
    case CiscoPort_t::CommandModeInterface:  pName = "Interface";   break;
    case CiscoPort_t::CommandModeRadius:     pName = "Radius";      break;
    case CiscoPort_t::CommandModeSsid:       pName = "SSID";        break;
    default:                                 pName = "!!unknown!!"; break;
    }
    return pName;
}

static DWORD
ChangeMode(
    CiscoPort_t *pCLI,
    CiscoPort_t::CommandMode_e OldMode,
    const char               *pOldSsid,
    int                        NewMode)
{
    const char *pNewSsid, *pNewSsidLog, *pOldSsidLog = pOldSsid;

    if ((int)OldMode < (int)CiscoPort_t::CommandModeSsid)
        pOldSsidLog = "-";

    if (NewMode < (int)CiscoPort_t::CommandModeSsid)
        pNewSsid = "SSID1", pNewSsidLog = "-";
    else
    if (NewMode == (int)CiscoPort_t::CommandModeSsid)
        pNewSsid = pNewSsidLog = "SSID1";
    else
        pNewSsid = pNewSsidLog = "SSID2",
        NewMode = (int)CiscoPort_t::CommandModeSsid;

    LogDebug(TEXT("\n"));
    LogDebug(TEXT("================ %hs (%hs) ==> %hs (%hs) ==================="),
             GetModeName(OldMode), pOldSsidLog,
             GetModeName((CiscoPort_t::CommandMode_e)NewMode), pNewSsidLog);

    pCLI->SetSsid(pNewSsid);
    return pCLI->SetCommandMode((CiscoPort_t::CommandMode_e)NewMode);
}

static DWORD
TestCommandModes(
    CiscoPort_t *pCLI)
{
    DWORD result = pCLI->Connect();
    if (NO_ERROR != result)
        return result;

    for (int oldMode = 0 ; oldMode < 7 ; ++oldMode)
    {
        for (int newMode = 0 ; newMode < 7 ; ++newMode)
        {
            result = ChangeMode(pCLI, pCLI->GetCommandMode(),
                                      pCLI->GetSsid(), oldMode);
            if (NO_ERROR != result)
                return result;
            result = ChangeMode(pCLI, pCLI->GetCommandMode(),
                                      pCLI->GetSsid(), newMode);
            if (NO_ERROR != result)
                return result;
        }
    }

    return result;
}
#endif /* TEST_COMMAND_MODES */

// ----------------------------------------------------------------------------
//
// Gets the configuration settings from the device.
//
DWORD
Cisco1200Controller_t::
LoadSettings(void)
{
#ifdef TEST_COMMAND_MODES
    DWORD result = TestCommandModes(m_pCLI);
    if (NO_ERROR != result)
        return result;
#endif

    Cisco1200Request_t request(this, m_pCLI, m_APInfo, m_pState);
    return request.LoadSettings();
}

// ----------------------------------------------------------------------------
//
// Updates the device's configuration settings.
//
DWORD
Cisco1200Controller_t::
UpdateSettings(const AccessPointState_t &NewState)
{
    DWORD result;

    // Update the configuration.
    Cisco1200Request_t request(this, m_pCLI, m_APInfo, m_pState);
    result = request.UpdateSettings(NewState);
    if (NO_ERROR != result)
        return result;

    // Save the updated configuration.
    result = SendCommand(CiscoPort_t::CommandModePrivileged, "write memory");
    if (NO_ERROR != result)
    {
        LogError(TEXT("Error saving updated configuration in"),
                 TEXT(" Cisco %hs at %s: %s"),
                 m_APInfo.ModelName,
                 m_pCLI->GetServerHost(),
                 Win32ErrorText(result));
        return result;
    }

    return NO_ERROR;
}

// ----------------------------------------------------------------------------
