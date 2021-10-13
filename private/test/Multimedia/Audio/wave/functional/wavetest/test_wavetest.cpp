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
#include "TUXMAIN.H"
#include "TEST_WAVETEST.H"
#include "ERRORMAP.H"

#include <Nkintr.h>
#include <pwrtestlib.h>
#include "WaveFileEx.h"

#define ROUND_MS_TO_SEC(T)         ((T+500)/1000)

#define FULL_VOLUME_LEFT           0xFFFF0000
#define FULL_VOLUME_RIGHT          0x0000FFFF
#define INCREMENT_VOLUME_LEFT      0x10000000
#define INCREMENT_VOLUME_RIGHT     0x00001000

#define BITSPERSAMPLE              8

#define CHANNELS                   2
#define SAMPLESPERSEC              44100
#define BLOCKALIGN                 CHANNELS * BITSPERSAMPLE / 8
#define AVGBYTESPERSEC             SAMPLESPERSEC * BLOCKALIGN
#define SIZE                       0

#define THREAD_ABANDONED           2
#define THREAD_FAILED              3
#define THREAD_SUCCEEDED           0
#define THREAD_TIMEOUT             1
#define THREAD_UNTESTED            4

#define WAVE_FORMAT_8000M08       0x00001001       /* 8 kHz, Mono,   8-bit  */
#define WAVE_FORMAT_8000M16       0x00001002       /* 8 kHz, Mono, 16-bit  */
#define WAVE_FORMAT_8000S08       0x00001004       /* 8 kHz, Stereo, 8-bit  */
#define WAVE_FORMAT_8000S16       0x00001008       /* 8 kHz, Stereo, 16-bit  */
#define WAVE_FORMAT_16000M08      0x00001010       /* 16 kHz, Mono, 8-bit  */
#define WAVE_FORMAT_16000M16      0x00001020       /* 16 kHz, Mono, 16-bit  */
#define WAVE_FORMAT_16000S08      0x00001040       /* 16 kHz, Stereo, 8-bit  */
#define WAVE_FORMAT_16000S16      0x00001080       /* 16 kHz, Stereo, 16-bit  */
#define WAVE_FORMAT_48000M08      0x00001100       /* 48 kHz, Mono, 8-bit  */
#define WAVE_FORMAT_48000M16      0x00001200       /* 48 kHz, Mono, 16-bit  */
#define WAVE_FORMAT_48000S08      0x00001400       /* 48 kHz, Stereo, 8-bit  */
#define WAVE_FORMAT_48000S16      0x00001800       /* 48 kHz, Stereo, 16-bit  */

#define CheckRegHRESULT(_hr,_function,_string,_cause) \
    if(_hr!=ERROR_SUCCESS) { \
        LOG(TEXT("ERROR:  %s %s failed with HRESULT=%x %s [%s:%u]\n"),TEXT(#_function),_string,hr,TEXT(__FILE__),__LINE__); \
        LOG(TEXT("Possible Cause:  %s\n"),TEXT(#_cause)); \
        return _hr; \
     }
#define QueryRegHRESULT(_hr,_function,_string,_cause) \
    if(_hr!=ERROR_SUCCESS) { \
        LOG(TEXT("ERROR:  %s %s failed with HRESULT=%x %s [%s:%u]\n"),TEXT(#_function),_string,hr,TEXT(__FILE__),__LINE__); \
        LOG(TEXT("Possible Cause:  %s\n"),TEXT(#_cause)); \
    }

typedef WINMMAPI MMRESULT (WINAPI *MMgetFunction)(HWAVEOUT,LPDWORD);
typedef WINMMAPI MMRESULT (WINAPI *MMsetFunction)(HWAVEOUT,DWORD);

typedef struct extension
{
    TCHAR *szName;
    MMgetFunction get;
    MMsetFunction set;
    DWORD value;
    DWORD flag;
    double fvalue;
    BOOL bContinue;
} extension;


BOOL  g_bNeedCSVFilePlayHeader     = TRUE;
DWORD g_dwNoOfThreads              = 13;
int   g_iLatencyTestDuration       = 0;

// Global Char to be used as user input for Headless Interactive user input scenarios
TCHAR g_tcharUserInput             = NULL;

DWORD g_dwOutDeviceID              = 0;
DWORD g_dwInDeviceID               = 0;
DWORD g_bSkipIn                    = FALSE;
DWORD g_bSkipOut                   = FALSE;
DWORD g_useNOTIFY                  = TRUE;
DWORD g_useSound                   = TRUE;
DWORD g_interactive                = FALSE;  // Run in interactive mode
DWORD g_headless                   = FALSE;  // Run in headless mode
DWORD g_powerTests                 = FALSE;  // Run power management tests
DWORD g_duration                   = 1;
DWORD g_dwSleepInterval            = 50;    //50 milliseconds
DWORD g_dwAllowance                = 200;    //200 milliseconds
DWORD g_dwInAllowance              = 200;    //200 milliseconds
DWORD g_oldVol                     = 36045;  // 0.55*65535 (65535 == Maximum Volume)
                                             // This global only needed for interactive portion of the TestVolume Tests

BEGINMAP(DWORD,lpFormats)
ENTRY(WAVE_FORMAT_8000M08)
ENTRY(WAVE_FORMAT_8000M16)
ENTRY(WAVE_FORMAT_8000S08)
ENTRY(WAVE_FORMAT_8000S16)
ENTRY(WAVE_FORMAT_16000M08)
ENTRY(WAVE_FORMAT_16000M16)
ENTRY(WAVE_FORMAT_16000S08)
ENTRY(WAVE_FORMAT_16000S16)
ENTRY(WAVE_FORMAT_1M08)
ENTRY(WAVE_FORMAT_1M16)
ENTRY(WAVE_FORMAT_1S08)
ENTRY(WAVE_FORMAT_1S16)
ENTRY(WAVE_FORMAT_2M08)
ENTRY(WAVE_FORMAT_2M16)
ENTRY(WAVE_FORMAT_2S08)
ENTRY(WAVE_FORMAT_2S16)
ENTRY(WAVE_FORMAT_4M08)
ENTRY(WAVE_FORMAT_4M16)
ENTRY(WAVE_FORMAT_4S08)
ENTRY(WAVE_FORMAT_4S16)
ENTRY(WAVE_FORMAT_48000M08)
ENTRY(WAVE_FORMAT_48000M16)
ENTRY(WAVE_FORMAT_48000S08)
ENTRY(WAVE_FORMAT_48000S16)
ENDMAP

BEGINMAP(DWORD,lpCallbacks)
ENTRY(CALLBACK_NULL)
ENTRY(CALLBACK_EVENT)
ENTRY(CALLBACK_THREAD)
ENTRY(CALLBACK_FUNCTION)
ENTRY(CALLBACK_WINDOW)
ENDMAP

#define EXTENTRY(a,b,c,d,e)     {{TEXT(#b),waveOutGet##b,waveOutSet##b,c,d,e,TRUE},TEXT(#a)},
#define EXTSTOPENTRY(a,b,c,d,e)    {{TEXT(#b),waveOutGet##b,waveOutSet##b,c,d,e,FALSE},TEXT(#a)},

BEGINMAP(extension,lpExtensions)
EXTENTRY    ("Change Pitch to 50%",        Pitch,0x00008000,WAVECAPS_PITCH,0.5*100.0)
EXTENTRY    ("Change Pitch to 100%",    Pitch,0x00010000,WAVECAPS_PITCH,1.0*100.0)
EXTENTRY    ("Change Pitch to 200%",    Pitch,0x00020000,WAVECAPS_PITCH,2.0*100.0)
EXTENTRY    ("Change Pitch to 250%",    Pitch,0x00028000,WAVECAPS_PITCH,2.5*100.0)
EXTSTOPENTRY    ("Change Pitch to 500%",    Pitch,0x00050000,WAVECAPS_PITCH,5.0*100.0)
EXTENTRY    ("Change PlaybackRate to 50%",    PlaybackRate,0x00008000,WAVECAPS_PLAYBACKRATE,0.5*100.0)
EXTENTRY    ("Change PlaybackRate to 100%",    PlaybackRate,0x00010000,WAVECAPS_PLAYBACKRATE,1.0*100.0)
EXTENTRY    ("Change PlaybackRate to 200%",    PlaybackRate,0x00020000,WAVECAPS_PLAYBACKRATE,2.0*100.0)
EXTENTRY    ("Change PlaybackRate to 250%",    PlaybackRate,0x00028000,WAVECAPS_PLAYBACKRATE,2.5*100.0)
EXTSTOPENTRY    ("Change PlaybackRate to 300%",    PlaybackRate,0x00030000,WAVECAPS_PLAYBACKRATE,3.0*100.0)
#ifdef MORE_VOLUME_TESTS
EXTENTRY    ("Change Right Volume to 100%",    Volume,0xFFFF0000,WAVECAPS_VOLUME,(float)0xFFFF/0xFFFF*100.0)
EXTENTRY    ("Change Right Volume to 94%",    Volume,0xEFFF0000,WAVECAPS_VOLUME,(float)0xEFFF/0xFFFF*100.0)
EXTENTRY    ("Change Right Volume to 87%",    Volume,0xDFFF0000,WAVECAPS_VOLUME,(float)0xDFFF/0xFFFF*100.0)
EXTENTRY    ("Change Right Volume to 81%",    Volume,0xCFFF0000,WAVECAPS_VOLUME,(float)0xCFFF/0xFFFF*100.0)
EXTENTRY    ("Change Right Volume to 75%",    Volume,0xBFFF0000,WAVECAPS_VOLUME,(float)0xBFFF/0xFFFF*100.0)
EXTENTRY    ("Change Right Volume to 69%",    Volume,0xAFFF0000,WAVECAPS_VOLUME,(float)0xAFFF/0xFFFF*100.0)
EXTENTRY    ("Change Right Volume to 62%",    Volume,0x9FFF0000,WAVECAPS_VOLUME,(float)0x9FFF/0xFFFF*100.0)
EXTENTRY    ("Change Right Volume to 56%",    Volume,0x8FFF0000,WAVECAPS_VOLUME,(float)0x8FFF/0xFFFF*100.0)
EXTENTRY    ("Change Right Volume to 50%",    Volume,0x7FFF0000,WAVECAPS_VOLUME,(float)0x7FFF/0xFFFF*100.0)
EXTENTRY    ("Change Right Volume to 44%",    Volume,0x6FFF0000,WAVECAPS_VOLUME,(float)0x6FFF/0xFFFF*100.0)
EXTENTRY    ("Change Right Volume to 37%",    Volume,0x5FFF0000,WAVECAPS_VOLUME,(float)0x5FFF/0xFFFF*100.0)
EXTENTRY    ("Change Right Volume to 31%",    Volume,0x4FFF0000,WAVECAPS_VOLUME,(float)0x4FFF/0xFFFF*100.0)
EXTENTRY    ("Change Right Volume to 25%",    Volume,0x3FFF0000,WAVECAPS_VOLUME,(float)0x3FFF/0xFFFF*100.0)
EXTENTRY    ("Change Right Volume to 19%",    Volume,0x2FFF0000,WAVECAPS_VOLUME,(float)0x2FFF/0xFFFF*100.0)
EXTENTRY    ("Change Right Volume to 12%",    Volume,0x1FFF0000,WAVECAPS_VOLUME,(float)0x1FFF/0xFFFF*100.0)
EXTSTOPENTRY    ("Change Right Volume to 6%",    Volume,0x0FFF0000,WAVECAPS_VOLUME,(float)0x0FFF/0xFFFF*100.0)
EXTENTRY    ("Change Left Volume to 100%",    Volume,0x0000FFFF,WAVECAPS_VOLUME,(float)0xFFFF/0xFFFF*100.0)
EXTENTRY    ("Change Left Volume to 94%",    Volume,0x0000EFFF,WAVECAPS_VOLUME,(float)0xEFFF/0xFFFF*100.0)
EXTENTRY    ("Change Left Volume to 87%",    Volume,0x0000DFFF,WAVECAPS_VOLUME,(float)0xDFFF/0xFFFF*100.0)
EXTENTRY    ("Change Left Volume to 81%",    Volume,0x0000CFFF,WAVECAPS_VOLUME,(float)0xCFFF/0xFFFF*100.0)
EXTENTRY    ("Change Left Volume to 75%",    Volume,0x0000BFFF,WAVECAPS_VOLUME,(float)0xBFFF/0xFFFF*100.0)
EXTENTRY    ("Change Left Volume to 69%",    Volume,0x0000AFFF,WAVECAPS_VOLUME,(float)0xAFFF/0xFFFF*100.0)
EXTENTRY    ("Change Left Volume to 62%",    Volume,0x00009FFF,WAVECAPS_VOLUME,(float)0x9FFF/0xFFFF*100.0)
EXTENTRY    ("Change Left Volume to 56%",    Volume,0x00008FFF,WAVECAPS_VOLUME,(float)0x8FFF/0xFFFF*100.0)
EXTENTRY    ("Change Left Volume to 50%",    Volume,0x00007FFF,WAVECAPS_VOLUME,(float)0x7FFF/0xFFFF*100.0)
EXTENTRY    ("Change Left Volume to 44%",    Volume,0x00006FFF,WAVECAPS_VOLUME,(float)0x6FFF/0xFFFF*100.0)
EXTENTRY    ("Change Left Volume to 37%",    Volume,0x00005FFF,WAVECAPS_VOLUME,(float)0x5FFF/0xFFFF*100.0)
EXTENTRY    ("Change Left Volume to 31%",    Volume,0x00004FFF,WAVECAPS_VOLUME,(float)0x4FFF/0xFFFF*100.0)
EXTENTRY    ("Change Left Volume to 25%",    Volume,0x00003FFF,WAVECAPS_VOLUME,(float)0x3FFF/0xFFFF*100.0)
EXTENTRY    ("Change Left Volume to 19%",    Volume,0x00002FFF,WAVECAPS_VOLUME,(float)0x2FFF/0xFFFF*100.0)
EXTENTRY    ("Change Left Volume to 12%",    Volume,0x00001FFF,WAVECAPS_VOLUME,(float)0x1FFF/0xFFFF*100.0)
EXTSTOPENTRY    ("Change Left Volume to 6%",    Volume,0x00000FFF,WAVECAPS_VOLUME,(float)0x0FFF/0xFFFF*100.0)
EXTSTOPENTRY    ("Change Volume:  Normal",    Volume,0xFFFFFFFF,WAVECAPS_VOLUME,(float)0xFFFF/0xFFFF*100.0)
#else
EXTENTRY    ("Change Right Volume to 100%",    Volume,0xFFFF0000,WAVECAPS_VOLUME,(float)0xFFFF/0xFFFF*100.0)
EXTENTRY    ("Change Right Volume to 87%",    Volume,0xDFFF0000,WAVECAPS_VOLUME,(float)0xDFFF/0xFFFF*100.0)
EXTENTRY    ("Change Right Volume to 75%",    Volume,0xBFFF0000,WAVECAPS_VOLUME,(float)0xBFFF/0xFFFF*100.0)
EXTENTRY    ("Change Right Volume to 62%",    Volume,0x9FFF0000,WAVECAPS_VOLUME,(float)0x9FFF/0xFFFF*100.0)
EXTENTRY    ("Change Right Volume to 50%",    Volume,0x7FFF0000,WAVECAPS_VOLUME,(float)0x7FFF/0xFFFF*100.0)
EXTENTRY    ("Change Right Volume to 37%",    Volume,0x5FFF0000,WAVECAPS_VOLUME,(float)0x5FFF/0xFFFF*100.0)
EXTENTRY    ("Change Right Volume to 25%",    Volume,0x3FFF0000,WAVECAPS_VOLUME,(float)0x3FFF/0xFFFF*100.0)
EXTSTOPENTRY    ("Change Right Volume to 12%",    Volume,0x1FFF0000,WAVECAPS_VOLUME,(float)0x1FFF/0xFFFF*100.0)
EXTENTRY    ("Change Left Volume to 100%",    Volume,0x0000FFFF,WAVECAPS_VOLUME,(float)0xFFFF/0xFFFF*100.0)
EXTENTRY    ("Change Left Volume to 87%",    Volume,0x0000DFFF,WAVECAPS_VOLUME,(float)0xDFFF/0xFFFF*100.0)
EXTENTRY    ("Change Left Volume to 75%",    Volume,0x0000BFFF,WAVECAPS_VOLUME,(float)0xBFFF/0xFFFF*100.0)
EXTENTRY    ("Change Left Volume to 62%",    Volume,0x00009FFF,WAVECAPS_VOLUME,(float)0x9FFF/0xFFFF*100.0)
EXTENTRY    ("Change Left Volume to 50%",    Volume,0x00007FFF,WAVECAPS_VOLUME,(float)0x7FFF/0xFFFF*100.0)
EXTENTRY    ("Change Left Volume to 37%",    Volume,0x00005FFF,WAVECAPS_VOLUME,(float)0x5FFF/0xFFFF*100.0)
EXTENTRY    ("Change Left Volume to 25%",    Volume,0x00003FFF,WAVECAPS_VOLUME,(float)0x3FFF/0xFFFF*100.0)
EXTSTOPENTRY    ("Change Left Volume to 12%",    Volume,0x00001FFF,WAVECAPS_VOLUME,(float)0x1FFF/0xFFFF*100.0)
EXTSTOPENTRY    ("Change Volume:  Normal",    Volume,0xFFFFFFFF,WAVECAPS_VOLUME,(float)0xFFFF/0xFFFF*100.0)
#endif
ENDMAP



TCHAR *extensionMessages[]=
{
    TEXT("Did you hear the pitch change from lower to higher? (cancel to retry)"),
    TEXT("Did you hear the playback rate change from slower to faster? (cancel to retry)"),
    TEXT("Did you hear the right volume change from higher to lower? (cancel to retry)"),
    TEXT("Did you hear the left volume change from higher to lower? (cancel to retry)"),
    TEXT("Did you hear the volume return to its original level? (cancel to retry)"),
};




struct PLAYBACKMIXINGTHREADPARM
{
     double dFrequency;
     int    iWaveFormID;
     UINT   uMsg;
};




struct PLAYBACKMIXING_sndPlaySound_THREADPARM
{
     UINT fuSound;
};




/*
 * Function Name: nBlockAlignBuffer
 *
 * Purpose: Insures that BufferLength is aligned, by adding the minimum amount
 *          necessary to make it a multiple of Alignment.
 */
void BlockAlignBuffer( DWORD* lpBufferLength, DWORD dwAlignment )
{
     if( 0 == dwAlignment )
          return;

     DWORD dwBufferLengthRemainder = *lpBufferLength % dwAlignment;
     *lpBufferLength += dwAlignment - dwBufferLengthRemainder;
}



/*
 * Function Name: GetReturnCode
 *
 * Purpose: Helper Function to reduce the amount of redundent code in the tests
 */
TESTPROCAPI GetReturnCode(DWORD current, DWORD next)
{
     if(current == TPR_PASS)
     {
          if(next == TPR_FAIL || next == TPR_ABORT || next == TPR_SKIP)
               return next;
          else
               return current;
     }
     else if(current == TPR_FAIL)
     {
          return TPR_FAIL;
     }
     else if(current == TPR_ABORT)
     {
          if(next == TPR_FAIL)
               return TPR_FAIL;
          else
               return TPR_ABORT;
     }
     else // current == TPR_SKIP
     {
          if(next == TPR_FAIL || next == TPR_ABORT)
               return next;
          else
               return current;
     }
}






DWORD GetVolumeControl( UINT uDeviceID )
{
     enum VOLUME_CONTROL eVolumeControl = VOLUME_ERROR;
     MMRESULT    mmRtn                   = MMSYSERR_NOERROR;
     WAVEOUTCAPS woc;

     //---- Get device WAVECAPS_VOLUME & WAVECAPS_LRVOLUME flags
     mmRtn = waveOutGetDevCaps( uDeviceID, &woc, sizeof( woc ) );
     if( MMSYSERR_NOERROR == mmRtn )
     {
          if( woc.dwSupport & WAVECAPS_VOLUME )
          {
               if( woc.dwSupport & WAVECAPS_LRVOLUME )
               {
                    eVolumeControl = SEPARATE;
                    LOG( TEXT( "Device %d supports separate left and right volume control." ), uDeviceID );
               }
               else
               {
                    eVolumeControl = JOINT;
                    LOG( TEXT( "Device %d supports joint left and right volume control." ), uDeviceID );
               }
          }
          else
          {
               eVolumeControl = NONE;
               LOG( TEXT( "Device %d doesn't support volume changes." ), uDeviceID );
          }
     }
     else
     {
          LOG( TEXT( "ERROR in %s @ line %u:" ), TEXT( __FILE__ ), __LINE__ );
          LOG( TEXT( "\tError %d detected by waveOutGetDevCaps." ), mmRtn );
     }
     return eVolumeControl;
}






//---- Display warning, if resolution is 1 ms or greater.
void ValidateResolution( timer_type ttResolution )
{
     if( ttResolution < TT_RESOLUTION_THRESHOLD )
     {
          LOG( TEXT( "WARNING:\tResolution = %I64i ticks/sec." ), ttResolution );
          LOG( TEXT( "\tIf timer is only accurate to about 1ms, then you may encounter \
                         timing errors due to this coarse timer resolution.") );
     }
}






//---- Headless Audio CETK Helper Functions

//---- Global Data
HINSTANCE hInst; // program instance handle




//---- Helper function: since we need to run tests on headless devices,
//----  we must use LoadLibrary() to make calls into coredll.dll
HMODULE GetCoreDLLHandle()
{
     HMODULE hCoreDLL = LoadLibrary(TEXT("coredll.dll"));
     if(hCoreDLL == NULL)
     {
          LOG(TEXT("ERROR: LoadLibrary Failed to load coredll.dll"));
          return NULL;
     }
     return hCoreDLL;
}





BOOL FreeCoreDLLHandle(HMODULE hCoreDLL)
{
     if( 0 == FreeLibrary(hCoreDLL))
     {
          int le = GetLastError();
          LOG(TEXT("ERROR: FreeLibrary Failed. Last Error was: %d\n"), le);
          return FALSE;
     }
     return TRUE;
}






namespace HeadlessInput
{

//---- The functions below are helper functions for running in Headless interactive mode
//
//---- NOTE: If you are  having trouble with your audio driver on a headless device,
//---- AND you think your keyboard/interactive user input is not responding on your system,
//---- you may uncomment out the LOG messages in the functions below below so that you
//---- can further diagnose how far you get in the interactive message processing


//---- Global Char to be used as user input for Headless Interactive user input scenarios
TCHAR g_tcharUserInput;



#define dim(x)      (sizeof(x) / sizeof(x[0]))



//---- structure associates messages with a function
struct decodeUINT
{
     UINT code;
     LRESULT (*Fxn)(HWND, UINT, WPARAM, LPARAM);
};



//---- Structure associates menu IDs with a function
struct decodeCMD
{
     UINT code;
     LRESULT(*Fxn)(HWND, WORD, HWND, WORD);
};



//---- Process WM_CREATE Msg for window
LRESULT DoCreateMain(HWND hWnd, UINT wMsg, WPARAM wParam, LPARAM lParam)
{
     //---- Received a WM_CREATE Message
     //LOG(TEXT("GOT WM_CREATE MSG"));
     return 0;
}




//---- Process WM_ACTIVATE message for window
LRESULT DoActivateMain(HWND hWnd, UINT wMsg, WPARAM wParam, LPARAM lParam)
{
     //---- Received a WM_ACTIVATE Message
     //LOG(TEXT("GOT WM_ACTIVATE MESSAGE"));
     return 0;
}




LRESULT DoDestroyMain(HWND hWnd, UINT wMsg, WPARAM wParam, LPARAM lParam)
{
     //---- Received a WM_DESTROYWINDOW Message
     //LOG(TEXT("GOT WM_DESTROYWINDOW MESSAGE"));
     PostQuitMessage(0);
     return 0;
}




LRESULT DoProcessChar(HWND hWnd, UINT wMsg, WPARAM wParam, LPARAM lParam)
{
     TCHAR ch = (TCHAR)wParam;

     if(  ch == (TCHAR)'y' || ch == (TCHAR)'Y' ||
          ch == (TCHAR)'n' || ch == (TCHAR)'N' ||
          ch == (TCHAR)'r' || ch == (TCHAR)'R' )
     {

          //---- record user input for logging purposes
          LOG(TEXT("User Response was: %c"), ch);

          //---- set the global input char to what we just got from the user
          g_tcharUserInput = ch;

          //---- We are done getting user input
          PostQuitMessage(0);
     }
     else
     {
          LOG(TEXT("NOTICE: User entered invalid input. Please enter 'Y', 'N' or 'R'"));
     }
     return 0;
}





//---- Process a WM_KEYDOWN message
LRESULT DoProcessKeyDown(HWND hWnd, UINT wMsg, WPARAM wParam, LPARAM lParam)
{
     //---- wParam value contains the UNICODE char represented by the key
    return 0;
}




//---- Message dispatch table for MainWindowProc
const struct decodeUINT MainMessages[] =
{
     WM_CREATE, DoCreateMain,
     WM_ACTIVATE, DoActivateMain,
     WM_DESTROY, DoDestroyMain,
     WM_CHAR, DoProcessChar,
     WM_KEYDOWN, DoProcessKeyDown,
};




//---- HeadlessWndProc - Callback function for application window
LRESULT CALLBACK HeadlessWndProc(HWND hWnd, UINT wMsg, WPARAM wParam, LPARAM lParam)
{
     INT i;

     //---- search the message list to see if we need to handle this message.
     //---- If in list, call procedure
     for(i = 0; i < dim(MainMessages); i++)
     {
         if(wMsg == MainMessages[i].code)
             return (*MainMessages[i].Fxn)(hWnd, wMsg, wParam, lParam);
     }

     return DefWindowProc(hWnd, wMsg, wParam, lParam);
}





//---- Thread procedure for interactive headless user input.
DWORD WINAPI SimpleHeadlessWindowMessageThreadProc(LPVOID lpParameter)
{
     //---- Initialize the application
     const TCHAR szAppName[] = TEXT("HeadlessApp");
     WNDCLASS wc;
     HINSTANCE hInstance = NULL;
     TCHAR tchUInput = NULL;

     if( NULL == lpParameter )
     {
          LOG(TEXT("ERROR: lpParameter was NULL."));
          return TPR_ABORT;
     }

     //---- Register Application main window class
     wc.style = 0;                      // window style
     wc.lpfnWndProc = HeadlessWndProc;  // Callback function
     wc.cbClsExtra = 0;                 // Extra class data
     wc.cbWndExtra = 0;                 // Extra window data
     wc.hInstance = hInstance;          // Owner Handle
     wc.hIcon = NULL;                   // Application Icon
     wc.hCursor = NULL;                 // Default cursor
     wc.hbrBackground = NULL;           // Background
     wc.lpszMenuName = NULL;            // Menu Name
     wc.lpszClassName = szAppName;      // Window Class Name

     if( RegisterClass(&wc) == 0 )
     {
          DWORD le = GetLastError();
          if(ERROR_CLASS_ALREADY_EXISTS == le)
          {
               LOG(TEXT("ERROR: RegisterClassFailed: ERROR_CLASS_ALREADY_EXISTS"), le);
          }
          else
          {
               LOG(TEXT("ERROR: RegisterClassFailed %d"), le);
               LOG(TEXT("SUGGESTION: Make sure you do not already have a window class \
                         named HeadlessApp registered on your system"));
          }

          return TPR_ABORT;
     }

     //---- Initialize this Instance
     HWND hWnd;

     //---- Save program instance handle in global variable
     hInst = hInstance;

     //---- Create Main Window
     hWnd = CreateWindow(     szAppName,     // window class
                             TEXT("Hello"), // window title
                             WS_VISIBLE,    // style flags
                             CW_USEDEFAULT, // x position
                             CW_USEDEFAULT, // y position
                             CW_USEDEFAULT, //Initial Width
                             CW_USEDEFAULT, // Initial Height
                             NULL,          // parent
                             NULL,          // Menu, must be null
                             hInstance,     // Application Instance
                             NULL);         // pointer to create parameters

     if(!IsWindow(hWnd))
     {
          //---- fail if not created
          LOG(TEXT("ERROR: CreateWindow Failed."));
          return TPR_ABORT;
     }
     //----else the window was created


     //---- Standard Show and update calls
     ShowWindow(hWnd, SW_SHOW);

     //---- Not supported API on our headless images
     // UpdateWindow(hWnd);

     if(hWnd == 0)
     {
          LOG(TEXT("ERROR: hWnd is NULL"));
          return TPR_ABORT;
     }

     //---- Set the focus
     if(0 == SetForegroundWindow(hWnd))
          LOG(TEXT("ERROR: Unable to set window as foreground window."));
     //----else Current Window is in Foreground

     MSG msg;

     //---- Application Message Loop
     while( GetMessage( &msg, NULL, 0, 0 ) )
     {
          TranslateMessage(&msg);
          DispatchMessage(&msg);
     }

     //---- "WM_QUIT message has been received. Start final clean up for termination of the thread

     //---- Take the user input from global var and pass it back through the pointer
     //---- that was passed into this function
     *(TCHAR *)lpParameter = g_tcharUserInput;

     //---- Destroy the window
     BOOL bRes = DestroyWindow(hWnd);
     if( 0 == bRes )
     {
          DWORD dLe = GetLastError();
          LOG(TEXT("ERROR: DestroyWindow failed with Last Error: %d"), dLe);
          return TPR_ABORT;
     }
     //----else Sucessfully destroyed the window

     //---- Unregister the class
     bRes = UnregisterClass( szAppName, NULL );     // second parameter is ignored
     if( 0 == bRes )
     {
          DWORD dLe = GetLastError();
          LOG(TEXT("ERROR: UnRegisterClass failed with Last Error: %d"), dLe);
          return TPR_ABORT;
     }
     return TPR_PASS;
}




/*
  * BOOL IsKeyboardPresent()
  * Helper function for headless Audio CETK that checks if a keyboard is present
  * and tries to ensure that it is enabled so that we can get interactive user input
  *
  * Return Values:
  * TRUE: Keyboard is present and enabled
  * FALSE: See Debug Spew For Reason(s) why this call failed
  */
BOOL IsKeyboardPresent()
{
     DWORD status = GetKeyboardStatus();
     LOG(TEXT("Checking for Keyboard Support:\n"));
     if( status & KBDI_KEYBOARD_PRESENT )
          LOG(TEXT("    KEYBOARD is PRESENT"));
     else
     {
          LOG(TEXT("ERROR: GetKeyboardStatus reports that a keyboard is not present on the system"));
          LOG(TEXT("SUGGESTION: Ensure that SYSGEN_MININPUT or greater functionality is part of your image. "));
          return FALSE;
     }
     if( status & KBDI_KEYBOARD_ENABLED )
          LOG(TEXT("    KEYBOARD is ENABLED"));
     else
     {
          LOG(TEXT("ERROR: GetKeyboardStatus reports that a keyboard is not enabled on the system"));
          LOG(TEXT("SUGGESTION: Ensure that SYSGEN_MININPUT or greater functionality is part of your image. "));

          LOG(TEXT("ERROR: Attempting to enable the keyboard:"));
          //---- Keyboard reports not being enabled, lets try to enable it here.
          if(TRUE == EnableHardwareKeyboard(TRUE))
               LOG(TEXT("ATTEMPTED TO ENABLE HARDWARE KEYBOARD"));
          LOG(TEXT("SUGGESTION: Just tried to enable the keyboard. This function returns TRUE no matter what, \
                    thus we do not know if it succeeded."));
          LOG(TEXT("SUGGESTION: Try re-running this test to see if it worked. If not, it is possible \
                    an application hung after disabling the keyboard. You may need to reboot."));
          return FALSE;
     }
     if( status & KBDI_KEYBOARD_ENTER_ESC )
          LOG(TEXT("    KEYBOARD has ENTER and ESC keys."));
     else
     {
          LOG(TEXT("ERROR: GetKeyboardStatus reports that the keyboard does not have ENTER and ESC keys."));
          LOG(TEXT("SUGGESTION: Ensure that your keyboard physically has an ENTER and ESC key."));
          LOG(TEXT("SUGGESTION: Ensure that your keyboard driver properly handles ENTER and ESC \
                    and reports that those keys are present."));
          return FALSE;
     }
     if( status & KBDI_KEYBOARD_ALPHA_NUM )
          LOG(TEXT("    KEYBOARD is ALPHANUMERIC"));
     else
     {
          LOG(TEXT("ERROR: GetKeyboardStatus reports that the keyboard is not ALPHANUMERIC"));
          LOG(TEXT("SUGGESTION: Ensure that you are using an ALPHANUMERIC keyboard."));
          return FALSE;
     }
     return TRUE;
}




/*
  *    TerminateChildThread(DWORD dwThreadID, DWORD dwExitCode)
  *   Purpose: Function that will terminate a specified thread. Consolidated into a single
  *   function to avoid a lot of repeated code.
  *
  *  dwThreadID = ThreadID of thread you want to terminate
  *  dwExitCode = Exit code you want for the thread that is about to be terminated

*/
TESTPROCAPI TerminateChildThread(DWORD dwThreadID, DWORD dwExitCode)
{
     DWORD dwRet = TPR_PASS;

     BOOL bRetVal = PostThreadMessage(dwThreadID, WM_QUIT, dwExitCode, 0);
     if( 0 == bRetVal )
     {
          DWORD le = GetLastError();
          if( ERROR_INVALID_THREAD_ID == le )
               LOG(TEXT("ERROR: TerminateChildThread failed. PostThreadMessage reported ERROR_INVALID_THREAD_ID."));
          else
               LOG(TEXT("ERROR: TerminateChildThread failed. PostThreadMessage reported a last error of %d"), le);
          dwRet = TPR_ABORT;
     }
     LOG(TEXT("NOTE: Child Thread was sucessfully terminated."));
     return dwRet;
}



}// end namespace HeadlessInput






/*
  *
  * Parameters:
  * TCHAR *szPrompt : Prompt to be displayed to user in the Debug Output
  * TCHAR *tchResult: character returned by the user
  *
  */
TESTPROCAPI GetHeadlessUserInput(TCHAR *szPrompt, TCHAR *tchResult)
{
     using namespace HeadlessInput;

     DWORD ret = TPR_PASS;
     HANDLE hThread = NULL;
     DWORD dwThreadID;
     DWORD le; // for last errors
     TCHAR tchUserInput = (TCHAR)'Z';

     if( NULL == tchResult )
     {
          LOG(TEXT("ERROR: Parameter 'tchResult' passed into GetHeadlessUserInput cannot be NULL"));
          return TPR_FAIL;
     }

     if( FALSE == IsKeyboardPresent() )
     {
          LOG(TEXT("ERROR: No Keyboard is present on the system. SKIPPING test. "));
          LOG(TEXT("SUGGESTION: Keyboard needed for Headless Interactive Mode."));
          LOG(TEXT("SUGGESTION: If you cannot add a keyboard to your device, run CETK tests in non-interactive mode. (default)"));
          LOG(TEXT("SUGGESTION: Try adding SYSGEN_MININPUT to your image for minimal keyboard support in a headless image."));
          return TPR_SKIP;
     }


     //---- Launch Thread/Window to get user input on a headless device
     hThread = CreateThread(  NULL,
                              0,
                              SimpleHeadlessWindowMessageThreadProc,
                              (PVOID)&tchUserInput,
                              0,
                              &dwThreadID);
     if( hThread==NULL )
     {
          LOG(TEXT("ERROR:  Unable to CreateThread"));
          ret = TPR_ABORT;
          goto ThreadError;
     }

     //----Remove me
     Sleep(50);  // Give new thread a breif moment to get up and running
     //---- Prompt user for input
     LOG(szPrompt);


     //---- Wait on hThread to know when User has entered input and child thread is done processing it
     DWORD hr = WaitForSingleObject(hThread, 120000);
     switch(hr)
     {
          case WAIT_FAILED:
               le = GetLastError();
               LOG(TEXT("ERROR: WaitForSingleObject (Event) Failed with code %d"), le);

               ret = TPR_ABORT; // Abort this test since the WaitForSingleObject failed

               //---- Terminate Child thread
               ret = GetReturnCode(ret, TerminateChildThread(dwThreadID, 0));
               goto ThreadError;
               break;

          case WAIT_TIMEOUT:
               LOG(TEXT("ERROR: The time-out interval (2 minutes) elapsed, and the object's state is nonsignaled."));
               LOG(TEXT("SUGGESTION: Please respond to interactive prompts in under 2 minutes."));

               ret = TPR_ABORT;

               //---- Terminate Child thread
               ret = GetReturnCode(ret, TerminateChildThread(dwThreadID, 0));
               goto ThreadError;
               break;

          case WAIT_OBJECT_0:
               //---- thread was signalled, thus we should have the user input which will be validated below
               //----  since we got here the child thread should have been successfully terminated as well.
               break;

          default:
               LOG(TEXT("ERROR: Unkown error waiting for notifcation of Headless user input completion"));

               ret = TPR_ABORT;

               //---- Terminate Child thread
               ret = GetReturnCode(ret, TerminateChildThread(dwThreadID, 0));
               goto ThreadError;
               break;
     }

     //---- Validate input
     if(  tchUserInput == (TCHAR)'Y' || tchUserInput == (TCHAR)'y' ||
          tchUserInput == (TCHAR)'N' || tchUserInput == (TCHAR)'n' ||
          tchUserInput == (TCHAR)'R' || tchUserInput == (TCHAR)'r')
     {
          //---- store input in param user passed in
          *tchResult = tchUserInput;
     }
     else
     {
          //---- input from user is not valid, and for some reason previous error checking did not handle it
          RETAILMSG(1, (TEXT("ERROR: User input Window returned illegal user input.")));
          ret = TPR_ABORT;
          goto ThreadError;
     }

ThreadError:
     //---- Close the handle to Window/Input handling thread
     if( FALSE == CloseHandle(hThread) )
     {
          LOG(TEXT("ERROR: Failed to close handle to child thread"));
          ret = GetReturnCode(ret, TPR_ABORT);
     }
     return ret;
}



//---- End of Headless Audio CETK Helper Functions




HRESULT ListActiveDrivers()
{
     HRESULT hr;
     HKEY hActive, hDriver;
     DWORD dwType, dwSize, i;
     TCHAR szKeyName[MAX_PATH], szDriverName[MAX_PATH], szDriverKey[MAX_PATH];

     hr = RegOpenKeyEx( HKEY_LOCAL_MACHINE, REG_HKEY_ACTIVE, 0, 0, &hActive );
     CheckRegHRESULT(hr, RegOpenKeyEx, TEXT("[Active Driver Key]"),"Unknown");
     LOG(TEXT("Active Drivers List\n"));

     i = 0;
     do
     {
          dwSize = MAX_PATH;
          hr = RegEnumKeyEx( hActive, i, szKeyName, &dwSize, NULL, NULL, NULL, NULL );
          if( hr == ERROR_NO_MORE_ITEMS )
               break;
          CheckRegHRESULT(hr, RegEnumKeyEx, TEXT("[Active Driver Key]"),"Unknown");
          hr = RegOpenKeyEx( hActive, szKeyName, 0, 0, &hDriver );
          CheckRegHRESULT(hr, RegOpenKeyEx, szKeyName, "Unknown");
          dwSize = sizeof( szDriverName );
          hr = RegQueryValueEx( hDriver, TEXT("Name"), NULL, &dwType, (BYTE*)szDriverName, &dwSize );
          if( hr != ERROR_SUCCESS )
              StringCchCopy(szDriverName, MAX_PATH, TEXT("UNKN:"));
          dwSize = sizeof( szDriverKey );
          hr = RegQueryValueEx( hDriver, TEXT("Key"), NULL, &dwType, (BYTE*)szDriverKey, &dwSize );
          if( hr != ERROR_SUCCESS )
            StringCchCopy(szDriverKey, MAX_PATH, TEXT("<unknown path>"));
          LOG(TEXT("%s %s\n"),szDriverName,szDriverKey);
          hr = RegCloseKey( hDriver );
          QueryRegHRESULT( hr, RegCloseKey, szKeyName, "Unknown" );
          i++;
     } while( 1 );

     hr = RegCloseKey( hActive );
     QueryRegHRESULT( hr, RegCloseKey, TEXT("[Active Driver Key]"), "Unknown" );
     return HRESULT_FROM_WIN32(ERROR_SUCCESS);
}





HRESULT GetActiveDriverKey( const TCHAR *szDriver, TCHAR *szKey, DWORD dwSize)
{
     HRESULT hr;
     HKEY hActive, hDriver;
     DWORD dwType, dwSizeKeyName, i;
     TCHAR szKeyName[MAX_PATH], szDriverName[MAX_PATH];

     hr = RegOpenKeyEx( HKEY_LOCAL_MACHINE, REG_HKEY_ACTIVE, 0, 0, &hActive );
     CheckRegHRESULT( hr, RegOpenKeyEx, TEXT("[Active Driver Key]"), "Unknown" );

     i = 0;
     while( 1 )
     {
          dwSizeKeyName = MAX_PATH;
          hr = RegEnumKeyEx( hActive, i, szKeyName, &dwSizeKeyName, NULL, NULL, NULL, NULL );
          if( hr == ERROR_NO_MORE_ITEMS )
               break;
          CheckRegHRESULT( hr, RegEnumKeyEx, TEXT("[Active Driver Key]"), "Unknown" );
          hr = RegOpenKeyEx( hActive, szKeyName, 0, 0, &hDriver );
          CheckRegHRESULT( hr, RegOpenKeyEx, szKeyName, "Unknown" );
          dwSizeKeyName = sizeof( szDriverName );
          hr = RegQueryValueEx( hDriver, TEXT("Name"), NULL, &dwType, (BYTE*)szDriverName, &dwSizeKeyName );
          if( hr == ERROR_SUCCESS )
          {
               if( _tcscmp( szDriver, szDriverName ) == 0 )
               {
                    StringCchCopy(szKey, MAX_PATH, TEXT("<unknown path>"));
                    RegQueryValueEx( hDriver, TEXT("Key"), NULL, &dwType, (BYTE*)szKey, &dwSize );
                    hr = RegCloseKey(hDriver);
                    QueryRegHRESULT( hr, RegCloseKey, szKeyName, "Unknown" );
                    hr = RegCloseKey( hActive );
                    QueryRegHRESULT( hr, RegCloseKey, "[Active Driver Key]", "Unknown" );
                    return HRESULT_FROM_WIN32(ERROR_SUCCESS);
               }
          }
          hr = RegCloseKey( hDriver );
          QueryRegHRESULT( hr, RegCloseKey, szKeyName, "Unknown" );
          i++;
     }

     hr = RegCloseKey( hActive );
     QueryRegHRESULT( hr, RegCloseKey, TEXT("[Active Driver Key]"), "Unknown" );
     return HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND);
}


void ListWaveDevices(void)
{
    UINT nInDevs, nOutDevs;
    nInDevs = waveInGetNumDevs();
    nOutDevs = waveOutGetNumDevs();

    MMRESULT mmRet;
    WAVEINCAPS wic;
    LOG(TEXT("WaveIn Devices:"));
    for(UINT i = 0; i < nInDevs; i++)
    {
        mmRet = waveInGetDevCaps(i, &wic, sizeof(wic));
        LOG(TEXT(" DeviceID: %d DeviceName: %s"), i, wic.szPname);
    }

    WAVEOUTCAPS woc;
    LOG(TEXT("WaveOut Devices:"));
    for(i = 0; i < nOutDevs; i++)
    {
        mmRet = waveOutGetDevCaps(i, &woc, sizeof(woc));
        LOG(TEXT(" DeviceID: %d DeviceName: %s"), i, woc.szPname);
    }
}


void LogWaveOutDevCaps(UINT uDeviceID, WAVEOUTCAPS *pWOC)
{
     DWORD i;

     LOG(TEXT("Capabilities for Waveform Audio Output Device:  %u\n"), uDeviceID);
     LOG(TEXT("    Product:  %s\n") , pWOC->szPname);
     LOG(TEXT("    Manufacture ID:  %u\n") , pWOC->wMid);
     LOG(TEXT("    Product ID:  %u\n") , pWOC->wPid);
     LOG(TEXT("    Driver Version:  %u") , pWOC->vDriverVersion);
     LOG(TEXT("    Channels:  %u\n") , pWOC->wChannels);
     LOG(TEXT("    Audio lpFormats:"));

     for( i = 0; lpFormats[i].szName; i++ )
          LOG(TEXT("        %s %s") , lpFormats[i].szName , (pWOC->dwFormats&lpFormats[i].value ?
                                                                 TEXT(" Supported") : TEXT("*****Not Supported*****")) );

     LOG(TEXT("    Extended functionality supported:"));

     if( pWOC->dwSupport & WAVECAPS_LRVOLUME )
          LOG(TEXT("        Supports separate left and right volume control"));

     if( pWOC->dwSupport & WAVECAPS_PITCH )
          LOG(TEXT("        Supports pitch control"));

     if( pWOC->dwSupport & WAVECAPS_PLAYBACKRATE )
          LOG(TEXT("        Supports playback rate control"));

     if( pWOC->dwSupport & WAVECAPS_VOLUME )
          LOG(TEXT("        Supports volume control"));

     if( pWOC->dwSupport & WAVECAPS_SAMPLEACCURATE )
          LOG(TEXT("        Returns sample-accurate position information"));
}





// Sine Lookup Table

#define NUM_SINE_SAMPLES 100
short sineLUT[NUM_SINE_SAMPLES];
#define MULTIPLIER 0x7fff

// create a lookup table
// lookup table [0 - 2*pi]
void initSineLUT()
{
    const double TWOPI = 2* 3.1415926535897931;
    for (int i = 0; i < NUM_SINE_SAMPLES; i++)
    {
        sineLUT[i] = (short)(MULTIPLIER * sin((double)i * TWOPI / (double)NUM_SINE_SAMPLES));
    }
}

// sineF = sin(2*pi*t*f) where t - time; f - frequency
// bytes == 1, sineF ranges 0 - 255
// bytes == 2, sineF ranges -32767 - +32767
short sineF(int time, int samplerate, int frequency, int bytes)
{
    static bool bInitSineLUT = false;
    if (!bInitSineLUT)
    {
        initSineLUT();
        bInitSineLUT = true;
    }

    int t = ((time * frequency * NUM_SINE_SAMPLES) / samplerate) % NUM_SINE_SAMPLES;

    short sineValue = sineLUT[t];
    if (1 == bytes)
    {
        sineValue += MULTIPLIER;
        sineValue = sineValue >> 8;
    }

    return sineValue;
}

// End - Sine Lookup Table


ULONG SineWaveLUT( void* pBuffer, ULONG ulNumBytes, WAVEFORMATEX *pwfx, int dFrequency )
{
     char *pClear, *pClearEnd;

     pClearEnd = (char*)pBuffer + ulNumBytes;
     pClear = pClearEnd - ulNumBytes % 4;
     while( pClear<pClearEnd )
     {
          *pClear = 0;
          pClear++;
     }

     int nsamples = ulNumBytes / pwfx->nBlockAlign;
     int i;
     int m_dSampleRate = pwfx->nSamplesPerSec;

     if( pwfx->wBitsPerSample == 8 )
     {
          unsigned char * pSamples = (unsigned char *) pBuffer;
          if( pwfx->nChannels == 1 )
          {
               for (i = 0; i < nsamples; i ++ )
               {
                    pSamples[i] = (unsigned char) sineF(i, m_dSampleRate, dFrequency, 1);
               }
          }
          else
          {
               for (i = 0; i < nsamples; i ++)
               {
                    pSamples[2*i] = (unsigned char) sineF(i, m_dSampleRate, dFrequency, 1);
                    pSamples[2*i+1] = pSamples[2*i]; // replicate across both channels
               }
          }
     }
     else
     {
          short * pSamples = (short *) pBuffer;
          if( pwfx->nChannels == 1 )
          {
               for (i = 0; i < nsamples; i += 1)
               {
                    pSamples[i] = (short) sineF(i, m_dSampleRate, dFrequency, 2);
               }
          }
          else
          {
               for (i = 0; i < nsamples; i ++)
               {
                    pSamples[2*i] = (short) sineF(i, m_dSampleRate, dFrequency, 2);
                    pSamples[2*i+1] = pSamples[2*i]; // replicate across both channels
               }
          }
     }

     return ulNumBytes;
}

ULONG SineWave( void* pBuffer, ULONG ulNumBytes, WAVEFORMATEX *pwfx, double dFrequency )
{
     return SineWaveLUT(pBuffer, ulNumBytes, pwfx, (int)dFrequency);

#if 0  // implemented in SineWaveLUT
     double dPhase = 0.0;
     double dAmplitude = 1.0;
     char *pClear, *pClearEnd;
     const double TWOPI = 2* 3.1415926535897931;

     pClearEnd = (char*)pBuffer + ulNumBytes;
     pClear = pClearEnd - ulNumBytes % 4;
     while( pClear<pClearEnd )
     {
          *pClear = 0;
          pClear++;
     }

     int nsamples = ulNumBytes / pwfx->nBlockAlign;
     int i, m_t0 = 0;
     int m_dSampleRate = pwfx->nSamplesPerSec;
     double dMultiplier, dOffset, dSample;

     if( pwfx->wBitsPerSample == 8 )
     {
          unsigned char * pSamples = (unsigned char *) pBuffer;
          dMultiplier = 127.0;
          dOffset = 128.0;
          if( pwfx->nChannels == 1 )
          {
               for (i = 0; i < nsamples; i ++ )
               {
                    double t = ((double) (i+m_t0)) / m_dSampleRate;
                    dSample = dAmplitude * sin( t*dFrequency* TWOPI + dPhase );
                    pSamples[i] = (unsigned char) (dSample * dMultiplier + dOffset);
               }
          }
          else
          {
               for (i = 0; i < nsamples; i ++)
               {
                    double t = ((double) (i+m_t0)) / m_dSampleRate;
                    dSample = dAmplitude * sin (t*dFrequency* TWOPI + dPhase);
                    pSamples[2*i] = (unsigned char) (dSample * dMultiplier + dOffset);
                    pSamples[2*i+1] = pSamples[2*i]; // replicate across both channels
               }
          }
     }
     else
     {
          short * pSamples = (short *) pBuffer;
          const double dMultiplier2 = 32767.0;
          const double dOffset2 = 0.0;
          if( pwfx->nChannels == 1 )
          {
               for (i = 0; i < nsamples; i += 1)
               {
                    double t = ((double) (i+m_t0)) / m_dSampleRate;
                    dSample = dAmplitude * sin (t*dFrequency* TWOPI + dPhase);
                    pSamples[i] = (short) (dSample * dMultiplier2 + dOffset2);
               }
          }
          else
          {
               for (i = 0; i < nsamples; i ++)
               {
                    double t = ((double) (i+m_t0)) / m_dSampleRate;
                    dSample = dAmplitude * sin( t*dFrequency* TWOPI + dPhase );
                    pSamples[2*i] = (short) (dSample * dMultiplier2 + dOffset2);
                    pSamples[2*i+1] = pSamples[2*i]; // replicate across both channels
               }
          }
     }
     m_t0 += i;
     return ulNumBytes;
#endif
}




HANDLE hEvent = NULL;
DWORD state = WOM_CLOSE;
HWND hwnd = NULL;





inline BOOL WaitAbandoned( DWORD dwWaitResult, DWORD dwHandleCount )
{
     //---- Returns TRUE if one of the objects was a mutex that was abandoned; FALSE otherwise.
     return BOOL(
          ( dwWaitResult >= WAIT_ABANDONED_0 ) &&
          ( dwWaitResult <= WAIT_ABANDONED_0 + dwHandleCount - 1 ) );
}





inline BOOL WaitSuceeded( DWORD dwWaitResult, DWORD dwHandleCount )
{
     //---- Returns TRUE if the wait Suceeded, FALSE otherwise.
     return BOOL(
          ( dwWaitResult >= WAIT_OBJECT_0 ) &&
          ( dwWaitResult <= WAIT_OBJECT_0 + dwHandleCount - 1 ) );
}





inline BOOL WaitTimedOut( DWORD dwWaitResult )
{
     //---- Returns TRUE if the operation timed out, FALSE otherwise.
     return BOOL( WAIT_TIMEOUT == dwWaitResult );
}





void CALLBACK waveOutProcCallback( HWAVEOUT hwo2, UINT uMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2 )
{
     switch(uMsg)
     {
          case WOM_CLOSE:
               SetEvent( hEvent );
               if( state == WOM_DONE )
                    state = WOM_CLOSE;
               else
                    LOG(TEXT("ERROR:  WOM_CLOSE received when not done"));
               break;

          case WOM_OPEN:
               SetEvent( hEvent );
               if( state == WOM_CLOSE )
                    state = WOM_OPEN;
               else
                    LOG(TEXT("ERROR:  WOM_OPEN received when not closed"));
               break;

          case WOM_DONE:
               SetEvent( hEvent );
               if( state == WOM_OPEN )
                    state = WOM_DONE;
               else
                    LOG(TEXT("ERROR:  WOM_DONE received when not opened"));
               break;

          default:
               LOG(TEXT("ERROR:  Unknown Message received (%u)"),uMsg);
     }
}





LRESULT CALLBACK windowProcCallback(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
     int retVal = 0;

     switch(uMsg)
     {
          case MM_WOM_CLOSE:
               waveOutProcCallback( (HWAVEOUT)wParam, WOM_CLOSE, (DWORD)hEvent, 0, 0 );
               return 0;

          case MM_WOM_OPEN:
               waveOutProcCallback( (HWAVEOUT)wParam, WOM_OPEN, (DWORD)hEvent, 0, 0 );
               return 0;

          case MM_WOM_DONE:
               waveOutProcCallback( (HWAVEOUT)wParam, WOM_DONE, (DWORD)hEvent, lParam, 0 );
               return 0;

          case WM_DESTROY:
               HMODULE hCoreDLL = GetCoreDLLHandle();
               if( NULL != hCoreDLL )
               {
                    PFNPOSTQUITMESSAGE pfnPostQuitMessage = (PFNPOSTQUITMESSAGE)GetProcAddress(hCoreDLL, TEXT("PostQuitMessage"));
                    if( NULL != pfnPostQuitMessage )
                         pfnPostQuitMessage (0);
                    else
                         LOG(TEXT("ERROR: GetProcAddress Failed for PostQuitMessage in Coredll.dll\n"));
                    FreeCoreDLLHandle(hCoreDLL);
               }
               else
                    LOG(TEXT("ERROR: Unable to get a handle to CoreDLL"));
               return 0;
     }


     HMODULE hCoreDLL = GetCoreDLLHandle();
     if(NULL != hCoreDLL)
     {
          PFNDEFWINDOWPROC pfnDefWindowProc = (PFNDEFWINDOWPROC)GetProcAddress(hCoreDLL, TEXT("DefWindowProcW"));
          if(NULL != pfnDefWindowProc)
               retVal = pfnDefWindowProc(hwnd,uMsg,wParam,lParam);
          else
               LOG(TEXT("ERROR: GetProcAddress Failed for DefWindowProc in Coredll.dll\n"));
          FreeCoreDLLHandle(hCoreDLL);
     }
     else
          LOG(TEXT("ERROR: Unable to get a handle to CoreDLL"));
     return retVal;
}






DWORD WINAPI WindowMessageThreadProc(LPVOID lpParameter)
{
     WNDCLASS wndclass;
     MSG msg;
     DWORD ret = TPR_PASS;

     if(lpParameter)
     {
          wndclass.style              =   CS_HREDRAW | CS_VREDRAW;
          wndclass.lpfnWndProc     =   windowProcCallback;
          wndclass.cbClsExtra         =   0;
          wndclass.cbWndExtra         =   0;
          wndclass.hInstance         =   g_hInst;
          wndclass.hIcon             =   NULL;
          wndclass.hCursor         =   NULL;
          wndclass.hbrBackground   =   NULL;
          wndclass.lpszMenuName    =   NULL;
          wndclass.lpszClassName   =   TEXT("WaveTest");

          HMODULE hCoreDLL = GetCoreDLLHandle();
          if(NULL != hCoreDLL)
          {
               PFNREGISTERCLASS pfnRegisterClass = (PFNREGISTERCLASS)GetProcAddress(hCoreDLL, TEXT("RegisterClassW"));
               if( NULL != pfnRegisterClass )
               {
                    if( 0 == pfnRegisterClass(&wndclass) )
                    {
                         int rt = GetLastError();
                         LOG(TEXT("ERROR: Register Class FAILED, GetLastError was %d"), rt );
                         FreeCoreDLLHandle(hCoreDLL);
                         return TPR_ABORT;
                    }
               }
               else
               {
                    LOG(TEXT("ERROR: Unable to get proc address for RegisterClass"));
                    FreeCoreDLLHandle(hCoreDLL);
                    return TPR_ABORT;
               }

               PFNCREATEWINDOWEX pfnCreateWindowEx = ( PFNCREATEWINDOWEX)GetProcAddress(hCoreDLL, TEXT("CreateWindowExW"));
               if(NULL != pfnCreateWindowEx)
               {
                    hwnd = pfnCreateWindowEx(     0,                  // extended style
                                                  TEXT("WaveTest"),
                                                  TEXT("WaveTest"),
                                                  WS_DISABLED,
                                                  CW_USEDEFAULT, CW_USEDEFAULT,
                                                  CW_USEDEFAULT, CW_USEDEFAULT,
                                                  NULL, NULL,
                                                  g_hInst,
                                                  &g_hInst );

                    if( hwnd == NULL )
                    {
                         LOG(TEXT("ERROR: CreateWindow Failed"));
                         FreeCoreDLLHandle(hCoreDLL);
                         ret = TPR_ABORT;
                         goto WndMsgThreadError;
                    }
               }
               else
               {
                    LOG(TEXT("ERROR: Unable to get proc address for CreateWindow"));
                    FreeCoreDLLHandle(hCoreDLL);
                    ret = TPR_ABORT;
                    goto WndMsgThreadError;
               }

               if( 0 == SetEvent(hEvent) )
               {
                    int iLastErr = GetLastError();
                    LOG(TEXT("ERROR: Unable to SetEvent. GetLastError was %d\n"), iLastErr);
                    return TPR_ABORT;
               }

               FreeCoreDLLHandle(hCoreDLL);
          }//----if(NULL != hCoreDLL)
     }//----if(lpParameter)

     HMODULE hCoreDll = GetCoreDLLHandle();
     if(NULL != hCoreDll)
     {
          PFNGETMESSAGE pfnGetMessage = (PFNGETMESSAGE)GetProcAddress(hCoreDll, TEXT("GetMessageW"));
          PFNTRANSLATEMESSAGE pfnTranslateMessage = (PFNTRANSLATEMESSAGE)GetProcAddress(hCoreDll, TEXT("TranslateMessage"));
          PFNDISPATCHMESSAGE pfnDispatchMessage = (PFNDISPATCHMESSAGE)GetProcAddress(hCoreDll, TEXT("DispatchMessageW"));
          if( (NULL != pfnGetMessage) && (NULL != pfnTranslateMessage) && (NULL != pfnDispatchMessage) )
          {
               while( pfnGetMessage( &msg, NULL, 0, 0) )
               {
                    if(NULL==msg.hwnd)
                    {
                         windowProcCallback(msg.hwnd,msg.message,msg.wParam,msg.lParam);
                    }
                    else
                    {
                         pfnTranslateMessage(&msg);
                         pfnDispatchMessage(&msg);
                    }
               } // end While
          }
          else
          {
               LOG(TEXT("ERROR: Message Processing Function(s) not found in coredll.dll"));
               if( NULL ==  pfnGetMessage )
                    LOG(TEXT("ERROR: GetProcAddress failed for GetMessage"));
               if( NULL ==  pfnTranslateMessage )
                    LOG(TEXT("ERROR: GetProcAddress failed for TranslateMessage"));
               if(NULL ==  pfnDispatchMessage)
                    LOG(TEXT("ERROR: GetProcAddress failed for DispatchMessage"));
               ret = TPR_ABORT;
          }
          FreeCoreDLLHandle(hCoreDll);
     }

     if(lpParameter)
     {
          WndMsgThreadError:
          HMODULE hCoreDLL = GetCoreDLLHandle();
          if( NULL != hCoreDLL )
          {
               PFNDESTROYWINDOW pfnDestroyWindow = (PFNDESTROYWINDOW)GetProcAddress(hCoreDLL, TEXT("DestroyWindow"));
               PFNUNREGISTERCLASS pfnUnregisterClass = (PFNUNREGISTERCLASS)GetProcAddress(hCoreDLL, TEXT("UnregisterClassW"));
               if((NULL != pfnDestroyWindow) && (NULL != pfnUnregisterClass))
               {
                    pfnDestroyWindow(hwnd);
                    pfnUnregisterClass(wndclass.lpszClassName,g_hInst);
               }
               else
               {
                    LOG(TEXT("ERROR: Destroy Window and UnRegisterClass Failed"));
                    ret = TPR_ABORT;
               }

               FreeCoreDLLHandle(hCoreDLL);
          }
          else
          {
               LOG(TEXT("ERROR: Unable to get proc address for DestroyWindow or UnRegisterClass in coredll.dll"));
               ret = TPR_ABORT;
          }
     }
     return ret;
}






TESTPROCAPI StringFormatToWaveFormatEx( WAVEFORMATEX *wfx, const TCHAR* szWaveFormat )
{
     int iRet;
     DWORD tr=TPR_PASS;
     TCHAR channels,bits1,bits2;

     ZeroMemory(wfx,sizeof(*wfx));
     iRet = _stscanf_s( szWaveFormat, TEXT("WAVE_FORMAT_%i%c%c%c"), &wfx->nSamplesPerSec, &channels,sizeof(TCHAR), &bits1,sizeof(TCHAR),&bits2,sizeof(TCHAR));
     if( iRet != 4 )
     {
          LOG( ( TEXT( "ERROR:  %s not recognized\n" ), (LPWSTR)szWaveFormat ) );
          LOG(TEXT("Possible Cause:  Supplied format not in the form of WAVE_FORMAT%%i%%c%%c%%c\n"));
          tr = TPR_FAIL;
          goto Error;
     }

     wfx->wFormatTag = WAVE_FORMAT_PCM;
     switch( channels )
     {
          case 'M':
          case 'm':
               wfx->nChannels=1;
               break;

          default:
               wfx->nChannels=2;
     }

     switch( wfx->nSamplesPerSec )
     {
          case 1:
          case 2:
          case 4:
               wfx->nSamplesPerSec=11025*wfx->nSamplesPerSec;
     }

     wfx->wBitsPerSample=(bits1-TEXT('0'))*10+(bits2-TEXT('0'));
     wfx->nBlockAlign=wfx->nChannels*wfx->wBitsPerSample/8;
     wfx->nAvgBytesPerSec=wfx->nSamplesPerSec*wfx->nBlockAlign;

Error:
     return tr;
}





TESTPROCAPI CommandLineToWaveFormatEx( WAVEFORMATEX *wfx )
{
     int iLatencyTestDuration;
     int iRet;
     int iSamplesPerSec;

     if(!wfx )
     {
          LOG( TEXT( "ERROR:  in %s @ line %u: wfx invalid."), TEXT( __FILE__ ), __LINE__ );
          return TPR_FAIL;
     }

     ZeroMemory( wfx, sizeof( *wfx ) );
     iRet = _stscanf_s (    g_pszWaveCharacteristics,
                         TEXT( "%u_%u_%u_%u" ),
                         &iLatencyTestDuration,
                         &iSamplesPerSec,
                         &(wfx->nChannels),
                         &(wfx->wBitsPerSample) );
     if( iRet !=4 )
     {
          LOG( TEXT( "ERROR:\tin %s @ line %u."), TEXT( __FILE__ ), __LINE__ );
          LOG( TEXT( "\t%s not recognized\n" ), g_pszWaveCharacteristics );
          LOG( TEXT( "Possible Cause:  Command line wave characteristic format not in the form: d_f_c_b\n" ) );
          return TPR_FAIL;
     }
     g_iLatencyTestDuration = g_iLatencyTestDuration ? g_iLatencyTestDuration : iLatencyTestDuration;
     wfx->nSamplesPerSec  = DWORD(iSamplesPerSec);
     wfx->wFormatTag      = WAVE_FORMAT_PCM;
     wfx->nBlockAlign     = wfx->nChannels * wfx->wBitsPerSample / 8;
     wfx->nAvgBytesPerSec = wfx->nSamplesPerSec * wfx->nBlockAlign;
     return TPR_PASS;
}





TESTPROCAPI PlayWaveFormat( const TCHAR *szWaveFormat, DWORD dwSeconds, DWORD
    dwNotification, extension* ext, DWORD *hrReturn, double dFrequency = 440.0)
{
     BOOL bWaveOutHeaderPrepared = FALSE;
     HRESULT hr;
     WAVEHDR wh;
     char *data = NULL;
     WAVEFORMATEX wfx;
     HWAVEOUT hwo = NULL;
     HANDLE hThread = NULL;
     DWORD tr = TPR_PASS, dwCallback = 0, dwStat;
     timer_type start_count, finish_count = 0, duration, expected, m_Resolution;
     DWORD dwExpectedPlaytime, dwTime = 0;

     state = WOM_CLOSE;
     QueryPerformanceFrequency( reinterpret_cast<LARGE_INTEGER*>(&m_Resolution) );
     ValidateResolution( m_Resolution );

     if( StringFormatToWaveFormatEx( &wfx,szWaveFormat ) != TPR_PASS )
     {
          tr|=TPR_FAIL;
          goto Error;
     }

     //---- This check added so that we don't overflow the DWORD and wind up with
     //----  a buffer that is smaller than we expected.
     if( dwSeconds < (ULONG_MAX / wfx.nAvgBytesPerSec) )
     {
          PREFAST_SUPPRESS(419, "The above line is checking for overflow. This seems to be prefast noise, \
                                since the path/trace Prefast lists is not possible. (Prefast team concurs so far.)");
          data = new char[dwSeconds * wfx.nAvgBytesPerSec];
     }
     else
     {
          LOG(TEXT("Error: Overflow in PlayWaveFormat."));
          tr|=TPR_FAIL;
          goto Error;
     }

     CheckCondition( !data, "ERROR:  New Failed for Data", TPR_ABORT, "Out of Memory" );
     ZeroMemory( data, dwSeconds * wfx.nAvgBytesPerSec );
     hEvent = CreateEvent( NULL, FALSE, FALSE, NULL );
     CheckCondition( hEvent == NULL, "ERROR:  Unable to CreateEvent", TPR_ABORT, "Refer to GetLastError for a Possible Cause" );
     switch( dwNotification )
     {
          case CALLBACK_NULL:
               dwCallback = NULL;
               break;

          case CALLBACK_EVENT:
               dwCallback = (DWORD)hEvent;
               break;

          case CALLBACK_FUNCTION:
               dwCallback = (DWORD)waveOutProcCallback;
               break;

          case CALLBACK_THREAD:
               hThread = CreateThread( NULL, 0, WindowMessageThreadProc, (void*)FALSE, 0, &dwCallback );
               CheckCondition( hThread==NULL, "ERROR:  Unable to CreateThread", TPR_ABORT, "Refer to GetLastError for a Possible Cause" );
               break;

          case CALLBACK_WINDOW:
               hThread = CreateThread( NULL, 0, WindowMessageThreadProc, (void*)TRUE, 0, NULL );
               CheckCondition( hThread == NULL, "ERROR:  Unable to CreateThread", TPR_ABORT, "Refer to GetLastError for a Possible Cause" );
               hr = WaitForSingleObject( hEvent, 10000 );
               CheckCondition( hr == WAIT_TIMEOUT, "ERROR:  Window didn't notify its creation within ten seconds", TPR_ABORT, "Unknown" );
               CheckCondition( hr != WAIT_OBJECT_0, "ERROR:  Unknown Error waiting for Window Creation \
                                                    to notify its creation", TPR_ABORT, "Unknown" );
               dwCallback = (DWORD)hwnd;
               break;
     }
     hr = waveOutOpen( &hwo, g_dwOutDeviceID, &wfx, dwCallback,(DWORD)hEvent, dwNotification );
     if(( hr != MMSYSERR_NOERROR) && ( hrReturn ) )
     {
          LOG(TEXT("ERROR: waveOutOpen failed."));
          *hrReturn = hr;
          tr |= TPR_FAIL;
          goto Error;
     }
     CheckMMRESULT( hr, "ERROR:  waveOutOpen failed to open driver", TPR_ABORT, "Driver doesn't really support this format" );
     switch( dwNotification )
     {
          case CALLBACK_FUNCTION:
          case CALLBACK_THREAD:
          case CALLBACK_WINDOW:
               hr = WaitForSingleObject( hEvent, 1000 );
               QueryCondition( hr == WAIT_TIMEOUT, "ERROR:  Function, Thread or Window didn't notify that it received the open \
                                                   message within one second", TPR_ABORT, "Open message wasn't sent to handler" );
               QueryCondition( hr != WAIT_OBJECT_0, "ERROR:  Unknown Error while waiting for driver to open", TPR_ABORT, "Unknown" );
               break;
     }
     ZeroMemory(&wh, sizeof(WAVEHDR));
     wh.lpData = data;
     wh.dwBufferLength = dwSeconds * wfx.nAvgBytesPerSec;
     wh.dwLoops = 1;
     wh.dwFlags = WHDR_BEGINLOOP | WHDR_ENDLOOP;
     dwExpectedPlaytime = wh.dwBufferLength * 1000 / wfx.nAvgBytesPerSec;

     if( ext )
     {
          hr = ext->get(hwo,&dwStat);
          if(( hr != MMSYSERR_NOERROR) && ( hrReturn ) )
          {
               if( MMSYSERR_NOTSUPPORTED == hr )
                    LOG( TEXT( "INFORMATION: Get %s reports MMSYSERR_NOTSUPPORTED." ), ext->szName );
               else
                    LOG( TEXT( "INFORMATION: Get %s returns %d." ), ext->szName, hr );
               *hrReturn = hr;
               tr |= TPR_FAIL;
               goto Error;
          }
          hr = ext->set( hwo, ext->value );
          if( (hr != MMSYSERR_NOERROR) && (hrReturn) )
          {
               if( MMSYSERR_NOTSUPPORTED == hr )
                    LOG( TEXT( "INFORMATION: Set %s reports MMSYSERR_NOTSUPPORTED." ), ext->szName );
               else
                    LOG( TEXT( "INFORMATION: Set %s returns %d." ), ext->szName, hr );
               *hrReturn=hr;
               tr|=TPR_FAIL;
               goto Error;
          }
          LOG(TEXT( "DWORD changed from %08x to %08x %s set to %f%%." ), dwStat, ext->value, ext->szName,ext->fvalue );
          if( ext->flag == WAVECAPS_PLAYBACKRATE )
          {
               dwExpectedPlaytime = (DWORD)( dwExpectedPlaytime / ( ext->fvalue/100.0 ) );
          }
     }
     //DWORD tick = GetTickCount();
     SineWave( wh.lpData, wh.dwBufferLength, &wfx, dFrequency );
     //LOG(TEXT( "SineWave takes %d ms." ), GetTickCount() - tick);
     hr = waveOutPrepareHeader( hwo, &wh, sizeof(WAVEHDR) );
     CheckMMRESULT( hr, "ERROR:  Failed to prepare header.  waveOutPrepareHeader",
                         TPR_ABORT, "Driver doesn't really support this format" );
     bWaveOutHeaderPrepared = TRUE;
     QueryPerformanceCounter( reinterpret_cast<LARGE_INTEGER*>( &start_count ) );
     hr = waveOutWrite( hwo, &wh, sizeof(WAVEHDR) );
     CheckMMRESULT( hr, "ERROR:  Failed to write.  waveOutWrite", TPR_ABORT, "Driver doesn't really support this format" );
     switch( dwNotification )
     {
          case CALLBACK_NULL:
               dwTime = 0;
               while( (!(wh.dwFlags&WHDR_DONE)) && ( dwTime<dwExpectedPlaytime+1000) )
               {
                    Sleep( g_dwSleepInterval );
                    dwTime += g_dwSleepInterval;
               }
               QueryPerformanceCounter( reinterpret_cast<LARGE_INTEGER*>(&finish_count) );
               break;

          case CALLBACK_FUNCTION:
          case CALLBACK_THREAD:
          case CALLBACK_WINDOW:
          case CALLBACK_EVENT:
               hr = WaitForSingleObject( hEvent, dwExpectedPlaytime+1000 );
               QueryPerformanceCounter( reinterpret_cast<LARGE_INTEGER*>(&finish_count) );
               break;
     }
     duration = (finish_count - start_count) * 1000 / m_Resolution;
     expected = dwExpectedPlaytime;
     LOG(TEXT("Resolution:  %I64i ticks/sec, Start:  %I64i ticks, Finish:  %I64i ticks,  Duration:  %I64i ms, \
              Expected duration:  %I64i ms.\n"), m_Resolution, start_count, finish_count, duration, expected );
     expected = duration-expected;
     if( expected < 0 )
     {
          QueryCondition( ( -expected > g_dwAllowance), "ERROR:  expected playtime is too short",
                              TPR_ABORT,"Driver notifying the end of playback too early" );
     }
     else
     {
          QueryCondition( ( expected > g_dwAllowance ),"ERROR:  expected playtime is too long",
                              TPR_ABORT, "Driver notifying the end of playback too late");
          QueryCondition( ( expected >= 1000 ), "ERROR:  notification not received within one second after expected playtime",
                              TPR_ABORT, "Driver is not signalling notification at all");
     }
Error:
     if( hwo )
     {
          hr = waveOutReset( hwo );
          QueryMMRESULT( hr, "ERROR:  Failed to reset.  waveOutReset", TPR_ABORT, "Driver didn't reset device properly" );
          if( bWaveOutHeaderPrepared )
          {
               hr = waveOutUnprepareHeader( hwo, &wh, sizeof(WAVEHDR) );
               QueryMMRESULT( hr, "ERROR:  waveOutUnprepareHeader failed.", TPR_ABORT, "Driver doesn't really support this format" );
               bWaveOutHeaderPrepared = FALSE;
          }
          hr = waveOutClose(hwo);
          QueryMMRESULT( hr, "ERROR:  Failed to close.  waveOutClose", TPR_ABORT, "Driver didn't close device properly" );
          switch( dwNotification )
          {
               case CALLBACK_FUNCTION:
               case CALLBACK_THREAD:
               case CALLBACK_WINDOW:
                    hr = WaitForSingleObject( hEvent, 1000 );
                    QueryCondition( hr == WAIT_TIMEOUT, "ERROR:  Function, Thread or Window didn't notify that it \
                                                        received the close message within one second", TPR_ABORT,
                                                        "Close message wasn't sent to handler");
                    QueryCondition( hr != WAIT_OBJECT_0, "ERROR:  Unknown Error while waiting for driver to close", TPR_ABORT, "Unknown");
                    break;
          }
          HMODULE hCoreDLL = GetCoreDLLHandle();
          switch( dwNotification )
          {
               case CALLBACK_WINDOW:
                    if(NULL != hCoreDLL)
                    {
                         PFNPOSTMESSAGE pfnPostMessage = (PFNPOSTMESSAGE)GetProcAddress( hCoreDLL, TEXT("PostMessageW") );
                         if( NULL != pfnPostMessage )
                              pfnPostMessage( hwnd, WM_QUIT, 0, 0 );
                    }
                    else
                    {
                         LOG(TEXT("ERROR: Unable to get proc address for PostMessage"));
                         tr |= TPR_ABORT;
                    }
                    break;

               case CALLBACK_THREAD:
                    if(FALSE == PostThreadMessage( dwCallback, WM_QUIT, 0, 0 ) )
                    {
                         int ret = GetLastError();
                         LOG(TEXT("ERROR: PostThreadMessage WM_QUIT Failed."));
                         if( ERROR_INVALID_THREAD_ID == ret )
                              LOG(TEXT("ERROR: INVALID_THREAD_ID"));
                         tr |= TPR_ABORT;
                    }
                    break;

               if( NULL != hCoreDLL )
               {
                    PFNPOSTTHREADMESSAGE pfnPostThreadMessage = (PFNPOSTTHREADMESSAGE)GetProcAddress( hCoreDLL, TEXT("PostThreadMessageW") );
                    if( NULL != pfnPostThreadMessage )
                    {
                         if(FALSE == pfnPostThreadMessage( dwCallback, WM_QUIT, 0, 0) )
                         {
                              LOG(TEXT("ERROR: PostThreadMessage WM_QUIT Failed."));
                              FreeCoreDLLHandle( hCoreDLL );
                              return TPR_ABORT;
                         }
                         else
                              LOG(TEXT("POSTTHREADMESSAGE PASSED"));
                         int ret = GetLastError();
                         if( ERROR_INVALID_THREAD_ID == ret)
                              LOG(TEXT("ERROR: INVALID_THREAD_ID"));
                    }
               }
               else
               {
                    LOG(TEXT("ERROR: Unable to get proc address for PostThreadMessage"));
                    tr |= TPR_ABORT;
               }
               break;
          }// end switch( dwNotification )

          FreeCoreDLLHandle(hCoreDLL);

          switch(dwNotification)
          {
               case CALLBACK_THREAD:
                    hr = WaitForSingleObject(hThread,1000);
                    QueryCondition( hr == WAIT_TIMEOUT, "ERROR:  Thread didn't release within one second", TPR_ABORT, "Unknown" );
                    QueryCondition( hr != WAIT_OBJECT_0, "ERROR:  Unknown Error while waiting for thread to release", TPR_ABORT, "Unknown" );
          }
     }// end if( hwo )

     if( data )
          delete [] data;
     if( hEvent )
          CloseHandle( hEvent );
     if( hThread )
          CloseHandle( hThread );
     return tr;
}


BOOL FullDuplex()
{
     MMRESULT mmRtn;
     HWAVEOUT hwo = NULL;
     HWAVEIN hwi = NULL;
     WAVEFORMATEX wfx;
     DWORD i;
     
     WAVEINCAPS wic;
     WAVEOUTCAPS woc;

     // get device capabilities so we can compare the device names
     if(MMSYSERR_NOERROR != waveInGetDevCaps(g_dwInDeviceID, &wic, sizeof(wic)) ||
        MMSYSERR_NOERROR != waveOutGetDevCaps(g_dwOutDeviceID, &woc, sizeof(woc)) )
     {
         LOG(TEXT("FAILURE: Could not get Device Capabilities"));
         return FALSE;
     }

     // show WaveIn and WaveOut DeviceIDs
     LOG(TEXT("FullDuplex Testing Devices:"));
     LOG(TEXT("  WaveIn DeviceID  = %d; DeviceName = %s"), g_dwInDeviceID, wic.szPname);
     LOG(TEXT("  WaveOut DeviceID = %d; DeviceName = %s"), g_dwOutDeviceID, woc.szPname);
     
     // compare the WaveIn and WaveOut device names
     if( 0 != _tcscmp(wic.szPname, woc.szPname))
     {
         LOG(TEXT("WARNING: Specified WaveIn and WaveOut devices are not the same device."));
     }
     
     for( i=0; lpFormats[i].szName; i++ )
     {
          StringFormatToWaveFormatEx( &wfx, lpFormats[i].szName );
          mmRtn = waveOutOpen( &hwo, g_dwOutDeviceID, &wfx, CALLBACK_NULL, NULL, NULL );
          if( mmRtn == MMSYSERR_NOERROR )
          {
               mmRtn = waveInOpen( &hwi, g_dwInDeviceID, &wfx, CALLBACK_NULL, NULL, NULL );
               if( mmRtn == MMSYSERR_NOERROR )
               {
                    if(hwo)
                         waveOutClose( hwo );
                    if(hwi)
                         waveInClose( hwi );
                    return TRUE;
               }
          }
          if( hwo )
               waveOutClose( hwo );
          if( hwi )
               waveInClose( hwi );
          hwo = NULL;
          hwi = NULL;
     }
     return FALSE;
}


TESTPROCAPI BVT(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
     BEGINTESTPROC

     HRESULT hr = 0;
     HANDLE hFile;
     UINT nOut,nIn;
     DWORD tr = TPR_PASS;
     WIN32_FIND_DATA findData;
     TCHAR szKeyPath[MAX_PATH] = TEXT("");

     if( g_bSkipOut )
          return TPR_SKIP;

     LOG(TEXT( "INSTRUCTIONS:  This test case searches the registry for active driver entries." ));
     LOG(TEXT( "INSTRUCTIONS:  Additionally, the registered WaveIn and WaveOut devices are listed."));
     LOG(TEXT( "INSTRUCTIONS:  Please confirm that your driver is listed." ));
     ListActiveDrivers();
     ListWaveDevices();
     nOut = waveOutGetNumDevs();
     nIn = waveInGetNumDevs();

#ifdef UNDER_CE
     LOG(TEXT("Verifying Waveform Audio Driver (WAM1:)"));
     hr = GetActiveDriverKey( TEXT("WAM1:"), szKeyPath, MAX_PATH );

     QueryCondition( FAILED(hr), "ERROR:  WAM1: was not found", TPR_FAIL,
               "Waveform Audio not included in image (BSP_NOAUDIO flag enabled)")
     else
     {
          LOG(TEXT("WAM1:  was found at %s"), szKeyPath );
     }
#endif

     if( nOut<=g_dwOutDeviceID )
     {
          LOG(TEXT("WARNING:  waveOutGetNumDevs reported %u device(s)"), nOut);
          LOG(TEXT("WARNING:  All output tests will be skipped"));
          g_bSkipOut = TRUE;
     }
     if( nIn<=g_dwInDeviceID )
     {
          LOG(TEXT("WARNING:  waveInGetNumDevs reported %u device(s)"), nIn);
          LOG(TEXT("WARNING:  All input tests will be skipped"));
          g_bSkipIn = TRUE;
     }
     
     hFile = FindFirstFile( TEXT("\\windows\\*.wav"), &findData );
     if( hFile == INVALID_HANDLE_VALUE )
     {
          LOG(TEXT("WARNING:  There are no wave files in the Windows Directory"));
          LOG(TEXT("WARNING:  Please include wave files in your image to verify your driver"));
          LOG(TEXT("Possible Cause:  Wave files not included in image"));
     }
     else
     {
          FindClose( hFile );
     }

     if( tr & TPR_FAIL )
     {
         g_bSkipOut = g_bSkipIn  =TRUE;
     }

     if( !(g_bSkipOut && g_bSkipIn) )
     {
          LOG(TEXT("Testing Driver's Ability to Handle Full-Duplex Operation"));
          if(g_useSound)
          {
               if( FullDuplex() )
               {
                    LOG(TEXT("Success:  Able to open waveIn and waveOut at the same time"));
                    LOG(TEXT("          Your driver claims Full-Duplex Operation"));
                    LOG(TEXT("          Your commandline claims Full-Duplex Operation"));
                    LOG(TEXT("Warning:  If you are unable to play and capture sound at the same time,"));
                    LOG(TEXT("          your driver claims to have Full-Duplex Operation,"));
                    LOG(TEXT("          but really doesn't support Full-Duplex Operation"));
               }
               else
               {
                    LOG(TEXT("Failure:  Unable to open waveIn and waveOut at the same time"));
                    LOG(TEXT("          Your driver claims Half-Duplex Operation"));
                    LOG(TEXT("          Your commandline claims Full-Duplex Operation"));
                    LOG(TEXT("          Turning off Full-Duplex Operation"));
                    g_useSound = FALSE;
                    tr = TPR_FAIL;
               }
               LOG(TEXT("          Fix your driver to work in Full-Duplex,"));
               LOG(TEXT("          or test your driver as a Half-Duplex driver"));
               LOG(TEXT("          with commandline -c \"-e\" options."));
          }
          else
          {
               if( FullDuplex() )
               {
                    LOG(TEXT("Failure:  Able to open waveIn and waveOut at the same time"));
                    LOG(TEXT("          Your driver claims Full-Duplex Operation"));
                    LOG(TEXT("          Your commandline claims Half-Duplex Operation (-c \"-e\")"));
                    LOG(TEXT("          Fix your driver to work in Half-Duplex by making sure that" ));
                    LOG(TEXT("          waveIn and waveOut cannot be opened at the same time"));
                    LOG(TEXT("          or test your driver as a Full-Duplex driver"));
                    LOG(TEXT("          without commandline -c \"-e\" options."));
                    tr = TPR_FAIL;
               }
               else
               {
                    LOG(TEXT("Success:  Unable to open waveIn and waveOut at the same time"));
                    LOG(TEXT("          Your driver claims Half-Duplex Operation"));
                    LOG(TEXT("          Your commandline claims Half-Duplex Operation (-c \"-e\")"));
               }
          }
     }
     else
     {
          tr = TPR_SKIP;
     }

     return tr;
}






TESTPROCAPI EasyPlayback(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
     BEGINTESTPROC

     BOOL result;
     HANDLE hFile;
     int iResponse;
     DWORD tr = TPR_PASS;
     WIN32_FIND_DATA findData;

     if( g_bSkipOut )
     {
          return TPR_SKIP;
     }

     LOG(TEXT("INSTRUCTIONS:  This test case plays wave files from the Windows Directory" ));
     LOG(TEXT("INSTRUCTIONS:  Please verify that you hear files being played by the sndPlaySound and PlaySound APIs" ));
     LOG(TEXT("INSTRUCTIONS:  Easy Playback only works for the default device" ));
     LOG(TEXT("INSTRUCTIONS:  There is no way of specifying which device to use with the sndPlaySound and PlaySound APIs" ));
     LOG(TEXT("INSTRUCTIONS:  Reconfigure your system so that your driver becomes the default driver to run this test case"));

RunsndPlaySound:
     LOG(TEXT("Playing all files in Windows Directory with sndPlaySound API" ));
     hFile = FindFirstFile( TEXT("\\windows\\*.wav"), &findData );
     if( INVALID_HANDLE_VALUE  == hFile )
     {
          LOG(TEXT("ERROR: There are no wave files to play in the \\windows\\ directory."));
          LOG(TEXT("Suggestion: Add some *.wav files to the \\windows\\ directory and try again."));
          FindClose( hFile );
          return TPR_SKIP;
     }

     LOG(TEXT("Using sndPlaySound to play %s"), findData.cFileName );
     result = sndPlaySound( findData.cFileName, SND_NODEFAULT );
     if( !result )
          goto Error;

     while( FindNextFile( hFile, &findData ) )
     {
          LOG(TEXT("Using sndPlaySound to play %s"), findData.cFileName );
          result = sndPlaySound( findData.cFileName, SND_NODEFAULT );
          if( !result )
               goto Error;
     }
     FindClose( hFile );
     if( g_interactive )
     {
          if( g_headless )
          {
               LOG(TEXT("Running in Interactive Headless Mode."));

               TCHAR tchC;
               DWORD retVal = GetHeadlessUserInput(TEXT("INTERACTIVE PROMPT: Did you hear the files from the \
                                                   Windows Directory?\n(Press 'Y' for Yes, 'N' for No or 'R' for Retry)"), &tchC);
               if( retVal != TPR_PASS )
               {
                    LOG(TEXT("ERROR: Failed to get headless interactive user input."));
                    if( retVal == TPR_SKIP )
                         //---- Then Keyboard is not present on device
                         return TPR_SKIP;
                    else
                         return GetReturnCode(retVal, TPR_ABORT);
               }

               if( tchC == (TCHAR)'N' || tchC == (TCHAR)'n' )
               {
                    LOG(TEXT("ERROR:  User said sndPlaySound failed.") );
                    tr |= TPR_FAIL;
               }
               else if( tchC == (TCHAR)'R' || tchC == (TCHAR)'r' )
               {
                    //---- Need to re-play the sound again
                    goto RunsndPlaySound;
               }
               //---- else user entered 'Y', thus they heard the audio, and thus we can move on with the test
          }
          else
          {
               LOG(TEXT("Running in Interactive Mode."));
               HMODULE hCoreDLL = GetCoreDLLHandle();
               if(NULL != hCoreDLL)
               {
                    PFNMESSAGEBOX pfnMessageBox = (PFNMESSAGEBOX)GetProcAddress(hCoreDLL, TEXT("MessageBoxW"));
                    if(NULL  != pfnMessageBox)
                    {
                         iResponse = pfnMessageBox(NULL,TEXT("Did you hear the files from the Windows Directory? \
                                                   (cancel to retry)"), TEXT("Interactive Response"), MB_YESNOCANCEL | MB_ICONQUESTION);
                         switch(iResponse)
                         {
                              case IDNO:
                                   LOG(TEXT("ERROR:  User said sndPlaySound failed "));
                                   tr|=TPR_FAIL;
                                   break;
                              case IDCANCEL:
                                   goto RunsndPlaySound;
                         }
                    }
                    else
                    {
                         LOG(TEXT("ERROR: pfnMessageBox is NULL, GetProcAddress failed."));
                         tr |= TPR_ABORT;
                    }
                    if( FALSE == FreeCoreDLLHandle(hCoreDLL) )
                    {
                         LOG(TEXT("ERROR: FreeCoreDLLHandle Failed."));
                         tr |= TPR_ABORT;
                    }
               }
               else
               {
                    //---- Could not get a handle to coredll
                    tr |= TPR_ABORT;
                    iResponse = IDYES;       //---- Set to Yes to skip switch statement below
               }
          }
     }

RunPlaySound:
     LOG(TEXT("Playing all files in Windows Directory with PlaySound API" ));
     hFile = FindFirstFile( TEXT("\\windows\\*.wav"), &findData );
     if( INVALID_HANDLE_VALUE  == hFile )
     {
          LOG(TEXT("ERROR: There are no wave files to play in the \\windows\\ directory."));
          LOG(TEXT("Suggestion: Add some *.wav files to the \\windows\\ directory and try again."));
          FindClose( hFile );
          return TPR_SKIP;
     }
     LOG(TEXT("Using PlaySound to play %s"), findData.cFileName );
     result = PlaySound( findData.cFileName, NULL, SND_NODEFAULT );
     if( !result )
          goto Error;

     while( FindNextFile( hFile, &findData ) )
     {
          LOG(TEXT("Using PlaySound to play %s"), findData.cFileName );
          result = PlaySound( findData.cFileName, NULL, SND_NODEFAULT );
          if( !result )
               goto Error;
     }
     FindClose( hFile );
     if( g_interactive )
     {
          if( g_headless )
          {
               TCHAR tchC;
               DWORD retVal = GetHeadlessUserInput( TEXT("INTERACTIVE PROMPT: Did you hear the files from the \
                                                   Windows Directory? \n(Press 'Y' for yes, 'N' for No or 'C' for Cancel/Retry)"), &tchC );
               if( retVal != TPR_PASS )
               {
                    LOG(TEXT("ERROR: Failed to get headless interactive user input."));
                    if( retVal == TPR_SKIP )
                         //---- Then Keyboard is not present on device
                         return TPR_SKIP;
                    else
                         return GetReturnCode( retVal, TPR_ABORT );
               }

               if( tchC == (TCHAR)'N' || tchC == (TCHAR)'n' )
               {
                    LOG(TEXT("ERROR:  User said PlaySound failed.") );
                    tr |= TPR_FAIL;
               }
               else if( tchC == (TCHAR)'R' || tchC == (TCHAR)'r' )
               {
                    //---- Need to re-play the sound again
                    goto RunPlaySound;
               }
               //---- else user entered 'Y', thus they heard the audio, and thus we can move on with the test

          }
          else
          {
               HMODULE hCoreDLL = GetCoreDLLHandle();
               if(NULL != hCoreDLL)
               {
                    PFNMESSAGEBOX pfnMessageBox = (PFNMESSAGEBOX)GetProcAddress(hCoreDLL, TEXT("MessageBoxW"));
                    if(NULL  != pfnMessageBox)
                    {
                         iResponse = pfnMessageBox(NULL,TEXT("Did you hear the files from the Windows Directory? \
                                                   (cancel to retry)"), TEXT("Interactive Response"), MB_YESNOCANCEL | MB_ICONQUESTION);
                         switch( iResponse )
                         {
                              case IDNO:
                                   LOG(TEXT("ERROR:  User said PlaySound failed "));
                                   tr |= TPR_FAIL;
                                   break;

                              case IDCANCEL:
                                   goto RunPlaySound;
                         } // end switch(iResponse)
                    }
                    else
                    {
                         LOG(TEXT("ERROR: pfnMessageBox is NULL, GetProcAddress failed."));
                         tr |= TPR_ABORT;
                    }
                    if( FALSE == FreeCoreDLLHandle(hCoreDLL) )
                         tr |= TPR_ABORT;
               }
               else
               {
                    //---- Could not get a handle to coredll
                    LOG(TEXT("ERROR: Could not get a handle to coredll."));
                    tr |= TPR_ABORT;
               }
          } // end else
     } // end interactive
     return tr;

Error:
     LOG(TEXT("ERROR:  The function returned FALSE (it was unsuccessful)" ));
     LOG(TEXT("Possible Cause:  Not Enough Memory" ));
     LOG(TEXT("Possible Cause:  %s is not playable on the device and/or driver"), findData.cFileName);
     FindClose( hFile );
     return TPR_FAIL;
}







TESTPROCAPI PlaybackCapabilities(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
     BEGINTESTPROC
     DWORD tr = TPR_PASS;
     MMRESULT mmRtn;
     WAVEOUTCAPS woc;

     if( g_bSkipOut )
          return TPR_SKIP;
     LOG(TEXT("INSTRUCTIONS:  This test case displays Driver Capabilities." ));
     LOG(TEXT("INSTRUCTIONS:  Driver Capabilies need to be confirmed manually" ));
     LOG(TEXT("INSTRUCTIONS:  Please confirm that your driver is capable of performing the functionality that it reports" ));
     mmRtn = waveOutGetDevCaps( g_dwOutDeviceID, &woc, sizeof(woc) );
     CheckMMRESULT( mmRtn, "ERROR:  Failed to get device caps.  waveOutGetDevCaps", TPR_FAIL, "Driver responded incorrectly" );
     LogWaveOutDevCaps( g_dwOutDeviceID, &woc );
Error:
     return tr;
}






TESTPROCAPI Playback(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
     BEGINTESTPROC
     DWORD tr=TPR_PASS;
     DWORD itr;
     DWORD hrReturn;
     DWORD i;
     WAVEOUTCAPS woc;
     MMRESULT mmRtn;
     int iResponse;

     if( g_bSkipOut )
          return TPR_SKIP;
     LOG(TEXT("INSTRUCTIONS:  This test case will attempt to playback a tone for all common lpFormats"));
     LOG(TEXT("INSTRUCTIONS:  Please listen to playback to ensure that a tone is played" ));
     mmRtn = waveOutGetDevCaps( g_dwOutDeviceID, &woc, sizeof(woc) );
     CheckMMRESULT( mmRtn, "ERROR:  Failed to get device caps.  waveOutGetDevCaps", TPR_FAIL, "Driver responded incorrectly" );

     for( i = 0; lpFormats[i].szName; i++ )
     {
          hrReturn = MMSYSERR_NOERROR;
          LOG(TEXT("Attempting to playback %s"),lpFormats[i].szName );
          itr = PlayWaveFormat( lpFormats[i].szName, g_duration, CALLBACK_NULL, NULL, &hrReturn );
          if( woc.dwFormats&lpFormats[i].value )
          {
               if( itr == TPR_FAIL )
                    LOG(TEXT("ERROR:  waveOutGetDevCaps reports %s is supported, but %s was returned"),
                                   lpFormats[i].szName, g_ErrorMap[hrReturn]);
               else if( itr == TPR_ABORT )
               {
                    LOG(TEXT("ERROR:  PlayFormat Returned An ABORT. See above Debug Info For Explanation."));
                    itr = TPR_FAIL;
               }
          }
          else if( itr == TPR_PASS )
          {
               //---- AND !woc.dwFormats&lpFormats[i].value
               LOG(TEXT("ERROR:  waveOutGetDevCaps reports %s is unsupported, but WAVERR_BADFORMAT was not returned"),lpFormats[i].szName);
               itr = TPR_FAIL;
          }
          else
          {
               //---- itr != TPR_PASS && !woc.dwFormats&lpFormats[i].value
               if( itr == TPR_ABORT )
               {
                    LOG(TEXT("ERROR:  PlayWaveFormat returned an ABORT. See above Debug Info For Explanation."));
                    itr = TPR_FAIL;
               }
               else if( itr == TPR_FAIL )
                    itr = TPR_FAIL;
               else
                    itr = TPR_PASS;
          }
          if(g_interactive)
          {
               if(g_headless)
               {
                    LOG(TEXT("Running in Interactive Headless Mode."));
                    TCHAR tchC;
                    DWORD retVal = GetHeadlessUserInput(TEXT("INTERACTIVE PROMPT: Did you hear the tone?\n\
                                                        (Press 'Y' for Yes, 'N' for No or 'R' for Retry)"), &tchC);
                    if(retVal != TPR_PASS )
                    {
                         LOG(TEXT("ERROR: Failed to get headless interactive user input."));
                         if( retVal == TPR_SKIP )
                              //---- Then Keyboard is not present on device
                              return TPR_SKIP;
                         else
                              return GetReturnCode( retVal, TPR_ABORT );
                    }

                    if( tchC == (TCHAR)'N' || tchC == (TCHAR)'n' )
                    {
                         LOG(TEXT("ERROR:  User said there was no tone produced for %s"), lpFormats[i].szName);
                         tr |= TPR_FAIL;
                    }
                    else if( tchC == (TCHAR)'R' || tchC == (TCHAR)'r' )
                    {
                         i--;
                    }
                    //---- else user entered 'Y', thus they heard the audio, and thus we can move on with the test
               }
               else
               {
                    LOG(TEXT("Running in Interactive Mode."));
                    HMODULE hCoreDLL = GetCoreDLLHandle();
                    if( NULL != hCoreDLL )
                    {
                         PFNMESSAGEBOX pfnMessageBox = (PFNMESSAGEBOX)GetProcAddress(hCoreDLL, TEXT("MessageBoxW"));
                         if( NULL  != pfnMessageBox )
                         {
                              iResponse = pfnMessageBox(NULL,TEXT("Did you hear the tone? (cancel to retry)"),
                                                       TEXT("Interactive Response"), MB_YESNOCANCEL | MB_ICONQUESTION);
                              switch(iResponse)
                              {
                                   case IDNO:
                                        LOG(TEXT("ERROR:  User said there was no tone produced for %s"),lpFormats[i].szName);
                                        tr |= TPR_FAIL;
                                        break;

                                   case IDCANCEL:
                                        i--;
                              } // end switch
                         }
                         else
                         {
                              LOG(TEXT("ERROR: pfnMessageBox is NULL, GetProcAddress failed."));
                              tr |= TPR_ABORT;
                              goto Error;
                         }
                         if( FALSE == FreeCoreDLLHandle(hCoreDLL) )
                         {
                              tr|=TPR_ABORT;
                              goto Error;
                         }
                    }
                    else
                    {
                         //---- Could not get a handle to coredll
                         LOG(TEXT("ERROR:Failed to get a handle to coredll.dll\n"));
                         tr|=TPR_ABORT;
                         goto Error;
                    }
               }
          } // end if(g_interactive)
          else
               tr|=itr;
     }// end for()
Error:
     return tr;
}






TESTPROCAPI PlaybackNotifications(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
     BEGINTESTPROC
     DWORD tr = TPR_PASS;
     DWORD itr;
     DWORD i, j;
     WAVEOUTCAPS woc;
     MMRESULT mmRtn;
     int iResponse;

     if( g_bSkipOut )
          return TPR_SKIP;
     LOG(TEXT("INSTRUCTIONS:  This test case will attempt to record and playback a tone for all types of lpCallbacks"));
     LOG(TEXT("INSTRUCTIONS:  Please listen to playback to ensure that a tone is played"));
     mmRtn = waveOutGetDevCaps( g_dwOutDeviceID, &woc, sizeof(woc) );
     CheckMMRESULT( mmRtn, "ERROR:  Failed to get device caps.  waveOutGetDevCaps", TPR_FAIL, "Driver responded incorrectly" );

     for( i = 0; lpFormats[i].szName; i++ )
          if( woc.dwFormats&lpFormats[i].value )
               break;
          if( !lpFormats[i].szName )
          {
               LOG(TEXT("ERROR:  There are no supported lpFormats"));
               return TPR_SKIP;
          }

     for( j = 0; lpCallbacks[j].szName; j++ )
     {
          LOG(TEXT("Attempting to playback with %s"),lpCallbacks[j].szName);
          itr = PlayWaveFormat( lpFormats[i].szName, g_duration, lpCallbacks[j].value, NULL, NULL );
          if( g_interactive )
          {
               if( g_headless )
               {
                    LOG(TEXT("Running in Interactive Headless Mode."));
                    TCHAR tchC;
                    DWORD retVal = GetHeadlessUserInput(TEXT("INTERACTIVE PROMPT: Did you hear the tone?\n(Press 'Y' for Yes, \
                                                             'N' for No or 'R' for Retry)"), &tchC);
                    if( retVal != TPR_PASS )
                    {
                         LOG(TEXT("ERROR: Failed to get headless interactive user input."));
                         if(retVal == TPR_SKIP)
                              //---- Then Keyboard is not present on device
                              return TPR_SKIP;
                         else
                              return GetReturnCode(retVal, TPR_ABORT);
                    }
                    if( tchC == (TCHAR)'N' || tchC == (TCHAR)'n' )
                    {
                         LOG(TEXT("ERROR:  User said there was no tone produced for %s"), lpFormats[i].szName);
                         tr |= TPR_FAIL;
                    }
                    else if( tchC == (TCHAR)'R' || tchC == (TCHAR)'r' )
                    {
                         j--;
                    }
                    //---- else user entered 'Y', thus they heard the audio, and thus we can move on with the test
               }
               else
               {
                    LOG(TEXT("Running in Interactive Mode."));
                    HMODULE hCoreDLL = GetCoreDLLHandle();
                    if( NULL != hCoreDLL )
                    {
                         PFNMESSAGEBOX pfnMessageBox = (PFNMESSAGEBOX)GetProcAddress(hCoreDLL, TEXT("MessageBoxW"));
                         if( NULL  != pfnMessageBox )
                         {
                              iResponse = pfnMessageBox(NULL,TEXT("Did you hear the tone? (cancel to retry)"),
                                                  TEXT("Interactive Response"),MB_YESNOCANCEL|MB_ICONQUESTION);
                              switch(iResponse)
                              {
                                   case IDNO:
                                        LOG(TEXT("ERROR:  User said there was no tone produced for %s"),lpFormats[i].szName);
                                        tr |= TPR_FAIL;
                                        break;

                                   case IDCANCEL:
                                        j--;
                              }
                         }
                         else
                         {
                              LOG(TEXT("ERROR: pfnMessageBox is NULL, GetProcAddress failed."));
                              tr |= TPR_ABORT;
                              goto Error;
                         }
                         if( FALSE == FreeCoreDLLHandle(hCoreDLL) )
                         {
                              tr |= TPR_ABORT;
                              goto Error;
                         }

                    }
                    else
                    {
                         //---- Could not get a handle to coredll
                         LOG(TEXT("ERROR:Failed to get a handle to coredll.dll\n"));
                         tr |= TPR_ABORT;
                         goto Error;
                    }
               }
          }
          else
               tr |= itr;
     }
Error:
     return tr;
}






TESTPROCAPI PlaybackVirtualFree (UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
     BEGINTESTPROC
     DWORD dwRet = TPR_PASS;
     DWORD nBytes = 0;
     WAVEHDR wh;
     HRESULT hResult;
     LPWAVEFORMATEX pwfx;
     LPCTSTR pszFilename = TEXT("windows\\startup.wav");
     CWaveFile WaveFile;
     MMRESULT mmRtn;
     HWAVEOUT hwo;
     UINT n = 0, i = 0;

     if(NULL == pszFilename)
     {
          LOG(TEXT("ERROR: pszFilename passed in was invalid."));
          return TPR_FAIL;
     }
     LOG(TEXT("File to playback is: %s"), pszFilename);
     ZeroMemory(&wh,sizeof(wh));

     //---- See how many waveOut devices are registered in the system.
     n = waveOutGetNumDevs();
     if(0 == n)
     {
          LOG(TEXT( "ERROR: waveOutGetNumDevs reported zero devices, we need at least one."));
          dwRet = TPR_SKIP;
          goto errorExitFunction;
     }

     hResult = WaveFile.Create( pszFilename, GENERIC_READ, OPEN_EXISTING, 0, (LPVOID*)&pwfx, NULL );
     if( ERROR_SUCCESS != hResult )
     {
          LOG(TEXT("ERROR: Could not Open %s"), pszFilename);
          LOG(TEXT("Now trying Default.wav instead."));
          //---- some retail images do not have startup.wav
          //---- Rather than give up on the test we will also try to use the shorter file default.wav
          hResult = WaveFile.Create( TEXT("windows\\default.wav"), GENERIC_READ, OPEN_EXISTING, 0, (LPVOID*)&pwfx, NULL );
          if( ERROR_SUCCESS != hResult )
          {
               LOG(TEXT("ERROR: Could not Open Default.wav"));
               dwRet = TPR_SKIP;
               goto errorInVirtFreeCloseOnly;
          }
     }

     wh.dwBufferLength = WaveFile.WaveSize();
     if( wh.dwBufferLength < UINT_MAX )
     {
          //---- Allocate Space for the buffer
          wh.lpData = (CHAR *)VirtualAlloc(NULL, wh.dwBufferLength, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
          if(wh.lpData  == NULL)
          {
               LOG(TEXT("ERROR: VirtualAlloc faild: Unable to allocate buffer for wave data.\n"));
               dwRet = TPR_ABORT;
               goto errorInVirtFreeCloseOnly;
          }
     }
     else
     {
          LOG(TEXT("ERROR: wh.dwBufferLength >= UINT_MAX "));
          dwRet = TPR_FAIL;
          goto errorInVirtFreeCloseOnly;
     }

     WaveFile.ReadData( wh.lpData, wh.dwBufferLength, &nBytes );
     if( nBytes != wh.dwBufferLength )
     {
          LOG(TEXT("ERROR: Could not read entire file"));
          dwRet = TPR_FAIL;
          goto errorInVirtFreeCloseOnly;
     }

     //---- Open the Wave Output Stream
     mmRtn = waveOutOpen( &hwo, g_dwOutDeviceID, pwfx, NULL, 0, CALLBACK_NULL );
     if( MMSYSERR_NOERROR != mmRtn )
     {
          LOG(TEXT("ERROR: waveOutOpen failed with return code %d"), mmRtn);
          dwRet = TPR_FAIL;
          goto errorInVirtFreePlayback;
     }

     //---- Prepare our header
     mmRtn = waveOutPrepareHeader( hwo,&wh, sizeof(wh) );
     if( MMSYSERR_NOERROR != mmRtn )
     {
          LOG(TEXT("ERROR: waveOutPrepareHeader failed with return code %d"), mmRtn);
          dwRet = TPR_FAIL;
          goto errorInVirtFreePlayback;
     }

     //---- Write out the header to the waveform audio output stream
     mmRtn=waveOutWrite( hwo, &wh, sizeof(wh) );
     if( MMSYSERR_NOERROR != mmRtn )
     {
          LOG(TEXT("ERROR: waveOutWrite failed with return code %d"), mmRtn);
          dwRet = TPR_FAIL;
          goto errorInVirtFreePlayback;
     }

     //---- Now attempt to VirtualFree the buffer that is currently playing
     BOOL res = VirtualFree(wh.lpData, 0, MEM_RELEASE);
     if( 0 == res )
     {
          int ngle = GetLastError();
          LOG(TEXT("ERROR: VirtualFree failed. Last Error was %d\n"), ngle);
          dwRet = TPR_FAIL;
          goto errorInVirtFreePlayback;
     }
     else
          LOG(TEXT("VirtualFree Succeeded in freeing the buffer."));


     //---- wait for the header to be processed
     UINT uiTotalSleep = 0;
     do
     {
          LOG(TEXT("In Loop. Waiting for Header to be marked WHDR_DONE."));
          Sleep( 500 );
          uiTotalSleep += 500;
          if( uiTotalSleep >= 30000 )
          {
               //---- we will wait for up to 30 seconds for header to be marked done
               LOG(TEXT("Error: After waiting for %d ms, freed header dwFlags was never set to WHDR_DONE"), uiTotalSleep);
               dwRet = TPR_FAIL;
               break;
          }
     } while( !(wh.dwFlags&WHDR_DONE) );

errorInVirtFreePlayback:
     //----unprepare the header
     mmRtn = waveOutUnprepareHeader( hwo, &wh, sizeof(wh));
     if( MMSYSERR_NOERROR != mmRtn )
     {
          LOG(TEXT("ERROR: waveOutUnprepareHeader failed with return code %d"), mmRtn);
          dwRet = TPR_FAIL;
     }

     //---- close the waveform output stream
     mmRtn = waveOutClose( hwo );
     if( MMSYSERR_NOERROR != mmRtn )
     {
          LOG(TEXT("ERROR: waveOutClose failed with return code %d"), mmRtn);
          dwRet = TPR_FAIL;
     }

errorInVirtFreeCloseOnly:
     WaveFile.Close();

errorExitFunction:
     return dwRet;
}






/*
  * Function: PrintWAVEFORMATEX
  * Purpose: Helper function for WaveOutTestReportedFormats() to make the code clearer
  * Arguments: LPWAVEFORMATEX pwfx : WAVEFORMATEX struc you want to log the details of
  */
void PrintWAVEFORMATEX( LPWAVEFORMATEX pwfx )
{
     if( pwfx == NULL )
          return;
     if( pwfx->wFormatTag == WAVE_FORMAT_PCM )
          LOG(TEXT("wFormatTag: WAVE_FORMAT_PCM"));
     else
          LOG(TEXT("     wFormatTag: %i"), pwfx->wFormatTag );

     LOG(TEXT("     nChannels: %i"), pwfx->nChannels  );
     LOG(TEXT("     nSamplesPerSec: %i"), pwfx->nSamplesPerSec );
     LOG(TEXT("     nAvgBytesPerSec: %i"), pwfx->nAvgBytesPerSec);
     LOG(TEXT("     nBlockAlign: %i"), pwfx->nBlockAlign );
     LOG(TEXT("     wBitsPerSample: %i"), pwfx->wBitsPerSample);
     LOG(TEXT("     cbSize: %i"), pwfx->cbSize);
}





/*
  * Function: IsFormatSupported
  * Purpose: Helper function for WaveOutTestReportedFormats() to make the code clearer
  * Arguments: LPWAVEFORMATEX pwfx : format you want to see if it is supported
  *                   UINT uDeviceID : device you want to see if supports the above format
  */
MMRESULT IsFormatSupported ( LPWAVEFORMATEX pwfx, UINT uDeviceID )
{
     return waveOutOpen( NULL,               // ptr can be NULL for query
                         uDeviceID,          // The device identifier
                         pwfx,               // Defines the requested format.
                         NULL,               // No callback
                         NULL,               // No instance data
                         WAVE_FORMAT_QUERY); // Query only, do not open.
}





/*
  * Function: PrintResult
  * Purpose: Helper function for WaveOutTestReportedFormats() to make the code clearer
  * Arguments: MMRESULT res
  */
void PrintResult( MMRESULT res )
{
     switch(res)
     {
          case MMSYSERR_NOERROR:
               //---- This format is supported
               LOG(TEXT("Is Supported!"));
               LOG(TEXT("\r\n"));
               break;
          case WAVERR_BADFORMAT:
               //---- This format is not supported
               LOG(TEXT("Is NOT supported."));
               LOG(TEXT("\r\n"));
               break;
          default:
               //---- unknown/other driver error
               LOG(TEXT("unknown/other driver error when querying format."));
               LOG(TEXT("\r\n"));
               break;
     }
}





/*
  * Function: IsFormatReportedAsSupported
  * Purpose: Helper function for WaveOutTestReportedFormats() to make the code clearer
  * Arguments: DWORD dwFormats : DWORD representing the formats supported on your device.
                                (This is taken from the dwFormats field of the WaveOutDevCaps struct)
               LPWAVEFORMATEX pwfx : pointer to WAVEFORMATEX describing the format you want to compare
  */
 BOOL IsFormatReportedAsSupported( DWORD dwFormats, LPWAVEFORMATEX pwfx )
{
     DWORD i = 0;

     //---- figure out which (if any) standard format == the pwfx struct
     for( i=0; lpFormats[i].szName; i++ )
     {
          if( dwFormats & lpFormats[i].value )
               return true;
     }

     //---- format is not supported or is not a standard defined format
     return false;
}







/* Playback Sample Rate Reporting
  *
  * Function: WaveOutTestReportedFormats
  *
  * Purpose: Test case to verify that for all formats the driver reports supporting
  * a call to WaveOutOpen with those formats actually succeeds.
  * Valid Range for dwWaveOutDeviceID is a value between 0 and GetWaveOutNumDevs()-1
  *
  */
int WINAPI WaveOutTestReportedFormats( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
     BEGINTESTPROC
     DWORD dwRet = TPR_PASS;
     WAVEFORMATEX wfx;
     WAVEOUTCAPS woc;
     MMRESULT mmRtn;
     UINT n = 0, i = 0;
     DWORD dwWaveOutDeviceID = g_dwOutDeviceID; // test the WAV device chosen on the command line

     LOG(TEXT("Test to Verify that WaveOutOpen succeeds for all reported formats in WaveOutGetDevCaps.\r\n"));
     LOG(TEXT("\r\n"));

     //---- See how many waveOut devices are registered in the system.
     n = waveOutGetNumDevs();

     if( 0 == n )
     {
          LOG(TEXT( "ERROR: waveOutGetNumDevs reported zero devices, we need at least one."));
          dwRet = TPR_SKIP;
          goto errorExitFunction;
     }

     //---- check to see if we really have a wave device with this ID
     if( dwWaveOutDeviceID >= n )
     {
          LOG(TEXT( "ERROR: dwWaveOutDeviceID is invalid as it is too large."));
          dwRet = TPR_ABORT;
          goto errorExitFunction;
     }
     
     //---- Get the WaveOut Device Caps
     mmRtn = waveOutGetDevCaps( dwWaveOutDeviceID, &woc, sizeof(woc) );
     if( mmRtn != MMSYSERR_NOERROR )
     {
          LOG(TEXT( "ERROR:  Failed to get device caps.  waveOutGetDevCaps. Driver responded incorrectly"));
          dwRet = TPR_FAIL;
          goto errorExitFunction;
     }

     //---- Log the supported formats
     LogWaveOutDevCaps( dwWaveOutDeviceID, &woc );

     LOG(TEXT("\r\n"));
     LOG(TEXT("Testing all of the STANDARD formats by Querying waveOutOpen:\r\n"));
     LOG(TEXT("\r\n"));

     //---- Loop through all reported supported formats and report if QUERY SUCCEEDS
     for( WORD bits = 8; bits < 17; bits +=8 )
     {
          for( int samplerate = 11025; samplerate < 44101; samplerate += samplerate )
          {
               for( WORD channels = 1; channels < 3; channels++ )
               {
                    //---- Setup the WAVEFORMATEX struct
                    wfx.wFormatTag      = WAVE_FORMAT_PCM;
                    wfx.nChannels       = channels;
                    wfx.nSamplesPerSec  = samplerate;
                    wfx.wBitsPerSample  = bits;
                    wfx.nBlockAlign     = wfx.nChannels * wfx.wBitsPerSample / 8;
                    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
                    wfx.cbSize          = 0;

                    mmRtn = IsFormatSupported( &wfx, dwWaveOutDeviceID );
                    PrintWAVEFORMATEX( &wfx );

                    BOOL bFormatReportedAsSupported = IsFormatReportedAsSupported( woc.dwFormats, &wfx );
                    //---- If the driver reported supporting this format and the waveOutOpen call failed,
                    //----  then we have a problem
                    if( mmRtn == WAVERR_BADFORMAT && bFormatReportedAsSupported )
                    {
                         //---- Update the return value for this function
                         dwRet = TPR_FAIL;
                         LOG(TEXT("ERROR: Format is reported as supported in the DevCaps, but call to waveOutOpen failed.\r\n"));
                         LOG(TEXT("\r\n"));
                    }
                    //---- If the call to waveOutOpen succeeded but the format is not supported then we have another problem
                    else if( mmRtn == MMSYSERR_NOERROR && !bFormatReportedAsSupported )
                    {
                         //---- Update the return value for this function
                         dwRet = TPR_FAIL;
                         LOG(TEXT("ERROR: Call to waveOutOpen succeeded, but this format is not reported as supported in the DevCaps.\r\n"));
                         LOG(TEXT("\r\n"));
                    }

                    if(  (mmRtn == MMSYSERR_NOERROR && bFormatReportedAsSupported) ||
                         (mmRtn == WAVERR_BADFORMAT && !bFormatReportedAsSupported))
                    {
                         //---- sucess, driver correctly reports this format
                         PrintResult( mmRtn );
                    }
                    else
                    {
                         //---- Update the return value for this function
                         dwRet = TPR_FAIL;
                         LOG(TEXT("ERROR: Call to waveOutOpen failed with return code %i.\r\n"), mmRtn);
                         goto errorExitFunction;
                    }
               }
          }
     }


     //---- QUERY THE DRIVER FOR A NON-STANDARD FORMAT
     //---- In this example we will explicitly query for 48KHz 16bit Stereo
     //---- This example would be modified if you need to support format(s) other than what is defined
     //---- in the DevCaps structure and you want to test your product for these other format(s)

     //---- Setup the WAVEFORMATEX struct
     wfx.wFormatTag      = WAVE_FORMAT_PCM;
     wfx.nChannels       = 2;
     wfx.nSamplesPerSec  = 48000;
     wfx.wBitsPerSample  = 16;
     wfx.nBlockAlign     = wfx.nChannels * wfx.wBitsPerSample / 8;
     wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
     wfx.cbSize          = 0;

     MMRESULT mmres = IsFormatSupported( &wfx, dwWaveOutDeviceID );

     LOG(TEXT("Testing NON-STANDARD format (Sample):\r\n"));
     LOG(TEXT("NOTE: this if for informational purposes only. Failure to support this NON-STANDARD format\r\n"));
     LOG(TEXT("will not cause this test case to fail, nor will it affect your driver certification.\r\n"));
     PrintWAVEFORMATEX( &wfx );
     PrintResult( mmres );

errorExitFunction:
     return dwRet;
}






/*
  * Function: SetOrGetVolume
  *
  * Purpose: Helper function for TestVolume
  * Valid Range for *pdVolume is a value between 0.0 and 1.0
  * If value is out of that range, only the lower 16 bits will be used.
  */
TESTPROCAPI SetOrGetVolume( BOOL fSetOrGet, double *pdVolume )
{
     MMRESULT result;
     DWORD tr = TPR_PASS;
     DWORD i;
     int iResponse = IDNO;
     BOOL foundWaveOut = FALSE;

     if( fSetOrGet )
          LOG(TEXT("SetOrGetVolume: set volume=%d.%d\r\n"), (int)(*pdVolume*100)/100, ((int)(*pdVolume*100)%100)/10);

     //---- Open the mixer device
     HMIXER hmx = NULL;

     result = mixerOpen( &hmx, /*m_WaveID*/0, 0, 0, MIXER_OBJECTF_WAVEOUT );
     if( result != MMSYSERR_NOERROR )
     {
          LOG( TEXT( "ERROR:\tin %s @ line %u"), TEXT( __FILE__ ), __LINE__ );
          LOG( TEXT( "mixerOpen failed with result = %d" ), result );
          mixerClose( hmx );
          return TPR_ABORT;
     }

     //---- Get the line info for the wave out destination line
     MIXERLINE mxl;

     mxl.cbStruct = sizeof(mxl);
     mxl.dwComponentType = MIXERLINE_COMPONENTTYPE_DST_SPEAKERS;

     result = mixerGetLineInfo( (HMIXEROBJ)hmx, &mxl, MIXER_GETLINEINFOF_COMPONENTTYPE );
     if( result != MMSYSERR_NOERROR )
     {
          LOG(TEXT("mixerGetLineInfo failed with result = %d"), result );
          mixerClose( hmx );
          return TPR_ABORT;
     }

     //---- save dwLineID of wave_out dest
     DWORD dwLineID = mxl.dwLineID;

     //---- Now find thesource line connected to this wave out destination
     DWORD cConnections = mxl.cConnections;

     //---- Try speakers
     for( i=0; i<cConnections; i++ )
     {
          mxl.dwSource = i;

          result = mixerGetLineInfo( (HMIXEROBJ)hmx, &mxl, MIXER_GETLINEINFOF_SOURCE );
          if( result != MMSYSERR_NOERROR )
          {
               LOG(TEXT("ERROR: mixerGetLineInfo() failed with result = %d"), result);
               mixerClose( hmx );
               return TPR_ABORT;
          }

          if( MIXERLINE_COMPONENTTYPE_DST_SPEAKERS  == mxl.dwComponentType )
          {
               foundWaveOut = TRUE;
               break;
          }
     }

     //---- get volume control on waveout
     MIXERCONTROL mxctrl;

     MIXERLINECONTROLS mxlctrl = { sizeof(mxlctrl),
                                   mxl.dwLineID,
                                   MIXERCONTROL_CONTROLTYPE_VOLUME,
                                   1,
                                   sizeof(MIXERCONTROL),
                                   &mxctrl };

     if( foundWaveOut )
     {
          result = mixerGetLineControls( (HMIXEROBJ) hmx, &mxlctrl, MIXER_GETLINECONTROLSF_ONEBYTYPE );
          if( result != MMSYSERR_NOERROR )
          {
               //---- we need to try wave-out destination
               foundWaveOut = FALSE;
          }
     }

     if( !foundWaveOut )
     {
          //---- try wave-out dest
          mxlctrl.cbStruct         = sizeof( MIXERLINECONTROLS );
          mxlctrl.dwLineID         = dwLineID;
          mxlctrl.dwControlType    = MIXERCONTROL_CONTROLTYPE_VOLUME;
          mxlctrl.cControls        = 1;
          mxlctrl.cbmxctrl         = sizeof( MIXERCONTROL );
          mxlctrl.pamxctrl         = &mxctrl;

          result = mixerGetLineControls( (HMIXEROBJ) hmx, &mxlctrl, MIXER_GETLINECONTROLSF_ONEBYTYPE );
          if( result != MMSYSERR_NOERROR )
          {
               LOG(TEXT("ERROR: mixerGetLineControls() failed with result = %d"), result);
               mixerClose( hmx );
               return TPR_ABORT;
          }
     }

     //---- Found!
     DWORD cChannels = mxl.cChannels;

     if( MIXERCONTROL_CONTROLF_UNIFORM & mxctrl.fdwControl )
          cChannels = 1;

     if( cChannels > 1 )
          cChannels = 2;

     MIXERCONTROLDETAILS_UNSIGNED pUnsigned[2];

     MIXERCONTROLDETAILS mxcd = {  sizeof(mxcd),
                                   mxctrl.dwControlID,
                                   cChannels,
                                   (HWND)0,
                                   sizeof(MIXERCONTROLDETAILS_UNSIGNED),
                                   (LPVOID) pUnsigned };

     result = mixerGetControlDetails( (HMIXEROBJ)hmx, &mxcd, MIXER_GETCONTROLDETAILSF_VALUE );
     if( result != MMSYSERR_NOERROR )
     {
          LOG(TEXT("ERROR: mixerGetControlDetails failed with result = %d"), result);
          mixerClose( hmx );
          return TPR_ABORT;
     }

     if( fSetOrGet )
     {
          //---- Save the old volume...used for interactive mode
          g_oldVol = pUnsigned[0].dwValue;

Restart:
          //---- Set the volume
          DOUBLE dValue = (*pdVolume) * mxctrl.Bounds.dwMaximum;

          //---- Play Sound at Old Volume
          PlaySound( TEXT("startup.wav"), NULL, SND_SYNC );

          pUnsigned[0].dwValue = (DWORD)dValue;

          //---- Rounds Up
          if( dValue - (DOUBLE)pUnsigned[0].dwValue > 0.5 )
               pUnsigned[0].dwValue ++;

          pUnsigned[cChannels-1].dwValue = pUnsigned[0].dwValue;

          result = mixerSetControlDetails( (HMIXEROBJ)hmx, &mxcd, MIXER_SETCONTROLDETAILSF_VALUE );
          if( result != MMSYSERR_NOERROR )
          {
               LOG(TEXT("ERROR: mixerSetControlDetails() failed with result = %d"), result);
               mixerClose( hmx );
               return TPR_ABORT;
          }


          LOG(TEXT("Volume was SET.\nvolume=%#x, max_volume=%#x\r\n"), pUnsigned[0].dwValue,  mxctrl.Bounds.dwMaximum);

          if( !fSetOrGet )
               //---- Meaning we were in this Block due to a user clicking Cancel/Retry. Now Go Back to the GetVolume Block
               goto Continue;
     }
     else
     {
          //---- Get the volume
Continue:

          result = mixerGetControlDetails( (HMIXEROBJ)hmx, &mxcd, MIXER_SETCONTROLDETAILSF_VALUE );
          if( result != MMSYSERR_NOERROR )
          {
               LOG(TEXT("ERROR: mixerGetControlDetails() failed with result = %d"), result);
               mixerClose( hmx );
               return TPR_ABORT;
          }

          LOG(TEXT("GET volume=%#x\r\n"), pUnsigned[0].dwValue);

          //---- Play Sound at Current Volume
          PlaySound( TEXT("startup.wav"), NULL, SND_SYNC );

          LOG(TEXT( "NOTICE: Changed Volume from %d.%d to %d.%d"),
                    (int)(g_oldVol*100)/100,
                    ((int)(g_oldVol*100)%100)/10,
                    (int)(pUnsigned[0].dwValue*100)/100,
                    ((int)(pUnsigned[0].dwValue*100)%100)/10 );

          if( g_interactive )
          {
               if( g_headless )
               {
                    LOG(TEXT("Running in Interactive Headless Mode."));

                    TCHAR tchC;
                    DWORD retVal = GetHeadlessUserInput(TEXT("INTERACTIVE PROMPT: Did you hear the change in volume?\n(Press 'Y' for yes, 'N' for No or 'R' for Retry)"), &tchC);
                    if( retVal != TPR_PASS )
                    {
                         LOG(TEXT("ERROR: Failed to get headless interactive user input."));
                         if( retVal == TPR_SKIP )
                              //---- Then Keyboard is not present on device
                              return TPR_SKIP;
                         else
                              return GetReturnCode( retVal, TPR_ABORT );
                    }

                    if( tchC == (TCHAR)'N' || tchC == (TCHAR)'n' )
                    {
                         LOG(TEXT( "ERROR:  User said there was no change in volume. Changed Volume from %d.%d to %d.%d"),
                                   (int)(g_oldVol*100)/100,
                                   ((int)(g_oldVol*100)%100)/10,
                                   (int)(pUnsigned[0].dwValue*100)/100,
                                   ((int)(pUnsigned[0].dwValue*100)%100)/10 );
                         tr =TPR_FAIL;
                    }
                    else if( tchC == (TCHAR)'R' || tchC == (TCHAR)'r' )
                    {
                         //---- Need to re-Set the sound to the oldValue and try this again.
                         pUnsigned[0].dwValue = g_oldVol;
                         pUnsigned[cChannels-1].dwValue = pUnsigned[0].dwValue;
                         result = mixerSetControlDetails( (HMIXEROBJ)hmx, &mxcd, MIXER_SETCONTROLDETAILSF_VALUE );
                         if( result != MMSYSERR_NOERROR )
                         {
                              LOG(TEXT("ERROR: mixersetControlDetails() failed with result = %d"), result);
                              mixerClose( hmx );
                              return TPR_ABORT;
                         }
                         goto Restart;
                    }
                    //---- else user entered 'Y', thus they heard the audio, and thus we can move on with the test
               }
               else
               {
                    LOG(TEXT("Running in Interactive Mode."));
                    HMODULE hCoreDLL = GetCoreDLLHandle();
                    if( NULL != hCoreDLL )
                    {
                         PFNMESSAGEBOX pfnMessageBox = (PFNMESSAGEBOX)GetProcAddress(hCoreDLL, TEXT("MessageBoxW"));
                         if(NULL  != pfnMessageBox)
                         {
                              iResponse = pfnMessageBox(NULL, TEXT( "Did you hear the change in volume?\n(Press Cancel to Retry)"),
                                                                    TEXT("Interactive Response "),
                                                                    MB_YESNOCANCEL|MB_ICONQUESTION);
                              switch( iResponse )
                              {
                                   case IDNO:
                                        LOG(TEXT( "ERROR:  User said there was no change in volume. Changed Volume from %d.%d to %d.%d"),
                                                  (int)(g_oldVol*100)/100,
                                                  ((int)(g_oldVol*100)%100)/10,
                                                  (int)(pUnsigned[0].dwValue*100)/100,
                                                  ((int)(pUnsigned[0].dwValue*100)%100)/10 );
                                        tr =TPR_FAIL;
                                        break;

                                   case IDCANCEL:
                                        //---- Need to re-Set the sound to the oldValue and try this again.
                                        pUnsigned[0].dwValue = g_oldVol;
                                        pUnsigned[cChannels-1].dwValue = pUnsigned[0].dwValue;
                                        result = mixerSetControlDetails( (HMIXEROBJ)hmx, &mxcd, MIXER_SETCONTROLDETAILSF_VALUE );
                                        if( result != MMSYSERR_NOERROR )
                                        {
                                             LOG(TEXT("ERROR: mixersetControlDetails() failed with result = %d"), result);
                                             mixerClose( hmx );
                                             return TPR_ABORT;
                                        }
                                        goto Restart;
                              }
                         }
                         else
                         {
                              LOG(TEXT("ERROR: pfnMessageBox is NULL, GetProcAddress failed."));
                              mixerClose( hmx );
                              return TPR_ABORT;
                         }
                         if( FALSE == FreeCoreDLLHandle(hCoreDLL) )
                         {
                              LOG(TEXT("ERROR: FreeCoreDLLHandle failed."));
                              mixerClose( hmx );
                              return TPR_ABORT; // FreeCoreDLL Handle Failed, aborting
                         }
                    }//----if( NULL != hCoreDLL )
                    else
                    {
                         //---- Could not get a handle to coredll
                         LOG(TEXT("ERROR:Failed to get a handle to coredll.dll\n"));
                         mixerClose( hmx );
                         return TPR_ABORT;
                    }
               }
          } //---- if( g_interactive )


          //---- For Interactive and Non-Interactive Tests,
          //---- try to programatically ensure that the Volume was changed as expected.
          if( g_oldVol == pUnsigned[0].dwValue )
          {
               LOG(TEXT("FAILED: Old Volume == Current Volume. The volume returned was not changed."));
               mixerClose( hmx );
               return TPR_FAIL;
          }
          //---- else Volume was changed as would be expected

          if( pUnsigned[0].dwValue > mxctrl.Bounds.dwMaximum || pUnsigned[0].dwValue < mxctrl.Bounds.dwMinimum )
          {
               LOG(TEXT("FAILED: Currently Set Volume is Not Within Advertised Boundaries."));
               mixerClose( hmx );
               return TPR_FAIL;
          }
          if( mxctrl.Bounds.dwMaximum > 0xFFFF )
          {
               LOG(TEXT("FAILED: Maximum Advertised Boundary > 0xFFFF"));
               LOG(TEXT("Suggestion: Check that your driver is not advertising a Maximum Value > 0xFFFF"));
               mixerClose( hmx );
               return TPR_FAIL;
          }
     }//---- fSetOrGet

     result = mixerClose( hmx );
     if( result != MMSYSERR_NOERROR )
     {
          LOG(TEXT("ERROR: mixClose() failed with result = %d"), result);
          return TPR_ABORT;
     }

     if( !fSetOrGet )
     {
          //---- get the volume
          *pdVolume = (DOUBLE)pUnsigned[0].dwValue/mxctrl.Bounds.dwMaximum;
          LOG(TEXT("SetOrGetVolume: get volume=%d.%d\r\n"), (int)(*pdVolume*100)/100, ((int)(*pdVolume*100)%100)/10);
     }
     return tr;
}






/*
  * Function Name: TestVolume
  *
  * Purpose: This function Tests Boundary Cases for setting and getting the volume
*/
TESTPROCAPI TestVolume( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
     BEGINTESTPROC
     DWORD tr = TPR_PASS;
     double dbl = 0.0;
     double dwInitialVolume = 0x7FFF;
     DWORD dwNumMixers = 0;

     //---- Check to make sure that there is a mixer device present on the system
     dwNumMixers = mixerGetNumDevs();
     if( 0 == dwNumMixers )
     {
          //---- No mixer devices are reported as being present on the system
          LOG(TEXT("ERROR: SKIPPING TEST: No Mixer devices reported as present by mixerGetNumDevs()"));
          LOG(TEXT("SUGGESTION: Ensure that your Audio driver correctly advertises the number of mixer devices on your system."));
          return TPR_SKIP;
     }

     //---- Save the initial volume so that it can be restored when this test is complete.
     tr = GetReturnCode( tr, SetOrGetVolume( FALSE, &dwInitialVolume ) );

CaseZero:
     //---- Case #0 Set the Volume to 0.55 in case something else changed the volume
     //----  This way we always start the volume tests from a consistent state.
     LOG(TEXT("\nCase #0: Setting Volume to 0.55 to have conistent starting volume."));
     LOG(TEXT("\nCase #0: The first ring you hear is just letting you hear the starting volume."));
     dbl = 0.55;
     tr = GetReturnCode( tr, SetOrGetVolume(TRUE, &dbl) );
     if( g_interactive )
     {
          int iResponse = IDNO;
          if( g_headless )
          {
               LOG(TEXT("Running in Interactive Headless Mode."));
               TCHAR tchC;
               DWORD retVal = GetHeadlessUserInput(TEXT( "INTERACTIVE PROMPT: This sound is just to set the sound to a consistent starting value.\nDid you hear the sound??\n(Press 'Y' for Yes, 'N' for No or 'R' for Retry)"), &tchC);
               if( retVal != TPR_PASS )
               {
                    LOG(TEXT("ERROR: Failed to get headless interactive user input."));
                    if( retVal == TPR_SKIP )
                         //---- Then Keyboard is not present on device
                         return TPR_SKIP;
                    else
                         return GetReturnCode( retVal, TPR_ABORT );
               }

               if( tchC == (TCHAR)'N' || tchC == (TCHAR)'n' )
               {
                    LOG(TEXT("ERROR:  User said there was no sound."));
                    return TPR_FAIL;
               }
               else if( tchC == (TCHAR)'R' || tchC == (TCHAR)'r' )
               {
                    //---- Try again
                    goto CaseZero;
               }
               //---- else user entered 'Y', thus they heard the audio, and thus we can move on with the test
          }
          else
          {
               LOG(TEXT("Running in Interactive Mode."));
               HMODULE hCoreDLL = GetCoreDLLHandle();
               if( NULL != hCoreDLL )
               {
                    PFNMESSAGEBOX pfnMessageBox = (PFNMESSAGEBOX)GetProcAddress(hCoreDLL, TEXT("MessageBoxW"));
                    if( NULL  != pfnMessageBox )
                    {
                         iResponse = pfnMessageBox( NULL, TEXT("This sound is just to set the sound to a consistent starting value.\nDid you hear the sound?\n(Press Cancel to Retry)"),
                                                          TEXT("Interactive Response "),
                                                          MB_YESNOCANCEL | MB_ICONQUESTION);
                         switch( iResponse )
                         {
                              case IDNO:
                                   LOG(TEXT("ERROR:  User said there was no sound."));
                                   return TPR_FAIL;
                              case IDCANCEL:
                                   goto CaseZero;
                         } // end switch
                    }
                    else
                    {
                         LOG(TEXT("ERROR: pfnMessageBox is NULL, GetProcAddress failed."));
                         return TPR_ABORT;
                    }
                    if( FALSE == FreeCoreDLLHandle(hCoreDLL) )
                    {
                         LOG(TEXT("ERROR: FreeCoreDLLHandle failed."));
                         return TPR_ABORT;
                    }
               }
               else
               {
                    LOG(TEXT("ERROR:Failed to get a handle to coredll.dll\n"));
                    return TPR_ABORT;
               }
          }
     } //---- end if  g_interactive
     LOG(TEXT("")); //---- Blank Line


     //---- Case #1 Set the Volume to a Boundary Value 1.0
     LOG(TEXT("\nCase #1: Setting Volume to 1.0"));
     dbl = 1.0;
     tr = GetReturnCode( tr, SetOrGetVolume(TRUE, &dbl) );
     tr = GetReturnCode( tr, SetOrGetVolume(FALSE, &dbl) );
     LOG(TEXT("")); //---- Blank Line

     //---- Case #2 Set the Volume to a Nominal Value 0.55
     LOG(TEXT("\nCase #2: Setting Volume to 0.55"));
     dbl = 0.55;
     tr = GetReturnCode( tr, SetOrGetVolume(TRUE, &dbl) );
     tr = GetReturnCode( tr, SetOrGetVolume(FALSE, &dbl) );
     LOG(TEXT("")); //---- Blank Line

     //---- Case #3 Set the Volume to a Boundary Value 0.1
     //---- This is very low, you may want to turn up your speakers for this one
     LOG(TEXT("\nCase #3: Setting Volume to 0.1"));
     LOG(TEXT("\nThis is very low, you may want to turn up your speakers for this one"));
     dbl = 0.1;
     tr = GetReturnCode( tr, SetOrGetVolume(TRUE, &dbl) );
     tr = GetReturnCode( tr, SetOrGetVolume(FALSE, &dbl) );
     LOG(TEXT("")); //---- Blank Line

     //---- Case #4 Set the Volume to a Boundary Value 0.9
     LOG(TEXT("\nCase #4: Setting Volume to 0.9"));
     dbl = 0.9;
     tr = GetReturnCode( tr, SetOrGetVolume(TRUE, &dbl) );
     tr = GetReturnCode( tr, SetOrGetVolume(FALSE, &dbl) );
     LOG(TEXT("")); //---- Blank Line

     //---- Case #5 Set the Volume to a Boundary Value 0.0
     LOG(TEXT("\nCase #5: Setting Volume to 0.0 (MUTED)"));
     dbl = 0.0;
     tr = GetReturnCode( tr, SetOrGetVolume(TRUE, &dbl) );
     tr = GetReturnCode (tr, SetOrGetVolume(FALSE, &dbl) );
     LOG(TEXT("")); // Blank Line

     //---- Restore Initial volume.
     tr = GetReturnCode( tr, SetOrGetVolume( TRUE, &dwInitialVolume ) );

     return tr;
}





//*****************************************************************************************************************
/*
  * Function Name: createWaveFile
  *
  * Purpose: This is a helper function for TestwaveOutSet_GetVolume()
  * It creates a wav file on disk of a specified format and buffer length.
  *
  *
*/
BOOL createWaveFile( TCHAR* pszFileName, char* pData, PCMWAVEFORMAT* ptrPCMWaveFormat,
                             DWORD dwBufferLength, DWORD & dwNumberOfBytesWritten )
{
     //----  Create a temporary wave file for DEVICE_IDD test scenario.
     CWaveFileEx cWF(    pszFileName,
                         GENERIC_READ| GENERIC_WRITE,
                         CREATE_ALWAYS,
                         FILE_ATTRIBUTE_NORMAL,
                         (LPVOID*)(ptrPCMWaveFormat),
                         NULL
                         );

     if( cWF.LastError() )
     {
          LOG( TEXT( "ERROR:\tin %s @ line %u"), TEXT( __FILE__ ), __LINE__ );
          LOG( TEXT( "\tError %d detected after constructor." ), cWF.LastError() );
          return FALSE;
     }

     if( !cWF.WriteData( pData, dwBufferLength, &dwNumberOfBytesWritten ) )
     {
          LOG( TEXT( "ERROR in %s @ line %u" ), TEXT( __FILE__ ), __LINE__ );
          LOG( TEXT( "\tError %d detected after WriteData." ), cWF.LastError() );
          cWF.Close();
          return FALSE;
     }

     if( dwBufferLength != dwNumberOfBytesWritten )
     {
          LOG( TEXT( "ERROR in %s @ line %u" ), TEXT( __FILE__ ), __LINE__ );
          LOG( TEXT( "\tUnexpected number of bytes written to WAV file." ) );
          cWF.Close();
          return FALSE;
     }

     if( !cWF.WriteData( pData, dwBufferLength, &dwNumberOfBytesWritten ) )
     {
          LOG( TEXT( "ERROR in %s @ line %u" ), TEXT( __FILE__ ), __LINE__ );
          LOG( TEXT( "\tError %d detected after second WriteData." ), cWF.LastError() );
          cWF.Close();
          return FALSE;
     }

     if( dwBufferLength != dwNumberOfBytesWritten )
     {
          LOG( TEXT( "ERROR in %s @ line %u" ), TEXT( __FILE__ ), __LINE__ );
          LOG( TEXT( "\tUnexpected number of bytes written to WAV file." ) );
          cWF.Close();
          return FALSE;
     }

     cWF.Close();
     return TRUE;
}



/*
  * Function Name: checkNextVolume
  *
  * Purpose: This is a helper function for TestwaveOutSet_GetVolume()
  *          It returns FALSE if calls to waveOutSetVolume() and waveOutGetVolume()
  *          fails with the supplied parameters, or if the volume returned by
  *          waveOutGetVolume() doesn't equal the value set by waveOutSetVolume().
  *          Returns TRUE otherwise.
*/
BOOL checkNextVolume( HWAVEOUT hwoDevice, DWORD dwNextSetVolume )
{
     DWORD dwGetVolume = 0;
     MMRESULT    mmRtn = MMSYSERR_NOERROR;

     //---- NOTE: waveOutSetVolume() can take either a device HANDLE or a device ID,
     //----   so we pass hwoDevice in this call because that changes from ID to HANDLE in the calling code.
     mmRtn = waveOutSetVolume( hwoDevice, dwNextSetVolume );
     if( MMSYSERR_NOERROR != mmRtn )
     {
          LOG( TEXT("FAIL in %s @ line %u:\twaveOutSetVolume() return code = %d."), TEXT( __FILE__ ), __LINE__, mmRtn );
          return FALSE;
     }
     mmRtn = waveOutGetVolume( hwoDevice, &dwGetVolume );
     if( MMSYSERR_NOERROR != mmRtn )
     {
          LOG( TEXT("FAIL in %s @ line %u:\twaveOutGetVolume() return code = %d."), TEXT( __FILE__ ), __LINE__, mmRtn );
          return FALSE;
     }
     if( dwGetVolume != dwNextSetVolume )
     {
          //---- waveOutGetVolume should always return exactly the same volume set by waveOutSetVolume,
          //----  even if the device doesn't support separate left and right volume control.
          LOG( TEXT("ERROR:\twaveOutSetVolume() and waveOutGetVolume() disagree.") );
          return FALSE;
     }
     LOG( TEXT( "\tVolume of device %d successfully set to %0#10X." ), hwoDevice, dwNextSetVolume );
     return TRUE;
}





/*
  * Function Name: promptUser
  *
  * Purpose: This is a helper function.
  * It prompts the user with the message in pUserPrompt (in)
  * and stores user reponse (if any) in iMessageBoxResponse (out)
  * or if headless device, in cHeadlessResponse (out).
  * The return will be one of TPR_PASS, TPR_SKIP, or TPR_ABORT.
  * Not to return TPR_FAIL because failure to get user response
  * is not to be construed as failure of the test case.
  *
  *
*/DWORD promptUser(
                         TCHAR* pUserPrompt,
                         TCHAR* pTitle,
                         DWORD dwMessageBoxType,
                         int & iMessageBoxResponse,
                         TCHAR* pHeadlessResponse
                  )
{
     DWORD dwReturn = TPR_PASS;
     HMODULE hCoreDLL = NULL;
     PFNMESSAGEBOX pfnMessageBox = NULL;

     //---- Make sure we can handle interactivity if it's specified
     if( g_interactive  &&  !g_headless )
     {
          hCoreDLL = GetCoreDLLHandle();
          if( hCoreDLL )
          {
               pfnMessageBox = (PFNMESSAGEBOX)GetProcAddress( hCoreDLL, TEXT("MessageBoxW") );
               if( !pfnMessageBox )
               {
                    LOG(TEXT("ERROR: pfnMessageBox is NULL, GetProcAddress failed."));
                    return TPR_ABORT;
               }
          }
          else
          {
               //---- Could not get a handle to coredll
               LOG(TEXT("ERROR: Could not get a handle to coredll."));
               return TPR_ABORT;
          }
     }

     if( g_headless )
     {
          dwReturn = GetHeadlessUserInput( pUserPrompt, pHeadlessResponse );
          if( dwReturn != TPR_PASS )
          {
               LOG(TEXT("ERROR: Failed to get headless interactive user input."));
               if( dwReturn == TPR_SKIP )
                    //---- Then Keyboard is not present on device
                    return TPR_SKIP;
               else
                    return GetReturnCode( dwReturn, TPR_ABORT );
          }
     }
     else
     {
          iMessageBoxResponse = pfnMessageBox( NULL, pUserPrompt, pTitle, dwMessageBoxType );
     }

     if( hCoreDLL  &&  !FreeCoreDLLHandle(hCoreDLL) )
     {
          LOG(TEXT("ERROR: FreeCoreDLLHandle failed. Aborting."));
          return TPR_ABORT;
     }

     return dwReturn;
}







/*
  * Function Name: testPlayingOfSound
  *
  * Purpose: This is a helper function for TestwaveOutSet_GetVolume()
  * It calls waveOutWrite() with the passed parameters. User can listen to result and have it try again.
  * It then calls PlaySound(), and user can again listen to result and have it try again.
  *
  *
*/DWORD testPlayingOfSound( HWAVEOUT hwo, WAVEHDR* pWH, TCHAR* pszFileName, DWORD dwExpectedPlayTime )
{
     //---- Assume success until proven otherwise
     int iResponse = IDYES;
     TCHAR tchC = (TCHAR)'Y';
     TCHAR* pPrompt = NULL;
     MMRESULT    mmRtn = MMSYSERR_NOERROR;

     //---- call waveOutWrite()
     do
     {
          //---- NOTE: waveOutWrite() can only take a device HANDLE, not a device ID,
          //----   so we must NOT use hwoDevice in this call.
          mmRtn = waveOutWrite( hwo, pWH, sizeof( WAVEHDR ) );
          if( MMSYSERR_NOERROR == mmRtn )
          {
               if( !g_interactive )
                    break;
               else
               {
                    if( g_headless )
                    {
                         pPrompt = TEXT("INTERACTIVE PROMPT: Did waveOutWrite() work on the correct speaker(s) at about the expected volume? \n(Press 'Y' for yes, 'N' for No or 'R' for Retry)");
                    }
                    else
                    {
                         pPrompt = TEXT( "Did waveOutWrite() work on the correct speaker(s) at about the expected volume? \nClick Cancel to repeat the attempt.");
                    }
                    DWORD retVal = promptUser(    pPrompt,
                                                  TEXT("Interactive Response"),
                                                  MB_YESNOCANCEL,
                                                  iResponse,
                                                  &tchC );

                    if( retVal != TPR_PASS )
                         return retVal;
                    if( IDNO == iResponse  ||  tchC == (TCHAR)'N' || tchC == (TCHAR)'n' )
                    {
                         LOG(TEXT("ERROR:  User has specified waveOutWrite() failure. "));
                         return TPR_FAIL;
                    }
                    //---- else user entered 'Y', thus they heard the audio, and thus we can exit this while loop
               }
          }
          else
          {
               LOG( TEXT("FAIL in %s @ line %u:\twaveOutWrite() failed. Returned error code = #%d." ),
                                   TEXT( __FILE__ ), __LINE__, mmRtn );
               return TPR_FAIL;
          }
     } while( IDCANCEL == iResponse  ||  tchC == (TCHAR)'R' || tchC == (TCHAR)'r' );


     //---- First, wait for the header to finish playing.
     Sleep( dwExpectedPlayTime + 250 );

     //---- Test PlaySound() volume.
     //---- Assume success until proven otherwise
     iResponse = IDYES;
     tchC = (TCHAR)'Y';
     do
     {
          if( PlaySound( pszFileName, NULL, SND_FILENAME | SND_NODEFAULT | SND_SYNC ) )
          {
               if( !g_interactive )
                    break;
               else
               {
                    if( g_headless )
                    {
                         pPrompt = TEXT("INTERACTIVE PROMPT: Did PlaySound() work on the correct speaker at about the expected volume? \n(Press 'Y' for yes, 'N' for No or 'R' for Retry)");
                    }
                    else
                    {
                         pPrompt = TEXT( "Did PlaySound() work on the correct speaker(s) at about the expected volume? \nClick Cancel to repeat the attempt.");
                    }
                    DWORD retVal = promptUser(    pPrompt,
                                                  TEXT("Interactive Response"),
                                                  MB_YESNOCANCEL,
                                                  iResponse,
                                                  &tchC );

                    if( retVal != TPR_PASS )
                         return retVal;
                    if( IDNO == iResponse  ||  tchC == (TCHAR)'N' || tchC == (TCHAR)'n' )
                    {
                         LOG(TEXT("ERROR:  User has specified PlaySound() failure. "));
                         return TPR_FAIL;
                    }
                    //---- else user entered 'Y', thus they heard the audio, and thus we can exit this while loop
               }
          }
          else
          {
               LOG( TEXT("FAIL in %s @ line %u:\tPlaySound() failed." ), TEXT( __FILE__ ), __LINE__ );
               return TPR_FAIL;
          }

     } while( IDCANCEL == iResponse  ||  tchC == (TCHAR)'R' || tchC == (TCHAR)'r' );

     return TPR_PASS;
}





/*
  * Function Name: TestwaveOutSet_GetVolume
  *
  * Purpose: This test exercises the waveOutSetVolume and waveOutGetVolume
  * functions to verify that they produce the correct results when passed valid
  * parameters, and respond appropriately otherwise.
  *
*/
TESTPROCAPI TestwaveOutSet_GetVolume( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
     BEGINTESTPROC

#define FULL_VOLUME_STEREO     0xFFFFFFFF
#define DECREMENT_VOLUME       0x10001000
#define LEFT_PASS_FILTER       0x0000FFFF
#define RIGHT_PASS_FILTER      0xFFFF0000


     if( g_bSkipOut )
          return TPR_SKIP;

     //---- The shell doesn't necessarily want us to execute the test. Make sure first.
     if( uMsg != TPM_EXECUTE )
     {
          LOG(TEXT("ERROR in TestwaveOutSet_GetVolume: Parameter uMsg != TPM_EXECUTE. uMsg passed in was: %d"), uMsg);
          return TPR_NOT_HANDLED;
     };

     //---- check for waveOut device
     if( !waveOutGetNumDevs() )
     {
          LOG( TEXT("ERROR: Skipping test, waveOutGetNumDevs reported zero devices, we need at least one.") );
          return TPR_SKIP;
     }

     enum SCENARIO       { DEVICE_IDD, DEVICE_HANDLE };
     enum SPEAKERBALANCE { STEREO, LEFTSPEAKER, RIGHTSPEAKER };

     char*          pData                    = NULL;
     DWORD          dwVolumeControl          = VOLUME_ERROR;
     DWORD          dwInitialVolume;
     DWORD          dwBufferLength           = 0;
     DWORD          dwExpectedPlayTime       = 1000; // in milliseconds
     DWORD          dwNumberOfBytesWritten   = 0;
     DWORD          dwReturn                 = TPR_PASS;
     DWORD          dwScenario               = DEVICE_IDD;
     DWORD          dwWaveOutDeviceID        = g_dwOutDeviceID;
     HWAVEOUT       hwo                      = NULL;
     HWAVEOUT       hwoDevice                = NULL;
     MMRESULT       mmRtn                    = MMSYSERR_NOERROR;

     PCMWAVEFORMAT  PcmWaveFormat            = {{ WAVE_FORMAT_PCM, CHANNELS, SAMPLESPERSEC, AVGBYTESPERSEC, BLOCKALIGN }, BITSPERSAMPLE };
     WAVEFORMATEX wfx                        = { WAVE_FORMAT_PCM, CHANNELS, SAMPLESPERSEC, AVGBYTESPERSEC, BLOCKALIGN, BITSPERSAMPLE, SIZE };
     WAVEHDR wh;


     //---- Set up an audio buffer for test.
     dwBufferLength = dwExpectedPlayTime * wfx.nAvgBytesPerSec / 1000;
     pData = new char[ dwBufferLength ];
     if( !pData )
     {
          LOG( TEXT( "ERROR:\tNew failed for data [%s:%u]" ), TEXT( __FILE__ ), __LINE__ );
          LOG( TEXT( "\tPossible Cause:  Out of Memory\n" ) );
          return TPR_ABORT;
     }

     ZeroMemory( pData, dwBufferLength );
     ZeroMemory( &wh, sizeof( WAVEHDR ) );

     //---- Check for a buffer that is smaller than we expected.
     if( dwExpectedPlayTime >= ( ULONG_MAX / wfx.nAvgBytesPerSec ) )
     {
          LOG( TEXT( "FAIL in %s @ line %u:" ), TEXT( __FILE__ ), __LINE__ );
          LOG( TEXT( "\tPotential overflow, dwExpectedPlayTime = %d ms."), dwExpectedPlayTime );
          delete [] pData;
          pData = NULL;
          return TPR_ABORT;
     }
     PREFAST_SUPPRESS(419, "The above line is checking for overflow. This seems to be prefast noise, since the path/trace Prefast lists is not possible. (Prefast team concurs so far.)");

     wh.lpData         = pData;
     wh.dwBufferLength = dwBufferLength;
     wh.dwLoops        = 1;
     wh.dwFlags        = 0;

     if( !SineWave( wh.lpData, wh.dwBufferLength, &wfx ) )
     {
          LOG( TEXT( "FAIL in %s @ line %u:\tSineWave returned a buffer of length zero." ), TEXT( __FILE__ ), __LINE__ );
          delete [] pData;
          pData = NULL;
          return TPR_ABORT;
     }

     //----  Create a temporary wave file for DEVICE_ID test scenario.
     TCHAR* pszFileName = TEXT( "\\Temp\\TestwaveOutSet_GetVolume.wav" );
     if( !createWaveFile( pszFileName, pData, &PcmWaveFormat, dwBufferLength, dwNumberOfBytesWritten ) )
     {
          LOG( TEXT( "ABORT in %s @ line %u:\tcreateWaveFile(...) returned with error." ), TEXT( __FILE__ ), __LINE__ );
          delete[] pData;
          pData = NULL;
          //---- the logging of this error will have been done in createWaveFile()
          return TPR_ABORT;
     }

     //---- Check what kind of volume adjustment the driver supports.
     dwVolumeControl = GetVolumeControl( dwWaveOutDeviceID );
     if( VOLUME_ERROR == dwVolumeControl )
     {
          LOG( TEXT( "FAIL in %s @ line %u:\tGetVolumeControl(...) returned with error." ), TEXT( __FILE__ ), __LINE__ );
          delete [] pData;
          pData = NULL;
          //---- the logging of this error will have been done in GetVolumeControl()
          return TPR_FAIL;
     }

     //---- If interactive mode, give the user information on this test.
     if( g_interactive)
     {
          TCHAR tchC = 'Y';
          TCHAR* pPrompt = NULL;
          int iResponse = IDOK;
          if( g_headless )
          {
               //---- The TEXT() macro doesn't like concatenated strings, so this string is stretched out
               pPrompt = TEXT("This test will go from full volume to zero volume in 16 steps, first in stereo, then left speaker, then right speaker. SET YOUR SPEAKER VOLUME TO MID-LEVEL. Press 'Y' to continue, 'N' to abort.");
          }
          else
          {
               //---- The TEXT() macro doesn't like concatenated strings, so this string is stretched out
               pPrompt = TEXT("This test will start with full stereo volume and go to zero volume in 16 steps. It will then repeat this cycle for the left speaker, followed by the same cycle for the right speaker. \nSet your speaker volume level to mid-level so you can hear the full range. After each output to the speakers, you will be asked if the sound played properly.\n\nClick YES to continue, NO to abort this test.");
          }
          DWORD retVal = promptUser(    pPrompt,
                                        TEXT("Interactive Response"),
                                        MB_YESNO,
                                        iResponse,
                                        &tchC );

          if( retVal != TPR_PASS )
          {
               delete [] pData;
               pData = NULL;
               return retVal;
          }
          if( IDNO == iResponse  ||  tchC == (TCHAR)'N' || tchC == (TCHAR)'n' )
          {
               delete [] pData;
               pData = NULL;
               LOG(TEXT("User has aborted this test."));
               return TPR_SKIP;
          }
     }

     //---- Create a waveform output device handle for DEVICE_HANDLE test scenario. Open the Wave Output Stream.
     mmRtn = waveOutOpen( &hwo, dwWaveOutDeviceID, &wfx, NULL,0, CALLBACK_NULL );
     if( MMSYSERR_NOERROR != mmRtn )
     {
          LOG( TEXT( "FAIL in %s @ line %u:\twaveOutOpen returned error code, #%d." ), TEXT( __FILE__ ), __LINE__, mmRtn );
          delete [] pData;
          pData = NULL;
          return TPR_FAIL;
     }

     BOOL bWaveOutPrepared = FALSE;

     //---- Save the initial volume so that it can be restored when this test is complete.
     mmRtn = waveOutGetVolume( (HWAVEOUT)dwWaveOutDeviceID, (LPDWORD) &dwInitialVolume );
     if( MMSYSERR_NOERROR != mmRtn )
     {
          LOG( TEXT( "FAIL in %s @ line %u:\twaveOutGetVolume returned error code, #%d." ), TEXT( __FILE__ ), __LINE__, mmRtn );
          delete [] pData;
          pData = NULL;
          return TPR_FAIL;
     }


     for( dwScenario = DEVICE_IDD; dwScenario <= DEVICE_HANDLE; dwScenario++ )
     {
          if( DEVICE_IDD == dwScenario )
               hwoDevice = (HWAVEOUT)dwWaveOutDeviceID;
          else
               hwoDevice = hwo;

          //---- Only prepare the header once for the entire test.
          if( !bWaveOutPrepared )
          {
               //---- NOTE: waveOutPrepareHeader() can only take a device HANDLE, not a device ID,
               //----   so we always pass hwo in this call
               mmRtn = waveOutPrepareHeader( hwo, &wh, sizeof( wh ) );
               if( MMSYSERR_NOERROR != mmRtn )
               {
                    LOG( TEXT("ERROR:\twaveOutPrepareHeader() failed. Returned error code = #%d."), mmRtn );
                    dwReturn = TPR_FAIL;
                    break;
               }
               else
                    bWaveOutPrepared = TRUE;
          }
          if( DEVICE_IDD == dwScenario )
               LOG( TEXT("Scenario %d: testing %s %d."), dwScenario + 1, TEXT("device ID"), hwoDevice );
          else
          {
               LOG( TEXT("Scenario %d: testing %s %d."), dwScenario + 1, TEXT("device HANDLE"), hwoDevice );

               //---- We are just starting the HANDLE testing.
               //----  This means the volume for the DEVICE ID is at 0.
               //----   And this means we should be able to set volume using HANDLE to maximum
               //----   but the DEVICE ID setting should override that, so no volume should be heard.
               //----   We need to verify this with just one test, then return to cycling through the 16 levels of volume.
               //---- If interactive mode, give the user information on this test.
               if( g_interactive)
               {
                    TCHAR tchC = 'Y';
                    TCHAR* pPrompt = NULL;
                    int iResponse = IDOK;
                    if( g_headless )
                    {
                         //---- The TEXT() macro doesn't like concatenated strings, so this string is stretched out
                         pPrompt = TEXT("Volume was set to min using Device ID. Setting to max using HANDLE. For this single test, you should hear no or very little sound at high speaker volume. Press 'Y' to continue, 'N' to abort.");
                    }
                    else
                    {
                         //---- The TEXT() macro doesn't like concatenated strings, so this string is stretched out
                         pPrompt = TEXT("Volume was set to min using Device ID. Setting to max using HANDLE. For this single test, you should hear only the slightest trace of sound at high speaker volume.\n\nClick YES to continue, NO to abort this test.");
                    }
                    DWORD retVal = promptUser(    pPrompt,
                                                  TEXT("Interactive Response"),
                                                  MB_YESNO,
                                                  iResponse,
                                                  &tchC );

                    if( retVal != TPR_PASS )
                    {
                         delete [] pData;
                         pData = NULL;
                         return retVal;
                    }
                    if( IDNO == iResponse  ||  tchC == (TCHAR)'N' || tchC == (TCHAR)'n' )
                    {
                         delete [] pData;
                         pData = NULL;
                         LOG(TEXT("User has aborted this test."));
                         return TPR_SKIP;
                    }
               }

               DWORD dwNextVolume = FULL_VOLUME_STEREO;          //0xFFFFFFFF

               //---- Here hwoDevice is the device HANDLE, to be passed to the waveOutSetVolume() call.
               //----  Even though it's setting to full volume, the device ID volume of 0 should override it.
               if( !checkNextVolume( hwoDevice, dwNextVolume ) )
               {
                    dwReturn = TPR_FAIL;
                    break;
               }

               DWORD dwTemp = testPlayingOfSound( hwo, &wh, pszFileName, dwExpectedPlayTime );
               dwReturn = GetReturnCode( dwReturn, dwTemp );
               if( TPR_PASS != dwReturn )
               {
                    if( TPR_FAIL == dwReturn )
                         LOG( TEXT( "Volume of 0 set with Device ID did not override volume > 0 set with Device Handle." ) );
                    break;
               }

               if( g_interactive)
               {
                    TCHAR tchC = 'Y';
                    TCHAR* pPrompt = NULL;
                    int iResponse = IDOK;
                    if( g_headless )
                    {
                         //---- The TEXT() macro doesn't like concatenated strings, so this string is stretched out
                         pPrompt = TEXT("Restoring max volume using Device ID. We now repeat test of 16 levels of stereo, left, and right speaker volume. SET YOUR SPEAKER VOLUME TO MID-LEVEL. Press 'Y' to continue, 'N' to abort.");
                    }
                    else
                    {
                         //---- The TEXT() macro doesn't like concatenated strings, so this string is stretched out
                         pPrompt = TEXT("Restoring max volume using Device ID. We now repeat test of 16 levels of stereo, left, and right speaker volume. SET YOUR SPEAKER VOLUME TO MID-LEVEL.\n\nClick YES to continue, NO to abort this test.");
                    }
                    DWORD retVal = promptUser(    pPrompt,
                                                  TEXT("Interactive Response"),
                                                  MB_YESNO,
                                                  iResponse,
                                                  &tchC );

                    if( retVal != TPR_PASS )
                    {
                         delete [] pData;
                         pData = NULL;
                         return retVal;
                    }
                    if( IDNO == iResponse  ||  tchC == (TCHAR)'N' || tchC == (TCHAR)'n' )
                    {
                         delete [] pData;
                         pData = NULL;
                         LOG(TEXT("User has aborted this test."));
                         return TPR_SKIP;
                    }
               }

               //---- We pass the device ID here before cycling through setting volume levels with device HANDLE,
               if( !checkNextVolume( (HWAVEOUT)dwWaveOutDeviceID, dwNextVolume ) )
               {
                    dwReturn = TPR_FAIL;
                    break;
               }

          }

          //---- In this for() loop, we alternate between stereo, left, and right volume adjustments
          for( unsigned int ui = STEREO; ui <= RIGHTSPEAKER; ui++ )
          {
               //---- start from full volume and descend
               DWORD dwNextVolume = FULL_VOLUME_STEREO;          //0xFFFFFFFF
               DWORD dwDecrementVolume = DECREMENT_VOLUME;       //0X10001000

               if( STEREO != ui  &&  SEPARATE != dwVolumeControl )
                    //---- no separate volume controls, exit after stereo volume adjustments
                    break;

               else if( LEFTSPEAKER == ui      &&   SEPARATE == dwVolumeControl )
               {
                    dwNextVolume &= LEFT_PASS_FILTER;
                    dwDecrementVolume &= LEFT_PASS_FILTER;
               }

               else if( RIGHTSPEAKER == ui     &&   SEPARATE == dwVolumeControl )
               {
                    dwNextVolume &= RIGHT_PASS_FILTER;
                    dwDecrementVolume &= RIGHT_PASS_FILTER;
               }

               while( 0 <= dwNextVolume )
               {
                    //---- We pass hwoDevice, which is either the device ID or the device HANDLE,
                    //----  to be passed to the waveOutSetVolume() call.
                    if( !checkNextVolume( hwoDevice, dwNextVolume ) )
                    {
                         dwReturn = TPR_FAIL;
                         break;
                    }

                    DWORD dwTemp = testPlayingOfSound( hwo, &wh, pszFileName, dwExpectedPlayTime );
                    dwReturn = GetReturnCode( dwReturn, dwTemp );
                    if( TPR_PASS != dwReturn )
                         break;

                    if( dwNextVolume > dwDecrementVolume )
                         dwNextVolume -= dwDecrementVolume;
                    else
                         break;
               }

               if( TPR_PASS != dwReturn )
                    break;

          } //---- end for()

          if( dwReturn != TPR_PASS )
               //---- If any scenario failed or aborted, don't do the subsequent scenario(s)
               break;

     } //---- for( dwScenario )

     //---- Clean up waveOut
     Sleep( dwExpectedPlayTime + 250 );

     if( bWaveOutPrepared )
     {
          //---- NOTE: waveOutUnprepareHeader() can only take a device HANDLE, not a device ID,
          //----   so we always pass hwo in this call
          mmRtn = waveOutUnprepareHeader( hwo, &wh, sizeof( wh ) );
          if( MMSYSERR_NOERROR != mmRtn )
          {
               LOG( TEXT("ERROR:\twaveOutUnprepareHeader() failed. Returned error code = #%d."), mmRtn );
               dwReturn = TPR_FAIL;
          }
     }

     //---- Restore Initial volume.
     mmRtn = waveOutSetVolume( (HWAVEOUT)dwWaveOutDeviceID, dwInitialVolume );
     if( MMSYSERR_NOERROR != mmRtn )
     {
          LOG( TEXT( "FAIL in %s @ line %u:\twaveOutSetVolume returned error code, #%d." ), TEXT( __FILE__ ), __LINE__, mmRtn );
          delete [] pData;
          pData = NULL;
          return TPR_FAIL;
     }

     //---- Must pass a HANDLE here, not a device ID
     mmRtn = waveOutClose( hwo );
     if( MMSYSERR_NOERROR != mmRtn )
     {
          LOG( TEXT("ERROR:\twaveOutClose() failed. Returned error code = #%d."), mmRtn );
          dwReturn = TPR_FAIL;
     }

     delete[] pData;
     pData = NULL;
     return dwReturn;
} //---- TestwaveOutSet_GetVolume


//*****************************************************************************************************************








TESTPROCAPI PlaybackExtended(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
     BEGINTESTPROC
     DWORD tr=TPR_PASS;
     DWORD itr;
     DWORD i,j,m;
     WAVEOUTCAPS woc;
     MMRESULT mmRtn;
     DWORD hrReturn;
     DWORD dwRestart;
     int iResponse;

     if(g_bSkipOut)
          return TPR_SKIP;

     LOG(TEXT("INSTRUCTIONS:  This test case will attempt to playback a tone for using extended functions"));
     LOG(TEXT("INSTRUCTIONS:  Please listen to playback to ensure that a tone is played and changed"));
     LOG(TEXT("INSTRUCTIONS:  When Pitch is changed, the pitch should change accordingly"));
     LOG(TEXT("INSTRUCTIONS:  When Playback rate is changed, the tone will change because the waveform is played at a different rate"));
     LOG(TEXT("INSTRUCTIONS:     Also, the expected play time of the buffer will also change"));
     LOG(TEXT("INSTRUCTIONS:  When Volume is changed, the tone is muted on the left while the right channel volume decreases"));
     LOG(TEXT("INSTRUCTIONS:     Then the right channel is muted while the left channel volume decreases"));

     mmRtn = waveOutGetDevCaps( g_dwOutDeviceID, &woc, sizeof(woc) );
     CheckMMRESULT( mmRtn, "ERROR:  Failed to get device caps.  waveOutGetDevCaps", TPR_FAIL, "Driver responded incorrectly");

     for( i = 0; lpFormats[i].szName; i++ )
          if( woc.dwFormats&lpFormats[i].value )
               break;
          if( !lpFormats[i].szName )
          {
               LOG(TEXT("ERROR:  There are no supported lpFormats"));
               return TPR_SKIP;
          }

     for( j = 0, m = 0, dwRestart = 0;   lpExtensions[j].szName;   j++ )
     {
Restart:
          hrReturn = MMSYSERR_NOERROR;
          itr = PlayWaveFormat( lpFormats[i].szName, g_duration, CALLBACK_NULL, &lpExtensions[j].value, &hrReturn );
          if( !lpExtensions[j].value.bContinue )
          {
               if( itr==TPR_PASS )
               {
                    if( g_interactive )
                    {
                         if( g_headless )
                         {
                              LOG(TEXT("Running in Interactive Headless Mode."));
                              TCHAR tchC;
                              DWORD retVal = GetHeadlessUserInput((TEXT("INTERACTIVE PROMPT: %s\n(Press 'Y' for Yes, \
                                                                   'N' for No or 'R' for Retry)"), extensionMessages[m]), &tchC);
                              if( retVal != TPR_PASS )
                              {
                                   LOG(TEXT("ERROR: Failed to get headless interactive user input."));
                                   if( retVal == TPR_SKIP ) // Then Keyboard is not present on device
                                        return TPR_SKIP;
                                   else
                                        return GetReturnCode( retVal, TPR_ABORT );
                              }

                              if( tchC == (TCHAR)'N' || tchC == (TCHAR)'n' )
                              {
                                   LOG(TEXT("ERROR:  User said there was no difference in the tone %s"),lpExtensions[j].szName);
                                   tr |= TPR_FAIL;
                              }
                              else if( tchC == (TCHAR)'R' || tchC == (TCHAR)'r' )
                              {
                                   j = dwRestart;
                                   goto Restart;
                              }
                              //---- else user entered 'Y', thus they heard the audio, and thus we can move on with the test
                         }
                         else
                         {
                              LOG(TEXT("Running in Interactive Mode."));
                              HMODULE hCoreDLL = GetCoreDLLHandle();
                              if( NULL != hCoreDLL )
                              {
                                   PFNMESSAGEBOX pfnMessageBox = (PFNMESSAGEBOX)GetProcAddress(hCoreDLL, TEXT("MessageBoxW"));
                                   if( NULL  != pfnMessageBox )
                                   {
                                        iResponse = pfnMessageBox( NULL, extensionMessages[m],
                                                            TEXT("Interactive Response"), MB_YESNOCANCEL | MB_ICONQUESTION);
                                        switch( iResponse )
                                        {
                                             case IDNO:
                                                  LOG(TEXT("ERROR:  User said there was no difference in the tone %s"),
                                                       lpExtensions[j].szName);
                                                  tr |= TPR_FAIL;
                                                  break;
                                             case IDCANCEL:
                                                  j = dwRestart;
                                                  goto Restart;
                                        } // end switch
                                   }
                                   else
                                   {
                                        LOG(TEXT("ERROR: pfnMessageBox is NULL, GetProcAddress failed."));
                                        return TPR_ABORT;
                                   }
                                   if( FALSE == FreeCoreDLLHandle(hCoreDLL) )
                                   {
                                        LOG(TEXT("ERROR: FreeCoreDLLHandle failed."));
                                        return TPR_ABORT; // FreeCoreDLL Handle Failed, aborting
                                   }
                              }
                              else
                              {
                                   //---- Could not get a handle to coredll
                                   LOG(TEXT("ERROR:Failed to get a handle to coredll.dll\n"));
                                   return TPR_ABORT;
                              }
                         }
                    }
                    else
                         tr |= itr;
               }
               m++;
               dwRestart = j+1;
          }
          if( woc.dwSupport&lpExtensions[j].value.flag )
          {
               if( itr == TPR_FAIL )
                    LOG(TEXT("ERROR:  waveOutGetDevCaps reports %s is supported, but %s was returned"),
                              lpExtensions[j].szName, g_ErrorMap[hrReturn]);
          }
          else if( itr == TPR_PASS )
          {
               LOG(TEXT("ERROR:  waveOutGetDevCaps reports %s is unsupported, but MMSYSERR_UNSUPPORTED was not returned"),lpExtensions[j].szName);
               itr = TPR_FAIL;
          }
          else
          {
            itr = TPR_PASS;
          }
          tr |= itr;
     }
Error:
     return tr;
}


/*
  * PlayAudio(DWORD audioType)
  * Helper function for PowerUpAndDown
  *
  * Argument:
  *  audioType == DWORDs defined in test_wavetest.h
  *  Currently they are: PLAYSOUND_SYNC, PLAYSOUND_ASYNC
  *  SNDPLAYSOUND_SYNC, SNDPLAYSOUND_ASYNC and WAVEOUT
  */
TESTPROCAPI PlayAudio(DWORD audioType)
{
     DWORD dwRet = TPR_PASS;
     DWORD dwRt = 0;

     //---- For use with waveOut*
     WAVEHDR wh;
     wh.lpData = NULL;
     UINT n = 0;
     HRESULT hResult;
     LPWAVEFORMATEX pwfx;
     CWaveFile WaveFile;
     DWORD nBytes = 0;
     MMRESULT mmRtn;
     HWAVEOUT hwo;

     switch( audioType )
     {
          case PLAYSOUND_SYNC:
               //---- Use Play  Sound to exercise the WaveAPI
               if( FALSE == PlaySound(TEXT("startup.wav"), NULL, SND_SYNC) )
                    dwRet = TPR_FAIL;
               break;
          case PLAYSOUND_ASYNC:
               //---- Use Play  Sound to exercise the WaveAPI
               if( FALSE == PlaySound(TEXT("startup.wav"), NULL, SND_ASYNC) )
                    dwRet = TPR_FAIL;
               break;
          case SNDPLAYSOUND_SYNC:
               //---- Use SndPlaySound to exercise the WaveAPI
               if( FALSE == sndPlaySound(TEXT("startup.wav"), SND_SYNC) )
                    dwRet = TPR_FAIL;
               break;
          case SNDPLAYSOUND_ASYNC:
               //---- Use SndPlaySound to exercise the WaveAPI
               if( FALSE == sndPlaySound(TEXT("startup.wav"), SND_ASYNC) )
                    dwRet = TPR_FAIL;
               break;
          case WAVEOUT:
               //---- Use WaveOutWrite to exercise the WaveAPI
               ZeroMemory( &wh,sizeof(wh) );
               //---- See how many waveOut devices are registered in the system.
               n = waveOutGetNumDevs();
               if(0 == n)
               {
                    LOG(TEXT( "ERROR: waveOutGetNumDevs reported zero devices, we need at least one."));
                    dwRet = TPR_FAIL;
                    goto endOfFunction;
               }

               hResult = WaveFile.Create(    TEXT("\\windows\\startup.wav"),
                                             GENERIC_READ,
                                             OPEN_EXISTING,
                                             0,
                                             (LPVOID*)&pwfx,
                                             NULL);
               if( ERROR_SUCCESS != hResult )
               {
                    LOG(TEXT("ERROR: Could not Create/Open startup.wav"));
                    dwRet = TPR_FAIL;
                    goto endOfFunction;
               }
               wh.dwBufferLength = WaveFile.WaveSize();
               if( wh.dwBufferLength < UINT_MAX )
               {
                    wh.lpData = new char[wh.dwBufferLength];
                    if( !wh.lpData )
                    {
                         LOG(TEXT("ERROR: Could not allocate memory for wav data."));
                         dwRet = TPR_ABORT;
                         goto endOfFunction;
                    }
               }
               else
               {
                    LOG(TEXT("ERROR: wh.dwBufferLength >= UINT_MAX "));
                    dwRet = TPR_FAIL;
                    goto endOfFunction;
               }
               WaveFile.ReadData( wh.lpData, wh.dwBufferLength, &nBytes );
               if( nBytes != wh.dwBufferLength )
               {
                    LOG(TEXT("ERROR: Could not read entire file"));
                    dwRet = TPR_FAIL;
                    goto endOfFunction;
               }
               //---- Open the Wave Output Stream
               mmRtn = waveOutOpen( &hwo, g_dwOutDeviceID, pwfx, NULL, 0, CALLBACK_NULL);
               if( MMSYSERR_NOERROR != mmRtn )
               {
                    LOG(TEXT("ERROR: waveOutOpen failed with return code %d"), mmRtn);
                    dwRet = TPR_FAIL;
                    goto endOfFunction;
               }
               //---- Prepare our header
               mmRtn = waveOutPrepareHeader( hwo, &wh, sizeof(wh) );
               if( MMSYSERR_NOERROR != mmRtn )
               {
                    LOG(TEXT("ERROR: waveOutPrepareHeader failed with return code %d"), mmRtn);
                    dwRet = TPR_FAIL;
                    goto cleanupThenEndOfFunction;
               }
               //---- Write out the header to the waveform audio output stream
               mmRtn = waveOutWrite( hwo, &wh, sizeof(wh) );
               if( MMSYSERR_NOERROR != mmRtn )
               {
                    LOG(TEXT("ERROR: waveOutWrite failed with return code %d"), mmRtn);
                    dwRet = TPR_FAIL;
                    goto cleanupThenEndOfFunction;
               }
               //---- wait for the header to be processed
               do
               {
                    Sleep(500);
               } while( !(wh.dwFlags&WHDR_DONE) );

cleanupThenEndOfFunction:
               //----unprepare the header
               mmRtn = waveOutUnprepareHeader( hwo, &wh, sizeof(wh) );
               if( MMSYSERR_NOERROR != mmRtn )
               {
                    LOG(TEXT("ERROR: waveOutUnprepareHeader failed with return code %d"), mmRtn);
                    dwRet = TPR_FAIL;
               }
               //---- close the waveform output stream
               mmRtn = waveOutClose( hwo );
               if (MMSYSERR_NOERROR != mmRtn )
               {
                    LOG(TEXT("ERROR: waveOutClose failed with return code %d"), mmRtn);
                    dwRet = TPR_FAIL;
                    goto endOfFunction;
               }

endOfFunction:
               if( wh.lpData )
                    delete wh.lpData;
               WaveFile.Close();

               break;
          default:
               LOG(TEXT("Unknown Test Type passed to PlayAudio. "));
               dwRet =  TPR_ABORT;

     }//---- end switch()
     return dwRet;
}






/*
  * PowerDownAndUp(DWORD audioType)
  * Helper function for TestPowerUpAndDown
  *
  * Parameter audioType == SNDPLAYSOUND_SYNC || SNDPLAYSOUND_ASYNC ||
  *                                      PLAYSOUND_SYNC || PLAYSOUND_ASYNC || WAVEOUT
  */
TESTPROCAPI PowerDownAndUp( DWORD audioType )
{
     DWORD dwRet = TPR_PASS;
     DWORD rt = 0;
     int iResponse = 0;
     HANDLE hThread = NULL;
     DWORD dwCallback = 0;

RestartFirstAudio:
     //---- Play a sound to demonstrate that the system audio is working properly
     dwRet = GetReturnCode( dwRet, PlayAudio(audioType) );
     if(dwRet != TPR_PASS)
     {
         goto Error;
     }

     if( g_interactive )
     {
          if( g_headless )
          {
               LOG(TEXT("Running in Interactive Headless Mode."));

               TCHAR tchC;
               DWORD retVal = GetHeadlessUserInput(TEXT("INTERACTIVE PROMPT: Did you hear the WAVE file playback?\n\
                                                        (Press 'Y' for Yes, 'N' for No or 'R' for Retry)"), &tchC);
               if( retVal != TPR_PASS )
               {
                    LOG(TEXT("ERROR: Failed to get headless interactive user input."));
                    if( retVal == TPR_SKIP )
                         //---- Then Keyboard is not present on device
                         return TPR_SKIP;
                    else
                         return GetReturnCode( retVal, TPR_ABORT );
               }

               if( tchC == (TCHAR)'N' || tchC == (TCHAR)'n' )
               {
                    LOG(TEXT("ERROR:  User said there was no Audio Playback before entering SUSPEND state.") );
                    return TPR_FAIL;
               }
               else if( tchC == (TCHAR)'R' || tchC == (TCHAR)'r' )
               {
                    //---- Need to re-play the sound again
                    goto RestartFirstAudio;
               }
               //---- else user entered 'Y', thus they heard the audio, and thus they can move on with the test
          }
          else
          {
               LOG(TEXT("Running in Interactive Mode."));

               HMODULE hCoreDLL = GetCoreDLLHandle();
               if(NULL != hCoreDLL)
               {
                    PFNMESSAGEBOX pfnMessageBox = (PFNMESSAGEBOX)GetProcAddress(hCoreDLL, TEXT("MessageBoxW"));
                    if(NULL  != pfnMessageBox)
                    {
                         iResponse = pfnMessageBox(NULL, TEXT("Did you hear the WAVE file playback?\n(Press Cancel to Retry)"), TEXT("Interactive Response "),MB_YESNOCANCEL|MB_ICONQUESTION);
                         switch( iResponse )
                         {
                              case IDNO:
                                   LOG(TEXT("ERROR:  User said there was no Audio Playback before entering SUSPEND state.") );
                                   FreeCoreDLLHandle( hCoreDLL );
                                   return TPR_FAIL;
                              case IDCANCEL:
                                   FreeCoreDLLHandle( hCoreDLL );
                                   //---- Need to re-play the sound again
                                   goto RestartFirstAudio;
                         }
                    }
                    else
                    {
                         LOG(TEXT("ERROR: pfnMessageBox is NULL, GetProcAddress failed."));
                         FreeCoreDLLHandle( hCoreDLL );
                         return TPR_ABORT;
                    }
               }
               else
               {
                    // Could not get a handle to coredll
                    LOG(TEXT("ERROR:Failed to get a handle to coredll.dll\n"));
                    return TPR_ABORT;
               }
               FreeCoreDLLHandle( hCoreDLL );
          }
     } //---- if( g_interactive )

     //---- Sleep 1 second to make sure any interupts from the MsgBox or Keyboard are fully processed
     Sleep(1000);

     //---- Setup the wakeup resource so that after being in suspended state for 30 seconds, the device resumes
     //---- NOTE: Do not set the timer for less than 20-25 seconds, there have been some known power management
     //---- problems if you set the time for < 20 seconds.

     //---- Prepare RTC wakeup resource...
     if( FALSE == SetupRTCWakeupResource (30) )
     {
          //---- will wakeup 30 sec later
          LOG(TEXT("ERROR: Can not setup RTC alarm as the wakeup resource"));
          LOG(TEXT("SKIPPING TEST: We cannot resume from the suspended state without RTC wakeup resource."));
          return TPR_SKIP;
     }

     //---- Now Power Down the System and enter the SUSPEND state.
     //---- When a device is asked to suspend, it is being asked to remain powered to the point that RAM is in
     //---- a self-refresh state where an interrupt can wake the device.
     LOG(TEXT("Now try to bring system into SUSPEND state..."));
     HMODULE hCoreDLL = GetCoreDLLHandle();
     if( NULL != hCoreDLL )
     {
          PFNSETSYSTEMPOWERSTATE pfnSetSystemPowerState = (PFNSETSYSTEMPOWERSTATE)GetProcAddress(hCoreDLL, TEXT("SetSystemPowerState"));
          if( NULL  != pfnSetSystemPowerState )
          {
               if( (rt = pfnSetSystemPowerState(TEXT("Suspend"), NULL, POWER_FORCE)) != ERROR_SUCCESS )
               {
                    LOG(TEXT("ERROR: SetSystemPowerState failed! Ret = 0x%x"), rt);
                    FreeCoreDLLHandle(hCoreDLL);
                    ReleaseRTCWakeupResource();
                    return TPR_FAIL;
               }
          }
          else
          {
               LOG(TEXT("ERROR: pfnSetSystemPowerState is NULL, GetProcAddress failed."));
               LOG(TEXT("SUGGESTION: Ensure that power management is present in this OS image."));
               ReleaseRTCWakeupResource();
               FreeCoreDLLHandle(hCoreDLL);
               return TPR_SKIP;
          }
          if(FALSE == FreeCoreDLLHandle(hCoreDLL))
          {
               LOG(TEXT("ERROR: FreeCoreDLLHandle failed."));
               ReleaseRTCWakeupResource();
               return TPR_ABORT;
          }
     }
     else
     {
          //---- Could not get a handle to coredll
          LOG(TEXT("ERROR:Failed to get a handle to coredll.dll\n"));
          return TPR_ABORT;
     }

     LOG(TEXT("System has waken up from the '%s' power state"), POWER_STATE_NAME_SUSPEND);
     ReleaseRTCWakeupResource();

     //---- pull the system state to On for possible next test case
     hCoreDLL = GetCoreDLLHandle();
     if( NULL != hCoreDLL )
     {
          PFNSETSYSTEMPOWERSTATE pfnSetSystemPowerState =
               (PFNSETSYSTEMPOWERSTATE)GetProcAddress(hCoreDLL, TEXT("SetSystemPowerState"));
          if( NULL  != pfnSetSystemPowerState )
          {
               if( (rt = pfnSetSystemPowerState(TEXT("On"), NULL, POWER_FORCE)) != ERROR_SUCCESS )
               {
                    LOG(TEXT("ERROR: In %s @ %d: SetSystemPowerState failed! Ret = 0x%x"),  __FILE__, __LINE__, rt);
                    return TPR_FAIL;
               }
          }
          else
          {
               LOG(TEXT("ERROR: pfnSetSystemPowerState is NULL, GetProcAddress failed."));
               LOG(TEXT("SUGGESTION: Ensure that power management is present in this OS image."));
               return TPR_SKIP;
          }
          if( FALSE == FreeCoreDLLHandle(hCoreDLL) )
          {
               LOG(TEXT("ERROR: FreeCoreDLLHandle failed."));
               return TPR_ABORT;
          }
     }
     else
     {
          //---- Could not get a handle to coredll
          LOG(TEXT("ERROR:Failed to get a handle to coredll.dll\n"));
          return TPR_ABORT;
     }
     FreeCoreDLLHandle(hCoreDLL);


RestartSecondAudio:
     LOG(TEXT("Now trying to play startup.wav again after powering back on. "));
     GetReturnCode( dwRet, PlayAudio(audioType) );

     if( g_interactive )
     {
          if( g_headless )
          {
               TCHAR tchC;
               DWORD retVal = GetHeadlessUserInput(TEXT("INTERACTIVE PROMPT: Did you hear the WAVE file playback?\n\
                                                        (Press 'Y' for Yes, 'N' for No or 'R' for Retry)"), &tchC);
               if(retVal != TPR_PASS )
               {
                    LOG(TEXT("ERROR: Failed to get headless interactive user input."));
                    if(retVal == TPR_SKIP)
                         //---- Then Keyboard is not present on device
                         return TPR_SKIP;
                    else
                         return GetReturnCode( retVal, TPR_ABORT );
               }

               if( tchC == (TCHAR)'N' || tchC == (TCHAR)'n' )
               {
                    LOG(TEXT("ERROR:  User said there was no Audio Playback after returning from the SUSPEND state.") );
                    return TPR_FAIL;
               }
               else if( tchC == (TCHAR)'R' || tchC == (TCHAR)'r' )
               {
                    //---- Need to re-play the sound again
                    goto RestartSecondAudio;
               }
               //---- else user entered 'Y', thus they heard the audio, and thus they can move on with the test
          }
          else
          {
               HMODULE hCoreDll = GetCoreDLLHandle();
               if( NULL != hCoreDll )
               {
                    PFNMESSAGEBOX pfnMessageBox = (PFNMESSAGEBOX)GetProcAddress(hCoreDll, TEXT("MessageBoxW"));
                    if( NULL  != pfnMessageBox )
                    {
                         iResponse = pfnMessageBox(NULL, TEXT("Did you hear the WAVE file playback?\n\
                                                              (Press Cancel to Retry)"), TEXT("Interactive Response "),
                                                              MB_YESNOCANCEL | MB_ICONQUESTION);
                         switch( iResponse )
                         {
                              case IDNO:
                                   LOG(TEXT("ERROR:  User said there was no Audio Playback after returning \
                                            from the SUSPEND state.") );
                                   FreeCoreDLLHandle(hCoreDll);
                                   return TPR_FAIL;
                              case IDCANCEL:
                                   FreeCoreDLLHandle(hCoreDll);
                                   //---- Need to re-play the sound again
                                   goto RestartSecondAudio;
                         }
                    }
                    else
                    {
                         LOG(TEXT("ERROR: pfnMessageBox is NULL, GetProcAddress failed."));
                         FreeCoreDLLHandle(hCoreDll);
                         return TPR_ABORT;
                    }
                    if( FALSE == FreeCoreDLLHandle(hCoreDll) )
                    {
                         LOG(TEXT("ERROR: FreeCoreDLLHandle failed."));
                         return TPR_ABORT; // FreeCoreDLL Handle Failed, aborting
                    }
               }
               else
               {
                    //---- Could not get a handle to coredll
                    LOG(TEXT("ERROR:Failed to get a handle to coredll.dll\n"));
                    return TPR_ABORT;
               }
               FreeCoreDLLHandle(hCoreDll);
          }
     }

Error:
     return dwRet;
}






/*
  * Function Name: TestPowerUpAndDown
  *
  * Purpose: This function Tests the functionality of
  * WAV_PowerUp and WAV_PowerDown
  *
*/
TESTPROCAPI TestPowerUpAndDown(UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE)
{
     BEGINTESTPROC
     DWORD dwRet = TPR_PASS;

     //---- The shell doesn't necessarily want us to execute the test. Make sure first.
     if( uMsg != TPM_EXECUTE )
     {
          LOG(TEXT("ERROR in TestPowerUpAndDown: Parameter uMsg != TPM_EXECUTE. uMsg passed in was: %d"), uMsg);
          return TPR_NOT_HANDLED;
     };

     if( g_powerTests == FALSE )
     {
          LOG(TEXT("NOTE: Power Management Tests are SKIPPED by default since not \
                   all systems support power management."));
          LOG(TEXT("SUGGESTION: To enable the power mangement tests, run the Audio CETK with the '-p' flag."));
          return TPR_SKIP;
     }

     //---- Make sure that power managemnet is installed and working on this system
     WCHAR  state[1024] = {0};
     LPWSTR pState = &state[0];
     DWORD dwBufChars = (sizeof(state) / sizeof(state[0]));
     DWORD  dwStateFlags = 0;
     DWORD dwErr;

     dwErr = GetSystemPowerState( pState, dwBufChars, &dwStateFlags );
     if( ERROR_SUCCESS != dwErr || ERROR_SERVICE_DOES_NOT_EXIST == dwErr )
     {
          LOG(TEXT("ERROR: SKIPPING TEST: Power Management does not exist as an installed service on this device."));
          LOG(TEXT("ERROR: GetSystemPowerState returned ERROR_SERVICE_DOES_NOT_EXIST"));
          LOG(TEXT("SUGGESTION: Make sure that you have more than minimal (SYSGEN_PMSTUBS) \
                   power managment in your OS image."));
          LOG(TEXT("SUGGESTION: SYSGEN_PMSTUBS just gives your image stubbed functions. \
                   Try SYSGEN_PM for sample power management implementation."));
          return TPR_SKIP;
     }




     //---- dwAudioType will tell us which type of Audio function we want to test across a Power Down/Up Cycle
     LPAUDIOTYPE   lpAudioType = (LPAUDIOTYPE)lpFTE->dwUserData;
     DWORD dwAudioType = (DWORD)lpAudioType->dwAudioType;

     switch( dwAudioType )
     {
          case PLAYSOUND_SYNC:
               LOG(TEXT("\nCase #1: PlaySound Synchronus"));
               dwRet = GetReturnCode(dwRet, PowerDownAndUp(PLAYSOUND_SYNC));
               LOG(TEXT("")); // Blank Line
               break;
          case PLAYSOUND_ASYNC:
               LOG(TEXT("\nCase #2: PlaySound Asynchronus"));
               dwRet = GetReturnCode(dwRet, PowerDownAndUp(PLAYSOUND_ASYNC));
               LOG(TEXT("")); // Blank Line
               break;
          case SNDPLAYSOUND_SYNC:
               LOG(TEXT("\nCase #3: sndPlaySound Synchronus"));
               dwRet = GetReturnCode(dwRet, PowerDownAndUp(SNDPLAYSOUND_SYNC));
               LOG(TEXT("")); // Blank Line
               break;
          case SNDPLAYSOUND_ASYNC:
               LOG(TEXT("\nCase #4: sndPlaySound Asynchronus"));
               dwRet = GetReturnCode(dwRet, PowerDownAndUp(SNDPLAYSOUND_ASYNC));
               LOG(TEXT("")); // Blank Line
               break;
          case WAVEOUT:
               LOG(TEXT("\nCase #5 Testing waveOutWrite"));
               dwRet = GetReturnCode(dwRet, PowerDownAndUp(WAVEOUT));
               LOG(TEXT("")); // Blank Line
               break;
          default:
               LOG(TEXT("ERROR: Unknown Audio Type %d Passed In as Parameter"), dwAudioType);
               dwRet = TPR_FAIL;
               break;
     }
     return dwRet;
}






/*
  * Function Name: PlaybackInitialLatency
  *
  * Purpose: tests the average latency of a single buffer (aka initial latency).
  *
*/

TESTPROCAPI PlaybackInitialLatency( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
     BEGINTESTPROC

     DWORD          dwBufferLength           = 0;
     DWORD          dwBufferLengthRemainder  = 0;
     DWORD          dwCSVFileBufferLength;
     DWORD          dwCSVFileBytesWritten    = 0;
     DWORD          dwExpectedPlayTime       = 0;      // in milliseconds
     DWORD          dwPlayCounter;
     HANDLE         hCSVFile;
     HWAVEOUT       hwo;
     LPLATENCYINFO  lpLI;
     int            iLatency;                          // in milliseconds
     int            iPlayTimeTolerance       = 200;    // in milliseconds
     MMRESULT       mmRtn;
     timer_type     m_Resolution;
     timer_type     ObservedPlayTime;                  // in milliseconds
     timer_type     PlayEndTime;
     timer_type     PlayStartTime;
     DWORD          retVal                   = TPR_PASS;
     WAVEFORMATEX   wfx;
     WAVEHDR        wh;
     char           *data                    = NULL;
     static const int iBUFFERSIZE = 121;
     TCHAR          lpszCSVOutputBuffer[ iBUFFERSIZE ];
     TCHAR          *szWaveFormat            = NULL;
     TCHAR          lpszCSVTestCaseTitle[]   = TEXT( "Audio Playback Latency Test Results for Device # " );
     TCHAR          lpszCSVColumnHeaders[]   = TEXT( "Playback Time in MS, Sample Frequency \
                                                     (Samples/Sec.), Number of Channels, Bits/Sample, \
                                                     Latency in ms, Status\r\n" );

     //---- The shell doesn't necessarily want us to execute the test. Make sure first.
     if( uMsg != TPM_EXECUTE )
     {
          LOG(TEXT("In PlaybackInitialLatency: Parameter uMsg != TPM_EXECUTE. uMsg passed in was: %d"), uMsg);
          return TPR_NOT_HANDLED;
     }

     //---- check for capture device
     if( !waveOutGetNumDevs() )
     {
          LOG( TEXT("ERROR: waveOutGetNumDevs reported zero devices, we need at least one.") );
          return TPR_SKIP;
     }

     if( g_pszWaveCharacteristics )
     {
          //---- Obtain test duration and wave characteristics from the command line.
          if( TPR_PASS != CommandLineToWaveFormatEx( &wfx ) )
          {
               LOG( TEXT ("FAIL in %s @ line %u: Unable to get command line wave characteristics."),
                              TEXT( __FILE__ ), __LINE__ );
               return TPR_ABORT;
          }
          wfx.cbSize         = 0;
          dwExpectedPlayTime = g_iLatencyTestDuration;
     }
     else
     {
          if( lpFTE )
               lpLI = (LPLATENCYINFO)lpFTE->dwUserData;
          else
          {
               LOG( TEXT( "FAIL in %s @ line %u:"), TEXT(__FILE__), __LINE__ );
               LOG( TEXT( "\tlpFTE NULL.") );
               return TPR_ABORT;
          }

          if( lpLI )
               szWaveFormat = (TCHAR*)lpLI->szWaveFormat;
          else
          {
               LOG( TEXT( "FAIL in %s @ line %u:"), TEXT(__FILE__), __LINE__ );
               LOG( TEXT( "\tlpLI NULL.") );
               return TPR_ABORT;
          }
          if( !szWaveFormat )
          {
               LOG( TEXT ("FAIL in %s @ line %u: Wave Format NULL."),
               TEXT(__FILE__),__LINE__ );
               return TPR_ABORT;
          }

          dwExpectedPlayTime = (int)lpLI->iTime;
          if( dwExpectedPlayTime < 0 )
          {
               LOG( TEXT ("FAIL in %s @ line %u: Expected play time < 0."),
               TEXT(__FILE__),__LINE__ );
               return TPR_ABORT;
          }

          //---- Extract expected play time and wave format information from szWaveFormat and populate wfx.
          mmRtn = StringFormatToWaveFormatEx( &wfx, szWaveFormat );
          wfx.cbSize = 0;
          if( TPR_PASS != mmRtn )
          {
               LOG( TEXT ("FAIL in %s @ line %u:\r\n\tStringFormatToWaveFormatEx returned error code, #%d."),
               TEXT(__FILE__),__LINE__, mmRtn);
               return TPR_ABORT;
          }
     }

     //---- Open the Wave Output Stream
     mmRtn = waveOutOpen( &hwo, g_dwOutDeviceID, &wfx, NULL, 0, NULL );
     if( MMSYSERR_NOERROR != mmRtn )
     {
          LOG( TEXT ("FAIL in %s @ line %u: \r\n\twaveOutOpen returned error code, #%d."),
          TEXT(__FILE__),__LINE__, mmRtn);
          return TPR_FAIL;
     }

     //---- Display test details.
     LOG( TEXT ( "" ) );
     LOG( TEXT ("Playback time = %d ms, Wave Format = %s, Device # %d." ), dwExpectedPlayTime,
               szWaveFormat ? szWaveFormat : g_pszWaveCharacteristics, g_dwOutDeviceID );

     //---- Set up an audio buffer for test.
     dwBufferLength = dwExpectedPlayTime * wfx.nAvgBytesPerSec / 1000;

     //---- If necessary block aligned dwBufferLength.
     BlockAlignBuffer( &dwBufferLength, wfx.nBlockAlign );
     data = new char[ dwBufferLength ];
     if( !data )
     {
          LOG(TEXT("ERROR:  \r\n\tNew failed for data [%s:%u]\n"), TEXT(__FILE__),__LINE__);
          LOG(TEXT("Possible Cause:  Out of Memory\n"));
          return TPR_ABORT;
     }

     ZeroMemory( data, dwBufferLength );
     ZeroMemory( &wh,sizeof( WAVEHDR ) );

     //---- a buffer that is smaller than we expected.
     if( dwExpectedPlayTime >= (ULONG_MAX / wfx.nAvgBytesPerSec) )
     {
          LOG( TEXT( "FAIL in %s @ line %u:" ), TEXT(__FILE__), __LINE__ );
          LOG( TEXT ("\tPotential overflow, dwExpectedPlayTime = %d ms."), dwExpectedPlayTime );
          return TPR_ABORT;
     }
     PREFAST_SUPPRESS(419, "The above line is checking for overflow. This seems to be prefast noise, \
                           since the path/trace Prefast lists is not possible. (Prefast team concurs so far.)");

     wh.lpData = data;
     wh.dwBufferLength = dwBufferLength;
     wh.dwLoops = 1;
     wh.dwFlags = 0;
     if( !SineWave( wh.lpData,wh.dwBufferLength, &wfx ) )
     {
          LOG( TEXT ("FAIL in %s @ line %u:\r\n\tSineWave returned a buffer of length zero."),
                         TEXT(__FILE__),__LINE__ );
          return TPR_ABORT;
     }

     //---- Prepare header
     mmRtn = waveOutPrepareHeader( hwo, &wh, sizeof(wh) );
     if( MMSYSERR_NOERROR != mmRtn )
     {
          LOG( TEXT ("FAIL in %s @ line %u:\r\n\twaveOutPrepareHeader failed with return code %d." ),
                         TEXT(__FILE__), __LINE__, mmRtn );
          return TPR_FAIL;
     }

     //---- Retrieve play start time.
     if( !QueryPerformanceCounter( reinterpret_cast<LARGE_INTEGER*>( &PlayStartTime ) ) )
     {
          LOG( TEXT ("FAIL in %s @ line %u:\r\n\tQueryPerformanceCounter failed." ),
                    TEXT(__FILE__), __LINE__ );
          return TPR_ABORT;
     }

     //---- Play wh.
     mmRtn = waveOutWrite( hwo, &wh, sizeof(wh) );
     if( MMSYSERR_NOERROR != mmRtn )
     {
          LOG( TEXT ("FAIL in %s @ line %u:\r\n\twaveOutWrite failed with return code %d." ),
                         TEXT(__FILE__), __LINE__, mmRtn );
          return TPR_FAIL;
     }

     //---- Wait for wh to finish playing.
     for(  dwPlayCounter = 0;
          ( dwPlayCounter < ( dwExpectedPlayTime + (DWORD)iPlayTimeTolerance ) ) && !( wh.dwFlags & WHDR_DONE );
          dwPlayCounter++ )
     {
          Sleep( 1 );
     }

     //---- Retrieve play end time.
     if( !QueryPerformanceCounter( reinterpret_cast<LARGE_INTEGER*>( &PlayEndTime ) ) )
     {
          LOG( TEXT ("FAIL in %s @ line %u:\r\n\tQueryPerformanceCounter failed." ),
                         TEXT(__FILE__), __LINE__ );
          return TPR_ABORT;
     }

     //---- Test complete, clean up and finish.

     //---- unprepare header
     mmRtn = waveOutUnprepareHeader( hwo, &wh, sizeof(wh) );
     if( MMSYSERR_NOERROR != mmRtn )
     {
          LOG( TEXT ("FAIL in %s @ line %u:\r\n\twaveOutUnprepareHeader failed with return code %d." ),
                        TEXT(__FILE__), __LINE__, mmRtn );
          return TPR_FAIL;
     }

     mmRtn = waveOutClose(hwo);
     if( MMSYSERR_NOERROR != mmRtn )
     {
          LOG( TEXT( "FAIL in %s @ line %u:"), TEXT(__FILE__), __LINE__ );
          LOG( TEXT( "twaveOutClose returned error code, #%d."), mmRtn );
          return TPR_FAIL;
     }
     delete [] data;
     data = NULL;

     //---- Obtain Performance Frequency resolution and calculate latency.
     if(!QueryPerformanceFrequency( reinterpret_cast<LARGE_INTEGER*>(&m_Resolution) ))
    {
         LOG( TEXT ("The installed hardware does not support a high-resolution performance counter!" ));
         return TPR_FAIL;
    }
     LOG( TEXT ("Performance frequency resolution = %d ticks/sec." ), m_Resolution );
     ValidateResolution( m_Resolution );
     ObservedPlayTime = ( PlayEndTime - PlayStartTime ) * 1000 / m_Resolution;
     iLatency = int( ObservedPlayTime - dwExpectedPlayTime );
     LOG( TEXT ("Latency = %d ms." ), iLatency );

     //---- If Latency exceeds tolerance fail the test.
     if(  ( iLatency < -iPlayTimeTolerance )
          || ( iLatency >  iPlayTimeTolerance ) )
     {
          LOG( TEXT( "FAIL in %s @ line %u:"), TEXT(__FILE__), __LINE__ );
          LOG( TEXT( "Latency = %d: must be >= -%d and <= %d."),
                         iLatency, iPlayTimeTolerance, iPlayTimeTolerance );
          retVal =  TPR_FAIL;
     }
     if( g_pszCSVFileName )
     {
          hCSVFile = CreateFile(   g_pszCSVFileName,
                                   GENERIC_WRITE,
                                   0,
                                   NULL,
                                   OPEN_ALWAYS,
                                   FILE_ATTRIBUTE_NORMAL,
                                   NULL );

          if( INVALID_HANDLE_VALUE == hCSVFile )
          {
               LOG( TEXT( "Unable to open file %s. Proceeding with test." ), g_pszCSVFileName );
          }
          else
          {
               //---- Set file pointer to end of file, to append data.
               if( 0xFFFFFFFF == SetFilePointer( hCSVFile, 0, NULL, FILE_END ) )
               {
                    LOG( TEXT( "Unable to set file pointer to end of file file %s. Proceeding with test." ), g_pszCSVFileName );
               }
               else
               {
                   int nChars = 0; 
                    if( g_bNeedCSVFilePlayHeader )
                    {
                         //---- display test case description
                         dwCSVFileBufferLength
                                   = wcslen( ( wchar_t * ) lpszCSVTestCaseTitle ) * 2; // since each char is 2 bytes
                         if( 0 == WriteFile( hCSVFile, lpszCSVTestCaseTitle,
                                   dwCSVFileBufferLength, &dwCSVFileBytesWritten, NULL ) )
                         {
                              LOG( TEXT("Unable to write audio playback latency test results to file file %s. \
                                        Error = %d. Proceeding with test." ), g_pszCSVFileName, GetLastError() ) ;
                         }

                         nChars = swprintf_s ( lpszCSVOutputBuffer,_countof(lpszCSVOutputBuffer), TEXT( " %d\r\n" ), g_dwOutDeviceID );
                         if(nChars < 0)
                          {
                             LOG( TEXT("swprintf_s error." ));
                          }
                         dwCSVFileBufferLength
                                   = wcslen( ( wchar_t  * ) lpszCSVOutputBuffer ) * 2; // since each char is 2 bytes

                         if( 0 == WriteFile( hCSVFile, lpszCSVOutputBuffer,
                                   dwCSVFileBufferLength, &dwCSVFileBytesWritten, NULL ) )
                         {
                              LOG( TEXT("Unable to write audio playback latency test results to file file %s. \
                                        Error = %d. Proceeding with test." ), g_pszCSVFileName, GetLastError() ) ;
                         }

                         //---- display column headers
                         dwCSVFileBufferLength
                                   = wcslen( ( wchar_t  * ) lpszCSVColumnHeaders ) * 2; // since each char is 2 bytes
                         if( 0 == WriteFile( hCSVFile, lpszCSVColumnHeaders,
                                   dwCSVFileBufferLength, &dwCSVFileBytesWritten, NULL ) )
                         {
                              LOG( TEXT("Unable to write audio playback latency test results to file file %s. \
                                        Error = %d. Proceeding with test." ), g_pszCSVFileName, GetLastError() ) ;
                         }
                         g_bNeedCSVFilePlayHeader = FALSE;
                    }
                    //---- display test results
                    nChars = swprintf_s (      lpszCSVOutputBuffer,_countof(lpszCSVOutputBuffer),
                                   TEXT(" %d, %d, %d, %d, %d, %s\r\n" ),
                                        dwExpectedPlayTime,
                                        wfx.nSamplesPerSec,
                                        wfx.nChannels,
                                        wfx.wBitsPerSample,
                                        iLatency,
                                        TPR_CODE_TO_TEXT(retVal) );
                     if(nChars < 0)
                     {
                        LOG( TEXT("swprintf_s error." ));
                     }
                    dwCSVFileBufferLength
                              = wcslen( ( wchar_t  * ) lpszCSVOutputBuffer ) * 2 ; // since each char is 2 bytes

                    if( 0 == WriteFile( hCSVFile, lpszCSVOutputBuffer,
                         dwCSVFileBufferLength, &dwCSVFileBytesWritten, NULL ) )
                    {
                         LOG( TEXT("Unable to write audio playback latency test results to file file %s.  \
                                   Error = %d.  Proceeding with test." ), g_pszCSVFileName, GetLastError() );
                    }
                    FlushFileBuffers( hCSVFile );
               }
               CloseHandle( hCSVFile );
          }
     }
     return retVal;
}






/*
  * Function Name: InitialLatencySeries
  *
  * Purpose: tests the average latency of a single buffer (aka initial latency)
  * for varying play times.  The entire series of tests will be repeated for
  * every wave format specified in the latency test table.
  *
*/
TESTPROCAPI PlaybackInitialLatencySeries( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
     BEGINTESTPROC

     DWORD         dwSeriesTestResult = TPR_PASS;
     DWORD         dwTestResult;
     int           iOriginalTime;
     int           iRet;
     LPLATENCYINFO lpLI;

     //---- The shell doesn't necessarily want us to execute the test. Make sure first.
     if( uMsg != TPM_EXECUTE )
     {
          LOG(TEXT("In InitialLatencySeries: Parameter uMsg != TPM_EXECUTE. uMsg passed in was: %d"), uMsg);
          return TPR_NOT_HANDLED;
     }

     //---- check for capture device
     if( !waveOutGetNumDevs() )
     {
          LOG( TEXT("ERROR: waveOutGetNumDevs reported zero devices, we need at least one.") );
          return TPR_SKIP;
     }

     if( g_pszWaveCharacteristics )
     {
          iRet = _stscanf_s ( g_pszWaveCharacteristics, TEXT( "%u" ), &g_iLatencyTestDuration);
          if( iRet != 1 )
          {
               LOG( TEXT( "ERROR:\tin %s @ line %u"),TEXT( __FILE__ ), __LINE__ );
               LOG( TEXT( "%s not recognized\n" ), g_pszWaveCharacteristics );
               LOG( TEXT( "\tPossible Cause:  Command line wave characteristic format not in the form: d_f_c_b\n") );
               return TPR_FAIL;
          }
          //---- Run a series of tests using the wave format from the command line,
          //----  and a series of decreasing record times.
          do
          {
               dwTestResult = PlaybackInitialLatency( uMsg, tpParam, lpFTE );
               dwSeriesTestResult = GetReturnCode( dwTestResult, dwSeriesTestResult );
               g_iLatencyTestDuration /= 2;
          } while( g_iLatencyTestDuration > 0 );

          g_iLatencyTestDuration = 0;
     }
     else
     {
          //---- Run a series of tests for each line in the latency test table.
          if( lpFTE )
               lpLI = (LPLATENCYINFO)lpFTE->dwUserData;
          else
          {
               LOG( TEXT( "FAIL in %s @ line %u:"), TEXT(__FILE__), __LINE__ );
               LOG( TEXT( "\tlpFTE NULL.") );
               return TPR_ABORT;
          }

          if( !lpLI )
          {
               LOG( TEXT( "FAIL in %s @ line %u:"), TEXT(__FILE__), __LINE__ );
               LOG( TEXT( "\tlpLI NULL.") );
               return TPR_ABORT;
          }

          while( lpLI->szWaveFormat )
          {
               //---- Save the original play time, so that it can be restored for any future runs.
               iOriginalTime = lpLI->iTime;

               //---- Run a series of tests with the same wave format, but decreasing play times.
               do
               {
                    dwTestResult = PlaybackInitialLatency( uMsg, tpParam, lpFTE );
                    dwSeriesTestResult = GetReturnCode( dwTestResult, dwSeriesTestResult );
                    lpLI->iTime /= 2;
               } while( lpLI->iTime > 0 );

               //---- Restored the rigional play time, for any later runs.
               lpLI->iTime = iOriginalTime;
               lpLI++;
               lpFTE->dwUserData = (DWORD)lpLI;
          }
     }
     return dwSeriesTestResult;
}






/*
  * Function Name: PlaybackMixing
  *
  * Purpose: This test stresses the system by launching a series of sound
  * threads. It supports an "interactive" command line option, allowing the
  * user to monitor the test and fail it, if the sound quality is unacceptable.
  *
*/

TESTPROCAPI PlaybackMixing( UINT uMsg, TPPARAM tpParam, LPFUNCTION_TABLE_ENTRY lpFTE )
{
     BEGINTESTPROC

     if( g_bSkipOut )
          return TPR_SKIP;

     BOOL  bRunTest                     = TRUE;
     DWORD dwMultipleThreadsResults     = TPR_PASS;
     DWORD dwUserResponse;
     DWORD tr                           = TPR_PASS;

     //---- check for capture device
     if( !waveOutGetNumDevs() )
     {
          LOG( TEXT("ERROR: waveOutGetNumDevs reported zero devices, we need at least one.") );
          return TPR_SKIP;
     }

     while( bRunTest )
     {
          bRunTest = FALSE;

          dwMultipleThreadsResults = LaunchMultipleAudioPlaybackThreads( uMsg );
          if( TPR_PASS != dwMultipleThreadsResults )
          {
               tr = GetReturnCode( tr, dwMultipleThreadsResults );
               LOG( TEXT( "FAIL in %s @ line %u:" ), TEXT( __FILE__ ), __LINE__ );
               LOG( TEXT( "\tLaunchMultipleAudioPlaybackThreads failed." ) );
          }

          if( g_interactive )
          {
               dwUserResponse = AskUserAboutSoundQuality();
               if( TPR_RERUN_TEST == dwUserResponse )
                    bRunTest = TRUE;
               else
                    tr = GetReturnCode( tr, dwUserResponse );
          }
     }
     return tr;
} // PlaybackMixing






/*
  * Function Name: LaunchMultipleAudioPlaybackThreads
  *
  * Purpose: Stresses the system by launching a series of sound threads.
  *
*/
TESTPROCAPI LaunchMultipleAudioPlaybackThreads( UINT uMsg )
{
     int     iWaveFormID;
     DWORD   dwThreadNo;
     DWORD   dwWaitResult;
     DWORD   dwWaveformStabilityResults;
     UINT    fuSound;
     DWORD   dwExitCode       = 0;
     DWORD   tr               = TPR_PASS;
     int     iSave_g_duration = g_duration;
     HANDLE* hSoundThreads    = new HANDLE[ g_dwNoOfThreads ];
     PLAYBACKMIXING_sndPlaySound_THREADPARM sndPlaySoundParm;
     PLAYBACKMIXINGTHREADPARM PlaybackMixingThreadParm;
     int    iNumRetry;
     #define MAX_NUM_RETRY    20

     //---- check for capture device
     if( !waveOutGetNumDevs() )
     {
          LOG( TEXT("ERROR: waveOutGetNumDevs reported zero devices, we need at least one.") );
          return TPR_SKIP;
     }

     //---- Check to make sure that the handle array allocation succeeded.
     if( !hSoundThreads )
     {
          LOG( TEXT( "FAIL in %s @ line %u:" ), TEXT( __FILE__ ), __LINE__ );
          LOG( TEXT( "\tFailed to allocate memory for threads." ) );
          tr = GetReturnCode( tr, TPR_ABORT );
          goto Error;
     }

     dwThreadNo = 0;

     LOG( TEXT( "INSTRUCTIONS:  This test case will attempt to playback a series \
                of overlapping tones for all common Formats" ) );
     LOG( TEXT( "INSTRUCTIONS:  Please listen to playback to ensure that a tone \
                is played properly." ) );

     PlaybackMixingThreadParm.dFrequency  = 110;

     //---- Launch a thread that uses sndPlaySound to repeatedly play a file,
     //----  while other threads are started.
     sndPlaySoundParm.fuSound = SND_LOOP | SND_NODEFAULT | SND_ASYNC;
     hSoundThreads[ dwThreadNo ] = CreateThread(  NULL,
                                                  0,
                                                  PlaybackMixing_sndPlaySound_Thread,
                                                  (PVOID)&sndPlaySoundParm,
                                                  0,
                                                  NULL );
     CheckCondition( NULL == hSoundThreads[dwThreadNo],"ERROR:  Unable to CreateThread # 5, \
                                                       PlaybackMixingThread.", TPR_ABORT, \
                                                       "Refer to GetLastError for a Possible Cause");
     dwThreadNo++;

     //---- Launch a series of concurrent threads that use waveOutWrite to play various wave formats.
     g_duration = g_dwNoOfThreads * PLAY_GAP / 1000;
     PlaybackMixingThreadParm.uMsg = uMsg;
     iWaveFormID = 0;
     for( ; g_dwNoOfThreads > dwThreadNo; dwThreadNo++ )
     {
          if( !lpFormats[ iWaveFormID ].szName )
               iWaveFormID = 0;

          //---- Delay bfore starting the next thread.
          Sleep( PLAY_GAP );

          PlaybackMixingThreadParm.iWaveFormID = iWaveFormID;
          PlaybackMixingThreadParm.dFrequency  = PlaybackMixingThreadParm.dFrequency + 200 % 5000;
          hSoundThreads[ dwThreadNo ]          = CreateThread(   NULL,
                                                                 0,
                                                                 PlaybackMixing_waveOutWrite_Thread,
                                                                 (PVOID)&PlaybackMixingThreadParm,
                                                                 0,
                                                                 NULL );

          if(CeSetThreadPriority(hSoundThreads[ dwThreadNo ], 248) == FALSE)
               LOG(TEXT("Unable to change the priority of the Thread: %d"), dwThreadNo);

          CheckCondition( NULL == hSoundThreads[dwThreadNo],"ERROR:  Unable to CreateThread # 1, \
                                                            PlaybackMixingThread.", TPR_ABORT, \
                                                            "Refer to GetLastError for a Possible Cause");
          iWaveFormID++;
     }

     //---- Allow time for the last and hence all other waveOutWrite() threads to finish.
     Sleep( g_duration * 1000 + 2 * PLAY_GAP );

     //---- sndPlaySound() thread is still in a loop and needs to be terminated.

     fuSound = SND_LOOP | SND_NODEFAULT;
     if( !sndPlaySound( NULL, fuSound ) )
     {
          LOG( TEXT( "FAIL in %s @ line %u:" ), TEXT( __FILE__ ), __LINE__ );
          LOG( TEXT( "\tPlaybackMixing: Unable to terminate sndPlaySound." ) );
          tr = GetReturnCode( tr, TPR_FAIL );
     }

     //---- Check the status of all threads, and kill any that are still active.
     dwWaveformStabilityResults = THREAD_UNTESTED;
     for( dwThreadNo = 1; dwThreadNo <= g_dwNoOfThreads; dwThreadNo++ )
     {
          dwWaitResult = WaitForSingleObject( hSoundThreads[ dwThreadNo -1 ], 0 );
          switch( dwWaveformStabilityResults )
          {
               case THREAD_FAILED:
                    break;
               case THREAD_ABANDONED:
                    if( WAIT_FAILED  == dwWaitResult )
                         dwWaveformStabilityResults = THREAD_FAILED;
                    break;
               case THREAD_TIMEOUT:
                    if( WAIT_FAILED == dwWaitResult )
                         dwWaveformStabilityResults = THREAD_FAILED;
                    else if( WaitAbandoned( dwWaitResult, 1 ) )
                         dwWaveformStabilityResults = THREAD_ABANDONED;
                    break;
               default:
                    if( WaitSuceeded( dwWaitResult, 1 ) )
                         dwWaveformStabilityResults = THREAD_SUCCEEDED;
                    else if( WaitAbandoned( dwWaitResult, 1 ) )
                         dwWaveformStabilityResults = THREAD_ABANDONED;
                    else if( WaitTimedOut( dwWaitResult ) )
                         dwWaveformStabilityResults = THREAD_TIMEOUT;
                    else
                         dwWaveformStabilityResults = THREAD_FAILED;
                    break;
          }

          iNumRetry = 0;
          do
          {
              iNumRetry++;
              if( GetExitCodeThread( hSoundThreads[ dwThreadNo - 1 ], &dwExitCode ) )
              {
                   switch( dwExitCode )
                   {
                        case TPR_PASS:
                             LOG( TEXT( "PlaybackMixingThread: Thread # %u finished." ), dwThreadNo );
                             break;
                        case STILL_ACTIVE:
                             if ( iNumRetry >= MAX_NUM_RETRY )
                             {
                                 tr = GetReturnCode( tr, TPR_FAIL );
                                 LOG( TEXT( "ERROR:\tIn %s @ line %u." ), TEXT( __FILE__ ), __LINE__ );
                                 LOG( TEXT( "\tThread %u appears to be hung..." ), dwThreadNo );
                                 if ( TerminateThread( hSoundThreads[ dwThreadNo - 1 ], 0 ) ) // to avoid crash, however waveAPI will abandon prepared wave headers
                                      LOG( TEXT( "\t\tThread is terminated." ) );
                                 else
                                      LOG( TEXT( "\t\tFailed to terminate the thread." ) );
                             }
                             else
                                 Sleep( PLAY_GAP );
                             break;
                        default:
                             tr = GetReturnCode( tr, dwExitCode );
                             LOG( TEXT( "ERROR:\tIn %s @ line %u." ), TEXT( __FILE__ ), __LINE__ );
                             LOG( TEXT( "\tThread %u returned %u." ), dwThreadNo, dwExitCode );
                             break;
                   }
              }
              else
              {
                   tr = GetReturnCode( tr, TPR_FAIL );
                   LOG( TEXT("ERROR:\tIn %s @ line %u.  GetExitCodeThread() failed for thread %u."), \
                        TEXT( __FILE__ ), __LINE__, dwThreadNo );
              }
          } while ( dwExitCode == STILL_ACTIVE && iNumRetry < MAX_NUM_RETRY ); // until the thread terminates or time out (~MAX_NUM_RETRY*PLAY_GAP/1000 second)

          if ( dwExitCode != STILL_ACTIVE && iNumRetry > 1 )
          {
             tr = GetReturnCode( tr, TPR_FAIL );
             LOG( TEXT( "ERROR:\tIt took too long for thread # %u to finish. (%i more ms than expected)" ), dwThreadNo, (iNumRetry - 1) * PLAY_GAP );
          }

          if( CloseHandle( hSoundThreads[ dwThreadNo - 1 ] ) )
               hSoundThreads[ dwThreadNo - 1 ] = NULL;
          else
          {
               LOG( TEXT( "FAIL in %s @ line %u" ), TEXT( __FILE__ ), __LINE__ );
               LOG( TEXT( "\t\tPlaybackMixingThread: Unable to close handle \
                          for thread # %i." ), dwThreadNo );
               tr = GetReturnCode( tr, TPR_FAIL );
          }
     } //---- for( dwThreadNo )

     switch( dwWaveformStabilityResults )
     {
          case THREAD_FAILED:
               LOG( TEXT( "FAIL in %s @ line %u:" ), TEXT( __FILE__ ), __LINE__ );
               LOG( TEXT( "\t\tPlaybackMixingThread: A failure occurred while waiting for \
                          one or more sound threads to complete." ) );
               tr = GetReturnCode( tr, TPR_FAIL );
               break;
          case THREAD_ABANDONED:
               LOG( TEXT( "FAIL in %s @ line %u:" ), TEXT( __FILE__ ), __LINE__ );
               LOG( TEXT( "\t\tPlaybackMixingThread: One or more sound threads were abandoned." ) );
               tr = GetReturnCode( tr, TPR_FAIL );
               break;
          case THREAD_TIMEOUT:
               LOG( TEXT( "FAIL in %s @ line %u:" ), TEXT( __FILE__ ), __LINE__ );
               LOG( TEXT ( "\t\tPlaybackMixingThread: One or more sound threads timed out \
                           before completing." ) );
               tr = GetReturnCode( tr, TPR_FAIL );
               break;
          default:
               //---- All thread completed successfully
               break;
     }

     //---- Restore g_duration to its original value.
     g_duration = iSave_g_duration;

     delete [] hSoundThreads;
     return tr;

Error:
     LOG( TEXT( "TERMINATING:   Terminating PlaybackMixing due to error condition listed above." ) );
     //---- Changes made to release all thread objects, even for threads that have exited.
     for( dwThreadNo = 0; dwThreadNo < g_dwNoOfThreads; dwThreadNo++ )
     {
          if( hSoundThreads[dwThreadNo] )
          {
               if( CloseHandle( hSoundThreads[dwThreadNo] ) )
                    hSoundThreads[dwThreadNo] = NULL;
               else
               {
                    LOG( TEXT( "FAIL in %s @ line %u" ), TEXT( __FILE__ ), __LINE__ );
                    LOG( TEXT( "\t\tPlaybackMixingThread: Unable to close handle for thread # %i." ), dwThreadNo + 1 );
                    tr = GetReturnCode( tr, TPR_FAIL );
               }
          }
     }

     delete [] hSoundThreads;
     return tr;
}






/*
  * Function Name: AskUserAboutSoundQuality
  *
  * Purpose: Solicit input from the user about the sound quality rendered while
  * the system is under stress.
  *
*/
DWORD AskUserAboutSoundQuality()
{
     BOOL    bRerunTest = FALSE;
     int     iResponse;
     DWORD   tr         = TPR_PASS;

     LOG( TEXT( "Running in Interactive Mode." ) );
     HMODULE hCoreDLL = GetCoreDLLHandle();
     if( NULL != hCoreDLL )
     {
          PFNMESSAGEBOX pfnMessageBox = (PFNMESSAGEBOX)GetProcAddress( hCoreDLL, TEXT( "MessageBoxW" ) );
          if( NULL  != pfnMessageBox )
          {
               iResponse = pfnMessageBox(    NULL,
                                             TEXT("Did you hear sounds mixed together properly, \
                                                   with acceptable quality and without audible glitches? \
                                                   Press YES or NO or press CANCEL if you wish to rerun this test." ),
                                             TEXT( "Interactive Response" ), MB_YESNOCANCEL | MB_ICONQUESTION );
               switch( iResponse )
               {
                    case IDYES:
                         break;
                    case IDNO:
                         LOG( TEXT("ERROR:   User said that the sound streams didn't mix together properly \
                                   with acceptable quality, or that there were audible glitches." ) );
                         tr = GetReturnCode( tr, TPR_FAIL );
                         break;
                    case IDCANCEL:
                         LOG(TEXT( "INFORMATION:   User said rerun test." ) );
                         bRerunTest = TRUE;
                         break;
                    default:
                         LOG( TEXT( "ERROR:\tin %s @ line %u" ), TEXT( __FILE__ ), __LINE__ );
                         LOG( TEXT( "\tInvalid response from MessageBox." ) );
                         tr = GetReturnCode( tr, TPR_FAIL );
                         break;
               }
          }
          else
          {
               LOG( TEXT( "ERROR:\tin %s @ line %u" ), TEXT( __FILE__ ), __LINE__ );
               LOG( TEXT( "\tpfnMessageBox is NULL, GetProcAddress failed." ) );
               tr = GetReturnCode( tr, TPR_ABORT );
          }
          if( FALSE == FreeCoreDLLHandle( hCoreDLL ) )
               tr = GetReturnCode( tr, TPR_FAIL );
     }
     else
     {
          //---- Could not get a handle to coredll
          LOG( TEXT( "ERROR:\tin %s @ line %u" ), TEXT( __FILE__ ), __LINE__ );
          LOG( TEXT( "\tFailed to get a handle to coredll.dll\n" ) );
          tr = GetReturnCode( tr, TPR_ABORT );
     }

     if( bRerunTest )
     {
          LOG( TEXT( "INFORMATION:   This test case is being rerun per user request." ) );
          return TPR_RERUN_TEST;
     }
     return tr;
} //---- AskUserAboutSoundQuality






DWORD WINAPI PlaybackMixing_waveOutWrite_Thread( LPVOID lpParm )
{
     double                    dFrequency;
     DWORD                     hrReturn;
     DWORD                     iWaveFormID;
     PLAYBACKMIXINGTHREADPARM* pPlaybackMixingThreadParm;
     DWORD                     tr                           = TPR_PASS;
     UINT                      uMsg;

     CheckMMRESULT( NULL == lpParm, "ERROR:   NULL parameter passes to PlaybackMixingThread.", \
                         TPR_FAIL, "Unable to execute thread");
     pPlaybackMixingThreadParm = (PLAYBACKMIXINGTHREADPARM*)lpParm;
     if( !pPlaybackMixingThreadParm )
     {
          LOG( TEXT( "ERROR:\tin %s @ line %u" ), TEXT( __FILE__ ), __LINE__ );
          LOG( TEXT( "\tpPlaybackMixingThreadParm NULL." ) );
          return TPR_FAIL;
     }
     uMsg        = pPlaybackMixingThreadParm->uMsg;
     iWaveFormID = pPlaybackMixingThreadParm->iWaveFormID;
     dFrequency  = pPlaybackMixingThreadParm->dFrequency;
     hrReturn    = MMSYSERR_NOERROR;
     LOG(TEXT( "Attempting to playback %s" ), lpFormats[ iWaveFormID ].szName );
     tr = PlayWaveFormat(     lpFormats[ iWaveFormID ].szName,
                              g_duration,
                              CALLBACK_NULL,
                              NULL,
                              &hrReturn,
                              dFrequency );
     if( ( tr != TPR_SKIP ) && ( tr != TPR_ABORT ) && ( tr != TPR_PASS ) )
          tr = TPR_FAIL;
     return tr;

Error:
     //---- This return code is what the calling test case will see.
     return( tr );
} //---- PlaybackMixing_waveOutWrite_Thread






DWORD WINAPI PlaybackMixing_sndPlaySound_Thread( LPVOID lpParm )
{
     WIN32_FIND_DATA findData;
     UINT            fuSound;
     HANDLE          hFile;
     PLAYBACKMIXING_sndPlaySound_THREADPARM* pPlaybackMixing_sndPlaySound_ThreadParm;
     DWORD           tr = TPR_PASS;

     CheckMMRESULT( NULL == lpParm, "ERROR:  NULL parameter passes to PlaybackMixing_sndPlaySound_Thread.", \
                         TPR_ABORT, "Unable to execute thread");

     pPlaybackMixing_sndPlaySound_ThreadParm = ( PLAYBACKMIXING_sndPlaySound_THREADPARM* )lpParm;

     fuSound = pPlaybackMixing_sndPlaySound_ThreadParm->fuSound;
     LOG( TEXT( "Attempting to play a file from the Windows Directory with the sndPlaySound API" ) );
     hFile = FindFirstFile( TEXT( "\\windows\\*.wav" ), &findData );
     if( INVALID_HANDLE_VALUE  == hFile )
     {
          LOG( TEXT( "ERROR:\tin %s @ line %u" ), TEXT( __FILE__ ), __LINE__ );
          LOG( TEXT( "\tThere are no wave files to play in the \\windows\\ directory." ));
          LOG( TEXT( "\tSuggestion: Add some *.wav files to the \\windows\\ directory and try again." ) );
          FindClose( hFile );
          tr = GetReturnCode( tr, TPR_SKIP );
          return tr;
     }
     LOG( TEXT( "Attempting to use sndPlaySound to play %s." ), findData.cFileName );
     if( !sndPlaySound( findData.cFileName, fuSound ) )
     {
          LOG( TEXT( "FAIL in %s @ line %u:" ), TEXT( __FILE__ ), __LINE__ );
          LOG( TEXT( "\t\tPlaybackMixingsndPlaySoundThread: sndPlaySound failed.") );
          tr = GetReturnCode( tr, TPR_ABORT );
          return tr;
     }

Error:
     //---- This return code is what the calling test case will see.
     return( tr );
}
