//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//
//
// This source code is licensed under Microsoft Shared Source License
// Version 1.0 for Windows CE.
// For a copy of the license visit http://go.microsoft.com/fwlink/?LinkId=3223.
//
/*++


Module Name:

    _mqini.h

Abstract:

    Definitions of parameters that can be read from ini file.
    Definitions of default values.
    General definitions shared among setup and QM (YoelA, 10-Feb-97)

--*/

#ifndef __TEMP_MQINI_H
#define __TEMP_MQINI_H

// Name of Falcon ini file and section.
#define MSMQ_INI_FILE            TEXT("MSMQ.INI")
#define MSMQ_INI_SECTION         TEXT("Parameters")

//---------------------------------------------------------
//  Definition for client configuration
//---------------------------------------------------------

// Registry name for remote QM machine name
#define RPC_REMOTE_QM_REGNAME     TEXT("RemoteQMMachine")

//---------------------------------------------------------
//  Definition of RPC end points
//---------------------------------------------------------

//
// If this registry does not exist (default) or it's 0 then use dynamic
// endpoints. Otherwise, use predefined ones, from registry.
// This is for MQIS interfaces.
//
#define RPC_DEFAULT_PREDEFINE_DS_EP     0
#define RPC_PREDEFINE_DS_EP_REGNAME     TEXT("UseDSPredefinedEP")
//
// as above, but for RT-QM and QM-QM interfaces.
//
#define RPC_DEFAULT_PREDEFINE_QM_EP     0
#define RPC_PREDEFINE_QM_EP_REGNAME   TEXT("UseQMPredefinedEP")

//Default local  RPC End Point between RT and QM
#define RPC_LOCAL_EP             TEXT("QMsvc")
#define RPC_LOCAL_EP_REGNAME     TEXT("RpcLocalEp")

#define RPC_LOCAL_EP2            TEXT("QMsvc2")
#define RPC_LOCAL_EP_REGNAME2    TEXT("RpcLocalEp2")

// default for RPC IP port (for QM remote read and dependent clients)
#define FALCON_DEFAULT_QM_RPC_IP_PORT   TEXT("2799")
#define FALCON_QM_RPC_IP_PORT_REGNAME   TEXT("MsmqQMRpcIpPort")

#define FALCON_DEFAULT_QM_RPC_IP_PORT2  TEXT("2801")
#define FALCON_QM_RPC_IP_PORT_REGNAME2  TEXT("MsmqQMRpcIpPort2")

// default for IPX port for RPC (for QM remote read)
#define FALCON_DEFAULT_QM_RPC_IPX_PORT  TEXT("2799")
#define FALCON_QM_RPC_IPX_PORT_REGNAME  TEXT("MsmqQMRpcIpxPort")

#define FALCON_DEFAULT_QM_RPC_IPX_PORT2 TEXT("2801")
#define FALCON_QM_RPC_IPX_PORT_REGNAME2 TEXT("MsmqQMRpcIpxPort2")

// Default local  RPC End Point between RT and MQDSSRV
#define DEFAULT_RPC_DS_LOCAL_EP     TEXT("DSLocal")
#define RPC_DS_LOCAL_EP_REGNAME     TEXT("DSRpcLocalEp")

// default for RPC IP port (for DS)
#define FALCON_DEFAULT_DS_RPC_IP_PORT   TEXT("2879")
#define FALCON_DS_RPC_IP_PORT_REGNAME   TEXT("MsmqDSRpcIpPort")

// default for IPX port for RPC (for DS)
#define FALCON_DEFAULT_DS_RPC_IPX_PORT  TEXT("2879")
#define FALCON_DS_RPC_IPX_PORT_REGNAME  TEXT("MsmqDSRpcIpxPort")

//---------------------------------------------------------
//  Definition of winsock ports
//---------------------------------------------------------

// Enable/disable ipx on win95 (1- enable, 0- disbale)
#define FALCON_DEFAULT_IPX_ON_WIN95     1
#define FALCON_IPX_ON_WIN95_REGNAME     TEXT("EnableIpx95")

// default IP port for Falcon sessions.
#define FALCON_DEFAULT_IP_PORT   1801
#define FALCON_IP_PORT_REGNAME   TEXT("MsmqIpPort")

// default IPX port for Falcon sessions.
#define FALCON_DEFAULT_IPX_PORT  876
#define FALCON_IPX_PORT_REGNAME  TEXT("MsmqIpxPort")

// default IP port for ping.
#define FALCON_DEFAULT_PING_IP_PORT   3527
#define FALCON_PING_IP_PORT_REGNAME   TEXT("MsmqIpPingPort")

// default IPX port for ping.
#define FALCON_DEFAULT_PING_IPX_PORT  3527
#define FALCON_PING_IPX_PORT_REGNAME  TEXT("MsmqIpxPingPort")


// Default for ack timeout
#define MSMQ_DEFAULT_ACKTIMEOUT  5000
#define MSMQ_ACKTIMEOUT_REGNAME  TEXT("AckTimeout")

// Default for Storage ack timeout
#define MSMQ_DEFAULT_STORE_ACKTIMEOUT  500
#define MSMQ_STORE_ACKTIMEOUT_REGNAME  TEXT("StoreAckTimeout")

// Default for Max Unacked Packet
#ifdef _DEBUG
#define MSMQ_DEFAULT_WINDOW_SIZE_PACKET  32
#else
#define MSMQ_DEFAULT_WINDOW_SIZE_PACKET  64
#endif
#define MSMQ_MAX_WINDOW_SIZE_REGNAME  TEXT("MaxUnackedPacket")

// Default for Cleanup interval
#define MSMQ_DEFAULT_SERVER_CLEANUP    120000
#define MSMQ_DEFAULT_CLIENT_CLEANUP    300000

#define MSMQ_CLEANUP_INTERVAL_REGNAME  TEXT("CleanupInterval")

#define MSMQ_DEFAULT_MESSAGE_CLEANUP    (6 * 60 * 60 * 1000)
#define MSMQ_MESSAGE_CLEANUP_INTERVAL_REGNAME  TEXT("MessageCleanupInterval")

// Default time interval for refreshing the DS servers list (12 hours)
#define MSMQ_DEFAULT_DSLIST_REFRESH  (12 * 60 * 60 * 1000)
#define MSMQ_DSLIST_REFRESH_REGNAME  TEXT("DSListRefresh")

// Minimum interval between successive ADS searches to find DS servers (in seconds) (30 minutes)
#define MSMQ_DEFAULT_DSCLI_ADSSEARCH_INTERVAL  (60 * 30)
#define MSMQ_DSCLI_ADSSEARCH_INTERVAL_REGNAME  TEXT("DSCliSearchAdsForServersIntervalSecs")

// Minimum interval between successive ADS searches to refresh IPSITE mapping (in seconds) (60 minutes)
#define MSMQ_DEFAULT_IPSITE_ADSSEARCH_INTERVAL  (60 * 60)
#define MSMQ_IPSITE_ADSSEARCH_INTERVAL_REGNAME  TEXT("DSAdsRefreshIPSitesIntervalSecs")

// For generating write requests
// Minimum interval between successive ADS searches to refresh NT4SITES mapping (in seconds) (6 hours)
#define MSMQ_DEFAULT_NT4SITES_ADSSEARCH_INTERVAL  (60 * 60 * 6)
#define MSMQ_NT4SITES_ADSSEARCH_INTERVAL_REGNAME  TEXT("DSAdsRefreshNT4SitesIntervalSecs")

// Default driver name
#define MSMQ_DEFAULT_DRIVER      TEXT("MQAC")
#define MSMQ_DRIVER_REGNAME      TEXT("DriverName")

// Unique name for QM
#define MSMQ_DEFAULT_UNIQUE      TEXT("Falcon")
#define MSMQ_UNIQUE_REGNAME      TEXT("UniqueName")

//  Name of MQIS datasource
#define MSMQ_DEFAULT_MQIS_DB     TEXT("mqis")
#define MSMQ_MQIS_DB_REGNAME     TEXT("mqisdb")

//  Name of MQIS datasource user name
#define MSMQ_DEFAULT_MQIS_USER   TEXT("sa")
#define MSMQ_MQIS_USER_REGNAME   TEXT("mqisuser")

// Name of number of connections opened with MQIS datasource
#define MSMQ_MQIS_NO_CONNECTIONS_REGNAME    TEXT("mqisConnectionNumber")
#define MSMQ_DEFAULT_MQIS_NO_CONNECTIONS    1

// Name of storage folders
#define MSMQ_STORE_RELIABLE_PATH_REGNAME        TEXT("StoreReliablePath")
#define MSMQ_STORE_PERSISTENT_PATH_REGNAME      TEXT("StorePersistentPath")
#define MSMQ_STORE_JOURNAL_PATH_REGNAME         TEXT("StoreJournalPath")
#define MSMQ_STORE_LOG_PATH_REGNAME             TEXT("StoreLogPath")

// Deafult size of memory mapped file
#define MSMQ_MESSAGE_SIZE_LIMIT_REGNAME         TEXT("MaxMessageSize")
#define MSMQ_DEFAULT_MESSAGE_SIZE_LIMIT         (4 * 1024 * 1024)

// Next message ID to be used
#define MSMQ_MESSAGE_ID_REGNAME                 TEXT("MessageID")

// Name of secured server connection to MQIS
#define MSMQ_SECURED_SERVER_CONNECTION_REGNAME  TEXT("SecuredServerConnection")
#define MSMQ_DEFAULT_SECURED_CONNECTION         0

// Name of DS server
#define MSMQ_DEFAULT_DS_SERVER   TEXT("\\\\")
#define MSMQ_DS_SERVER_REGNAME   TEXT("MachineCache\\MQISServer")
#define MSMQ_DS_CURRENT_SERVER_REGNAME \
                                 TEXT("MachineCache\\CurrentMQISServer")
#define MAX_REG_DSSERVER_LEN  1500
#define DS_SERVER_SEPERATOR_SIGN    ','

// Static DS server option
#define MSMQ_STATIC_DS_SERVER_REGNAME TEXT("MachineCache\\StaticMQISServer")

// Name of MQ service
#define MSMQ_MQS_REGNAME        TEXT("MachineCache\\MQS")

// Name of MQIS replication message timeout
#define MSMQ_MQIS_REPLICATIONTIMEOUT_REGNAME    TEXT("ReplicationMsgTimeout")

// Name of MQIS write message timeout
#define MSMQ_MQIS_WRITETIMEOUT_REGNAME  TEXT("WriteMsgTimeout")

// Name of MQIS PURGE frequency
#define MSMQ_MQIS_PURGE_FREQUENCY_REGNAME  TEXT("PurgeFrequencySN")

// Name of MQIS BSC ACK frequency
#define MSMQ_MQIS_BSC_ACK_FREQUENCY_REGNAME  TEXT("BscAckFrequency")

// Name of QM id
#define MSMQ_QMID_REGNAME   TEXT("MachineCache\\QMId")

// Name of Address
#define MSMQ_ADDRESS_REGNAME   TEXT("MachineCache\\Address")

// Name of CNs
#define MSMQ_CNS_REGNAME   TEXT("MachineCache\\CNs")

// Name of site id
#define MSMQ_SITEID_REGNAME     TEXT("MachineCache\\SiteId")
#define MSMQ_SITENAME_REGNAME   TEXT("MachineCache\\SiteName")

// Name of enterprise id
#define MSMQ_ENTERPRISEID_REGNAME   TEXT("MachineCache\\EnterpriseId")

// Name of MQIS master id
#define MSMQ_MQIS_MASTERID_REGNAME  TEXT("MachineCache\\MasterId")

// Name of key for servers cache.
#define MSMQ_SERVERS_CACHE_REGNAME  TEXT("ServersCache")

// machine quota
#define MSMQ_MACHINE_QUOTA_REGNAME TEXT("MachineCache\\MachineQuota")

// Machine journal quota
#define MSMQ_MACHINE_JOURNAL_QUOTA_REGNAME TEXT("MachineCache\\MachineJournalQuota")

// Default for transaction crash point
#define FALCON_DEFAULT_CRASH_POINT    0
#define FALCON_CRASH_POINT_REGNAME    TEXT("XactCrashPoint")

// Default for transaction crash latency
#define FALCON_DEFAULT_CRASH_LATENCY  0
#define FALCON_CRASH_LATENCY_REGNAME  TEXT("XactCrashLatency")

// Default for transaction stub processing
#define FALCON_DEFAULT_XACT_STUB      0
#define FALCON_XACT_STUB_REGNAME      TEXT("XactStub")

// Default for sequential acks resend time
#define FALCON_DEFAULT_SEQ_ACK_RESEND_TIME  60
#define FALCON_SEQ_ACK_RESEND_REGNAME  TEXT("SeqAckResendTime")

// Default for ordered resend times: 1-3, 4-6, 7-9, all further
#define FALCON_DEFAULT_ORDERED_RESEND13_TIME  30
#define FALCON_ORDERED_RESEND13_REGNAME  TEXT("SeqResend13Time")

#define FALCON_DEFAULT_ORDERED_RESEND46_TIME  (5 * 60)
#define FALCON_ORDERED_RESEND46_REGNAME  TEXT("SeqResend46Time")

#define FALCON_DEFAULT_ORDERED_RESEND79_TIME  (30 * 60)
#define FALCON_ORDERED_RESEND79_REGNAME  TEXT("SeqResend79Time")

#define FALCON_DEFAULT_ORDERED_RESEND10_TIME  (6 * 60 * 60)
#define FALCON_ORDERED_RESEND10_REGNAME  TEXT("SeqResend10Time")

#define FALCON_DEFAULT_EMERGENCY_HOLE_TIME   (15 * 60)
#define FALCON_EMERGENCY_HOLE_TIME_REGNAME  TEXT("EmergencyHoleTime")

#define FALCON_DEFAULT_EMERGENCY_HOLE_REPEATS  (6)
#define FALCON_EMERGENCY_HOLE_REPEATS_REGNAME  TEXT("EmergencyHoleRepeats")

#define FALCON_DEFAULT_RESEND_RESTRICT_INDEX  4
#define FALCON_RESEND_RESTRICT_INDEX_REGNAME  TEXT("SeqResendRestrictIndex")

#define FALCON_DEFAULT_RESEND_RESTRICT_AMOUNT  500
#define FALCON_RESEND_RESTRICT_AMOUNT_REGNAME  TEXT("SeqResendRestrictAmount")

// Debugging lever: all resend times the same
#define FALCON_DBG_RESEND_REGNAME       TEXT("XactResendTime")

// Min interval between sending identical ordering acks
#define FALCON_DEFAULT_TIME_BETWEEN_ORDER_ACKS  15
#define FALCON_TIME_BETWEEN_ORDER_ACKS_REGNAME  TEXT("SeqMinAckInterval")

// Interval(msec) for log manager to check if the flush/chkpoint is needed
#define FALCON_DEFAULT_LOGMGR_TIMERINTERVAL     10
#define FALCON_LOGMGR_TIMERINTERVAL_REGNAME     TEXT("LogMgrTimerInterval")

// Max interval (msec) for log manager flushes (if there was no other reason to do it before)
#define FALCON_DEFAULT_LOGMGR_FLUSHINTERVAL     50
#define FALCON_LOGMGR_FLUSHINTERVAL_REGNAME     TEXT("LogMgrFlushInterval")

// Max interval (msec) for log manager internal checkpoints (if there was no other reason to do it before)
#define FALCON_DEFAULT_LOGMGR_CHKPTINTERVAL     10000
#define FALCON_LOGMGR_CHKPTINTERVAL_REGNAME     TEXT("LogMgrChkptInterval")

// Log manager buffers number
#define FALCON_DEFAULT_LOGMGR_BUFFERS           400
#define FALCON_LOGMGR_BUFFERS_REGNAME           TEXT("LogMgrBuffers")

// Log manager file size
#define FALCON_DEFAULT_LOGMGR_SIZE              0x600000
#define FALCON_LOGMGR_SIZE_REGNAME              TEXT("LogMgrFileSize")

// Log manager sleep time if not enough append asynch threads
#define FALCON_DEFAULT_LOGMGR_SLEEP_ASYNCH      500
#define FALCON_LOGMGR_SLEEP_ASYNCH_REGNAME      TEXT("LogMgrSleepAsynch")

// Log manager append asynch repeat limit
#define FALCON_DEFAULT_LOGMGR_REPEAT_ASYNCH     100
#define FALCON_LOGMGR_REPEAT_ASYNCH_REGNAME     TEXT("LogMgrRepeatAsynchLimit")

// Falcon interval (msec) for probing log manager flush
#define FALCON_DEFAULT_LOGMGR_PROBE_INTERVAL    100
#define FALCON_LOGMGR_PROBE_INTERVAL_REGNAME    TEXT("LogMgrProbeInterval")

// Resource manager checkpoints period (msec)
#define FALCON_DEFAULT_RM_FLUSH_INTERVAL          1800000
#define FALCON_RM_FLUSH_INTERVAL_REGNAME          TEXT("RMFlushInterval")

// Resource manager client name
#define FALCON_DEFAULT_RM_CLIENT_NAME             TEXT("Falcon")
#define FALCON_RM_CLIENT_NAME_REGNAME            TEXT("RMClientName")

// RT stub RM name
#define FALCON_DEFAULT_STUB_RM_NAME              TEXT("StubRM")
#define FALCON_RM_STUB_NAME_REGNAME             TEXT("RMStubName")

// Transactions persistent file location
#define FALCON_DEFAULT_XACTFILE_PATH          TEXT("MQTrans")
#define FALCON_XACTFILE_PATH_REGNAME            TEXT("StoreXactLogPath")
#define FALCON_XACTFILE_REFER_NAME              TEXT("Transactions")

// Incoming sequences persistent file location
#define FALCON_DEFAULT_INSEQFILE_PATH           TEXT("MQInSeqs")
#define FALCON_INSEQFILE_PATH_REGNAME           TEXT("StoreInSeqLogPath")
#define FALCON_INSEQFILE_REFER_NAME             TEXT("Incoming Sequences")

// Outgoming sequences persistent file location
#define FALCON_DEFAULT_OUTSEQFILE_PATH          TEXT("MQOutSeqs")
#define FALCON_OUTSEQFILE_PATH_REGNAME          TEXT("StoreOutSeqLogPath")
#define FALCON_OUTSEQFILE_REFER_NAME            TEXT("Outgoing Sequences")

// Logger file
#define FALCON_DEFAULT_LOGMGR_PATH              TEXT("QMLog")
#define FALCON_LOGMGR_PATH_REGNAME              TEXT("StoreMqLogPath")

// Default for TIME_TO_REACH_QUEUE (90 days, in seconds).
#define MSMQ_LONG_LIVE_REGNAME        TEXT("MachineCache\\LongLiveTime")
#define MSMQ_DEFAULT_LONG_LIVE       (90 * 24 * 60 * 60)

// Expiration time of entries in the crypto key cache.
#define CRYPT_KEY_CACHE_DEFAULT_EXPIRATION_TIME (60000 * 10) // 10 minutes.
#define CRYPT_KEY_CACHE_EXPIRATION_TIME_REG_NAME TEXT("CryptKeyExpirationTime")

// Cache size for send crypto keys.
#define CRYPT_SEND_KEY_CACHE_DEFAULT_SIZE       53
#define CRYPT_SEND_KEY_CACHE_REG_NAME           TEXT("CryptSendKeyCacheSize")

// Cache size for receive crypto keys.
#define CRYPT_RECEIVE_KEY_CACHE_DEFAULT_SIZE    127
#define CRYPT_RECEIVE_KEY_CACHE_REG_NAME        TEXT("CryptReceiveKeyCacheSize")

// Certificate info cache.
#define CERT_INFO_CACHE_DEFAULT_SIZE            53
#define CERT_INFO_CACHE_SIZE_REG_NAME           TEXT("CertInfoCacheSize")

// QM public key cache.
#define QM_PB_KEY_CACHE_DEFAULT_SIZE            53
#define QM_PB_KEY_CACHE_SIZE_REG_NAME           TEXT("QmPbKeyCacheSize")

// Domain Controllers cache.
#define DC_CACHE_DEFAULT_EXPIRATION_TIME        (60000 * 60 * 3) // 3 hours
#define DC_CACHE_EXPIRATION_TIME_REG_NAME       TEXT("DcCacheExpirationTime")
#define DC_CACHE_SIZE_DEFAULT_SIZE              17
#define DC_CACHE_SIZE_REG_NAME                  TEXT("DcCacheSize")

// User group info cache.
#define USER_CACHE_DEFAULT_EXPIRATION_TIME      (60000 * 30 * 3) // 30 minutes.
#define USER_CACHE_EXPIRATION_TIME_REG_NAME     TEXT("UserCacheExpirationTime")
#define USER_CACHE_SIZE_DEFAULT_SIZE            253
#define USER_CACHE_SIZE_REG_NAME                TEXT("UserCacheSize")

//---------------------------------------------------------
// Definition for private system queues
//---------------------------------------------------------

#define MSMQ_MAX_PRIV_SYSQUEUE_REGNAME   TEXT("MaxSysQueue")
#define MSMQ_PRIV_SYSQUEUE_PRIO_REGNAME  TEXT("SysQueuePriority")
//
// the default for private system queue priority is defined in
// mqprops.h:
// #define DEFAULT_SYS_Q_BASEPRIORITY  0x7fff
//

//---------------------------------------------------------
//  Multiple QM
//---------------------------------------------------------

// machine name
#define FALCON_MACHINE_NAME_REGNAME  TEXT("MachineName")

// end ports file name
#define FALCON_PORTS_FILE_NAME_REGNAME TEXT("EndPortsFileName")
#define MQIS_ENDPORTS_SECTION   TEXT("MQIS")

//---------------------------------------------------------
//  Wolfpack support
//---------------------------------------------------------

// cluster name
#define FALCON_CLUSTER_NAME_REGNAME  TEXT("ClusterName")

//---------------------------------------------------------
//  Definition for threads pool used in remote read.
//---------------------------------------------------------

// Maimum number of threads.
#define FALCON_DEFUALT_MAX_RRTHREADS_WKS     16
#define FALCON_DEFUALT_MAX_RRTHREADS_SRV     64
#define FALCON_MAX_RRTHREADS_REGNAME         TEXT("MaxRRThreads")

//---------------------------------------------------------
//  Definition for licensing
//---------------------------------------------------------

// maximum number of connections per server (limitted server on NTS).
#define DEFAULT_FALCON_SERVER_MAX_CLIENTS  25

// number of allowed sessions for clients.
#define DEFAULT_FALCON_MAX_SESSIONS_WKS    10

//----------------------------------------------------------
//  Definition for RPC cancel
//----------------------------------------------------------

#define FALCON_DEFAULT_RPC_CANCEL_TIMEOUT       ( 5 )	// 5 minutes
#define FALCON_RPC_CANCEL_TIMEOUT_REGNAME       TEXT("RpcCancelTimeout")

//----------------------------------------------------------
//  General definitions shared among setup and QM
//----------------------------------------------------------

#define MQ_SETUP_CN GUID_NULL

//---------------------------------------------------------
//  General definition for controling QM operation
//---------------------------------------------------------
#define FALCON_WAIT_TIMEOUT_REGNAME     TEXT("WaitTime")
#define FALCON_USING_PING_REGNAME       TEXT("UsePing")
#define FALCON_QM_THREAD_NO_REGNAME     TEXT("QMThreadNo")

//---------------------------------------------------------
//
//  Registry used for server authentication.
//
//---------------------------------------------------------

// Use server authentication when communicating via RPC
// with the parent server (BSC->PSC, PSC->PEC)
#define DEFAULT_SRVAUTH_WITH_PARENT             1
#define SRVAUTH_WITH_PARENT_REG_NAME            TEXT("UseServerAuthWithParentDs")

//
// Crypto Store where server certificate is placed.
//
#define SRVAUTHN_STORE_NAME_REGNAME    TEXT("security\\ServerCertStore")
//
// Digest (16 bytes) of server certificate.
//
#define SRVAUTHN_CERT_DIGEST_REGNAME   TEXT("security\\ServerCertDigest")

//
// The followings are credentials of a user that is used for impersonation
// before querying the DS. With NT5 DS, there is a problem when client
// MSMQ service call a server for querying the DS. The server impersonate
// the client. If client is LocalSystem  service then server impersonate it
// as anonymous and with that impersonation is can't query the DS.
// So the solution is as follow:
// 1. to query the DS without impersonation (i.e., if impersonating to
//    anonymous then revert to local credentials. This mean everyone must
//    have "get" permissions.
// 2. Administrator will give credentials of a special user and MSMQ will
//    log on that user and impersonate it (when it finds that impersonation
//    of an incoming RPC call result in anonymous. Credentials are kept
//    in registry, in the following values.
//
#define  DSQUERY_USERNAME_REGNAME      TEXT("security\\DSQueryUserName")
#define  DSQUERY_USERDOMAIN_REGNAME    TEXT("security\\DSQueryUserDomain")
#define  DSQUERY_USERPASSWORD_REGNAME  TEXT("security\\DSQueryUserPassword")

//---------------------------------------------------------
//
//  Registry used by the NT5 replication service.
//
//---------------------------------------------------------

// Interval for replication, NT5 to NT4.  in seconds
#define RP_DEFAULT_REPLICATION_INTERVAL   (15 * 60)
#define RP_REPLICATION_INTERVAL_REGNAME   \
                                     TEXT("Migration\\ReplicationInterval")

// Interval to next replication cycle, if present one failed. in Seconds.
#define RP_DEFAULT_FAIL_REPL_INTERVAL   (120)
#define RP_FAIL_REPL_INTERVAL_REGNAME   TEXT("Migration\\FailReplInterval")

// Timeout of replication messages. in second.
#define RP_DEFAULT_REPL_MSG_TIMEOUT        (20 * 60)
#define RP_REPL_MSG_TIMEOUT_REGNAME        TEXT("Migration\\ReplMsgTimeout")

// My site id in NT4.
#define MSMQ_NT4_MASTERID_REGNAME  TEXT("Migration\\MasterIdOnNt4")

// Number of threads for answering replication/sync messages from NT4 servers.
#define RP_DEFAULT_REPL_NUM_THREADS        4
#define RP_REPL_NUM_THREADS_REGNAME        TEXT("Migration\\ReplThreads")

// If "ON_DEMAND" is 1, then replication is done on demand when
// "REPLICATE_NOW" is 1. The service read the "_NOW" flag each second.
#define RP_REPL_ON_DEMAND_REGNAME        TEXT("Migration\\ReplOnDemand")
#define RP_REPLICATE_NOW_REGNAME         TEXT("Migration\\ReplicateNow")

#endif  // __TEMP_MQINI_H

