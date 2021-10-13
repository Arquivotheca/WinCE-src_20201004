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
#include "radiotest.h"
#include "framework.h"
#include "radiotest_metric.h"

CMetrics* General_Metric_Init (void)
{
    General_Metric   *metric;

    metric = new General_Metric();

    return metric;
}

General_Metric::General_Metric (void)
{
    return;
}

General_Metric::~General_Metric (void)
{
    return;
}

void General_Metric::PrintHeader (void)
{
    CMetrics::PrintHeader();
    PrintBorder();
}

void General_Metric::PrintSummary (void)
{
    CMetrics::PrintSummary();
    g_pLog->Log( UI_LOG | LOG_MASK_METRIC_FILE, TEXT( "%-30.30s = %d out of %d (%d%%)" ),
                                TEXT( "Tests completed" ),
                                dwNumTestsPassed + dwNumTestsFailed,
                                dwNumTestsTotal,
                                dwNumTestsTotal ? ((dwNumTestsPassed + dwNumTestsFailed) * 100 + (dwNumTestsTotal/2))/dwNumTestsTotal : 0);
    g_pLog->Log( UI_LOG | LOG_MASK_METRIC_FILE, TEXT( "%-30.30s = %d (%d%%)" ),
                                TEXT( "Tests passed "), dwNumTestsPassed,
                                dwNumTestsTotal ? ((dwNumTestsPassed * 100) + (dwNumTestsTotal/2))/dwNumTestsTotal : 0);

    PrintBorder();
}

void General_Metric::PrintCheckPoint(void)
{
    CMetrics::PrintCheckPoint();
   g_pLog->Log( UI_LOG, TEXT( "%-30.30s = %d out of %d (%d%%)" ),
                                TEXT( "Tests completed" ),
                                dwNumTestsPassed + dwNumTestsFailed,
                                dwNumTestsTotal,
                                dwNumTestsTotal ? ((dwNumTestsPassed + dwNumTestsFailed) * 100 + (dwNumTestsTotal/2))/dwNumTestsTotal : 0);
  g_pLog->Log( UI_LOG, TEXT( "%-30.30s = %d out of %d (%d%%)"),
                                TEXT( "Tests Passed" ),
                                dwNumTestsPassed,
                                dwNumTestsTotal,
                                dwNumTestsTotal ? ((dwNumTestsPassed  * 100) + (dwNumTestsTotal/2))/dwNumTestsTotal : 0);
    PrintBorder();
    g_pLog->Log( UI_LOG, TEXT( "\n" ) );
}

