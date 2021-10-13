#include "util.h"

struct ERR 
{
    DWORD   dwMask;
    LPTSTR  lpszLabel;
    LPTSTR  lpszMeaning;

};

ERR g_tError[] = 
{
    CE_BREAK,       TEXT("CE_BREAK"), 
    TEXT("The hardware detected a break condition"),
    CE_FRAME,       TEXT("CE_FRAME"),
    TEXT("The hardware detected a framing error"),
    CE_IOE,         TEXT("CE_IOE"),
    TEXT("An input/output error occurred"),
    CE_MODE,        TEXT("CE_MODE"),
    TEXT("A requested mode is not supported, or the hPort param is invalid"),
    CE_OVERRUN,     TEXT("CE_OVERRUN"),
    TEXT("Character-buffer overrun"),
    CE_RXOVER,      TEXT("CE_RXOVER"),
    TEXT("Receive queue overflow"),
    CE_RXPARITY,    TEXT("CE_RXPARITY"), 
    TEXT("The hardware detected a parity error"),
    CE_TXFULL,      TEXT("CE_TXFULL"),
    TEXT("The WriteComm service was called when the transmit queue was full"),
    CE_DNS,         TEXT("CE_DNS"),
    TEXT("The parallel device is not selected"),
    CE_PTO,         TEXT("CE_PTO"),
    TEXT("Time-out on parallel device"),
    CE_OOP,         TEXT("CE_OOP"),
    TEXT("The parallel device signaled that it is out of paper")
};

#define NUM_ERRORS sizeof(g_tError) / sizeof(ERR)

void PrintCommStatus( DWORD dwMask ) 
{
    DWORD i;        
    g_pKato->Log( LOG_DETAIL, TEXT("Communication Status Flags:") );
    for( i = 0; i < NUM_ERRORS; i++ )
    {
        if( dwMask & g_tError[i].dwMask )
            g_pKato->Log( 
                    LOG_DETAIL, 
                    TEXT("  %s (%s)."), 
                    g_tError[i].lpszMeaning,
                    g_tError[i].lpszLabel );
    }

}

// Log contents of a COMMTIMEOUTS struct
void PrintCommTimeouts( LPCOMMTIMEOUTS lpCt )
{
    g_pKato->Log( LOG_DETAIL, TEXT("COMMTIMEOUTS contents:") );
    g_pKato->Log( LOG_DETAIL, TEXT("   ReadIntervalTimeout: %d"),
        lpCt->ReadIntervalTimeout );
    g_pKato->Log( LOG_DETAIL, TEXT("   ReadTotalTimeoutConstant: %d"),
        lpCt->ReadTotalTimeoutConstant );
    g_pKato->Log( LOG_DETAIL, TEXT("   ReadTotalTimeoutMultiplier: %d"),
        lpCt->ReadTotalTimeoutMultiplier );
    g_pKato->Log( LOG_DETAIL, TEXT("   WriteTotalTimeoutConstant: %d"),
        lpCt->WriteTotalTimeoutConstant );
    g_pKato->Log( LOG_DETAIL, TEXT("   WriteTotalTimeoutMultiplier: %d"),
        lpCt->WriteTotalTimeoutMultiplier );
}

void PrintCommProperties( LPCOMMPROP lpCp )
{
    g_pKato->Log( LOG_DETAIL, TEXT("COMMPROP contents:") );
    g_pKato->Log( LOG_DETAIL, TEXT("   PacketLength: %d"),
            lpCp->wPacketLength );
    g_pKato->Log( LOG_DETAIL, TEXT("   PacketVersion: %d"),
            lpCp->wPacketVersion );
    g_pKato->Log( LOG_DETAIL, TEXT("   MaxTxQueue: %d"),
            lpCp->dwMaxTxQueue );
    g_pKato->Log( LOG_DETAIL, TEXT("   MaxRxQueue: %d"),
            lpCp->dwMaxRxQueue );
    g_pKato->Log( LOG_DETAIL, TEXT("   MaxBaud: %x"),
            lpCp->dwMaxBaud );
    g_pKato->Log( LOG_DETAIL, TEXT("   ProvSubType: %x"),
            lpCp->dwProvSubType );
    g_pKato->Log( LOG_DETAIL, TEXT("   SettableBaud: %x"),
            lpCp->dwSettableBaud );
}

VOID ErrorMessage( DWORD dwMessageId )
{
	LPTSTR msgBuffer;		// string returned from system
	DWORD cBytes;			// length of returned string

	cBytes = FormatMessage(
				FORMAT_MESSAGE_FROM_SYSTEM |
				FORMAT_MESSAGE_ALLOCATE_BUFFER,
				NULL,
				dwMessageId,
				MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
				(TCHAR *)&msgBuffer,
				500,
				NULL );
	if( msgBuffer && cBytes ) {
		msgBuffer[ cBytes ] = TEXT('\0');
		g_pKato->Log(LOG_DETAIL, TEXT( "Error:%s"), msgBuffer );
		LocalFree( msgBuffer );
	}
	else {
		g_pKato->Log(LOG_DETAIL, TEXT( "FormatMessage error: %d"), GetLastError());
	}
}
