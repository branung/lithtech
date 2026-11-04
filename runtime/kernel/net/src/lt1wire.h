// LithTech 1.0's wire, for CLT1Driver.
// The driver packets (full id 0 and a TCPSUB_ subtype) and the reliable layer (a frame dword on every
// guaranteed packet, acks on full id 0x41, layer packets on id 1) are LT1's, as retail Shogo 27.1 speaks them.
// The reliable layer lives on the connection

#ifndef __LT1WIRE_H__
#define __LT1WIRE_H__

#ifndef __NETMGR_H__
#include "netmgr.h"
#endif

#ifdef _WIN32
	#ifndef _WINSOCKAPI_
	#include <winsock.h>
	#endif
	typedef int lt1_socklen;
	inline void lt1_SetNonBlocking(SOCKET s) { u_long nArg = 1; ioctlsocket(s, FIONBIO, &nArg); }
	#define LT1_CLOSESOCKET closesocket
#else
	#include <sys/types.h>
	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <arpa/inet.h>
	#include <netdb.h>
	#include <fcntl.h>
	#include <unistd.h>
	typedef int SOCKET;
	typedef socklen_t lt1_socklen;
	inline void lt1_SetNonBlocking(SOCKET s) { fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) | O_NONBLOCK); }
	#define LT1_CLOSESOCKET close
#endif

#include <stdint.h>
#include "ltbasedefs.h"
#include <map>
#include <vector>

// Driver packets: full id 0 and a subtype
#define TCPSUB_DISCONNECT		0
#define TCPSUB_QUERY			1
#define TCPSUB_RESPONSE			2
#define TCPSUB_CONNECT			3
#define TCPSUB_CONNECTED		5
#define TCPSUB_FULL				6
#define TCPSUB_WRONGGUID		7

#define LT1_GUARANTEED			0x80
#define LT1_DEFAULT_PORT		27888

// How often an ack goes out, owed or not, as LT1's ACKSEND_TIMEOUT.
// The ack carries the frames sent, so a peer learns of a frame lost off the end of a burst from the next one,
// as LT1 has no resend timer. A ping rides on an ack every 0 to 3 s, at random
#define LT1_ACK_DELAY_MS		333
#define LT1_PING_MAX_MS			3000

typedef std::vector<uint8> LT1Bytes;

inline uint16 LT1Word(const uint8 *p) { return (uint16)(p[0] | (p[1] << 8)); }
inline uint32 LT1DWord(const uint8 *p) { return (uint32)p[0] | ((uint32)p[1] << 8) | ((uint32)p[2] << 16) | ((uint32)p[3] << 24); }
inline void LT1PutWord(LT1Bytes &c, uint16 v) { c.push_back((uint8)v); c.push_back((uint8)(v >> 8)); }
inline void LT1PutDWord(LT1Bytes &c, uint32 v) { LT1PutWord(c, (uint16)v); LT1PutWord(c, (uint16)(v >> 16)); }

// LT1's compressed rotation: three bytes, or six when the first is non-negative
bool LT1DecodeRotation(const signed char b[6], LTRotation &rot);

// One connection: its peer's address and its reliable layer state
class CLT1Conn : public CBaseConn
{
public:
	CLT1Conn();
	virtual ~CLT1Conn() {}

	virtual float GetPing() { return m_fPing; }
	virtual bool GetIPAddress(uint8 pAddr[4], uint16 *pPort);

	// A datagram straight to the peer
	void			SendRaw(intptr_t nSocket, const uint8 *pData, uint32 nLen);
	// An LT1 packet: the id byte, the payload, and for a guaranteed one the frame number,
	// kept until the peer acks it
	void			SendLT1(intptr_t nSocket, uint8 nID, const LT1Bytes &cPayload, bool bGuaranteed);
	// Full id 0x41
	void			SendAck(intptr_t nSocket);
	// Sends an ack every LT1_ACK_DELAY_MS, and at once when one is forced
	void			UpdateAck(intptr_t nSocket);
	// One received datagram: the messages it delivers, in order, are appended to cOut.
	// Layer packets (id 1) are answered here
	void			OnDatagram(intptr_t nSocket, const uint8 *pData, uint32 nLen, std::vector<LT1Bytes> &cOut);

	// A guaranteed frame until it's acked, and how often the peer has asked for it again.
	// LT1 resends on every other request
	struct Outgoing
	{
		LT1Bytes	m_cData;
		uint32		m_nRequests;
	};

	sockaddr_in		m_Addr;
	uint32			m_nSent;			// Guaranteed frames sent
	uint32			m_nNextRecv;		// Next frame to deliver
	uint32			m_nPeerSent;		// Frames the peer has sent
	uint32			m_nLastAck;			// time_GetMSTime of the last ack sent
	bool			m_bAckForced;		// An early frame, so ack now
	bool			m_bForcedDisconnect;	// The peer sent layer sub 1
	float			m_fPing;
	uint32			m_nNextPing;		// time_GetMSTime the next ping is due
	uint32			m_nPingSent;
	uint16			m_nPingID;
	uint32			m_nLastHeard;		// time_GetMSTime of the last datagram
	std::map<uint32, LT1Bytes>	m_Early;	// Frames ahead of order
	std::map<uint32, Outgoing>	m_Outgoing;	// Frames until acked

private:
	void			OnLayerPacket(intptr_t nSocket, const uint8 *pData, uint32 nLen, std::vector<LT1Bytes> &cOut);
};

#endif // __LT1WIRE_H__
