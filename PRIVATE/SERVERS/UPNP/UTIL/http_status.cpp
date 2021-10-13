//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//

#include "windows.h"
#include "http_status.h"
#include "assert.h"

namespace ce
{

const char* http_status_string(int status, bool bNoNumber/* = false*/)
{
    const char* psz;
    char        buff[21];
    
    switch(status)
    {
        case 100: psz = "100 Continue"; break;
        case 101: psz = "101 Switching Protocols"; break;
        
        case 200: psz = "200 OK"; break;
        case 201: psz = "201 Created"; break;
        case 202: psz = "202 Accepted"; break;
        case 203: psz = "203 Non-Authoritative Information"; break;
        case 204: psz = "204 No Content"; break;
        case 205: psz = "205 Reset Content"; break;
        case 206: psz = "206 Partial Content"; break;
        
        case 300: psz = "300 Multiple Choices"; break;
        case 301: psz = "301 Moved Permanently"; break;
        case 302: psz = "302 Found"; break;
        case 303: psz = "303 See Other"; break;
        case 304: psz = "304 Not Modified"; break;
        case 305: psz = "305 Use Proxy"; break;
        case 307: psz = "307 Temporary Redirect"; break;
        
        case 400: psz = "400 Bad Request"; break;
        case 401: psz = "401 Unauthorized"; break;
        case 402: psz = "402 Payment Required"; break;
        case 403: psz = "403 Forbidden"; break;
        case 404: psz = "404 Not Found"; break;
        case 405: psz = "405 Method Not Allowed"; break;
        case 406: psz = "406 Not Acceptable"; break;
        case 407: psz = "407 Proxy Authentication Required"; break;
        case 408: psz = "408 Request Timeout"; break;
        case 409: psz = "409 Conflict"; break;
        case 410: psz = "410 Gone"; break;
        case 411: psz = "411 Length Required"; break;
        case 412: psz = "412 Precondition Failed"; break;
        case 413: psz = "413 Request Entity Too Large"; break;
        case 414: psz = "414 Request URI Too Long"; break;
        case 415: psz = "415 Unsupported Media Type"; break;
        case 416: psz = "416 Requested range not satisfiable"; break;
        case 417: psz = "417 Expectation Failed"; break;
        
        case 500: psz = "500 Internal Server Error"; break;
        case 501: psz = "501 Not Implemented"; break;
        case 502: psz = "502 Bad Gateway"; break;
        case 503: psz = "503 Service Unavailable"; break;
        case 504: psz = "504 Gateway Timeout"; break;
        case 505: psz = "505 HTTP Version Not Supported"; break;
        
        default: assert(0);
                 _itoa(status, buff, 10);
                 psz = buff;
                 break;
    }
    
    return bNoNumber ? psz + 4 : psz;
}

};
