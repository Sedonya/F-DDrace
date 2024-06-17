/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef ENGINE_SERVER_SERVER_H
#define ENGINE_SERVER_SERVER_H

#include <base/hash.h>
#include <engine/server.h>
#include <engine/shared/memheap.h>

#include <engine/masterserver.h>
#include <engine/shared/demo.h>
#include <engine/shared/econ.h>
#include <engine/shared/mapchecker.h>
#include <engine/shared/netban.h>
#include "register.h"
#include <engine/shared/fifo.h>

#include "antibot.h"
#include "authmanager.h"
#include <engine/engine.h>
#include <list>
#include <vector>
#include <string>

#if defined (CONF_SQL)
	#include "sql_connector.h"
	#include "sql_server.h"
#endif

class CSnapIDPool
{
	enum
	{
		MAX_IDS = 32*1024,
	};

	class CID
	{
	public:
		short m_Next;
		short m_State; // 0 = free, 1 = allocated, 2 = timed
		int m_Timeout;
	};

	CID m_aIDs[MAX_IDS];

	int m_FirstFree;
	int m_FirstTimed;
	int m_LastTimed;
	int m_Usage;
	int m_InUsage;

public:

	CSnapIDPool();

	void Reset();
	void RemoveFirstTimeout();
	int NewID();
	void TimeoutIDs();
	void FreeID(int ID);
};


class CServerBan : public CNetBan
{
	class CServer *m_pServer;

	template<class T> int BanExt(T *pBanPool, const typename T::CDataType *pData, int Seconds, const char *pReason);

public:
	class CServer *Server() const { return m_pServer; }

	void InitServerBan(class IConsole *pConsole, class IStorage *pStorage, class CServer* pServer);

	virtual int BanAddr(const NETADDR *pAddr, int Seconds, const char *pReason);
	virtual int BanRange(const CNetRange *pRange, int Seconds, const char *pReason);

	static void ConBanExt(class IConsole::IResult *pResult, void *pUser);
};


class CServer : public IServer
{
	class IGameServer *m_pGameServer;
	class CConfig *m_pConfig;
	class IConsole *m_pConsole;
	class IStorage *m_pStorage;
	class IEngineAntibot *m_pAntibot;
	class IRegister *m_pRegister;
	class IRegister *m_pRegisterTwo;

#if defined(CONF_FAMILY_UNIX)
	UNIXSOCKETADDR m_ConnLoggingDestAddr;
	bool m_ConnLoggingSocketCreated;
	UNIXSOCKET m_ConnLoggingSocket;
#endif

#if defined(CONF_SQL)
	lock m_GlobalSqlLock;

	CSqlServer *m_apSqlReadServers[MAX_SQLSERVERS];
	CSqlServer *m_apSqlWriteServers[MAX_SQLSERVERS];
#endif
public:
	class IGameServer *GameServer() { return m_pGameServer; }
	class CConfig *Config() { return m_pConfig; }
	class IConsole *Console() { return m_pConsole; }
	class IStorage *Storage() { return m_pStorage; }
	class IEngineAntibot *Antibot() { return m_pAntibot; }

	enum
	{
		MAX_RCONCMD_SEND=16,
		MAX_MAPLISTENTRY_SEND = 32,
		MIN_MAPLIST_CLIENTVERSION=0x0703,	// todo 0.8: remove me
		MAX_RCONCMD_RATIO=8,
	};

	struct CMapListEntry;

	class CClient
	{
	public:

		enum
		{
			STATE_EMPTY = 0,
			STATE_PREAUTH,
			STATE_AUTH,
			STATE_FAKE_MAP,
			STATE_CONNECTING,
			STATE_CONNECTING_AS_SPEC,
			STATE_READY,
			STATE_INGAME,
			STATE_DUMMY,

			SNAPRATE_INIT=0,
			SNAPRATE_FULL,
			SNAPRATE_RECOVER,

			DNSBL_STATE_NONE = 0,
			DNSBL_STATE_PENDING,
			DNSBL_STATE_BLACKLISTED,
			DNSBL_STATE_WHITELISTED,

			PGSC_STATE_NONE = 0,
			PGSC_STATE_PENDING,
			PGSC_STATE_DONE,
		};

		class CInput
		{
		public:
			int m_aData[MAX_INPUT_SIZE];
			int m_GameTick; // the tick that was chosen for the input
			bool m_HammerflyMarked;
		};

		// connection state info
		int m_State;
		int m_Latency;
		int m_SnapRate;

		int m_LastAckedSnapshot;
		int m_LastInputTick;
		CSnapshotStorage m_Snapshots;

		CInput m_LatestInput;
		CInput m_aInputs[200]; // TODO: handle input better
		int m_CurrentInput;

		char m_aName[MAX_NAME_LENGTH];
		char m_aClan[MAX_CLAN_LENGTH];
		int m_Version;
		int m_Country;
		int m_Score;
		int m_Authed;
		int m_AuthKey;
		int m_AuthTries;

		int m_MapChunk;
		bool m_NoRconNote;
		bool m_Quitting;
		const IConsole::CCommandInfo *m_pRconCmdToSend;
		const CMapListEntry *m_pMapListEntryToSend;

		void Reset();
		void ResetContent();

		// DDrace
		bool m_ShowIps;

		float m_Traffic;
		int64 m_TrafficSince;

		bool m_Sevendown;
		int m_Socket;

		// dummy
		bool m_IdleDummy;
		int m_LastIntendedTick;
		bool m_DummyHammer;
		bool m_Main;

		bool m_HammerflyMarked;
		int m_LastFire;

		int m_aIdleDummyTrack[5];
		int m_CurrentIdleTrackPos;

		// design
		bool m_DesignChange;
		int m_CurrentMapDesign;

		bool m_Rejoining;

		char m_aLanguage[5]; // would be 2, but "none" is 4

		class CDnsblLookup : public IJob
		{
			void Run() override;
		public:
			CDnsblLookup() {};
			CDnsblLookup(const char *pCmd)
			{
				str_copy(m_aCommand, pCmd, sizeof(m_aCommand));
				m_Result = 0;
			}

			int m_Result;
			char m_aCommand[512];
		};
		int m_DnsblState;
		std::shared_ptr<CDnsblLookup> m_pDnsblLookup;

		class CPgscLookup : public IJob
		{
			void Run() override;
		public:
			CPgscLookup() {};
			CPgscLookup(const NETADDR *pAddr, const char *pFindString)
			{
				net_addr_str(pAddr, m_aAddress, sizeof(m_aAddress), false);
				str_copy(m_aFindString, pFindString, sizeof(m_aFindString));
				m_Result = 0;
			}

			int m_Result;
			char m_aAddress[NETADDR_MAXSTRSIZE];
			char m_aFindString[128];
		};
		int m_PgscState; // Proxy Game Server Check
		std::shared_ptr<CPgscLookup> m_pPgscLookup;

		int m_aIdMap[VANILLA_MAX_CLIENTS];
		int m_aReverseIdMap[MAX_CLIENTS];

		bool m_GotDDNetVersionPacket;
		bool m_DDNetVersionSettled;
		int m_DDNetVersion;
		char m_aDDNetVersionStr[64];
		CUuid m_ConnectionID;
	};

	CClient m_aClients[MAX_CLIENTS];

	CSnapshotDelta m_SnapshotDelta;
	CSnapshotBuilder m_SnapshotBuilder;
	CSnapIDPool m_IDPool;
	CNetServer m_NetServer;
	CEcon m_Econ;
#if defined(CONF_FAMILY_UNIX)
	CFifo m_Fifo;
#endif
	CServerBan m_ServerBan;

	IEngineMap *m_pMap;

	int64 m_GameStartTime;
	enum
	{
		UNINITIALIZED = 0,
		RUNNING = 1,
		STOPPING = 2
	};
	int m_RunServer;
	bool m_MapReload;
	int m_RconClientID;
	int m_RconAuthLevel;
	int m_PrintCBIndex;

	int m_RconRestrict;

	// map
	enum
	{
		MAP_CHUNK_SIZE=NET_MAX_PAYLOAD-NET_MAX_CHUNKHEADERSIZE-4, // msg type
	};
	char m_aCurrentMap[128];
	SHA256_DIGEST m_CurrentMapSha256;
	unsigned m_CurrentMapCrc;
	unsigned char *m_pCurrentMapData;
	unsigned int m_CurrentMapSize;
	int m_MapChunksPerRequest;

	// fake map
	unsigned int m_FakeMapCrc;
	unsigned char *m_pFakeMapData;
	unsigned int m_FakeMapSize;
	void LoadUpdateFakeMap();

	// map designs
	enum
	{
		NUM_MAP_DESIGNS = 8
	};
	struct MapDesign
	{
		char m_aName[128];
		SHA256_DIGEST m_Sha256;
		unsigned int m_Crc;
		unsigned char *m_pData;
		unsigned int m_Size;
	} m_aMapDesign[NUM_MAP_DESIGNS];
	std::vector<std::string> m_vMapDesignFiles;
	void LoadMapDesigns();
	static int InitMapDesign(const char *pName, int IsDir, int StorageType, void *pUser);
	virtual void ChangeMapDesign(int ClientID, const char *pName);
	void SendMapDesign(int ClientID, int Design);
	virtual const char *GetMapDesign(int ClientID);

	// map https url
	const char *GetHttpsMapURL(int Design = -1);

	//maplist
	struct CMapListEntry
	{
		CMapListEntry *m_pPrev;
		CMapListEntry *m_pNext;
		char m_aName[IConsole::TEMPMAP_NAME_LENGTH];
	};

	struct CSubdirCallbackUserdata
	{
		CServer *m_pServer;
		char m_aName[IConsole::TEMPMAP_NAME_LENGTH];
	};

	CHeap *m_pMapListHeap;
	CMapListEntry *m_pLastMapEntry;
	CMapListEntry *m_pFirstMapEntry;
	int m_NumMapEntries;

	int m_RconPasswordSet;
	int m_GeneratedRconPassword;

	CDemoRecorder m_DemoRecorder;
	CMapChecker m_MapChecker;
	CAuthManager m_AuthManager;

	CServer();
	~CServer();

	int TrySetClientName(int ClientID, const char* pName);

	virtual void SetClientName(int ClientID, const char *pName);
	virtual void SetClientClan(int ClientID, char const *pClan);
	virtual void SetClientCountry(int ClientID, int Country);
	virtual void SetClientScore(int ClientID, int Score);

	int Kick(int ClientID, const char *pReason);
	void Ban(int ClientID, int Seconds, const char *pReason); // bans ip of player with clientid

	void DemoRecorder_HandleAutoStart();
	bool DemoRecorder_IsRecording();

	int64 TickStartTime(int Tick);

	int Init();

	void InitRconPasswordIfUnset();

	void SetRconCID(int ClientID);
	int GetAuthedState(int ClientID) const;
	const char *AuthName(int ClientID) const;
	bool IsBanned(int ClientID);
	void GetMapInfo(char *pMapName, int MapNameSize, int *pMapSize, SHA256_DIGEST *pMapSha256, int *pMapCrc);
	int GetClientInfo(int ClientID, CClientInfo *pInfo) const;
	void GetClientAddr(int ClientID, char *pAddrStr, int Size, bool AddPort = false) const;
	void SetClientDDNetVersion(int ClientID, int DDNetVersion);
	int GetClientVersion(int ClientID) const;
	const char *ClientName(int ClientID) const;
	const char *ClientClan(int ClientID) const;
	int ClientCountry(int ClientID) const;
	bool ClientIngame(int ClientID) const;

	virtual int SendMsg(CMsgPacker *pMsg, int Flags, int ClientID);

	void DoSnapshot();

	static int NewClientCallback(int ClientID, bool Sevendown, int Socket, void *pUser);
	static int DelClientCallback(int ClientID, const char *pReason, void *pUser);
	static int ClientRejoinCallback(int ClientID, bool Sevendown, int Socket, void *pUser);

	// returns whether client can close the connection or not right now, e.g. when in design change or the dummy, so that the client doesnt close the connection so they can rejoin
	static bool ClientCanCloseCallback(int ClientID, void *pUser);

	void SendRconType(int ClientID, bool UsernameReq);
	void SendCapabilities(int ClientID);
	void SendMapData(int ClientID, int Chunk, bool FakeMap);
	void SendMap(int ClientID);
	void SendFakeMap(int ClientID);
	void SendConnectionReady(int ClientID);
	void SendRconLine(int ClientID, const char *pLine);
	static void SendRconLineAuthed(const char *pLine, void *pUser, bool Highlighted);

	void SendRconCmdAdd(const IConsole::CCommandInfo *pCommandInfo, int ClientID);
	void SendRconCmdRem(const IConsole::CCommandInfo *pCommandInfo, int ClientID);
	void UpdateClientRconCommands();
	void SendMapListEntryAdd(const CMapListEntry *pMapListEntry, int ClientID);
	void SendMapListEntryRem(const CMapListEntry *pMapListEntry, int ClientID);
	void UpdateClientMapListEntries();

	void ProcessClientPacket(CNetChunk *pPacket);

	bool m_ServerInfoNeedsUpdate;
	void SendServerInfo(int ClientID);
	void GenerateServerInfo(CPacker *pPacker, int Token, int Socket);
	void SendServerInfoSevendown(const NETADDR *pAddr, int Token, int Socket);
	void UpdateRegisterServerInfo();
	void UpdateServerInfo(bool Resend = false);
	virtual void ExpireServerInfo();
	void FillAntibot(CAntibotRoundData *pData) override;
	const char *GetGameTypeServerInfo();

	void PumpNetwork();

	virtual void ChangeMap(const char *pMap);
	virtual const char *GetFileName(char *pPath);
	virtual const char *GetCurrentMapName();
	virtual const char *GetMapName();
	int LoadMap(const char *pMapName);

	void InitInterfaces(CConfig *pConfig, IConsole *pConsole, IGameServer *pGameServer, IEngineMap *pMap, IStorage *pStorage, IEngineAntibot *pAntibot);
	int Run();

	static int MapListEntryCallback(const char *pFilename, int IsDir, int DirType, void *pUser);

	static void ConEuroMode(IConsole::IResult *pResult, void *pUser);
	static void ConTestingCommands(IConsole::IResult *pResult, void *pUser);
	static void ConRescue(IConsole::IResult *pResult, void *pUser);
	static void ConKick(IConsole::IResult *pResult, void *pUser);
	static void ConStatus(IConsole::IResult *pResult, void *pUser);
	static void ConShutdown(IConsole::IResult *pResult, void *pUser);
	static void ConRecord(IConsole::IResult *pResult, void *pUser);
	static void ConStopRecord(IConsole::IResult *pResult, void *pUser);
	static void ConMapReload(IConsole::IResult *pResult, void *pUser);
	static void ConSaveConfig(IConsole::IResult *pResult, void *pUser);
	static void ConLogout(IConsole::IResult *pResult, void *pUser);

	static void ConchainSpecialInfoupdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainPlayerSlotsUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainMaxclientsUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainMaxclientsperipUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainCommandAccessUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainConsoleOutputLevelUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);

	static void ConchainMapUpdate(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);

	static void ConShowIps(IConsole::IResult* pResult, void* pUser);
	void ConchainRconPasswordChangeGeneric(int Level, const char *pCurrent, IConsole::IResult *pResult);
	static void ConchainRconPasswordChange(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainRconModPasswordChange(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainRconHelperPasswordChange(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
	static void ConchainFakeMapCrc(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);

#if defined(CONF_FAMILY_UNIX)
	static void ConchainConnLoggingServerChange(IConsole::IResult *pResult, void *pUserData, IConsole::FCommandCallback pfnCallback, void *pCallbackUserData);
#endif

	void LogoutClient(int ClientID, const char *pReason);
	void LogoutKey(int Key, const char *pReason);
	void AuthRemoveKey(int KeySlot);
	static void ConAuthAdd(IConsole::IResult *pResult, void *pUser);
	static void ConAuthAddHashed(IConsole::IResult *pResult, void *pUser);
	static void ConAuthUpdate(IConsole::IResult *pResult, void *pUser);
	static void ConAuthUpdateHashed(IConsole::IResult *pResult, void *pUser);
	static void ConAuthRemove(IConsole::IResult *pResult, void *pUser);
	static void ConAuthList(IConsole::IResult *pResult, void *pUser);

	void RegisterCommands();

	virtual int SnapNewID();
	virtual void SnapFreeID(int ID);
	virtual void *SnapNewItem(int Type, int ID, int Size);
	void SnapSetStaticsize(int ItemType, int Size);

	virtual void RestrictRconOutput(int ClientID) { m_RconRestrict = ClientID; }
	virtual void SetRconAuthLevel(int AuthLevel) { m_RconAuthLevel = AuthLevel; }

	// F-DDrace

	const char *GetNetErrorString(int ClientID) { return m_NetServer.ErrorString(ClientID); };
	void ResetNetErrorString(int ClientID) { m_NetServer.ResetErrorString(ClientID); };
	bool SetTimedOut(int ClientID, int OrigID);
	void SetTimeoutProtected(int ClientID) { m_NetServer.SetTimeoutProtected(ClientID); };

	void SendMsgRaw(int ClientID, const void *pData, int Size, int Flags);

	void GetClientAddr(int ClientID, NETADDR* pAddr);
	const char* GetAnnouncementLine(char const* FileName);
	unsigned m_AnnouncementLastLine;

	bool IsBrowserScoreFix();

	class CBotLookup : public IJob
	{
		void Run() override;
	public:
		// TODO: not rly thread safe xd
		CBotLookup(CServer *pServer)
		{
			m_pServer = pServer;
		}
		CServer *m_pServer;
	};
	enum
	{
		BOTLOOKUP_STATE_DONE = 0,
		BOTLOOKUP_STATE_PENDING
	};
	virtual void PrintBotLookup();
	int m_BotLookupState;

	void InitProxyGameServerCheck(int ClientID);
	void InitDnsbl(int ClientID);
	struct
	{
		std::vector<NETADDR> m_vBlacklist;
		std::vector<NETADDR> m_vWhitelist;
	} m_DnsblCache;

	// white list in case iphub.info falsely flagged someone or to whitelist a server ip in case no proxy game server string is set and someone falsely got banned as "proxy game server"
	struct SWhitelist
	{
		NETADDR m_Addr;
		char m_aReason[64];
	};
	std::vector<SWhitelist> m_vWhitelist;
	virtual void SaveWhitelist();
	virtual void AddWhitelist(const NETADDR *pAddr, const char *pReason);
	virtual void RemoveWhitelist(const NETADDR *pAddr);
	virtual void RemoveWhitelistByIndex(unsigned int Index);
	virtual void PrintWhitelist();

	class CWebhook : public IJob
	{
		void Run() override;
	public:
		CWebhook(const char *pCommand) { str_copy(m_aCommand, pCommand, sizeof(m_aCommand)); }
		char m_aCommand[1024];
	};
	virtual void SendWebhookMessage(const char *pURL, const char *pMessage, const char *pUsername = "", const char *pAvatarURL = "");
	virtual void SendWebhook1vs1(const char *pMessage, const char *pUsername, const char *pAvatarURL);

	virtual const char *GetAuthIdent(int ClientID);

	virtual int *GetIdMap(int ClientID);
	virtual int *GetReverseIdMap(int ClientID);

	void DummyJoin(int DummyID);
	void DummyLeave(int DummyID);

	virtual bool IsMain(int ClientID) { return m_aClients[ClientID].m_Main; }
	virtual bool DesignChanging(int ClientID) { return m_aClients[ClientID].m_DesignChange; }
	virtual bool HammerflyMarked(int ClientID) { return m_aClients[ClientID].m_HammerflyMarked; }
	virtual bool IsIdleDummy(int ClientID) { return m_aClients[ClientID].m_IdleDummy; }
	virtual bool IsDummyHammer(int ClientID) { return m_aClients[ClientID].m_DummyHammer; }
	virtual bool IsSevendown(int ClientID) { return m_aClients[ClientID].m_Sevendown; }
	virtual int NumClients();
	bool IsDoubleInfo();

	class CTranslateChat : public IJob
	{
		void Run() override;
	public:
		CTranslateChat(CServer *pServer, int ClientID, int Mode, const char *pMessage, const char *pLanguage)
		{
			m_pServer = pServer;
			m_ClientID = ClientID;
			m_Mode = Mode;
			str_copy(m_aMessage, pMessage, sizeof(m_aMessage));
			str_copy(m_aLanguage, pLanguage, sizeof(m_aLanguage));
		}
		
		CServer *m_pServer;
		int m_ClientID;
		int m_Mode;
		char m_aMessage[256];
		char m_aLanguage[5];
	};
	enum
	{
		TRANSLATE_STATE_DONE = 0,
		TRANSLATE_STATE_PENDING
	};
	virtual void TranslateChat(int ClientID, const char *pMsg, int Mode);
	int m_TranslateState;
	virtual const char *GetLanguage(int ClientID) { return m_aClients[ClientID].m_aLanguage; }
	virtual void SetLanguage(int ClientID, const char *pLanguage) { str_copy(m_aClients[ClientID].m_aLanguage, pLanguage, sizeof(m_aClients[ClientID].m_aLanguage)); }

	const char *GetClientVersionStr(int ClientID) const;

	virtual bool IsUniqueAddress(int ClientID);
	virtual int GetDummy(int ClientID);
	virtual bool IsDummy(int ClientID1, int ClientID2);
	virtual bool DummyControlOrCopyMoves(int ClientID);

#ifdef CONF_FAMILY_UNIX
	enum CONN_LOGGING_CMD
	{
		OPEN_SESSION = 1,
		CLOSE_SESSION = 2,
	};

	void SendConnLoggingCommand(CONN_LOGGING_CMD Cmd, const NETADDR *pAddr);
#endif

#if defined (CONF_SQL)
	// console commands for sqlmasters
	static void ConAddSqlServer(IConsole::IResult *pResult, void *pUserData);
	static void ConDumpSqlServers(IConsole::IResult *pResult, void *pUserData);

	static void CreateTablesThread(void *pData);
#endif
};

#endif
