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
#include <clparse.h>
#include "AccHelper.h"

static const DWORD DEFAULT_TIMEOUT = 10 * ONE_SECOND;

void Usage()
{    
    LOG( "" );
    LOG( "Accelerometer Data Streaming Application" );
    LOG( "----------------------------------------" );
    LOG( "Usage:");
    LOG( "   accstream <|-s|-r>|<-d (delay)>|<-v>");
    LOG( "        -s: Streaming data mode with Roll Pitch Information");
    LOG( "        -r: Rotation detection mode");
    LOG( "        -d: How many seconds to sample" );
    LOG( "        -v: Enables console logging" );
    LOG( "" );

}//Usage


//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
INT WINAPI WinMain(HINSTANCE /*hi*/, HINSTANCE /*hpi*/, LPWSTR pszCmdLine, INT /*nCmdShow*/)
{
    LOG_START();
   
    CClParse cmdLine(pszCmdLine);
    DWORD dwTimeout = DEFAULT_TIMEOUT;
    BOOL bVerbose = FALSE;
    
    if(cmdLine.GetOpt(L"help") || cmdLine.GetOpt(L"?")) 
    {
        Usage();    
        goto DONE;
    }

    //Check for sample timeout
    if(cmdLine.GetOpt(L"d")  ) 
    {
        if( cmdLine.GetOptVal(L"d", &(dwTimeout)))
        {    
            LOG( "read :%d", dwTimeout );
            if( dwTimeout < 1 )
            {
                dwTimeout = DEFAULT_TIMEOUT;
            }           
            else
            {
                dwTimeout *= ONE_SECOND;
            }
        }
        else
        {
            LOG_WARN( "Invalid Input Parameter - Timeout Value" );
            Usage();        
            goto DONE;
        }
    }

    //Check for verbose flag
    if(cmdLine.GetOpt(L"v")  ) 
    {
        bVerbose = TRUE;
    }

    //Check for rotation or stream. Do only streaming if both selected.  
    if(cmdLine.GetOpt(L"s")  ) 
    {    
        AccDataStream( dwTimeout, bVerbose );
    }
    else if(cmdLine.GetOpt(L"r")  ) 
    {    
        AccDetectRotation( dwTimeout, bVerbose );
    }
    

DONE:
    LOG_END();
    return 0;
}