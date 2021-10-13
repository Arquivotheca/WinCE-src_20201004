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
#include "utilities.h"
#include "clparse.h"

extern DWORD g_dwVerbosity;
extern COtak *g_pCOtakResult;
extern BOOL g_fUseXmlLog;
extern TSTRING g_tsXmlDest;

TSTRING dtos(double d)
{
    TSTRING tsTmp;
    TCHAR tcTmp[MAXLINE];
    _sntprintf_s(tcTmp, countof(tcTmp)-1, TEXT("%f"), d);
    tcTmp[MAXLINE-1]=TEXT('\0');
    tsTmp = tcTmp;
    return tsTmp;
}

TSTRING itos(int i)
{
    TSTRING tsTmp;    
    TCHAR tcTmp[MAXLINE];
    _sntprintf_s(tcTmp, countof(tcTmp)-1, TEXT("%d"), i);
    tcTmp[MAXLINE-1]=TEXT('\0');
    tsTmp = tcTmp;
    return tsTmp;
}

TSTRING itohs(int i)
{
    TSTRING tsTmp;    
    TCHAR tcTmp[MAXLINE];
    _sntprintf_s(tcTmp, countof(tcTmp)-1, TEXT("0x%08x"), i);
    tcTmp[MAXLINE-1]=TEXT('\0');
    tsTmp = tcTmp;
    return tsTmp;
}

BOOL
ParseCommandLine(LPCTSTR CommandLine, TSTRING * tsFileName)
{
    CClParse cCLPparser(CommandLine);

    TCHAR tcFileName[MAX_PATH];
    TCHAR tcModuleName[MAX_PATH];
    BOOL bRval = TRUE;
    OtakDestinationStruct ODS;

    if(GetModuleFileName(NULL, tcModuleName, countof(tcModuleName)) < MAX_PATH)
    {
        // start at the first non null character, an emtpy string results in index being less than 0
        // which skips the loop
        int index = _tcslen(tcModuleName) - 1;

        for(; tcModuleName[index] != '\\' && index > 0; index--);

        // if the index is less than 0, then it's an "empty" path.
        // if the index is 0, then it's a single character
        if(index >= 0 && tcModuleName[index] == '\\')
            tcModuleName[index + 1] = NULL;

        // print usage info on command
        if(cCLPparser.GetOpt(TEXT("?")))
        {
            // bump the verbosity level down to lowest setting to make sure this text is printed
            DWORD dwOldVerbosity = g_dwVerbosity;
            g_dwVerbosity = VERBOSITY_ALL;

            info(ECHO, TEXT("*** =================================================================="));
            info(ECHO, TEXT("*** USAGE INFO:"));
            info(ECHO, TEXT("*** "));
            info(ECHO, TEXT("***   /s <scriptfile.txt>  Input script file"));
            info(ECHO, TEXT("***   /v <0|1|2>           Verbosity"));
            info(ECHO, TEXT("***   /f <logresult.csv>   Destination Filename"));
            info(ECHO, TEXT("***   /x                   Result Log in XML Format"));
            info(ECHO, TEXT("***   /d                   Perf Results Sent to Debug Output"));
            info(ECHO, TEXT("***"));
            info(ECHO, TEXT("*** NOTES:"));
            info(ECHO, TEXT("***   /d and /f are mutually exclusive"));
            info(ECHO, TEXT("***   /d and /x are mutually exclusive"));
            info(ECHO, TEXT("*** "));
            info(ECHO, TEXT("*** =================================================================="));

            // restore original verbosity level
            g_dwVerbosity = dwOldVerbosity ;
        }

        // Set up user command line options for the source script file.
        if(cCLPparser.GetOptString(TEXT("S"), tcFileName, MAX_PATH))
            *tsFileName = tcFileName;
        else
        {
            *tsFileName = tcModuleName;
            *tsFileName += DEFAULTSCRIPTFILE;
        }

        // Kato doesn't support verbosity, so lets keep track of this internally
        if(!cCLPparser.GetOptVal(TEXT("v"), &g_dwVerbosity))
            g_dwVerbosity = (DWORD)VERBOSITY_DEFAULT;

        if(cCLPparser.GetOpt(TEXT("x")))
        {
            // enable PerfScenario XML formatted result log file
            g_fUseXmlLog = TRUE;

            // set the destination location
            if (cCLPparser.GetOptString(TEXT("f"), tcFileName, MAX_PATH))
            {
                g_tsXmlDest = tcFileName;
            }
            else
            {
                g_tsXmlDest = tcModuleName;
                g_tsXmlDest += DEFAULTXMLFILE;
            }
        }
        else
        {
            // create a CSV perf result file
            g_fUseXmlLog = FALSE;

            // create our perf result Otak logger (if CSV format only)
            g_pCOtakResult = new(COtak);
            if (NULL == g_pCOtakResult)
            {
                bRval = FALSE;
            }
            else
            {

                ODS.dwSize = sizeof(OtakDestinationStruct);

                // Set up the logger for the results.
                if (cCLPparser.GetOptString(TEXT("f"), tcFileName, MAX_PATH))
                {
                    ODS.dwDestination = OTAKFILEDESTINATION;
                    ODS.tsFileName = tcFileName;
                }
                else if(cCLPparser.GetOpt(TEXT("d")))
                    ODS.dwDestination = OTAKDEBUGDESTINATION;
                else
                {
                    ODS.dwDestination = OTAKFILEDESTINATION;
                    ODS.tsFileName = tcModuleName;
                    ODS.tsFileName += DEFAULTLOGFILE;
                }

                // the Result output logger always has a verbosity of 0, since
                // the only thing it's used for is results.
                g_pCOtakResult->SetVerbosity(0);

                // set the CSV perf log destination
                g_pCOtakResult->SetDestination(ODS);
            }
        }
    }
    else
    {
        OutputDebugString(TEXT("Failed to retrieve module file name."));
        bRval = FALSE;
    }

    return bRval;
}

// This function takes a list, and concatinates a list of data into a CSV string
BOOL
ListToCSVString(std::list<TSTRING> *tsList, TSTRING *ts)
{
    std::list<TSTRING>::iterator itr;

    if(ts == NULL || tsList == NULL)
        return FALSE;

    // clear the resulting string.
    *ts = TEXT("");

    itr = tsList->begin();

    // concatinate until we get to the end of the list
    while(itr != tsList->end())
    {
        (*ts) += *itr;

        itr++;

        // if we haven't hit the end, then tag on a comma for the next entry.
        if(itr != tsList->end())
            *ts += TEXT(",");
    }

    return TRUE;
}

// This is a table of the standard normal curve areas.
// dStdAreas[0][x] is the area, dStdAreas[1][x] is the related z-value.
struct ProbZPair sStdAreas[] = {
                                    PROBZENTRY(0.000241555,-3.49),
                                    PROBZENTRY(0.000250753,-3.48),
                                    PROBZENTRY(0.000260276,-3.47),
                                    PROBZENTRY(0.000270135,-3.46),
                                    PROBZENTRY(0.000280341,-3.45),
                                    PROBZENTRY(0.000290906,-3.44),
                                    PROBZENTRY(0.000301840,-3.43),
                                    PROBZENTRY(0.000313156,-3.42),
                                    PROBZENTRY(0.000324865,-3.41),
                                    PROBZENTRY(0.000336981,-3.40),
                                    PROBZENTRY(0.000349515,-3.39),
                                    PROBZENTRY(0.000362482,-3.38),
                                    PROBZENTRY(0.000375895,-3.37),
                                    PROBZENTRY(0.000389767,-3.36),
                                    PROBZENTRY(0.000404113,-3.35),
                                    PROBZENTRY(0.000418948,-3.34),
                                    PROBZENTRY(0.000434286,-3.33),
                                    PROBZENTRY(0.000450144,-3.32),
                                    PROBZENTRY(0.000466538,-3.31),
                                    PROBZENTRY(0.000483483,-3.30),
                                    PROBZENTRY(0.000500996,-3.29),
                                    PROBZENTRY(0.000519095,-3.28),
                                    PROBZENTRY(0.000537798,-3.27),
                                    PROBZENTRY(0.000557122,-3.26),
                                    PROBZENTRY(0.000577086,-3.25),
                                    PROBZENTRY(0.000597711,-3.24),
                                    PROBZENTRY(0.000619014,-3.23),
                                    PROBZENTRY(0.000641016,-3.22),
                                    PROBZENTRY(0.000663738,-3.21),
                                    PROBZENTRY(0.000687202,-3.20),
                                    PROBZENTRY(0.000736440,-3.18),
                                    PROBZENTRY(0.000967671,-3.10),
                                    PROBZENTRY(0.000762260,-3.17),
                                    PROBZENTRY(0.000788912,-3.16),
                                    PROBZENTRY(0.000816419,-3.15),
                                    PROBZENTRY(0.000844806,-3.14),
                                    PROBZENTRY(0.000874099,-3.13),
                                    PROBZENTRY(0.000904323,-3.12),
                                    PROBZENTRY(0.000935504,-3.11),
                                    PROBZENTRY(0.000967671,-3.10),
                                    PROBZENTRY(0.001000851,-3.09),
                                    PROBZENTRY(0.001035071,-3.08),
                                    PROBZENTRY(0.001070363,-3.07),
                                    PROBZENTRY(0.001106754,-3.06),
                                    PROBZENTRY(0.001144276,-3.05),
                                    PROBZENTRY(0.001182960,-3.04),
                                    PROBZENTRY(0.001222838,-3.03),
                                    PROBZENTRY(0.001263943,-3.02),
                                    PROBZENTRY(0.001306308,-3.01),
                                    PROBZENTRY(0.001349967,-3.00),
                                    PROBZENTRY(0.001394956,-2.99),
                                    PROBZENTRY(0.001441311,-2.98),
                                    PROBZENTRY(0.001489068,-2.97),
                                    PROBZENTRY(0.001538264,-2.96),
                                    PROBZENTRY(0.001588938,-2.95),
                                    PROBZENTRY(0.001641129,-2.94),
                                    PROBZENTRY(0.001694878,-2.93),
                                    PROBZENTRY(0.001750225,-2.92),
                                    PROBZENTRY(0.001807211,-2.91),
                                    PROBZENTRY(0.001865880,-2.90),
                                    PROBZENTRY(0.001926276,-2.89),
                                    PROBZENTRY(0.001988442,-2.88),
                                    PROBZENTRY(0.002052424,-2.87),
                                    PROBZENTRY(0.002118270,-2.86),
                                    PROBZENTRY(0.002186026,-2.85),
                                    PROBZENTRY(0.002255740,-2.84),
                                    PROBZENTRY(0.002327463,-2.83),
                                    PROBZENTRY(0.002401244,-2.82),
                                    PROBZENTRY(0.002477136,-2.81),
                                    PROBZENTRY(0.002555191,-2.80),
                                    PROBZENTRY(0.002635461,-2.79),
                                    PROBZENTRY(0.002718003,-2.78),
                                    PROBZENTRY(0.002802872,-2.77),
                                    PROBZENTRY(0.002890125,-2.76),
                                    PROBZENTRY(0.002979819,-2.75),
                                    PROBZENTRY(0.003072013,-2.74),
                                    PROBZENTRY(0.003166769,-2.73),
                                    PROBZENTRY(0.003264148,-2.72),
                                    PROBZENTRY(0.003364211,-2.71),
                                    PROBZENTRY(0.003467023,-2.70),
                                    PROBZENTRY(0.003572649,-2.69),
                                    PROBZENTRY(0.003681155,-2.68),
                                    PROBZENTRY(0.003792607,-2.67),
                                    PROBZENTRY(0.003907076,-2.66),
                                    PROBZENTRY(0.004024631,-2.65),
                                    PROBZENTRY(0.004145342,-2.64),
                                    PROBZENTRY(0.004269282,-2.63),
                                    PROBZENTRY(0.004396526,-2.62),
                                    PROBZENTRY(0.004527147,-2.61),
                                    PROBZENTRY(0.004661222,-2.60),
                                    PROBZENTRY(0.004798829,-2.59),
                                    PROBZENTRY(0.004940046,-2.58),
                                    PROBZENTRY(0.005084954,-2.57),
                                    PROBZENTRY(0.005233635,-2.56),
                                    PROBZENTRY(0.005386170,-2.55),
                                    PROBZENTRY(0.005542646,-2.54),
                                    PROBZENTRY(0.005703147,-2.53),
                                    PROBZENTRY(0.005867760,-2.52),
                                    PROBZENTRY(0.006036575,-2.51),
                                    PROBZENTRY(0.006209680,-2.50),
                                    PROBZENTRY(0.006387167,-2.49),
                                    PROBZENTRY(0.006569129,-2.48),
                                    PROBZENTRY(0.006755661,-2.47),
                                    PROBZENTRY(0.006946857,-2.46),
                                    PROBZENTRY(0.007142815,-2.45),
                                    PROBZENTRY(0.007343633,-2.44),
                                    PROBZENTRY(0.007549411,-2.43),
                                    PROBZENTRY(0.007760251,-2.42),
                                    PROBZENTRY(0.007976255,-2.41),
                                    PROBZENTRY(0.008197529,-2.40),
                                    PROBZENTRY(0.008424177,-2.39),
                                    PROBZENTRY(0.008656308,-2.38),
                                    PROBZENTRY(0.008894029,-2.37),
                                    PROBZENTRY(0.009137452,-2.36),
                                    PROBZENTRY(0.009386687,-2.35),
                                    PROBZENTRY(0.009641850,-2.34),
                                    PROBZENTRY(0.009903053,-2.33),
                                    PROBZENTRY(0.010170414,-2.32),
                                    PROBZENTRY(0.010444050,-2.31),
                                    PROBZENTRY(0.010724081,-2.30),
                                    PROBZENTRY(0.011010627,-2.29),
                                    PROBZENTRY(0.011303811,-2.28),
                                    PROBZENTRY(0.011603756,-2.27),
                                    PROBZENTRY(0.011910588,-2.26),
                                    PROBZENTRY(0.012224433,-2.25),
                                    PROBZENTRY(0.012545420,-2.24),
                                    PROBZENTRY(0.012873678,-2.23),
                                    PROBZENTRY(0.013209339,-2.22),
                                    PROBZENTRY(0.013552534,-2.21),
                                    PROBZENTRY(0.013903399,-2.20),
                                    PROBZENTRY(0.014262068,-2.19),
                                    PROBZENTRY(0.014628679,-2.18),
                                    PROBZENTRY(0.015003369,-2.17),
                                    PROBZENTRY(0.015386280,-2.16),
                                    PROBZENTRY(0.015777551,-2.15),
                                    PROBZENTRY(0.016177325,-2.14),
                                    PROBZENTRY(0.016585747,-2.13),
                                    PROBZENTRY(0.017002962,-2.12),
                                    PROBZENTRY(0.017429116,-2.11),
                                    PROBZENTRY(0.017864357,-2.10),
                                    PROBZENTRY(0.018308836,-2.09),
                                    PROBZENTRY(0.018762701,-2.08),
                                    PROBZENTRY(0.019226106,-2.07),
                                    PROBZENTRY(0.019699203,-2.06),
                                    PROBZENTRY(0.020182148,-2.05),
                                    PROBZENTRY(0.020675095,-2.04),
                                    PROBZENTRY(0.021178201,-2.03),
                                    PROBZENTRY(0.021691624,-2.02),
                                    PROBZENTRY(0.022215525,-2.01),
                                    PROBZENTRY(0.022750062,-2.00),
                                    PROBZENTRY(0.023295398,-1.99),
                                    PROBZENTRY(0.023851694,-1.98),
                                    PROBZENTRY(0.024419115,-1.97),
                                    PROBZENTRY(0.024997825,-1.96),
                                    PROBZENTRY(0.025587990,-1.95),
                                    PROBZENTRY(0.026189776,-1.94),
                                    PROBZENTRY(0.026803350,-1.93),
                                    PROBZENTRY(0.027428881,-1.92),
                                    PROBZENTRY(0.028066539,-1.91),
                                    PROBZENTRY(0.028716493,-1.90),
                                    PROBZENTRY(0.029378914,-1.89),
                                    PROBZENTRY(0.030053974,-1.88),
                                    PROBZENTRY(0.030741845,-1.87),
                                    PROBZENTRY(0.031442700,-1.86),
                                    PROBZENTRY(0.032156713,-1.85),
                                    PROBZENTRY(0.032884058,-1.84),
                                    PROBZENTRY(0.033624911,-1.83),
                                    PROBZENTRY(0.034379445,-1.82),
                                    PROBZENTRY(0.035147838,-1.81),
                                    PROBZENTRY(0.035930266,-1.80),
                                    PROBZENTRY(0.036726904,-1.79),
                                    PROBZENTRY(0.037537931,-1.78),
                                    PROBZENTRY(0.038363523,-1.77),
                                    PROBZENTRY(0.039203858,-1.76),
                                    PROBZENTRY(0.040059114,-1.75),
                                    PROBZENTRY(0.040929468,-1.74),
                                    PROBZENTRY(0.041815099,-1.73),
                                    PROBZENTRY(0.042716185,-1.72),
                                    PROBZENTRY(0.043632903,-1.71),
                                    PROBZENTRY(0.044565432,-1.70),
                                    PROBZENTRY(0.045513949,-1.69),
                                    PROBZENTRY(0.046478632,-1.68),
                                    PROBZENTRY(0.047459659,-1.67),
                                    PROBZENTRY(0.048457206,-1.66),
                                    PROBZENTRY(0.049471451,-1.65),
                                    PROBZENTRY(0.050502569,-1.64),
                                    PROBZENTRY(0.051550737,-1.63),
                                    PROBZENTRY(0.052616130,-1.62),
                                    PROBZENTRY(0.053698923,-1.61),
                                    PROBZENTRY(0.054799289,-1.60),
                                    PROBZENTRY(0.055917403,-1.59),
                                    PROBZENTRY(0.057053437,-1.58),
                                    PROBZENTRY(0.058207562,-1.57),
                                    PROBZENTRY(0.059379950,-1.56),
                                    PROBZENTRY(0.060570771,-1.55),
                                    PROBZENTRY(0.061780193,-1.54),
                                    PROBZENTRY(0.063008383,-1.53),
                                    PROBZENTRY(0.064255510,-1.52),
                                    PROBZENTRY(0.065521737,-1.51),
                                    PROBZENTRY(0.066807229,-1.50),
                                    PROBZENTRY(0.068112148,-1.49),
                                    PROBZENTRY(0.069436656,-1.48),
                                    PROBZENTRY(0.070780913,-1.47),
                                    PROBZENTRY(0.072145075,-1.46),
                                    PROBZENTRY(0.073529300,-1.45),
                                    PROBZENTRY(0.074933743,-1.44),
                                    PROBZENTRY(0.076358555,-1.43),
                                    PROBZENTRY(0.077803888,-1.42),
                                    PROBZENTRY(0.079269891,-1.41),
                                    PROBZENTRY(0.080756711,-1.40),
                                    PROBZENTRY(0.082264493,-1.39),
                                    PROBZENTRY(0.083793378,-1.38),
                                    PROBZENTRY(0.085343508,-1.37),
                                    PROBZENTRY(0.086915021,-1.36),
                                    PROBZENTRY(0.088508052,-1.35),
                                    PROBZENTRY(0.090122734,-1.34),
                                    PROBZENTRY(0.091759198,-1.33),
                                    PROBZENTRY(0.093417573,-1.32),
                                    PROBZENTRY(0.095097982,-1.31),
                                    PROBZENTRY(0.096800549,-1.30),
                                    PROBZENTRY(0.098525394,-1.29),
                                    PROBZENTRY(0.100272634,-1.28),
                                    PROBZENTRY(0.102042381,-1.27),
                                    PROBZENTRY(0.103834747,-1.26),
                                    PROBZENTRY(0.105649839,-1.25),
                                    PROBZENTRY(0.107487762,-1.24),
                                    PROBZENTRY(0.109348617,-1.23),
                                    PROBZENTRY(0.111232501,-1.22),
                                    PROBZENTRY(0.113139509,-1.21),
                                    PROBZENTRY(0.115069732,-1.20),
                                    PROBZENTRY(0.117023256,-1.19),
                                    PROBZENTRY(0.119000166,-1.18),
                                    PROBZENTRY(0.121000541,-1.17),
                                    PROBZENTRY(0.123024458,-1.16),
                                    PROBZENTRY(0.125071989,-1.15),
                                    PROBZENTRY(0.127143201,-1.14),
                                    PROBZENTRY(0.129238161,-1.13),
                                    PROBZENTRY(0.131356927,-1.12),
                                    PROBZENTRY(0.133499557,-1.11),
                                    PROBZENTRY(0.135666102,-1.10),
                                    PROBZENTRY(0.137856610,-1.09),
                                    PROBZENTRY(0.140071125,-1.08),
                                    PROBZENTRY(0.142309686,-1.07),
                                    PROBZENTRY(0.144572328,-1.06),
                                    PROBZENTRY(0.146859081,-1.05),
                                    PROBZENTRY(0.149169971,-1.04),
                                    PROBZENTRY(0.151505020,-1.03),
                                    PROBZENTRY(0.153864244,-1.02),
                                    PROBZENTRY(0.156247655,-1.01),
                                    PROBZENTRY(0.158655260,-1.00),
                                    PROBZENTRY(0.161087061,-0.99),
                                    PROBZENTRY(0.163543057,-0.98),
                                    PROBZENTRY(0.166023240,-0.97),
                                    PROBZENTRY(0.168527597,-0.96),
                                    PROBZENTRY(0.171056112,-0.95),
                                    PROBZENTRY(0.173608762,-0.94),
                                    PROBZENTRY(0.176185520,-0.93),
                                    PROBZENTRY(0.178786354,-0.92),
                                    PROBZENTRY(0.181411225,-0.91),
                                    PROBZENTRY(0.184060092,-0.90),
                                    PROBZENTRY(0.186732906,-0.89),
                                    PROBZENTRY(0.189429614,-0.88),
                                    PROBZENTRY(0.192150158,-0.87),
                                    PROBZENTRY(0.194894473,-0.86),
                                    PROBZENTRY(0.197662492,-0.85),
                                    PROBZENTRY(0.200454139,-0.84),
                                    PROBZENTRY(0.203269335,-0.83),
                                    PROBZENTRY(0.206107994,-0.82),
                                    PROBZENTRY(0.208970026,-0.81),
                                    PROBZENTRY(0.211855334,-0.80),
                                    PROBZENTRY(0.214763817,-0.79),
                                    PROBZENTRY(0.217695369,-0.78),
                                    PROBZENTRY(0.220649876,-0.77),
                                    PROBZENTRY(0.223627221,-0.76),
                                    PROBZENTRY(0.22662728,-0.750),
                                    PROBZENTRY(0.229649924,-0.74),
                                    PROBZENTRY(0.232695018,-0.73),
                                    PROBZENTRY(0.235762424,-0.72),
                                    PROBZENTRY(0.238851994,-0.71),
                                    PROBZENTRY(0.241963578,-0.70),
                                    PROBZENTRY(0.245097021,-0.69),
                                    PROBZENTRY(0.248252158,-0.68),
                                    PROBZENTRY(0.251428824,-0.67),
                                    PROBZENTRY(0.254626846,-0.66),
                                    PROBZENTRY(0.257846044,-0.65),
                                    PROBZENTRY(0.261086235,-0.64),
                                    PROBZENTRY(0.264347230,-0.63),
                                    PROBZENTRY(0.267628834,-0.62),
                                    PROBZENTRY(0.270930848,-0.61),
                                    PROBZENTRY(0.274253065,-0.60),
                                    PROBZENTRY(0.277595276,-0.59),
                                    PROBZENTRY(0.280957264,-0.58),
                                    PROBZENTRY(0.284338808,-0.57),
                                    PROBZENTRY(0.287739682,-0.56),
                                    PROBZENTRY(0.291159655,-0.55),
                                    PROBZENTRY(0.294598489,-0.54),
                                    PROBZENTRY(0.298055944,-0.53),
                                    PROBZENTRY(0.301531771,-0.52),
                                    PROBZENTRY(0.305025719,-0.51),
                                    PROBZENTRY(0.308537533,-0.50),
                                    PROBZENTRY(0.312066949,-0.49),
                                    PROBZENTRY(0.315613701,-0.48),
                                    PROBZENTRY(0.319177519,-0.47),
                                    PROBZENTRY(0.322758126,-0.46),
                                    PROBZENTRY(0.326355241,-0.45),
                                    PROBZENTRY(0.329968580,-0.44),
                                    PROBZENTRY(0.333597852,-0.43),
                                    PROBZENTRY(0.337242763,-0.42),
                                    PROBZENTRY(0.340903014,-0.41),
                                    PROBZENTRY(0.344578303,-0.40),
                                    PROBZENTRY(0.348268323,-0.39),
                                    PROBZENTRY(0.351972760,-0.38),
                                    PROBZENTRY(0.355691301,-0.37),
                                    PROBZENTRY(0.359423626,-0.36),
                                    PROBZENTRY(0.363169410,-0.35),
                                    PROBZENTRY(0.366928327,-0.34),
                                    PROBZENTRY(0.370700045,-0.33),
                                    PROBZENTRY(0.374484230,-0.32),
                                    PROBZENTRY(0.378280543,-0.31),
                                    PROBZENTRY(0.382088643,-0.30),
                                    PROBZENTRY(0.385908182,-0.29),
                                    PROBZENTRY(0.389738814,-0.28),
                                    PROBZENTRY(0.393580186,-0.27),
                                    PROBZENTRY(0.397431943,-0.26),
                                    PROBZENTRY(0.401293726,-0.25),
                                    PROBZENTRY(0.405165176,-0.24),
                                    PROBZENTRY(0.409045927,-0.23),
                                    PROBZENTRY(0.412935613,-0.22),
                                    PROBZENTRY(0.416833866,-0.21),
                                    PROBZENTRY(0.420740313,-0.20),
                                    PROBZENTRY(0.424654580,-0.19),
                                    PROBZENTRY(0.428576291,-0.18),
                                    PROBZENTRY(0.432505067,-0.17),
                                    PROBZENTRY(0.436440527,-0.16),
                                    PROBZENTRY(0.440382288,-0.15),
                                    PROBZENTRY(0.444329967,-0.14),
                                    PROBZENTRY(0.448283177,-0.13),
                                    PROBZENTRY(0.452241530,-0.12),
                                    PROBZENTRY(0.456204636,-0.11),
                                    PROBZENTRY(0.460172104,-0.10),
                                    PROBZENTRY(0.464143544,-0.09),
                                    PROBZENTRY(0.468118560,-0.08),
                                    PROBZENTRY(0.472096760,-0.07),
                                    PROBZENTRY(0.476077747,-0.06),
                                    PROBZENTRY(0.480061127,-0.05),
                                    PROBZENTRY(0.484046501,-0.04),
                                    PROBZENTRY(0.488033473,-0.03),
                                    PROBZENTRY(0.492021646,-0.02),
                                    PROBZENTRY(0.496010621,-0.01),
                                    PROBZENTRY(0.500000000,00.00)};

double
NormDistInv(double p)
{
    int entry;
    double dMatchDiff1, dMatchDiff2;

    // if the selected probability is less than the smallest in our area curve, then
    // return the smallest.
    if(p <= sStdAreas[0].dProbability)
        entry = 0;
    else
    {
        for(entry = 0; entry < (countof(sStdAreas) - 1); entry++)
        {

            // if we're between two entryes, one less one greater, then it's one of the two.
            // if either entry is equal, then it's a match so we just want to return it.
            if(p >= sStdAreas[entry].dProbability && p <= sStdAreas[entry + 1].dProbability)
                break;
        }
    }
    // if we hit then end of the loop, or we exit regularly we want to return the closest match to p.
    dMatchDiff1 = fabs(sStdAreas[entry].dProbability - p);
    dMatchDiff2 = fabs(sStdAreas[entry + 1].dProbability - p);

    // if match 1 is closest or they're both equal, return match 1 to error on more runs.
    // otherwise match 2 is closest.  If the entry isn't found, then we return 0 (which is also the match to .5).
    if(dMatchDiff1 <= dMatchDiff2)
        return sStdAreas[entry].dZValue;
    else return sStdAreas[entry + 1].dZValue;
}
