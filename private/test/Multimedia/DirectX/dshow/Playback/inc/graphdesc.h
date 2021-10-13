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
////////////////////////////////////////////////////////////////////////////////

#ifndef GRAPH_DESC_H
#define GRAPH_DESC_H

#include <windows.h>
#include <vector>

class ConnectionDesc;
typedef std::vector<ConnectionDesc*> ConnectionDescList;
typedef std::vector<TCHAR*> StringList;

// This describes a specific connection between 2 pins in a graph
class ConnectionDesc
{
public:
    TCHAR fromFilter[FILTER_NAME_LENGTH];
    TCHAR toFilter[FILTER_NAME_LENGTH];
    int fromPin;
    int toPin;

public:
    TCHAR* GetFromFilter();
    TCHAR* GetToFilter();
    int GetFromPin();
    int GetToPin();
};

// This contains a description of the filters in the graph and the connection structure
class GraphDesc
{
public:
    ConnectionDescList connectionDescList;
    StringList filterList;

public:
    GraphDesc();
    ~GraphDesc();
    
    // Get the number of filters in the list
    int GetNumFilters();

    // Get the list of filters
    StringList GetFilterList();

    // The number of connections
    int GetNumConnections();

    // Get the list of connections
    ConnectionDescList* GetConnectionList();
};

#endif


#if 0
class ConnectionDescList
{
private:
    vector<ConnectionDesc*> connectionList;

public:
    
};
#endif