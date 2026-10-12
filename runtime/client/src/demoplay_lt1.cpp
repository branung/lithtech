// Engine side LT1 demo playback
//
// Recorded frame 0 applies at once and holds through the world load and the first rendered frame in it.
// After that, each rendered frame plays one recorded frame.
// The clock goes through the Demo* engine variables, which the client frame and the local server read

#include "bdefs.h"
#include "demoplay_lt1.h"
#include "systimer.h"
#include "consolecommands.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

extern int32 g_nDemoClockMS;
extern float g_fDemoFrameTime;
extern float g_fDemoClientFrameTime;
extern int32 g_nDemoAnimMS;
extern const char *g_DemoStopCommand;

namespace
{
	const unsigned int MAGIC = 0xF0F0AABCu;
	enum { TAG_TIME = 0, TAG_INPUT = 1, TAG_END = 2 };

	// The headers field sizes in bytes:
	// A magic number/signature 0xF0F0AABC (4 bytes), unused 64 bytes, the world name length (2 bytes), then the client clock written twice (8 bytes)
	enum { HDR_MAGIC = 4, HDR_UNUSED = 64, HDR_NAMELEN = 2, HDR_CLOCKS = 8 };

	// The number of frames that parse from pos, with pEnded set if they run to the end record
	unsigned CountFrames(const std::vector<unsigned char> &fileBytes, size_t pos, bool *pEnded)
	{
		*pEnded = false;
		unsigned nFrames = 0;
		float fLastTime = -1.0e30f;
		while (pos < fileBytes.size())
		{
			if (fileBytes[pos] == TAG_END) { *pEnded = true; return nFrames; }
			if (fileBytes[pos] != TAG_TIME || pos + 19 > fileBytes.size()) return nFrames;
			float fTime; memcpy(&fTime, &fileBytes[pos + 1], 4);
			if (!(fTime == fTime) || fTime < fLastTime || fTime > 1.0e7f) return nFrames; // Three tests: NaN, backwards time progression, or absurdly large time
			fLastTime = fTime;
			if (fileBytes[pos + 5] != TAG_INPUT) return nFrames;
			unsigned cnt = fileBytes[pos + 18];
			if (pos + 19 + cnt > fileBytes.size()) return nFrames;
			pos += 19 + cnt;
			++nFrames;
		}
		return nFrames;
	}
}

CDemoPlaybackLT1 &DemoPlaybackLT1()
{
	static CDemoPlaybackLT1 s_Demo;
	return s_Demo;
}

bool CDemoPlaybackLT1::Load(const char *pPath)
{
	m_Frames.clear();
	m_bPlaying = false;
	FILE *fp = pPath ? fopen(pPath, "rb") : NULL;
	if (!fp)
	{
		dsi_ConsolePrint("Cannot open demo %s", pPath ? pPath : "");
		return false;
	}
	std::vector<unsigned char> fileBytes;
	unsigned char chunk[65536];
	size_t nRead;
	while ((nRead = fread(chunk, 1, sizeof(chunk), fp)) > 0)
		fileBytes.insert(fileBytes.end(), chunk, chunk + nRead);
	fclose(fp);

	unsigned magic = 0;
	if (fileBytes.size() < HDR_MAGIC + HDR_UNUSED + HDR_NAMELEN + HDR_CLOCKS || (memcpy(&magic, &fileBytes[0], HDR_MAGIC), magic != MAGIC))
	{
		dsi_ConsolePrint("%s is not a demo", pPath);
		return false;
	}
	size_t pos = HDR_MAGIC + HDR_UNUSED;
	unsigned short len; memcpy(&len, &fileBytes[pos], HDR_NAMELEN); pos += HDR_NAMELEN;
	if (pos + len + HDR_CLOCKS > fileBytes.size())
	{
		dsi_ConsolePrint("Demo %s is truncated", pPath);
		return false;
	}
	m_World.assign((const char *)&fileBytes[pos], len); pos += len;
	memcpy(&m_fHeaderT, &fileBytes[pos], 4); pos += HDR_CLOCKS;

	// The shell block's length: the shortest one after which the records reach an end record.
	// For a recording that was never closed, it's the one that parses the most frames
	size_t best = (size_t)-1; unsigned bestN = 0;
	for (size_t skip = 0; skip <= 1024 && pos + skip < fileBytes.size(); skip += 4)
	{
		bool bEnded;
		unsigned c = CountFrames(fileBytes, pos + skip, &bEnded);
		if (bEnded && c > 0) { best = skip; bestN = c; break; }
		if (c > bestN) { best = skip; bestN = c; }
	}
	if (best == (size_t)-1 || bestN == 0)
	{
		dsi_ConsolePrint("Demo %s has no readable frames", pPath);
		return false;
	}
	pos += best;
	for (unsigned i = 0; i < bestN; i++)
	{
		Frame f;
		memcpy(&f.t, &fileBytes[pos + 1], 4);
		memcpy(f.axis, &fileBytes[pos + 6], 12);
		unsigned cnt = fileBytes[pos + 18];
		f.flips.assign(fileBytes.begin() + pos + 19, fileBytes.begin() + pos + 19 + cnt);
		pos += 19 + cnt;
		m_Frames.push_back(f);
	}
	return true;
}

void CDemoPlaybackLT1::Toggle(const Frame &f)
{
	for (unsigned char id : f.flips)
		m_Bank[id] = !m_Bank[id];
	for (int i = 0; i < 3; i++)
		m_Axis[i] = f.axis[i];
}

void CDemoPlaybackLT1::Begin()
{
	if (m_Frames.empty())
		return;
	srand(123); // LT1 seeds the C runtime when a recording starts
	memset(m_Bank, 0, sizeof(m_Bank));
	m_Axis[0] = m_Axis[1] = m_Axis[2] = 0.0f;
	Toggle(m_Frames[0]); // Recorded before the world loads
	m_nNext = 1;
	m_bPrimed = false;
	m_bPlaying = true;
	m_fT0 = m_fLastT = m_Frames[0].t;
	m_dBaseMS = (double)time_GetMSTime() + 1000.0; // A second ahead of the system timer so the game's clock never runs back
	SetClock(m_fT0);
	// The first frame is timed from the header's clock
	g_fDemoClientFrameTime = m_fT0 - m_fHeaderT > 0.0f ? m_fT0 - m_fHeaderT : 0.0f;
}

void CDemoPlaybackLT1::SetClock(float t)
{
	const double ms = m_dBaseMS + ((double)t - (double)m_fT0) * 1000.0;
	g_nDemoClockMS = (int32)(ms + 0.5);
	const float fFrame = t - m_fLastT; // The difference of the recorded float clocks
	m_fLastT = t;
	g_fDemoFrameTime = fFrame > 0.0f ? fFrame : 0.0f;
	g_fDemoClientFrameTime = g_fDemoFrameTime;
	g_nDemoAnimMS = fFrame > 0.0f ? (int32)((double)fFrame * 1000.0) : 0; // Truncated as LT1's model step is
}

void CDemoPlaybackLT1::StepClock(bool bWorldOpen)
{
	if (!m_bPlaying || !bWorldOpen)
		return;
	if (m_nNext >= m_Frames.size())
	{
		Stop();
		return;
	}
	// The first rendered frame in the world keeps recorded frame 0's time, and recorded frame 1 plays on the next rendered frame
	if (m_nNext == 1 && !m_bPrimed)
	{
		m_bPrimed = true;
		return;
	}
	const Frame &f = m_Frames[m_nNext];
	SetClock(f.t);
	Toggle(f);
	++m_nNext;
}

void CDemoPlaybackLT1::ApplyInput(unsigned char *pCommands, unsigned nCommands, float *pAxis)
{
	if (!m_bPlaying)
		return;
	for (unsigned i = 0; i < nCommands; i++)
		pCommands[i] = i < 256 ? m_Bank[i] : 0;
	pAxis[0] = m_Axis[0];
	pAxis[1] = m_Axis[1];
	pAxis[2] = m_Axis[2];
}

void CDemoPlaybackLT1::Stop()
{
	if (!m_bPlaying)
		return;
	m_bPlaying = false;
	memset(m_Bank, 0, sizeof(m_Bank));
	m_Axis[0] = m_Axis[1] = m_Axis[2] = 0.0f;
	g_nDemoClockMS = 0;
	g_fDemoFrameTime = 0.0f;
	g_fDemoClientFrameTime = 0.0f;
	g_nDemoAnimMS = -1;

	// The game's own restore. Copied first because the command may clear the variable
	if (g_DemoStopCommand && g_DemoStopCommand[0])
	{
		char szCmd[256];
		strncpy(szCmd, g_DemoStopCommand, sizeof(szCmd) - 1);
		szCmd[sizeof(szCmd) - 1] = 0;
		c_CommandHandler(szCmd);
	}
}
