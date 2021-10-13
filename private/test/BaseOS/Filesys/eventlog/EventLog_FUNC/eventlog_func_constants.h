#ifndef __EVENTLOG_FUNC_CONSTANTS_H__
#define __EVENTLOG_FUNC_CONSTANTS_H__

#define WINEVENT_LEVEL_LOGALWAYS        0x0
#define WINEVENT_LEVEL_CRITICAL         0x1
#define WINEVENT_LEVEL_ERROR            0x2
#define WINEVENT_LEVEL_WARNING          0x3
#define WINEVENT_LEVEL_INFO             0x4
#define WINEVENT_LEVEL_VERBOSE          0x5

const DWORD gc_arrLevelMasks[6] = {
                                                    0x2000, 
                                           0x4000 | 0x2000, 
                                  0x8000 | 0x4000 | 0x2000,
                        0x10000 | 0x8000 | 0x4000 | 0x2000,
              0x20000 | 0x10000 | 0x8000 | 0x4000 | 0x2000,
    0x40000 | 0x20000 | 0x10000 | 0x8000 | 0x4000 | 0x2000
};
#define LEVEL_INFO_MASK                   
#define LEVEL_WARNING_MASK                          
#define LEVEL_ERROR_MASK                                      
#define LEVEL_CRITICAL_MASK                                            
#define LEVEL_LOGALWAYS_MASK                                                    


#define DEFAULT_TEST_USER_DATA      0xDEADBEEF

#define MAX_MESSAGE 8192 

#define PERMISSIVE_ANY_KEYWORD          0xFFFFFFFFFFFFFFFF
#define PERMISSIVE_ALL_KEYWORD          0

#define TEST_PROVIDER_COUNT         16
#define TEST_CHANNEL_COUNT          8
#define TEST_EVENT_COUNT            25
#define VALID_TEST_EVENT_COUNT      22
#define TEST_PROC_COUNT             20 

#define DEFAULT_CHANNEL_BUFFERS     4000
#define DEFAULT_CHANNEL_SIZE        8092
#define SMALL_CHANNEL_SIZE              4096

//#define TEST_CHANNEL_1_BUFFERS      32



#define UNUSED_PARAM                0xBA5EBA11

//make this architecture aware!!
#define EVENT_OVERHEAD              16
#define DEFAULT_TIMEOUT             60000
#define TIMEOUT_INCREASE_PER_PROC   60000

//INTERNAL TEST CHANNELS

//Event Descriptor Stuff
#define TEST_EVENT_INT_CMD          0
#define TEST_EVENT_INT_RESULT       1
#define TEST_EVENT_INT_READY        2
#define TEST_EVENT_INT_DEINIT       3

const EVENT_DESCRIPTOR g_IntDescriptors[TEST_PROC_COUNT][4] = {
    {FUNC_TEST_INT_P0_CMD,
    FUNC_TEST_INT_P0_RESULT,
    FUNC_TEST_INT_P0_READY, FUNC_TEST_INT_P0_DEINIT},
    {FUNC_TEST_INT_P1_CMD,
    FUNC_TEST_INT_P1_RESULT,
    FUNC_TEST_INT_P1_READY, FUNC_TEST_INT_P1_DEINIT},
    {FUNC_TEST_INT_P2_CMD,
    FUNC_TEST_INT_P2_RESULT,
    FUNC_TEST_INT_P2_READY, FUNC_TEST_INT_P2_DEINIT},
    {FUNC_TEST_INT_P3_CMD,
    FUNC_TEST_INT_P3_RESULT,
    FUNC_TEST_INT_P3_READY, FUNC_TEST_INT_P3_DEINIT},
    {FUNC_TEST_INT_P4_CMD,
    FUNC_TEST_INT_P4_RESULT,
    FUNC_TEST_INT_P4_READY, FUNC_TEST_INT_P4_DEINIT},
    {FUNC_TEST_INT_P5_CMD,
    FUNC_TEST_INT_P5_RESULT,
    FUNC_TEST_INT_P5_READY, FUNC_TEST_INT_P5_DEINIT},
    {FUNC_TEST_INT_P6_CMD,
    FUNC_TEST_INT_P6_RESULT,
    FUNC_TEST_INT_P6_READY, FUNC_TEST_INT_P6_DEINIT},
    {FUNC_TEST_INT_P7_CMD,
    FUNC_TEST_INT_P7_RESULT,
    FUNC_TEST_INT_P7_READY, FUNC_TEST_INT_P7_DEINIT},
    {FUNC_TEST_INT_P8_CMD,
    FUNC_TEST_INT_P8_RESULT,
    FUNC_TEST_INT_P8_READY, FUNC_TEST_INT_P8_DEINIT},
    {FUNC_TEST_INT_P9_CMD,
    FUNC_TEST_INT_P9_RESULT,
    FUNC_TEST_INT_P9_READY, FUNC_TEST_INT_P9_DEINIT},
    {FUNC_TEST_INT_P10_CMD,
    FUNC_TEST_INT_P10_RESULT,
    FUNC_TEST_INT_P10_READY, FUNC_TEST_INT_P10_DEINIT},
    {FUNC_TEST_INT_P11_CMD,
    FUNC_TEST_INT_P11_RESULT,
    FUNC_TEST_INT_P11_READY, FUNC_TEST_INT_P11_DEINIT},
    {FUNC_TEST_INT_P12_CMD,
    FUNC_TEST_INT_P12_RESULT,
    FUNC_TEST_INT_P12_READY, FUNC_TEST_INT_P12_DEINIT},
    {FUNC_TEST_INT_P13_CMD,
    FUNC_TEST_INT_P13_RESULT,
    FUNC_TEST_INT_P13_READY, FUNC_TEST_INT_P13_DEINIT},
    {FUNC_TEST_INT_P14_CMD,
    FUNC_TEST_INT_P14_RESULT,
    FUNC_TEST_INT_P14_READY, FUNC_TEST_INT_P14_DEINIT},
    {FUNC_TEST_INT_P15_CMD,
    FUNC_TEST_INT_P15_RESULT,
    FUNC_TEST_INT_P15_READY, FUNC_TEST_INT_P15_DEINIT},
    {FUNC_TEST_INT_P16_CMD,
    FUNC_TEST_INT_P16_RESULT,
    FUNC_TEST_INT_P16_READY, FUNC_TEST_INT_P16_DEINIT},
    {FUNC_TEST_INT_P17_CMD,
    FUNC_TEST_INT_P17_RESULT,
    FUNC_TEST_INT_P17_READY, FUNC_TEST_INT_P17_DEINIT},
    {FUNC_TEST_INT_P18_CMD,
    FUNC_TEST_INT_P18_RESULT,
    FUNC_TEST_INT_P18_READY, FUNC_TEST_INT_P18_DEINIT},
    {FUNC_TEST_INT_P19_CMD,
    FUNC_TEST_INT_P19_RESULT,
    FUNC_TEST_INT_P19_READY, FUNC_TEST_INT_P19_DEINIT}

};
const GUID g_arrIntGUIDs[TEST_PROC_COUNT] = {
    WINDOWSCE_EVENTLOG_FUNC_TEST_PROC0_INT,
    WINDOWSCE_EVENTLOG_FUNC_TEST_PROC1_INT,
    WINDOWSCE_EVENTLOG_FUNC_TEST_PROC2_INT,
    WINDOWSCE_EVENTLOG_FUNC_TEST_PROC3_INT,
    WINDOWSCE_EVENTLOG_FUNC_TEST_PROC4_INT,
    WINDOWSCE_EVENTLOG_FUNC_TEST_PROC5_INT,
    WINDOWSCE_EVENTLOG_FUNC_TEST_PROC6_INT,
    WINDOWSCE_EVENTLOG_FUNC_TEST_PROC7_INT,
    WINDOWSCE_EVENTLOG_FUNC_TEST_PROC8_INT,
    WINDOWSCE_EVENTLOG_FUNC_TEST_PROC9_INT,
    WINDOWSCE_EVENTLOG_FUNC_TEST_PROC10_INT,
    WINDOWSCE_EVENTLOG_FUNC_TEST_PROC11_INT,
    WINDOWSCE_EVENTLOG_FUNC_TEST_PROC12_INT,
    WINDOWSCE_EVENTLOG_FUNC_TEST_PROC13_INT,
    WINDOWSCE_EVENTLOG_FUNC_TEST_PROC14_INT,
    WINDOWSCE_EVENTLOG_FUNC_TEST_PROC15_INT,
    WINDOWSCE_EVENTLOG_FUNC_TEST_PROC16_INT,
    WINDOWSCE_EVENTLOG_FUNC_TEST_PROC17_INT,
    WINDOWSCE_EVENTLOG_FUNC_TEST_PROC18_INT,
    WINDOWSCE_EVENTLOG_FUNC_TEST_PROC19_INT
};
WCHAR g_arrIntPaths[TEST_PROC_COUNT][MAX_PATH] = {
    L"Microsoft-WindowsCE-EventLogFUNCTest-Proc0-INT-CH\0",
    L"Microsoft-WindowsCE-EventLogFUNCTest-Proc1-INT-CH\0",
    L"Microsoft-WindowsCE-EventLogFUNCTest-Proc2-INT-CH\0",
    L"Microsoft-WindowsCE-EventLogFUNCTest-Proc3-INT-CH\0",
    L"Microsoft-WindowsCE-EventLogFUNCTest-Proc4-INT-CH\0",
    L"Microsoft-WindowsCE-EventLogFUNCTest-Proc5-INT-CH\0",
    L"Microsoft-WindowsCE-EventLogFUNCTest-Proc6-INT-CH\0",
    L"Microsoft-WindowsCE-EventLogFUNCTest-Proc7-INT-CH\0",
    L"Microsoft-WindowsCE-EventLogFUNCTest-Proc8-INT-CH\0",
    L"Microsoft-WindowsCE-EventLogFUNCTest-Proc9-INT-CH\0",
    L"Microsoft-WindowsCE-EventLogFUNCTest-Proc10-INT-CH\0",
    L"Microsoft-WindowsCE-EventLogFUNCTest-Proc11-INT-CH\0",
    L"Microsoft-WindowsCE-EventLogFUNCTest-Proc12-INT-CH\0",
    L"Microsoft-WindowsCE-EventLogFUNCTest-Proc13-INT-CH\0",
    L"Microsoft-WindowsCE-EventLogFUNCTest-Proc14-INT-CH\0",
    L"Microsoft-WindowsCE-EventLogFUNCTest-Proc15-INT-CH\0",
    L"Microsoft-WindowsCE-EventLogFUNCTest-Proc16-INT-CH\0",
    L"Microsoft-WindowsCE-EventLogFUNCTest-Proc17-INT-CH\0",
    L"Microsoft-WindowsCE-EventLogFUNCTest-Proc18-INT-CH\0",
    L"Microsoft-WindowsCE-EventLogFUNCTest-Proc19-INT-CH\0"
    };

const DWORD g_arrIntCIDs[TEST_PROC_COUNT] = {
    chProc0INT,
    chProc1INT,
    chProc2INT,
    chProc3INT,
    chProc4INT,
    chProc5INT,
    chProc6INT,
    chProc7INT,
    chProc8INT,
    chProc9INT,
    chProc10INT,
    chProc11INT,
    chProc12INT,
    chProc13INT,
    chProc14INT,
    chProc15INT,
    chProc16INT,
    chProc17INT,
    chProc18INT,
    chProc19INT
};
//TEST CHANNELS

/*
<keywords>
            <keyword name="one" mask="0x100"/>
            <keyword name="two" mask="0x200"/>
            <keyword name="three" mask="0x400"/>
            <keyword name="four" mask="0x800"/>
            <keyword name="five" mask="0x1000"/>
            <keyword name="logalways" mask="0x2000"/>
            <keyword name="critical" mask="0x4000"/>
            <keyword name="error" mask="0x8000"/>
            <keyword name="warning" mask="0x10000"/>
            <keyword name="informational" mask="0x20000"/>
            <keyword name="verbose" mask="0x40000"/>
        </keywords>
*/

const DWORD g_arrTestEventKeywords[TEST_EVENT_COUNT] = {
    0x2000,                                         //0
    0x4000                                  | 0x100,
    0x8000                                  | 0x200,
    0x10000                         | 0x200 | 0x100,
    0x20000                 | 0x400                ,
    0x40000                 | 0x400         | 0x100,//5
    0x2000                 | 0x400 | 0x200 | 0x100,
    0x4000          | 0x800 ,
    0x8000          | 0x800                 | 0x100,
    0x10000          | 0x800         | 0x200        ,
    0x20000         | 0x800         | 0x200 | 0x100,//10
    0x40000         | 0x800 | 0x400 ,
    0x2000         | 0x800 | 0x400         | 0x100,
    0x4000          | 0x800 | 0x400 | 0x200,
    0x8000          | 0x800 | 0x400 | 0x200 | 0x100,
    0x10000 | 0x1000,                                   //15th
    0x20000 | 0x1000                        | 0x100,
    0x40000 | 0x1000                | 0x200,
    0x2000  | 0x1000                | 0x200 | 0x100,
    0x4000  | 0x1000        | 0x400 ,
    0x8000  | 0x1000        | 0x400         | 0x100,           //20th
    0x10000 | 0x1000        | 0x400 | 0x200 | 0x100,
    0x20000 | 0x1000 | 0x800 ,
    0x40000 | 0x1000 | 0x800                | 0x100,
    0x2000  | 0x1000 | 0x800        | 0x200 | 0x100     //24th
    
};


const EVENT_DESCRIPTOR g_arrTestEvents[TEST_PROVIDER_COUNT][TEST_EVENT_COUNT] = {
{TEST_EVENT_PROV0_EVENT0, TEST_EVENT_PROV0_EVENT1,TEST_EVENT_PROV0_EVENT2,TEST_EVENT_PROV0_EVENT3,TEST_EVENT_PROV0_EVENT4,TEST_EVENT_PROV0_EVENT5,TEST_EVENT_PROV0_EVENT6,TEST_EVENT_PROV0_EVENT7,TEST_EVENT_PROV0_EVENT8,TEST_EVENT_PROV0_EVENT9,TEST_EVENT_PROV0_EVENT10,TEST_EVENT_PROV0_EVENT11,TEST_EVENT_PROV0_EVENT12,TEST_EVENT_PROV0_EVENT13,TEST_EVENT_PROV0_EVENT14,TEST_EVENT_PROV0_EVENT15,TEST_EVENT_PROV0_EVENT16,TEST_EVENT_PROV0_EVENT17,TEST_EVENT_PROV0_EVENT18,TEST_EVENT_PROV0_EVENT19,TEST_EVENT_PROV0_EVENT20,TEST_EVENT_PROV0_EVENT21,TEST_EVENT_PROV0_EVENT22,TEST_EVENT_PROV0_EVENT23,TEST_EVENT_PROV0_EVENT24},
{TEST_EVENT_PROV1_EVENT0, TEST_EVENT_PROV1_EVENT1,TEST_EVENT_PROV1_EVENT2,TEST_EVENT_PROV1_EVENT3,TEST_EVENT_PROV1_EVENT4,TEST_EVENT_PROV1_EVENT5,TEST_EVENT_PROV1_EVENT6,TEST_EVENT_PROV1_EVENT7,TEST_EVENT_PROV1_EVENT8,TEST_EVENT_PROV1_EVENT9,TEST_EVENT_PROV1_EVENT10,TEST_EVENT_PROV1_EVENT11,TEST_EVENT_PROV1_EVENT12,TEST_EVENT_PROV1_EVENT13,TEST_EVENT_PROV1_EVENT14,TEST_EVENT_PROV1_EVENT15,TEST_EVENT_PROV1_EVENT16,TEST_EVENT_PROV1_EVENT17,TEST_EVENT_PROV1_EVENT18,TEST_EVENT_PROV1_EVENT19,TEST_EVENT_PROV1_EVENT20,TEST_EVENT_PROV1_EVENT21,TEST_EVENT_PROV1_EVENT22,TEST_EVENT_PROV1_EVENT23,TEST_EVENT_PROV1_EVENT24},
{TEST_EVENT_PROV2_EVENT0, TEST_EVENT_PROV2_EVENT1,TEST_EVENT_PROV2_EVENT2,TEST_EVENT_PROV2_EVENT3,TEST_EVENT_PROV2_EVENT4,TEST_EVENT_PROV2_EVENT5,TEST_EVENT_PROV2_EVENT6,TEST_EVENT_PROV2_EVENT7,TEST_EVENT_PROV2_EVENT8,TEST_EVENT_PROV2_EVENT9,TEST_EVENT_PROV2_EVENT10,TEST_EVENT_PROV2_EVENT11,TEST_EVENT_PROV2_EVENT12,TEST_EVENT_PROV2_EVENT13,TEST_EVENT_PROV2_EVENT14,TEST_EVENT_PROV2_EVENT15,TEST_EVENT_PROV2_EVENT16,TEST_EVENT_PROV2_EVENT17,TEST_EVENT_PROV2_EVENT18,TEST_EVENT_PROV2_EVENT19,TEST_EVENT_PROV2_EVENT20,TEST_EVENT_PROV2_EVENT21,TEST_EVENT_PROV2_EVENT22,TEST_EVENT_PROV2_EVENT23,TEST_EVENT_PROV2_EVENT24},
{TEST_EVENT_PROV3_EVENT0, TEST_EVENT_PROV3_EVENT1,TEST_EVENT_PROV3_EVENT2,TEST_EVENT_PROV3_EVENT3,TEST_EVENT_PROV3_EVENT4,TEST_EVENT_PROV3_EVENT5,TEST_EVENT_PROV3_EVENT6,TEST_EVENT_PROV3_EVENT7,TEST_EVENT_PROV3_EVENT8,TEST_EVENT_PROV3_EVENT9,TEST_EVENT_PROV3_EVENT10,TEST_EVENT_PROV3_EVENT11,TEST_EVENT_PROV3_EVENT12,TEST_EVENT_PROV3_EVENT13,TEST_EVENT_PROV3_EVENT14,TEST_EVENT_PROV3_EVENT15,TEST_EVENT_PROV3_EVENT16,TEST_EVENT_PROV3_EVENT17,TEST_EVENT_PROV3_EVENT18,TEST_EVENT_PROV3_EVENT19,TEST_EVENT_PROV3_EVENT20,TEST_EVENT_PROV3_EVENT21,TEST_EVENT_PROV3_EVENT22,TEST_EVENT_PROV3_EVENT23,TEST_EVENT_PROV3_EVENT24},
{TEST_EVENT_PROV4_EVENT0, TEST_EVENT_PROV4_EVENT1,TEST_EVENT_PROV4_EVENT2,TEST_EVENT_PROV4_EVENT3,TEST_EVENT_PROV4_EVENT4,TEST_EVENT_PROV4_EVENT5,TEST_EVENT_PROV4_EVENT6,TEST_EVENT_PROV4_EVENT7,TEST_EVENT_PROV4_EVENT8,TEST_EVENT_PROV4_EVENT9,TEST_EVENT_PROV4_EVENT10,TEST_EVENT_PROV4_EVENT11,TEST_EVENT_PROV4_EVENT12,TEST_EVENT_PROV4_EVENT13,TEST_EVENT_PROV4_EVENT14,TEST_EVENT_PROV4_EVENT15,TEST_EVENT_PROV4_EVENT16,TEST_EVENT_PROV4_EVENT17,TEST_EVENT_PROV4_EVENT18,TEST_EVENT_PROV4_EVENT19,TEST_EVENT_PROV4_EVENT20,TEST_EVENT_PROV4_EVENT21,TEST_EVENT_PROV4_EVENT22,TEST_EVENT_PROV4_EVENT23,TEST_EVENT_PROV4_EVENT24},
{TEST_EVENT_PROV5_EVENT0, TEST_EVENT_PROV5_EVENT1,TEST_EVENT_PROV5_EVENT2,TEST_EVENT_PROV5_EVENT3,TEST_EVENT_PROV5_EVENT4,TEST_EVENT_PROV5_EVENT5,TEST_EVENT_PROV5_EVENT6,TEST_EVENT_PROV5_EVENT7,TEST_EVENT_PROV5_EVENT8,TEST_EVENT_PROV5_EVENT9,TEST_EVENT_PROV5_EVENT10,TEST_EVENT_PROV5_EVENT11,TEST_EVENT_PROV5_EVENT12,TEST_EVENT_PROV5_EVENT13,TEST_EVENT_PROV5_EVENT14,TEST_EVENT_PROV5_EVENT15,TEST_EVENT_PROV5_EVENT16,TEST_EVENT_PROV5_EVENT17,TEST_EVENT_PROV5_EVENT18,TEST_EVENT_PROV5_EVENT19,TEST_EVENT_PROV5_EVENT20,TEST_EVENT_PROV5_EVENT21,TEST_EVENT_PROV5_EVENT22,TEST_EVENT_PROV5_EVENT23,TEST_EVENT_PROV5_EVENT24},
{TEST_EVENT_PROV6_EVENT0, TEST_EVENT_PROV6_EVENT1,TEST_EVENT_PROV6_EVENT2,TEST_EVENT_PROV6_EVENT3,TEST_EVENT_PROV6_EVENT4,TEST_EVENT_PROV6_EVENT5,TEST_EVENT_PROV6_EVENT6,TEST_EVENT_PROV6_EVENT7,TEST_EVENT_PROV6_EVENT8,TEST_EVENT_PROV6_EVENT9,TEST_EVENT_PROV6_EVENT10,TEST_EVENT_PROV6_EVENT11,TEST_EVENT_PROV6_EVENT12,TEST_EVENT_PROV6_EVENT13,TEST_EVENT_PROV6_EVENT14,TEST_EVENT_PROV6_EVENT15,TEST_EVENT_PROV6_EVENT16,TEST_EVENT_PROV6_EVENT17,TEST_EVENT_PROV6_EVENT18,TEST_EVENT_PROV6_EVENT19,TEST_EVENT_PROV6_EVENT20,TEST_EVENT_PROV6_EVENT21,TEST_EVENT_PROV6_EVENT22,TEST_EVENT_PROV6_EVENT23,TEST_EVENT_PROV6_EVENT24},
{TEST_EVENT_PROV7_EVENT0, TEST_EVENT_PROV7_EVENT1,TEST_EVENT_PROV7_EVENT2,TEST_EVENT_PROV7_EVENT3,TEST_EVENT_PROV7_EVENT4,TEST_EVENT_PROV7_EVENT5,TEST_EVENT_PROV7_EVENT6,TEST_EVENT_PROV7_EVENT7,TEST_EVENT_PROV7_EVENT8,TEST_EVENT_PROV7_EVENT9,TEST_EVENT_PROV7_EVENT10,TEST_EVENT_PROV7_EVENT11,TEST_EVENT_PROV7_EVENT12,TEST_EVENT_PROV7_EVENT13,TEST_EVENT_PROV7_EVENT14,TEST_EVENT_PROV7_EVENT15,TEST_EVENT_PROV7_EVENT16,TEST_EVENT_PROV7_EVENT17,TEST_EVENT_PROV7_EVENT18,TEST_EVENT_PROV7_EVENT19,TEST_EVENT_PROV7_EVENT20,TEST_EVENT_PROV7_EVENT21,TEST_EVENT_PROV7_EVENT22,TEST_EVENT_PROV7_EVENT23,TEST_EVENT_PROV7_EVENT24},
{TEST_EVENT_PROV8_EVENT0, TEST_EVENT_PROV8_EVENT1,TEST_EVENT_PROV8_EVENT2,TEST_EVENT_PROV8_EVENT3,TEST_EVENT_PROV8_EVENT4,TEST_EVENT_PROV8_EVENT5,TEST_EVENT_PROV8_EVENT6,TEST_EVENT_PROV8_EVENT7,TEST_EVENT_PROV8_EVENT8,TEST_EVENT_PROV8_EVENT9,TEST_EVENT_PROV8_EVENT10,TEST_EVENT_PROV8_EVENT11,TEST_EVENT_PROV8_EVENT12,TEST_EVENT_PROV8_EVENT13,TEST_EVENT_PROV8_EVENT14,TEST_EVENT_PROV8_EVENT15,TEST_EVENT_PROV8_EVENT16,TEST_EVENT_PROV8_EVENT17,TEST_EVENT_PROV8_EVENT18,TEST_EVENT_PROV8_EVENT19,TEST_EVENT_PROV8_EVENT20,TEST_EVENT_PROV8_EVENT21,TEST_EVENT_PROV8_EVENT22,TEST_EVENT_PROV8_EVENT23,TEST_EVENT_PROV8_EVENT24},
{TEST_EVENT_PROV9_EVENT0, TEST_EVENT_PROV9_EVENT1,TEST_EVENT_PROV9_EVENT2,TEST_EVENT_PROV9_EVENT3,TEST_EVENT_PROV9_EVENT4,TEST_EVENT_PROV9_EVENT5,TEST_EVENT_PROV9_EVENT6,TEST_EVENT_PROV9_EVENT7,TEST_EVENT_PROV9_EVENT8,TEST_EVENT_PROV9_EVENT9,TEST_EVENT_PROV9_EVENT10,TEST_EVENT_PROV9_EVENT11,TEST_EVENT_PROV9_EVENT12,TEST_EVENT_PROV9_EVENT13,TEST_EVENT_PROV9_EVENT14,TEST_EVENT_PROV9_EVENT15,TEST_EVENT_PROV9_EVENT16,TEST_EVENT_PROV9_EVENT17,TEST_EVENT_PROV9_EVENT18,TEST_EVENT_PROV9_EVENT19,TEST_EVENT_PROV9_EVENT20,TEST_EVENT_PROV9_EVENT21,TEST_EVENT_PROV9_EVENT22,TEST_EVENT_PROV9_EVENT23,TEST_EVENT_PROV9_EVENT24},
{TEST_EVENT_PROV10_EVENT0, TEST_EVENT_PROV10_EVENT1,TEST_EVENT_PROV10_EVENT2,TEST_EVENT_PROV10_EVENT3,TEST_EVENT_PROV10_EVENT4,TEST_EVENT_PROV10_EVENT5,TEST_EVENT_PROV10_EVENT6,TEST_EVENT_PROV10_EVENT7,TEST_EVENT_PROV10_EVENT8,TEST_EVENT_PROV10_EVENT9,TEST_EVENT_PROV10_EVENT10,TEST_EVENT_PROV10_EVENT11,TEST_EVENT_PROV10_EVENT12,TEST_EVENT_PROV10_EVENT13,TEST_EVENT_PROV10_EVENT14,TEST_EVENT_PROV10_EVENT15,TEST_EVENT_PROV10_EVENT16,TEST_EVENT_PROV10_EVENT17,TEST_EVENT_PROV10_EVENT18,TEST_EVENT_PROV10_EVENT19,TEST_EVENT_PROV10_EVENT20,TEST_EVENT_PROV10_EVENT21,TEST_EVENT_PROV10_EVENT22,TEST_EVENT_PROV10_EVENT23,TEST_EVENT_PROV10_EVENT24},
{TEST_EVENT_PROV11_EVENT0, TEST_EVENT_PROV11_EVENT1,TEST_EVENT_PROV11_EVENT2,TEST_EVENT_PROV11_EVENT3,TEST_EVENT_PROV11_EVENT4,TEST_EVENT_PROV11_EVENT5,TEST_EVENT_PROV11_EVENT6,TEST_EVENT_PROV11_EVENT7,TEST_EVENT_PROV11_EVENT8,TEST_EVENT_PROV11_EVENT9,TEST_EVENT_PROV11_EVENT10,TEST_EVENT_PROV11_EVENT11,TEST_EVENT_PROV11_EVENT12,TEST_EVENT_PROV11_EVENT13,TEST_EVENT_PROV11_EVENT14,TEST_EVENT_PROV11_EVENT15,TEST_EVENT_PROV11_EVENT16,TEST_EVENT_PROV11_EVENT17,TEST_EVENT_PROV11_EVENT18,TEST_EVENT_PROV11_EVENT19,TEST_EVENT_PROV11_EVENT20,TEST_EVENT_PROV11_EVENT21,TEST_EVENT_PROV11_EVENT22,TEST_EVENT_PROV11_EVENT23,TEST_EVENT_PROV11_EVENT24},
{TEST_EVENT_PROV12_EVENT0, TEST_EVENT_PROV12_EVENT1,TEST_EVENT_PROV12_EVENT2,TEST_EVENT_PROV12_EVENT3,TEST_EVENT_PROV12_EVENT4,TEST_EVENT_PROV12_EVENT5,TEST_EVENT_PROV12_EVENT6,TEST_EVENT_PROV12_EVENT7,TEST_EVENT_PROV12_EVENT8,TEST_EVENT_PROV12_EVENT9,TEST_EVENT_PROV12_EVENT10,TEST_EVENT_PROV12_EVENT11,TEST_EVENT_PROV12_EVENT12,TEST_EVENT_PROV12_EVENT13,TEST_EVENT_PROV12_EVENT14,TEST_EVENT_PROV12_EVENT15,TEST_EVENT_PROV12_EVENT16,TEST_EVENT_PROV12_EVENT17,TEST_EVENT_PROV12_EVENT18,TEST_EVENT_PROV12_EVENT19,TEST_EVENT_PROV12_EVENT20,TEST_EVENT_PROV12_EVENT21,TEST_EVENT_PROV12_EVENT22,TEST_EVENT_PROV12_EVENT23,TEST_EVENT_PROV12_EVENT24},
{TEST_EVENT_PROV13_EVENT0, TEST_EVENT_PROV13_EVENT1,TEST_EVENT_PROV13_EVENT2,TEST_EVENT_PROV13_EVENT3,TEST_EVENT_PROV13_EVENT4,TEST_EVENT_PROV13_EVENT5,TEST_EVENT_PROV13_EVENT6,TEST_EVENT_PROV13_EVENT7,TEST_EVENT_PROV13_EVENT8,TEST_EVENT_PROV13_EVENT9,TEST_EVENT_PROV13_EVENT10,TEST_EVENT_PROV13_EVENT11,TEST_EVENT_PROV13_EVENT12,TEST_EVENT_PROV13_EVENT13,TEST_EVENT_PROV13_EVENT14,TEST_EVENT_PROV13_EVENT15,TEST_EVENT_PROV13_EVENT16,TEST_EVENT_PROV13_EVENT17,TEST_EVENT_PROV13_EVENT18,TEST_EVENT_PROV13_EVENT19,TEST_EVENT_PROV13_EVENT20,TEST_EVENT_PROV13_EVENT21,TEST_EVENT_PROV13_EVENT22,TEST_EVENT_PROV13_EVENT23,TEST_EVENT_PROV13_EVENT24},
{TEST_EVENT_PROV14_EVENT0, TEST_EVENT_PROV14_EVENT1,TEST_EVENT_PROV14_EVENT2,TEST_EVENT_PROV14_EVENT3,TEST_EVENT_PROV14_EVENT4,TEST_EVENT_PROV14_EVENT5,TEST_EVENT_PROV14_EVENT6,TEST_EVENT_PROV14_EVENT7,TEST_EVENT_PROV14_EVENT8,TEST_EVENT_PROV14_EVENT9,TEST_EVENT_PROV14_EVENT10,TEST_EVENT_PROV14_EVENT11,TEST_EVENT_PROV14_EVENT12,TEST_EVENT_PROV14_EVENT13,TEST_EVENT_PROV14_EVENT14,TEST_EVENT_PROV14_EVENT15,TEST_EVENT_PROV14_EVENT16,TEST_EVENT_PROV14_EVENT17,TEST_EVENT_PROV14_EVENT18,TEST_EVENT_PROV14_EVENT19,TEST_EVENT_PROV14_EVENT20,TEST_EVENT_PROV14_EVENT21,TEST_EVENT_PROV14_EVENT22,TEST_EVENT_PROV14_EVENT23,TEST_EVENT_PROV14_EVENT24},
{TEST_EVENT_PROV15_EVENT0, TEST_EVENT_PROV15_EVENT1,TEST_EVENT_PROV15_EVENT2,TEST_EVENT_PROV15_EVENT3,TEST_EVENT_PROV15_EVENT4,TEST_EVENT_PROV15_EVENT5,TEST_EVENT_PROV15_EVENT6,TEST_EVENT_PROV15_EVENT7,TEST_EVENT_PROV15_EVENT8,TEST_EVENT_PROV15_EVENT9,TEST_EVENT_PROV15_EVENT10,TEST_EVENT_PROV15_EVENT11,TEST_EVENT_PROV15_EVENT12,TEST_EVENT_PROV15_EVENT13,TEST_EVENT_PROV15_EVENT14,TEST_EVENT_PROV15_EVENT15,TEST_EVENT_PROV15_EVENT16,TEST_EVENT_PROV15_EVENT17,TEST_EVENT_PROV15_EVENT18,TEST_EVENT_PROV15_EVENT19,TEST_EVENT_PROV15_EVENT20,TEST_EVENT_PROV15_EVENT21,TEST_EVENT_PROV15_EVENT22,TEST_EVENT_PROV15_EVENT23,TEST_EVENT_PROV15_EVENT24}
};
const GUID g_arrTestProviderGUIDs[TEST_PROC_COUNT] = {
    WINDOWSCE_EVENTLOG_FUNC_TEST_PROV_0,
    WINDOWSCE_EVENTLOG_FUNC_TEST_PROV_1,
    WINDOWSCE_EVENTLOG_FUNC_TEST_PROV_2,
    WINDOWSCE_EVENTLOG_FUNC_TEST_PROV_3,
    WINDOWSCE_EVENTLOG_FUNC_TEST_PROV_4,
    WINDOWSCE_EVENTLOG_FUNC_TEST_PROV_5,
    WINDOWSCE_EVENTLOG_FUNC_TEST_PROV_6,
    WINDOWSCE_EVENTLOG_FUNC_TEST_PROV_7,
    WINDOWSCE_EVENTLOG_FUNC_TEST_PROV_8,
    WINDOWSCE_EVENTLOG_FUNC_TEST_PROV_9,
    WINDOWSCE_EVENTLOG_FUNC_TEST_PROV_10,
    WINDOWSCE_EVENTLOG_FUNC_TEST_PROV_11,
    WINDOWSCE_EVENTLOG_FUNC_TEST_PROV_12,
    WINDOWSCE_EVENTLOG_FUNC_TEST_PROV_13,
    WINDOWSCE_EVENTLOG_FUNC_TEST_PROV_14,
    WINDOWSCE_EVENTLOG_FUNC_TEST_PROV_15
};

const WCHAR g_arrSTestProviderGUIDs[TEST_PROC_COUNT][100] = {
    L"B6406204-DB58-425A-B5D5-69529BCBA368",
    L"7042D56B-814D-4EC6-ADFB-9D250651A8FA",
    L"1340D8CE-35B4-45A7-B1A9-973D48A75CF6",
    L"FCBF65A5-3B56-4670-B018-7FE53AC1DE2B",
    L"58137DD1-154D-443A-9012-E88900307EA0",
    L"E4219D76-5ABA-4E81-9458-902983C8E32F",
    L"180B6DC9-EEEC-406A-968E-1E200BC711B9",
    L"7888120B-2E1E-4FAC-BA00-30A992D08A57",
    L"18C09E2F-A060-4112-9B98-F50AE4C7E997",
    L"7F2ADAE2-EAA3-49FF-9947-C3584D4B6ED9",
    L"256FDD28-2F90-490D-A532-8FD3266BACA6",
    L"5205A640-BD29-4EFA-8050-3681EBDB7907",
    L"4EDB7B59-17FB-434E-B767-0085E6AC35B6",
    L"E14623DD-A93F-4077-BBDA-28276F927610",
    L"90672516-F007-482A-9B34-E8C4D4B2C878",
    L"F9E8FA7B-04D2-4DA4-9A25-ACF7E701866F"
};

const DWORD g_arrTestCIDs[TEST_CHANNEL_COUNT] = {
    TestChannel0,
    TestChannel1,
    TestChannel2,
    TestChannel3,
    TestChannel4,
    TestChannel5,
    TestChannel6,
    TestChannel7
};
const WCHAR g_arrTestChannelPaths[TEST_CHANNEL_COUNT][MAX_PATH] = {
    L"MS-CE-EL-TestChannel0",
    L"MS-CE-EL-TestChannel1",
    L"MS-CE-EL-TestChannel2",
    L"MS-CE-EL-TestChannel3",
    L"MS-CE-EL-TestChannel4",
    L"MS-CE-EL-TestChannel5",
    L"MS-CE-EL-TestChannel6",
    L"MS-CE-EL-TestChannel7"
};
WCHAR g_arrTestProviderMultiSZs[TEST_PROVIDER_COUNT][MAX_PATH] = {
L"MS-CE-EL-TestChannel0\0",
L"MS-CE-EL-TestChannel1\0",
L"MS-CE-EL-TestChannel2\0",
L"MS-CE-EL-TestChannel3\0",
L"MS-CE-EL-TestChannel4\0",
L"MS-CE-EL-TestChannel5\0",
L"MS-CE-EL-TestChannel6\0",
L"MS-CE-EL-TestChannel7\0",
L"MS-CE-EL-TestChannel0\0MS-CE-EL-TestChannel1\0",
L"MS-CE-EL-TestChannel2\0MS-CE-EL-TestChannel3\0",
L"MS-CE-EL-TestChannel4\0MS-CE-EL-TestChannel5\0",
L"MS-CE-EL-TestChannel6\0MS-CE-EL-TestChannel7\0",
L"MS-CE-EL-TestChannel0\0MS-CE-EL-TestChannel1\0MS-CE-EL-TestChannel2\0MS-CE-EL-TestChannel3\0",
L"MS-CE-EL-TestChannel4\0MS-CE-EL-TestChannel5\0MS-CE-EL-TestChannel6\0MS-CE-EL-TestChannel7\0",
L"MS-CE-EL-TestChannel0\0MS-CE-EL-TestChannel1\0MS-CE-EL-TestChannel2\0MS-CE-EL-TestChannel3\0MS-CE-EL-TestChannel4\0MS-CE-EL-TestChannel5\0MS-CE-EL-TestChannel6\0MS-CE-EL-TestChannel7\0",
L"MS-CE-EL-TestChannel0\0MS-CE-EL-TestChannel1\0MS-CE-EL-TestChannel2\0MS-CE-EL-TestChannel3\0MS-CE-EL-TestChannel4\0MS-CE-EL-TestChannel5\0MS-CE-EL-TestChannel6\0MS-CE-EL-TestChannel7\0"
};

//Flags for use in ProcCmd, specific to API being called
//CeEventRead
#define WAIT_ON_EVENT           0x1
#define DONT_LOG                0x1

//Structs for internal eventing use
struct ProcCmd{
    DWORD dwMessageIndex;
    int nAPIIndex;
    int nHandleIndex;
    int nChannelIndex;
    int nEventIndex;
    int nOptions;
    LONG lExpected;
    ULONGLONG ullAnyKeyword;
    ULONGLONG ullAllKeyword;
    int nLevel;
};

struct TestResult{
    ProcCmd cmd;
    BOOL bResult;
    LONG lResult;
    ULONGLONG ullExecutionTime;
};


const enum CMD_EVENTLOG_APIS{
    CMD_EVENTREGISTER,
    CMD_EVENTENABLED,
    CMD_EVENTPROVIDERENABLED,
    CMD_EVENTWRITE,
    CMD_EVENTUNREGISTER,
    CMD_CEEVENTSUBSCRIBE,
    CMD_CEEVENTNEXT,
    CMD_CEEVENTREAD,
    CMD_CEEVENTCLOSE
};
const DWORD g_arrProvidesToChannelsMask[TEST_PROVIDER_COUNT] = {
    0x1, 0x2, 0x4, 0x8, 0xF, 0x10, 0x20, 0x40, 
    0x1 | 0x2,
    0x4 | 0x8,
    0xF | 0x10,
    0x20 | 0x40, 
    0x1 | 0x2 | 0x4 | 0x8,
    0xF | 0x10 | 0x20 | 0x40, 
    0x1 | 0x2 | 0x4 | 0x8 | 0xF | 0x10 | 0x20 | 0x40,
    0x1 | 0x2 | 0x4 | 0x8 | 0xF | 0x10 | 0x20 | 0x40
};


const enum TEST_CHANNEL_ENUM{
    FUNC_TEST_CHANNEL_0,
    FUNC_TEST_CHANNEL_1,
    FUNC_TEST_CHANNEL_2,
    FUNC_TEST_CHANNEL_3,
    FUNC_TEST_CHANNEL_4,
    FUNC_TEST_CHANNEL_5,
    FUNC_TEST_CHANNEL_6,
    FUNC_TEST_CHANNEL_7,
    FUNC_TEST_CHANNEL_8,
    FUNC_TEST_CHANNEL_9,
    FUNC_TEST_CHANNEL_10,
    FUNC_TEST_CHANNEL_11,
    FUNC_TEST_CHANNEL_12,
    FUNC_TEST_CHANNEL_13,
    FUNC_TEST_CHANNEL_14,
    FUNC_TEST_CHANNEL_15,
    FUNC_TEST_CHANNEL_16,
    FUNC_TEST_CHANNEL_17,
    FUNC_TEST_CHANNEL_18,
    FUNC_TEST_CHANNEL_19
};

const enum TEST_PROVIDER_ENUM{
    FUNC_TEST_PROVIDER_0,
    FUNC_TEST_PROVIDER_1,
    FUNC_TEST_PROVIDER_2,
    FUNC_TEST_PROVIDER_3,
    FUNC_TEST_PROVIDER_4,
    FUNC_TEST_PROVIDER_5,
    FUNC_TEST_PROVIDER_6,
    FUNC_TEST_PROVIDER_7,
    FUNC_TEST_PROVIDER_8,
    FUNC_TEST_PROVIDER_9,
    FUNC_TEST_PROVIDER_10,
    FUNC_TEST_PROVIDER_11,
    FUNC_TEST_PROVIDER_12,
    FUNC_TEST_PROVIDER_13,
    FUNC_TEST_PROVIDER_14,
    FUNC_TEST_PROVIDER_15,
    FUNC_TEST_PROVIDER_16,
    FUNC_TEST_PROVIDER_17,
    FUNC_TEST_PROVIDER_18,
    FUNC_TEST_PROVIDER_19
};

const enum FUNC_TEST_EVENTS{
    FUNC_TEST_EVENT_0,
    FUNC_TEST_EVENT_1,
    FUNC_TEST_EVENT_2,
    FUNC_TEST_EVENT_3,
    FUNC_TEST_EVENT_4,
    FUNC_TEST_EVENT_5,
    FUNC_TEST_EVENT_6,
    FUNC_TEST_EVENT_7,
    FUNC_TEST_EVENT_8,
    FUNC_TEST_EVENT_9,
    FUNC_TEST_EVENT_10,
    FUNC_TEST_EVENT_11,
    FUNC_TEST_EVENT_12,
    FUNC_TEST_EVENT_13,
    FUNC_TEST_EVENT_14,
    FUNC_TEST_EVENT_15,
    FUNC_TEST_EVENT_16,
    FUNC_TEST_EVENT_17,
    FUNC_TEST_EVENT_18,
    FUNC_TEST_EVENT_19,
    FUNC_TEST_EVENT_20,
    FUNC_TEST_EVENT_21,
    FUNC_TEST_EVENT_22,
    FUNC_TEST_EVENT_23,
    FUNC_TEST_EVENT_24
};


#endif //__EVENTLOG_FUNC_CONSTANTS_H__