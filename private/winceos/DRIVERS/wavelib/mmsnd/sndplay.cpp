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
#include <wavelib.h>
#include <resource.h>
#include <debug.h>
#include "audplay.h"
#include <unknown.h>
#include <filestream.h>

#define NUM_SOUND_EVENTS   21

// IMPORTANT: These table order can not be changed !!
const LPCWSTR gszSoundEvents[NUM_SOUND_EVENTS] =
{
    L"SystemAsterisk",              // asterisk
    L"Close",               // close program
    L"SystemHand",          // critical
    L"SystemDefault",       // default sound
    L"EmptyRecycleBin",     // empty recycle bin
    L"SystemExclamation",   // exclamation
    L"IRBegin",             // IR begin
    L"IREnd",               // IR end
    L"IRIntr",              // IR Interrupt
    L"MenuPopup",           // menu popup
    L"MenuCommand",         // menu selection
    L"Open",                // open program
    L"SystemQuestion",      // question
    L"RNBegin",             // remote networking begin
    L"RNEnd",               // remote networking end
    L"RNIntr",              // remote networking interrupt
    L"SystemStart",         // startup
    //L"Wakeup",            // wakeup
    L"Maximize",            // window maximize
    L"Minimize",            // window minimize
    L"RecordStart",                 // start recording
    L"RecordEnd",                   // end recording
};

// must be sorted like that !!!
const LPCWSTR gROM_DefaultScheme[NUM_SOUND_EVENTS] =
{
    L"Asterisk", // asterisk
    L"",             // close program
    L"critical", // critical
    L"default",      // default sound
    L"",         // empty recycle bin
    L"exclam",   // exclamation
    L"infbeg",       // IR begin
    L"infend",       // IR end
    L"infintr",      // IR Inetruupt
    L"",             // menu popup
    L"",             // menu selection
    L"",             // open program
    L"question", // question
    L"infbeg",       // remote networking begin
    L"infend",       // remote networking end
    L"infintr",      // remote networking interrupt
    L"startup",      // startup
    //L"wakeup",     // wakeup
    L"",             // window maximize
    L"",             // window minimize
    L"",             // start recording
    L"",             // end recording
};

const LPCWSTR gROM_NullScheme[] =
{
    L"",    // asterisk
    L"",    // close program
    L"",    // critical
    L"",    // default sound
    L"",    // empty recycle bin
    L"",    // exclamation
    L"",    // IR begin
    L"",    // IR end
    L"",    // IR Inetruupt
    L"",    // menu popup
    L"",    // menu selection
    L"",    // open program
    L"",    // question
    L"",    // remote networking begin
    L"",    // remote networking end
    L"",    // remote networking interrupt
    L"",    // startup
    //L"",  // wakeup
    L"",    // window maximize
    L"",    // window minimize
    L"",             // start recording
    L"",             // end recording
};

const WCHAR gszRegEventsFull[]          = L"Snd\\Event";
const WCHAR gszRegEventCache[]          = L"EventCache";
const WCHAR gszRegSchemes[]             = L"Scheme";
const WCHAR gszRegSchemesFull[]         = L"Snd\\Scheme";
const WCHAR gszRegCurrentScheme[]       = L".Scheme";
const WCHAR gszRegDefault[]             = L".Default";
const WCHAR gszRegNullScheme[]          = L".None";
const WCHAR gszWaveExt[]                = L".wav";

HINSTANCE g_hPlaySoundHookDll;
PFN_PLAY_SOUND_HOOK_START   g_pfnPlaySoundHookStart;
PFN_PLAY_SOUND_HOOK_STOP    g_pfnPlaySoundHookStop;
PFN_PLAY_SOUND_HOOK_UPDATE  g_pfnPlaySoundHookUpdate;

typedef struct
{
    LPWSTR  str;
    int             len;
} WSTRINFO;

// when resolving sound names in PlaySound, we expand out to file names
// using the following format strings for StringCchPrintf
WSTRINFO g_aFormatString[] =
{
    // format strings and their *effective* length - i.e. factor out the %s
        #define MKWSTR(str) {TEXT(str), _countof(TEXT(str) - 3)}

    MKWSTR("%s.wav"),
    MKWSTR("\\Windows\\%s"),
    MKWSTR("\\Windows\\%s.wav"),
};


const TCHAR szSystemDefaultSound[] = TEXT("SystemDefault"); // Name of default sound.


// MUTED must be 0, since variables are in .bss section (ie: zero-inite'd)
#define NO_MUTE_NOTIFY  0x01
#define NO_MUTE_APPS    0x02
#define NO_MUTE_EVENTS  0x04
#define NO_MUTE_ALL     NO_MUTE_NOTIFY | NO_MUTE_APPS | NO_MUTE_EVENTS


DWORD g_dwPlaySoundVolume; // applies only to PlaySound stream(s)
#define DEFAULT_INITIAL_VOLUME  0x9FFF9FFF       // a mid volume

// zero-init'ed by default, uses less ROM to let system do it...

int keyclick_volume;
int touchclick_volume;
int muting;  // no mute

PBYTE v_pKeyClick;
PBYTE v_pTouchClick;

static CRITICAL_SECTION g_csPlaySound;
static CRITICAL_SECTION g_csConfig;
static BOOL g_bUpdateConfig=TRUE;
static DWORD g_cNumBuffers = 0;
static DWORD g_msPerBuffer = 0;

static BOOL AbortCurrentSound();

void PlaySound_UpdateConfig(void);

// CSoundInfo - a sound described by a format descriptor and a data buffer
class CSoundInfo : public _simpleunknown<IAudNotify>
{
public:
    CSoundInfo();
    ~CSoundInfo();

    HRESULT Done();

    BOOL ReleaseAudPlay()
    {
        if (m_pAudPlay)
        {
            m_pAudPlay->Close();
            m_pAudPlay->Release();
            m_pAudPlay=NULL;
        }
        return TRUE;
    }

    DWORD Load (IStream * pStream);
    DWORD Play();
    VOID SetFlags(DWORD dwFlags) {m_dwFlags=dwFlags;}

    DWORD   m_dwFlags;

private:
    HANDLE  m_hevSoundDone;   // set by worker thread when sound is done
    IAudPlay *m_pAudPlay;
    IStream *m_pStream;
};

static CSoundInfo *g_pSndInfoCurrent = NULL;

//////////////////////////////////////////////////////////////////////////////
// Methods for CSoundInfo
//////////////////////////////////////////////////////////////////////////////
CSoundInfo::CSoundInfo() :
    m_pAudPlay(NULL),
    m_pStream(NULL),
    m_hevSoundDone(0)
{
}

CSoundInfo::~CSoundInfo()
{
    if (m_hevSoundDone)
    {
        CloseHandle(m_hevSoundDone);
        m_hevSoundDone=0;
    }

    ReleaseAudPlay();

    if (m_pStream)
    {
        m_pStream->Release();
        m_pStream = NULL;
    }
}

HRESULT CSoundInfo::Done()
{
    // If this is a looped sound, just restart it
    if (m_dwFlags & SND_LOOP)
    {
        m_pAudPlay->Seek(0);
        m_pAudPlay->Preroll();
        m_pAudPlay->Start();
        return S_OK;
    }

    if (g_pfnPlaySoundHookStop)
    {
        DEBUGMSG(ZONE_SPS, (TEXT("g_pfnPlaySoundHookStop\r\n")));
        g_pfnPlaySoundHookStop();
    }

    if (m_hevSoundDone)
    {
        SetEvent(m_hevSoundDone);
    }

    EnterCriticalSection(&g_csPlaySound);
    if (g_pSndInfoCurrent == this)
    {
        g_pSndInfoCurrent=NULL;
        Release();
    }
    LeaveCriticalSection(&g_csPlaySound);

    // Release the pAudPlay interface. This will turn around and release the CSoundInfo ref which will cause us to self-destruct.
    ReleaseAudPlay();

    return S_OK;
}

DWORD
CSoundInfo::Load (IStream * pStream)
{
    if (m_pStream)
    {
        m_pStream->Release();
    }
    m_pStream = pStream;
    return ERROR_SUCCESS;
}

// Play the currently loaded sound
DWORD CSoundInfo::Play()
{
    DWORD dwRet = ERROR_SUCCESS;

    ULONG Timeout = 0; // Max amount of time to wait (in ms) for sound to complete.
                       // If 0, don't wait at all.

    EnterCriticalSection(&g_csPlaySound);

    // If SND_NOSTOP is set, don't interrupt any currently playing sounds
    if ((m_dwFlags & SND_NOSTOP) && g_pSndInfoCurrent)
    {
        dwRet = ERROR_BUSY;
        goto Exit;
    }

    // Before we play a new sound, kill the current sound
    AbortCurrentSound();

    HRESULT hr;
    hr = CreateAudPlay(g_cNumBuffers, g_msPerBuffer, &m_pAudPlay);
    if (FAILED(hr))
    {
        dwRet = hr;
        goto Exit;
    }

    // Open the stream. This will query the format, open the wave driver, allocate buffers, etc.
    // Open will take a reference to us and to m_pStream
    hr = m_pAudPlay->Open(m_pStream, this);
    if (FAILED(hr))
    {
        dwRet = hr;
        goto Exit;
    }

    // If this is not an async sound, we need to wait for it to complete at the end of this function.
    if ( !(m_dwFlags & SND_ASYNC) )
    {
        if (!m_hevSoundDone)
        {
            m_hevSoundDone = CreateEvent(NULL, FALSE, FALSE, NULL);
        }

        // Query the duration here (since m_pAudPlay might go away before we wait if it's a very short sound).
        ULONG Duration = 0;
        m_pAudPlay->Duration(&Duration);

        // compute and use a timeout value, based on the expected duration of the currently playing sound
        Timeout = 100 + 2 * Duration;  // allow for a slop factor of 2X + 100ms
    }

    // Set the default volume to use for this sound
    m_pAudPlay->SetVolume(g_dwPlaySoundVolume);

    // Start playback now!
    hr = m_pAudPlay->Start();
    if (FAILED(hr))
    {
        dwRet = hr;
        goto Exit;
    }

    // If everything has gone okay, then exit
    AddRef();                       // Take a ref since we're giving a ptr to the global
    g_pSndInfoCurrent = this;       // Link into global ptr

Exit:

    // We release the critical section here to ensure we're not holding it while we wait.
    LeaveCriticalSection(&g_csPlaySound);

    // If we failed somewhere, cleanup
    if (dwRet != ERROR_SUCCESS)
    {
        ReleaseAudPlay();
    }
    // If we succeeded, we may need to wait for the sound to complete
    else if (Timeout)
    {
        WaitForSingleObject(m_hevSoundDone, Timeout);
    }

    return dwRet;
}

// Aborts the current sound.
// Note: ASSUMES g_csPlaySound already held!
BOOL AbortCurrentSound()
{
    BOOL bRet = TRUE;

    CSoundInfo *pSndInfo = g_pSndInfoCurrent;
    if (pSndInfo)
    {
        bRet = pSndInfo->ReleaseAudPlay();
        if (bRet)
        {
            g_pSndInfoCurrent=NULL;
            pSndInfo->Release();
        }
    }

    return bRet;
}

typedef struct
{
    PWSTR pszScheme[NUM_SOUND_EVENTS];  // Array of string pointers.
    CSoundInfo * pSndInfo[NUM_SOUND_EVENTS]; // event audio data cache
    PBYTE pbSchemeAlloc;                // Buffer allocated to hold scheme data. Null if ROM scheme used.
} SCHEME, * PSCHEME;

SCHEME g_scheme;

enum RESOLVE_ALIAS_TYPE
{
    EVENT_NOT_FOUND = 0,
    EVENT_ASSOC_NOT_FOUND,
    EVENT_ASSOC_FOUND,
    EVENT_ASSOC_FOUND_CACHED
};

// -----------------------------------------------------------------------------
//
// @doc     INTERNAL
//
// @func    BOOL | AudiopFileExists | Determine if the specified file exists.
//
// @parm    LPWSTR | lszBuffer | Pointer to the UNICODE filename (with path)
//
// @rdesc   TRUE if file exists, FALSE if it does not.
//
// -----------------------------------------------------------------------------
BOOL
AudiopFileExists(
                PWSTR szBuffer
                )
{
    HANDLE fh;

    fh = CreateFile (szBuffer, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                     NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );

    if (fh == (HANDLE) HFILE_ERROR)
    {
        DEBUGMSG(ZONE_SPS, (TEXT("FileExists... %s failed\r\n"), szBuffer));
        return FALSE;
    }
    else
    {
        DEBUGMSG(ZONE_SPS, (TEXT("FileExists... %s okay!\r\n"), szBuffer));
        CloseHandle(fh);
        return TRUE;
    }
}


// -----------------------------------------------------------------------------
//
// @doc     INTERNAL
//
// @func    BOOL | AudiopResolveAlias | This function gets the real name
//          of the sound file from an alias.
//
// @parm    PCWSTR | szAlias    | The alias.
// @parm    PWSTR  | szFileName | The real existing filename (return buffer)
//
// @rdesc   Returns FALSE if the file doesn't exist otherwise returns TRUE
//
// -----------------------------------------------------------------------------
RESOLVE_ALIAS_TYPE
AudiopResolveAlias(
                  PCWSTR szAlias,
                  __out_ecount(dwFileNameLength) PWSTR  szFileName,
                  DWORD  dwFileNameLength,
                  CSoundInfo **  ppSndInfo
                  )
{
    PTCHAR pSchemeAlias;
    int   EvtNum;
    RESOLVE_ALIAS_TYPE ratRet = EVENT_NOT_FOUND;

    DEBUGMSG(ZONE_SPS,(L"AudiopResolveAlias: '%s' \r\n",szAlias));

    if ((!szFileName) || (0==dwFileNameLength))
    {
        return EVENT_NOT_FOUND;
    }

    *szFileName = 0;            // assume no sound file
    //
    // make sure the alias name is in our table.
    //
    for (EvtNum = 0; EvtNum < NUM_SOUND_EVENTS; EvtNum++)
    {

        if (0 == wcscmp(gszSoundEvents[EvtNum], szAlias))
        {
            if (g_scheme.pSndInfo[EvtNum] != NULL)
            {
                *ppSndInfo = g_scheme.pSndInfo[EvtNum];
                g_scheme.pSndInfo[EvtNum]->AddRef();

                return EVENT_ASSOC_FOUND_CACHED;
            }

            pSchemeAlias = (PWSTR) g_scheme.pszScheme[EvtNum];

            if ( !*pSchemeAlias || *pSchemeAlias == ' ')
            {
                // Space or NULL mean no name.
                return EVENT_ASSOC_NOT_FOUND;
            }

            // everything is OK. found it.
            if (FAILED(StringCchCopy(szFileName,dwFileNameLength,pSchemeAlias)))
            {
                DEBUGMSG(ZONE_ERROR, (TEXT("SPS: AudiopResolveAlias exceeded string buffer\r\n")));
                return EVENT_NOT_FOUND;
            }

            return EVENT_ASSOC_FOUND;
        }
    }

    return EVENT_NOT_FOUND;
}


// -----------------------------------------------------------------------------
//
// @doc     INTERNAL
//
// @func    BOOL |  AudiopGetName | This function gets the real name
//          of the sound file from a relative name.
//
// @parm    LPCWSTR | lszSoundName | The name of the file to be resolved.
//
// @parm    LPWSTR  | lszBuffer | The resolved filename with complete path.
//
// @parm    DWORD | flags | Options (filename vs. alias...)
//
// @rdesc   Returns FALSE if the file doesn't exist otherwise returns TRUE
//
// @comm    The relative name is searched in the registry and the path.
//
// -----------------------------------------------------------------------------
BOOL
AudiopGetName(
             PCWSTR szSoundName,
             __out_ecount(dwFileNameLength) PWSTR  szFileName,
             DWORD  dwFileNameLength,
             UINT*  wFlags,
             CSoundInfo **  ppSndInfo
             )
{
    RESOLVE_ALIAS_TYPE  ratRet = EVENT_NOT_FOUND;

    // If the sound is defined in the registry, get it and remove the
    // description, otherwise assume it is a file and qualify it.
    // If we KNOW it is a filename do not look in the registry.
    //
    if (!(*wFlags & SND_FILENAME))
    {
        //        return    FALSE;
        ratRet = AudiopResolveAlias (szSoundName, szFileName, dwFileNameLength, ppSndInfo);

        switch (ratRet)
        {

        case EVENT_ASSOC_FOUND_CACHED:
            //
            // This name is an EVENT name.
            //
            *wFlags |= SND_EVENTS;
            return(TRUE);

        case EVENT_ASSOC_FOUND:
            //
            // This name is an EVENT name.
            //
            *wFlags |= SND_EVENTS;
            break;

        case EVENT_NOT_FOUND:
            //
            // This name wasn't an EVENT name, so it's an APPS sound.
            //
            if (*wFlags & SND_ALIAS)
            {
                //
                // If we explicitly asked for aliases and didn't find it
                // then we should fail the (snd)PlaySound.
                //
                return(FALSE);
            }
            *wFlags |= SND_APPS;

            if (FAILED(StringCchCopy(szFileName,dwFileNameLength,szSoundName)))
            {
                DEBUGMSG(ZONE_ERROR, (TEXT("SPS: AudiopGetName exceeded string buffer\r\n")));
                return FALSE;
            }

            break;

        case EVENT_ASSOC_NOT_FOUND:
            //
            // The event exists but it's grounded so we don't want anything
            // to play.
            //
            return(FALSE);
        }


    }
    else
    {
        //
        // Called with SND_FILENAME so we assume it's an APPS type of sound.
        //
        *wFlags |= SND_APPS;

        if (FAILED(StringCchCopy(szFileName,dwFileNameLength,szSoundName)))
        {
            DEBUGMSG(ZONE_ERROR, (TEXT("SPS: AudiopGetName exceeded string buffer\r\n")));
            return FALSE;
        }
    }

    if (AudiopFileExists(szFileName))
    {
        return TRUE;
    }

    {
        // Not happy about putting so many long strings on the stack; maybe we could reuse szSoundName, but
        // we'd have to make sure our caller doesn't depend on it being valid after returning from this function

        // Copy filename to temp so we can generate a bunch of other pathnames into szFileName and
        // try to see if they exist
        TCHAR szTempName[MAX_PATH];

        if (FAILED(StringCchCopy(szTempName,_countof(szTempName),szFileName)))
        {
            DEBUGMSG(ZONE_ERROR, (TEXT("SPS: AudiopGetName exceeded string buffer\r\n")));
            return FALSE;
        }

        // the file doesn't exist with the resolved name, but we can try a few other options

        for (int i = 0; i < _countof(g_aFormatString); i++)
        {

            if (FAILED(StringCchPrintf(szFileName, dwFileNameLength, g_aFormatString[i].str, szTempName)))
            {
                // Why continue? That's what the old code did in case of error here...
                continue;
            }

            if (AudiopFileExists(szFileName))
            {
                return TRUE;
            }
        }
    }

    // fell of the end of the format table and none of them yielded an existing file
    return FALSE;
}

// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
DWORD
LoadSoundInfo(
             LPCWSTR szSoundName,
             PUINT   pwFlags,
             CSoundInfo **  ppSndInfo
             )
{
    DWORD dwRet = ERROR_SUCCESS;

    *ppSndInfo = NULL;

    IStream *pStream = NULL;

    //
    // If we are looking for the sound in memory
    //
    if (*pwFlags & SND_MEMORY)
    {
        //
        // If the SND_MEMORY flag is supplied, then we assume it's an application
        // sound type.
        //
        *pwFlags |= SND_APPS;

        // The pointer to the memory file is passed in via the szSoundName parameter.
        //
        // TODO: Is there any way to determine the valid length? Apparently the API doesn't allow for this
        MemoryStream *pms = new MemoryStream((PBYTE) szSoundName, 0xFFFFFF);
        if (!pms)
        {
            return ERROR_OUTOFMEMORY;   //
        }

        pStream = pms;
    }
    else
    {
        // Not a memory-image file.
        // Now disambiguate between alias and filenames
        //

        // Temporary variable to hold the complete resolved filename from an
        // incomplete filename or a registry alias.
        //
        TCHAR szResolvedName[MAX_PATH];

        DEBUGMSG(ZONE_SPS, (TEXT("SPS: File name is (%s)\r\n"), szSoundName));

        //
        // Resolve the name and load the file from the file system
        //
        if (!AudiopGetName (szSoundName, szResolvedName, _countof(szResolvedName), pwFlags, ppSndInfo))
        {
            DEBUGMSG(ZONE_SPS, (TEXT("sndPlaySound: No association with Event Name.\r\n")));
            //
            // Load file failed, try the default sound...
            //
            if (!(*pwFlags & SND_NODEFAULT))
            {
                //
                // Now the sound is an alias to default. All other options apply.
                //
                *pwFlags &= ~SND_FILENAME;
                *pwFlags |=  SND_ALIAS;

                if (!AudiopGetName (szSystemDefaultSound, szResolvedName, _countof(szResolvedName), pwFlags, ppSndInfo))
                {
                    DEBUGMSG(ZONE_SPS,(TEXT("sndPlaySound: No association with Default Event Name.")));
                    return ERROR_FILE_NOT_FOUND;
                }
            }
            else
            {
                return ERROR_FILE_NOT_FOUND;
            }
        }

        // if we got a CSoundInfo back, we're done
        if (*ppSndInfo != NULL)
        {
            return ERROR_SUCCESS;
        }

        // otherwise, try to use the resolved name
        FileStream *pfs = new FileStream;
        if (!pfs)
        {
            return ERROR_OUTOFMEMORY; //
        }

        if (!pfs->open(szResolvedName))
        {
            pfs->Release();
            return ERROR_FILE_NOT_FOUND;
        }

        pStream = pfs;
    }

    CSoundInfo * pSndInfo = new CSoundInfo;
    if (pSndInfo == NULL)
    {
        pStream->Release();
        return ERROR_OUTOFMEMORY;
    }

    dwRet = pSndInfo->Load(pStream);
    if (dwRet == ERROR_SUCCESS)
    {
        *ppSndInfo = pSndInfo;
    }
    else
    {
        pStream->Release();
        pSndInfo->Release();
    }

    // one way or the the other, return the result
    return dwRet;
}




// ---------------------------------------------------------------------------
//
// @doc     DRIVERS
//
// @func    BOOL | sndPlaySound | Plays a waveform sound
//
// @parm    LPCTSTR | lpszSoundName | Name of sound to play.
//
// @parm    UINT | fuOptions | Specifies options for sound play.
//
// @rdesc   If the function was successful TRUE is returned; otherwise, FALSE.
//
// @comm    For a complete description of sndPlaySound, see the Win32
//          Programmers Reference Manual
//
// ---------------------------------------------------------------------------
BOOL
sndPlaySound_I (
               LPCWSTR szSoundName,
               UINT    wFlags
               )
{
    CSoundInfo * pSndInfo = NULL;
    DWORD dwRet           = ERROR_SUCCESS;
    BOOL bRet     = FALSE;


    if (g_bUpdateConfig)
    {
        EnterCriticalSection(&g_csConfig);
        if (g_bUpdateConfig)
        {
            PlaySound_UpdateConfig();
            g_bUpdateConfig=FALSE;
        }
        LeaveCriticalSection(&g_csConfig);
    }

    //
    // If NULL, then simply kill all sounds directly.
    //
    if (szSoundName == NULL)
    {

        if (wFlags == SND_TCHCLICK)
        {
            if (NULL == (szSoundName = (LPTSTR) v_pTouchClick))
            {
                // Get ptr to wavelib object
                // Call InitCallbackInterface to create the message queue, if it was not already created
                // else returns TRUE.
                CWaveLib *pCWaveLib = GetWaveLib();
                if(pCWaveLib)
                    pCWaveLib->InitCallbackInterface();
                // we have no registered touch click sound to play, so we'll just pretend everything is OK.
                bRet = TRUE;
                goto Exit;
            }
            else
            {
                wFlags |= SND_NOSTOP | SND_ASYNC | SND_MEMORY;
            }
        }
        else if (wFlags == SND_KEYCLICK)
        {
            if (NULL == (szSoundName = (LPTSTR) v_pKeyClick))
            {
                // we have no registered touch click sound to play, so we'll just pretend everything is OK.
                bRet = TRUE;
                goto Exit;
            }
            else
            {
                wFlags |= SND_NOSTOP | SND_ASYNC | SND_MEMORY;
            }
        }
        else
        {
            // The "cancel" case.
            // sndPlaySound(NULL,x) just cancels any pending PlaySound operations
            EnterCriticalSection(&g_csPlaySound);
            AbortCurrentSound();
            LeaveCriticalSection(&g_csPlaySound);
            bRet = TRUE;
            goto Exit;
        }
    }

    EnterCriticalSection(&g_csConfig);
    dwRet = LoadSoundInfo(szSoundName, &wFlags, &pSndInfo);
    LeaveCriticalSection(&g_csConfig);
    if (dwRet != ERROR_SUCCESS)
    {
        bRet = FALSE;
        goto Exit;
    }

    // If this is an APPS sound, but not a click, ignore if muted
    if ((wFlags & SND_APPS) && !(wFlags & SND_CLICK) && !(muting & NO_MUTE_APPS))
    {
        bRet = TRUE;
        goto Exit;
    }

    // Same for EVENT type sounds...
    if ((wFlags & SND_EVENTS) && !(muting & NO_MUTE_EVENTS))
    {
        bRet = TRUE;
        goto Exit;
    }

    if (g_pfnPlaySoundHookStart)
    {
        DEBUGMSG(ZONE_SPS, (TEXT("g_pfnPlaySoundHookStart\r\n")));
        if (!g_pfnPlaySoundHookStart(szSoundName, wFlags))
        {
            //
            // The hook says to ignore it, but it's not an error.
            //
            bRet = TRUE;
            goto Exit;
        }
    }

    AccessibilitySoundSentryEvent();

    pSndInfo->SetFlags(wFlags);
    dwRet = pSndInfo->Play();
    bRet = (dwRet == ERROR_SUCCESS);

    Exit:
    if (pSndInfo)
    {
        pSndInfo->Release();
    }

    DEBUGMSG(ZONE_SPS, (TEXT("Exit sndPlaySound [%d]!!\r\n"), dwRet));
    SetLastError(dwRet);
    return(bRet);
}



//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL
PlaySoundW(
          LPCTSTR pszSound,
          HMODULE hmod,
          DWORD   fdwSound
          )
{
    BOOL    bRet = FALSE;

    __try
    {
        // Note: The following code for SND_MEMORY and SND_RESOURCE relies on the fact
        // that mmsystem defines:
        //
        // #define SND_MEMORY          0x0004  /* pszSound points to a memory file */
        // #define SND_RESOURCE    0x00040004L /* name is resource name or atom */
        //
        // Therefore, when we see SND_RESOURCE, we just load the file into memory,
        // point pszSound at it, and call the common sndPlaySound_I function with the
        // SND_RESOURCE flag still set.
        // sndPlaySound_I will check the flags with (Flags & SND_MEMORY) and think
        // it's a SND_MEMORY and take care of the rest.


        // Is it a resource?
        // Note: There's a small contradiction in the API definition. The API says that if pszSound is NULL, we
        // should just stop playing all audio. However, if we need to play a sound resource using a resource ID,
        // and that resource ID happens to be 0 (which I think is legal), then that would conflict with the first
        // rule. I decided that if the flags say SND_RESOURCE, that takes precedence.
        if ((fdwSound & SND_RESOURCE) == SND_RESOURCE)
        {
            // Is it the name of a resource, or a resource ID?
            // If it's the name of a resource, we need to map the pointer and copy the string
            // to a temp buffer.
            // If it's a resource ID, we can just pass the ID into FindResource as-is
            // If any of the upper 16 bits of pszSound are set, it's a pointer to a resource name

            // Load the resource into memory
            HRSRC   hrsrc;
            hrsrc = FindResource (hmod, pszSound, TEXT("WAVE"));
            if (hrsrc == NULL)
            {
                SetLastError(ERROR_RESOURCE_DATA_NOT_FOUND);
                goto Exit;
            }

            pszSound = (LPTSTR) LoadResource (hmod, hrsrc);
            if (pszSound == NULL)
            {
                SetLastError(ERROR_RESOURCE_DATA_NOT_FOUND);
                goto Exit;
            }
        }

        if (fdwSound & SND_NOWAIT)
            fdwSound |= SND_NOSTOP;

        bRet = sndPlaySound_I (pszSound, fdwSound);
    }
    __except( EXCEPTION_EXECUTE_HANDLER )
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        bRet = FALSE;
    }

    Exit:
    return bRet;
}


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
BOOL
sndPlaySoundW(
             LPCTSTR pszSound,
             UINT    fuSound
             )
{
    // sndPlaySound doesn't support some of the flags from PlaySound
    if (  ((fuSound & SND_RESOURCE) == SND_RESOURCE)
          || ((fuSound & SND_NOWAIT  ) == SND_NOWAIT  ) )
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    // Just call back to wapi_PlaySound, which has all the appropriate pointer mapping & security code
    return PlaySoundW(pszSound, 0, fuSound);
}



// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
PBYTE GetWaveResource (HMODULE hModule, int irsrc)
{
    PBYTE pb;
    HRSRC  hrsrc;
    HGLOBAL hResource;

    hrsrc = FindResource (hModule, MAKEINTRESOURCE(irsrc), TEXT("WAVE"));
    if (hrsrc == NULL)
    {
        DEBUGMSG(ZONE_ERROR, (TEXT("FindResource FAILED (%d)!\r\n"), GetLastError()));
    }
    hResource = LoadResource (hModule, hrsrc);
    if (hResource == NULL)
    {
        DEBUGMSG(ZONE_ERROR, (TEXT("LoadResource FAILED (%d)!\r\n"), GetLastError()));
    }
    pb = (PBYTE) LockResource (hResource);
    if (pb == NULL)
    {
        DEBUGMSG(ZONE_ERROR, (TEXT("LockResource FAILED (%d)!\r\n"), GetLastError()));
    }
    return pb;
}

// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
VOID
GetAllWaveResources (HKEY hClickKey)
{
    switch (keyclick_volume)
    {
    case WC_CLICKVOL_MUTED :
        v_pKeyClick = NULL;
        break;

    case WC_CLICKVOL_QUIET :
        v_pKeyClick = GetWaveResource (g_hInstance, ID_WAVE_KEYSOFT);
        break;

    case WC_CLICKVOL_LOUD :
        v_pKeyClick = GetWaveResource (g_hInstance, ID_WAVE_KEYLOUD);
        break;
    }

    switch (touchclick_volume)
    {
    case WC_CLICKVOL_MUTED :
        v_pTouchClick = NULL;
        break;

    case WC_CLICKVOL_QUIET :
        v_pTouchClick = GetWaveResource (g_hInstance, ID_WAVE_TCHSOFT);
        break;

    case WC_CLICKVOL_LOUD :
        v_pTouchClick = GetWaveResource (g_hInstance, ID_WAVE_TCHLOUD);
        break;
    }
}

// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
void PlaySound_UpdateSettings(void)
{
    g_bUpdateConfig=TRUE;
}

void PlaySound_UpdateConfig(void)
{
    DWORD dwRegValue;
    HKEY  hCplKey;
    HKEY  hSndKey = NULL;
    DWORD dwSize, dwType, i;
    LONG Result;
    PTCHAR lpSchemeEvents;
    TCHAR szSchemeName[64];
    DWORD dwEventCache;

    // See if someone wants PlaySound to be hooked
    HKEY hKey;
    Result = RegOpenKeyExW(HKEY_LOCAL_MACHINE,L"Drivers\\BuiltIn\\WAPIMAN", 0,  KEY_READ, &hKey);
    if (Result == ERROR_SUCCESS)
    {
        DWORD dwSize, dwType;
        TCHAR szDll[40];
        dwSize = sizeof(szDll);
        Result = RegQueryValueExW (hKey, (LPTSTR) TEXT("PlaySoundHookDll"), 0, &dwType, (LPBYTE)szDll, &dwSize);

        if ( (Result == ERROR_SUCCESS) && (dwType == REG_SZ) )
        {
            szDll[_countof(szDll)-1] = _T('\0');    // PREFAST: Call to 'RegQueryValueExW' might not zero-terminate string

            //
            // OK, lets see if we can actually load this DLL
            //
            if (NULL != (g_hPlaySoundHookDll = LoadLibrary( szDll )))
            {
                g_pfnPlaySoundHookStart  = (PFN_PLAY_SOUND_HOOK_START)  GetProcAddress (g_hPlaySoundHookDll, TEXT("PlaySoundHookStart"));
                g_pfnPlaySoundHookStop   = (PFN_PLAY_SOUND_HOOK_STOP)   GetProcAddress (g_hPlaySoundHookDll, TEXT("PlaySoundHookStop"));
                g_pfnPlaySoundHookUpdate = (PFN_PLAY_SOUND_HOOK_UPDATE) GetProcAddress (g_hPlaySoundHookDll, TEXT("PlaySoundHookUpdate"));
            }
            else
            {
                g_pfnPlaySoundHookStart  = NULL;
                g_pfnPlaySoundHookStop   = NULL;
                g_pfnPlaySoundHookUpdate = NULL;
                DEBUGMSG(ZONE_TEST, (TEXT("Unable to open PlaySoundHookDLL \"%s\"\r\n"), szDll));
            }
        }

        RegCloseKey(hKey);
    }

    if (g_pfnPlaySoundHookUpdate)
    {
        DEBUGMSG(ZONE_SPS, (TEXT("g_pfnPlaySoundHookUpdate\r\n")));
        g_pfnPlaySoundHookUpdate();
    }

    //
    // Get clicking preferences
    //
    Result = RegOpenKeyExW(HKEY_CURRENT_USER,L"ControlPanel\\Volume", 0, KEY_READ, &hCplKey);

    if (Result == ERROR_SUCCESS)
    {
        //-----------------------------------------------------------------------
        //                                                          PlaySound volume
        dwSize = sizeof(DWORD);
        Result = RegQueryValueExW (hCplKey, (LPTSTR) TEXT("PlaySound"), 0, &dwType,
                                   (LPBYTE)&dwRegValue, &dwSize);

        if ( (Result == ERROR_SUCCESS) && (dwType == REG_DWORD))
        {
            g_dwPlaySoundVolume = dwRegValue;
        }

        //-----------------------------------------------------------------------
        //                                                          Key Volume
        dwSize = sizeof(DWORD);
        Result = RegQueryValueExW (hCplKey, (LPTSTR) TEXT("Key"), 0, &dwType,
                                   (LPBYTE)&dwRegValue, &dwSize);

        dwRegValue &= 0xffff;     // upper word is used for something else
        if ( (Result != ERROR_SUCCESS) ||
             (dwType != REG_DWORD)     ||
             (dwRegValue > 2))

            keyclick_volume = WC_CLICKVOL_MUTED;
        else
            keyclick_volume = dwRegValue;

        //-----------------------------------------------------------------------
        //                                                          Touch Volume
        dwSize = sizeof(DWORD);
        Result = RegQueryValueExW (hCplKey, (LPTSTR) TEXT("Screen"), 0, &dwType,
                                   (LPBYTE)&dwRegValue, &dwSize);

        dwRegValue &= 0xffff;     // upper word is used for something else
        if ( (Result != ERROR_SUCCESS) ||
             (dwType != REG_DWORD)     ||
             (dwRegValue > 2))
            touchclick_volume = WC_CLICKVOL_MUTED;
        else
            touchclick_volume = dwRegValue;

        //-----------------------------------------------------------------------
        //                                                          Muting
        dwSize = sizeof(DWORD);
        Result = RegQueryValueExW (hCplKey, (LPTSTR) TEXT("Mute"), 0, &dwType,
                                   (LPBYTE)&dwRegValue, &dwSize);

        if ( (Result != ERROR_SUCCESS) ||
             (dwType != REG_DWORD)     ||
             (dwRegValue > 0x7))
            muting = NO_MUTE_ALL;
        else
            muting = dwRegValue;

        RegCloseKey(hCplKey);
    }
    else
    {
        keyclick_volume = WC_CLICKVOL_MUTED;
        touchclick_volume = WC_CLICKVOL_MUTED;
        muting = NO_MUTE_ALL;
    }

    //--------------------------------------------------------------------------
    //--------------------------------------------------------------------------
    //
    // Get the sound scheme.
    //
    if (g_scheme.pbSchemeAlloc != NULL)
    {
        //
        // Free the previous alloc if any.
        //
        LocalFree(g_scheme.pbSchemeAlloc);
        g_scheme.pbSchemeAlloc = NULL;
    }

    for (i = 0; i < NUM_SOUND_EVENTS; i++)
    {
        if (g_scheme.pSndInfo[i] != NULL)
        {
            //
            // Free the previous alloc if any.
            //
            g_scheme.pSndInfo[i]->Release();
            g_scheme.pSndInfo[i] = NULL;
        }
    }

    Result = RegOpenKeyExW(HKEY_LOCAL_MACHINE, gszRegEventsFull, 0, KEY_READ, &hSndKey);

    if (Result == ERROR_SUCCESS)
    {

        //-----------------------------------------------------------------------
        //                                                          Event
        dwSize = sizeof(szSchemeName);
        Result = RegQueryValueExW (hSndKey, (LPTSTR) gszRegCurrentScheme, 0, &dwType,
                                   (LPBYTE)szSchemeName, &dwSize);

        if ( (Result != ERROR_SUCCESS) || (dwType != REG_SZ) )
        {
            DEBUGMSG(ZONE_SPS,(TEXT("Querying current scheme Failed. Using defaults.")));
            goto SCHEME_USE_DEFAULT;
        }

        szSchemeName[_countof(szSchemeName)-1] = _T('\0');    // PREFAST: Call to 'RegQueryValueExW' might not zero-terminate string

        if (!wcscmp(szSchemeName, (LPTSTR)gszRegDefault))
        {
            //
            // is it the default sound scheme, use the ROM table
            //
            for (i = 0; i < NUM_SOUND_EVENTS; i++)
            {
                g_scheme.pszScheme[i] = (LPWSTR) gROM_DefaultScheme[i];
            }
        }
        else if (!wcscmp(szSchemeName, (LPTSTR)gszRegNullScheme))
        {
            //
            // is it NULL default sound scheme, use the ROM table
            //
            for (i = 0; i < NUM_SOUND_EVENTS; i++)
            {
                g_scheme.pszScheme[i] = (LPWSTR) gROM_NullScheme[i];
            }
        }
        else
        {
            //
            // it is not the default tables, find the length of the scheme string
            //
            Result = RegQueryValueEx(hSndKey, (LPTSTR)szSchemeName, NULL, &dwType ,(LPBYTE)NULL, &dwSize);
            if ( (Result != ERROR_SUCCESS) || (dwType != REG_MULTI_SZ) )
            {
                DEBUGMSG(ZONE_SPS,(TEXT("UpdateFromRegistry: RegQueryValueEx Failed")));
                goto SCHEME_USE_DEFAULT;
            }

            // we have the size, now allocate the memory
            g_scheme.pbSchemeAlloc = (PUCHAR) LocalAlloc(LMEM_FIXED, dwSize + sizeof(TCHAR));
            if (g_scheme.pbSchemeAlloc == NULL)
            {
                goto SCHEME_USE_DEFAULT;
            }

            Result = RegQueryValueEx(hSndKey, (LPTSTR)szSchemeName, NULL, &dwType ,(LPBYTE)g_scheme.pbSchemeAlloc, &dwSize);
            if ( (Result != ERROR_SUCCESS) || (dwType != REG_MULTI_SZ) )
            {
                DEBUGMSG(ZONE_SPS,(TEXT("UpdateFromRegistry: RegQueryValueEx Failed")));
                //
                // Free the alloc, then use the default.
                //
                LocalFree(g_scheme.pbSchemeAlloc);
                g_scheme.pbSchemeAlloc = NULL;
                goto SCHEME_USE_DEFAULT;
            }

            //
            // now lets get the event file names from the MULTI_SZ string.
            //
            lpSchemeEvents = (PWSTR) g_scheme.pbSchemeAlloc;
            for (i = 0; i < NUM_SOUND_EVENTS; i++)
            {
                g_scheme.pszScheme[i] = lpSchemeEvents;
                lpSchemeEvents += wcslen(lpSchemeEvents) + 1;
            }
        }

    }
    else
    {
        //
        // Use default if no registry.
        //
        DEBUGMSG(ZONE_SPS,(TEXT("Opening Snd\\Events Failed. Using defaults.")));
        SCHEME_USE_DEFAULT:
        for (i = 0; i < NUM_SOUND_EVENTS; i++)
        {
            g_scheme.pszScheme[i] = (LPWSTR) gROM_DefaultScheme[i];
        }
    }


    if (hSndKey != NULL)
    {
        //-----------------------------------------------------------------------
        //                                                          EventCache
        dwSize = sizeof(DWORD);
        Result = RegQueryValueExW (hSndKey, (LPTSTR) gszRegEventCache, 0, &dwType,
                                   (LPBYTE)&dwEventCache, &dwSize);

        if ( (Result != ERROR_SUCCESS) || (dwType != REG_DWORD) )
        {
            // None specified...
            dwEventCache = 0;
        }
        else
        {

            DEBUGMSG(ZONE_VERBOSE, (TEXT("Event Cache = 0x%08X\r\n"), dwEventCache));
            for (i = 0; i < NUM_SOUND_EVENTS; i++)
            {
                if ((dwEventCache >> i) & 1)
                {

                    DWORD dwRet;
                    CSoundInfo * pSndInfo;
                    UINT wFlags = SND_NODEFAULT;
                    //
                    // Load up the event wave file.
                    //
                    dwRet = LoadSoundInfo(gszSoundEvents[i], &wFlags, &pSndInfo);
                    if (dwRet == ERROR_SUCCESS)
                    {
                        DEBUGMSG(ZONE_VERBOSE, (TEXT("Caching wave event %s (0x%08X)\r\n"), gszSoundEvents[i], pSndInfo));
                        g_scheme.pSndInfo[i] = pSndInfo;
                    }
                }
            }
        }
    }

    if (hSndKey != NULL)
        RegCloseKey(hSndKey);

    //
    // See if we are handling clicks, or if another DLL is handling them
    //
    v_pKeyClick = NULL;
    v_pTouchClick = NULL;

    switch (keyclick_volume)
    {
    case WC_CLICKVOL_MUTED :
        v_pKeyClick = NULL;
        break;

    case WC_CLICKVOL_QUIET :
        v_pKeyClick = GetWaveResource (g_hInstance, ID_WAVE_KEYSOFT);
        break;

    case WC_CLICKVOL_LOUD :
        v_pKeyClick = GetWaveResource (g_hInstance, ID_WAVE_KEYLOUD);
        break;
    }

    switch (touchclick_volume)
    {
    case WC_CLICKVOL_MUTED :
        v_pTouchClick = NULL;
        break;

    case WC_CLICKVOL_QUIET :
        v_pTouchClick = GetWaveResource (g_hInstance, ID_WAVE_TCHSOFT);
        break;

    case WC_CLICKVOL_LOUD :
        v_pTouchClick = GetWaveResource (g_hInstance, ID_WAVE_TCHLOUD);
        break;
    }

}

BOOL
SPS_Init (VOID)
{
    InitializeCriticalSection(&g_csPlaySound);
    InitializeCriticalSection(&g_csConfig);

    // Read configurable settings for our buffers
    HKEY hKey = NULL;
    DWORD dwOpenRegKey = RegOpenKeyEx(HKEY_LOCAL_MACHINE, TEXT("Audio\\PlaySound"), 0, KEY_READ, &hKey);
    if(dwOpenRegKey == ERROR_SUCCESS)
    {
        DWORD dwType, numBuffers, msPerBuffer, dwSize, dwQuery;
        dwQuery = RegQueryValueExW(hKey, TEXT("Buffers"), NULL, &dwType, (LPBYTE) &numBuffers, &dwSize);
        if(dwQuery == ERROR_SUCCESS && dwType == REG_DWORD)
        {
            g_cNumBuffers = numBuffers;
        }
        dwQuery = RegQueryValueExW(hKey, TEXT("msPerBuffer"), NULL, &dwType, (LPBYTE) &msPerBuffer, &dwSize);
        if(dwQuery == ERROR_SUCCESS && dwType == REG_DWORD)
        {
            g_msPerBuffer = msPerBuffer;
        }
    }
    if(hKey != NULL)
    {
        RegCloseKey(hKey);
    }

    g_dwPlaySoundVolume = 0xFFFFFFFF;
    g_bUpdateConfig=TRUE;

    return TRUE;
}

BOOL
SPS_DeInit (VOID)
{
    if (g_hPlaySoundHookDll != NULL)
    {
        FreeLibrary(g_hPlaySoundHookDll);
    }

    DeleteCriticalSection(&g_csPlaySound);
    DeleteCriticalSection(&g_csConfig);
    return TRUE;
}


