/*++
THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF
ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A
PARTICULAR PURPOSE.
 
Copyright (c) 1998-1999  Microsoft Corporation
 
Module Name:
 
     WaveFile.h
 
Abstract:
 
     This is the header for my implementation of a WAVE file handler library.
 
Notes:
 
--*/
#ifndef _WAVEFILE_H
#define _WAVEFILE_H

#include <windows.h>
#include <mmsystem.h>

#ifdef QALOG
#include <katoex.h>
#endif

/* ------------------------------------------------------------------------
	Wave RIFF form Definition:
	    <WAVE-form> ->    RIFF( 'WAVE'                  // Form type
                                <fmt-ck>                // Waveform format
                                <data-ck> )             // Waveform data

        <fmt-ck> ->       fmt(  <wave-format>            // WaveFormat structure
                                <format-specific> )      // Dependent on format
                                                         //    category

        <wave-format> ->  struct {
                                    WORD    wFormatTag;       // Format category
                                    WORD    nChannels;        // Number of channels
                                    DWORD   nSamplesPerSec;   // Sampling rate
                                    DWORD   nAvgBytesPerSec;  // For buffering
                                    WORD    nBlockAlign;      // Block alignments
                                 }

        <PCM-format-specific> -> struct {
                                            UINT    nBitsPerSample;   // Sample size
                                        }

        <data-ck> ->   data( <wave-data> )

    NOTES:
        <wave-format> -> struct WAVEFORMAT
        Use PCMWAVEFORMAT for <fmt-ck> when using PCM
        'WAVE'->ckSize = filesize - 4
        'fmt'->ckSize = sizeof(<wave-format>) + sizeof(<formate-secific>)
        'data'->ckSize = # of wave bytes.
------------------------------------------------------------------------ */


/* ------------------------------------------------------------------------
	Basic types used in RIFF ( Resource Interchange File Formate )
------------------------------------------------------------------------ */
typedef DWORD   FOURCC;

typedef struct _CK_TAG {
    FOURCC  ckID;
    DWORD   ckSize;     // the size of field <ckData>
    BYTE    *ckData;    // the actual data of the chunk 
} CK, *LPCK; 

// Used to write a complete RIFF header to a file in a single write.
typedef struct _CKHDR_TAG {
    FOURCC  ckID;
    DWORD   ckSize;
} CKHDR, *LPCKHDR;

typedef struct _RIFFCK_TAG {
    FOURCC  ckID;
    DWORD   ckSize; // = sizeof( ckData ) + sizeof( ckFrom )
    FOURCC  ckForm; // This member is included in ckSize 
} RIFFCK, *LPRIFFCK;

#ifndef MAKEFOURCC
#define MAKEFOURCC(ch0, ch1, ch2, ch3)  \  
    ((DWORD)(BYTE)(ch0) | ((DWORD)(BYTE)(ch1) << 8) |  \ 
    ((DWORD)(BYTE)(ch2) << 16) | ((DWORD)(BYTE)(ch3) << 24 ));
#endif MAKEFOURCC

inline void FOURCCtoStr( FOURCC fourcc, LPSTR lpstr )
{
    lpstr[0] = (CHAR)( fourcc & 0x000000FF);
    lpstr[1] = (CHAR)( fourcc & 0x0000FF00 >> 8);
    lpstr[2] = (CHAR)( fourcc & 0x00FF0000 >> 16);
    lpstr[3] = (CHAR)( fourcc & 0xFF000000 >> 24);
    lpstr[4] = '\0';
    
} /* inline void FOURCCtoStr( FOURCC fourcc, LPSTR lpstr ) */

/* ------------------------------------------------------------------------
	Common four character codes
------------------------------------------------------------------------ */
#define RIFF_FOURCC 0x46464952
#define RIFX_FOURCC 0x58464952
#define WAVE_FOURCC 0x45564157
#define fmt_FOURCC  0x20746D66
#define data_FOURCC 0x61746164
#define INFO_FOURCC 0x4F464E49
#define LIST_FOURCC MAKEFOURCC('L', 'I', 'S', 'T')
#define fact_FOURCC MAKEFOURCC('f', 'a', 'c', 't')

/* ------------------------------------------------------------------------
	The following is an infor structure that is filled with info data
	to be placed in a WAVE file.
------------------------------------------------------------------------ */
//#define MAX_AUX_INFO    32

/* ------------------------------------------------------------------------
    Creates or opens a RIFF-WAVE file.  When this function returns the
    file pointer is pointing to wave data. 
    NOTE:  If a new file is created and lplpWaveInfo is not NULL,
           lplpWaveInfo is assumed to be pointing to a correct LIST-INFO
           chunk of data and will be writen to the new file.
------------------------------------------------------------------------ */
class CWaveFile {
public:    
    CWaveFile( LPVOID pLog = NULL );
    CWaveFile(
        LPCTSTR         lpFileName,	            // pointer to name of the file 
        DWORD           dwDesiredAccess,	    // access (read-write) mode 
        DWORD           dwCreationDistribution,	// how to create 
        DWORD           dwFlagsAndAttributes,	// file attributes 
        LPVOID          *lplpWaveFormat,        // Wave Format data
        LPVOID          *lplpInfo,               // Pointer to a pointer for info
        LPVOID          pLog = NULL             // Log pointer
    );
    
    ~CWaveFile();

    DWORD Create(
        LPCTSTR         lpFileName,	            // pointer to name of the file 
        DWORD           dwDesiredAccess,	    // access (read-write) mode 
        DWORD           dwCreationDistribution,	// how to create 
        DWORD           dwFlagsAndAttributes,	// file attributes 
        LPVOID          *lpPcmWaveFormat,       // Wave Format data
        LPVOID          *lplpInfo               // Pointer to a pointer for info  
    );
    
    BOOL ReadData(
        LPVOID          lpWaveData,	            // address of buffer that receives WAVE data
        DWORD           nNumberOfBytesToRead,	// number of bytes to read 
        LPDWORD         lpNumberOfBytesRead 	// address of number of bytes read 
    );
    
    BOOL WriteData(
        LPCVOID         lpWaveData,	            // pointer to WAVE data to write to file 
        DWORD           nNumberOfBytesToWrite,	// number of bytes to write 
        LPDWORD         lpNumberOfBytesWritten	// pointer to number of bytes written 
    );
    
    BOOL    Rewind( );
    DWORD   MoveTo(DWORD dwSample);
    BOOL    End( );
    
    VOID Close( );
    BOOL IsOpen() { return (INVALID_HANDLE_VALUE != hWaveData); };

    DWORD   WaveSize() { return nWaveSize; }
    DWORD   FileSize() { return nRiffSize + 8; }

    DWORD   LastError() { return dwLastError; };
    
protected:
    HANDLE  hWaveData;
    HANDLE  hRiffSize;
    HANDLE  hWaveSize;

    DWORD   nRiffSize;
    DWORD   nWaveSize;
    DWORD   dwDataOffSet;
    DWORD   dwInfoOffSet;

    DWORD   nSamples;

    DWORD   dwLastError;

    PCMWAVEFORMAT   m_PcmWaveFormat;

    inline BOOL     WriteSize(HANDLE hFileSize, DWORD nBytes);

    VOID            *m_pLog;

#ifdef QALOG
    inline BOOL     Log( DWORD dwVerbosity, LPCTSTR szFormat, ...);
    inline BOOL     LogV( DWORD dwVerbosity, LPCTSTR szFormate, va_list pArgs );
#else   
    inline BOOL     Log( DWORD dwVerbosity, LPCTSTR szFormat, ...) { return TRUE;}
    inline BOOL     LogV( DWORD dwVerbosity, LPCTSTR szFormate, va_list pArgs ) { return TRUE;}
#endif    
    
    inline DWORD    WaveFileError( void );
    inline DWORD    WaveFileError( DWORD dwError );
      
    DWORD           GetFormat( LPVOID*          lpWaveFormat, 
                               LPVOID*          lplpInfo );
    DWORD           ReadWaveFormat( LPVOID *lplpWaveFormat );
    DWORD           ReadListInfo( LPVOID *lplpInfo );
    DWORD           ReadFactChunk( void );
    DWORD           InitFile( PCMWAVEFORMAT*    lpWaveFormat, 
                              LPVOID*           lplpInfo );
};

#endif _WAVEFILE_H
