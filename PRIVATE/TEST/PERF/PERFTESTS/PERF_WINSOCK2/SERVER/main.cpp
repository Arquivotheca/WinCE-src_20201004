//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// Use of this source code is subject to the terms of your Microsoft Windows CE
// Source Alliance Program license form.  If you did not accept the terms of
// such a license, you are not authorized to use this source code.
//
// main.cpp

#include "server.h"

INT _tmain(INT argc, TCHAR* argv[])
{
    ServiceStart(0, NULL);
    //ServiceStop();
    return 0;
}
