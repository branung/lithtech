#ifndef __LT1DRIVER_H__
#define __LT1DRIVER_H__

// LithTech 1.0's internet driver, as a Jupiter CBaseDriver: the client half of LT1's UDP protocol,
// so the port's client can join a retail Shogo or Blood 2 server.
// Jupiter keeps reliability inside its drivers, so this one owns LT1's whole wire (lt1wire.h) and meets CNetMgr
// only at whole messages. CNetMgr::AddDriver("lt1") makes it, and only the client's STARTGAME_CLIENTTCP asks for it,
// under the LT1Net engine variable.
// The join, the object updates, the sounds and the instant special effects are translated, and the client's commands
// go out in LT1's update packet. Anything else the server sends is dropped

#ifndef __NETMGR_H__
#include "netmgr.h"
#endif

#include <stdint.h>
#include <deque>
#include <string>
#include <map>
#include <vector>

#include "lt1wire.h"

// The client's input for LT1's update packet (0x25): the commands held this frame as ids, at most nMax,
// and the update rate. CClientMgr sets it when it connects through this driver
typedef uint32 (*LT1ClientUpdateFn)(uint8 *pCommands, uint32 nMax, uint8 *pRate);
extern LT1ClientUpdateFn g_pfnLT1ClientUpdate;

class CLT1Driver : public CBaseDriver
{
public:
						CLT1Driver();
	virtual				~CLT1Driver();

	virtual bool		Init();
	virtual void		Term();
	virtual void		Update();

	virtual LTRESULT	ConnectTCP(const char *sAddress);
	virtual void		Disconnect(CBaseConn *id, EDisconnectReason reason);

	virtual bool		SendPacket(const CPacket_Read &cPacket, CBaseConn *idSendTo, bool bGuaranteed);
	virtual bool		GetPacket(CPacket_Read *pPacket, CBaseConn **pSender);

private:
	typedef std::vector<uint8> Bytes;

	void				Receive();
	void				OnDatagram(const uint8 *pData, uint32 nLen);
	void				OnMessage(const Bytes &cMsg);
	void				SendRaw(const uint8 *pData, uint32 nLen);
	void				SendLT1(uint8 nID, const Bytes &cPayload, bool bGuaranteed);
	void				CloseSocket();
	void				Deliver(uint8 nJupiterID, const uint8 *pData, uint32 nLen);
	void				TranslateUpdate(const uint8 *pBody, uint32 nBody);
	void				SendClientUpdate();
	void				TranslateUnguaranteed(const uint8 *pBody, uint32 nBody);

	intptr_t			m_Socket;		// A SOCKET (pointer sized on Win64), -1 for none
	CLT1Conn			*m_pConn;
	bool				m_bWinsock;
	std::deque<CPacket_Read>	m_Incoming;	// Translated, for GetPacket

	// What the update translation needs to remember about each object: its type, for the per type fields,
	// and its LT1 flags, whose FLAG_FULLPOSITIONRES decides the position's encoding
	struct LT1Obj { uint8 m_nType; uint32 m_nFlags; LT1Obj() : m_nType(0), m_nFlags(0) {} };
	std::map<uint16, LT1Obj>	m_Objects;
	// LT1's per file sound flags, which the server sends only on change
	std::map<uint16, uint16>	m_SoundFlags;

	// The last update packet sent (SendClientUpdate)
	Bytes				m_LastUpdate;
	uint32				m_nLastUpdateTime;
};

#endif // __LT1DRIVER_H__
