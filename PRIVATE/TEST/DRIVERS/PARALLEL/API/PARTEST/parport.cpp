#include "parport.h"

#define __FILE_NAME__   TEXT("PARPORT.CPP")

//*****************************************************************************
PARPORT::PARPORT() :
    m_hParallel( INVALID_HANDLE_VALUE )
//*****************************************************************************   
{ ; }

//*****************************************************************************
PARPORT::~PARPORT()
//*****************************************************************************
{ ; }

//*****************************************************************************
BOOL 
PARPORT::Open( )
//*****************************************************************************
{
    return Open( g_driverName );
}

//*****************************************************************************
BOOL 
PARPORT::Open( 
    LPTSTR psz )
//*****************************************************************************    
{
    //
    // open handle to the parallel port
    //
    m_hParallel = CreateFile( psz, 
                              GENERIC_READ | GENERIC_WRITE,
                              0, 
                              NULL,
                              OPEN_EXISTING,
                              0, 
                              NULL );
    //
    // make sure the handle is valid
    //
    if( INVALID_HANDLE_VALUE == m_hParallel )
    {
        FAIL("CreateFile");
        ErrorMessage( GetLastError() );
    }
    
    return ( INVALID_HANDLE_VALUE != m_hParallel );
}

//*****************************************************************************
BOOL 
PARPORT::Close( )
//*****************************************************************************
{
    BOOL fRet = CloseHandle( m_hParallel );

    //
    // If the close handle failed, log it
    //
    if( !fRet )
    {
        FAIL("CloseHandle");
        ErrorMessage( GetLastError() );
    }

    //
    // Otherwise, reset the handle value
    //
    else
    {
        m_hParallel = INVALID_HANDLE_VALUE;
    }
    
    return fRet;
}

//*****************************************************************************
BOOL 
PARPORT::GetTimeouts( 
    LPCOMMTIMEOUTS pCtm, 
    BOOL fUseWinAPI )
//*****************************************************************************    
{
    BOOL fRet = TRUE;
    
    //
    // User requested to use the Windows API to get the timeouts
    //
    if( fUseWinAPI )
    {
        fRet = GetCommTimeouts( m_hParallel, pCtm );
        
        //
        // if the operation failed, log an error condition
        //
        if( !fRet )
        {
            FAIL("GetCommTimeouts" );
            ErrorMessage( GetLastError() );
        }           
    }

    //
    // Use the IOCTL code to get the timeouts by default
    //
    else
    {
        DWORD dwBytes;
        fRet = DeviceIoControl( m_hParallel, 
                                IOCTL_PARALLEL_GET_TIMEOUTS,
                                NULL, 
                                0,
                                pCtm, 
                                sizeof(COMMTIMEOUTS),
                                &dwBytes, 
                                NULL );
        //
        // if the operation failed, log an error condition
        //
        if( !fRet )
        {
            FAIL("DeviceIoControl(IOCTL_PARALLEL_GET_TIMEOUTS)" );
            ErrorMessage( GetLastError() );
        }    
        
        //
        // If the operation succeeded, make sure that the dwBytes value 
        // was set correctly and flag an error if it was not
        //
        else if( sizeof(COMMTIMEOUTS) != dwBytes )
        {
            //
            // Signal failure and log it
            //
            fRet = FALSE;
            FAIL("DeviceIoControl(IOCTL_PARALLEL_GET_TIMEOUTS) returned incorrect write buffer size"); 
        }
    }    
     
    return fRet;
}

//*****************************************************************************
BOOL 
PARPORT::SetTimeouts( 
    LPCOMMTIMEOUTS pCtm,
    BOOL fUseWinAPI )
//*****************************************************************************    
{
    BOOL fRet = TRUE;
    
    // 
    // User requested to use the Windows API to set the timeouts
    //
    if( fUseWinAPI )
    {
        fRet = SetCommTimeouts( m_hParallel, pCtm );
        
        //
        // if the operation failed, log an error condition
        //
        if( !fRet )
        {
            FAIL("SetCommTimeouts)" );
            ErrorMessage( GetLastError() );
        }
    }

    //
    // Use the IOCTL code to set the timeouts by default
    //
    else
    {
        DWORD dwBytes = 0;
        fRet = DeviceIoControl( m_hParallel,
                                IOCTL_PARALLEL_SET_TIMEOUTS,
                                pCtm, 
                                sizeof(COMMTIMEOUTS),
                                NULL, 
                                0,
                                &dwBytes, 
                                NULL );

        //
        // if the operation failed, log an error condition
        //
        if( !fRet )
        {
            FAIL("DeviceIoControl(IOCTL_PARALLEL_SET_TIMEOUTS)" );
            ErrorMessage( GetLastError() );
        }  

// 
// for setting the COMMTIMEOUTS, neither the serial or the parallel driver
// set the lpBytesReturned parameter -- this may be a 't think
// so. So, we don't check the value for correctness. The code is here in case
// we want to test for this case
//
#if 0        
        //
        // Make sure that the dwBytes value was set correctly
        //
        else if( sizeof(COMMTIMEOUTS) != dwBytes )
        {
            //
            // Signal failure and log it
            //
            fRet = FALSE;
            FAIL("DeviceIoControl(IOCTL_PARALLEL_SET_TIMEOUTS) returned incorrect write buffer size");
        }
    
#endif

    }
    return fRet;
}

//*****************************************************************************
BOOL 
PARPORT::GetDeviceID( 
    LPBYTE pDId, 
    DWORD dwLength, 
    LPDWORD pdwBytes )
//*****************************************************************************    
{
    BOOL fRet = TRUE;
    
    //
    // Since dwLength varies depending on the length of the actual device Id
    // string, we don't bother checking it and pass it straight back to the
    // caller
    //
    fRet = DeviceIoControl( m_hParallel,
                            IOCTL_PARALLEL_GETDEVICEID,
                            NULL, 
                            0,
                            pDId, 
                            dwLength,
                            pdwBytes, 
                            NULL );
    //
    // if the operation failed, log an error condition
    //
    if( !fRet )
    {
        FAIL("DeviceIoControl(IOCTL_PARALLEL_GETDEVICEID)" );

        //
        // Doesn't report error correctly
        //
        //ErrorMessage( GetLastError() );
    }

    return fRet;                            
}

//*****************************************************************************
BOOL 
PARPORT::GetStatus( 
    LPDWORD pdwStatus )
//*****************************************************************************    
{
    DWORD dwBytes;
    
    BOOL fRet = DeviceIoControl( m_hParallel, 
                                 IOCTL_PARALLEL_STATUS,
                                 NULL, 
                                 0,
                                 pdwStatus, 
                                 sizeof(DWORD),
                                 &dwBytes, 
                                 NULL );
    //
    // if the operation failed, log an error condition
    //
    if( !fRet )
    {
        FAIL("DeviceIoControl(IOCTL_PARALLEL_STATUS)" );
        ErrorMessage( GetLastError() );
    }
    
    //
    // Make sure dwBytes is correct
    //
    else if( sizeof(DWORD) != dwBytes )
    {
        //
        // Signal and log if dwBytes is the wrong value
        //
        fRet = FALSE;
        FAIL("DeviceIoControl(IOCTL_PARALLEL_STATUS) returned incorrect read buffer size");
    }

    return fRet;   
}

//*****************************************************************************
BOOL 
PARPORT::GetECPChannel32( 
    LPDWORD pdwChan32 )
//*****************************************************************************    
{
    DWORD dwBytes;
    
    BOOL fRet = DeviceIoControl( m_hParallel,
                                 IOCTL_PARALLEL_GET_ECP_CHANNEL32,
                                 NULL, 0,
                                 pdwChan32, sizeof(DWORD),
                                 &dwBytes, NULL );

    //
    // if the operation failed, log an error condition
    //
    if( !fRet )
    {
        FAIL("DeviceIoControl(IOCTL_PARALLEL_GET_ECP_CHANNEL32)" );
        ErrorMessage( GetLastError() );
    }
    
    //
    // Make sure dwBytes is correct
    //
    else if( sizeof(DWORD) != dwBytes )
    {
        //
        // Signal and log if dwBytes is the wrong value
        //
        fRet = FALSE;
        FAIL("DeviceIoControl(IOCTL_PARALLEL_GET_ECP_CHANNEL32) returned incorrect read buffer size");
    }
    
    return fRet;
}

//*****************************************************************************
BOOL 
PARPORT::Write( 
    LPBYTE pbBuffer, 
    DWORD dwBytes, 
    LPDWORD pdwWritten,
    BOOL fUseWinAPI )
//*****************************************************************************    
{
    BOOL fRet = TRUE;
    
    //
    // Use the Windows API WriteFile() call to write to the port
    //
    if( fUseWinAPI )
    {    
        fRet = WriteFile( m_hParallel, 
                          pbBuffer, 
                          dwBytes, 
                          pdwWritten,
                          NULL );
                          
        //
        // if the operation failed, log an error condition
        //
        if( !fRet )
        {
            FAIL("WriteFile");
            ErrorMessage( GetLastError() );
        }
    }

    //
    // Use the IOCTL to write to the port by default
    //
    else
    {
        fRet = DeviceIoControl( m_hParallel,
                                IOCTL_PARALLEL_WRITE,
                                pbBuffer,
                                dwBytes,
                                NULL,
                                0,
                                pdwWritten,
                                NULL );
    
    
        //
        // if the operation failed, log an error condition
        //
        if( !fRet )
        {
            FAIL("DeviceIoControl(IOCTL_PARALLEL_WRITE)" );
            ErrorMessage( GetLastError() );
        }
        else
        {
            *pdwWritten = dwBytes;
        }
    }
    
    return fRet;
}

//*****************************************************************************
BOOL 
PARPORT::Read( 
    LPBYTE pbBuffer, 
    DWORD dwBytes, 
    LPDWORD pdwRead )
//*****************************************************************************    
{
    //
    // There is no option for a direct IOCTL to read from the port. In fact,
    // reading from the port always returns an empty buffer and dwBytes == 0.
    // The driver apparently does not support bi-directional communication 
    // so this function really serves no purpose at all.
    //
    return ReadFile ( m_hParallel,
                      pbBuffer,
                      dwBytes,
                      pdwRead,
                      NULL );
}
