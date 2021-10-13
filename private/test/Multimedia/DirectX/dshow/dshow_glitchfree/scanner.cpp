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
#include "Scanner.h"

CScanner::CScanner()
{
}

CScanner::~CScanner()
{
    // If we have an open file
    if (m_ifs != NULL)
    {
        // Close it
        fclose(m_ifs);
    }
}

bool
CScanner::Init(const char *szFilename)
{
    // Try to open the file
    m_ifs = NULL;
    errno_t err = fopen_s(&m_ifs, szFilename, "r");
    if(err!=0)
    {
        // Report it
        return false;
    }
    else
    {       
        // If we didn't succeeded
        if (!m_ifs)
        {
            // Report it
            return false;
        }
    }
    
    // Advance reading the file
    return Advance();
}

bool
CScanner::SkipWhiteSpace()
{
    // I'm considering the following characters as white space: 
    // space character (0x20), carriage return(0x9), line feed(0xD) and tab(0xA)
    while (1)
    {
        // Break if current character is not a white space
        if (m_ch != 0x20 && m_ch != 0x9 && m_ch != 0xD && m_ch != 0xA)
        {
            return true;
        }

        // If we couldn't advance reading the file, something went wrong
        if (!Advance())
        {
            // Report failure
            return false;
        }
    }
}

//go to next XML tag

bool
CScanner::Advance()
{
    // If we are not at the end of the file
    if (!feof(m_ifs))
    {
        // Read next character
        m_ch = (char)fgetc(m_ifs);

        // And we are done
        return true;
    }

    // We cannot advance because we reached the end of the file
    return false;
}

bool
CScanner::Match(const char ch)
{
    // Is the input character equals to the current character?
    return m_ch == ch;
}

bool
CScanner::GetName(char **ppszName, int *pcchName)
{
    if (!ppszName || !pcchName)
    {
        return false;
    }

    *ppszName = NULL;
    *pcchName = 0;

    const int BUFFER_SIZE = 1024;
    char pszName[BUFFER_SIZE];
    int cchName = 0;

    while (1)
    {
        if (feof(m_ifs))
        {
            return false;
        }

        if (m_ch == 0x20 || m_ch == 0x9 || m_ch == 0xD || m_ch == 0xA || m_ch == '/' || m_ch == '>')
        {
            break;
        }

        if(cchName < BUFFER_SIZE)
            pszName[cchName++] = m_ch;
        else
            return false;

        if (!Advance())
        {
            return false;
        }
    }

    if (cchName)
    {
        *ppszName = new char[cchName + 1];
        if (!*ppszName)
        {
            return false;
        }

        errno_t err = strncpy_s(*ppszName, _countof(pszName), pszName, cchName + 1);
        if(err!=0)
        {
            return false;
        }
        else
        {
            (*ppszName)[cchName] = '\0';
            *pcchName = cchName;
        }
    }

    return true;
}

bool
CScanner::GetAttributeName(char **ppszName, int *pcchName)
{
    if (!ppszName || !pcchName)
    {
        return false;
    }

    *ppszName = NULL;
    *pcchName = 0;

    const int BUFFER_SIZE = 1204;
    char pszName[BUFFER_SIZE];
    int cchName = 0;

    while (1)
    {
        if (feof(m_ifs))
        {
            return false;
        }

        if (m_ch == 0x20 || m_ch == 0x9 || m_ch == 0xD || m_ch == 0xA || m_ch == '=')
        {
            break;
        }

        if(cchName < BUFFER_SIZE)
            pszName[cchName++] = m_ch;
        else 
            return false;

        if (!Advance())
        {
            return false;
        }
    }

    if (cchName)
    {
        *ppszName = new char[cchName + 1];
        if (!*ppszName)
        {
            return false;
        }

        errno_t err = strncpy_s(*ppszName, _countof(pszName), pszName, cchName + 1);
        if(err!=0)
        {
            return false;
        }
        else
        {
            (*ppszName)[cchName] = '\0';
            *pcchName = cchName;
        }
    }

    return true;
}

//return the value of this XML attribute

bool
CScanner::GetAttributeValue(char **ppszValue, int *pcchValue)
{
    if (!ppszValue || !pcchValue)
    {
        return false;
    }

    *ppszValue = NULL;
    *pcchValue = 0;

    const int BUFFER_SIZE = 1024;
    char szValue[BUFFER_SIZE];
    int  cchValue = 0;

    while (1)
    {
        if (feof(m_ifs))
        {
            return false;
        }

        if (m_ch == '\'' || m_ch == '\"')
        {
            break;
        }

        if(cchValue < BUFFER_SIZE)
            szValue[cchValue++] = m_ch;
        else 
            return false;

        if (!Advance())
        {
            return false;
        }
    }

    if (cchValue)
    {
        *ppszValue = new char[cchValue + 1];
        if (!*ppszValue)
        {
            return false;
        }

        errno_t err = strncpy_s(*ppszValue, _countof(szValue), szValue, cchValue + 1);
        if(err!=0)
        {
            return false;
        }
        else
        {
            (*ppszValue)[cchValue] = '\0';
            *pcchValue = cchValue;
        }
    }

    return true;
}

bool
CScanner::GetCharacters(char **ppszCharacters, int *pcchCharacters)
{
    if (!ppszCharacters || !pcchCharacters)
    {
        return false;
    }

    *ppszCharacters = NULL;
    *pcchCharacters = 0;

    char pszCharacters[1024];
    int cchCharacters = 0;

    while (1)
    {
        if (feof(m_ifs))
        {
            return false;
        }

        if (m_ch == '<')
        {
            break;
        }

        pszCharacters[cchCharacters++] = m_ch;
        if (!Advance())
        {
            return false;
        }
    }

    if (cchCharacters)
    {
        *ppszCharacters = new char[cchCharacters + 1];
        if (!*ppszCharacters)
        {
            return false;
        }

        errno_t err = strncpy_s(*ppszCharacters, _countof(pszCharacters), pszCharacters, cchCharacters + 1);
        if(err!=0)
        {
            return false;
        }
        else
        {
            (*ppszCharacters)[cchCharacters] = '\0';
            *pcchCharacters = cchCharacters;
        }
    }

    return true;
}
