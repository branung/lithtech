// A native IMA sequencer

// Credit to OpenBlood2's ima files, which this was transcribed from

#include "imaseq.h"

#include <algorithm>
#include <cstring>
#include <map>
#include <math.h>

namespace imaseq
{
namespace
{

typedef std::vector<uint8_t> Buf;

int16_t Word(int32_t v)			{ return (int16_t)(uint16_t)(uint32_t)v; }
int8_t  SByte(uint8_t v)		{ return (int8_t)v; }

int PopCount(uint32_t v)
{
	int n = 0;
	for (; v; v &= v - 1) ++n;
	return n;
}

std::string ToLower(const std::string &s)
{
	std::string o(s);
	for (size_t i = 0; i < o.size(); ++i)
		if (o[i] >= 'A' && o[i] <= 'Z') o[i] = (char)(o[i] - 'A' + 'a');
	return o;
}

// ---------------------------------------------------------------- reader ---

// A bounds-checked little-endian reader.  A read past the end latches the
// error and answers 0, so a caller checks good() once at the end.
class Reader
{
public:
	Reader(const uint8_t *p, size_t n) : m_p(p), m_n(n), m_pos(0), m_bGood(true) {}

	size_t	Remaining() const	{ return m_n - m_pos; }
	bool	Good() const		{ return m_bGood; }

	bool Skip(size_t n)
	{
		if (n > Remaining()) { m_bGood = false; return false; }
		m_pos += n;
		return true;
	}
	const uint8_t *Take(size_t n)
	{
		if (n > Remaining()) { m_bGood = false; return 0; }
		const uint8_t *p = m_p + m_pos;
		m_pos += n;
		return p;
	}
	uint8_t		U8()	{ const uint8_t *p = Take(1); return p ? p[0] : 0; }
	uint16_t	U16()	{ uint16_t a = U8(); uint16_t b = U8(); return (uint16_t)(a | (b << 8)); }
	uint32_t	U32()	{ uint32_t a = U16(); uint32_t b = U16(); return a | (b << 16); }
	int16_t		S16()	{ return (int16_t)U16(); }
	int32_t		S32()	{ return (int32_t)U32(); }

private:
	const uint8_t	*m_p;
	size_t			m_n;
	size_t			m_pos;
	bool			m_bGood;
};

// One RIFF chunk, flattened in file order with its depth: the outer RIFF is
// depth 0 and its children depth 1.  m_nOffset is the chunk HEADER's offset in
// the file; its payload starts 8 bytes later.
struct Chunk
{
	char		m_Id[5];
	char		m_Form[5];		// RIFF and LIST only
	uint32_t	m_nOffset;
	uint32_t	m_nSize;
	uint32_t	m_nDepth;

	bool Is(const char *psz) const		{ return memcmp(m_Id, psz, 4) == 0; }
	bool FormIs(const char *psz) const	{ return memcmp(m_Form, psz, 4) == 0; }
};

bool WalkChunks(const Buf &b, uint32_t nStart, uint32_t nEnd, uint32_t nDepth,
				std::vector<Chunk> &out)
{
	if (nDepth > 16) return false;
	uint32_t pos = nStart;
	while (pos < nEnd)
	{
		if (nEnd - pos < 8 || out.size() >= 8192) return false;
		Chunk c;
		memset(&c, 0, sizeof c);
		memcpy(c.m_Id, &b[pos], 4);
		c.m_nOffset = pos;
		c.m_nSize = (uint32_t)b[pos + 4] | ((uint32_t)b[pos + 5] << 8) |
					((uint32_t)b[pos + 6] << 16) | ((uint32_t)b[pos + 7] << 24);
		c.m_nDepth = nDepth;
		if (c.m_nSize > nEnd - pos - 8) return false;
		const bool bContainer = c.Is("RIFF") || c.Is("LIST");
		if (bContainer)
		{
			if (c.m_nSize < 4) return false;
			memcpy(c.m_Form, &b[pos + 8], 4);
		}
		out.push_back(c);
		if (bContainer &&
			!WalkChunks(b, pos + 12, pos + 8 + c.m_nSize, nDepth + 1, out))
			return false;
		pos += 8 + c.m_nSize + (c.m_nSize & 1);
	}
	return pos == nEnd || pos == nEnd + 1;
}

bool ReadChunks(const Buf &b, const char *pszForm, std::vector<Chunk> &out)
{
	out.clear();
	if (b.size() < 12 || memcmp(&b[0], "RIFF", 4) != 0 || memcmp(&b[8], pszForm, 4) != 0)
		return false;
	return WalkChunks(b, 0, (uint32_t)b.size(), 0, out);
}

// A UTF-16 label that stops at its first NUL; bytes after it are padding.
bool Label16(const uint8_t *p, size_t n, std::string &out)
{
	out.clear();
	if (!p) return false;
	for (size_t i = 0; i + 1 < n; i += 2)
	{
		uint16_t c = (uint16_t)(p[i] | (p[i + 1] << 8));
		if (!c) return true;
		if (c < 32 || c > 126) return false;
		out.push_back((char)c);
	}
	return false;
}

bool Label8(const uint8_t *p, size_t n, std::string &out)
{
	out.clear();
	if (!p) return false;
	for (size_t i = 0; i < n; ++i)
	{
		if (!p[i]) return true;
		if (p[i] < 32 || p[i] > 126) return false;
		out.push_back((char)p[i]);
	}
	return false;
}

// --------------------------------------------------------------- section ---

struct SectionCommand	{ int32_t m_nTime; uint8_t m_Value[4]; };
struct SectionMute		{ int32_t m_nTime; uint16_t m_nValue; };
struct SectionChord
{
	uint8_t		m_Flags;
	int8_t		m_nBeat;
	int16_t		m_nMeasure;
	uint32_t	m_ChordPattern;
	uint32_t	m_ScalePattern;
	int8_t		m_nRoot;
};

struct Section
{
	std::string	m_sStyle;				// sref
	std::string	m_sPersonality;			// prnm
	int16_t		m_nTempo;
	int16_t		m_nRepeats;				// secn +0x26: passes after the first
	int16_t		m_nMeasures;
	int16_t		m_nTicksPerMeasure;
	int16_t		m_nTicksPerBeat;
	uint16_t	m_nTempoFraction;
	uint8_t		m_DefaultChord;
	uint32_t	m_nHeaderOffset;		// the secn chunk, for band selection
	std::vector<SectionCommand>	m_Commands;
	std::vector<SectionMute>	m_Mutes;
	std::vector<SectionChord>	m_Chords;
	uint32_t	m_nOpaqueNotes;			// section-level note/curv chunks: unsupported
};

bool ReadSection(const Buf &b, const std::vector<Chunk> &chunks, Section &s, std::string &sError)
{
	bool bHeader = false, bPers = false, bCmd = false, bMute = false, bChord = false;
	s = Section();
	s.m_nOpaqueNotes = 0;
	for (size_t i = 0; i < chunks.size(); ++i)
	{
		const Chunk &c = chunks[i];
		if (c.m_nDepth != 1) continue;
		const uint8_t *p = &b[c.m_nOffset + 8];
		Reader r(p, c.m_nSize);
		if (c.Is("sref"))
		{
			if (!Label16(p, c.m_nSize, s.m_sStyle)) { sError = "bad section style reference"; return false; }
		}
		else if (c.Is("secn"))
		{
			if (bHeader || c.m_nSize != 120) { sError = "bad section header"; return false; }
			bHeader = true;
			s.m_nHeaderOffset = c.m_nOffset;
			r.Skip(4 + 32);
			s.m_nTempo = r.S16();
			s.m_nRepeats = r.S16();
			s.m_nMeasures = r.S16();
			s.m_nTicksPerMeasure = r.S16();
			s.m_nTicksPerBeat = r.S16();
			s.m_nTempoFraction = r.U16();
			r.U32();
			s.m_DefaultChord = r.U8();
		}
		else if (c.Is("prnm"))
		{
			if (bPers || c.m_nSize > 80 || !Label16(p, c.m_nSize, s.m_sPersonality))
			{ sError = "bad section personality"; return false; }
			bPers = true;
		}
		else if (c.Is("cmnd") || c.Is("mute") || c.Is("chrd"))
		{
			bool &bSeen = c.Is("cmnd") ? bCmd : c.Is("mute") ? bMute : bChord;
			const uint32_t nStride = c.Is("chrd") ? 117 : 8;
			if (!bHeader || bSeen || c.m_nSize < 2 || r.U16() != nStride ||
				(c.m_nSize - 2) % nStride)
			{ sError = "bad section record list"; return false; }
			bSeen = true;
			while (r.Remaining() && r.Good())
			{
				if (c.Is("cmnd"))
				{
					SectionCommand v;
					v.m_nTime = r.S32();
					for (int k = 0; k < 4; ++k) v.m_Value[k] = r.U8();
					s.m_Commands.push_back(v);
				}
				else if (c.Is("mute"))
				{
					SectionMute v;
					v.m_nTime = r.S32();
					v.m_nValue = r.U16();
					r.U16();
					s.m_Mutes.push_back(v);
				}
				else
				{
					SectionChord v;
					r.Skip(32);					// name
					v.m_Flags = r.U8();
					v.m_nBeat = SByte(r.U8());
					v.m_nMeasure = r.S16();
					v.m_ChordPattern = r.U32();
					v.m_ScalePattern = r.U32();
					r.U32();
					v.m_nRoot = SByte(r.U8());
					r.Skip(68);
					s.m_Chords.push_back(v);
				}
			}
		}
		else if (c.Is("note") || c.Is("curv"))
			++s.m_nOpaqueNotes;
		if (!r.Good()) { sError = "truncated section chunk"; return false; }
	}
	if (!bHeader) { sError = "section has no header"; return false; }
	return true;
}

// ----------------------------------------------------------------- style ---

struct Note
{
	uint8_t		m_nType;
	uint8_t		m_nChannel;			// the IMA part, 0..15
	int16_t		m_nTimeOffset;
	uint16_t	m_Variations;
	uint8_t		m_PitchModifier;
	uint8_t		m_MidiNote;
	uint8_t		m_Velocity;
	uint16_t	m_MusicValue;
	int16_t		m_nDuration;
	uint8_t		m_TimeRange;
	uint8_t		m_DurationRange;
	uint8_t		m_VelocityRange;
	uint8_t		m_PlayMode;
};

struct CurvePoint
{
	uint8_t		m_nShape;
	uint8_t		m_Flags;
	int16_t		m_nStart, m_nValue, m_nEnd, m_nTarget;
};

struct Curve
{
	uint8_t		m_nType;
	uint8_t		m_nChannel;
	int16_t		m_nTimeOffset;
	uint16_t	m_Variations;
	uint8_t		m_nController;
	std::vector<CurvePoint>	m_Points;
};

// A grid's child chunks in file order: a run of notes or a run of curves.  The
// engine dispatches a grid's note-ons and curve messages in this order.
struct GridItem
{
	bool		m_bCurves;
	uint32_t	m_nFirst;			// index into the grid's m_Notes or m_Curves
	uint32_t	m_nCount;
};

struct Grid
{
	int16_t					m_nIndex;
	std::vector<Note>		m_Notes;
	std::vector<Curve>		m_Curves;
	std::vector<GridItem>	m_Items;
};

struct Pattern
{
	uint32_t	m_Flags;
	int32_t		m_nLength;
	int16_t		m_Timing[4];		// [0] ticks a grid, [3] length in measures
	uint32_t	m_ChordMask;
	uint32_t	m_ScaleMask;
	int8_t		m_Upper[16];
	int8_t		m_Lower[16];
	uint16_t	m_RangeChannels;
	uint16_t	m_VariationFlags[16][16];
	int8_t		m_nRoot;
	uint8_t		m_PlayModes[16];
	std::vector<Grid>	m_Grids;
};

struct Band
{
	uint32_t	m_nOffset;			// its "band" chunk
	bool		m_bHasCollection;
	int8_t		m_nByte104;
	uint8_t		m_LegacyProgram[16];
	uint8_t		m_Volume[16];
	uint8_t		m_Pan[16];
	int8_t		m_Transpose[16];
	uint16_t	m_Bank[16];
	uint8_t		m_Program[16];
};

struct Style
{
	int16_t		m_nBeatsPerMeasure;
	int16_t		m_nBeatUnit;
	int16_t		m_nTempo;
	uint32_t	m_nHeaderOffset;
	std::vector<Pattern>	m_Patterns;
	std::vector<Band>		m_Bands;
};

bool ReadNotes(Reader &r, Grid &g)
{
	if (r.Remaining() < 2 || r.U16() != 18 || r.Remaining() % 18) return false;
	size_t n = r.Remaining() / 18;
	for (size_t i = 0; i < n; ++i)
	{
		Note v;
		v.m_nType = r.U8();
		v.m_nChannel = r.U8();
		v.m_nTimeOffset = r.S16();
		v.m_Variations = r.U16();
		v.m_PitchModifier = r.U8();
		r.U8();
		v.m_MidiNote = r.U8();
		v.m_Velocity = r.U8();
		v.m_MusicValue = r.U16();
		v.m_nDuration = r.S16();
		v.m_TimeRange = r.U8();
		v.m_DurationRange = r.U8();
		v.m_VelocityRange = r.U8();
		v.m_PlayMode = r.U8();
		g.m_Notes.push_back(v);
	}
	return r.Good();
}

// The curv chunk is two-level: two record sizes, then per curve a 7-byte header,
// a point count and that many 10-byte points.
bool ReadCurves(Reader &r, Grid &g)
{
	if (r.Remaining() < 4 || r.U16() != 7 || r.U16() != 10) return false;
	while (r.Remaining())
	{
		if (r.Remaining() < 9) return false;
		Curve c;
		c.m_nType = r.U8();
		c.m_nChannel = r.U8();
		c.m_nTimeOffset = r.S16();
		c.m_Variations = r.U16();
		c.m_nController = r.U8();
		uint16_t n = r.U16();
		if (n > r.Remaining() / 10) return false;
		for (uint16_t i = 0; i < n; ++i)
		{
			CurvePoint pt;
			pt.m_nShape = r.U8();
			pt.m_Flags = r.U8();
			pt.m_nStart = r.S16();
			pt.m_nValue = r.S16();
			pt.m_nEnd = r.S16();
			pt.m_nTarget = r.S16();
			c.m_Points.push_back(pt);
		}
		g.m_Curves.push_back(c);
	}
	return r.Good();
}

bool ReadBand(const uint8_t *p, uint32_t n, Band &band)
{
	std::string sName, sCollection;
	if (n != 202 || !Label16(p, 32, sName) || !Label8(p + 170, 32, sCollection))
		return false;
	band.m_bHasCollection = !sCollection.empty();
	Reader r(p, n);
	r.Skip(32 + 8);
	for (int i = 0; i < 16; ++i) band.m_LegacyProgram[i] = r.U8();
	for (int i = 0; i < 16; ++i) band.m_Volume[i] = r.U8();
	for (int i = 0; i < 16; ++i) band.m_Pan[i] = r.U8();
	for (int i = 0; i < 16; ++i) band.m_Transpose[i] = SByte(r.U8());
	band.m_nByte104 = SByte(r.U8());
	r.U8();
	for (int i = 0; i < 16; ++i) band.m_Bank[i] = r.U16();
	for (int i = 0; i < 16; ++i) band.m_Program[i] = r.U8();
	return r.Good();
}

// Every direct RIFF/AABN band, in file order; each has exactly one "band".
bool ReadBands(const Buf &b, const std::vector<Chunk> &chunks, std::vector<Band> &out)
{
	for (size_t i = 1; i < chunks.size(); ++i)
	{
		const Chunk &c = chunks[i];
		if (c.m_nDepth != 1 || !c.Is("RIFF") || !c.FormIs("AABN")) continue;
		bool bHeader = false;
		Band band;
		memset(&band, 0, sizeof band);
		for (size_t j = i + 1; j < chunks.size() && chunks[j].m_nDepth > 1; ++j)
		{
			if (chunks[j].m_nDepth != 2 || !chunks[j].Is("band")) continue;
			if (bHeader || !ReadBand(&b[chunks[j].m_nOffset + 8], chunks[j].m_nSize, band))
				return false;
			bHeader = true;
			band.m_nOffset = chunks[j].m_nOffset;
		}
		if (!bHeader) return false;
		out.push_back(band);
	}
	return true;
}

size_t NextSibling(const std::vector<Chunk> &chunks, size_t i)
{
	uint32_t d = chunks[i++].m_nDepth;
	while (i < chunks.size() && chunks[i].m_nDepth > d) ++i;
	return i;
}

bool ReadGrid(const Buf &b, const std::vector<Chunk> &chunks, size_t i, Pattern &pat)
{
	Grid g;
	bool bIndex = false;
	for (size_t k = i + 1, end = NextSibling(chunks, i); k < end; k = NextSibling(chunks, k))
	{
		const Chunk &c = chunks[k];
		Reader r(&b[c.m_nOffset + 8], c.m_nSize);
		if (c.Is("clik"))
		{
			if (bIndex || c.m_nSize != 2) return false;
			bIndex = true;
			g.m_nIndex = r.S16();
		}
		else if (c.Is("note"))
		{
			GridItem it = { false, (uint32_t)g.m_Notes.size(), 0 };
			if (!ReadNotes(r, g)) return false;
			it.m_nCount = (uint32_t)g.m_Notes.size() - it.m_nFirst;
			g.m_Items.push_back(it);
		}
		else if (c.Is("curv"))
		{
			GridItem it = { true, (uint32_t)g.m_Curves.size(), 0 };
			if (!ReadCurves(r, g)) return false;
			it.m_nCount = (uint32_t)g.m_Curves.size() - it.m_nFirst;
			g.m_Items.push_back(it);
		}
	}
	if (!bIndex) return false;
	pat.m_Grids.push_back(g);
	return true;
}

bool ReadPattern(const Buf &b, const std::vector<Chunk> &chunks, size_t i, Style &sty)
{
	Pattern pat;
	memset(pat.m_Timing, 0, sizeof pat.m_Timing);
	bool bHeader = false;
	for (size_t k = i + 1, end = NextSibling(chunks, i); k < end; k = NextSibling(chunks, k))
	{
		const Chunk &c = chunks[k];
		if (c.Is("patt"))
		{
			if (bHeader || c.m_nSize != 625) return false;
			bHeader = true;
			Reader r(&b[c.m_nOffset + 8], c.m_nSize);
			r.U32();
			pat.m_nLength = r.S32();
			pat.m_Flags = r.U32();
			for (int t = 0; t < 4; ++t) pat.m_Timing[t] = r.S16();
			r.Skip(32);						// name
			pat.m_ChordMask = r.U32();
			pat.m_ScaleMask = r.U32();
			for (int t = 0; t < 16; ++t) pat.m_Upper[t] = SByte(r.U8());
			for (int t = 0; t < 16; ++t) pat.m_Lower[t] = SByte(r.U8());
			pat.m_RangeChannels = r.U16();
			for (int ch = 0; ch < 16; ++ch)
				for (int v = 0; v < 16; ++v)
					pat.m_VariationFlags[ch][v] = r.U16();
			r.U16();
			pat.m_nRoot = SByte(r.U8());
			for (int t = 0; t < 16; ++t) pat.m_PlayModes[t] = r.U8();
			if (!r.Good()) return false;
		}
		else if (c.Is("LIST") && c.FormIs("AACL") && !ReadGrid(b, chunks, k, pat))
			return false;
	}
	if (!bHeader) return false;
	sty.m_Patterns.push_back(pat);
	return true;
}

bool ReadStyle(const Buf &b, Style &sty, std::string &sError)
{
	std::vector<Chunk> chunks;
	sty = Style();
	if (!ReadChunks(b, "AASY", chunks)) { sError = "style is not a RIFF AASY"; return false; }
	bool bHeader = false;
	for (size_t i = 1; i < chunks.size(); i = NextSibling(chunks, i))
	{
		const Chunk &c = chunks[i];
		if (c.Is("styl"))
		{
			if (bHeader || c.m_nSize != 94) { sError = "bad style header"; return false; }
			bHeader = true;
			sty.m_nHeaderOffset = c.m_nOffset;
			Reader r(&b[c.m_nOffset + 8], c.m_nSize);
			r.Skip(32);
			sty.m_nBeatsPerMeasure = r.S16();
			sty.m_nBeatUnit = r.S16();
			r.S16();
			r.S16();
			r.S16();
			sty.m_nTempo = r.S16();
		}
		else if (c.Is("RIFF") && c.FormIs("AAPT") && !ReadPattern(b, chunks, i, sty))
		{
			sError = "bad style pattern";
			return false;
		}
	}
	if (!bHeader) { sError = "style has no header"; return false; }
	if (!ReadBands(b, chunks, sty.m_Bands)) { sError = "bad style band"; return false; }
	return true;
}

// ----------------------------------------------------------- personality ---

// Only the scale mask is needed: it is the base harmony.  REPs is big-endian.
bool ReadPersonalityScale(const Buf &b, uint32_t &nScale)
{
	if (b.size() < 12 + 20 || memcmp(&b[0], "REPs", 4) != 0) return false;
	uint32_t nHeader = ((uint32_t)b[8] << 24) | ((uint32_t)b[9] << 16) |
					   ((uint32_t)b[10] << 8) | b[11];
	if (nHeader < 1164 || nHeader > b.size() - 12) return false;
	const uint8_t *h = &b[12];
	nScale = (uint32_t)h[16] | ((uint32_t)h[17] << 8) | ((uint32_t)h[18] << 16) |
			 ((uint32_t)h[19] << 24);
	return true;
}

// -------------------------------------------------------------- timeline ---

// The section's records re-timed against the style's meter, which the loader
// prefers over the section's own: a beat is 768 / beat unit ticks, a measure
// that times the style's beats per measure.
struct Timeline
{
	int16_t		m_nTicksPerBeat;
	int16_t		m_nTicksPerMeasure;
	int32_t		m_nDuration;
	std::vector<int32_t>	m_CommandTimes;		// parallel to Section::m_Commands
	std::vector<int32_t>	m_MuteTimes;		// one per beat
	std::vector<uint16_t>	m_MuteValues;
	std::vector<size_t>		m_ChordSource;		// retained chords
	std::vector<int32_t>	m_ChordTimes;
};

bool PrepareTimeline(const Section &sec, const Style &sty, Timeline &t, std::string &sError)
{
	const int nUnit = sty.m_nBeatUnit ? sty.m_nBeatUnit : 4;
	const int nBeat = nUnit > 0 ? 768 / nUnit : 0;
	const int nMeasure = nBeat * (int)sty.m_nBeatsPerMeasure;
	if (sec.m_nMeasures <= 0 || sec.m_nTicksPerMeasure <= 0 || nBeat <= 0 ||
		nMeasure <= 0 || nMeasure > 32767)
	{
		sError = "unsafe section length or style meter";
		return false;
	}
	t = Timeline();
	t.m_nTicksPerBeat = (int16_t)nBeat;
	t.m_nTicksPerMeasure = (int16_t)nMeasure;
	t.m_nDuration = (int32_t)sec.m_nMeasures * nMeasure;
	if ((t.m_nDuration + nBeat - 1) / nBeat > 8192) { sError = "section too long"; return false; }

	for (size_t i = 0; i < sec.m_Commands.size(); ++i)
	{
		// The original's signed division keeps only AX.
		int16_t nM = Word(sec.m_Commands[i].m_nTime / sec.m_nTicksPerMeasure);
		t.m_CommandTimes.push_back((int32_t)nM * nMeasure);
	}

	// Authored order, including an initial future record's value; no sort.
	size_t cur = 0;
	uint16_t v = sec.m_Mutes.empty() ? 0 : sec.m_Mutes[0].m_nValue;
	for (int32_t tick = 0; tick < t.m_nDuration; tick += nBeat)
	{
		while (cur < sec.m_Mutes.size() && sec.m_Mutes[cur].m_nTime <= tick)
			v = sec.m_Mutes[cur++].m_nValue;
		t.m_MuteTimes.push_back(tick);
		t.m_MuteValues.push_back(v);
	}

	for (size_t i = 0; i < sec.m_Chords.size(); ++i)
	{
		const SectionChord &c = sec.m_Chords[i];
		if (c.m_nBeat >= nMeasure / nBeat) continue;
		t.m_ChordSource.push_back(i);
		t.m_ChordTimes.push_back((int32_t)c.m_nMeasure * nMeasure + (int32_t)c.m_nBeat * nBeat);
	}
	return true;
}

// --------------------------------------------------------------- harmony ---

struct Harmonic
{
	uint32_t	m_ChordMask;
	uint32_t	m_ScaleMask;
	int8_t		m_nChordRoot;
	int8_t		m_nScaleRoot;
	uint8_t		m_ChordFlags;
};

struct PreparedChord
{
	Harmonic	m_H;
	uint16_t	m_VariationFlags;		// what a variation must carry to be eligible
};

uint8_t PitchFlags(uint32_t mask)
{
	const int n = PopCount(mask);
	// Not masked to four bits: the original ORs the full byte.
	return (uint8_t)(0x10 | (n > 3 ? 0x20 : 0) | (mask & 0x3c0000 ? 0x40 : 0) | n);
}

uint32_t Bit(int i)	{ return (uint32_t)1 << ((unsigned)i & 31); }

uint32_t Rotated(uint32_t mask, int root)
{
	const int rem = root % 12;		// signed remainder, x86 masked shifts
	mask &= 0xffffff;
	return ((mask >> ((unsigned)(24 - rem) & 31)) | (mask << ((unsigned)rem & 31))) & 0xffffff;
}

bool OutsideTemplates(const SectionChord &c, const Harmonic &base)
{
	const uint32_t scale = Rotated(base.m_ChordMask, base.m_nChordRoot);
	const uint32_t mask = Rotated(c.m_ChordPattern, c.m_nRoot);
	uint32_t tpl[4] = { 0, 0, 0, 0 };
	int degree = 0, index = c.m_nRoot % 12;
	for (int i = 0; i < 12; ++i)
	{
		const uint32_t v = Bit(index);
		if (scale & v)
		{
			++degree;
			if (degree == 1 || degree == 3 || degree == 5)
				for (int k = 0; k < 4; ++k) tpl[k] |= v;
			else if (degree == 6)
				tpl[1] |= v;
			else if (degree == 7)
			{
				tpl[2] |= v;
				tpl[3] |= Bit(index == 0 ? 23 : index - 1);
			}
		}
		if (++index >= 24) index = 0;
	}
	for (int k = 0; k < 4; ++k)
		if ((tpl[k] & mask) == mask) return false;
	return true;
}

uint16_t Classify(const SectionChord &c, const SectionChord *pNext, const Harmonic &base)
{
	const uint32_t scale = c.m_ScalePattern ? c.m_ScalePattern : base.m_ChordMask;
	uint16_t flags = (c.m_ChordPattern & 0x91) == 0x91 ? 0x100
				   : (c.m_ChordPattern & 0x89) == 0x89 ? 0x200 : 0x400;
	int degree = -1;
	bool bOff = false;
	for (int i = 0; i <= ((int)c.m_nRoot - (int)base.m_nChordRoot + 36) % 12; ++i)
	{
		bOff = !(scale & Bit(i));
		if (!bOff) ++degree;
	}
	flags |= bOff ? 0x80 : (uint16_t)Bit(std::max(0, degree));
	if (pNext && pNext->m_nBeat == 0)
	{
		degree = 0;
		// Keeps bOff if a negative remainder skips the loop.
		for (int i = 0; i <= ((int)pNext->m_nRoot - (int)base.m_nChordRoot + 36) % 12; ++i)
		{
			bOff = !(scale & Bit(i));
			if (!bOff) ++degree;
		}
		if (!bOff) flags |= degree == 1 ? 0x800 : degree == 5 ? 0x1000 : 0;
	}
	flags |= OutsideTemplates(c, base) ? 0x4000 : 0x2000;
	return flags;
}

void PrepareHarmony(const Section &sec, const Timeline &t, uint32_t nPersScale,
					std::vector<PreparedChord> &out)
{
	Harmonic base;
	base.m_ChordMask = nPersScale;
	base.m_ScaleMask = nPersScale;
	base.m_nChordRoot = (int8_t)(sec.m_DefaultChord & 127);
	base.m_nScaleRoot = (int8_t)(sec.m_DefaultChord & 127);
	base.m_ChordFlags = PitchFlags(nPersScale);

	out.clear();
	for (size_t i = 0; i < t.m_ChordSource.size(); ++i)
	{
		const SectionChord &c = sec.m_Chords[t.m_ChordSource[i]];
		const SectionChord *pNext = i + 1 < t.m_ChordSource.size()
			? &sec.m_Chords[t.m_ChordSource[i + 1]] : 0;
		PreparedChord pc;
		pc.m_H.m_ChordMask = c.m_ChordPattern;
		pc.m_H.m_ScaleMask = c.m_ScalePattern ? c.m_ScalePattern : nPersScale;
		pc.m_H.m_nChordRoot = c.m_nRoot;
		pc.m_H.m_nScaleRoot = base.m_nScaleRoot;
		pc.m_H.m_ChordFlags = (uint8_t)(PitchFlags(c.m_ChordPattern) | (c.m_Flags & 8 ? 128 : 0));
		pc.m_VariationFlags = Classify(c, pNext, base);
		out.push_back(pc);
	}
}

// ------------------------------------------------------------- selection ---

struct PatternRequest	{ int16_t m_nMeasures; uint32_t m_Flags; };
struct SectionSelect
{
	int16_t			m_nMeasure;
	size_t			m_nChord;
	uint16_t		m_MuteMask;
	PatternRequest	m_Req;
};

void SelectSection(const Section &s, const Timeline &t, int32_t tick, SectionSelect &out)
{
	out.m_nMeasure = Word(tick / t.m_nTicksPerMeasure);
	out.m_nChord = 0;
	// The first entry is selected even before its time.
	while (out.m_nChord + 1 < t.m_ChordTimes.size() && t.m_ChordTimes[out.m_nChord + 1] <= tick)
		++out.m_nChord;
	out.m_MuteMask = 0;
	if (!t.m_MuteTimes.empty())
	{
		size_t i = 0;
		while (i + 1 < t.m_MuteTimes.size() && t.m_MuteTimes[i + 1] <= tick) ++i;
		out.m_MuteMask = t.m_MuteValues[i];
	}

	PatternRequest &r = out.m_Req;
	r.m_nMeasures = Word((int32_t)s.m_nMeasures - out.m_nMeasure);
	r.m_Flags = 0x200;
	size_t cmd = 0;
	for (; cmd < t.m_CommandTimes.size() && t.m_CommandTimes[cmd] <= tick; ++cmd)
	{
		const uint8_t *v = s.m_Commands[cmd].m_Value;
		const uint32_t flags = (uint32_t)v[0] | ((uint32_t)v[1] << 8) |
							   ((uint32_t)v[2] << 16) | ((uint32_t)v[3] << 24);
		if (flags & 0x13f80) r.m_Flags = flags & 0x13f80;
		if (t.m_CommandTimes[cmd] == tick)
		{
			r.m_Flags |= flags & 0x63;
			break;						// the first equal command, not the last
		}
	}
	if (cmd < t.m_CommandTimes.size())
	{
		if (t.m_CommandTimes[cmd] <= tick) ++cmd;
		if (cmd < t.m_CommandTimes.size())
			r.m_nMeasures = Word(t.m_CommandTimes[cmd] / t.m_nTicksPerMeasure - out.m_nMeasure);
	}
	for (size_t i = out.m_nChord; i < t.m_ChordTimes.size(); ++i)
	{
		if (t.m_ChordTimes[i] < tick) continue;
		const SectionChord &c = s.m_Chords[t.m_ChordSource[i]];
		const int16_t delta = std::max<int16_t>(0, Word((int32_t)c.m_nMeasure - out.m_nMeasure));
		if (delta)
		{
			r.m_nMeasures = std::min(r.m_nMeasures, delta);
			if (r.m_Flags & 8) { r.m_nMeasures = 1; break; }
		}
		if (!c.m_nBeat) continue;
		if (delta)
		{
			r.m_nMeasures = std::min(r.m_nMeasures, delta);
			r.m_Flags = (r.m_Flags & ~0x1cu) | 4;
			break;
		}
		if (c.m_nBeat & 1)
		{
			r.m_nMeasures = 1;
			r.m_Flags = (r.m_Flags & ~0x1cu) | 0x10;
			break;
		}
		r.m_Flags = (r.m_Flags & ~0x1cu) | 8;
	}
	if (!(r.m_Flags & 0x18)) r.m_Flags |= 4;
	r.m_nMeasures = std::min<int16_t>(r.m_nMeasures, 12);		// no lower clamp
}

// Flags must all match and the 0x63 embellishment bits must be equal; the
// longest pattern that fits wins, else the last that matched, else the first.
size_t SelectPattern(const Style &sty, const PatternRequest &req)
{
	bool bLast = false, bBest = false;
	size_t nLast = 0, nBest = 0;
	int16_t nLen = 0;
	if (req.m_nMeasures > 0)
		for (size_t i = 0; i < sty.m_Patterns.size(); ++i)
		{
			const Pattern &p = sty.m_Patterns[i];
			if ((p.m_Flags & req.m_Flags) != req.m_Flags || ((p.m_Flags ^ req.m_Flags) & 0x63))
				continue;
			bLast = true;
			nLast = i;
			if (p.m_Timing[3] > nLen && p.m_Timing[3] <= req.m_nMeasures)
			{
				bBest = true;
				nBest = i;
				nLen = p.m_Timing[3];
			}
		}
	return bBest ? nBest : bLast ? nLast : 0;
}

// ------------------------------------------------------------ variations ---

// The loader's defaults: per part, bits 0x0800 and 0x1000 are added to every
// variation unless some variation of that part already carries them; a zero
// play mode becomes 1 on part 5, 2 on part 4 and 3 elsewhere.
struct Channels
{
	uint16_t	m_Flags[16][16];
	uint8_t		m_PlayModes[16];
};

void PrepareChannels(const Pattern &p, Channels &out)
{
	memcpy(out.m_Flags, p.m_VariationFlags, sizeof out.m_Flags);
	memcpy(out.m_PlayModes, p.m_PlayModes, sizeof out.m_PlayModes);
	for (int ch = 0; ch < 16; ++ch)
	{
		uint16_t missing = 0x1800;
		for (int v = 0; v < 16; ++v) missing &= (uint16_t)~out.m_Flags[ch][v];
		for (int v = 0; v < 16; ++v) out.m_Flags[ch][v] |= missing;
		if (!out.m_PlayModes[ch])
			out.m_PlayModes[ch] = ch == 5 ? 1 : ch == 4 ? 2 : 3;
	}
}

// A variation is eligible when it carries every required bit; with none
// eligible all sixteen are.  A random part draws once, even for one candidate,
// in ascending part order; a cycle part rotates left; any other part holds.
// Cycle and hold do not consult eligibility.
void ChooseVariations(const Channels &ch, uint16_t required, uint16_t cur[16],
					  uint16_t random, uint16_t cycle, Random &rng, uint32_t &nDraws)
{
	for (int c = 0; c < 16; ++c)
	{
		const uint16_t bit = (uint16_t)(1u << c);
		uint16_t cand = 0;
		for (int v = 0; v < 16; ++v)
			if ((ch.m_Flags[c][v] & required) == required) cand |= (uint16_t)(1u << v);
		if (!cand) cand = 0xffff;
		if (random & bit)
		{
			int index = rng.Draw() % PopCount(cand);
			++nDraws;
			for (int v = 0; v < 16; ++v)
				if ((cand & (1u << v)) && index-- == 0)
				{
					cur[c] = (uint16_t)(1u << v);
					break;
				}
		}
		else if (cycle & bit)
			cur[c] = (cur[c] & 0x8000) ? 1 : (uint16_t)(cur[c] << 1);
	}
}

// ----------------------------------------------------------------- pitch ---

uint32_t Sar(uint32_t mask, unsigned count = 1)
{
	return (uint32_t)((int32_t)mask >> (count & 31));
}

uint8_t HarmonicPitch(uint16_t music, uint8_t modifier, const Harmonic &h, uint8_t flags,
					  bool &bZero)
{
	bZero = false;
	uint8_t degree = (uint8_t)((music >> 8) & 15);
	uint8_t steps = (uint8_t)((music >> 4) & 15);
	int chromatic = music & 15;
	if (chromatic > 8) chromatic -= 16;
	const uint8_t base = (uint8_t)((music >> 12) * 12 + h.m_nChordRoot);
	uint8_t pitch = base;
	const int threshold = flags & 0x20 ? 4 : 3;
	if ((flags & 15) > threshold) degree = (uint8_t)(degree + (flags & 15) - threshold);
	uint32_t chord = h.m_ChordMask;
	while (chord)
	{
		if (chord & 1)
		{
			if (!degree) break;
			--degree;
		}
		chord = Sar(chord);
		if (!chord)
		{
			if (!modifier) { bZero = true; return 0; }
			pitch = base;
			steps = (uint8_t)(SByte(modifier) >> 4);
			chromatic = modifier & 15;		// the fallback keeps this unsigned
			break;
		}
		++pitch;
	}
	if (steps)
	{
		uint32_t scale = h.m_ScaleMask | (h.m_ScaleMask << 12);
		const int rem = ((int)pitch - h.m_nScaleRoot) % 12;
		scale = Sar(scale, (unsigned)rem);
		while (scale)
		{
			if (scale & 1)
			{
				if (!steps) break;
				--steps;
			}
			scale = Sar(scale);
			++pitch;
		}
	}
	return (uint8_t)(pitch + chromatic);
}

bool ScalePitch(const Note &n, const Harmonic &h, int16_t &out)
{
	uint16_t remaining = (uint16_t)(SByte(n.m_PitchModifier) >> 4);
	uint32_t scale = h.m_ScaleMask | (h.m_ScaleMask << 12);
	int pitch = (n.m_MusicValue >> 12) * 12 + h.m_nScaleRoot;
	while (remaining)
	{
		if (scale == 0) return false;
		if (scale == 0xffffffffu) { pitch += remaining; break; }
		scale = Sar(scale);
		if (scale & 1) --remaining;
		++pitch;
	}
	int chromatic = n.m_PitchModifier & 15;
	if (chromatic > 8) chromatic -= 16;
	out = Word(pitch + chromatic + 12);
	return true;
}

bool ResolvePitch(const Note &n, const Pattern &p, const Harmonic &h, int16_t &out)
{
	const int8_t raw = SByte(n.m_MidiNote);
	if (!n.m_MusicValue) { out = raw; return true; }

	uint8_t mode = n.m_PlayMode;
	if ((mode & 15) == 7 || !(mode & 0x80))
	{
		mode = p.m_PlayModes[n.m_nChannel];
		if (!mode) mode = n.m_nChannel == 5 ? 1 : n.m_nChannel == 4 ? 2 : 3;
	}
	mode &= 15;
	int16_t pitch = raw;
	if (mode == 4)
	{
		if (!ScalePitch(n, h, pitch)) return false;
	}
	else if (mode != 1)
	{
		uint16_t music = n.m_MusicValue;
		if (mode == 5 || mode == 6)
			music = (uint16_t)((music & 0xf00f) | (((music & 0x0f00) >> 3) + (music & 0x00f0)));
		const bool bFlags = mode != 2 && mode != 5;
		bool bZero;
		pitch = HarmonicPitch(music, n.m_PitchModifier, h, bFlags ? h.m_ChordFlags : 0, bZero);
		if (bFlags && (h.m_ChordFlags & 0x40))
		{
			pitch -= 12;
			if (pitch <= 0) pitch += 12;
		}
	}
	if (pitch)
	{
		pitch = Word(pitch - 12);
		if ((p.m_RangeChannels & (1u << n.m_nChannel)) && (h.m_ChordFlags & 0x10))
		{
			const int upper = p.m_Upper[n.m_nChannel], lower = p.m_Lower[n.m_nChannel];
			if (upper < pitch)
				pitch = Word(pitch - ((uint16_t)(pitch - upper + 11) / 12) * 12);
			if (lower > pitch)
				pitch = Word(pitch + ((uint16_t)(lower - pitch + 11) / 12) * 12);
		}
	}
	out = pitch;
	return true;
}

// ----------------------------------------------------------- note events ---

// One draw for each nonzero range, in the order time, duration, velocity; none
// for a zero pitch.  The original idiv remainder takes the nonnegative
// numerator's sign; the half is a sar, so it rounds down.
int Jitter(uint8_t raw, Random &rng, uint32_t &nDraws)
{
	if (!raw) return 0;
	const int range = SByte(raw);
	const int half = range < 0 ? -((-range + 1) / 2) : range / 2;
	++nDraws;
	return rng.Draw() % (range < 0 ? -range : range) - half;
}

bool NoteEvents(const Note &n, int32_t gridTime, int16_t resolved, int8_t transpose,
				Random &rng, uint32_t &nDraws, Event &on, Event &off)
{
	if (!resolved) return false;
	int pitch = Word(resolved + transpose);
	if (pitch < 1) pitch += ((12 - pitch) / 12) * 12;
	if (pitch > 127) pitch -= ((pitch - 116) / 12) * 12;

	const int32_t start = gridTime + n.m_nTimeOffset + Jitter(n.m_TimeRange, rng, nDraws);
	const int dur = std::max(1, (int)n.m_nDuration + Jitter(n.m_DurationRange, rng, nDraws));
	const int vel = std::min(127, std::max(1, (int)SByte(n.m_Velocity) +
											  Jitter(n.m_VelocityRange, rng, nDraws)));
	const uint8_t ch = (uint8_t)((n.m_nChannel + 4u) & 15u);
	on.m_nTick = start;
	on.m_Status = (uint8_t)(0x90 | ch);
	on.m_Data1 = (uint8_t)pitch;
	on.m_Data2 = (uint8_t)vel;
	off = on;
	off.m_nTick = start + dur;
	off.m_Status = (uint8_t)(0x80 | ch);
	return true;
}

// ---------------------------------------------------------------- curves ---

int CurveTable(uint8_t shape, int index)
{
	static int s_Log[193], s_Sine[193];
	static bool s_bReady = false;
	if (!s_bReady)
	{
		const double kPi = 3.14159265358979323846;
		for (int i = 0; i <= 192; ++i)
		{
			s_Log[i] = (int)(19200 * log10(1.0 + 9.0 * i / 192));
			s_Sine[i] = (int)(9600 * (1 - cos(kPi * i / 192)));
		}
		s_Sine[96] = 9600;		// the exact boundary; libm can land just below
		s_bReady = true;
	}
	switch (shape)
	{
	case 2:  return 19200 - s_Log[192 - index];
	case 3:  return s_Log[index];
	case 4:  return s_Sine[index];
	default: return 100 * index;
	}
}

// A style curve expanded to messages.  Types 3..6 are pitch bend, controller,
// channel pressure, key pressure.  Unchanged endpoints are omitted, and the
// last message is replaced when it already sits at the endpoint.
void CurveEvents(const Curve &cv, int32_t tick, std::vector<Event> &out)
{
	if (cv.m_nType < 3 || cv.m_nType > 6 || cv.m_nChannel >= 16) return;
	const uint8_t status = (uint8_t)((cv.m_nType == 3 ? 0xe0 : cv.m_nType == 4 ? 0xb0
									: cv.m_nType == 5 ? 0xd0 : 0xa0) | ((cv.m_nChannel + 4) & 15));
	const size_t nFirst = out.size();
	struct Emit
	{
		const Curve &cv; uint8_t status; int32_t tick; std::vector<Event> &out; size_t nFirst;
		void operator()(int time, int value, bool bReplace = false)
		{
			const uint16_t bits = (uint16_t)value;
			Event e;
			e.m_nTick = tick + time;
			e.m_Status = status;
			e.m_Data1 = (uint8_t)(cv.m_nType == 3 ? bits & 127 : cv.m_nController);
			e.m_Data2 = (uint8_t)(cv.m_nType == 3 ? (bits >> 7) & 254 : bits & 255);
			if (bReplace && out.size() > nFirst) out.back() = e;
			else out.push_back(e);
		}
	} emit = { cv, status, tick, out, nFirst };

	for (size_t k = 0; k < cv.m_Points.size(); ++k)
	{
		const CurvePoint &pt = cv.m_Points[k];
		if (pt.m_nShape == 1)
		{
			emit(pt.m_nStart, pt.m_Flags & 2 ? pt.m_nValue : pt.m_nTarget);
			continue;
		}
		const int delta = Word((int)pt.m_nTarget - pt.m_nValue);
		const int dur = Word((int)pt.m_nEnd - pt.m_nStart);
		struct ValueAt
		{
			const CurvePoint &pt; int delta;
			int operator()(int index) const
			{
				const int v = Word(pt.m_nValue + CurveTable(pt.m_nShape, index) * delta / 19200);
				return pt.m_Flags & 2 ? Word(pt.m_nValue - v + pt.m_nTarget) : Word(v);
			}
		} valueAt = { pt, delta };
		int lastValue = valueAt(pt.m_Flags & 1 ? 192 : 0);
		int lastTime = pt.m_nStart;
		emit(lastTime, lastValue);
		for (int i = 1; i < 192; ++i)
		{
			const int time = Word(pt.m_nStart + i * dur / 192);
			if (time - lastTime < 3) continue;
			const int value = valueAt(pt.m_Flags & 1 ? 192 - i : i);
			if (value == lastValue || time == lastTime) continue;
			emit(time, value);
			lastTime = time;
			lastValue = value;
		}
		const int finalValue = valueAt(pt.m_Flags & 1 ? 0 : 192);
		if (finalValue != lastValue) emit(pt.m_nEnd, finalValue, pt.m_nEnd == lastTime);
	}
}

// ------------------------------------------------------------- the cursor ---

bool ChordContains(uint8_t key, const Harmonic &h)
{
	// Unsigned word comparison, low-word subtraction, x86 shift masking.
	uint16_t degree = (uint16_t)(key % 12);
	const uint16_t root = (uint16_t)h.m_nChordRoot;
	if (root > degree) degree = (uint16_t)(degree + 12);
	degree = (uint16_t)(degree - root);
	uint32_t mask = h.m_ChordMask | (h.m_ChordMask << 12);
	mask |= mask << 12;
	return (mask & ((uint32_t)1 << (degree & 31))) != 0;
}

struct ActiveNote	{ Event m_Off; uint16_t m_nSerial; };

int16_t GridCount(const Pattern &p)
{
	return Word((int32_t)(uint32_t)(uint16_t)(p.m_nLength / p.m_Timing[0]));
}

// ------------------------------------------------------------- band setup --

// The loaded section's last band after its header wins; else the style's last
// band after its header with nonzero byte 104.
const Band *SelectBand(const std::vector<Band> &secBands, uint32_t nSecHeader, const Style &sty)
{
	for (size_t i = secBands.size(); i > 0; --i)
		if (secBands[i - 1].m_nOffset > nSecHeader) return &secBands[i - 1];
	for (size_t i = sty.m_Bands.size(); i > 0; --i)
		if (sty.m_Bands[i - 1].m_nOffset > sty.m_nHeaderOffset && sty.m_Bands[i - 1].m_nByte104 != 0)
			return &sty.m_Bands[i - 1];
	return 0;
}

bool BandSetup(const Band &band, std::vector<Event> &out)
{
	for (unsigned i = 0; i < 16; ++i)
	{
		uint32_t patch = ((uint32_t)band.m_Bank[i] << 7) | band.m_Program[i];
		if (!band.m_bHasCollection && !(patch & 0x200000)) patch |= 0x400000;
		if (i == 5) patch |= 0x80000000u;
		const bool bLegacy = (patch & 0x600000) == 0x400000;
		const uint8_t program = bLegacy ? band.m_LegacyProgram[i] : (uint8_t)(patch & 127);
		if (band.m_Volume[i] > 127 || band.m_Pan[i] > 127 || program > 127) return false;
		const uint8_t ch = (uint8_t)((i + 4) & 15);
		const uint8_t cc = (uint8_t)(0xb0 | ch);
		Event e = { 0, cc, 7, band.m_Volume[i] };					out.push_back(e);
		e.m_Data1 = 10; e.m_Data2 = band.m_Pan[i];					out.push_back(e);
		e.m_Data1 = 0;  e.m_Data2 = (uint8_t)(bLegacy ? 0 : (patch >> 14) & 127);	out.push_back(e);
		e.m_Data1 = 32; e.m_Data2 = (uint8_t)(bLegacy ? 0 : (patch >> 7) & 127);	out.push_back(e);
		e.m_Status = (uint8_t)(0xc0 | ch); e.m_Data1 = program; e.m_Data2 = 0;		out.push_back(e);
	}
	return true;
}

// -------------------------------------------------------------- the files --

std::string FindFileNamed(FileProvider &provider, const std::string &sWantLower)
{
	std::vector<std::string> all;
	provider.List(all);
	for (size_t i = 0; i < all.size(); ++i)
		if (ToLower(all[i]) == sWantLower) return all[i];
	return std::string();
}

std::string Trim(const std::string &s)
{
	size_t a = 0, b = s.size();
	while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r')) ++a;
	while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r')) --b;
	return s.substr(a, b - a);
}

// [Styles] NameN -> FileN from iMusic.ini, lower-cased names.
std::map<std::string, std::string> StyleCatalogue(FileProvider &provider)
{
	std::map<std::string, std::string> out, names, files;
	std::string sIni = FindFileNamed(provider, "imusic.ini");
	Buf ini;
	if (sIni.empty() || !provider.Read(sIni.c_str(), ini)) return out;
	std::string sText(ini.begin(), ini.end()), sSection;
	size_t p = 0;
	while (p < sText.size())
	{
		size_t e = sText.find_first_of("\r\n", p);
		if (e == std::string::npos) e = sText.size();
		std::string s = Trim(sText.substr(p, e - p));
		p = e + 1;
		if (s.size() >= 2 && s[0] == '[' && s[s.size() - 1] == ']')
		{
			sSection = ToLower(s.substr(1, s.size() - 2));
			continue;
		}
		if (sSection != "styles") continue;
		size_t eq = s.find('=');
		if (eq == std::string::npos) continue;
		std::string k = ToLower(Trim(s.substr(0, eq))), v = Trim(s.substr(eq + 1));
		if (k.compare(0, 4, "name") == 0) names[k.substr(4)] = v;
		else if (k.compare(0, 4, "file") == 0) files[k.substr(4)] = v;
	}
	for (std::map<std::string, std::string>::iterator it = names.begin(); it != names.end(); ++it)
	{
		std::map<std::string, std::string>::iterator f = files.find(it->first);
		if (f != files.end()) out[ToLower(it->second)] = f->second;
	}
	return out;
}

std::string FindStyleFile(FileProvider &provider, const std::string &sStyle)
{
	std::map<std::string, std::string> cat = StyleCatalogue(provider);
	std::map<std::string, std::string>::iterator it = cat.find(ToLower(sStyle));
	if (it != cat.end())
	{
		std::string sReal = FindFileNamed(provider, ToLower(it->second));
		return sReal.empty() ? it->second : sReal;
	}
	return FindFileNamed(provider, ToLower(sStyle) + ".sty");
}

std::string FindDls(FileProvider &provider)
{
	std::vector<std::string> all, dls;
	provider.List(all);
	for (size_t i = 0; i < all.size(); ++i)
	{
		std::string l = ToLower(all[i]);
		if (l.size() > 4 && l.compare(l.size() - 4, 4, ".dls") == 0) dls.push_back(all[i]);
	}
	std::sort(dls.begin(), dls.end());
	return dls.size() == 1 ? dls[0] : std::string();
}

} // namespace

// ------------------------------------------------------------ the program --

uint32_t TickToMilliseconds(const Program &prog, int32_t nTick)
{
	const int t = prog.m_nTempo;
	const uint32_t tempo16 = t < 10 ? 160u : t >= 350 ? 5600u
		: ((uint32_t)t << 4) | (prog.m_nTempoFraction >> 12);
	if (nTick < 0) nTick = 0;
	// MulDiv, nearest, halves up.
	return (uint32_t)(((uint64_t)nTick * 5000u + tempo16 / 2) / tempo16);
}

uint32_t ExportMicrosecondsPerQuarter(const Program &prog)
{
	const uint32_t tempo64 = ((uint32_t)(uint16_t)prog.m_nTempo << 6) | (prog.m_nTempoFraction >> 10);
	return tempo64 ? 3840000000u / tempo64 : 500000u;
}

Program Sequence(FileProvider &provider, const char *pszSecName, const Options &opts)
{
	Random rng(opts.m_nSeed);
	return SequenceWith(provider, pszSecName, opts, rng);
}

Program SequenceWith(FileProvider &provider, const char *pszSecName,
					 const Options &opts, Random &rng)
{
	Program prog;

	// ---- the files
	Buf secBuf;
	if (!provider.Read(pszSecName, secBuf))
	{
		prog.m_sError = std::string("cannot read ") + pszSecName;
		return prog;
	}
	std::vector<Chunk> secChunks;
	if (!ReadChunks(secBuf, "AASE", secChunks))
	{
		prog.m_sError = "not a RIFF AASE section";
		return prog;
	}
	Section sec;
	if (!ReadSection(secBuf, secChunks, sec, prog.m_sError)) return prog;
	if (sec.m_nOpaqueNotes)
	{
		prog.m_sError = "section-level notes or curves are not supported";
		return prog;
	}
	std::vector<Band> secBands;
	if (!ReadBands(secBuf, secChunks, secBands))
	{
		prog.m_sError = "bad section band";
		return prog;
	}

	prog.m_sStyleName = FindStyleFile(provider, sec.m_sStyle);
	Buf styBuf;
	Style sty;
	if (prog.m_sStyleName.empty() || !provider.Read(prog.m_sStyleName.c_str(), styBuf))
	{
		prog.m_sError = "cannot find style '" + sec.m_sStyle + "'";
		return prog;
	}
	if (!ReadStyle(styBuf, sty, prog.m_sError)) return prog;
	if (sty.m_Patterns.empty())
	{
		prog.m_sError = "style has no patterns";
		return prog;
	}
	for (size_t i = 0; i < sty.m_Patterns.size(); ++i)
	{
		const Pattern &p = sty.m_Patterns[i];
		if (p.m_Timing[0] <= 0 || p.m_nLength <= 0 || GridCount(p) <= 0)
		{
			prog.m_sError = "pattern does not progress";
			return prog;
		}
		for (size_t g = 0; g < p.m_Grids.size(); ++g)
			for (size_t n = 0; n < p.m_Grids[g].m_Notes.size(); ++n)
				if (p.m_Grids[g].m_Notes[n].m_nChannel >= 16)
				{
					prog.m_sError = "note on an invalid part";
					return prog;
				}
	}

	std::string sPer = FindFileNamed(provider, ToLower(sec.m_sPersonality) + ".per");
	Buf perBuf;
	uint32_t nPersScale = 0;
	if (sPer.empty() || !provider.Read(sPer.c_str(), perBuf) ||
		!ReadPersonalityScale(perBuf, nPersScale))
	{
		prog.m_sError = "cannot read personality '" + sec.m_sPersonality + "'";
		return prog;
	}

	prog.m_sDlsName = FindDls(provider);
	if (prog.m_sDlsName.empty())
	{
		prog.m_sError = "expected exactly one .dls beside the section";
		return prog;
	}

	const Band *pBand = SelectBand(secBands, sec.m_nHeaderOffset, sty);
	if (!pBand)
	{
		prog.m_sError = "no band in the section, and none marked in the style";
		return prog;
	}

	Timeline tl;
	if (!PrepareTimeline(sec, sty, tl, prog.m_sError)) return prog;
	if (tl.m_ChordTimes.empty())
	{
		prog.m_sError = "section has no chord";
		return prog;
	}
	std::vector<PreparedChord> chords;
	PrepareHarmony(sec, tl, nPersScale, chords);

	prog.m_nTempo = sec.m_nTempo;
	prog.m_nTempoFraction = sec.m_nTempoFraction;

	// 1 + secn +0x26 passes, the output clock running on across them while the
	// section position starts again. Every demo section has 0 repeats, so the repeat
	// path is read from the code and not yet compared against the original.
	const int nPasses = 1 + std::max<int>(0, sec.m_nRepeats);
	if (nPasses > 64) { prog.m_sError = "section repeats too often"; return prog; }
	prog.m_nDurationTicks = tl.m_nDuration * nPasses;
	if (!BandSetup(*pBand, prog.m_Events))
	{
		prog.m_sError = "band volume, pan or program out of MIDI range";
		return prog;
	}
	const size_t nSetup = prog.m_Events.size();

	// ---- the cursor
	int32_t tick = 0;			// the section position
	int32_t base = 0;			// output ticks before this pass
	int nPass = 0;
	int16_t grid = 0;
	uint16_t serial = 0;
	size_t nPattern = 0;
	uint16_t variations[16];
	for (int c = 0; c < 16; ++c) variations[c] = opts.m_InitialVariation;
	uint32_t prevChord = 0;
	int8_t prevRoot = 0;
	std::vector<ActiveNote> active;
	const uint32_t kGridLimit = 65536;

	for (;;)
	{
		if (prog.m_nGrids == kGridLimit)
		{
			prog.m_sError = "grid budget exceeded";
			prog.m_Events.clear();
			return prog;
		}

		SectionSelect sel;
		SelectSection(sec, tl, tick, sel);
		const PreparedChord &chord = chords[sel.m_nChord];
		const bool bChordChanged = chord.m_H.m_ChordMask != prevChord ||
								   chord.m_H.m_nChordRoot != prevRoot;

		// The ordinal is captured BEFORE a command resets the pattern cursor.
		const int16_t nOrdinal = grid;
		for (size_t i = 0; i < tl.m_CommandTimes.size(); ++i)
		{
			if (tl.m_CommandTimes[i] > tick) break;
			if (tl.m_CommandTimes[i] == tick) { grid = 0; break; }
		}
		if (grid == 0)
		{
			nPattern = SelectPattern(sty, sel.m_Req);
			if (prog.m_nPatternsSelected++ == 0) prog.m_nFirstPattern = (uint32_t)nPattern;
		}
		const Pattern &pat = sty.m_Patterns[nPattern];
		const int16_t interval = pat.m_Timing[0];
		const int16_t nGrids = GridCount(pat);

		if (grid == 0 || bChordChanged)
		{
			Channels ch;
			PrepareChannels(pat, ch);
			ChooseVariations(ch, chord.m_VariationFlags, variations,
							 opts.m_RandomChannels, opts.m_CycleChannels, rng, prog.m_nDraws);
		}

		// Normal modes 2/3 turn harmonic conversion on.
		std::vector<Event> dispatch;
		std::vector<ActiveNote> fresh;
		if ((size_t)nOrdinal < pat.m_Grids.size())
		{
			const Grid &g = pat.m_Grids[(size_t)nOrdinal];
			for (size_t it = 0; it < g.m_Items.size(); ++it)
			{
				const GridItem &item = g.m_Items[it];
				for (uint32_t k = item.m_nFirst; k < item.m_nFirst + item.m_nCount; ++k)
				{
					if (!item.m_bCurves)
					{
						const Note &n = g.m_Notes[k];
						if ((sel.m_MuteMask & (1u << n.m_nChannel)) ||
							!(variations[n.m_nChannel] & n.m_Variations))
							continue;
						if (n.m_nType != 8 && n.m_nType != 9) continue;
						int16_t pitch = 0;
						if (!ResolvePitch(n, pat, chord.m_H, pitch))
						{
							prog.m_sError = "scale-only pitch walk does not terminate";
							prog.m_Events.clear();
							return prog;
						}
						if (pitch && pitch != SByte(n.m_MidiNote)) ++prog.m_nPitchChanged;
						Event on, off;
						if (!NoteEvents(n, base + tick, pitch, pBand->m_Transpose[n.m_nChannel],
										rng, prog.m_nDraws, on, off))
							continue;
						dispatch.push_back(on);
						ActiveNote a = { off, serial };
						fresh.push_back(a);
						++prog.m_nNotes;
					}
					else
					{
						const Curve &cv = g.m_Curves[k];
						if (cv.m_nChannel >= 16) continue;
						if ((sel.m_MuteMask & (1u << cv.m_nChannel)) ||
							!(variations[cv.m_nChannel] & cv.m_Variations))
							continue;
						const size_t nBefore = dispatch.size();
						CurveEvents(cv, base + tick, dispatch);
						prog.m_nCurveMessages += (uint32_t)(dispatch.size() - nBefore);
					}
				}
			}
		}

		// New offs are prepended before the chord-change check, so pending is
		// the fresh offs newest-first, then the held ones.
		const int32_t horizon = base + tick + interval;
		std::vector<ActiveNote> pending(fresh.rbegin(), fresh.rend());
		pending.insert(pending.end(), active.begin(), active.end());
		std::vector<Event> offs;
		std::vector<ActiveNote> keep;
		for (size_t i = 0; i < pending.size(); ++i)
		{
			ActiveNote a = pending[i];
			if (bChordChanged && a.m_nSerial != serial && !ChordContains(a.m_Off.m_Data1, chord.m_H))
				a.m_Off.m_nTick = base + tick + 1;		// assigned, not min()'d
			if (a.m_Off.m_nTick <= horizon) offs.push_back(a.m_Off);
			else keep.push_back(a);
		}
		// Both partitions are built by head insertion in the original.
		std::reverse(offs.begin(), offs.end());
		std::reverse(keep.begin(), keep.end());
		active.swap(keep);

		prog.m_Events.insert(prog.m_Events.end(), dispatch.begin(), dispatch.end());
		prog.m_Events.insert(prog.m_Events.end(), offs.begin(), offs.end());
		++prog.m_nGrids;

		prevChord = chord.m_H.m_ChordMask;
		prevRoot = chord.m_H.m_nChordRoot;
		tick += interval;
		++grid;
		++serial;
		if (tick >= tl.m_nDuration)
		{
			// The end of a pass zeroes the position, the serial and the grid.
			if (++nPass >= nPasses) break;
			base += tl.m_nDuration;
			tick = 0;
			grid = 0;
			serial = 0;
			continue;
		}
		if (grid >= nGrids) grid = 0;
	}

	// The remaining list goes out in its current order; end of section does
	// not truncate a held note.
	for (size_t i = 0; i < active.size(); ++i)
		prog.m_Events.push_back(active[i].m_Off);

	// A positive epoch keeps anticipations instead of clipping them; the band
	// setup stays at tick 0.
	int32_t earliest = 0;
	for (size_t i = nSetup; i < prog.m_Events.size(); ++i)
		earliest = std::min(earliest, prog.m_Events[i].m_nTick);
	prog.m_nPrerollTicks = -earliest;
	prog.m_nLastTick = prog.m_nDurationTicks + prog.m_nPrerollTicks;
	for (size_t i = nSetup; i < prog.m_Events.size(); ++i)
	{
		prog.m_Events[i].m_nTick += prog.m_nPrerollTicks;
		prog.m_nLastTick = std::max(prog.m_nLastTick, prog.m_Events[i].m_nTick);
	}
	prog.m_bOk = true;
	return prog;
}

} // namespace imaseq
