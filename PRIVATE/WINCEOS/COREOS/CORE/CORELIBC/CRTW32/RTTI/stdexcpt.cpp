//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
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
        _m_what = new char[strlen(what)+1];
        if ( _m_what != NULL )
            strcpy( (char*)_m_what, what );
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
            _m_what = new char[strlen(that._m_what) + 1];
            if (_m_what != NULL)
                strcpy( (char*)_m_what, that._m_what );
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


