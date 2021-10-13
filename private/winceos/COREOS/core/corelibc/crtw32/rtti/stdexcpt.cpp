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
/***
*stdexcpt.cpp - defines C++ standard exception classes
*
*
*Purpose:
*       Implementation of C++ standard exception classes, as specified in
*       [lib.header.exception] (section 17.3.2 of 5/27/94 WP):
*
*        exception (formerly xmsg)
*          logic
*            domain
*          runtime
*            range
*            alloc
*
*******************************************************************************/

#include <string.h>
#include <exception>

////////////////////////////////////////////////////////////////////////////////
//
// Implementation of class "exception"
//

//
// Default constructor - initialize to blank
//
std::exception::exception ()
{
        _m_what = NULL;
        _m_doFree = 0;
}

//
// Standard constructor: initialize with copy of string
//
std::exception::exception ( const char* what )
{
        size_t _Buf_size = strlen( what ) + 1;
        _m_what = new char[_Buf_size];
        if ( _m_what != NULL )
            memcpy( (char*)_m_what, what, _Buf_size );
        _m_doFree = 1;
}

//
// Copy constructor
//
std::exception::exception ( const std::exception & that )
{
        _m_doFree = that._m_doFree;
        if (_m_doFree)
        {
            size_t _Buf_size = strlen( that._m_what ) + 1;
            _m_what = new char[_Buf_size];
            if (_m_what != NULL)
                memcpy( (char*)_m_what, that._m_what, _Buf_size );
        }
        else
           _m_what = that._m_what;
}

//
// Assignment operator: destruct, then copy-construct
//
std::exception& std::exception::operator=( const std::exception& that )
{
        if (this != &that)
        {
            this->exception::~exception();
            this->exception::exception(that);
        }
        return *this;
}

//
// Destructor: free the storage used by the message string if it was
// dynamicly allocated
//
std::exception::~exception()
{
        if (_m_doFree)
            delete[] (char*)_m_what;
}


//
// exception::what
//  Returns the message string of the exception.
//  Default implementation of this method returns the stored string if there
//  is one, otherwise returns a standard string.
//
const char* std::exception::what() const
{
        if ( _m_what != NULL )
            return _m_what;
        else
            return "Unknown exception";
}


