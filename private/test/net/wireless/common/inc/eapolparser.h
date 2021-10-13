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
// ----------------------------------------------------------------------------
//
// Use of this source code is subject to the terms of the Microsoft end-user
// license agreement (EULA) under which you licensed this SOFTWARE PRODUCT.
// If you did not accept the terms of the EULA, you are not authorized to use
// this source code. For a copy of the EULA, please see the LICENSE.RTF on your
// install media.
//
// ----------------------------------------------------------------------------
//
#include <FrameParser.hpp>
#include <netmain.h>
#include <auto_xxx.hxx>
#include <netmontypes.hpp>
#include <vector.hxx>

enum states_e {IDLE = 0, START, EAP, PreAuth,   
	WEP_P1, WEP_G1,   
	WPA_P1, WPA_P2, WPA_P3,	WPA_P4, WPA_G1, WPA_G2,    
	WPA2_P1, WPA2_P2, WPA2_P3, WPA2_P4,    
	DHCP_1, DHCP_2};
enum message_e {TYPE_WEP, TYPE_WPA, TYPE_WPA2, TYPE_PSK_WPA, TYPE_PSK_WPA2};

#define MAX_STATES 18

#define SUCCESS		3

#define KEY			3
#define UNICAST		1
#define BROADCAST	0

#define KEY_WEP		1
#define KEY_WPA2	2
#define KEY_WPA		254

#define DHCP_ACK	0x350105  //2

struct State
{
	int state;
	_int64 TimeStamp;
	DWORD FrameNumber;
	BYTE Dest_addr[6];
	BYTE Src_addr[6];
	DWORD FrameType;
};

typedef ce::vector<State*> StateList;

struct Timing_info 
{
	State * EAPOL_Start;
	State * DHCP_Response;
	State * EAPOL_KEY_1;
	State * EAPOL_KEY_2;
	State * EAPOL_KEY_3;
	State * EAPOL_KEY_4;
	State * EAPOL_G_KEY;
	State * EAP_Begin;
	State * EAP_End;

};

class Iteration
{
public:
	Iteration(const int matrix[MAX_STATES][MAX_STATES]);
	~Iteration();
	int TransitionTo(int next, _int64 time, DWORD frame, bool invalid_state);
	virtual int OnKeyWEP(const ProtocolStack_t& stack);
	virtual int OnKeyWPA(const ProtocolStack_t& stack);
	virtual int OnKeyWPA2(const ProtocolStack_t& stack);

	StateList sl;
	StateList bad_state;

	//times are in ms
	_int64 roam_time;
	_int64 EAP_time;
	_int64 EAPOL_START_response;
	_int64 Fourway_handshake_CE;
	_int64 Fourway_handshake_remote;
	_int64 Fourway_handshake_total;  
	_int64 Key_time;			//used for group key time in WPA or for the key times in WEP
	int type;
	Timing_info ti;
	bool eap_auth_occured;
	bool detected_out_of_order_Start;

private:
	int transition_matrix[MAX_STATES][MAX_STATES];
	int EAPOL_count;
	int pairwiseKeyCount;
	int groupKeyCount;
	
	
};

typedef ce::vector<Iteration*> IterationList;

class EapParser : public FrameParser_t
{
public:
	EapParser( );
	virtual ~EapParser();
	virtual Iteration *NewIteration();
	void SetSrc(PBYTE Device);
	virtual void HandleProtocol(const ProtocolStack_t& stack, int protocol);
	virtual int OnEAP(const ProtocolStack_t& stack);
	virtual int OnEAPOL(const ProtocolStack_t& stack);
	virtual int OnDHCP(const ProtocolStack_t& stack);
	virtual int OnGeneralTraffic(const ProtocolStack_t& stack);
	virtual void Print_all();

	virtual double get_roam_average();

	IterationList iter_list;
	int total_iterations;
	int total_pass;
	int total_fail;	

private:
	virtual void Print_state(int state);
	BYTE DevMacAddr[6];	//Device address
	static const int WEP_WPA_WPA_matrix[MAX_STATES][MAX_STATES];
};

bool ConvertStringToMAC( WCHAR *s, BYTE src[6]);

