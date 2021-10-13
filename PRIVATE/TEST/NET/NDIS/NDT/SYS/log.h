//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
#ifndef __LOG_H
#define __LOG_H

//------------------------------------------------------------------------------

#define NDT_XXX(a, b, c)      ((a << 24)|(b << 16)|c)

//------------------------------------------------------------------------------

#define NDT_ERR               0x0002
#define NDT_WRN               0x0003
#define NDT_INF               0x000A
#define NDT_DBG               0x000C
#define NDT_CMT               0x000E
#define NDT_VRB               0x000F

#define NDT_LOG_MASK          0x000F
#define NDT_LOG_PACKETS       0x8000

//------------------------------------------------------------------------------

#define NDT_ERR_MEMORY_LEAKS                          NDT_XXX(NDT_ERR, 0, 0)
#define NDT_ERR_MEMORY_SIGNATURE                      NDT_XXX(NDT_ERR, 0, 1)
#define NDT_ERR_MEMORY_BLOCK                          NDT_XXX(NDT_ERR, 0, 2)
#define NDT_ERR_MEMORY_TAIL                           NDT_XXX(NDT_ERR, 0, 3)
#define NDT_ERR_MEMORY_DUMP                           NDT_XXX(NDT_ERR, 0, 4)
#define NDT_INF_BEAT_LOG1                             NDT_XXX(NDT_INF, 0, 5)
#define NDT_INF_BEAT_LOG2                             NDT_XXX(NDT_INF, 0, 6)
#define NDT_INF_BEAT_SEND                             NDT_XXX(NDT_INF, 0, 7)

//------------------------------------------------------------------------------

#define NDT_DBG_BIND_ADAPTER_0_ENTRY                  NDT_XXX(NDT_DBG, 1, 0)
#define NDT_DBG_BIND_ADAPTER_0_EXIT                   NDT_XXX(NDT_DBG, 1, 1)
#define NDT_DBG_BIND_ADAPTER_1_ENTRY                  NDT_XXX(NDT_DBG, 1, 2)
#define NDT_DBG_BIND_ADAPTER_1_EXIT                   NDT_XXX(NDT_DBG, 1, 3)
#define NDT_DBG_PROTOCOL_UNLOAD_0_ENTRY               NDT_XXX(NDT_DBG, 1, 4)
#define NDT_DBG_PROTOCOL_UNLOAD_0_EXIT                NDT_XXX(NDT_DBG, 1, 5)
#define NDT_DBG_PROTOCOL_UNLOAD_1_ENTRY               NDT_XXX(NDT_DBG, 1, 6)
#define NDT_DBG_PROTOCOL_UNLOAD_1_EXIT                NDT_XXX(NDT_DBG, 1, 7)
#define NDT_DBG_PNP_EVENT_0_ENTRY                     NDT_XXX(NDT_DBG, 1, 8)
#define NDT_DBG_PNP_EVENT_0_EXIT                      NDT_XXX(NDT_DBG, 1, 9)
#define NDT_DBG_PNP_EVENT_1_ENTRY                     NDT_XXX(NDT_DBG, 1, 10)
#define NDT_DBG_PNP_EVENT_1_EXIT                      NDT_XXX(NDT_DBG, 1, 11)
#define NDT_DBG_OPEN_ADAPTER_COMPLETE_ENTRY           NDT_XXX(NDT_DBG, 1, 12)
#define NDT_DBG_OPEN_ADAPTER_COMPLETE_EXIT            NDT_XXX(NDT_DBG, 1, 13)
#define NDT_DBG_CLOSE_ADAPTER_COMPLETE_ENTRY          NDT_XXX(NDT_DBG, 1, 14)
#define NDT_DBG_CLOSE_ADAPTER_COMPLETE_EXIT           NDT_XXX(NDT_DBG, 1, 15)
#define NDT_DBG_RESET_COMPLETE_ENTRY                  NDT_XXX(NDT_DBG, 1, 16)
#define NDT_DBG_RESET_COMPLETE_EXIT                   NDT_XXX(NDT_DBG, 1, 17)
#define NDT_DBG_REQUEST_COMPLETE_ENTRY                NDT_XXX(NDT_DBG, 1, 18)
#define NDT_DBG_REQUEST_COMPLETE_EXIT                 NDT_XXX(NDT_DBG, 1, 19)
#define NDT_DBG_STATUS_ENTRY                          NDT_XXX(NDT_DBG, 1, 20)
#define NDT_DBG_STATUS_EXIT                           NDT_XXX(NDT_DBG, 1, 21)
#define NDT_DBG_STATUS_COMPLETE_ENTRY                 NDT_XXX(NDT_DBG, 1, 22)
#define NDT_DBG_STATUS_COMPLETE_EXIT                  NDT_XXX(NDT_DBG, 1, 23)
#define NDT_DBG_SEND_COMPLETE_ENTRY                   NDT_XXX(NDT_DBG, 1, 24)
#define NDT_DBG_SEND_COMPLETE_EXIT                    NDT_XXX(NDT_DBG, 1, 25)
#define NDT_DBG_TRANSFER_DATA_COMPLETE_ENTRY          NDT_XXX(NDT_DBG, 1, 26)
#define NDT_DBG_TRANSFER_DATA_COMPLETE_EXIT           NDT_XXX(NDT_DBG, 1, 27)
#define NDT_DBG_RECEIVE_ENTRY                         NDT_XXX(NDT_DBG, 1, 28)
#define NDT_DBG_RECEIVE_EXIT                          NDT_XXX(NDT_DBG, 1, 29)
#define NDT_DBG_RECEIVE_COMPLETE_ENTRY                NDT_XXX(NDT_DBG, 1, 30)
#define NDT_DBG_RECEIVE_COMPLETE_EXIT                 NDT_XXX(NDT_DBG, 1, 31)
#define NDT_DBG_RECEIVE_PACKET_ENTRY                  NDT_XXX(NDT_DBG, 1, 32)
#define NDT_DBG_RECEIVE_PACKET_EXIT                   NDT_XXX(NDT_DBG, 1, 33)
#define NDT_DBG_UNBIND_ADAPTER_ENTRY                  NDT_XXX(NDT_DBG, 1, 34)
#define NDT_DBG_UNBIND_ADAPTER_EXIT                   NDT_XXX(NDT_DBG, 1, 35)
#define NDT_DBG_CO_SEND_COMPLETE_ENTRY                NDT_XXX(NDT_DBG, 1, 36)
#define NDT_DBG_CO_SEND_COMPLETE_EXIT                 NDT_XXX(NDT_DBG, 1, 37)
#define NDT_DBG_CO_STATUS_ENTRY                       NDT_XXX(NDT_DBG, 1, 38)
#define NDT_DBG_CO_STATUS_EXIT                        NDT_XXX(NDT_DBG, 1, 39)
#define NDT_DBG_CO_RECEIVE_PACKET_ENTRY               NDT_XXX(NDT_DBG, 1, 40)
#define NDT_DBG_CO_RECEIVE_PACKET_EXIT                NDT_XXX(NDT_DBG, 1, 41)
#define NDT_DBG_CO_AF_REGISTER_ENTRY                  NDT_XXX(NDT_DBG, 1, 42)
#define NDT_DBG_CO_AF_REGISTER_EXIT                   NDT_XXX(NDT_DBG, 1, 43)

//------------------------------------------------------------------------------

#define NDT_ERR_NEW_PROTOCOL_40                       NDT_XXX(NDT_ERR, 2, 0)
#define NDT_ERR_REGISTER_PROTOCOL_40                  NDT_XXX(NDT_ERR, 2, 1)
#define NDT_ERR_NEW_PROTOCOL_5X                       NDT_XXX(NDT_ERR, 2, 2)
#define NDT_ERR_REGISTER_PROTOCOL_5X                  NDT_XXX(NDT_ERR, 2, 3)

//------------------------------------------------------------------------------

#define NDT_ERR_UNEXP_OPEN_ADAPTER_COMPLETE           NDT_XXX(NDT_ERR, 3, 0)
#define NDT_ERR_UNEXP_CLOSE_ADAPTER_COMPLETE          NDT_XXX(NDT_ERR, 3, 1)
#define NDT_ERR_UNEXP_RESET_COMPLETE                  NDT_XXX(NDT_ERR, 3, 2)
#define NDT_ERR_UNEXP_REQUEST_COMPLETE                NDT_XXX(NDT_ERR, 3, 3)
#define NDT_ERR_UNEXP_SEND_COMPLETE_PACKET            NDT_XXX(NDT_ERR, 3, 4)
#define NDT_ERR_UNEXP_SEND_COMPLETE_REQ               NDT_XXX(NDT_ERR, 3, 5)
#define NDT_ERR_UNEXP_SEND_COMPLETE_REQ_TYPE          NDT_XXX(NDT_ERR, 3, 6)
#define NDT_ERR_RECV_PACKET_ZERO_LENGTH               NDT_XXX(NDT_ERR, 3, 7)
#define NDT_ERR_RECV_PACKET_TOO_LARGE                 NDT_XXX(NDT_ERR, 3, 8)
#define NDT_ERR_RECV_PACKET_COPY                      NDT_XXX(NDT_ERR, 3, 9)
#define NDT_ERR_RECV_PACKET_SIZE                      NDT_XXX(NDT_ERR, 3, 10)
#define NDT_ERR_RECV_PACKET_LOOKHEAD_SIZE             NDT_XXX(NDT_ERR, 3, 11)
#define NDT_ERR_RECV_PACKET_OUTOFDESCRIPTORS          NDT_XXX(NDT_ERR, 3, 12)
#define NDT_ERR_RECV_PACKET_TRANSFER                  NDT_XXX(NDT_ERR, 3, 13)
#define NDT_ERR_UNEXP_TRANSFER_DATA_PACKET            NDT_XXX(NDT_ERR, 3, 14)
#define NDT_ERR_TRANSFER_DATA_FAILED                  NDT_XXX(NDT_ERR, 3, 15)
#define NDT_ERR_RECV_PACKET_OUTOFDESCRIPTORS2         NDT_XXX(NDT_ERR, 3, 16)

//------------------------------------------------------------------------------

void Log(ULONG ulId, ...);
void LogSetLevel(ULONG ulLogLevel);
void LogX(LPCTSTR szFormat, ...);

//------------------------------------------------------------------------------

#endif
