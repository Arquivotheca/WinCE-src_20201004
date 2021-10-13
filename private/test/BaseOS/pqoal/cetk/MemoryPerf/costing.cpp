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


// ***************************************************************************************************************
//
// This set of functions calculates the scenario cost of the device's memory hierarchy configuration and latencies
//
// ***************************************************************************************************************


#include "MemoryPerf.h"
#include "globals.h"


// ***************************************************************************************************************
// some data structures needed to configure and calculate the score

typedef struct {
    float m_fPercentInst;
    float m_fPercentRead;
    float m_fPercentWrite;
} tsAccessDistribution;

tsAccessDistribution tsAccessTypes = { 76.23f, 16.05f, 7.73f };         // Contacts Launch with 250 Contacts from Feb 17, 2009 DELog

typedef struct {
    // instruction Reads
    float m_fL1Inst;
    float m_fL2Inst;
    float m_fMBInst;
    // cached Data reads
    float m_fL1Data;
    float m_fL2Data;
    float m_fMBData;
    // cached Data writes
    float m_fL1Write;
    float m_fL2Write;
    float m_fMBWrite;
    // RAS transitions
    float m_fRASExtraInst;
    float m_fRASExtraData;
} tsHitRates;

// two typical configurations based on line sizes
//                                    L1Inst  L2Inst MBInst  L1Read, L2Read MBRead  L1Write L2Writ MBWrit  RasExtra  
const tsHitRates tsHitRates32_128 = { 99.03f, 0.81f, 0.16f,  99.41f, 0.43f, 0.16f,  99.07f, 0.68f, 0.25f,  0.03f, 0.09f };
const tsHitRates tsHitRates64_64  = { 99.23f, 0.52f, 0.25f,  99.60f, 0.22f, 0.18f,  98.95f, 0.76f, 0.29f,  0.04f, 0.12f };

typedef struct {
    int   iTLBSize;
    float m_fInstWalkRate;
    float m_fDataWalkRate;
} tsWalkRate;

// typical configurations used to interpolate page walk rates based on TLB size.
tsWalkRate taWalkRates[] = {
                                {       1, 0.25f, 0.50f },       // assume something pretty bad if TLB size is unknown
                                {      32, 0.14f, 0.13f },       // assume really 32 data and separate 32 instruction
                                {      40, 0.18f, 0.35f },       // 8 associative and 32 1-way set-associative
                                {      64, 0.09f, 0.15f },       // 64 fully associative
                                {      72, 0.09f, 0.18f },       // 8 associative and 64 2-way set-associative
                                { INT_MAX, 0.05f, 0.13f }        // terminate the loop
                            };

class tComponents {
    friend float Cost( const tComponents &Weights, const tComponents &Latencies, float &fCachedCost, float &fUncachedCost, float &fDDSCost, const char * strLabel );
public:
    tComponents( const int iL2LineSize, const int iTLBSize, const char * strLabel );
    tComponents( const bool bTarget, const char * strLabel );
    // Cached memory reads
    float m_fL1;
    float m_fL2;
    float m_fMB;
    // cached memory writes
    float m_fL1Write;
    float m_fL2Write;
    float m_fMBWrite;
    // for all accesses of all types
    float m_fRASExtra;
    float m_fPageWalk;
    // uncached weights
    float m_fUCMB;
    float m_fUCMBWrite;
    // DDraw Surface Costs
    float m_fDSMB;
    float m_fDSMBWrite;
};


// ***************************************************************************************************************
// In the Scoring expressions below the basic formula combines percentage of accesses from the scenario as
//  <Instruction Percent><Hit Rate><Costing Weight> + <Read Percent><Hit Rate><Costing Weight> + <Write Percent><Hit Rate><Costing Weight>
// where <Costing Weight> may be 1 is the CPU will be stalled or quite small if accesses can happen in parallel without stalling the CPU
// ***************************************************************************************************************

tComponents::tComponents( const int iL2LineSize, const int iTLBSize, const char * strLabel )
{
    const tsHitRates * ptsHitsRates = 128==iL2LineSize ? &tsHitRates32_128 : &tsHitRates64_64;
    // L1 instruction and data reads that hit L1                 
    m_fL1 = tsAccessTypes.m_fPercentInst*ptsHitsRates->m_fL1Inst + tsAccessTypes.m_fPercentRead*ptsHitsRates->m_fL1Data;
    // L2 instruction and data reads that hit L2
    m_fL2 = tsAccessTypes.m_fPercentInst*ptsHitsRates->m_fL2Inst + tsAccessTypes.m_fPercentRead*ptsHitsRates->m_fL2Data;
    // MB instruction and data reads that access memory
    m_fMB = tsAccessTypes.m_fPercentInst*ptsHitsRates->m_fMBInst + tsAccessTypes.m_fPercentRead*ptsHitsRates->m_fMBData;
    // L1 Write Hits (0.01 is the write stall ratio, needs to be measured)
    m_fL1Write = tsAccessTypes.m_fPercentWrite*ptsHitsRates->m_fL1Write*0.01f;
    m_fL2Write = tsAccessTypes.m_fPercentWrite*ptsHitsRates->m_fL2Write*0.01f;
    m_fMBWrite = tsAccessTypes.m_fPercentWrite*ptsHitsRates->m_fMBWrite*0.01f;
    // SDRam Row crossings
    m_fRASExtra = tsAccessTypes.m_fPercentInst*ptsHitsRates->m_fRASExtraInst + tsAccessTypes.m_fPercentRead*ptsHitsRates->m_fRASExtraData + tsAccessTypes.m_fPercentWrite*ptsHitsRates->m_fRASExtraData;
    // PageWalks instructions, reads and writes
    const tsWalkRate * ptsWalkRate = taWalkRates;
    while ( iTLBSize > ptsWalkRate->iTLBSize )
        ptsWalkRate++;
    m_fPageWalk = tsAccessTypes.m_fPercentInst*ptsWalkRate->m_fInstWalkRate + tsAccessTypes.m_fPercentRead*ptsWalkRate->m_fDataWalkRate + tsAccessTypes.m_fPercentWrite*ptsWalkRate->m_fDataWalkRate;
    // Uncached reads - WAG - need to measure the number of uncached accesses
    m_fUCMB      = 0.001f;
    // Uncached Writes- WAG - need to measure the number of uncached writes, particularly with IE which uses GWE
    m_fUCMBWrite = 0.001f;
    // DDraw Surface Reads - WAG - need to measure the number of DDraw surface reads
    m_fDSMB      = 0.001f;
    // DDraw Surface Writes- WAG need to measure the number of DDraw surface writes since most graphics will do this
    m_fDSMBWrite = 10.00f;
    CSVPrintf( "Scoring Weights %s, %6.2f, %6.2f, %6.2f, %6.2f, %6.2f, %6.2f, %6.2f, %6.2f, %6.2f, %6.2f, %6.2f, %6.2f\r\n",
                strLabel, m_fL1, m_fL2, m_fMB, m_fL1Write, m_fL2Write, m_fMBWrite, m_fRASExtra, m_fPageWalk, m_fUCMB, m_fUCMBWrite, m_fDSMB, m_fDSMBWrite
               );

}
// these weights are determined by observing the number of accesses of a typical scenario
// such as Contacts launch with 250 contacts.
// DELog => MemModel is the basic flow but with some additional measurements are used.
// 


// ***************************************************************************************************************
// Latency constructor
// ***************************************************************************************************************

tComponents::tComponents( const bool bTarget, const char * strLabel  )
{
    const int ciL1Default  =  10;
    const int ciL2Default  =  70;
    const int ciMBDefault  = 350;
    const int ciLDR2RegStall = 3;   // 3 cycles before a register dependency stalls the CPU, (all latencies are for 100 accesses)
    if ( bTarget )
    {
        // target Latencies which should earn a device a score of 100 if it is using a target configuration
        m_fL1          = 0;
        m_fL2          =  10-ciLDR2RegStall;
        m_fMB          = 140-ciLDR2RegStall;
        m_fL1Write     =   2.5f;
        m_fL2Write     =  10;
        m_fMBWrite     =  20;
        m_fRASExtra    =  50;
        m_fPageWalk    =  22;
        m_fUCMB        = 170;
        m_fUCMBWrite   = 110;
        m_fDSMB        = (170+7*0.0f)/8;
        m_fDSMBWrite   = (110+7*2.5f)/8;
    }
    else
    {
        // if some quantities are not measured, assume pretty bad values for them for the purpose of scoring.
        m_fL1         = (gtGlobal.dwL1Latency>0 && gtGlobal.bCachedMemoryTested) ? 
                                            (gtGlobal.dwL1Latency>100*ciLDR2RegStall ? (gtGlobal.dwL1Latency/100.0f - ciLDR2RegStall): 0) : ciL1Default;
        m_fL2         = (gtGlobal.dwL2Latency>0 && gtGlobal.bCachedMemoryTested) ? 
                                            (gtGlobal.dwL2Latency>100*ciLDR2RegStall ? (gtGlobal.dwL2Latency/100.0f - ciLDR2RegStall) : 0) : ciL2Default;
        m_fMB         = (gtGlobal.dwMBLatency>0 && gtGlobal.bCachedMemoryTested) ? 
                                            (gtGlobal.dwMBLatency>100*ciLDR2RegStall ? (gtGlobal.dwMBLatency/100.0f - ciLDR2RegStall) : 0) : ciMBDefault;
        m_fL1Write    = (gtGlobal.dwL1WriteLatency>0 && gtGlobal.bCachedMemoryTested) ? 
                                            gtGlobal.dwL1WriteLatency/100.0f : ciL1Default;
        m_fL2Write    = gtGlobal.dwL2WriteLatency>0   ? 
                                            gtGlobal.dwL2WriteLatency/100.0f  : ciL2Default;
        m_fMBWrite    = gtGlobal.dwMBWriteLatency>0   ? 
                                            gtGlobal.dwMBWriteLatency/100.0f  : ciMBDefault;
        m_fRASExtra   = gtGlobal.dwRASExtraLatency>0  ? 
                                            gtGlobal.dwRASExtraLatency/100.0f  : ciL2Default;
        m_fPageWalk   = gtGlobal.dwPWLatency>0        ? 
                                            gtGlobal.dwPWLatency/100.0f  : 2*m_fMB;       // page walks are two cycles
        m_fUCMB       = gtGlobal.dwUCMBLatency>0      ? 
                                            gtGlobal.dwUCMBLatency/100.0f : ciMBDefault;
        m_fUCMBWrite  = gtGlobal.dwUCMBWriteLatency>0 ? 
                                            gtGlobal.dwUCMBWriteLatency/100.0f : ciMBDefault;
        m_fDSMB       = gtGlobal.dwDSMBLatency>0      ? 
                                            gtGlobal.dwDSMBLatency/100.0f : ciMBDefault; // (ciMBDefault+7*ciL1Default)/8.0f;
        m_fDSMBWrite  = gtGlobal.dwDSMBWriteLatency>0 ? 
                                        gtGlobal.dwDSMBWriteLatency/100.0f : ciMBDefault; // (ciMBDefault+7*ciL1Default)/8.0f;
    }
    CSVPrintf( "Scoring Latencies %s, %6.2f, %6.2f, %6.2f, %6.2f, %6.2f, %6.2f, %6.2f, %6.2f, %6.2f, %6.2f, %6.2f, %6.2f\r\n",
                strLabel, m_fL1, m_fL2, m_fMB, m_fL1Write, m_fL2Write, m_fMBWrite, m_fRASExtra, m_fPageWalk, m_fUCMB, m_fUCMBWrite, m_fDSMB, m_fDSMBWrite
               );
}


// ***************************************************************************************************************
// compute the cost
// ***************************************************************************************************************

float Cost( const tComponents &tWeights, const tComponents &tLatencies, float &fCachedCost, float &fUnCachedCost, float &fDDSurfaceCost, const char * strLabel )
{
    const float cfTotalAccesses = 360992178.0f;
    const float cfWeightScale   = 10000.0f;
    const float cfScale2MS = (cfTotalAccesses/cfWeightScale)/1000000.0f;
    // cost the components into Cached, Uncached and DDraw subtotals and then total
    fCachedCost    = 0.0f;
    fUnCachedCost  = 0.0f;
    fDDSurfaceCost = 0.0f;  

    fCachedCost   += cfScale2MS * tWeights.m_fL1         * tLatencies.m_fL1;       
    fCachedCost   += cfScale2MS * tWeights.m_fL2         * tLatencies.m_fL2;       
    fCachedCost   += cfScale2MS * tWeights.m_fMB         * tLatencies.m_fMB ;      
    fCachedCost   += cfScale2MS * tWeights.m_fL1Write    * tLatencies.m_fL1Write;  
    fCachedCost   += cfScale2MS * tWeights.m_fL2Write    * tLatencies.m_fL2Write;  
    fCachedCost   += cfScale2MS * tWeights.m_fMBWrite    * tLatencies.m_fMBWrite;  
    fCachedCost   += cfScale2MS * tWeights.m_fRASExtra   * tLatencies.m_fRASExtra; 
    fCachedCost   += cfScale2MS * tWeights.m_fPageWalk   * tLatencies.m_fPageWalk;

    fUnCachedCost += cfScale2MS * tWeights.m_fUCMB       * tLatencies.m_fUCMB;     
    fUnCachedCost += cfScale2MS * tWeights.m_fUCMBWrite  * tLatencies.m_fUCMBWrite;

    fDDSurfaceCost+= cfScale2MS * tWeights.m_fDSMB       * tLatencies.m_fDSMB;     
    fDDSurfaceCost+= cfScale2MS * tWeights.m_fDSMBWrite  * tLatencies.m_fDSMBWrite;

    CSVPrintf( "Scoring %s Components, %6.2f, %6.2f, %6.2f, %6.2f, %6.2f, %6.2f, %6.2f, %6.2f, %6.2f, %6.2f, %6.2f, %6.2f\r\n",
                    strLabel,
                    cfScale2MS * tWeights.m_fL1         * tLatencies.m_fL1,       
                    cfScale2MS * tWeights.m_fL2         * tLatencies.m_fL2,       
                    cfScale2MS * tWeights.m_fMB         * tLatencies.m_fMB,      
                    cfScale2MS * tWeights.m_fL1Write    * tLatencies.m_fL1Write,  
                    cfScale2MS * tWeights.m_fL2Write    * tLatencies.m_fL2Write,  
                    cfScale2MS * tWeights.m_fMBWrite    * tLatencies.m_fMBWrite,  
                    cfScale2MS * tWeights.m_fRASExtra   * tLatencies.m_fRASExtra, 
                    cfScale2MS * tWeights.m_fPageWalk   * tLatencies.m_fPageWalk, 
                    cfScale2MS * tWeights.m_fUCMB       * tLatencies.m_fUCMB,     
                    cfScale2MS * tWeights.m_fUCMBWrite  * tLatencies.m_fUCMBWrite,
                    cfScale2MS * tWeights.m_fDSMB       * tLatencies.m_fDSMB,     
                    cfScale2MS * tWeights.m_fDSMBWrite  * tLatencies.m_fDSMBWrite
              );
    return fCachedCost + fUnCachedCost + fDDSurfaceCost;  

}


// ***************************************************************************************************************
// compute the score
// ***************************************************************************************************************

bool ScoreMemoryPerf()
{
    bool fRet = true;       // assume all the tests will pass
    // Scoring System
    CSVPrintf( "Scoring Columns, L1 Read, L2 Read, MB Read, L1 Write, L2 Write, MB Write, RAS Extra, PageWalk, UC Read, UC Write, DS Read, DS Write\r\n" );
    tComponents tsTargetWeights = tComponents( 128, 64, "Target" );
    tComponents tsDeviceWeights = tComponents( gtGlobal.iL2LineSize, gtGlobal.iTLBSize, "Device" );
    tComponents tsTargetLatencies = tComponents( true, "Target" );
    tComponents tsDeviceLatencies = tComponents( false, "Device" );


    // Score the Target System
    float fTargetCachedCost = 0.0f, fTargetUnCachedCost = 0.0f, fTargetDDrawCost = 0.0f;  
    float fTargetCost = Cost( tsTargetWeights, tsTargetLatencies, fTargetCachedCost, fTargetUnCachedCost, fTargetDDrawCost, "Target" );

    // now score the device under test
    float fDeviceCachedCost = 0.0f, fDeviceUnCachedCost = 0.0f, fDeviceDDrawCost = 0.0f;
    float fDeviceCost = Cost( tsDeviceWeights, tsDeviceLatencies, fDeviceCachedCost, fDeviceUnCachedCost, fDeviceDDrawCost, "Device" );

    // Score and Report
    float fScore = fDeviceCost > 0 ? 100.0f*fTargetCost/fDeviceCost : 0.0f;
    float fCachedPercent   = fDeviceCachedCost>0   ? 100.0f*fTargetCost/fDeviceCachedCost : 0.0f;
    float fUnCachedPercent = fDeviceUnCachedCost>0 ? 100.0f*fTargetUnCachedCost/fDeviceUnCachedCost : 0.0f;
    float fDDrawPercent    = fDeviceDDrawCost>0    ? 100.0f*fTargetDDrawCost/fDeviceDDrawCost : 0.0f;

    LogPrintf( "%6.2f%% Overall Memory Score (should be greater than 100%%) [Cost: Device %6.2f vs Target %6.2f][Cached %6.2f%%, Uncached %6.2f%%, DDraw Surface %6.2f%%]\r\n", 
                    fScore, fDeviceCost, fTargetCost, fCachedPercent, fUnCachedPercent, fDDrawPercent
              );
    PerfReport( _T("Overall Memory Score (GT 100)"),  (DWORD)fScore );

    return fRet;
}
