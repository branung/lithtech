#ifndef __IMASEQ_H__
#define __IMASEQ_H__

// A native IMA sequencer: one LT1 music section (.sec) played the way IMUSIC25.DLL plays it,
// as a list of timed MIDI events. It runs IMUSIC25's own rules (pattern selection, variation choice,
// harmonic pitch conversion, per-note randomization, chord-change note shortening, curves)
// and hands the result to a synth.

// A transcription of OpenBlood2's IMA engine.

// Depends on nothing but the C++ standard library, so it needs no engine objects.

#include <cstdint>
#include <string>
#include <vector>

namespace imaseq
{

// Where a section's files are read from: the section, its style, the folder's catalog and its .dls.
// Names are matched case insensitively, as the archives do
struct FileProvider
{
	virtual ~FileProvider() {}

	// One file from the folder. False if it isn't there
	virtual bool Read(const char *pszName, std::vector<uint8_t> &out) = 0;

	// Every file name in the folder, in any order
	virtual bool List(std::vector<std::string> &out) = 0;
};

// One MIDI message at a tick. Ticks are IMA ticks (768 to a whole note,
// whatever the time signature). m_Data2 is unused for program change.
struct Event
{
	int32_t		m_nTick;
	uint8_t		m_Status;
	uint8_t		m_Data1;
	uint8_t		m_Data2;
};

// The random stream. IMUSIC25 draws from the C runtime's rand(), which is
// this LCG; m_nState is what srand() sets.
struct Random
{
	uint32_t	m_nState;

	explicit Random(uint32_t nSeed = 1) : m_nState(nSeed) {}
	uint16_t Draw()
	{
		m_nState = m_nState * 214013u + 2531011u;
		return (uint16_t)((m_nState >> 16) & 0x7FFF);
	}
};

struct Options
{
	// The seed for the random stream, as srand() would take it.
	uint32_t	m_nSeed;

	// Which of the sixteen parts choose their variation at random, and which
	// cycle through them.  A channel in neither holds its variation.
	uint16_t	m_RandomChannels;
	uint16_t	m_CycleChannels;

	// The variation each part starts with.  Only cycle and hold read it; a
	// random part replaces it at the first pattern start.
	uint16_t	m_InitialVariation;

	Options() : m_nSeed(1), m_RandomChannels(0xFFFF), m_CycleChannels(0),
				m_InitialVariation(1) {}
};

struct Program
{
	bool					m_bOk;
	std::string				m_sError;

	// Band setup first (80 messages at tick 0: CC7, CC10, bank MSB, bank LSB,
	// program, for each of the sixteen routed channels), then every note and
	// curve message in the order the engine dispatches them.  Ticks are
	// already shifted by m_nPrerollTicks, so the earliest note is never
	// negative; the section's own tick 0 is at m_nPrerollTicks.
	std::vector<Event>		m_Events;

	int32_t					m_nPrerollTicks;
	int32_t					m_nDurationTicks;		// one pass of the section
	int32_t					m_nLastTick;			// the last event, preroll included

	// Tempo as the section stores it: whole beats a minute and a 1/65536
	// fraction.  TickToMilliseconds turns ticks into time the way IMRT25 does.
	int16_t					m_nTempo;
	uint16_t				m_nTempoFraction;

	std::string				m_sStyleName;			// the .sty that played
	std::string				m_sDlsName;				// the folder's one .dls

	// Reported for the trace and the test.
	uint32_t				m_nGrids;
	uint32_t				m_nNotes;				// note-ons emitted
	uint32_t				m_nCurveMessages;
	uint32_t				m_nDraws;				// random numbers consumed
	uint32_t				m_nPitchChanged;		// notes whose resolved pitch is not the stored MIDI note
	uint32_t				m_nPatternsSelected;
	uint32_t				m_nFirstPattern;		// index in the style

	Program() : m_bOk(false), m_nPrerollTicks(0), m_nDurationTicks(0),
				m_nLastTick(0), m_nTempo(0), m_nTempoFraction(0),
				m_nGrids(0), m_nNotes(0), m_nCurveMessages(0), m_nDraws(0),
				m_nPitchChanged(0), m_nPatternsSelected(0), m_nFirstPattern(0) {}
};

// Sequence one pass of the section pszSecName, a bare name in the provider's
// folder.  Its style, personality and bank are resolved in that folder.  The
// random stream is created from opts.m_nSeed.
Program Sequence(FileProvider &provider, const char *pszSecName,
				 const Options &opts = Options());

// The same, drawing from a stream the caller owns, so that several sections can
// share one stream the way they share the engine's rand().
Program SequenceWith(FileProvider &provider, const char *pszSecName,
					 const Options &opts, Random &rng);

// IMRT25's normal-speed clock: tempo clamped to 10..350 bpm and quantized to
// 1/16 bpm, 192 ticks to the tempo's beat, time rounded to whole milliseconds.
uint32_t TickToMilliseconds(const Program &prog, int32_t nTick);

// Microseconds a quarter note, as the original export writes its tempo event.
uint32_t ExportMicrosecondsPerQuarter(const Program &prog);

} // namespace imaseq

#endif // __IMASEQ_H__
