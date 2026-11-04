// LithTech 1.0's internet driver, client half: see lt1driver.h.
// The wire is LT1's, as retail Shogo 27.1 and Blood 2 26.8 servers speak it

#include "bdefs.h"
#include "lt1driver.h"
#include "systimer.h"
#include "packetdefs.h"
#include "ftbase.h"
#include "ltmessage.h"
#include "misctools.h"
#include "ltbasedefs.h"

#include "lt1wire.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

LT1ClientUpdateFn g_pfnLT1ClientUpdate = LTNULL;

// Server to client ids in LT1
#define LT1_SMSG_NETPROTOCOLVERSION	4
#define LT1_SMSG_UNLOADWORLD	5
#define LT1_SMSG_LOADWORLD		6
#define LT1_SMSG_CLIENTOBJECTID	7
#define LT1_SMSG_YOURID			12
#define LT1_SMSG_MESSAGE		13
#define LT1_SMSG_PACKETGROUP	14
#define LT1_SMSG_CHANGEOBJECTFILENAMES	15
#define LT1_SMSG_CONSOLEVAR		16
#define LT1_SMSG_SKYDEF			17
#define LT1_SMSG_INSTANTSPECIALEFFECT	18
#define LT1_SMSG_PRELOADLIST	21
#define LT1_STC_FILEDESC		50
#define LT1_NETPROTOCOL_VERSION	5
#define LT1_SMSG_UPDATE			8
#define LT1_SMSG_UPDATE_C		9
#define LT1_SMSG_UNGUARANTEEDUPDATE	10

// Jupiter's model instance bitmask widths (model.h): MAX_PIECES_PER_MODEL 64 and MAX_HIDEABLE_MODEL_NODES 128,
// read as dwords after the attachments
#define JUPITER_HIDDENPIECE_DWORDS	2
#define JUPITER_HIDDENNODE_DWORDS	4

// Client to server ids in LT1
#define LT1_CMSG_HELLO			0x23
#define LT1_CMSG_GOODBYE		0x24
#define LT1_CMSG_UPDATE			0x25
#define LT1_CMSG_CONNECTSTAGE	0x27
#define LT1_CMSG_GAMEMESSAGE	0x29
#define LT1_CTS_FILESTATUS		54


CLT1Driver::CLT1Driver() :
	m_Socket(-1),
	m_pConn(LTNULL),
	m_bWinsock(false),
	m_nLastUpdateTime(0)
{
	strcpy(m_Name, "lt1");
	m_DriverFlags = NETDRIVER_TCPIP;
}

CLT1Driver::~CLT1Driver()
{
	Term();
}

bool CLT1Driver::Init()
{
#ifdef _WIN32
	WSADATA cData;
	m_bWinsock = (WSAStartup(MAKEWORD(1, 1), &cData) == 0);
	return m_bWinsock;
#else
	return true;
#endif
}

void CLT1Driver::Term()
{
	if (m_pConn)
		Disconnect(m_pConn, DISCONNECTREASON_SHUTDOWN);
	CloseSocket();
#ifdef _WIN32
	if (m_bWinsock)
		WSACleanup();
#endif
	m_bWinsock = false;
}

void CLT1Driver::CloseSocket()
{
	if (m_Socket != -1)
	{
		LT1_CLOSESOCKET((SOCKET)m_Socket);
		m_Socket = -1;
	}
}

void CLT1Driver::SendRaw(const uint8 *pData, uint32 nLen)
{
	if (m_pConn)
		m_pConn->SendRaw(m_Socket, pData, nLen);
}

void CLT1Driver::SendLT1(uint8 nID, const Bytes &cPayload, bool bGuaranteed)
{
	if (m_pConn)
		m_pConn->SendLT1(m_Socket, nID, cPayload, bGuaranteed);
}

LTRESULT CLT1Driver::ConnectTCP(const char *sAddress)
{
	// "host[:port]"
	char sHost[256];
	uint16 nPort = LT1_DEFAULT_PORT;
	strncpy(sHost, sAddress ? sAddress : "", sizeof(sHost) - 1);
	sHost[sizeof(sHost) - 1] = 0;
	char *pColon = strchr(sHost, ':');
	if (pColon)
	{
		*pColon = 0;
		nPort = (uint16)atoi(pColon + 1);
	}

	sockaddr_in cAddr;
	memset(&cAddr, 0, sizeof(cAddr));
	cAddr.sin_family = AF_INET;
	cAddr.sin_port = htons(nPort);
	cAddr.sin_addr.s_addr = inet_addr(sHost);
	if (cAddr.sin_addr.s_addr == INADDR_NONE)
	{
		hostent *pHost = gethostbyname(sHost);
		if (!pHost)
			return LT_CANTBINDTOPORT;
		memcpy(&cAddr.sin_addr, pHost->h_addr_list[0], 4);
	}

	CloseSocket();
	SOCKET s = socket(AF_INET, SOCK_DGRAM, 0);
	if ((intptr_t)s == -1)
		return LT_CANTBINDTOPORT;
	sockaddr_in cLocal;
	memset(&cLocal, 0, sizeof(cLocal));
	cLocal.sin_family = AF_INET;
	if (bind(s, (sockaddr*)&cLocal, sizeof(cLocal)) != 0)
	{
		LT1_CLOSESOCKET(s);
		return LT_CANTBINDTOPORT;
	}
	lt1_SetNonBlocking(s);
	m_Socket = (intptr_t)s;

	// The connect request: version 0 and the app GUID, resent every 300 ms until the host answers
	// or ten seconds pass, as LT1's join does
	uint8 aRequest[19];
	aRequest[0] = 0;
	aRequest[1] = TCPSUB_CONNECT;
	aRequest[2] = 0;
	memcpy(&aRequest[3], m_pNetMgr->GetAppGuid(), 16);

	uint32 nStart = time_GetMSTime();
	uint32 nLastSend = nStart - 300;
	while (time_GetMSTime() - nStart < 10000)
	{
		if (time_GetMSTime() - nLastSend >= 300)
		{
			sendto(s, (const char*)aRequest, sizeof(aRequest), 0, (sockaddr*)&cAddr, sizeof(cAddr));
			nLastSend = time_GetMSTime();
		}

		uint8 aBuf[2048];
		sockaddr_in cFrom;
		lt1_socklen nFromLen = sizeof(cFrom);
		int nRead = recvfrom(s, (char*)aBuf, sizeof(aBuf), 0, (sockaddr*)&cFrom, &nFromLen);
		if (nRead >= 2 && aBuf[0] == 0)
		{
			if (aBuf[1] == TCPSUB_CONNECTED)
			{
				m_pConn = new CLT1Conn;
				m_pConn->m_pDriver = this;
				m_LastUpdate.clear();
				m_pConn->m_Addr = cFrom;
				m_pNetMgr->NewConnectionNotify(m_pConn);
				return LT_OK;
			}
			if (aBuf[1] == TCPSUB_FULL || aBuf[1] == TCPSUB_WRONGGUID)
			{
				CloseSocket();
				return (aBuf[1] == TCPSUB_FULL) ? LT_REJECTED : LT_NOTSAMEGUID;
			}
		}
		// Anything else that beats the answer is dropped, as LT1's join loop does
	}

	CloseSocket();
	return LT_TIMEOUT;
}

void CLT1Driver::Disconnect(CBaseConn *id, EDisconnectReason reason)
{
	if (!m_pConn || id != m_pConn)
		return;

	// LT1 sends its disconnect four times, as nothing acknowledges it
	uint8 aBye[3] = { 0, TCPSUB_DISCONNECT, 0 };
	for (int i = 0; i < 4; i++)
		SendRaw(aBye, sizeof(aBye));

	CLT1Conn *pConn = m_pConn;
	m_pConn = LTNULL;
	m_Incoming.clear();
	m_Objects.clear();
	m_SoundFlags.clear();
	m_pNetMgr->DisconnectNotify(pConn, reason);
	delete pConn;
	CloseSocket();
}

void CLT1Driver::Update()
{
	Receive();
	SendClientUpdate();

	if (m_pConn)
		m_pConn->UpdateAck(m_Socket);
}

// LT1's update packet, 0x25: the update rate, then the count and ids of the commands held.
// Sent unguaranteed at once when the rate or the held set changes, else every 0.2 s.
// The retail server diffs each list against the last and calls the server shell's OnCommandOn and OnCommandOff
// for every change, which is how a 1998 player's keys reach its object
void CLT1Driver::SendClientUpdate()
{
	if (!m_pConn || !g_pfnLT1ClientUpdate)
		return;

	uint8 aCommands[64];
	uint8 nRate = 10;
	uint32 nCommands = g_pfnLT1ClientUpdate(aCommands, sizeof(aCommands), &nRate);

	Bytes cPayload;
	cPayload.push_back(nRate);
	cPayload.push_back((uint8)nCommands);
	cPayload.insert(cPayload.end(), aCommands, aCommands + nCommands);

	uint32 nNow = time_GetMSTime();
	if (cPayload == m_LastUpdate && nNow - m_nLastUpdateTime < 200)
		return;

	SendLT1(LT1_CMSG_UPDATE, cPayload, false);
	m_LastUpdate = cPayload;
	m_nLastUpdateTime = nNow;
}

void CLT1Driver::Receive()
{
	if (m_Socket == -1)
		return;

	for (;;)
	{
		uint8 aBuf[8192];
		sockaddr_in cFrom;
		lt1_socklen nFromLen = sizeof(cFrom);
		int nRead = recvfrom((SOCKET)m_Socket, (char*)aBuf, sizeof(aBuf), 0, (sockaddr*)&cFrom, &nFromLen);
		if (nRead <= 0)
			return;
		if (!m_pConn)
			continue;

		if (aBuf[0] == 0)
		{
			if (nRead >= 2 && aBuf[1] == TCPSUB_DISCONNECT)
			{
				CLT1Conn *pConn = m_pConn;
				m_pConn = LTNULL;
				m_Incoming.clear();
				m_Objects.clear();
				m_SoundFlags.clear();
				m_pNetMgr->DisconnectNotify(pConn, DISCONNECTREASON_VOLUNTARY_SERVERSIDE);
				delete pConn;
				CloseSocket();
				return;
			}
			continue;
		}

		OnDatagram(aBuf, (uint32)nRead);
	}
}

// The reliable layer's receive side is the connection's (lt1wire.cpp), and this translates what it delivers.
// A message can end the connection, so each is checked for it
void CLT1Driver::OnDatagram(const uint8 *pData, uint32 nLen)
{
	if (!m_pConn)
		return;
	std::vector<Bytes> cDelivered;
	m_pConn->OnDatagram(m_Socket, pData, nLen, cDelivered);
	for (size_t i = 0; i < cDelivered.size() && m_pConn; i++)
		OnMessage(cDelivered[i]);
}

// Copies a Jupiter packet's bits from the read position to its end as bytes.
// Every field LT1 and this driver translate is a whole byte, word, dword, float or NUL terminated string,
// and Jupiter writes those LSB first and unpadded, so a byte aligned Jupiter body is the LT1 byte stream.
// A body that isn't a whole number of bytes came from a bit level writer, and its tail is padded with zeros
static void LT1ReadRest(CPacket_Read &cRead, std::vector<uint8> &cOut)
{
	uint32 nBits = cRead.TellEnd();
	while (nBits >= 8)
	{
		cOut.push_back(cRead.Readuint8());
		nBits -= 8;
	}
	if (nBits)
		cOut.push_back((uint8)cRead.ReadBits(nBits));
}

// Hands the client one Jupiter packet: the id, then LT1 bytes unchanged
void CLT1Driver::Deliver(uint8 nJupiterID, const uint8 *pData, uint32 nLen)
{
	CPacket_Write cPacket;
	cPacket.Writeuint8(nJupiterID);
	for (uint32 i = 0; i < nLen; i++)
		cPacket.Writeuint8(pData[i]);
	m_Incoming.push_back(CPacket_Read(cPacket));
}


// The object updates: LT1's byte records as Jupiter's bit packed ones
// (shellnet.cpp UnpackObjectChange, ReadAnimInfo, ReadNewObjectInfo, OnUnguaranteedUpdatePacket)

// A bounds checked cursor over an LT1 message. Any read past the end sets m_bBad and returns zeros,
// and the caller drops the rest of the message, as a client handler given a half translated record disconnects
struct LT1Cursor
{
	const uint8 *m_p;
	uint32 m_nLen, m_nPos;
	bool m_bBad;

	LT1Cursor(const uint8 *p, uint32 n) : m_p(p), m_nLen(n), m_nPos(0), m_bBad(false) {}
	uint32 Left() const { return m_nPos < m_nLen ? m_nLen - m_nPos : 0; }
	bool Need(uint32 n) { if (Left() < n) { m_bBad = true; return false; } return true; }
	uint8 Byte() { if (!Need(1)) return 0; return m_p[m_nPos++]; }
	uint16 Word() { if (!Need(2)) return 0; uint16 v = LT1Word(&m_p[m_nPos]); m_nPos += 2; return v; }
	uint32 DWord() { if (!Need(4)) return 0; uint32 v = LT1DWord(&m_p[m_nPos]); m_nPos += 4; return v; }
	float Float() { uint32 v = DWord(); float f; memcpy(&f, &v, 4); return f; }
	void Skip(uint32 n) { if (Need(n)) m_nPos += n; }
	const uint8 *Here() const { return &m_p[m_nPos]; }
	void String(std::string &out)
	{
		out.clear();
		while (Left() && m_p[m_nPos])
			out += (char)m_p[m_nPos++];
		if (!Need(1)) return;
		m_nPos++;
	}
};

// LT1's object flags (the server sends the low word) as Jupiter's.
// The bit numbers don't carry over below bit 13, and several are per object type
static uint32 LT1FlagsToJupiter(uint32 nLT1, uint8 nType)
{
	// The same bit in both: VISIBLE 0, SHADOW/UNSIGNED 1, FULLPOSITIONRES 8, NOLIGHT 9, YROTATION 11, SOLID 13,
	// BOXPHYSICS 14, CLIENTNONSOLID 15
	uint32 nOut = nLT1 & ((1u << 0) | (1u << 1) | (1u << 8) | (1u << 9) | (1u << 11) |
		(1u << 13) | (1u << 14) | (1u << 15));

	// Bit 3: a light's CASTSHADOWS, a sprite's ROTATEABLESPRITE and a particle system's UPDATEUNSEEN are Jupiter's
	// bit 3 too. A model's GOURAUDSHADE has no counterpart
	if ((nLT1 & (1u << 3)) && nType != OT_MODEL)
		nOut |= (1u << 3);
	// Bit 5: GLOWSPRITE, ONLYLIGHTWORLD and ENVIRONMENTMAPONLY agree. A model's ENVIRONMENTMAP isn't Jupiter's bit 5
	if ((nLT1 & (1u << 5)) && nType != OT_MODEL)
		nOut |= (1u << 5);
	// Bit 6: DONTLIGHTBACKFACING and REALLYCLOSE agree. A sprite's SPRITEBIAS is Jupiter's bit 2
	if (nLT1 & (1u << 6))
		nOut |= (nType == OT_SPRITE) ? FLAG_SPRITEBIAS : (1u << 6);
	// Bit 7: a model's ANIMTRANSITION is Jupiter's bit 22 and a sprite's SPRITE_NOZ Jupiter's bit 4.
	// A light's FOGLIGHT has none
	if (nLT1 & (1u << 7))
	{
		if (nType == OT_MODEL)
			nOut |= FLAG_ANIMTRANSITION;
		else if (nType == OT_SPRITE)
			nOut |= FLAG_SPRITE_NOZ;
	}
	// Dropped: 2 MODELTINT (Jupiter's SPRITEBIAS), 4 SOLIDLIGHT/CHROMAKEY/WIREFRAME/WASDRAWN (Jupiter's
	// WASDRAWN/SPRITE_NOZ), 10 HARDWAREONLY (Jupiter's PAUSED, which stops a model's animation),
	// 12 SKYOBJECT (Jupiter's RAYHIT, and sky objects arrive in SMSG_SKYDEF)
	return nOut;
}

// LT1's compressed rotation, 3 or 6 bytes by the sign of the first, decoded to be written with Jupiter's own writer
static bool LT1ReadRotation(LT1Cursor &r, LTRotation &rot)
{
	signed char b[6] = { 0, 0, 0, 0, 0, 0 };
	b[0] = (signed char)r.Byte();
	b[1] = (signed char)r.Byte();
	b[2] = (signed char)r.Byte();
	if (b[0] >= 0)
	{
		b[3] = (signed char)r.Byte();
		b[4] = (signed char)r.Byte();
		b[5] = (signed char)r.Byte();
	}
	if (r.m_bBad)
		return false;
	return LT1DecodeRotation(b, rot);
}

// One model animation, as Jupiter's tracker list with the main tracker only (ReadAnimInfo).
// LT1 sends the time as a fraction of the animation out of 255, and the driver doesn't know animation lengths,
// so the time is 0. A guaranteed MODELINFO is a change of animation, where LT1 restarts too
static void JupiterWriteAnim(CPacket_Write &w, uint8 nAnimByte)
{
	w.Writebool(true);							// A tracker follows
	w.Writebool(true);							// Dirty
	w.Writebool(false);							// Short index
	w.WriteBits(nAnimByte & 0x7F, MODELINFO_ANIMINDEX_SHORT);
	w.Writebool((nAnimByte & 0x80) != 0);		// Looping
	w.WriteBits(0, 2);							// Time size class 0
	w.WriteBits(0, MODELINFO_ANIMTIME_SIZE0);	// Time 0
	w.Writebool(false);							// Default rate
	w.Writebool(false);							// No more trackers
}

// LT1's PLAYSOUND_ flags as Jupiter's. The low seven (ambient, 3D, loop, attached, get handle, time, volume)
// and CLIENTLOCAL agree. Jupiter put CTRL_PITCH at LT1's REVERB (0x80) and moved REVERB, CLIENT and TIMESYNC up one,
// so LT1's FILESTREAM (0x400) is Jupiter's TIMESYNC. Passed through unmapped, a reverb sound would make the client
// read a pitch float that isn't there, and a streamed one an offset time. FILESTREAM has no Jupiter bit and is dropped
static uint16 LT1SoundFlagsToJupiter(uint16 nLT1)
{
	uint16 nOut = (uint16)(nLT1 & 0x087F);
	if (nLT1 & 0x0080) nOut |= PLAYSOUND_REVERB;
	if (nLT1 & 0x0100) nOut |= PLAYSOUND_CLIENT;
	if (nLT1 & 0x0200) nOut |= PLAYSOUND_TIMESYNC;
	return nOut;
}

// A sound description, from the change mask through the time into the sound (tracked sounds only), as Jupiter's
// ReadPlaySound and ReadNewSoundInfo read it. The layouts agree field for field but for the flags' numbering,
// and for Jupiter's pitch float, which LT1 has no flag to ask for.
// nFlags is LT1's per file flag cache, which the server diffs against: which fields follow depends on flags sent
// only when they change. The client keeps its own cache of the mapped flags, filled by the same masks
static void LT1TranslateSoundFields(LT1Cursor &r, uint16 &nFlags, bool bTrack, CPacket_Write &w)
{
	uint8 nMask = r.Byte();
	w.Writeuint8(nMask);
	if (nMask & 1)
	{
		nFlags = r.Word();
		w.Writeuint16(LT1SoundFlagsToJupiter(nFlags));
	}
	if (nMask & 2)
		w.Writeuint8(r.Byte());						// Priority
	if (nFlags & 3)									// PLAYSOUND_AMBIENT | PLAYSOUND_3D
	{
		if (nMask & 4)
		{
			w.Writeuint16(r.Word());				// Outer radius
			w.Writeuint8(r.Byte());					// Inner, as a fraction of 255
		}
	}
	if (nFlags & 0x40)								// PLAYSOUND_CTRL_VOL
		w.Writeuint8(r.Byte());
	if (bTrack && (nFlags & 0x200))					// LT1's PLAYSOUND_TIMESYNC
	{
		uint8 nTime = r.Byte();
		w.Writeuint8(nTime);
		if (nTime == 0xFF)
			w.Writeuint32(r.DWord());
	}
}

// SMSG_UPDATE's records as Jupiter's, each with a dword of its size in bits.
// The packet's group section (byte length and a whole packet, ended by 0) goes to the client first, as separate messages.
// Object removes go in a packet of their own, since Jupiter's reader takes them to the end of one
void CLT1Driver::TranslateUpdate(const uint8 *pBody, uint32 nBody)
{
	LT1Cursor r(pBody, nBody);
	while (r.Left())
	{
		uint32 nSub = r.Byte();
		if (!nSub)
			break;
		if (!r.Need(nSub))
			return;
		OnMessage(Bytes(r.Here(), r.Here() + nSub));
		r.Skip(nSub);
	}

	CPacket_Write cOut;
	Bytes cRemoves;
	while (r.Left() && !r.m_bBad)
	{
		uint32 nFlags = r.Byte();
		if (nFlags & CF_OTHER)
			nFlags |= (uint32)r.Byte() << 8;

		if (!nFlags)
		{
			uint8 nSubType = r.Byte();
			if (nSubType == UPDATESUB_PLAYSOUND || nSubType == UPDATESUB_SOUNDTRACK)
			{
				// Jupiter's record: no change flags, the subtype, the sound, then a dword of user data LT1 doesn't have.
				// The positions are three words over LT1's box, which ReadCompPos decodes under LT1CompPos, as for objects
				CPacket_Write w;
				w.Writeuint8(0);
				w.Writeuint8(nSubType);
				if (nSubType == UPDATESUB_PLAYSOUND)
				{
					uint16 nFile = r.Word();
					w.Writeuint16(nFile);
					uint16 &nSoundFlags = m_SoundFlags[nFile];
					LT1TranslateSoundFields(r, nSoundFlags, false, w);
					if ((nSoundFlags & 3) && !(nSoundFlags & 0x800))	// A position unless CLIENTLOCAL
					{
						for (uint32 i = 0; i < 3; i++)
							w.Writeuint16(r.Word());
					}
					w.Writeuint32(0);
				}
				else
				{
					// LT1 reads only NEW and POSITION here, which are Jupiter's CF_ bits too
					uint8 nTrackFlags = (uint8)(r.Byte() & (CF_NEWOBJECT | CF_POSITION));
					w.Writeuint8(nTrackFlags);
					w.Writeuint16(r.Word());				// The track's id
					if (nTrackFlags & CF_NEWOBJECT)
					{
						uint16 nFile = r.Word();
						w.Writeuint16(nFile);
						LT1TranslateSoundFields(r, m_SoundFlags[nFile], true, w);
						w.Writeuint32(0);
					}
					if (nTrackFlags & CF_POSITION)
					{
						for (uint32 i = 0; i < 3; i++)
							w.Writeuint16(r.Word());
					}
				}
				if (r.m_bBad)
					break;
				cOut.Writeuint32(w.Size());
				cOut.WritePacket(CPacket_Read(w));
			}
			else if (nSubType == UPDATESUB_OBJECTREMOVES)
			{
				while (r.Left() >= 2)
				{
					uint16 nID = r.Word();
					LT1PutWord(cRemoves, nID);
					m_Objects.erase(nID);
				}
			}
			else
			{
				r.m_bBad = true;
			}
			continue;
		}

		uint16 nID = r.Word();
		LT1Obj &cObj = m_Objects[nID];

		CPacket_Write w;
		w.Writeuint8((uint8)((nFlags & 0xFF) | ((nFlags & 0xFF00) ? CF_OTHER : 0)));
		if (nFlags & 0xFF00)
			w.Writeuint8((uint8)(nFlags >> 8));
		w.Writeuint16(nID);

		if (nFlags & CF_NEWOBJECT)
		{
			uint8 nTypeByte = r.Byte();
			cObj.m_nType = (uint8)(nTypeByte & 0x3F);	// Retail's mask
			cObj.m_nFlags = 0;
			uint32 nSFX = 0;
			if (nTypeByte & 0x40)
				nSFX = (nTypeByte & 0x80) ? r.Word() : r.Byte();
			if (!r.Need(nSFX))
				break;

			// Jupiter: the type byte, GlobalForceOverride, then the special effect message's length in bits
			// (a byte under 256, else a word)
			uint32 nSFXBits = nSFX * 8;
			w.Writeuint8((uint8)(cObj.m_nType | (nSFX ? SFXMESSAGE_FLAG : 0) |
				(nSFXBits >= 256 ? LONGSFXMESSAGE_FLAG : 0)));
			w.WriteLTVector(LTVector(0.0f, 0.0f, 0.0f));
			if (nSFX)
			{
				w.WriteBits(nSFXBits, nSFXBits >= 256 ? 16 : 8);
				for (uint32 i = 0; i < nSFX; i++)
					w.Writeuint8(r.Here()[i]);
				r.Skip(nSFX);
			}

			if (cObj.m_nType == OT_WORLDMODEL || cObj.m_nType == OT_CONTAINER)
			{
				std::string sName;
				r.String(sName);
				w.WriteString(sName.c_str());
				if (cObj.m_nType == OT_CONTAINER)
					w.Writeuint16(r.Word());
			}
			else if (cObj.m_nType == OT_MODEL)
			{
				// One file, no child models, one skin or none, no render styles
				uint16 nFile = r.Word();
				uint16 nSkin = r.Word();
				w.Writeuint16(nFile);
				w.WriteBits(1, FNumBits<MAX_CHILD_MODELS>::k_nValue);
				w.WriteBits(nSkin != 0xFFFF ? 1 : 0, FNumBits<MAX_MODEL_TEXTURES>::k_nValue);
				if (nSkin != 0xFFFF)
					w.Writeuint16(nSkin);
				w.WriteBits(0, FNumBits<MAX_MODEL_RENDERSTYLES>::k_nValue);
			}
			else if (cObj.m_nType == OT_SPRITE)
			{
				w.Writeuint16(r.Word());
			}
		}

		if (nFlags & CF_MODELINFO)
		{
			if (cObj.m_nType == OT_MODEL)
			{
				uint8 nAnim = r.Byte();
				r.Byte();						// The time, out of 255 (see JupiterWriteAnim)
				JupiterWriteAnim(w, nAnim);
			}
			else if (cObj.m_nType == OT_SPRITE)
			{
				// LT1's clipper is a poly index, and Jupiter's an HPOLY into a converted world whose numbering
				// isn't known here, so none (no clipping)
				r.DWord();
				HPOLY hNone;
				w.WriteType(hNone);
			}
		}

		if (nFlags & CF_FLAGS)
		{
			cObj.m_nFlags = r.Word();
			uint32 nUser = r.DWord();
			w.Writeuint32(LT1FlagsToJupiter(cObj.m_nFlags, cObj.m_nType));
			w.Writeuint16(0);					// Flags2, which LT1 doesn't have, at the width this client reads it
			w.Writeuint32(nUser);
		}

		if (nFlags & CF_RENDERINFO)
		{
			// Four bits a channel, n * 255 / 15 as LT1's client extends them.
			// The color's meaning is LT1's under LT1ObjectColor
			uint8 c0 = r.Byte(), c1 = r.Byte();
			w.Writeuint8((uint8)((c0 & 0xF) * 17));
			w.Writeuint8((uint8)((c0 >> 4) * 17));
			w.Writeuint8((uint8)((c1 & 0xF) * 17));
			w.Writeuint8((uint8)((c1 >> 4) * 17));
			w.Writeuint8(0);					// Render group
			if (cObj.m_nType == OT_LIGHT)
				w.Writeuint16(r.Word());
		}

		if (nFlags & CF_SCALE)
		{
			w.Writefloat(r.Float());
			w.Writefloat(r.Float());
			if (cObj.m_nType != OT_SPRITE)
				w.Writefloat(r.Float());
		}

		if (nFlags & (CF_POSITION | CF_TELEPORT))
		{
			if (cObj.m_nFlags & FLAG_FULLPOSITIONRES)
			{
				w.Writefloat(r.Float());
				w.Writefloat(r.Float());
				w.Writefloat(r.Float());
				w.WriteLTVector(LTVector(0.0f, 0.0f, 0.0f));	// LT1 sends no velocity
			}
			else
			{
				// The three words pass through, as under LT1CompPos the client's ReadCompPos decodes them over LT1's own box
				w.Writeuint16(r.Word());
				w.Writeuint16(r.Word());
				w.Writeuint16(r.Word());
				CLTMessage_Write::WriteCompLTVector(w, LTVector(0.0f, 0.0f, 0.0f));
			}
		}

		if (nFlags & (CF_ROTATION | CF_SNAPROTATION))
		{
			if (cObj.m_nFlags & FLAG_FULLPOSITIONRES)
			{
				w.Writefloat(r.Float());
				w.Writefloat(r.Float());
				w.Writefloat(r.Float());
				w.Writefloat(r.Float());
			}
			else
			{
				LTRotation rot;
				if (!LT1ReadRotation(r, rot))
					break;
				CLTMessage_Write::WriteCompLTRotation(w, rot);
			}
		}

		if (nFlags & CF_ATTACHMENTS)
		{
			for (;;)
			{
				uint16 nChild = r.Word();
				if (r.m_bBad || nChild == 0xFFFF)
					break;
				w.Writeuint16(nChild);
				w.Writeuint32(r.DWord());
				LTVector vOffset;
				vOffset.x = r.Float();
				vOffset.y = r.Float();
				vOffset.z = r.Float();
				w.WriteLTVector(vOffset);
				LTRotation rot;
				if (!LT1ReadRotation(r, rot))
					break;
				CLTMessage_Write::WriteCompLTRotation(w, rot);
			}
			w.Writeuint16(0xFFFF);
			if (cObj.m_nType == OT_MODEL)
			{
				// LT1 sends a count and node indices, and Jupiter no hidden pieces and the nodes as a bitmask
				uint32 aNodes[JUPITER_HIDDENNODE_DWORDS] = { 0, 0, 0, 0 };
				uint32 nHidden = r.Byte();
				for (uint32 i = 0; i < nHidden; i++)
				{
					uint32 nNode = r.Byte();
					if (nNode < JUPITER_HIDDENNODE_DWORDS * 32)
						aNodes[nNode / 32] |= 1u << (nNode % 32);
				}
				for (uint32 i = 0; i < JUPITER_HIDDENPIECE_DWORDS; i++)
					w.Writeuint32(0);
				for (uint32 i = 0; i < JUPITER_HIDDENNODE_DWORDS; i++)
					w.Writeuint32(aNodes[i]);
			}
		}

		if (r.m_bBad)
			break;

		cOut.Writeuint32(w.Size());
		cOut.WritePacket(CPacket_Read(w));
	}

	if (!cOut.Empty())
	{
		CPacket_Write cPacket;
		cPacket.Writeuint8(SMSG_UPDATE);
		cPacket.WritePacket(CPacket_Read(cOut));
		m_Incoming.push_back(CPacket_Read(cPacket));
	}
	if (!cRemoves.empty())
	{
		CPacket_Write cPacket;
		cPacket.Writeuint8(SMSG_UPDATE);
		CPacket_Write cRecord;
		cRecord.Writeuint8(0);
		cRecord.Writeuint8(UPDATESUB_OBJECTREMOVES);
		for (size_t i = 0; i < cRemoves.size(); i++)
			cRecord.Writeuint8(cRemoves[i]);
		cPacket.Writeuint32(cRecord.Size());
		cPacket.WritePacket(CPacket_Read(cRecord));
		m_Incoming.push_back(CPacket_Read(cPacket));
	}
}

// SMSG_UNGUARANTEEDUPDATE: a word `id | 0x4000 pos | 0x8000 yaw | 0x2000 rot | 0x1000 anim` and its fields,
// ended by FFFF and the game time, as Jupiter's id word, four UUF_ bits and fields
void CLT1Driver::TranslateUnguaranteed(const uint8 *pBody, uint32 nBody)
{
	LT1Cursor r(pBody, nBody);
	while (r.Left())
	{
		uint32 nSub = r.Byte();
		if (!nSub)
			break;
		if (!r.Need(nSub))
			return;
		OnMessage(Bytes(r.Here(), r.Here() + nSub));
		r.Skip(nSub);
	}

	CPacket_Write w;
	while (r.Left() >= 2 && !r.m_bBad)
	{
		uint16 nWord = r.Word();
		if (nWord == 0xFFFF)
		{
			w.Writeuint16(ID_TIMESTAMP);
			w.WriteBits(0, UUF_FLAGCOUNT);
			w.Writefloat(r.Float());
			break;
		}

		uint32 nUUF = 0;
		if (nWord & 0x4000) nUUF |= UUF_POS;
		if (nWord & 0x8000) nUUF |= UUF_YROTATION;
		else if (nWord & 0x2000) nUUF |= UUF_ROT;
		if (nWord & 0x1000) nUUF |= UUF_ANIMINFO;

		CPacket_Write rec;
		rec.Writeuint16((uint16)(nWord & 0x0FFF));
		rec.WriteBits(nUUF, UUF_FLAGCOUNT);
		if (nUUF & UUF_POS)
		{
			rec.Writeuint16(r.Word());
			rec.Writeuint16(r.Word());
			rec.Writeuint16(r.Word());
			rec.Writebool(false);					// No velocity
		}
		if (nUUF & UUF_YROTATION)
		{
			// LT1's Y rotation: (signed char) / 127 * pi, as Euler yaw
			float fYaw = (float)(signed char)r.Byte() / 127.0f * MATH_PI;
			CLTMessage_Write::WriteYRotation(rec, LTRotation(0.0f, fYaw, 0.0f));
		}
		else if (nUUF & UUF_ROT)
		{
			LTRotation rot;
			if (!LT1ReadRotation(r, rot))
				break;
			CLTMessage_Write::WriteCompLTRotation(rec, rot);
		}
		if (nUUF & UUF_ANIMINFO)
			JupiterWriteAnim(rec, r.Byte());
		if (r.m_bBad)
			break;
		w.WritePacket(CPacket_Read(rec));
	}

	if (!w.Empty())
	{
		CPacket_Write cPacket;
		cPacket.Writeuint8(SMSG_UNGUARANTEEDUPDATE);
		cPacket.WritePacket(CPacket_Read(w));
		m_Incoming.push_back(CPacket_Read(cPacket));
	}
}

// One LT1 message, in order, translated into what Jupiter's client handlers read (shellnet.cpp, ftclient.cpp).
//
//   LT1 server to client               Jupiter              change
//   4  NETPROTOCOLVERSION dword 5      4                    Jupiter's version and a bandwidth dword
//   5  UNLOADWORLD, 6 LOADWORLD,       same ids             none: float, words, bytes
//   7  CLIENTOBJECTID, 12 YOURID,
//   50 STC_FILEDESC
//   15 CHANGEOBJECTFILENAMES           SMSG_UPDATE          an update record with CF_FILENAMES
//   16 CONSOLEVAR                      SMSG_CONSOLEVAR 15   id
//   17 SKYDEF                          SMSG_SKYDEF 16       id (a 48 byte SkyDef in both)
//   18 INSTANTSPECIALEFFECT            same id              none
//   21 PRELOADLIST                     SMSG_PRELOADLIST 19  id (the subtypes agree)
//   13 MESSAGE, id last                SMSG_MESSAGE 13      id moved to the front
//   8  UPDATE, 10 UNGUARANTEEDUPDATE   same ids             re-encoded: TranslateUpdate, TranslateUnguaranteed
//
// Everything else is dropped: PORTALFLAGS and FOGLIGHT, which Jupiter's client has no handler for,
// CTABLE and the compressed UPDATE_C, which a stock server sends neither of
void CLT1Driver::OnMessage(const Bytes &cMsg)
{
	if (cMsg.empty() || !m_pConn)
		return;

	uint8 nID = (uint8)(cMsg[0] & 0x3F);

	// SMSG_PACKETGROUP: byte length and a whole packet, repeated
	if (nID == LT1_SMSG_PACKETGROUP)
	{
		uint32 nPos = 1;
		while (nPos < cMsg.size())
		{
			uint32 nSub = cMsg[nPos++];
			if (!nSub || nPos + nSub > cMsg.size())
				break;
			OnMessage(Bytes(cMsg.begin() + nPos, cMsg.begin() + nPos + nSub));
			nPos += nSub;
		}
		return;
	}

	const uint8 *pBody = cMsg.size() > 1 ? &cMsg[1] : LTNULL;
	uint32 nBody = (uint32)cMsg.size() - 1;

	switch (nID)
	{
		case LT1_SMSG_NETPROTOCOLVERSION:
		{
			uint32 nVersion = nBody >= 4 ? LT1DWord(pBody) : 0;
			if (nVersion != LT1_NETPROTOCOL_VERSION)
			{
				// Not a server this driver knows, so the client's own check refuses it with the number the server sent
				Bytes cOut;
				LT1PutDWord(cOut, nVersion);
				LT1PutDWord(cOut, 0xFFFFFFFF);
				Deliver(SMSG_NETPROTOCOLVERSION, &cOut[0], (uint32)cOut.size());
				break;
			}
			Bytes cOut;
			LT1PutDWord(cOut, LT_NET_PROTOCOL_VERSION);
			LT1PutDWord(cOut, 0xFFFFFFFF);	// No server bandwidth limit to adopt
			Deliver(SMSG_NETPROTOCOLVERSION, &cOut[0], (uint32)cOut.size());
			break;
		}

		case LT1_SMSG_UNLOADWORLD:		m_Objects.clear(); Deliver(SMSG_UNLOADWORLD, pBody, nBody); break;
		case LT1_SMSG_LOADWORLD:		m_Objects.clear(); Deliver(SMSG_LOADWORLD, pBody, nBody); break;
		case LT1_SMSG_CLIENTOBJECTID:	Deliver(SMSG_CLIENTOBJECTID, pBody, nBody); break;
		case LT1_SMSG_YOURID:			Deliver(SMSG_YOURID, pBody, nBody); break;
		case LT1_STC_FILEDESC:			Deliver(STC_FILEDESC, pBody, nBody); break;
		case LT1_SMSG_CONSOLEVAR:		Deliver(SMSG_CONSOLEVAR, pBody, nBody); break;
		case LT1_SMSG_SKYDEF:			Deliver(SMSG_SKYDEF, pBody, nBody); break;

		// An object's new files: its id, the file and the skin or FFFF.
		// Jupiter carries the change in the update record as CF_FILENAMES, so it becomes a record of its own
		case LT1_SMSG_CHANGEOBJECTFILENAMES:
		{
			if (nBody < 6)
				break;
			uint16 nObjID = LT1Word(pBody), nFile = LT1Word(pBody + 2), nSkin = LT1Word(pBody + 4);
			std::map<uint16, LT1Obj>::iterator iObj = m_Objects.find(nObjID);
			if (iObj == m_Objects.end() ||
				(iObj->second.m_nType != OT_MODEL && iObj->second.m_nType != OT_SPRITE))
				break;
			CPacket_Write w;
			w.Writeuint8(CF_OTHER);
			w.Writeuint8((uint8)(CF_FILENAMES >> 8));
			w.Writeuint16(nObjID);
			w.Writeuint16(nFile);
			if (iObj->second.m_nType == OT_MODEL)
			{
				// One file, no child models, one skin or none, no render styles, as a new object's are written
				w.WriteBits(1, FNumBits<MAX_CHILD_MODELS>::k_nValue);
				w.WriteBits(nSkin != 0xFFFF ? 1 : 0, FNumBits<MAX_MODEL_TEXTURES>::k_nValue);
				if (nSkin != 0xFFFF)
					w.Writeuint16(nSkin);
				w.WriteBits(0, FNumBits<MAX_MODEL_RENDERSTYLES>::k_nValue);
			}
			CPacket_Write cPacket;
			cPacket.Writeuint8(SMSG_UPDATE);
			cPacket.Writeuint32(w.Size());
			cPacket.WritePacket(CPacket_Read(w));
			m_Incoming.push_back(CPacket_Read(cPacket));
			break;
		}

		// The game's own bytes in both engines: both wrap the shell's message whole,
		// and both clients hand it whole to SpecialEffectNotify
		case LT1_SMSG_INSTANTSPECIALEFFECT:	Deliver(SMSG_INSTANTSPECIALEFFECT, pBody, nBody); break;

		// The subtypes are Jupiter's: 3 texture, 4 sprite, as measured on a retail server's own lists
		case LT1_SMSG_PRELOADLIST:		Deliver(SMSG_PRELOADLIST, pBody, nBody); break;

		case LT1_SMSG_UPDATE:				TranslateUpdate(pBody, nBody); break;
		case LT1_SMSG_UNGUARANTEEDUPDATE:	TranslateUnguaranteed(pBody, nBody); break;

		case LT1_SMSG_MESSAGE:
		{
			if (!nBody)
				break;
			Bytes cOut;
			cOut.push_back(pBody[nBody - 1]);
			cOut.insert(cOut.end(), pBody, pBody + nBody - 1);
			Deliver(SMSG_MESSAGE, &cOut[0], (uint32)cOut.size());
			break;
		}

		default:
			break;
	}
}

// What the client sends, translated into LT1's packets.
//
//   Jupiter client to server           LT1                  change
//   CMSG_HELLO 5                       0x23                 none: word length and client data
//   CMSG_GOODBYE 6                     0x24                 none
//   CMSG_CONNECTSTAGE 9                0x27                 none: one byte
//   CMSG_MESSAGE 11, game id first     0x29, id last        id moved to the end
//   CTS_FILESTATUS 54                  54                   every id gets 0x8000 (below)
//
// The goodbye matters: a retail host ignores TCPSUB_DISCONNECT from an address it already has a connection for,
// so without it the server keeps a departed client until it times out.
// CMSG_UPDATE, which in Jupiter carries a bandwidth word and finished sounds but no commands, is dropped,
// as SendClientUpdate sends LT1's own with the commands
bool CLT1Driver::SendPacket(const CPacket_Read &cPacket, CBaseConn *idSendTo, bool bGuaranteed)
{
	if (!m_pConn || idSendTo != m_pConn)
		return false;

	CPacket_Read cRead(cPacket);
	uint8 nID = cRead.Readuint8();
	Bytes cBody;
	LT1ReadRest(cRead, cBody);

	switch (nID)
	{
		case CMSG_HELLO:
			SendLT1(LT1_CMSG_HELLO, cBody, true);
			return true;

		case CMSG_GOODBYE:
			SendLT1(LT1_CMSG_GOODBYE, Bytes(), true);
			return true;

		case CMSG_UPDATE:
			return true;		// SendClientUpdate sends LT1's

		case CMSG_LT1COMMANDS:
			return true;		// The same states go in LT1's update (0x25)

		case CMSG_CONNECTSTAGE:
			SendLT1(LT1_CMSG_CONNECTSTAGE, cBody, true);
			return true;

		case CMSG_MESSAGE:
		{
			if (cBody.empty())
				return true;
			Bytes cOut(cBody.begin() + 1, cBody.end());
			cOut.push_back(cBody[0]);
			SendLT1(LT1_CMSG_GAMEMESSAGE, cOut, bGuaranteed);
			return true;
		}

		case CTS_FILESTATUS:
		{
			// Claim every file whatever the client's file manager said. LT1's client never downloads,
			// and a file the server believes wanted is sent block after block without end
			for (size_t i = 0; i + 1 < cBody.size(); i += 2)
				cBody[i + 1] |= 0x80;
			SendLT1(LT1_CTS_FILESTATUS, cBody, true);
			return true;
		}

		default:
			return true;
	}
}

bool CLT1Driver::GetPacket(CPacket_Read *pPacket, CBaseConn **pSender)
{
	Receive();
	if (m_Incoming.empty() || !m_pConn)
		return false;

	*pPacket = m_Incoming.front();
	m_Incoming.pop_front();
	*pSender = m_pConn;
	return true;
}
