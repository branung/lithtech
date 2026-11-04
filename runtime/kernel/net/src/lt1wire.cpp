// LithTech 1.0's wire, for CLT1Driver: see lt1wire.h

#include "bdefs.h"
#include "lt1wire.h"
#include "systimer.h"

#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>


CLT1Conn::CLT1Conn() :
	m_nSent(0), m_nNextRecv(0), m_nPeerSent(0), m_nLastAck(0), m_bAckForced(false),
	m_bForcedDisconnect(false), m_fPing(0.0f), m_nNextPing(0), m_nPingSent(0), m_nPingID(0),
	m_nLastHeard(0)
{
	memset(&m_Addr, 0, sizeof(m_Addr));
	m_nLastAck = m_nNextPing = time_GetMSTime();
}

bool CLT1Conn::GetIPAddress(uint8 pAddr[4], uint16 *pPort)
{
	memcpy(pAddr, &m_Addr.sin_addr, 4);
	*pPort = ntohs(m_Addr.sin_port);
	return true;
}

void CLT1Conn::SendRaw(intptr_t nSocket, const uint8 *pData, uint32 nLen)
{
	if (nSocket == -1)
		return;
	sendto((SOCKET)nSocket, (const char*)pData, nLen, 0, (sockaddr*)&m_Addr, sizeof(m_Addr));
}

void CLT1Conn::SendLT1(intptr_t nSocket, uint8 nID, const LT1Bytes &cPayload, bool bGuaranteed)
{
	LT1Bytes cPacket;
	cPacket.push_back((uint8)(nID | (bGuaranteed ? LT1_GUARANTEED : 0)));
	cPacket.insert(cPacket.end(), cPayload.begin(), cPayload.end());
	if (bGuaranteed)
	{
		LT1PutDWord(cPacket, m_nSent);
		Outgoing &cOut = m_Outgoing[m_nSent];
		cOut.m_cData = cPacket;
		cOut.m_nRequests = 0;
		m_nSent++;
	}
	SendRaw(nSocket, &cPacket[0], (uint32)cPacket.size());
}

// Full id 0x41: sub 0, a ping when one is due, the last frame delivered in order, the frames sent,
// then up to eight frames still missing.
// LT1 lists the gaps between the frames it holds and then counts on from the last gap to the peer's frame count,
// which skips the frames before the first one held and asks again for some it has. This lists exactly the frames
// missing below the peer's count, which an LT1 sender answers the same way
void CLT1Conn::SendAck(intptr_t nSocket)
{
	uint32 nNow = time_GetMSTime();
	LT1Bytes cPacket;
	cPacket.push_back(0x41);
	cPacket.push_back(0);
	if ((int32)(nNow - m_nNextPing) >= 0)
	{
		m_nPingID++;
		m_nPingSent = nNow;
		m_nNextPing = nNow + (uint32)(rand() % (LT1_PING_MAX_MS + 1));
		cPacket.push_back(1);
		LT1PutWord(cPacket, m_nPingID);
	}
	else
	{
		cPacket.push_back(0);
	}
	LT1PutDWord(cPacket, m_nNextRecv ? m_nNextRecv - 1 : 0xFFFFFFFF);
	LT1PutDWord(cPacket, m_nSent);

	uint32 nEnd = m_nPeerSent;
	if (!m_Early.empty() && m_Early.rbegin()->first >= nEnd)
		nEnd = m_Early.rbegin()->first + 1;
	uint32 nMissing = 0;
	for (uint32 nFrame = m_nNextRecv; nFrame < nEnd && nMissing < 8; nFrame++)
	{
		if (m_Early.find(nFrame) == m_Early.end())
		{
			LT1PutDWord(cPacket, nFrame);
			nMissing++;
		}
	}

	SendRaw(nSocket, &cPacket[0], (uint32)cPacket.size());
	m_nLastAck = nNow;
	m_bAckForced = false;
}

void CLT1Conn::UpdateAck(intptr_t nSocket)
{
	uint32 nNow = time_GetMSTime();
	if (m_bAckForced || nNow - m_nLastAck > LT1_ACK_DELAY_MS)
		SendAck(nSocket);
}

// Id 1 is the layer's own. A guaranteed packet ends in its frame number and is delivered in order,
// and an unguaranteed one at once
void CLT1Conn::OnDatagram(intptr_t nSocket, const uint8 *pData, uint32 nLen, std::vector<LT1Bytes> &cOut)
{
	if (!nLen)
		return;
	m_nLastHeard = time_GetMSTime();

	if ((pData[0] & 0x3F) == 1)
	{
		OnLayerPacket(nSocket, pData, nLen, cOut);
		return;
	}

	if (!(pData[0] & LT1_GUARANTEED))
	{
		cOut.push_back(LT1Bytes(pData, pData + nLen));
		return;
	}

	if (nLen < 5)
		return;
	uint32 nFrame = LT1DWord(&pData[nLen - 4]);
	LT1Bytes cBody(pData, pData + nLen - 4);

	if (nFrame == m_nNextRecv)
	{
		m_nNextRecv++;
		cOut.push_back(cBody);
		std::map<uint32, LT1Bytes>::iterator iNext;
		while ((iNext = m_Early.find(m_nNextRecv)) != m_Early.end())
		{
			cOut.push_back(iNext->second);
			m_Early.erase(iNext);
			m_nNextRecv++;
		}
	}
	else if (nFrame > m_nNextRecv)
	{
		if (m_Early.find(nFrame) == m_Early.end())
			m_Early[nFrame] = cBody;
		// An early frame forces an ack, so the peer resends the gap
		m_bAckForced = true;
	}
	if (m_nPeerSent < m_nNextRecv)
		m_nPeerSent = m_nNextRecv;
}

// Id 1: sub 0 an ack, maybe with a ping to answer, 1 a forced disconnect, 2 a pong, 3 a group of whole packets
void CLT1Conn::OnLayerPacket(intptr_t nSocket, const uint8 *pData, uint32 nLen, std::vector<LT1Bytes> &cOut)
{
	if (nLen < 2)
		return;

	uint32 nPos = 2;
	switch (pData[1])
	{
		case 0:
		{
			if (nPos < nLen && pData[nPos++] && nPos + 2 <= nLen)
			{
				uint8 aPong[4] = { 1, 2, pData[nPos], pData[nPos + 1] };
				SendRaw(nSocket, aPong, sizeof(aPong));
				nPos += 2;
			}
			if (nPos + 8 > nLen)
				return;
			uint32 nLastAcked = LT1DWord(&pData[nPos]);
			uint32 nPeerSent = LT1DWord(&pData[nPos + 4]);
			nPos += 8;
			if (nPeerSent > m_nPeerSent)
				m_nPeerSent = nPeerSent;
			if (nLastAcked != 0xFFFFFFFF)
			{
				while (!m_Outgoing.empty() && m_Outgoing.begin()->first <= nLastAcked)
					m_Outgoing.erase(m_Outgoing.begin());
			}
			// Each frame asked for is resent on every other request, as LT1 does, so one lost resend costs
			// two acks rather than doubling every resend on a link slower than the ack rate
			for (int i = 0; i < 8 && nPos + 4 <= nLen; i++, nPos += 4)
			{
				std::map<uint32, Outgoing>::iterator iFrame = m_Outgoing.find(LT1DWord(&pData[nPos]));
				if (iFrame == m_Outgoing.end())
					continue;
				if ((iFrame->second.m_nRequests++ % 2) == 0)
					SendRaw(nSocket, &iFrame->second.m_cData[0], (uint32)iFrame->second.m_cData.size());
			}
			break;
		}

		case 1:
			m_bForcedDisconnect = true;
			break;

		case 2:
			if (nPos + 2 <= nLen && LT1Word(&pData[nPos]) == m_nPingID)
				m_fPing = (float)(time_GetMSTime() - m_nPingSent) / 1000.0f;
			break;

		case 3:
			while (nPos + 2 <= nLen)
			{
				uint32 nSub = LT1Word(&pData[nPos]);
				nPos += 2;
				if (nPos + nSub > nLen)
					break;
				OnDatagram(nSocket, &pData[nPos], nSub, cOut);
				nPos += nSub;
			}
			break;
	}
}


// LT1's compressed rotation

// The three byte form's forward and derived up: up is forward with 50 added to y, made perpendicular to forward
static void LT1SuperForwardUp(const signed char b[6], LTVector &forward, LTVector &up)
{
	forward.x = ((b[1] / 127.0f) - 0.5f) * 2.0f;
	forward.y = ((b[0] / 127.0f) - 0.5f) * 2.0f;
	forward.z = (float)b[2] / 127.0f;
	up.Init(forward.x, forward.y + 50.0f, forward.z);
	float t = -forward.Dot(up) / forward.MagSqr();
	up += forward * t;
	up.Normalize();
	forward.Normalize();
}

bool LT1DecodeRotation(const signed char bIn[6], LTRotation &rot)
{
	signed char b[6];
	memcpy(b, bIn, sizeof(b));

	LTVector right, up, forward;
	if (b[0] < 0)
	{
		bool bFlip = false;
		b[0] = (signed char)-b[0];
		if (b[1] < 0)
		{
			b[1] = (signed char)-b[1];
			bFlip = true;
		}
		LT1SuperForwardUp(b, forward, up);
		if (bFlip)
			up = -up;
	}
	else
	{
		forward.y = ((b[0] / 127.0f) - 0.5f) * 2.0f;
		forward.x = ((b[1] / 127.0f) - 0.5f) * 2.0f;
		forward.z = (float)b[2] / 127.0f;
		up.x = (float)b[3] / 127.0f;
		up.y = (float)b[4] / 127.0f;
		up.z = (float)b[5] / 127.0f;
	}

	right = forward.Cross(up);
	forward = up.Cross(right);
	right.Normalize();
	forward.Normalize();
	up = right.Cross(forward);

	LTMatrix mat;
	Mat_SetBasisVectors(&mat, &right, &up, &forward);
	quat_ConvertFromMatrix(rot.m_Quat, mat.m);
	return true;
}
