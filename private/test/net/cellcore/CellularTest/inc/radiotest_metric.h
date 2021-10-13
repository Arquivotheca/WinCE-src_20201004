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


#ifndef _RADIOTEST_METRIC_H
#define _RADIOTEST_METRIC_H

#include "radiotest.h"
#include "tux.h"

#define PrintBorder()\
    g_pLog->Log( LOG, \
    TEXT( "---------------------------------------------------------------" ) );

struct CaseInfo
{
    DWORD dwCaseId;
    DWORD dwUserData;
};

// Abstract class for all metrics
class CMetrics
{
public:

    TCHAR   tszMetric_Name[METRIC_NAME_LENGTH + 1];
    DWORD   dwNumTestsTotal;
    DWORD   dwNumTestsPassed;
    DWORD   dwNumTestsFailed;
    DWORD   dwNumConsecutiveTestsFailed;

    DWORD   dwNumLongCallsPassed;

    DWORD   dwGPRSThroughput;
    DWORD   dwGPRSTransferPercent;
    DWORD   dwGPRSConnectionsMade;
    DWORD   dwGPRSConnectionsFailed;
    DWORD   dwGPRSURLFailed;
    DWORD   dwGPRSTransferMs;
    DWORD   dwGPRSBytesTransferred;
    DWORD   dwGPRSByteExpected;
    DWORD   dwGPRSTimeouts;

    DWORD   dwCSDThroughput;
    DWORD   dwCSDTransferPercent;
    DWORD   dwCSDConnectionsMade;
    DWORD   dwCSDConnectionsFailed;
    DWORD   dwCSDURLFailed;
    DWORD   dwCSDTransferMs;
    DWORD   dwCSDBytesTransferred;
    DWORD   dwCSDByteExpected;
    DWORD   dwCSDTimeouts;

    DWORD   dwRadioResets;
    BOOL    fPassDuration;
    BOOL    fTooManyFails;
    UINT64  uliTestStartTime;
    UINT64  uliTestEndTime;

    HANDLE  m_hMetricFile;

    CMetrics();
    virtual ~CMetrics();

    virtual void PrintHeader(void);
    virtual void PrintSummary(void);
    virtual void PrintCheckPoint(void);
    virtual BOOL LoadTestSuites(FUNCTION_TABLE_ENTRY * &pFTE);

protected:
    FUNCTION_TABLE_ENTRY * m_pFTE;
    DWORD m_dwCaseNumber;
    BOOL LoadCaseList(const CaseInfo *pList);
    DWORD FLAG_OF_CASELIST;

private:

    BOOL    CreateMetricFile(TCHAR *tszPath);
    void    PrintAppVersion(void);
    void    PrintRadioInfo(TCHAR *tszManufacturer, TCHAR* tszModel, TCHAR* tszRevision);
    void    PrintDeviceInfo(void);

};

// General functionality metric
class General_Metric : public CMetrics
{
public:

    General_Metric(void);
    ~General_Metric(void);

    void PrintHeader(void);
    void PrintSummary(void);
    void PrintCheckPoint(void);
};

// Stress metric
class StressMetrics : public CMetrics
{
public:

    StressMetrics(void);
    ~StressMetrics(void);
    void PrintHeader(void);
    void PrintSummary(void);
    void PrintCheckPoint(void);
    BOOL LoadTestSuites(FUNCTION_TABLE_ENTRY * &pFTE);
};

// LTK metric
class LTKMetrics : public CMetrics
{
public:

    LTKMetrics(void);
    ~LTKMetrics(void);
    void PrintHeader(void);
    void PrintSummary(void);
    void PrintCheckPoint(void);
    BOOL LoadTestSuites(FUNCTION_TABLE_ENTRY * &pFTE);
};

// Mobility metric
class MobilityMetrics : public CMetrics
{
public:

    MobilityMetrics(void);
    ~MobilityMetrics(void);
    void PrintHeader(void);
    void PrintSummary(void);
    void PrintCheckPoint(void);
    BOOL LoadTestSuites(FUNCTION_TABLE_ENTRY * &pFTE);
};

// Network fault injection metrics
class NFIMetrics : public CMetrics
{
public:

    NFIMetrics(void);
    ~NFIMetrics(void);
    void PrintHeader(void);
    void PrintSummary(void);
    void PrintCheckPoint(void);
    BOOL LoadTestSuites(FUNCTION_TABLE_ENTRY * &pFTE);
};

class AllCasesMetrics : public CMetrics
{
public:

    AllCasesMetrics(void);
    ~AllCasesMetrics(void);
    void PrintHeader(void);
    void PrintSummary(void);
    void PrintCheckPoint(void);
    BOOL LoadTestSuites(FUNCTION_TABLE_ENTRY * &pFTE);
};

class BTSMetrics : public CMetrics
{
public:

    BTSMetrics(void);
    ~BTSMetrics(void);
    void PrintHeader(void);
    void PrintSummary(void);
    void PrintCheckPoint(void);
    BOOL LoadTestSuites(FUNCTION_TABLE_ENTRY * &pFTE);
};

extern CMetrics *g_pMetrics;
#endif
