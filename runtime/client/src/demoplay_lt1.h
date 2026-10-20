#ifndef __DEMOPLAY_LT1_H__
#define __DEMOPLAY_LT1_H__

// Plays back an LT1 demo for a shell that asks for one through StartGameRequest::m_PlaybackFilename, as Shogo's playdemo does
// The file holds only the player's input and the clock. The world is simulated again from them
// The shell's settings block in the header is skipped, its length found by where the frames start parsing

#include <vector>
#include <string>

class CDemoPlaybackLT1
{
public:
	// Reads the whole file. On success, World() is the recording's world
	bool        Load(const char *pPath);
	const char *World() const { return m_World.c_str(); }
	bool        Playing() const { return m_bPlaying; }

	// Called from StartGame. Frame 0's input applies at once, and the clock holds at its time while the world loads
	void        Begin();
	// Top of every client frame before its time update. Once the world is open, each frame plays one recorded frame
	void        StepClock(bool bWorldOpen);
	// After the devices are read the recorded keys and mouse replace them
	void        ApplyInput(unsigned char *pCommands, unsigned nCommands, float *pAxis);
	void        Stop();

private:
	struct Frame
	{
		float t;
		float axis[3];
		std::vector<unsigned char> flips;
	};
	void        SetClock(float t);
	void        Toggle(const Frame &f);

	std::vector<Frame> m_Frames;
	std::string m_World;
	float       m_fHeaderT = 0.0f, m_fT0 = 0.0f, m_fLastT = 0.0f;
	double      m_dBaseMS = 0.0;
	double      m_dRealStartMS = 0.0;	// The system timer at the first played frame (for LT1DemoEndConsole)
	unsigned    m_nWorldFrames = 0;		// Frames played in the world
	size_t      m_nNext = 0;
	bool        m_bPlaying = false, m_bPrimed = false;
	unsigned char m_Bank[256];
	float       m_Axis[3];
};

CDemoPlaybackLT1 &DemoPlaybackLT1();

#endif
