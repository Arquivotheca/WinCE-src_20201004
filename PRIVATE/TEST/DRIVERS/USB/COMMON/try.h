//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef __TRY_H_
#define __TRY_H_

//#define _TRY_IGNORE_COMMENTS

/*
*   Print out string that everything is fine -- be this verbose
*   only if this is the DEBUG version of the test.
*/
#ifdef  _DEBUG
#ifndef _TRY_IGNORE_COMMENTS
#define     TRY_OK(condition,expression)                \
        {                                               \
            KatoLog( KatoGetDefaultObject(), LOG_COMMENT, \
                TEXT("Expression \"%s\"==\"%s\" SUCCEDED.\r\n"), \
                TEXT(#expression),                      \
                TEXT(#condition)                        \
                );                                      \
        }
#else
#define     TRY_OK(condition,expression)
#endif
#else
#define     TRY_OK(condition,expression)
#endif



/*
*   Print out string that expression failed against the
*   expected condition.  Be this verbose regardless if this
*   is the DEBUG version, but only if user requires it.
*/
#define     TRY_FAIL(condition,expression)              \
        {                                               \
            KatoLog( KatoGetDefaultObject(), LOG_COMMENT, \
                TEXT("Expression \"%s\"==\"%s\" FAILED.\r\n"), \
                TEXT(#expression),                      \
                TEXT(#condition)                        \
                );                                      \
        }


/*
*   Print out string that we are about to execute expression -- be this verbose
*   only if this is the DEBUG version of the test.
*/
#ifdef  _DEBUG
#ifndef _TRY_IGNORE_COMMENTS
#define     TRY_PRINT_EVALUATING(expression)            \
        {                                               \
            KatoLog( KatoGetDefaultObject(), LOG_COMMENT, \
                TEXT("Evaluating \"%s\".\r\n"),         \
                TEXT(#expression)                       \
                );                                      \
        }
#else
#define     TRY_PRINT_EVALUATING(expression)
#endif
#else
#define     TRY_PRINT_EVALUATING(expression)
#endif



/*
*   TRY macro workhorse 
*   -- envelops expression inside try-except
*/
#define     TRY(expression)                             \
    {                                                   \
    BOOL    bException=FALSE;                           \
    DWORD   dwExceptionCode;                            \
        __try                                           \
        {                                               \
            expression;                                 \
        }                                               \
        __except(EXCEPTION_EXECUTE_HANDLER)             \
        {                                               \
            bException      = TRUE;                     \
            dwExceptionCode = _exception_code();        \
        }                                               \
        if ( bException )                               \
        {                                               \
            DWORD dwLastError = GetLastError();         \
            KatoLog( KatoGetDefaultObject(), LOG_FAIL,  \
                TEXT("Expression \"%s\" FAILed, generated exception %u. GetLastError()=%u.\r\n"), \
                TEXT(#expression),                      \
                dwExceptionCode,                        \
                dwLastError                             \
                );                                      \
        }                                               \
    }


/*
*   TRY macro, verbose version 
*   -- envelops condition AND expression inside try-except
*/
#define     TRY_EXCEPT_VERBOSE(condition,expression)    \
    {                                                   \
    BOOL    bRet=FALSE;                                 \
    BOOL    bCondition=TRUE;                            \
        TRY(bCondition = (condition) ? TRUE : FALSE);   \
        TRY_PRINT_EVALUATING(expression);               \
        TRY(bRet = (expression) ? TRUE : FALSE);        \
        if ( bRet != bCondition )                       \
        {                                               \
            TRY_FAIL(condition,expression);             \
        }                                               \
        else                                            \
        {                                               \
            TRY_OK(condition,expression);               \
        }                                               \
    }


/*
*   TRY macro, shorter but still verbose version 
*   -- envelops ONLY expression inside try-except
*/
#define     TRY_EXCEPT(condition,expression)            \
    {                                                   \
    BOOL    bRet=FALSE;                                 \
    BOOL    bCondition=TRUE;                            \
        bCondition = (condition) ? TRUE : FALSE;        \
        TRY(bRet = (expression) ? TRUE : FALSE);        \
        if ( bRet != bCondition )                       \
        {                                               \
            TRY_FAIL(condition,expression);             \
        }                                               \
        else                                            \
        {                                               \
            TRY_OK(condition,expression);               \
        }                                               \
    }


/*
*   TRY macro, shorter but still verbose version 
*   -- envelops ONLY expression inside try-except
*   prints only if condition failed
*/
#define     TRY_PRINT_FAILED(condition,expression)      \
    {                                                   \
    BOOL    bRet=FALSE;                                 \
    BOOL    bCondition=TRUE;                            \
        bCondition = (condition) ? TRUE : FALSE;        \
        TRY(bRet = (expression) ? TRUE : FALSE);        \
        if ( bRet != bCondition )                       \
        {                                               \
            TRY_FAIL(condition,expression);             \
        }                                               \
    }


#endif  //__USBFN_H_
