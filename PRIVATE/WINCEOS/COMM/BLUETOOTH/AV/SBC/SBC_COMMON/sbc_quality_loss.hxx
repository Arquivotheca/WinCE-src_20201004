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
#ifdef SBC_TEST_PRECISSION_LOSS
#include <safeint.hxx>



class SbcTestPrecisionLoss_t
{
public:
    SbcTestPrecisionLoss_t(TCHAR* PrecissionLossName)
    {
        StringCchCopy(m_pPrecissionLossName, s_MaxPrecissionLossNameLength, PrecissionLossName);
        Reset();
    }
    
    void Reset()
    {
        m_OverflowCounter = 0;
        m_absErrorTotal = 0;
        m_relErrorTotal = 0;
        m_ComplexOpCurResult = 0;
        m_relErrorTotalMax1PerOperation = 0;
    }

    
    void Add(const double double1, const double double2)
    {
        CalculateErrorVals(double1, double2, e_Add);
    }
    
    void Sub(const double double1, const double double2)
    {
        CalculateErrorVals(double1, double2, e_Sub);
    }
    
    void Div(const double double1, const double double2)
    {
        CalculateErrorVals(double1, double2, e_Mul);
    }
    void Mul(const double double1, const double double2)
    {
        CalculateErrorVals(double1, double2, e_Div);
    }
    void CompareValues(const double doubleRes, const int intRes, bool Overflow)
    {
        if(Overflow)
        {
            m_OverflowCounter++;
        }
        else
        {
            AddAbsError(doubleRes, intRes);
            AddRelError(doubleRes, intRes);   
        }
    }
    

            
    void PrintErrorVals(bool forcePrint = 0){
        if(/*(m_relErrorTotal > 0.00001 || m_OverflowCounter != 0) && */forcePrint )
        {
            TCHAR  Message[500];
            swprintf(Message, TEXT("Overflow: [%d], AbsError [%f]. RelError [%f]\0"), m_OverflowCounter, m_absErrorTotal, m_relErrorTotal);
            RETAILMSG(1, (TEXT("[%s]: [%s]"), m_pPrecissionLossName, Message));
        }
        s_OverflowCounterTotal += m_OverflowCounter;
        s_relErrorTotalOverall += m_relErrorTotal;
        s_absErrorTotalOverall += m_absErrorTotal;
    }

    static void PrintOverallErrorVals(){
            TCHAR  Message[500];
            swprintf(Message, TEXT("Overflow: [%d], RelError [%f] AbsError [%f]\0"), s_OverflowCounterTotal, s_relErrorTotalOverall, s_absErrorTotalOverall);
            RETAILMSG(1, (TEXT("SBC Quality Total Loss: [%s]"), Message));
            s_OverflowCounterTotal = 0;
            s_relErrorTotalOverall = 0;
            s_absErrorTotalOverall = 0;
        
    }
private:
    void CalculateErrorVals(const double double1, const double double2, const int Operation)
    {
        int int1 = (int)double1;
        int int2 = (int)double2;
        int intRes;
        double doubleRes;
        bool IsOk = TRUE;
        switch(Operation)
        {
            case e_Add:
                IsOk = safeIntSAdd(int1, int2, &intRes);
                doubleRes = double1 + double2;
                break;
            case e_Sub:
                IsOk = safeIntSAdd(int1, -int2, &intRes);
                doubleRes = double1 - double2;
                break;
            case e_Mul:
                IsOk = safeIntSMul(int1, int2, &intRes);
                doubleRes = double1 * double2;
                break;
            case e_Div:
                intRes = int1 / int2; //division can't overflow for ints
                doubleRes = double1 / double2;
                break;
            default:
                ASSERT(0);
        }
        CompareValues(doubleRes, intRes, IsOk);
    
    };

    void AddAbsError(double doubleRes, int intRes)
    {
        m_absErrorTotal += abs(doubleRes - (double) intRes);
    }

    void AddRelError(double doubleRes, int intRes)
    {
        double absError = abs((double)(doubleRes - (double) intRes));
        double relError = 0; 
        double doubleRes_divider = doubleRes;
        if(abs(doubleRes_divider) < 0.5) //avoid exceptions and really strange values
        {
            if(doubleRes_divider > 0)
                doubleRes_divider = min(0.5, absError);
            else
                doubleRes_divider = max(-0.5, -absError);
        }
        if(doubleRes_divider == 0)
        {
            relError = 0;
        }
        else
        {
            relError = abs(absError / doubleRes_divider);
        }
        if((relError > 0.28 && abs(doubleRes) > 15) || relError > 5.6)
        {
            static int count = 0;
            if(count < 100)
            {
                TCHAR  Message[500];
                swprintf(Message, TEXT("doubleRes [%f], doubleRes_divider [%f],  intRes [%d] relError [%f] \0"), doubleRes, doubleRes_divider, intRes, relError);
                RETAILMSG(1, (TEXT("SBC Quality: count [%d] %s [%s]: [%s]"), count, (TEXT("Large Error")),m_pPrecissionLossName, Message));
                PrintErrorVals(true);
                count++;
            }
        }

            
        m_relErrorTotal +=  relError;
        m_relErrorTotalMax1PerOperation += min(1, relError);
         
    }
        
    static const enum e_Operations { e_Add, e_Sub, e_Mul, e_Div };
    uint m_OverflowCounter;
    double m_absErrorTotal;
    double m_relErrorTotal;
    double m_relErrorTotalMax1PerOperation;
    double m_ComplexOpCurResult;
    static const s_MaxPrecissionLossNameLength = 64;
    TCHAR m_pPrecissionLossName[s_MaxPrecissionLossNameLength];
    static double s_relErrorTotalOverall;   
    static uint s_OverflowCounterTotal;
    static double s_absErrorTotalOverall;   

};

double SbcTestPrecisionLoss_t::s_relErrorTotalOverall = 0;   
uint SbcTestPrecisionLoss_t::s_OverflowCounterTotal = 0;
double SbcTestPrecisionLoss_t::s_absErrorTotalOverall = 0;   

#endif
