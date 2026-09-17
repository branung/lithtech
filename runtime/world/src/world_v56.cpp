// LithTech 1.0 world (DAT v56) to Jupiter v85 image

#include "world_v56.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

// stricmp is MSVC's spelling of strcasecmp
#ifndef _WIN32
#include <strings.h>
#define stricmp strcasecmp
#endif

#include <map>
#include <set>
#include <string>
#include <vector>
#include <algorithm>

// LT1 surface flags
static const uint32 kSurfNonExistent = 1u << 1;	// Preprocessor drops these
static const uint32 kSurfInvisible = 1u << 2;	// Don't draw
static const uint32 kSurfTransparent = 1u << 3;	// Alpha-blended (glass, grates, sky layers)
static const uint32 kSurfSky = 1u << 4;			// Sky portal, not geometry
static const uint32 kSurfLightMap = 1u << 7;
static const uint32 kSurfPanningSky = 1u << 15; // Sky shadow pan overlaid on this surface

// Property type code for a string.
// Used to pick out the sky objects' names
static const uint8 kPropTypeString = 0;

// ASCII lowercase for the sky object name match because tolower() is locale dependent
static std::string ToLower(const std::string &s)
{
	std::string out(s);
	for (size_t i = 0; i < out.size(); ++i)
	{
		if (out[i] >= 'A' && out[i] <= 'Z')
			out[i] = (char)(out[i] - 'A' + 'a');
	}
	return out;
}

// ePCShader_* codes written into each render section
static const uint8 kShaderGouraud = 1;
static const uint8 kShaderLightmap = 2;			// Base pass: the section's lightmap page
static const uint8 kShaderLightmapTexture = 4;	// Texture pass
// The sky shadow pan's base pass
static const uint8 kShaderSkypan = 5;
static const uint8 kShaderGouraudTranslucent = 12;

// Lightmap atlas pages.
// A v85 section carries one lightmap, so the per poly LT1 lightmaps are packed into pages of fixed width.
static const uint32 kLMPageW = 128;
static const uint32 kLMPageMaxH = 256;

// Jupiter's world info flags
static const uint32 kWifMoveable = 1u << 1;
static const uint32 kWifMainWorld = 1u << 2;
static const uint32 kWifPhysicsBsp = 1u << 4;

// LT1's object light grid, rebulit in the same order with the same rounding
static const double kLightGridCell = 350.0;
static const double kLightGridMargin = 300.0;

namespace {

class Reader
{
public:
	Reader(const uint8 *pData, uint32 nSize) : m_pData(pData), m_nSize(nSize), m_nPos(0), m_bBad(false) {}

	bool Bad() const { return m_bBad; }
	uint32 Pos() const { return m_nPos; }
	void SeekTo(uint32 nPos) { if (nPos > m_nSize) m_bBad = true; else m_nPos = nPos; }

	const uint8 *Raw(uint32 n)
	{
		if (m_bBad || m_nPos + n > m_nSize || m_nPos + n < m_nPos)
		{
			m_bBad = true;
			return NULL;
		}
		const uint8 *p = m_pData + m_nPos;
		m_nPos += n;
		return p;
	}

	void Skip(uint32 n) { Raw(n); }

	uint8 U8() { const uint8 *p = Raw(1); return p ? p[0] : 0; }
	uint16 U16() { const uint8 *p = Raw(2); uint16 v = 0; if (p) memcpy(&v, p, 2); return v; }
	uint32 U32() { const uint8 *p = Raw(4); uint32 v = 0; if (p) memcpy(&v, p, 4); return v; }
	int32 I32() { const uint8 *p = Raw(4); int32 v = 0; if (p) memcpy(&v, p, 4); return v; }
	float F32() { const uint8 *p = Raw(4); float v = 0; if (p) memcpy(&v, p, 4); return v; }

	void V3(float *pOut)
	{
		const uint8 *p = Raw(12);
		if (p) memcpy(pOut, p, 12);
		else { pOut[0] = pOut[1] = pOut[2] = 0.0f; }
	}

	std::string S16()
	{
		uint16 n = U16();
		const uint8 *p = Raw(n);
		return p ? std::string((const char *)p, n) : std::string();
	}

private:
	const uint8 *m_pData;
	uint32 m_nSize;
	uint32 m_nPos;
	bool m_bBad;
};

struct Surface
{
	float m_O[3], m_P[3], m_Q[3];
	// The lightmap basis. 
	// The basis length is a per-surface luxel scale, so it is never normalised.
	float m_LMU[3], m_LMV[3];
	uint16 m_nTexture;
	uint32 m_nFlags;
	uint16 m_nTexFlags;

	// Byte 0 of the field after the flags, which LT1's vis tree builder prunes on
	uint8 m_nVisPrune;

	// The surface effect
	// Empty when the surface carries none
	std::string m_sEffect;
	std::string m_sEffectParams;
};

struct PolyVert
{
	uint16 m_nVert;
	uint8 m_RGB[3];
};

struct Poly
{
	uint32 m_nSurface;
	std::vector<PolyVert> m_Verts;
	// The world-space origin the lightmap basis measures from
	float m_LMOrigin[3];

	// The polygon's own lightmap grid
	// Zero on a v56 polygon
	uint16 m_nLMW, m_nLMH;
};

struct LeafList
{
	uint16 m_nPortalId;
	std::vector<uint8> m_Payload;
};

struct Leaf
{
	uint16 m_nCount;
	uint16 m_nAlias;
	std::vector<LeafList> m_Lists;
};

struct Node
{
	uint32 m_nPoly;
	uint16 m_nLeaf;
	int32 m_Child0, m_Child1;
};

struct WorldModel
{
	// Set by the v56 reader
	bool m_bHasVisPrune;

	std::string m_sName;

	uint32 m_nPoints, m_nPlanes, m_nSurfaces, m_nUserPortals;
	uint32 m_nPolies, m_nLeaves, m_nVerts, m_nNodes;

	float m_MinBox[3], m_MaxBox[3], m_Translation[3];

	// A user portal, with every field LT1 keeps carried verbatim
	struct UserPortal
	{
		std::string m_sName;
		uint32 m_nUnknown32;	// LT1 reads this into a scratch and drops it
		uint16 m_nUnknown16;	// portal+4 at runtime (unidentified)
		float m_Centre[3];
		float m_Dims[3];
	};

	std::vector<UserPortal> m_UserPortals;
	std::vector<std::string> m_Textures;
	std::vector<uint32> m_PolyVertCounts;	// drawn
	std::vector<uint32> m_PolyVertTotals;	// drawn + T-junction extras
	std::vector<Leaf> m_Leaves;
	std::vector<Surface> m_Surfaces;
	std::vector<Poly> m_Polies;
	std::vector<Node> m_Nodes;
	std::vector<float> m_Points; // 3 floats per point
	int32 m_nRootNode;

	// Per poly lightmap pages keyed by polygon index, as they sit in the file's packed lightmap block
	struct LMPage
	{
		uint8 m_nW, m_nH;
		std::vector<uint8> m_Texels;
	};
	std::map<uint32, LMPage> m_Lightmaps;

	// True when the object block registers this model as a sky object. 
	// Set by ReadWorld
	bool m_bSkyObject;

	// LT1's whole model translucency verdict: 1 blended, 0 opaque, -1 decide per polygon
	int m_nLT1Translucent;
};

struct SourceWorld
{
	std::string m_sWorldInfo;
	std::vector<WorldModel> m_Models;
	std::vector<uint8> m_ObjectBlock;
};


bool ReadWorldModel(Reader &r, WorldModel &m)
{
	// Each world model is preceded by the offset of the next one and 32 bytes of dummy data
	r.U32();
	r.Skip(32);

	m.m_bSkyObject = false;		// Set by ReadWorld from the object block
	m.m_nLT1Translucent = -1;	// Stamped by world_v56_BuildV85Image

	r.U32(); // World info flags, not carried
	m.m_sName = r.S16();
	r.U32(); // Next position (LT1 only)

	m.m_nPoints = r.U32();
	m.m_nPlanes = r.U32();
	m.m_nSurfaces = r.U32();
	m.m_nUserPortals = r.U32();
	m.m_nPolies = r.U32();
	m.m_nLeaves = r.U32();
	m.m_nVerts = r.U32();
	// Two sizing hints for the leaf section, which Jupiter never uses
	r.U32();	// totalVisListSize
	r.U32();	// nLeafLists
	m.m_nNodes = r.U32();

	// Total number of leaf node indices, which Jupiter's header has no field for
	r.U32();

	r.V3(m.m_MinBox);
	r.V3(m.m_MaxBox);
	r.V3(m.m_Translation);

	r.U32(); // name length
	uint32 nTextures = r.U32();
	if (r.Bad() || nTextures > 0x10000) return false;

	m.m_Textures.reserve(nTextures);
	for (uint32 i = 0; i < nTextures; ++i)
	{
		// WorldTexture: a null terminated name
		std::string s;
		for (;;)
		{
			uint8 c = r.U8();
			if (r.Bad() || c == 0) break;
			s += (char)c;
		}
		m.m_Textures.push_back(s);
	}

	// Two per poly vertex counts: 
	// The polygon as drawn, then extra edge vertices that close T-junction cracks.
	// Only the first set is drawn since summing them produces lots of z fighting.
	if (r.Bad() || m.m_nPolies > 0x400000) return false;
	m.m_PolyVertCounts.reserve(m.m_nPolies);
	m.m_PolyVertTotals.reserve(m.m_nPolies);
	for (uint32 i = 0; i < m.m_nPolies; ++i)
	{
		uint8 a = r.U8();
		uint8 b = r.U8();
		m.m_PolyVertCounts.push_back((uint32)a);
		m.m_PolyVertTotals.push_back((uint32)a + (uint32)b);
	}

	// Leaves
	// Kept verbatim. The v85 loader walks and skips them.
	if (r.Bad() || m.m_nLeaves > 0x400000) return false;
	m.m_Leaves.resize(m.m_nLeaves);
	for (uint32 i = 0; i < m.m_nLeaves; ++i)
	{
		Leaf &leaf = m.m_Leaves[i];
		leaf.m_nCount = r.U16();
		leaf.m_nAlias = 0;
		if (leaf.m_nCount == 0xFFFF)
		{
			leaf.m_nAlias = r.U16();
		}
		else
		{
			leaf.m_Lists.resize(leaf.m_nCount);
			for (uint32 k = 0; k < leaf.m_nCount; ++k)
			{
				leaf.m_Lists[k].m_nPortalId = r.U16();
				uint16 nSize = r.U16();
				const uint8 *p = r.Raw(nSize);
				if (!p) return false;
				leaf.m_Lists[k].m_Payload.assign(p, p + nSize);
			}
		}
		// LT1 extra leaf data which v85 has no place for
		uint16 nLeafPolies = r.U16();
		r.Skip((uint32)nLeafPolies * 4);
		r.F32();
		if (r.Bad()) return false;
	}

	// Planes are dropped cuz a plane is derived per polygon on the way out
	if (r.Bad() || m.m_nPlanes > 0x400000) return false;
	r.Skip(m.m_nPlanes * 16);

	// Surfaces
	if (r.Bad() || m.m_nSurfaces > 0x400000) return false;
	m.m_bHasVisPrune = false;
	m.m_Surfaces.resize(m.m_nSurfaces);
	for (uint32 i = 0; i < m.m_nSurfaces; ++i)
	{
		Surface &s = m.m_Surfaces[i];
		float dummy[3];
		r.V3(s.m_O); r.V3(s.m_P); r.V3(s.m_Q);
		r.V3(s.m_LMU); r.V3(s.m_LMV);	// Lightmap basis (see Surface)
		r.V3(dummy);					// Color
		s.m_nTexture = r.U16();

		r.U32(); // Plane index

		s.m_nFlags = r.U32();

		// LT1 keeps only the first of these four bytes, which the vis tree builder prunes on
		{
			uint32 nFour = r.U32();
			s.m_nVisPrune = (uint8)(nFour & 0xFF);
			m.m_bHasVisPrune = true;
		}

		uint8 bUseEffects = r.U8();
		if (bUseEffects == 1)
		{
			// Effect name then effect parameters
			s.m_sEffect = r.S16();
			s.m_sEffectParams = r.S16();
		}
		s.m_nTexFlags = r.U16();
		if (r.Bad()) return false;
	}

	// Polygons
	m.m_Polies.resize(m.m_nPolies);
	for (uint32 i = 0; i < m.m_nPolies; ++i)
	{
		Poly &p = m.m_Polies[i];
		// Four fields LT1 never stores
		r.U16(); r.U16();
		r.U32(); r.U32();

		p.m_nSurface = r.U32(); // Bounds checked against nSurfaces

		uint32 nVerts = m.m_PolyVertCounts[i];
		uint32 nOnDisk = m.m_PolyVertTotals[i];
		p.m_Verts.resize(nVerts);
		for (uint32 k = 0; k < nOnDisk; ++k)
		{
			uint16 nVert = r.U16();
			const uint8 *pRGB = r.Raw(3);
			if (!pRGB) return false;
			// Past the drawn set are the T-junction vertices, which are consumed and not kept
			if (k < nVerts)
			{
				p.m_Verts[k].m_nVert = nVert;
				memcpy(p.m_Verts[k].m_RGB, pRGB, 3);
			}
		}
		if (r.Bad()) return false;
	}

	// Nodes. 
	// v85 keeps only the poly index, leaf index, and two child indices.
	if (r.Bad() || m.m_nNodes > 0x400000) return false;
	m.m_Nodes.resize(m.m_nNodes);
	for (uint32 i = 0; i < m.m_nNodes; ++i)
	{
		r.U32(); // Plane index

		m.m_Nodes[i].m_nPoly = r.U32();
		m.m_Nodes[i].m_nLeaf = r.U16();
		m.m_Nodes[i].m_Child0 = r.I32();
		m.m_Nodes[i].m_Child1 = r.I32();

		// A bounding sphere
		r.Skip(16); // centre (float[3]) + radius
	}

	// User portals
	// Carried verbatim into the v85 image
	m.m_UserPortals.resize(m.m_nUserPortals);
	for (uint32 i = 0; i < m.m_nUserPortals; ++i)
	{
		WorldModel::UserPortal &up = m.m_UserPortals[i];
		up.m_sName = r.S16();
		up.m_nUnknown32 = r.U32();
		up.m_nUnknown16 = r.U16();
		r.V3(up.m_Centre);
		r.V3(up.m_Dims);
		if (r.Bad()) return false;
	}

	// Points
	if (r.Bad() || m.m_nPoints > 0x400000) return false;
	m.m_Points.resize((size_t)m.m_nPoints * 3);
	for (uint32 i = 0; i < m.m_nPoints; ++i)
		r.V3(&m.m_Points[(size_t)i * 3]);

	// LT1 polygon block table
	{
		uint32 a = r.U32();
		uint32 b = r.U32();
		uint32 c = r.U32();
		float dummy[3];
		r.V3(dummy); r.V3(dummy);
		if (r.Bad()) return false;
		// Guard the product since a corrupt header would otherwise spin
		double nBlocks = (double)a * (double)b * (double)c;
		if (nBlocks > 4.0 * 1024.0 * 1024.0) return false;
		uint32 nTotal = a * b * c;
		for (uint32 i = 0; i < nTotal; ++i)
		{
			uint16 nSize = r.U16();
			r.U16();
			r.Skip((uint32)nSize * 6);
			if (r.Bad()) return false;
		}
	}

	m.m_nRootNode = r.I32();

	// LT1 tail: 
	// An unidentified word, per poly vectors, then packed lightmaps.
	// The LT1 runtime never reads this.
	r.U32();
	// One lightmap origin per polygon
	for (uint32 i = 0; i < m.m_nPolies; ++i)
		r.V3(m.m_Polies[i].m_LMOrigin);

	// A byte count for the packed lightmap block. Not a number of lightmaps
	uint32 nLightmapBytes = r.U32();
	if (nLightmapBytes > 0)
	{
		for (uint32 i = 0; i < m.m_nPolies; ++i)
		{
			uint32 nSurf = m.m_Polies[i].m_nSurface;
			if (nSurf >= m.m_nSurfaces) continue;
			if (!(m.m_Surfaces[nSurf].m_nFlags & kSurfLightMap)) continue;
			uint8 w = r.U8();
			uint8 h = r.U8();
			WorldModel::LMPage &page = m.m_Lightmaps[i];
			page.m_nW = w;
			page.m_nH = h;
			page.m_Texels.resize((size_t)w * h * 2);
			if (page.m_Texels.size())
			{
				const uint8 *pTexels = r.Raw((uint32)page.m_Texels.size());
				if (pTexels)
					memcpy(&page.m_Texels[0], pTexels, page.m_Texels.size());
			}
			if (r.Bad()) return false;
		}
	}

	return !r.Bad();
}


bool ReadV56(const uint8 *pData, uint32 nSize, SourceWorld &out)
{
	Reader r(pData, nSize);

	if (r.U32() != 56) return false;

	uint32 nObjectDataPos = r.U32();

	// Offset of the world model section. The objects are walked instead of seeking here.
	r.U32();

	uint32 nInfoLen = r.U32();
	const uint8 *pInfo = r.Raw(nInfoLen);
	if (!pInfo) return false;
	out.m_sWorldInfo.assign((const char *)pInfo, nInfoLen);

	// Eight dummy ints. LT1 carries no extents here, they come from the root model's box.
	r.Skip(8 * 4);

	// The object list comes before the world models.
	// Each object's data length covers everything after that field.
	r.SeekTo(nObjectDataPos);
	uint32 nObjects = r.U32();
	if (r.Bad() || nObjects > 0x100000) return false;
	std::set<std::string> SkyObjectNames;
	for (uint32 i = 0; i < nObjects; ++i)
	{
		uint32 nStart = r.Pos();
		uint16 nDataLen = r.U16();
		std::string sType = r.S16();

		// DemoSkyWorldModel is a sky object itself, and SkyPointer names another brush in SkyObjectName
		if (sType == "DemoSkyWorldModel" || sType == "SkyPointer")
		{
			const char *pszWant = (sType == "DemoSkyWorldModel") ? "Name" : "SkyObjectName";
			uint32 nProps = r.U32();
			for (uint32 nProp = 0; nProp < nProps && !r.Bad(); ++nProp)
			{
				std::string sProp = r.S16();
				uint8 nCode = r.U8();
				r.U32(); // Flags
				uint16 nLen = r.U16();
				const uint8 *pVal = r.Raw(nLen);
				// A string property's data is itself length-prefixed
				if (pVal && sProp == pszWant && nCode == kPropTypeString && nLen >= 2)
				{
					uint16 nChars = 0;
					memcpy(&nChars, pVal, 2);
					if ((uint32)nChars + 2 <= (uint32)nLen && nChars > 0)
						SkyObjectNames.insert(ToLower(std::string((const char *)pVal + 2, nChars)));
				}
			}
		}

		r.SeekTo(nStart + 2 + nDataLen);
		if (r.Bad()) return false;
	}

	// The object list is identical between v56 and v85. Carried across verbatim
	out.m_ObjectBlock.assign(pData + nObjectDataPos, pData + r.Pos());

	// LT1 has no world tree. The root world model is first, then a count and the rest.
	out.m_Models.resize(1);
	if (!ReadWorldModel(r, out.m_Models[0])) return false;

	uint32 nMore = r.U32();
	if (r.Bad() || nMore > 0x10000) return false;
	for (uint32 i = 0; i < nMore; ++i)
	{
		out.m_Models.resize(out.m_Models.size() + 1);
		if (!ReadWorldModel(r, out.m_Models.back())) return false;
	}

	// A world model registered as a sky object is shaded differnetly
	// Matched case insensitively as LT1's name lookups are.
	for (uint32 i = 0; i < out.m_Models.size(); ++i)
	{
		WorldModel &m = out.m_Models[i];
		if (!m.m_sName.empty() && SkyObjectNames.find(ToLower(m.m_sName)) != SkyObjectNames.end())
		{
			m.m_bSkyObject = true;
		}
	}

	return true;
}

}


// Writing

namespace {

class Writer
{
public:
	void U8(uint8 v) { m_Buf.push_back(v); }
	void U16(uint16 v) { Append(&v, 2); }
	void U32(uint32 v) { Append(&v, 4); }
	void I32(int32 v) { Append(&v, 4); }
	void F32(float v) { Append(&v, 4); }
	void V3(const float *p) { Append(p, 12); }
	void V3(float x, float y, float z) { float v[3] = {x, y, z}; Append(v, 12); }

	void Raw(const void *p, size_t n) { Append(p, n); }

	// CGenLTStream::ReadString: uint16 length then characters
	void LTString(const std::string &s)
	{
		U16((uint16)s.size());
		Append(s.data(), s.size());
	}

	void PatchU32(size_t nAt, uint32 v) { memcpy(&m_Buf[nAt], &v, 4); }

	size_t Size() const { return m_Buf.size(); }
	const uint8 *Data() const { return m_Buf.empty() ? NULL : &m_Buf[0]; }

private:
	void Append(const void *p, size_t n)
	{
		const uint8 *b = (const uint8 *)p;
		m_Buf.insert(m_Buf.end(), b, b + n);
	}

	std::vector<uint8> m_Buf;
};



// Appends a decimal unsigned without pulling in stdio
void AppendUInt(std::string &s, uint32 v)
{
	char szBuf[16];
	int n = 0;
	do { szBuf[n++] = (char)('0' + (v % 10)); v /= 10; } while (v && n < (int)sizeof(szBuf));
	while (n--) s += szBuf[n];
}

void PlaneFromPoints(const std::vector<const float *> &pts, float *pNormal, float *pDist)
{
	pNormal[0] = 0.0f; pNormal[1] = 1.0f; pNormal[2] = 0.0f;
	*pDist = 0.0f;
	if (pts.size() < 3) return;

	double nx = 0.0, ny = 0.0, nz = 0.0;
	size_t n = pts.size();
	for (size_t i = 0; i < n; ++i)
	{
		const float *a = pts[i];
		const float *b = pts[(i + 1) % n];
		nx += ((double)a[1] - (double)b[1]) * ((double)a[2] + (double)b[2]);
		ny += ((double)a[2] - (double)b[2]) * ((double)a[0] + (double)b[0]);
		nz += ((double)a[0] - (double)b[0]) * ((double)a[1] + (double)b[1]);
	}

	double mag = sqrt(nx * nx + ny * ny + nz * nz);
	if (mag < 1e-12) return;

	nx /= mag; ny /= mag; nz /= mag;
	pNormal[0] = (float)nx;
	pNormal[1] = (float)ny;
	pNormal[2] = (float)nz;
	*pDist = (float)(nx * pts[0][0] + ny * pts[0][1] + nz * pts[0][2]);
}


// Synthesised due to the v56 word in that position not being a bitfield
uint32 V85WorldInfoFlags(bool bRoot)
{
	return bRoot ? (kWifMainWorld | kWifPhysicsBsp) : kWifMoveable;
}


struct RenderVert
{
	float m_Pos[3];
	float m_U, m_V;
	uint32 m_Color;
};

// One polygon's lightmap
// In double until the block is atlased
struct PolyLM
{
	bool m_bHas;
	uint8 m_nW, m_nH;
	std::vector<uint8> m_Texels;	// RGB565
	std::vector<double> m_LU, m_LV;

	PolyLM() : m_bHas(false), m_nW(0), m_nH(0) {}
};

// Synthesised occluders derived per block from the level's large opaque polygons
const double kMinOccluderArea = 4000.0;
// Off, since the engine's occluder is two sided and a one sided surface would leave holes from behind
const size_t kOccludersPerBlock = 0;


// A polygon's eligibility to be used as an occluder
// Decided once when the sections are built
struct PolyOcc
{
	bool m_bEligible;
	double m_Area;
	float m_Normal[3];
	float m_Dist;

	PolyOcc() : m_bEligible(false), m_Area(0.0), m_Dist(0.0)
	{
		m_Normal[0] = 0.0f; m_Normal[1] = 1.0f; m_Normal[2] = 0.0f;
	}
};


// Fills pOut if this polygon may be used as an occluder.
// The engine requires it to be convex, planar, and nondegenerate, 
// and the caller checks it draws opaque
void OccluderCandidate(const std::vector<const float *> &pts, PolyOcc *pOut)
{
	size_t n = pts.size();
	if (n < 3 || n > 255) return;

	double nx = 0.0, ny = 0.0, nz = 0.0;
	for (size_t i = 0; i < n; ++i)
	{
		const float *a = pts[i];
		const float *b = pts[(i + 1) % n];
		nx += ((double)a[1] - (double)b[1]) * ((double)a[2] + (double)b[2]);
		ny += ((double)a[2] - (double)b[2]) * ((double)a[0] + (double)b[0]);
		nz += ((double)a[0] - (double)b[0]) * ((double)a[1] + (double)b[1]);
	}

	double mag = sqrt(nx * nx + ny * ny + nz * nz);
	if (mag < 1e-12) return;

	double area = mag * 0.5;
	if (area < kMinOccluderArea) return;

	nx /= mag; ny /= mag; nz /= mag;

	// Convexity
	// Every corner must turn the same way about the face normal, skipping collinear corners
	int nSign = 0;
	for (size_t i = 0; i < n; ++i)
	{
		const float *a = pts[i];
		const float *b = pts[(i + 1) % n];
		const float *c = pts[(i + 2) % n];

		double e1[3], e2[3];
		for (int k = 0; k < 3; ++k)
		{
			e1[k] = (double)b[k] - (double)a[k];
			e2[k] = (double)c[k] - (double)b[k];
		}

		double cx = e1[1] * e2[2] - e1[2] * e2[1];
		double cy = e1[2] * e2[0] - e1[0] * e2[2];
		double cz = e1[0] * e2[1] - e1[1] * e2[0];
		double d = cx * nx + cy * ny + cz * nz;

		if (d > -1e-6 && d < 1e-6) continue;

		int sd = (d > 0.0) ? 1 : -1;
		if (nSign == 0) nSign = sd;
		else if (sd != nSign) return;
	}

	pOut->m_bEligible = true;
	pOut->m_Area = area;
	pOut->m_Normal[0] = (float)nx;
	pOut->m_Normal[1] = (float)ny;
	pOut->m_Normal[2] = (float)nz;
	pOut->m_Dist = (float)(nx * (double)pts[0][0] + ny * (double)pts[0][1] + nz * (double)pts[0][2]);
}


struct Section
{
	// Texture effect name synthesised from an LT1 surface effect (empty for none)
	std::string m_sEffectName;

	std::string m_sTexture;
	uint8 m_nShader;					// ePCShader_* code written to the section
	std::vector<RenderVert> m_Polys;	// Flattened with m_PolyStarts
	std::vector<uint32> m_PolyStarts;	// Index into m_Polys, plus a terminator
	std::vector<PolyLM> m_PolyLMs;		// Parallel to the polys
	std::vector<PolyOcc> m_PolyOcc;		// Parallel to the polys

	// Part of the section's key so every polygon in a section agrees about it
	bool m_bSkypan;

	Section() : m_nShader(0), m_bSkypan(false) {}
};


void GetTexDims(WorldV56TexDimsFn pFn, void *pUser, const std::string &sName, float *pW, float *pH)
{
	*pW = 128.0f;
	*pH = 128.0f;
	if (!pFn) return;

	uint32 w = 0, h = 0;
	if (pFn(sName.c_str(), &w, &h, pUser) && w && h)
	{
		*pW = (float)w;
		*pH = (float)h;
	}
}


// A SURF_SKY polygon
// Kept as a portal for ExtendSkyBounds
struct SkyPortal
{
	std::vector<float> m_Verts; // xyz per vertex
	float m_Normal[3];
	float m_Dist;
};

// Group a world model's polygons by texture and accumulate the bounds
void BuildSections(const std::vector<const WorldModel *> &models, WorldV56TexDimsFn pTexDims, void *pUser, 
	std::vector<Section> &out, float *pMin, float *pMax, std::vector<SkyPortal> *pSkyPortals)
{
	for (int k = 0; k < 3; ++k) { pMin[k] = 1e30f; pMax[k] = -1e30f; }

	std::map<std::string, size_t> index;

	for (size_t mi = 0; mi < models.size(); ++mi)
	{
		const WorldModel &m = *models[mi];

		// LT1 sorts a whole WorldModel into the blended list from polygon 0's flag alone.
		// A verdict of -1 keeps the per polygon reading.
		const int nLT1Translucent = m.m_nLT1Translucent;

		for (size_t pi = 0; pi < m.m_Polies.size(); ++pi)
		{
			const Poly &p = m.m_Polies[pi];
			if (p.m_nSurface >= m.m_Surfaces.size()) continue;
			const Surface &surf = m.m_Surfaces[p.m_nSurface];
			if (surf.m_nTexture >= m.m_Textures.size()) continue;
			const bool bTransparent = (nLT1Translucent < 0) ? ((surf.m_nFlags & kSurfTransparent) != 0) : (nLT1Translucent != 0);

			// A sky surface is a portal - not geometry. As such it is kept out of the drawn set
			if (surf.m_nFlags & kSurfSky)
			{
				if (pSkyPortals && p.m_Verts.size() >= 3)
				{
					std::vector<const float *> pts;
					pts.reserve(p.m_Verts.size());
					for (size_t vi = 0; vi < p.m_Verts.size(); ++vi)
					{
						uint32 nPoint = p.m_Verts[vi].m_nVert;
						if (nPoint < m.m_nPoints)
							pts.push_back(&m.m_Points[(size_t)nPoint * 3]);
					}
					if (pts.size() >= 3)
					{
						SkyPortal portal;
						portal.m_Verts.reserve(pts.size() * 3);
						for (size_t vi = 0; vi < pts.size(); ++vi)
						{
							portal.m_Verts.push_back(pts[vi][0]);
							portal.m_Verts.push_back(pts[vi][1]);
							portal.m_Verts.push_back(pts[vi][2]);
						}
						PlaneFromPoints(pts, portal.m_Normal, &portal.m_Dist);
						pSkyPortals->push_back(portal);
					}
				}
				continue;
			}

			// LT1 skips these! Drawing them puts placeholders on screen
			if (surf.m_nFlags & (kSurfInvisible | kSurfNonExistent)) continue;

			const std::string &sTex = m.m_Textures[surf.m_nTexture];
			float tw, th;
			GetTexDims(pTexDims, pUser, sTex, &tw, &th);

			std::vector<RenderVert> verts;
			verts.reserve(p.m_Verts.size());
			bool bOk = true;

			for (size_t vi = 0; vi < p.m_Verts.size(); ++vi)
			{
				uint32 nPoint = p.m_Verts[vi].m_nVert;
				if (nPoint >= m.m_nPoints) { bOk = false; break; }

				const float *V = &m.m_Points[(size_t)nPoint * 3];

				double d[3] = { (double)V[0] - (double)surf.m_O[0], (double)V[1] - (double)surf.m_O[1], (double)V[2] - (double)surf.m_O[2] };

				RenderVert rv;
				rv.m_Pos[0] = V[0]; rv.m_Pos[1] = V[1]; rv.m_Pos[2] = V[2];
				rv.m_U = (float)((d[0] * (double)surf.m_P[0] + d[1] * (double)surf.m_P[1] + d[2] * (double)surf.m_P[2]) / (double)tw);
				rv.m_V = (float)((d[0] * (double)surf.m_Q[0] + d[1] * (double)surf.m_Q[1] + d[2] * (double)surf.m_Q[2]) / (double)th);

				// The per vertex color is the level's baked lighting
				const uint8 *rgb = p.m_Verts[vi].m_RGB;
				rv.m_Color = ((uint32)rgb[0] << 16) | ((uint32)rgb[1] << 8) |
							  (uint32)rgb[2] | 0xFF000000u;

				verts.push_back(rv);
				for (int k = 0; k < 3; ++k)
				{
					if (V[k] < pMin[k]) pMin[k] = V[k];
					if (V[k] > pMax[k]) pMax[k] = V[k];
				}
			}

			if (!bOk || verts.size() < 3) continue;

			// A lightmapped polygon becomes two passes: a lightmap section with the atlased page and a texture section blended over it.
			// Transparent polygons are not lightmapped since the lightmap base under them would double blend.
			// A sky object is never lightmapped and shades from its vertex colors.
			PolyLM lm;
			if ((surf.m_nFlags & kSurfLightMap) && !bTransparent && !m.m_bSkyObject)
			{
				std::map<uint32, WorldModel::LMPage>::const_iterator itLM = m.m_Lightmaps.find((uint32)pi);
				if (itLM != m.m_Lightmaps.end() && itLM->second.m_nW >= 1 && itLM->second.m_nH >= 1)
				{
					lm.m_bHas = true;
					lm.m_nW = itLM->second.m_nW;
					lm.m_nH = itLM->second.m_nH;
					lm.m_Texels = itLM->second.m_Texels;
					lm.m_LU.reserve(verts.size());
					lm.m_LV.reserve(verts.size());
					const double dW = (double)lm.m_nW, dH = (double)lm.m_nH;
					for (size_t k = 0; k < verts.size(); ++k)
					{
						const float *V = verts[k].m_Pos;
						double d[3] = { (double)V[0] - (double)p.m_LMOrigin[0], (double)V[1] - (double)p.m_LMOrigin[1], (double)V[2] - (double)p.m_LMOrigin[2] };
						double lu = (d[0] * (double)surf.m_LMU[0] + d[1] * (double)surf.m_LMU[1] + d[2] * (double)surf.m_LMU[2]) * 0.05 + 0.5;
						double lv = (d[0] * (double)surf.m_LMV[0] + d[1] * (double)surf.m_LMV[1] + d[2] * (double)surf.m_LMV[2]) * 0.05 + 0.5;
						// Clamped to the page, as the atlas puts another polygon's page right beside it
						if (lu < 0.0) lu = 0.0; else if (lu > dW) lu = dW;
						if (lv < 0.0) lv = 0.0; else if (lv > dH) lv = dH;
						lm.m_LU.push_back(lu);
						lm.m_LV.push_back(lv);
					}
				}
			}

			// LT1 draws a SURF_PANNINGSKY polygon as cloud * vertex color with the texture over it,
			// so it converts like a lightmapped surface with the pan texture standing in for the page.
			// SURF_LIGHTMAP wins where a surface has both, as LT1 tests it first.
			const bool bSkypan = ((surf.m_nFlags & kSurfPanningSky) != 0) && !lm.m_bHas && !bTransparent && !m.m_bSkyObject;

			// The shader is part of the section's key. As such glass and a wall sharing a texture do not merge
			uint8 nShader;
			if (bTransparent)
				nShader = kShaderGouraudTranslucent;
			else if (lm.m_bHas || bSkypan)
				nShader = kShaderLightmapTexture;
			else
				nShader = kShaderGouraud;

			// An LT1 Pan becomes a UV rate here cuz the rate is part of the section's key.
			// LT1 pans by moving the texture origin along P and Q, so shifting O by a*P moves u by -a*|P|^2 / tw.
			std::string sEffectName;
			if (!surf.m_sEffect.empty() && stricmp(surf.m_sEffect.c_str(), "Pan") == 0)
			{
				double uRate = 0.0, vRate = 0.0;
				if (!surf.m_sEffectParams.empty())
				{
					const char *pArg = surf.m_sEffectParams.c_str();
					uRate = atof(pArg);
					while (*pArg == ' ' || *pArg == 9) ++pArg;
					while (*pArg && *pArg != ' ' && *pArg != 9) ++pArg;
					while (*pArg == ' ' || *pArg == 9) ++pArg;
					if (*pArg) vRate = atof(pArg);
				}

				const double pLenSq = (double)surf.m_P[0]*surf.m_P[0] + (double)surf.m_P[1]*surf.m_P[1] + (double)surf.m_P[2]*surf.m_P[2];
				const double qLenSq = (double)surf.m_Q[0]*surf.m_Q[0] + (double)surf.m_Q[1]*surf.m_Q[1] + (double)surf.m_Q[2]*surf.m_Q[2];

				const double du = (tw > 0) ? -uRate * pLenSq / (double)tw : 0.0;
				const double dv = (th > 0) ? -vRate * qLenSq / (double)th : 0.0;

				if (du != 0.0 || dv != 0.0)
				{
					char szEffect[128];
					sprintf(szEffect, "@LT1Pan %.8f %.8f", du, dv);
					sEffectName = szEffect;
				}
			}

			// Key is texture name, shader code, and effect name in first encounter order.
			const char SEP = (char)1;
			std::string sKey = sTex;
			sKey += SEP;
			sKey += (char)nShader;
			sKey += SEP;
			sKey += sEffectName;
			sKey += SEP;
			sKey += (char)(bSkypan ? 1 : 0);

			std::map<std::string, size_t>::iterator it = index.find(sKey);
			size_t nSection;
			if (it == index.end())
			{
				nSection = out.size();
				index[sKey] = nSection;
				out.resize(nSection + 1);
				out[nSection].m_sTexture = sTex;
				out[nSection].m_nShader = nShader;
				out[nSection].m_sEffectName = sEffectName;
				out[nSection].m_bSkypan = bSkypan;
			}
			else
			{
				nSection = it->second;
			}

			Section &sec = out[nSection];
			sec.m_PolyStarts.push_back((uint32)sec.m_Polys.size());
			sec.m_Polys.insert(sec.m_Polys.end(), verts.begin(), verts.end());
			sec.m_PolyLMs.push_back(lm);

			// An occluder is only built from a polygon the renderer draws opaque
			PolyOcc occ;
			if (nShader != kShaderGouraudTranslucent)
			{
				std::vector<const float *> opts;
				opts.reserve(verts.size());
				for (size_t k = 0; k < verts.size(); ++k)
					opts.push_back(verts[k].m_Pos);
				OccluderCandidate(opts, &occ);
			}
			sec.m_PolyOcc.push_back(occ);
		}
	}

	// Terminate each section's polygon index so each polygon's extent can be found without a stored length
	for (size_t i = 0; i < out.size(); ++i)
		out[i].m_PolyStarts.push_back((uint32)out[i].m_Polys.size());

	if (out.empty())
	{
		for (int k = 0; k < 3; ++k) { pMin[k] = -1.0f; pMax[k] = 1.0f; }
	}
}



struct SecLM
{
	uint32 m_nW, m_nH;
	std::vector<uint8> m_Data;
	SecLM() : m_nW(0), m_nH(0) {}
};

// A lightmapped polygon carried through block assembly to the atlas step
struct BlockLMPoly
{
	std::vector<RenderVert> m_Verts;
	PolyLM m_LM;
};

struct BlockOccluder
{
	std::vector<float> m_Verts; // xyz triples
	float m_Normal[3];
	float m_Dist;
};

struct Block
{
	std::vector<RenderVert> m_Verts;
	std::vector<std::string> m_SecNames;
	std::vector<uint8> m_SecShaders;				// Parallel to m_SecNames
	std::vector<std::string> m_SecEffects;			// Parallel (empty = none)
	std::vector<std::vector<uint32> > m_SecTris;	// Triples (flattened)
	std::vector<SecLM> m_SecLMs;					// Parallel (empty = none)

	// Part of a section's key but never written. 
	// It's base pass is emitted as a section of its own as a result
	std::vector<uint8> m_SecSkypan;

	std::vector<BlockLMPoly> m_LMPolys; // Atlased after assembly

	// Copies of every SkyPan polygon in the block.
	// Turned into one kShaderSkypan section after assembly
	std::vector<std::vector<RenderVert> > m_SkypanPolys;
	std::vector<BlockOccluder> m_Occluders; // Chosen after assembly
	float m_Min[3], m_Max[3];

	// False for an interior block - has no geometry or bounds of its own
	bool m_bHasBounds;

	Block() : m_bHasBounds(false) {}
};

// Render block tree shape
// The vertex cost a leaf aims for, the leaf cap, and the ceiling the vertex budget may push to
const uint32 kBlockTargetVerts = 1024;
const uint32 kMaxTreeLeaves = 64;
const uint32 kHardTreeLeaves = 1024;

// World tree: Jupiter's object quadtree with one subdivide bit per node in preorder
const double kWorldTreeCellTarget = 1024.0;
const uint32 kMaxWorldTreeDepth = 6;

// The span is taken in double so both halves of the byte identical pair round alike
static uint32 WorldTreeDepth(const float bmin[3], const float bmax[3])
{
	double dx = (double)bmax[0] - (double)bmin[0];
	double dz = (double)bmax[2] - (double)bmin[2];
	double span = (dx > dz) ? dx : dz;
	uint32 depth = 0;
	while (depth < kMaxWorldTreeDepth && span > kWorldTreeCellTarget * (double)(1u << depth))
	{
		++depth;
	}
	return depth;
}

static void WorldTreeWalk(uint32 d, uint32 depth, std::vector<uint8> &bits)
{
	bool bSub = (d < depth);
	bits.push_back(bSub ? 1 : 0);
	if (bSub)
	{
		for (int i = 0; i < 4; ++i)
			WorldTreeWalk(d + 1, depth, bits);
	}
}

// LT1's compressed vis tree (the PVS)
// Built here as the prune reads a surface byte the v85 surface record cannot carry.

static const int32 kVisSolid = -2;	// child slot 0 terminates in -2
static const int32 kVisEmpty = -1;	// child slot 1 terminates in -1

struct VisNodeOut
{
	uint32 m_nBspNode;
	int32 m_Slot0, m_Slot1; // Vis node index.. or -1 for either sentinel
};

// Returns the visnode index for this BSP node, 
// -1 for a sentinel or -2 when pruned
static int32 BuildVisTree(const WorldModel &m, int32 nIdx, std::vector<VisNodeOut> &order)
{
	for (;;)
	{
		if (nIdx == kVisEmpty || nIdx == kVisSolid)
			return -1;
		if (nIdx < 0 || (size_t)nIdx >= m.m_Nodes.size())
			return -1;
		const Node &n = m.m_Nodes[nIdx];
		if (n.m_nPoly < m.m_Polies.size())
		{
			uint32 nSurf = m.m_Polies[n.m_nPoly].m_nSurface;
			if (nSurf < m.m_Surfaces.size() && m.m_Surfaces[nSurf].m_nVisPrune != 0)
				return -2; // prune: emit nothing, no recursion
		}
		if (n.m_nLeaf != 0xFFFF) // Carries a leaf -> always emitted
			break;
		if (n.m_Child1 == kVisEmpty)
			break;
		if (n.m_Child0 != kVisSolid)
			break;
		nIdx = n.m_Child1; // Collapse: descend, emit nothing
	}

	// Emitted in preorder, the order the bitstream is written against
	size_t nSelf = order.size();
	order.push_back(VisNodeOut());
	order[nSelf].m_nBspNode = (uint32)nIdx;
	order[nSelf].m_Slot0 = -1;
	order[nSelf].m_Slot1 = -1;

	const Node &n = m.m_Nodes[nIdx];
	int32 s0 = BuildVisTree(m, n.m_Child0, order);
	int32 s1 = BuildVisTree(m, n.m_Child1, order);
	order[nSelf].m_Slot0 = (s0 < 0) ? -1 : s0;
	order[nSelf].m_Slot1 = (s1 < 0) ? -1 : s1;
	return (int32)nSelf;
}

// Root index, node count, then 16 bytes per node in emission order
static void WriteVisTree(Writer &w, const WorldModel &m)
{
	std::vector<VisNodeOut> order;
	int32 nRoot = BuildVisTree(m, m.m_nRootNode, order);
	// The header is written even when the walk emitted nothing.
	// The loader already expects a tree to follow. Skipping it would misread the planes.
	w.I32(nRoot < 0 ? -1 : nRoot);
	w.U32((uint32)order.size());
	for (size_t i = 0; i < order.size(); ++i)
	{
		const VisNodeOut &v = order[i];
		w.U32(v.m_nBspNode);
		w.U16(v.m_nBspNode < m.m_Nodes.size() ? m.m_Nodes[v.m_nBspNode].m_nLeaf : (uint16)0xFFFF);
		w.U16(0);
		w.I32(v.m_Slot0);
		w.I32(v.m_Slot1);
	}
}

// Returns the node count and fills layout with the subdivide bits
static uint32 WorldTreeLayout(const float bmin[3], const float bmax[3], std::vector<uint8> &layout)
{
	std::vector<uint8> bits;
	WorldTreeWalk(0, WorldTreeDepth(bmin, bmax), bits);
	layout.assign((bits.size() + 7) / 8, 0);
	for (size_t i = 0; i < bits.size(); ++i)
	{
		if (bits[i])
			layout[i >> 3] |= (uint8)(1u << (i & 7));
	}
	return (uint32)bits.size();
}


// One polygon, as the spatial partition sees it
struct BlockPoly
{
	uint32 m_nSection;
	uint32 m_nPoly;
	uint32 m_nCost;
	double m_Centroid[3];
};


// Expand packed RGB565 texels to 3 bytes per pixel
void RGB565To888(const std::vector<uint8> &texels, std::vector<uint8> &out)
{
	out.clear();
	out.reserve((texels.size() / 2) * 3);
	for (size_t i = 0; i + 1 < texels.size(); i += 2)
	{
		uint32 v = (uint32)texels[i] | ((uint32)texels[i + 1] << 8);
		uint32 r = (v >> 11) & 0x1F;
		uint32 g = (v >> 5) & 0x3F;
		uint32 b = v & 0x1F;
		out.push_back((uint8)((r << 3) | (r >> 2)));
		out.push_back((uint8)((g << 2) | (g >> 4)));
		out.push_back((uint8)((b << 3) | (b >> 2)));
	}
}


// TGA-style RLE over 24-bit pixels, as DecompressLMData reads it.
// A tag byte's high bit means a run and its low 7 bits are the span length - 1 (capped at 128)
void CompressLM(const std::vector<uint8> &px, std::vector<uint8> &out)
{
	out.clear();
	size_t n = px.size() / 3;
	size_t i = 0;
	while (i < n)
	{
		const uint8 *p = &px[i * 3];
		if (i + 1 < n && memcmp(&px[(i + 1) * 3], p, 3) == 0)
		{
			size_t j = i;
			while (j < n && memcmp(&px[j * 3], p, 3) == 0 && j - i < 128)
				j++;
			out.push_back((uint8)(0x80 | (j - i - 1)));
			out.insert(out.end(), p, p + 3);
			i = j;
		}
		else
		{
			size_t j = i;
			while (j < n && j - i < 128 && !(j + 1 < n && memcmp(&px[j * 3], &px[(j + 1) * 3], 3) == 0))
				j++;
			out.push_back((uint8)(j - i - 1));
			out.insert(out.end(), &px[i * 3], &px[j * 3]);
			i = j;
		}
	}
}


uint32 NextPow2(uint32 v)
{
	uint32 n = 1;
	while (n < v) n *= 2;
	return n;
}


// Appends a block's base pass sections: 
// one lightmap section per atlased page, and one Skypan section if needed
void BuildBlockBasePassSections(Block &b)
{
	// Either kind of base pass is reason enough to run
	if (b.m_LMPolys.empty() && b.m_SkypanPolys.empty())
		return;

	struct Placement { size_t nPoly; uint32 nX, nY; };
	struct Page
	{
		std::vector<uint8> m_Canvas; // kLMPageW * kLMPageMaxH * 3
		uint32 m_nUsedH;
		std::vector<Placement> m_Placed;
	};

	std::vector<Page> pages;
	Page cur;
	bool bOpen = false;
	uint32 x = 0, y = 0, shelfH = 0;

	for (size_t i = 0; i < b.m_LMPolys.size(); ++i)
	{
		const PolyLM &lm = b.m_LMPolys[i].m_LM;
		uint32 w = lm.m_nW, h = lm.m_nH;
		if (!bOpen)
		{
			cur = Page();
			cur.m_Canvas.assign((size_t)kLMPageW * kLMPageMaxH * 3, 0);
			x = y = shelfH = 0;
			bOpen = true;
		}
		if (x + w > kLMPageW)
		{
			y += shelfH;
			x = 0;
			shelfH = 0;
		}
		if (y + h > kLMPageMaxH)
		{
			cur.m_nUsedH = y + shelfH;
			pages.push_back(cur);
			cur = Page();
			cur.m_Canvas.assign((size_t)kLMPageW * kLMPageMaxH * 3, 0);
			x = y = shelfH = 0;
		}
		std::vector<uint8> rgb;
		RGB565To888(lm.m_Texels, rgb);
		for (uint32 row = 0; row < h; ++row)
		{
			size_t src = (size_t)row * w * 3;
			size_t dst = ((size_t)(y + row) * kLMPageW + x) * 3;
			if (src + w * 3 <= rgb.size())
				memcpy(&cur.m_Canvas[dst], &rgb[src], (size_t)w * 3);
		}
		Placement pl;
		pl.nPoly = i; pl.nX = x; pl.nY = y;
		cur.m_Placed.push_back(pl);
		x += w;
		if (h > shelfH) shelfH = h;
	}
	if (bOpen && !cur.m_Placed.empty())
	{
		cur.m_nUsedH = y + shelfH;
		pages.push_back(cur);
	}

	// The shader never samples the texture name, but the loader's missing-texture warning reads it
	std::string sName;
	for (size_t k = 0; k < b.m_SecShaders.size(); ++k)
	{
		if (b.m_SecShaders[k] == kShaderLightmapTexture)
		{
			sName = b.m_SecNames[k];
			break;
		}
	}

	for (size_t pg = 0; pg < pages.size(); ++pg)
	{
		Page &page = pages[pg];
		uint32 nPageH = NextPow2(page.m_nUsedH ? page.m_nUsedH : 1);
		if (nPageH > kLMPageMaxH) nPageH = kLMPageMaxH;

		std::vector<uint32> tris;
		for (size_t k = 0; k < page.m_Placed.size(); ++k)
		{
			const Placement &pl = page.m_Placed[k];
			const BlockLMPoly &lp = b.m_LMPolys[pl.nPoly];
			const PolyLM &lm = lp.m_LM;

			uint32 nBase = (uint32)b.m_Verts.size();
			for (size_t vi = 0; vi < lp.m_Verts.size(); ++vi)
			{
				RenderVert rv = lp.m_Verts[vi];
				// m_LU/m_LV are already in texels of the page. So only the placement is added.
				rv.m_U = (float)(((double)pl.nX + lm.m_LU[vi]) / (double)kLMPageW);
				rv.m_V = (float)(((double)pl.nY + lm.m_LV[vi]) / (double)nPageH);
				b.m_Verts.push_back(rv);
			}
			for (uint32 t = 1; t + 1 < (uint32)lp.m_Verts.size(); ++t)
			{
				tris.push_back(nBase);
				tris.push_back(nBase + t);
				tris.push_back(nBase + t + 1);
			}
		}

		std::vector<uint8> canvas(page.m_Canvas.begin(), page.m_Canvas.begin() + (size_t)nPageH * kLMPageW * 3);
		SecLM out;
		out.m_nW = kLMPageW;
		out.m_nH = nPageH;
		CompressLM(canvas, out.m_Data);

		b.m_SecNames.push_back(sName);
		b.m_SecShaders.push_back(kShaderLightmap);
		b.m_SecEffects.push_back(std::string()); // Lightmap pass never pans
		b.m_SecSkypan.push_back(0);
		b.m_SecTris.push_back(tris);
		b.m_SecLMs.push_back(out);
	}

	// The sky shadow pan's base pass: one section per block with copies of every SkyPan polygon.
	// The base UV carries the world X and Z unscaled, and the renderer applies LT1's divide and scroll as a texture transform.
	if (!b.m_SkypanPolys.empty())
	{
		std::vector<uint32> tris;
		for (size_t k = 0; k < b.m_SkypanPolys.size(); ++k)
		{
			const std::vector<RenderVert> &pv = b.m_SkypanPolys[k];
			uint32 nBase = (uint32)b.m_Verts.size();
			for (size_t vi = 0; vi < pv.size(); ++vi)
			{
				RenderVert rv = pv[vi];
				rv.m_U = rv.m_Pos[0];
				rv.m_V = rv.m_Pos[2];
				b.m_Verts.push_back(rv);
			}
			for (uint32 t = 1; t + 1 < (uint32)pv.size(); ++t)
			{
				tris.push_back(nBase);
				tris.push_back(nBase + t);
				tris.push_back(nBase + t + 1);
			}
		}

		std::string sPanName;
		for (size_t k = 0; k < b.m_SecShaders.size(); ++k)
		{
			if (b.m_SecShaders[k] == kShaderLightmapTexture)
			{
				sPanName = b.m_SecNames[k];
				break;
			}
		}

		b.m_SecNames.push_back(sPanName);
		b.m_SecShaders.push_back(kShaderSkypan);
		b.m_SecEffects.push_back(std::string());	// The pan is the effect
		b.m_SecSkypan.push_back(0);					// Identity only (not reread)
		b.m_SecTris.push_back(tris);
		b.m_SecLMs.push_back(SecLM());
	}
}


// Children of a block, in the heap layout the tree uses
void BlockChildren(size_t nIndex, size_t nCount, size_t *pKids, size_t *pNumKids)
{
	*pNumKids = 0;
	size_t a = nIndex * 2 + 1;
	size_t b = nIndex * 2 + 2;
	if (a < nCount) pKids[(*pNumKids)++] = a;
	if (b < nCount) pKids[(*pNumKids)++] = b;
}


// Bounds covering a block and everything beneath it.
// A block is drawn only if every ancestor's bounds pass the frustum test.
bool SubtreeBounds(size_t nIndex, const std::vector<Block> &blocks, float *pMin, float *pMax)
{
	bool bAny = blocks[nIndex].m_bHasBounds;
	if (bAny)
	{
		for (int k = 0; k < 3; ++k)
		{
			pMin[k] = blocks[nIndex].m_Min[k];
			pMax[k] = blocks[nIndex].m_Max[k];
		}
	}

	size_t kids[2], nKids;
	BlockChildren(nIndex, blocks.size(), kids, &nKids);
	for (size_t i = 0; i < nKids; ++i)
	{
		float cmin[3], cmax[3];
		if (!SubtreeBounds(kids[i], blocks, cmin, cmax))
			continue;

		if (!bAny)
		{
			for (int k = 0; k < 3; ++k) { pMin[k] = cmin[k]; pMax[k] = cmax[k]; }
			bAny = true;
			continue;
		}

		for (int k = 0; k < 3; ++k)
		{
			if (cmin[k] < pMin[k]) pMin[k] = cmin[k];
			if (cmax[k] > pMax[k]) pMax[k] = cmax[k];
		}
	}

	return bAny;
}


// Order polygon indices by centroid along one axis (ties broken by the index so the order is total)
struct CentroidLess
{
	const std::vector<BlockPoly> *m_pItems;
	int m_nAxis;

	bool operator()(uint32 a, uint32 b) const
	{
		const double ca = (*m_pItems)[a].m_Centroid[m_nAxis];
		const double cb = (*m_pItems)[b].m_Centroid[m_nAxis];
		if (ca < cb) return true;
		if (cb < ca) return false;
		return a < b;
	}
};

void SortByCentroid(std::vector<uint32> &order, const std::vector<BlockPoly> &items, int nAxis)
{
	CentroidLess cmp;
	cmp.m_pItems = &items;
	cmp.m_nAxis = nAxis;
	std::sort(order.begin(), order.end(), cmp);
}


// Orders occluder candidates by descending area (ties broken by encounter order)
struct OccAreaGreater
{
	const std::vector<double> *m_pArea;

	bool operator()(size_t a, size_t b) const
	{
		const double aa = (*m_pArea)[a];
		const double ab = (*m_pArea)[b];
		if (aa > ab) return true;
		if (ab > aa) return false;
		return a < b;
	}
};


// Split one group of polygon indices in two where half the vertex cost along the longest axis has been passed
void SplitGroup(const std::vector<BlockPoly> &items, const std::vector<uint32> &idxs, std::vector<uint32> &outA, std::vector<uint32> &outB)
{
	outA.clear();
	outB.clear();

	if (idxs.size() < 2)
	{
		outA = idxs;
		return;
	}

	double lo[3], hi[3];
	for (int k = 0; k < 3; ++k) { lo[k] = 1e30; hi[k] = -1e30; }
	for (size_t i = 0; i < idxs.size(); ++i)
	{
		const double *c = items[idxs[i]].m_Centroid;
		for (int k = 0; k < 3; ++k)
		{
			if (c[k] < lo[k]) lo[k] = c[k];
			if (c[k] > hi[k]) hi[k] = c[k];
		}
	}

	int axis = 0;
	double best = hi[0] - lo[0];
	for (int k = 1; k < 3; ++k)
	{
		if ((hi[k] - lo[k]) > best) { best = hi[k] - lo[k]; axis = k; }
	}

	std::vector<uint32> order(idxs);
	SortByCentroid(order, items, axis);

	uint32 nTotal = 0;
	for (size_t i = 0; i < order.size(); ++i)
		nTotal += items[order[i]].m_nCost;

	uint32 nHalf = nTotal / 2;
	uint32 nAcc = 0;
	size_t nCut = 0;
	for (size_t i = 0; i < order.size(); ++i)
	{
		nAcc += items[order[i]].m_nCost;
		if (nAcc > nHalf) { nCut = i + 1; break; }
	}

	// Recursion avoidence
	if (nCut < 1) nCut = 1;
	if (nCut >= order.size()) nCut = order.size() - 1;

	outA.assign(order.begin(), order.begin() + nCut);
	outB.assign(order.begin() + nCut, order.end());
}


// How many leaves to aim for, as a power of two
uint32 TreeLeaves(uint32 nTotalCost, uint32 nMaxBlockVerts)
{
	uint32 nLeaves = 1;
	while (nLeaves < kMaxTreeLeaves && (nTotalCost / nLeaves) > kBlockTargetVerts)
		nLeaves *= 2;
	// The vertex budget always wins over the target
	while ((nTotalCost / nLeaves) > nMaxBlockVerts && nLeaves < kHardTreeLeaves)
		nLeaves *= 2;
	return nLeaves;
}


// Serialise one CD3D_RenderWorld:
// Its blocks, then its nested world models
void EmitRenderWorld(Writer &w, const std::vector<const WorldModel *> &models, const std::vector<const WorldModel *> &nested, WorldV56TexDimsFn pTexDims, void *pUser, WorldV56WarnFn pWarn, void *pWarnUser, uint32 nMaxBlockVerts, const char *pszLabel)
{
	std::vector<Section> sections;
	std::vector<SkyPortal> skyPortals;
	float worldMin[3], worldMax[3];
	BuildSections(models, pTexDims, pUser, sections, worldMin, worldMax, &skyPortals);

	// Partition the polygons into a tree of render blocks laid out as a binary heap with children at 2i+1 and 2i+2.
	// The split is spatial so each block's bounds can reject its subtree, and only the leaves carry geometry.
	std::vector<Block> blocks;
	{
		std::vector<BlockPoly> items;
		for (size_t si = 0; si < sections.size(); ++si)
		{
			const Section &sec = sections[si];
			for (size_t pi = 0; pi + 1 < sec.m_PolyStarts.size(); ++pi)
			{
				uint32 nStart = sec.m_PolyStarts[pi];
				uint32 nEnd = sec.m_PolyStarts[pi + 1];
				uint32 nCount = nEnd - nStart;
				if (nCount < 3) continue;

				// A lightmapped polygon's vertices are emitted once per pass, so it costs double
				const PolyLM &lm = sec.m_PolyLMs[pi];

				BlockPoly bp;
				bp.m_nSection = (uint32)si;
				bp.m_nPoly = (uint32)pi;
				bp.m_nCost = nCount * (lm.m_bHas ? 2 : 1);

				double c[3] = { 0.0, 0.0, 0.0 };
				for (uint32 k = 0; k < nCount; ++k)
				{
					const RenderVert &rv = sec.m_Polys[nStart + k];
					for (int a = 0; a < 3; ++a)
						c[a] += (double)rv.m_Pos[a];
				}
				for (int a = 0; a < 3; ++a)
					bp.m_Centroid[a] = c[a] / (double)nCount;

				items.push_back(bp);
			}
		}

		uint32 nTotal = 0;
		for (size_t i = 0; i < items.size(); ++i)
			nTotal += items[i].m_nCost;

		uint32 nLeaves = items.empty() ? 1 : TreeLeaves(nTotal, nMaxBlockVerts);

		// A complete tree with nLeaves leaves has 2*nLeaves-1 nodes, and the leaves come last
		size_t nCount = (size_t)nLeaves * 2 - 1;
		std::vector<std::vector<uint32> > groups(nCount);

		groups[0].reserve(items.size());
		for (size_t i = 0; i < items.size(); ++i)
			groups[0].push_back((uint32)i);

		for (size_t i = 0; i + 1 < (size_t)nLeaves; ++i)
		{
			std::vector<uint32> a, b;
			SplitGroup(items, groups[i], a, b);
			groups[i * 2 + 1].swap(a);
			groups[i * 2 + 2].swap(b);
			groups[i].clear(); // Interior blocks carry no geometry
		}

		blocks.resize(nCount);
		for (size_t bi = 0; bi < nCount; ++bi)
		{
			Block &cur = blocks[bi];
			const std::vector<uint32> &g = groups[bi];

			// The vertex array is laid out section by section, so the first pass buckets polygons by section
			std::vector<std::vector<uint32> > secPolys;
			for (size_t gi = 0; gi < g.size(); ++gi)
			{
				const BlockPoly &bp = items[g[gi]];
				const Section &sec = sections[bp.m_nSection];

				// Texture, shader, and effect are the section's key, 
				// so that translucent and opaque sections stay apart
				bool bFound = false;
				size_t nSec = 0;
				for (size_t k = 0; k < cur.m_SecNames.size(); ++k)
				{
					if (cur.m_SecNames[k] == sec.m_sTexture && cur.m_SecShaders[k] == sec.m_nShader && cur.m_SecEffects[k] == sec.m_sEffectName && cur.m_SecSkypan[k] == (sec.m_bSkypan ? 1 : 0))
					{
						bFound = true;
						nSec = k;
						break;
					}
				}
				if (!bFound)
				{
					nSec = cur.m_SecNames.size();
					cur.m_SecNames.push_back(sec.m_sTexture);
					cur.m_SecEffects.push_back(sec.m_sEffectName);
					cur.m_SecShaders.push_back(sec.m_nShader);
					cur.m_SecSkypan.push_back(sec.m_bSkypan ? 1 : 0);
					cur.m_SecTris.resize(cur.m_SecTris.size() + 1);
					secPolys.resize(secPolys.size() + 1);
				}
				secPolys[nSec].push_back(g[gi]);
			}

			// Second pass: emit each section's polygons together, 
			// collecting occluder candidates in the same order
			std::vector<size_t> occOrder; // index into items
			std::vector<double> occArea;
			for (size_t nSec = 0; nSec < secPolys.size(); ++nSec)
			{
				for (size_t k2 = 0; k2 < secPolys[nSec].size(); ++k2)
				{
					const BlockPoly &bp = items[secPolys[nSec][k2]];
					const Section &sec = sections[bp.m_nSection];

					uint32 nStart = sec.m_PolyStarts[bp.m_nPoly];
					uint32 nEnd = sec.m_PolyStarts[bp.m_nPoly + 1];
					uint32 nCount2 = nEnd - nStart;
					const PolyLM &lm = sec.m_PolyLMs[bp.m_nPoly];

					const PolyOcc &po = sec.m_PolyOcc[bp.m_nPoly];
					if (po.m_bEligible)
					{
						occOrder.push_back(secPolys[nSec][k2]);
						occArea.push_back(po.m_Area);
					}

					uint32 nBase = (uint32)cur.m_Verts.size();
					for (uint32 k = 0; k < nCount2; ++k)
						cur.m_Verts.push_back(sec.m_Polys[nStart + k]);

					for (uint32 k = 1; k + 1 < nCount2; ++k)
					{
						cur.m_SecTris[nSec].push_back(nBase);
						cur.m_SecTris[nSec].push_back(nBase + k);
						cur.m_SecTris[nSec].push_back(nBase + k + 1);
					}

					if (lm.m_bHas)
					{
						cur.m_LMPolys.resize(cur.m_LMPolys.size() + 1);
						BlockLMPoly &lp = cur.m_LMPolys.back();
						lp.m_Verts.assign(sec.m_Polys.begin() + nStart, sec.m_Polys.begin() + nEnd);
						lp.m_LM = lm;
					}

					if (sec.m_bSkypan)
					{
						cur.m_SkypanPolys.resize(cur.m_SkypanPolys.size() + 1);
						cur.m_SkypanPolys.back().assign( sec.m_Polys.begin() + nStart, sec.m_Polys.begin() + nEnd);
					}
				}
			}

			{
				std::vector<size_t> pick(occOrder.size());
				for (size_t i = 0; i < pick.size(); ++i) pick[i] = i;

				OccAreaGreater cmp;
				cmp.m_pArea = &occArea;
				std::sort(pick.begin(), pick.end(), cmp);

				size_t nTake = pick.size();
				if (nTake > kOccludersPerBlock) nTake = kOccludersPerBlock;

				for (size_t i = 0; i < nTake; ++i)
				{
					const BlockPoly &bp = items[occOrder[pick[i]]];
					const Section &sec = sections[bp.m_nSection];
					const PolyOcc &po = sec.m_PolyOcc[bp.m_nPoly];

					uint32 nStart = sec.m_PolyStarts[bp.m_nPoly];
					uint32 nEnd = sec.m_PolyStarts[bp.m_nPoly + 1];

					cur.m_Occluders.resize(cur.m_Occluders.size() + 1);
					BlockOccluder &bo = cur.m_Occluders.back();
					for (uint32 v = nStart; v < nEnd; ++v)
					{
						bo.m_Verts.push_back(sec.m_Polys[v].m_Pos[0]);
						bo.m_Verts.push_back(sec.m_Polys[v].m_Pos[1]);
						bo.m_Verts.push_back(sec.m_Polys[v].m_Pos[2]);
					}
					for (int k = 0; k < 3; ++k) bo.m_Normal[k] = po.m_Normal[k];
					bo.m_Dist = po.m_Dist;
				}
			}
		}
	}

	// Drop sections that produced no triangles, which would trip the engine's assert and compute each block's bounds.
	// Blocks with no sections are kept since dropping them would renumber the heap.
	{
		bool bAnySections = false;

		for (size_t i = 0; i < blocks.size(); ++i)
		{
			Block &b = blocks[i];

			std::vector<std::string> names;
			std::vector<std::string> effects;
			std::vector<uint8> shaders;
			std::vector<uint8> skypan;
			std::vector<std::vector<uint32> > tris;
			for (size_t k = 0; k < b.m_SecNames.size(); ++k)
			{
				if (b.m_SecTris[k].empty()) continue;
				names.push_back(b.m_SecNames[k]);
				effects.push_back(b.m_SecEffects[k]);
				shaders.push_back(b.m_SecShaders[k]);
				skypan.push_back(b.m_SecSkypan[k]);
				tris.push_back(b.m_SecTris[k]);
			}

			b.m_SecNames = names;
			b.m_SecEffects = effects;
			b.m_SecShaders = shaders;
			b.m_SecSkypan = skypan; // Stays parallel - nothing reads it past here
			b.m_SecTris = tris;

			if (!names.empty())
				bAnySections = true;

			// Texture sections carry no page. The atlas step appends the lightmap sections.
			b.m_SecLMs.assign(b.m_SecNames.size(), SecLM());
			BuildBlockBasePassSections(b);

			b.m_bHasBounds = !b.m_Verts.empty();
			for (int k = 0; k < 3; ++k) { b.m_Min[k] = 1e30f; b.m_Max[k] = -1e30f; }
			for (size_t v = 0; v < b.m_Verts.size(); ++v)
			{
				for (int k = 0; k < 3; ++k)
				{
					if (b.m_Verts[v].m_Pos[k] < b.m_Min[k]) b.m_Min[k] = b.m_Verts[v].m_Pos[k];
					if (b.m_Verts[v].m_Pos[k] > b.m_Max[k]) b.m_Max[k] = b.m_Verts[v].m_Pos[k];
				}
			}
		}

		// A render world with no drawn geometry emits no blocks
		if (!bAnySections)
			blocks.clear();
	}

	if (pWarn)
	{
		for (size_t i = 0; i < blocks.size(); ++i)
		{
			if (blocks[i].m_Verts.size() <= nMaxBlockVerts) continue;
			std::string sMsg = "world_v56: ";
			sMsg += (pszLabel && pszLabel[0]) ? pszLabel : "root";
			sMsg += " block still holds ";
			AppendUInt(sMsg, (uint32)blocks[i].m_Verts.size());
			sMsg += " verts after splitting - a single polygon cannot be divided";
			pWarn(sMsg.c_str(), pWarnUser);
		}
	}

	// uint32 count, then per portal: uint8 vert count, the verts, and the plane
	struct PortalWriter
	{
		static void Write(Writer &w, const std::vector<SkyPortal> &portals)
		{
			w.U32((uint32)portals.size());
			for (size_t i = 0; i < portals.size(); ++i)
			{
				const SkyPortal &sp = portals[i];
				uint32 nVerts = (uint32)(sp.m_Verts.size() / 3);
				w.U8((uint8)nVerts);
				for (uint32 v = 0; v < nVerts; ++v)
					w.V3(&sp.m_Verts[(size_t)v * 3]);
				w.V3(sp.m_Normal);
				w.F32(sp.m_Dist);
			}
		}
	};

	// A world with only sky portals gets one geometry-less block to carry them
	if (blocks.empty() && !skyPortals.empty())
	{
		float pmin[3] = { 1e30f, 1e30f, 1e30f };
		float pmax[3] = { -1e30f, -1e30f, -1e30f };
		for (size_t i = 0; i < skyPortals.size(); ++i)
			for (size_t v = 0; v + 2 < skyPortals[i].m_Verts.size(); v += 3)
				for (int k = 0; k < 3; ++k)
				{
					float f = skyPortals[i].m_Verts[v + k];
					if (f < pmin[k]) pmin[k] = f;
					if (f > pmax[k]) pmax[k] = f;
				}
		w.U32(1);
		float centre[3], half[3];
		for (int k = 0; k < 3; ++k)
		{
			centre[k] = (pmin[k] + pmax[k]) * 0.5f;
			half[k] = (pmax[k] - pmin[k]) * 0.5f;
			if (half[k] < 1.0f) half[k] = 1.0f;
		}
		w.V3(centre); w.V3(half);
		w.U32(0);	// Sections
		w.U32(0);	// Verts
		w.U32(0);	// Tris
		PortalWriter::Write(w, skyPortals);
		w.U32(0);	// Occluders
		w.U32(0);	// Light groups
		w.U8(0);
		w.U32(0xFFFFFFFFu); w.U32(0xFFFFFFFFu);
		// The nested model list is written by the shared code below
	}
	else
	w.U32((uint32)blocks.size());

	for (size_t bi = 0; bi < blocks.size(); ++bi)
	{
		const Block &b = blocks[bi];

		float bmin[3], bmax[3];
		if (!SubtreeBounds(bi, blocks, bmin, bmax))
		{
			// A subtree with no geometry beneath it
			for (int k = 0; k < 3; ++k) { bmin[k] = -1.0f; bmax[k] = 1.0f; }
		}

		float centre[3], half[3];
		for (int k = 0; k < 3; ++k)
		{
			centre[k] = (bmin[k] + bmax[k]) * 0.5f;
			half[k] = (bmax[k] - bmin[k]) * 0.5f;
			if (half[k] < 1.0f) half[k] = 1.0f;
		}

		w.V3(centre);
		w.V3(half);
		w.U32((uint32)b.m_SecNames.size());
		for (size_t i = 0; i < b.m_SecNames.size(); ++i)
		{
			w.LTString(b.m_SecNames[i]);
			w.LTString(std::string());
			w.U8(b.m_SecShaders[i]); // ePCShader_* code
			w.U32((uint32)(b.m_SecTris[i].size() / 3));

			// The section's texture effect name, which CTextureScriptMgr resolves to a texture matrix
			w.LTString(i < b.m_SecEffects.size() ? b.m_SecEffects[i] : std::string());
			const SecLM &lmOut = b.m_SecLMs[i];
			if (!lmOut.m_Data.empty())
			{
				w.U32(lmOut.m_nW);
				w.U32(lmOut.m_nH);
				w.U32((uint32)lmOut.m_Data.size());
				w.Raw(&lmOut.m_Data[0], lmOut.m_Data.size());
			}
			else
			{
				w.U32(0); w.U32(0); w.U32(0); // Lightmap w, h, size
			}
		}

		w.U32((uint32)b.m_Verts.size());
		for (size_t i = 0; i < b.m_Verts.size(); ++i)
		{
			w.V3(b.m_Verts[i].m_Pos);
			w.F32(b.m_Verts[i].m_U);
			w.F32(b.m_Verts[i].m_V);
			w.F32(0.0f); w.F32(0.0f);
			w.U32(b.m_Verts[i].m_Color);
			w.V3(0.0f, 1.0f, 0.0f); // Placeholder normal
		}

		size_t nTotalTris = 0;
		for (size_t i = 0; i < b.m_SecTris.size(); ++i) nTotalTris += b.m_SecTris[i].size() / 3;

		w.U32((uint32)nTotalTris);
		for (size_t i = 0; i < b.m_SecTris.size(); ++i)
		{
			const std::vector<uint32> &t = b.m_SecTris[i];
			for (size_t k = 0; k + 2 < t.size(); k += 3)
			{
				w.U32(t[k]); w.U32(t[k + 1]); w.U32(t[k + 2]);
				w.U32(0);
			}
		}

		// Sky portals go in the root block whose bounds cover the whole tree
		if (bi == 0)
			PortalWriter::Write(w, skyPortals);
		else
			w.U32(0);

		// Occluders: a geometry poly then a uint32 id which is the index
		w.U32((uint32)b.m_Occluders.size());
		for (size_t oi = 0; oi < b.m_Occluders.size(); ++oi)
		{
			const BlockOccluder &bo = b.m_Occluders[oi];
			uint32 nVerts = (uint32)(bo.m_Verts.size() / 3);
			w.U8((uint8)nVerts);
			for (uint32 v = 0; v < nVerts; ++v)
				w.V3(&bo.m_Verts[v * 3]);
			w.V3(bo.m_Normal);
			w.F32(bo.m_Dist);
			w.U32((uint32)oi);
		}

		w.U32(0); // Per block light groups

		// Children, in heap order. 
		// The flags say which slots are real
		// 0xFFFFFFFF means none
		size_t kids[2], nKids;
		BlockChildren(bi, blocks.size(), kids, &nKids);

		uint8 nFlags = 0;
		for (size_t i = 0; i < nKids; ++i) nFlags |= (uint8)(1 << i);
		w.U8(nFlags);
		for (size_t i = 0; i < 2; ++i)
			w.U32(i < nKids ? (uint32)kids[i] : 0xFFFFFFFFu);
	}

	// Nested world models: a name plus a render world each so the non-Root brushes stay out of the main block
	w.U32((uint32)nested.size());
	for (size_t i = 0; i < nested.size(); ++i)
	{
		w.LTString(nested[i]->m_sName);
		std::vector<const WorldModel *> one(1, nested[i]);
		std::vector<const WorldModel *> none;
		EmitRenderWorld(w, one, none, pTexDims, pUser, pWarn, pWarnUser, nMaxBlockVerts, nested[i]->m_sName.c_str());
	}
}


// LightTableRes out of the world info string (or dDefault when the world does not carry one)
static double ParseLT1LightTableRes(const std::string &sInfo, double dDefault)
{
	static const char szKey[] = "LightTableRes";
	size_t i = sInfo.find(szKey);
	if (i == std::string::npos) return dDefault;

	std::string rest = sInfo.substr(i + sizeof(szKey) - 1);
	for (size_t k = 0; k < rest.size(); ++k)
		if (rest[k] == ';') rest[k] = ' ';

	size_t pos = 0;
	while (pos < rest.size() && (rest[pos] == ' ' || rest[pos] == '\t' || rest[pos] == '\r' || rest[pos] == '\n')) ++pos;
	size_t end = pos;
	while (end < rest.size() && !(rest[end] == ' ' || rest[end] == '\t' || rest[end] == '\r' || rest[end] == '\n')) ++end;
	if (end <= pos) return dDefault;

	char *pEnd = NULL;
	std::string tok = rest.substr(pos, end - pos);
	double v = strtod(tok.c_str(), &pEnd);
	if (pEnd == tok.c_str() || *pEnd != '\0') return dDefault;
	if (v <= 0.0) return dDefault;
	return v;
}


// The world info's AmbientLight
// Clamped to 0-255 and black when absent as in LT1
static void ParseLT1AmbientLight(const std::string &sInfo, uint8 *pOut)
{
	pOut[0] = pOut[1] = pOut[2] = 0;
	static const char szKey[] = "AmbientLight";
	size_t i = sInfo.find(szKey);
	if (i == std::string::npos) return;

	std::string rest = sInfo.substr(i + sizeof(szKey) - 1);
	for (size_t k = 0; k < rest.size(); ++k)
		if (rest[k] == ';') rest[k] = ' ';

	size_t pos = 0;
	for (int k = 0; k < 3; ++k)
	{
		while (pos < rest.size() && (rest[pos] == ' ' || rest[pos] == '\t' || rest[pos] == '\r' || rest[pos] == '\n')) ++pos;
		size_t end = pos;
		while (end < rest.size() && !(rest[end] == ' ' || rest[end] == '\t' || rest[end] == '\r' || rest[end] == '\n')) ++end;
		double v = 0.0;
		if (end > pos)
		{
			char *pEnd = NULL;
			std::string tok = rest.substr(pos, end - pos);
			v = strtod(tok.c_str(), &pEnd);
			if (pEnd == tok.c_str() || *pEnd != '\0') v = 0.0;
		}
		if (v < 0.0) v = 0.0;
		if (v > 255.0) v = 255.0;
		pOut[k] = (uint8)(int)v;
		pos = end;
	}
}

struct LT1Light
{
	float m_Pos[3];
	float m_Color[3];
	float m_fRadius;
};

// Every light LT1's grid builder accepts in file order
static void ParseLT1Lights(const std::vector<uint8> &block, std::vector<LT1Light> &out)
{
	out.clear();
	if (block.size() < 4) return;
	Reader r(&block[0], (uint32)block.size());
	uint32 nCount = r.U32();
	for (uint32 i = 0; i < nCount && !r.Bad(); ++i)
	{
		uint32 nStart = r.Pos();
		uint16 nDataLen = r.U16();
		uint32 nBodyEnd = nStart + 2 + nDataLen;
		std::string sType = r.S16();
		uint32 nProps = r.U32();

		bool bHavePos = false, bHaveColor = false, bHaveRadius = false;
		bool bLightObjects = true;
		LT1Light l;
		for (uint32 k = 0; k < nProps && !r.Bad(); ++k)
		{
			std::string sName = r.S16();
			uint8 nCode = r.U8();
			r.U32(); // property flags
			uint16 nLen = r.U16();
			const uint8 *pData = r.Raw(nLen);
			if (!pData) break;
			if (nCode == 1 && nLen == 12 && sName == "Pos")
			{
				memcpy(l.m_Pos, pData, 12); bHavePos = true;
			}
			else if (nCode == 2 && nLen == 12 && (sName == "LightColor" || sName == "InnerColor"))
			{
				memcpy(l.m_Color, pData, 12); bHaveColor = true;
			}
			else if (nCode == 3 && nLen == 4 && sName == "LightRadius")
			{
				memcpy(&l.m_fRadius, pData, 4); bHaveRadius = true;
			}
			else if (nCode == 5 && nLen >= 1 && sName == "LightObjects")
			{
				bLightObjects = pData[0] != 0;
			}
		}
		r.SeekTo(nBodyEnd);
		if ((sType == "Light" || sType == "DirLight" || sType == "ObjectLight") && bLightObjects && bHavePos && bHaveColor && bHaveRadius)
		{
			out.push_back(l);
		}
	}
}

static void BuildLT1LightGrid(double kLightGridCell, double kLightGridMargin, const float *pMin, const float *pMax, const uint8 *pAmbient, const std::vector<LT1Light> &lights, float *pOrigin, int32 *pDims, std::vector<uint8> &cells)
{
	double origin[3], far[3];
	for (int k = 0; k < 3; ++k)
	{
		origin[k] = (double)pMin[k] - kLightGridMargin;
		far[k] = (double)pMax[k] + kLightGridMargin;
		int32 n = (int32)((far[k] - origin[k]) / kLightGridCell) + 1;
		pDims[k] = n < 1 ? 1 : n;
		pOrigin[k] = (float)origin[k];
	}
	const int32 nx = pDims[0], ny = pDims[1], nz = pDims[2];
	cells.resize((size_t)nx * ny * nz * 3);
	for (size_t c = 0; c < cells.size(); c += 3)
	{
		cells[c] = pAmbient[0]; cells[c + 1] = pAmbient[1]; cells[c + 2] = pAmbient[2];
	}

	for (size_t li = 0; li < lights.size(); ++li)
	{
		const double lx = lights[li].m_Pos[0], ly = lights[li].m_Pos[1], lz = lights[li].m_Pos[2];
		const double radius = lights[li].m_fRadius;
		// A light with no finite position or radius is skipped
		if (radius <= 0.0 || !(lx == lx && ly == ly && lz == lz && radius == radius) || fabs(lx) > 1e30 || fabs(ly) > 1e30 || fabs(lz) > 1e30 || fabs(radius) > 1e30)
			continue;
		const double r2 = radius * radius;
		const double col[3] = { lights[li].m_Color[0], lights[li].m_Color[1], lights[li].m_Color[2] };

		int32 x0 = (int32)((lx - radius - origin[0]) / kLightGridCell); if (x0 < 0) x0 = 0;
		int32 y0 = (int32)((ly - radius - origin[1]) / kLightGridCell); if (y0 < 0) y0 = 0;
		int32 z0 = (int32)((lz - radius - origin[2]) / kLightGridCell); if (z0 < 0) z0 = 0;
		int32 x1 = (int32)((lx + radius - origin[0]) / kLightGridCell) + 1; if (x1 > nx - 1) x1 = nx - 1;
		int32 y1 = (int32)((ly + radius - origin[1]) / kLightGridCell) + 1; if (y1 > ny - 1) y1 = ny - 1;
		int32 z1 = (int32)((lz + radius - origin[2]) / kLightGridCell) + 1; if (z1 > nz - 1) z1 = nz - 1;

		for (int32 z = z0; z <= z1; ++z)
		{
			const double dz = origin[2] + z * kLightGridCell - lz;
			for (int32 y = y0; y <= y1; ++y)
			{
				const double dy = origin[1] + y * kLightGridCell - ly;
				const size_t row = ((size_t)z * ny + y) * nx;
				for (int32 x = x0; x <= x1; ++x)
				{
					const double dx = origin[0] + x * kLightGridCell - lx;
					const double d2 = dx * dx + dy * dy + dz * dz;
					if (d2 >= r2) continue;
					const double f = 1.0 - sqrt(d2) / radius;
					uint8 *pCell = &cells[(row + x) * 3];
					for (int k = 0; k < 3; ++k)
					{
						double v = (double)pCell[k] + col[k] * f;
						if (v < 0.0) v = 0.0;
						if (v > 255.0) v = 255.0;
						pCell[k] = (uint8)(int)v;
					}
				}
			}
		}
	}
}


// Serialise one world model in the v85 WorldBsp layout
void WriteWorldModel(Writer &w, const WorldModel &m, const std::string &sName, bool bRoot)
{
	w.U32(V85WorldInfoFlags(bRoot));
	w.LTString(sName);

	// Texture name block: null terminated names and their total length
	std::string names;
	for (size_t i = 0; i < m.m_Textures.size(); ++i)
	{
		names += m.m_Textures[i];
		names += '\0';
	}

	// v56 polygons carry no plane index.
	// As such plane is computed per polygon and nPlanes equals nPolies
	std::vector<float> planes; // 4 floats each
	planes.resize(m.m_Polies.size() * 4);
	for (size_t i = 0; i < m.m_Polies.size(); ++i)
	{
		std::vector<const float *> pts;
		const Poly &p = m.m_Polies[i];
		for (size_t k = 0; k < p.m_Verts.size(); ++k)
		{
			uint32 nPoint = p.m_Verts[k].m_nVert;
			if (nPoint < m.m_nPoints) pts.push_back(&m.m_Points[(size_t)nPoint * 3]);
		}
		PlaneFromPoints(pts, &planes[i * 4], &planes[i * 4 + 3]);
	}

	uint32 nTotalVerts = 0;
	for (size_t i = 0; i < m.m_PolyVertCounts.size(); ++i) nTotalVerts += m.m_PolyVertCounts[i];

	w.U32(m.m_nPoints);
	w.U32((uint32)m.m_Polies.size()); // nPlanes == nPolies
	w.U32(m.m_nSurfaces);
	// The loader has to consume the user portal records below or it desynchronises at the texture list
	w.U32((uint32)m.m_UserPortals.size()); // nUserPortals
	w.U32(m.m_nPolies);
	w.U32(m.m_nLeaves);
	w.U32(nTotalVerts);
	// A non zero totalVisListSize is the loader's flag that a vis tree follows the leaf lists
	uint32 nVisBytes = 0, nVisLists = 0;
	if (m.m_bHasVisPrune)
	{
		for (size_t i = 0; i < m.m_Leaves.size(); ++i)
		{
			const Leaf &lf = m.m_Leaves[i];
			if (lf.m_nCount == 0xFFFF) continue;
			nVisLists += (uint32)lf.m_Lists.size();
			for (size_t k = 0; k < lf.m_Lists.size(); ++k)
				nVisBytes += (uint32)lf.m_Lists[k].m_Payload.size();
		}
	}
	w.U32(nVisBytes);	// totalVisListSize
	w.U32(nVisLists);	// nLeafLists
	w.U32(m.m_nNodes);

	// The bounds of this model's own points
	float boxMin[3], boxMax[3];
	if (m.m_nPoints)
	{
		for (int k = 0; k < 3; ++k) { boxMin[k] = 1e30f; boxMax[k] = -1e30f; }
		for (uint32 i = 0; i < m.m_nPoints; ++i)
		{
			const float *pt = &m.m_Points[(size_t)i * 3];
			for (int k = 0; k < 3; ++k)
			{
				if (pt[k] < boxMin[k]) boxMin[k] = pt[k];
				if (pt[k] > boxMax[k]) boxMax[k] = pt[k];
			}
		}
	}
	else
	{
		memcpy(boxMin, m.m_MinBox, 12);
		memcpy(boxMax, m.m_MaxBox, 12);
	}

	w.V3(boxMin);
	w.V3(boxMax);

	// The engine puts the collision box's centre at m_WorldTranslation, so the root is recentred on its box.
	if (bRoot)
	{
		float centre[3];
		for (int k = 0; k < 3; ++k)
			centre[k] = (float)(((double)boxMin[k] + (double)boxMax[k]) * 0.5);
		w.V3(centre);
	}
	else
	{
		w.V3(m.m_Translation);
	}

	// The user portal records
	// Each the v56 one verbatim
	for (size_t i = 0; i < m.m_UserPortals.size(); ++i)
	{
		const WorldModel::UserPortal &up = m.m_UserPortals[i];
		w.LTString(up.m_sName);
		w.U32(up.m_nUnknown32);
		w.U16(up.m_nUnknown16);
		w.V3(up.m_Centre);
		w.V3(up.m_Dims);
	}

	w.U32((uint32)names.size());
	w.U32((uint32)m.m_Textures.size());
	w.Raw(names.data(), names.size());

	// Per poly vertex counts
	// A single byte each since WorldBsp::Load reads a uint8
	for (size_t i = 0; i < m.m_PolyVertCounts.size(); ++i)
		w.U8((uint8)(m.m_PolyVertCounts[i] > 255 ? 255 : m.m_PolyVertCounts[i]));

	// Leaves
	// The v85 loader only skips over these, but the shape must match.
	for (size_t i = 0; i < m.m_Leaves.size(); ++i)
	{
		const Leaf &leaf = m.m_Leaves[i];
		if (leaf.m_nCount == 0xFFFF)
		{
			w.U16(0xFFFF);
			w.U16(leaf.m_nAlias);
		}
		else
		{
			w.U16((uint16)leaf.m_Lists.size());
			for (size_t k = 0; k < leaf.m_Lists.size(); ++k)
			{
				w.U16(leaf.m_Lists[k].m_nPortalId);
				w.U16((uint16)leaf.m_Lists[k].m_Payload.size());
				if (!leaf.m_Lists[k].m_Payload.empty())
					w.Raw(&leaf.m_Lists[k].m_Payload[0], leaf.m_Lists[k].m_Payload.size());
			}
		}
	}

	// The vis tree immediately after the lists it indexes
	if (nVisBytes)
		WriteVisTree(w, m);

	// Planes: LTPlane is normal + distance
	for (size_t i = 0; i < m.m_Polies.size(); ++i)
	{
		w.V3(&planes[i * 4]);
		w.F32(planes[i * 4 + 3]);
	}

	// Surfaces: SDiskSurface is flags, texture, textureFlags
	for (size_t i = 0; i < m.m_Surfaces.size(); ++i)
	{
		w.U32(m.m_Surfaces[i].m_nFlags);
		w.U16(m.m_Surfaces[i].m_nTexture);
		w.U16(m.m_Surfaces[i].m_nTexFlags);
	}

	// Polygons: SDiskPoly is surface, plane, then one uint32 per vertex
	for (size_t i = 0; i < m.m_Polies.size(); ++i)
	{
		uint32 nSurf = m.m_Polies[i].m_nSurface;
		if (nSurf >= m.m_nSurfaces) nSurf = 0;
		w.U32(nSurf);
		w.U32((uint32)i);
		for (size_t k = 0; k < m.m_Polies[i].m_Verts.size(); ++k)
		{
			uint32 nPoint = m.m_Polies[i].m_Verts[k].m_nVert;
			w.U32(nPoint < m.m_nPoints ? nPoint : 0);
		}
	}

	// Nodes
	for (size_t i = 0; i < m.m_Nodes.size(); ++i)
	{
		w.U32(m.m_Nodes[i].m_nPoly < m.m_nPolies ? m.m_Nodes[i].m_nPoly : 0);
		w.U16(m.m_Nodes[i].m_nLeaf);
		w.I32(m.m_Nodes[i].m_Child0);
		w.I32(m.m_Nodes[i].m_Child1);
	}

	// Points
	for (uint32 i = 0; i < m.m_nPoints; ++i)
		w.V3(&m.m_Points[(size_t)i * 3]);

	w.I32(m.m_nRootNode);
	w.U32(0); // nSections
}

}


// The whole model translucency verdict for each model.
// The main world is never blended, and any other model is blended if its polygon 0 carries SURF_TRANSPARENT.
static void StampWholeModelTranslucency(SourceWorld &src)
{
	for (size_t i = 0; i < src.m_Models.size(); ++i)
	{
		WorldModel &m = src.m_Models[i];
		if (i == 0 || m.m_Polies.empty() || m.m_Polies[0].m_nSurface >= m.m_Surfaces.size())
		{
			m.m_nLT1Translucent = 0;
		}
		else
		{
			const Surface &s0 = m.m_Surfaces[m.m_Polies[0].m_nSurface];
			m.m_nLT1Translucent = (s0.m_nFlags & kSurfTransparent) ? 1 : 0;
		}
	}
}


LTRESULT world_v56_BuildV85Image(const uint8 *pSrc, uint32 nSrcSize, uint8 **ppImage, uint32 *pImageSize, WorldV56TexDimsFn pTexDims, void *pUser, WorldV56WarnFn pWarn, void *pWarnUser, uint32 nMaxBlockVerts)
{
	if (!pSrc || !ppImage || !pImageSize) return LT_INVALIDPARAMS;

	*ppImage = NULL;
	*pImageSize = 0;

	SourceWorld src;
	if (!ReadV56(pSrc, nSrcSize, src)) return LT_INVALIDFILE;
	if (src.m_Models.empty()) return LT_INVALIDFILE;

	StampWholeModelTranslucency(src);

	// Extents come from every model, so the world box and world tree cover the whole level
	std::vector<const WorldModel *> all;
	for (size_t i = 0; i < src.m_Models.size(); ++i) all.push_back(&src.m_Models[i]);

	float bmin[3], bmax[3];
	{
		std::vector<Section> scratch;
		BuildSections(all, pTexDims, pUser, scratch, bmin, bmax, NULL);
	}

	Writer w;
	w.U32(85);

	size_t nOffField = w.Size();
	for (int i = 0; i < 6; ++i) w.U32(0);
	for (int i = 0; i < 8; ++i) w.U32(0);

	// The info string goes through as LT1 wrote it, and its AmbientLight seeds the light grid below
	std::string sInfo = src.m_sWorldInfo;

	w.U32((uint32)sInfo.size());
	w.Raw(sInfo.data(), sInfo.size());
	w.V3(bmin);
	w.V3(bmax);
	w.V3(0.0f, 0.0f, 0.0f); // World offset

	// World tree: a uniform quadtree over the level box
	{
		std::vector<uint8> layout;
		uint32 nWTNodes = WorldTreeLayout(bmin, bmax, layout);
		w.V3(bmin);
		w.V3(bmax);
		w.U32(nWTNodes);	// Node count
		w.U32(0);			// Dummy terrain depth
		if (!layout.empty()) w.Raw(&layout[0], layout.size());
	}

	// World models
	// Objects bind to these by name, meaning an empty placeholder fails with LT_MISSINGWORLDMODEL.
	w.U32((uint32)src.m_Models.size());
	for (size_t i = 0; i < src.m_Models.size(); ++i)
	{
		w.U32(0); // per-model dummy uint32
		WriteWorldModel(w, src.m_Models[i], i == 0 ? std::string("Root") : src.m_Models[i].m_sName, i == 0);
	}

	uint32 nObjectPos = (uint32)w.Size();
	// The object block goes through untouched. FastLightObjects false would add lights on top of the grid otherwise
	if (!src.m_ObjectBlock.empty())
		w.Raw(&src.m_ObjectBlock[0], src.m_ObjectBlock.size());

	uint32 nBlindPos = (uint32)w.Size();
	w.U32(0);

	uint32 nLightGridPos = (uint32)w.Size();
	// LT1's object light grid
	// Rebuilt from the objects over the root model's box
	{
		uint8 ambient[3];
		ParseLT1AmbientLight(sInfo, ambient);
		std::vector<LT1Light> lights;
		ParseLT1Lights(src.m_ObjectBlock, lights);

		// The cell size is the world's own LightTableRes
		const double dCell = ParseLT1LightTableRes(sInfo, kLightGridCell);

		float origin[3]; int32 dims[3];
		std::vector<uint8> cells, grid;
		BuildLT1LightGrid(dCell, kLightGridMargin, src.m_Models[0].m_MinBox, src.m_Models[0].m_MaxBox, ambient, lights, origin, dims, cells);
		w.V3(origin); // m_vWorldBasePos
		w.V3((float)dCell, (float)dCell, (float)dCell);
		w.I32(dims[0]); w.I32(dims[1]); w.I32(dims[2]);
		CompressLM(cells, grid); // Same RLE as CLightTable::Load
		w.U32((uint32)grid.size());
		if (!grid.empty())
			w.Raw(&grid[0], grid.size());
	}

	uint32 nCollisionPos = (uint32)w.Size();
	w.U32(0); w.U32(0);

	uint32 nParticlePos = (uint32)w.Size();
	w.U32(0); w.U32(0);

	uint32 nRenderPos = (uint32)w.Size();
	{
		// Root supplies the main render world, and every other world model becomes a nested one keyed by name
		std::vector<const WorldModel *> root(1, &src.m_Models[0]);
		std::vector<const WorldModel *> nested;
		for (size_t i = 1; i < all.size(); ++i)
			nested.push_back(all[i]);
		EmitRenderWorld(w, root, nested, pTexDims, pUser, pWarn, pWarnUser, nMaxBlockVerts, "");
	}
	w.U32(0); // Light group count

	const uint32 offsets[6] = { nObjectPos, nBlindPos, nLightGridPos, nCollisionPos, nParticlePos, nRenderPos };
	for (int i = 0; i < 6; ++i)
		w.PatchU32(nOffField + (size_t)i * 4, offsets[i]);

	uint32 nSize = (uint32)w.Size();
	uint8 *pImage = new uint8[nSize];
	if (!pImage) return LT_OUTOFMEMORY;
	memcpy(pImage, w.Data(), nSize);

	*ppImage = pImage;
	*pImageSize = nSize;
	return LT_OK;
}

bool world_v56_IsV56(const uint8 *pData, uint32 nSize)
{
	if (!pData || nSize < 4) return false;
	uint32 nVersion = 0;
	memcpy(&nVersion, pData, 4);
	return nVersion == 56;
}

void world_v56_FreeImage(uint8 *pImage)
{
	delete[] pImage;
}
