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
#include <windows.h>
#include <stdio.h>
#include <math.h>
#include <cwavefile.h>
#include "TuxMain.h"

#define NUM_BUFFERS 200     //Number of buffers to queue
#define MS_BUFFERS 5        //Length of a buffer in ms

//Call to waveIn and waveOut API's
#define CALL_WAVEINAPI(ret, funcall, message) ret = funcall; if (MMSYSERR_NOERROR!=ret) { TCHAR errmsg[256]; waveInGetErrorText(ret, errmsg, 255); _putts(errmsg); puts(message); goto exit;}
#define CALL_WAVEOUTAPI(ret, funcall, message) ret = funcall; if (MMSYSERR_NOERROR!=ret) { TCHAR errmsg[256]; waveOutGetErrorText(ret, errmsg, 255); _putts(errmsg); puts(message); goto exit;}

//To verify memory allocation
#define CHECK_ALLOC(pb, message) if (NULL == pb) { puts(message); goto exit;}

//To check for an error, print message and go to a particular label
#define MRCHECK(r,str, filename, label)\
      if ((r != MMSYSERR_NOERROR)) { RETAILMSG(1, (TEXT(#str) TEXT(" failed on %s. mr=%08x\r\n"), filename,r)); mmRtn = r; goto label;}

//Delete array of buffers
#define SAFE_ARRAYDELETE(p) {if (p) delete[] (p); (p) = NULL;}

AudioQalityTestData gLoopbackTestData;
static BOOL         bUsage = FALSE;
static TCHAR  *next_Token;
//Make buffer size block aligned
DWORD ChooseBufferSize(WAVEFORMATEX *pwfx, DWORD msec)
{
     DWORD dwBufSize;
     //Calculate buffer size
     dwBufSize = (pwfx->nAvgBytesPerSec * msec) / 1000;
     
     // Now round up to the nearest nBlockAlign
     dwBufSize += pwfx->nBlockAlign - 1;
     dwBufSize -= (dwBufSize % pwfx->nBlockAlign);
     return dwBufSize;
}

//Check audio device capabilities
BOOL CheckCaps(int deviceId)
{
     BOOL bRet = FALSE;                                 //Return FALSE if capabilities not supported
     MMRESULT mmRes = MMSYSERR_NOERROR;
     
     INT cWaveInDevs;                                  //No. of capture devices
     INT cWaveOutDevs;                                 //No. of playback devices
     
     WAVEINCAPS waveInCap;                             //Capture capabilities
     WAVEOUTCAPS waveOutCap;                           //Playback capabilities
     
     memset(&waveOutCap, 0, sizeof(waveOutCap));       //Clear waveOutCap      
     memset(&waveInCap, 0, sizeof(waveInCap));         //Clear waveInCap
     
     cWaveOutDevs = waveOutGetNumDevs ();              //Get No. of playback devices

     //Check if playback device we want to verify is available or not
     if (deviceId >= cWaveOutDevs)
     {
          Debug(TEXT("Render Device: %d not available\n"), deviceId);
          goto exit;
     }
     
     //Get playback deviceId capabilities
     CALL_WAVEOUTAPI ( mmRes, waveOutGetDevCaps (deviceId, &waveOutCap, sizeof(waveOutCap)), "waveOutGetDevCaps failed.\n");
     Debug(TEXT("Playback Device %d - %s\n"), deviceId, waveOutCap.szPname);
     
     cWaveInDevs = waveInGetNumDevs ();                //Get No. of capture devices

     //Check if capture device we want to verify is available or not
     if (deviceId >= cWaveInDevs)
     {
          Debug(TEXT("Capture Device: %d not available\n"), deviceId);
          goto exit;
     }
     
     //Get capture deviceId capabilities
     CALL_WAVEINAPI ( mmRes, waveInGetDevCaps (deviceId, &waveInCap, sizeof(waveInCap)), "waveInGetDevCaps failed.\n");
     Debug(TEXT("Capture Device %d - %s\n"), deviceId, waveInCap.szPname);
     
     bRet = TRUE;

exit:
     return bRet;
} //end of CheckCaps()

//Parse global DWORDs
BOOL ChangeGlobalDWORD( TCHAR* szSeps, DWORD *dwGlobal ) 
{
     TCHAR *szToken;
     int iRet;
     szToken=_tcstok_s( NULL, szSeps,&next_Token);
     iRet = _stscanf_s ( szToken, TEXT( "%u" ), dwGlobal );
     return ( iRet==1 );
}

//Parse global HEXs
BOOL ChangeGlobalHEX( TCHAR* szSeps, DWORD *dwGlobal ) 
{
     TCHAR *szToken;
     int iRet;
     szToken= _tcstok_s ( NULL, szSeps,&next_Token);
     iRet = _stscanf_s ( szToken, TEXT( "%x" ), dwGlobal );
     return ( iRet==1 );
}

//Parse global STRINGs
BOOL ChangeGlobalFileName (TCHAR* szSeps, TCHAR* szToken) 
{
     
     szToken = _tcstok_s ( NULL, szSeps,&next_Token);  // advance to next token

     if( szToken && szToken[ 0 ] != TEXT( '-' ) ) 
          gLoopbackTestData.pszString = szToken;
     else                                         
          gLoopbackTestData.pszString = NULL;
    return TRUE;
}

//Parse command line arguments
BOOL ProcessCommandLine(LPCTSTR szCmdLine)
{
     TCHAR* szToken;
     TCHAR* szSwitch;
     TCHAR* szCommandLine=(TCHAR*)szCmdLine;
     TCHAR  szSeps[]=TEXT(" ,\t\n");
     TCHAR  szSwitches[]=TEXT("diovx?");
     szToken=_tcstok_s (szCommandLine,szSeps,&next_Token);

     bUsage = FALSE;

     while(szToken!=NULL)
     {
          if(szToken[0]==TEXT('-')) 
          {
               szSwitch=_tcschr(szSwitches,szToken[1]);
            if(szSwitch) 
               {
                switch(*szSwitch) 
                    {
                    case 'i':
                         ChangeGlobalFileName(szSeps, szToken); 
                         gLoopbackTestData.pszInputFile = gLoopbackTestData.pszString;
                         break;
                    case 'o':
                         ChangeGlobalFileName(szSeps, szToken);
                         gLoopbackTestData.pszOutputFile = gLoopbackTestData.pszString;
                         break;
                    case 'v':
                         ChangeGlobalHEX(szSeps,&gLoopbackTestData.dwVolume);
                         break;
                    case 'd':
                         ChangeGlobalDWORD(szSeps,&gLoopbackTestData.dwDeviceId);
                         break;
                    case '?':
                         Debug(TEXT(" " ) );
                         Debug(TEXT("usage: loopbacktest parameters\n"));
                         Debug(TEXT(" " ) );
                         Debug(TEXT("Parameters: [-i Playback Filename] [-o Capture Filename] [-v Volume] [-d device id]" ) );
                         Debug(TEXT(" " ) );
                         Debug(TEXT("\t-i\tplayback filename\n"));
                         Debug(TEXT("\t-o\tcapture filename\n"));
                         Debug(TEXT("\t-v\tvolume setting (default=0xC000C000)\n"));
                         Debug(TEXT("\t-d\taudio device id (default=0)\n"));
                         Debug(TEXT("\t-?\tthis help message\n"));
                         Debug(TEXT("Please press any key to exit..\n"));
                         bUsage = TRUE;
                         break;
                    default:
                         Debug(TEXT("ParseCommandLine:  Unknown Switch \"%s\"\n"),szToken);
                  }
            }
            else Debug(TEXT("ParseCommandLine:  Unknown Switch \"%s\"\n"),szToken);
        }
        else Debug(TEXT("ParseCommandLine:  Unknown Parameter \"%s\"\n"),szToken);
        szToken=_tcstok_s (NULL,szSeps,&next_Token); 
    }
    return TRUE;
}

//
// Main program code
//
TESTPROCAPI 
AudioQualityLoopbackTest( UINT uMsg, 
                          TPPARAM tpParam, 
                          LPFUNCTION_TABLE_ENTRY lpFTE )
{
    DWORD retval = TPR_PASS;

    // Handle tux messages
    if ( HandleTuxMessages(uMsg, tpParam) == SPR_HANDLED )
        return SPR_HANDLED;
    else if ( TPM_EXECUTE != uMsg )
        return TPR_NOT_HANDLED;

    if ( bUsage ) 
        return TPR_PASS;

     //Verify available audio devices
     if (!CheckCaps(gLoopbackTestData.dwDeviceId))
     {
          Debug(TEXT("Audio device capability check failed...\n"));
          return TPR_SKIP;
     }
     
     DWORD dwChannels = 1;                             //Mono
     DWORD dwBitsPerSample = 16;                       //16 bits per sample

     HWAVEIN hwi = NULL;                               //Waveform-audio input device handle             
     HWAVEOUT hwo = NULL;                              //Waveform-audio output device handle   
     
     WAVEHDR wavInHdr[NUM_BUFFERS];                    //input buffer header
     memset(wavInHdr, 0, sizeof(wavInHdr));            //Clear input buffer header
     
     WAVEHDR wavOutHdr[NUM_BUFFERS];                   //Output buffer header
     memset(wavOutHdr, 0, sizeof(wavOutHdr));          //Clear output buffer header
     
     CWaveFile PlaybackFile;                            //Output file handler
     CWaveFile RecordFile;                              //Input file handler

     HANDLE hqWaveCallback = NULL;
     
     int i;
     DWORD dwBytesWritten;
     DWORD nBytes = 0;
     
     //Increase thread priority to get more accrurate results
     if ( 0 == CeSetThreadPriority(GetCurrentThread(), 150) )
     {
          Debug(TEXT("failed to set thread priority\n"));
          retval = TPR_FAIL;
          goto exit;
     }
     
     //Setup format of waveform-audio input data
     LPWAVEFORMATEX pwfxMic;
     WAVEFORMATEX wfxMic;
     LPWAVEFORMATEX pwfxSpk;

     //Open playback file.
     if(ERROR_SUCCESS != PlaybackFile.Create(gLoopbackTestData.pszInputFile, GENERIC_READ, OPEN_EXISTING, 0,(LPVOID *)&pwfxSpk, NULL))
     {
         Debug(TEXT("ERROR: Could not Open %s\n"), gLoopbackTestData.pszInputFile);
         retval = TPR_FAIL;
         goto exit;
     }
 
     //Copy playback format to capture format
     wfxMic.wFormatTag = pwfxSpk->wFormatTag;
     wfxMic.nChannels = pwfxSpk->nChannels;
     wfxMic.nSamplesPerSec = pwfxSpk->nSamplesPerSec;
     wfxMic.nAvgBytesPerSec = pwfxSpk->nAvgBytesPerSec;
     wfxMic.nBlockAlign = pwfxSpk->nBlockAlign;
     wfxMic.wBitsPerSample = pwfxSpk->wBitsPerSample;
     wfxMic.cbSize = pwfxSpk->cbSize;
     
     pwfxMic = &wfxMic;
        
     // prepare capture file
     if(ERROR_SUCCESS != RecordFile.Create(gLoopbackTestData.pszOutputFile,GENERIC_WRITE,CREATE_ALWAYS,0,(LPVOID*)&pwfxMic,NULL))
     {
         Debug(TEXT("ERROR: Could not Open %s\n"), gLoopbackTestData.pszOutputFile);
         retval = TPR_FAIL;
         goto exit;
     }

     // create the message queue
     MSGQUEUEOPTIONS qopts;

     qopts.dwSize = sizeof(qopts);
     qopts.dwFlags = MSGQUEUE_ALLOW_BROKEN;
     qopts.bReadAccess = TRUE;
     qopts.cbMaxMessage = sizeof(WAVEMSG);
     qopts.dwMaxMessages = 2 * (NUM_BUFFERS+2); // account for all possible headers, plus WOM_OPEN and WOM_CLOSE

     hqWaveCallback = CreateMsgQueue(NULL, &qopts);
     if (hqWaveCallback == NULL)
     {
          Debug(TEXT("cannot create message queue"));
          retval = TPR_FAIL;
          goto exit;
     }
     
     MMRESULT mmRes = MMSYSERR_NOERROR;
     
     // Query input device if it supports specified wave format
     CALL_WAVEINAPI (mmRes, waveInOpen(NULL, 0, &wfxMic, NULL, NULL, WAVE_FORMAT_QUERY), "waveInOpen does not support the specified format.\n");

     // Open wave input and get it ready
     CALL_WAVEINAPI (mmRes, waveInOpen(&hwi, 0, &wfxMic, (DWORD)hqWaveCallback, NULL, CALLBACK_MSGQUEUE), "waveInOpen failed.\n");
     
     // Query output device if it supports specified wave format.
     CALL_WAVEOUTAPI (mmRes, waveOutOpen(NULL, 0, pwfxSpk, NULL, NULL, WAVE_FORMAT_QUERY), "waveOutOpen does not support the specified format.\n");

     // Open wave output and get it ready (but leave it paused).
     CALL_WAVEOUTAPI (mmRes, waveOutOpen(&hwo, 0, pwfxSpk, (DWORD)hqWaveCallback, NULL, CALLBACK_MSGQUEUE), "waveOutOpen failed.\n");
     CALL_WAVEOUTAPI (mmRes, waveOutPause(hwo),"waveOutPause failed.\n");
     
     // Set maximum volume
     waveOutSetVolume(0, gLoopbackTestData.dwVolume);
     waveOutSetVolume(hwo, gLoopbackTestData.dwVolume);
     
     // Queue up input buffers
     DWORD cCaptureBufLen = ChooseBufferSize(&wfxMic, MS_BUFFERS);
     for (i = 0; i < NUM_BUFFERS; i++)
     {
          wavInHdr[i].dwBufferLength = cCaptureBufLen;
          wavInHdr[i].dwFlags  = 0;
          wavInHdr[i].lpData = (LPSTR)new BYTE[cCaptureBufLen];;
          
          CHECK_ALLOC (wavInHdr[i].lpData, "out of memory.\n");
          memset(wavInHdr[i].lpData, 0, cCaptureBufLen);
          
          CALL_WAVEINAPI (mmRes, waveInPrepareHeader(hwi, &wavInHdr[i], sizeof(wavInHdr[i])), "waveInPrepareHeader failed.\n");
          CALL_WAVEINAPI (mmRes, waveInAddBuffer(hwi, &wavInHdr[i], sizeof(wavInHdr[i])), "waveInAddBuffer failed.\n");
     }
     
     // Queue up output buffers
     DWORD cRenderBufLen  = ChooseBufferSize(pwfxSpk, MS_BUFFERS);
     for (i = 0; i < NUM_BUFFERS; i++)
     {
          wavOutHdr[i].dwBufferLength = cRenderBufLen;
          wavOutHdr[i].dwFlags  = 0;
          wavOutHdr[i].lpData = (LPSTR)new BYTE[cRenderBufLen];

          CHECK_ALLOC (wavOutHdr[i].lpData, "out of memory.\n");
          memset(wavOutHdr[i].lpData, 0, cRenderBufLen);

          // read bytes out!
          PlaybackFile.ReadData(wavOutHdr[i].lpData,wavOutHdr[i].dwBufferLength,&nBytes);
          if(nBytes != wavOutHdr[i].dwBufferLength)
          {
              Debug(TEXT("ERROR: Could not read a buffer's worth of data from the file."));
              retval = TPR_FAIL;
              goto exit;
          }

          CALL_WAVEOUTAPI (mmRes, waveOutPrepareHeader(hwo, &wavOutHdr[i], sizeof(wavOutHdr[i])), "waveOutPrepareHeader failed.\n");
          CALL_WAVEOUTAPI (mmRes, waveOutWrite(hwo, &wavOutHdr[i], sizeof(wavOutHdr[i])),"waveOutWrite failed.\n");
     }

     CALL_WAVEINAPI (mmRes, waveInStart(hwi), "waveInStart failed.\n");

     // Sleep for 200ms (play with this number)
     Sleep(200);

     CALL_WAVEOUTAPI (mmRes, waveOutRestart(hwo),"waveOutRestart failed.\n");
     Debug(TEXT("Playback and recording started. Please wait...."));

     while (1)
     {
          WAVEMSG msg;
          DWORD dwBytesRead;
          DWORD dwTimeout = INFINITE;
          DWORD dwReadFlags;

          if (! ReadMsgQueue(hqWaveCallback, &msg, sizeof(msg), &dwBytesRead, dwTimeout, &dwReadFlags))
          {
               // assume timeout
               Debug( TEXT("ReadMsgQueue failed") );
               retval = TPR_FAIL;
               break;
          }

          if (dwBytesRead != sizeof(msg))
          {
               Debug( TEXT("ReadMsgQueue error") );
               retval = TPR_FAIL;
               break;
          }

          switch (msg.uMsg)
          {
               case WIM_DATA:
               {

                    WAVEHDR *pHdr = (PWAVEHDR) msg.dwParam1;

                    // write bytes out!
                    if(!RecordFile.WriteData(pHdr->lpData, pHdr->dwBytesRecorded, &dwBytesWritten))
                    {
                        Debug(TEXT("ERROR: Failed to Write Data Out to file."));
                        retval = TPR_FAIL;
                        goto exit;
                    }
                    if(pHdr->dwBytesRecorded != dwBytesWritten)
                    {
                        Debug(TEXT("ERROR: Number of bytes written to file is less than we asked for"));
                        retval = TPR_FAIL;
                        goto exit;
                    }

                    // Requeue buffer.
                    CALL_WAVEINAPI (mmRes, waveInAddBuffer(hwi, pHdr, sizeof(WAVEHDR)), "waveInAddBuffer failed.\n");
                    break;
               }
               case WOM_DONE:
               {
                   WAVEHDR *pHdr = (PWAVEHDR) msg.dwParam1;
                   
                   // read bytes out!
                   PlaybackFile.ReadData(pHdr->lpData,pHdr->dwBufferLength,&nBytes);
                   if(nBytes != pHdr->dwBufferLength)
                   {
                       Debug(TEXT("Output file created at %s\n\n"), gLoopbackTestData.pszOutputFile);
                       Debug(TEXT("Please verify the output file manually according to the test document\n")); 
                       Debug(TEXT("to find playback+capture latency.\n"));
                       goto exit; //stop playback
                   }
                   CALL_WAVEOUTAPI (mmRes, waveOutWrite(hwo, pHdr, sizeof(WAVEHDR)), "waveOutWrite failed.\n");
                   break;
               }
          }
     }

exit:
    
     //Clean up and delete capture buffers
     if (hwi)
     {
          waveInStop(hwi);
          waveInReset(hwi);
          for (i = 0; i < NUM_BUFFERS; i++)
          {
               waveInUnprepareHeader (hwi, &wavInHdr[i],  sizeof(wavInHdr[i]));
               SAFE_ARRAYDELETE(wavInHdr[i].lpData);
          }
          waveInClose(hwi);
     }
     
     //Clean up and delete capture buffers
     if (hwo)
     {
         waveOutPause(hwo);
         waveOutReset(hwo);
         
         for (i = 0; i < NUM_BUFFERS; i++)
         {
             waveOutUnprepareHeader(hwo, &wavOutHdr[i], sizeof(wavOutHdr[i]));
             SAFE_ARRAYDELETE(wavOutHdr[i].lpData);
         }
         waveOutClose(hwo);
     }

     //Close input and output file handles
     PlaybackFile.Close();
     RecordFile.Close();

     CloseHandle(hqWaveCallback);
     return retval;
}

