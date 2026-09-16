#include "bdefs.h"
#include "model.h"
#include "model_abc.h"
#include "ltb.h"
#include "iltstream.h"

#include <math.h>
#include <string.h>

// D3D vertex stream flags
#define ABC_VERTDATATYPE_POSITION   0x0001
#define ABC_VERTDATATYPE_NORMAL     0x0002
#define ABC_VERTDATATYPE_UVSETS_1   0x0010

#define ABC_STREAM_FLAGS  (ABC_VERTDATATYPE_POSITION | \
                           ABC_VERTDATATYPE_NORMAL   | \
                           ABC_VERTDATATYPE_UVSETS_1)

// float3 position + float3 normal + float2 uv
#define ABC_VERTEX_SIZE   32

// CRenderObject::RENDER_OBJECT_TYPES
#define ABC_RO_RIGIDMESH  4
#define ABC_RO_SKELMESH   5
#define ABC_RO_VAMESH     6   // CRenderObject::eVAMesh

#define ABC_FILE_TOKEN    "MonolithExport Model File v6"

// 1 mirrors model data in z at load, as Shogo's renderer did
// 0 leaves it as authored. 
int32 g_bLT1ModelMirror = 1;

// LT1 node flags
#define ABC_NODEFLAG_NULL         1
#define ABC_NODEFLAG_TRIANGLES    2
#define ABC_NODEFLAG_DEFORMATION  4

#define ABC_MAX_NODES       256
#define ABC_MAX_VERTS       65535
#define ABC_MAX_TRIS        65535
#define ABC_MAX_ANIMS       512
#define ABC_MAX_KEYFRAMES   4096


// Bounds-checked sequential reader over a memory buffer
class CABCReader
{
public:

	CABCReader(const uint8 *pData, uint32 len)
		: m_pData(pData), m_Len(len), m_Pos(0), m_bBad(false) {}

	bool  Bad() const     { return m_bBad; }
	uint32 Pos() const    { return m_Pos; }

	void SeekTo(uint32 pos)
	{
		if (pos > m_Len)
			m_bBad = true;
		else
			m_Pos = pos;
	}

	const uint8* Raw(uint32 n)
	{
		if (m_bBad || m_Pos + n > m_Len || m_Pos + n < m_Pos)
		{
			m_bBad = true;
			return LTNULL;
		}
		const uint8 *p = m_pData + m_Pos;
		m_Pos += n;
		return p;
	}

	uint8  U8()  { const uint8 *p = Raw(1); return p ? *p : 0; }
	int8   I8()  { return (int8)U8(); }

	uint16 U16() { const uint8 *p = Raw(2); uint16 v = 0; if (p) memcpy(&v, p, 2); return v; }
	uint32 U32() { const uint8 *p = Raw(4); uint32 v = 0; if (p) memcpy(&v, p, 4); return v; }
	float  F32() { const uint8 *p = Raw(4); float  v = 0; if (p) memcpy(&v, p, 4); return v; }

	// ABC strings are uint16 length followed by unterminated characters
	void Str(char *pOut, uint32 maxBytes)
	{
		uint16 n = U16();
		const uint8 *p = Raw(n);
		uint32 copy = (n < maxBytes - 1) ? n : maxBytes - 1;
		if (p && maxBytes)
			memcpy(pOut, p, copy);
		if (maxBytes)
			pOut[p ? copy : 0] = 0;
	}

	void Skip(uint32 n) { Raw(n); }

private:

	const uint8 *m_pData;
	uint32       m_Len;
	uint32       m_Pos;
	bool         m_bBad;
};


// Growable output buffer
class CLTBWriter
{
public:

	CLTBWriter() : m_pData(LTNULL), m_Size(0), m_Cap(0), m_bBad(false) {}
	~CLTBWriter() { delete[] m_pData; }

	bool    Bad() const  { return m_bBad; }
	uint32  Size() const { return m_Size; }

	// Hands ownership of the buffer to the caller
	uint8* Detach(uint32 *pSize)
	{
		uint8 *p = m_pData;
		*pSize = m_Size;
		m_pData = LTNULL;
		m_Size = m_Cap = 0;
		return p;
	}

	void Write(const void *pData, uint32 n)
	{
		if (m_bBad)
			return;
		if (!Reserve(m_Size + n))
			return;
		memcpy(m_pData + m_Size, pData, n);
		m_Size += n;
	}

	void U8(uint8 v)   { Write(&v, 1); }
	void U16(uint16 v) { Write(&v, 2); }
	void U32(uint32 v) { Write(&v, 4); }
	void I32(int32 v)  { Write(&v, 4); }
	void F32(float v)  { Write(&v, 4); }

	// Model::LoadString -> CGenLTStream::ReadString: uint16 length, then characters with no terminator
	void Str(const char *p)
	{
		uint32 n = p ? (uint32)strlen(p) : 0;
		U16((uint16)n);
		if (n)
			Write(p, n);
	}

	// Patch a uint32 already written, for sizes only known afterwards.
	void PatchU32(uint32 offset, uint32 v)
	{
		if (!m_bBad && offset + 4 <= m_Size)
			memcpy(m_pData + offset, &v, 4);
	}

private:

	bool Reserve(uint32 need)
	{
		if (need <= m_Cap)
			return true;

		uint32 cap = m_Cap ? m_Cap : 4096;
		while (cap < need)
		{
			if (cap > 0x40000000)
			{
				m_bBad = true;
				return false;
			}
			cap *= 2;
		}

		uint8 *pNew = LTNULL;
		LT_MEM_TRACK_ALLOC(pNew = new uint8[cap], LT_MEM_TYPE_MODEL);
		if (!pNew)
		{
			m_bBad = true;
			return false;
		}

		if (m_pData)
		{
			memcpy(pNew, m_pData, m_Size);
			delete[] m_pData;
		}
		m_pData = pNew;
		m_Cap = cap;
		return true;
	}

	uint8  *m_pData;
	uint32  m_Size;
	uint32  m_Cap;
	bool    m_bBad;
};


// Parsed ABC contents.
namespace
{

struct ABCVert
{
	float  x, y, z;
	float  nx, ny, nz;
	uint8  iNode;
};

struct ABCTri
{
	float   u[3], v[3];
	uint16  idx[3];
};

struct ABCNode
{
	char    szName[64];
	uint16  iIndex;
	uint8   nFlags;
	uint32  nChildren;
	uint32  nMDVerts;

	// Which vertices this node deforms
	uint16 *pMDVerts;
};

struct ABCKey
{
	uint32  nTime;
	char    szString[128];
};

// One node's keyframe track within one animation.
struct ABCNodeTrack
{
	LTVector *pPos;		// nKeyFrames entries
	float    *pQuat		// nKeyFrames * 4 entries, x,y,z,w as stored

	// Vertex animation for a node that owns an MD vertex list
	float    *pMDPos;
};

struct ABCAnim
{
	char           szName[64];
	uint32         nLength;
	LTVector       vDims;
	uint32         nKeyFrames;
	ABCKey        *pKeys;
	ABCNodeTrack  *pTracks;   // One per node
};

struct ABCModel
{
	ABCModel()
	{
		memset(this, 0, sizeof(*this));
	}

	~ABCModel()
	{
		delete[] pVerts;
		delete[] pTris;
		if (pNodes)
		{
			for (uint32 i = 0; i < nNodes; i++)
				delete[] pNodes[i].pMDVerts;
		}
		delete[] pNodes;
		delete[] pBindGlobals;

		if (pAnims)
		{
			for (uint32 i = 0; i < nAnims; i++)
			{
				delete[] pAnims[i].pKeys;
				if (pAnims[i].pTracks)
				{
					for (uint32 n = 0; n < nNodes; n++)
					{
						delete[] pAnims[i].pTracks[n].pPos;
						delete[] pAnims[i].pTracks[n].pQuat;
						delete[] pAnims[i].pTracks[n].pMDPos;
					}
					delete[] pAnims[i].pTracks;
				}
			}
			delete[] pAnims;
		}
	}

	char      szCommandString[256];

	ABCVert  *pVerts;
	uint32    nVerts; // Vertices in the full-detail mesh
	ABCTri   *pTris;
	uint32    nTris;

	ABCNode  *pNodes;
	uint32    nNodes;

	ABCAnim  *pAnims;
	uint32    nAnims;

	// Bind-pose global transform per node, composed from animation 0's first keyframe
	LTMatrix *pBindGlobals;

	LTVector  vBoundsMin, vBoundsMax;
};

}


// ABC parsing

// Returns the payload offset of a named section, or 0xFFFFFFFF.  
// A section is a name followed by the absolute offset of the next
static uint32 abc_FindSection(const uint8 *pData, uint32 len, const char *pWant)
{
	CABCReader r(pData, len);
	uint32 guard = 0;

	while (!r.Bad() && guard++ < 64)
	{
		char szName[64];
		r.Str(szName, sizeof(szName));
		uint32 nNext = r.U32();

		if (r.Bad())
			break;

		if (stricmp(szName, pWant) == 0)
			return r.Pos();

		if (nNext == 0xFFFFFFFF || nNext >= len)
			break;
		r.SeekTo(nNext);
	}
	return 0xFFFFFFFF;
}


static bool abc_ParseHeader(const uint8 *pData, uint32 len, ABCModel &m)
{
	uint32 off = abc_FindSection(pData, len, "Header");
	if (off == 0xFFFFFFFF)
		return false;

	CABCReader r(pData, len);
	r.SeekTo(off);

	char szToken[128];
	r.Str(szToken, sizeof(szToken));
	if (strcmp(szToken, ABC_FILE_TOKEN) != 0)
		return false;

	r.Str(m.szCommandString, sizeof(m.szCommandString));
	return !r.Bad();
}


static bool abc_ParseGeometry(const uint8 *pData, uint32 len, ABCModel &m)
{
	uint32 off = abc_FindSection(pData, len, "Geometry");
	if (off == 0xFFFFFFFF)
		return false;

	CABCReader r(pData, len);
	r.SeekTo(off);

	m.vBoundsMin.x = r.F32(); m.vBoundsMin.y = r.F32(); m.vBoundsMin.z = r.F32();
	m.vBoundsMax.x = r.F32(); m.vBoundsMax.y = r.F32(); m.vBoundsMax.z = r.F32();

	uint32 nLODVerts = r.U32();
	if (nLODVerts > ABC_MAX_VERTS)
		return false;

	r.Skip(2 * (nLODVerts + 1));

	uint32 nTris = r.U32();
	if (r.Bad() || nTris == 0 || nTris > ABC_MAX_TRIS)
		return false;

	LT_MEM_TRACK_ALLOC(m.pTris = new ABCTri[nTris], LT_MEM_TYPE_MODEL);
	if (!m.pTris)
		return false;
	m.nTris = nTris;

	for (uint32 i = 0; i < nTris; i++)
	{
		ABCTri &t = m.pTris[i];
		for (uint32 c = 0; c < 3; c++)
		{
			t.u[c] = r.F32();
			t.v[c] = r.F32();
		}
		t.idx[0] = r.U16();
		t.idx[1] = r.U16();
		t.idx[2] = r.U16();
		r.Skip(3); // Face normal, 3 signed bytes
	}

	uint32 nAllVerts = r.U32();
	uint32 nNormalVerts = r.U32();

	if (r.Bad() || nAllVerts == 0 || nAllVerts > ABC_MAX_VERTS ||
	    nNormalVerts == 0 || nNormalVerts > nAllVerts)
		return false;

	LT_MEM_TRACK_ALLOC(m.pVerts = new ABCVert[nAllVerts], LT_MEM_TYPE_MODEL);
	if (!m.pVerts)
		return false;

	for (uint32 i = 0; i < nAllVerts; i++)
	{
		ABCVert &v = m.pVerts[i];

		// LT1's model mirror, baked in at load.
		// The z is negated on every vertex, normal, keyframe translation and quaternion
		const float fMirrorZ = g_bLT1ModelMirror ? -1.0f : 1.0f;

		v.x = r.F32();
		v.y = r.F32();
		v.z = fMirrorZ * r.F32();

		// Normals are signed bytes over 127, then renormalised. 
		// The z negation is the same mirror. A sign flip preserves magnitude.
		float nx = (float)r.I8() / 127.0f;
		float ny = (float)r.I8() / 127.0f;
		float nz = fMirrorZ * (float)r.I8() / 127.0f;
		float mag = (float)sqrt(nx * nx + ny * ny + nz * nz);
		if (mag > 0.0001f)
		{
			nx /= mag; ny /= mag; nz /= mag;
		}
		else
		{
			nx = 0.0f; ny = 1.0f; nz = 0.0f;
		}
		v.nx = nx; v.ny = ny; v.nz = nz;

		v.iNode = r.U8();
		r.Skip(4); // Replacements[2]
	}

	// The full-detail mesh is what the triangles index.
	// Keep the whole array so indices stay valid, but report the count the format calls normal
	m.nVerts = nAllVerts;

	return !r.Bad();
}


static bool abc_ParseNodes(const uint8 *pData, uint32 len, ABCModel &m)
{
	uint32 off = abc_FindSection(pData, len, "Nodes");
	if (off == 0xFFFFFFFF)
		return false;

	CABCReader r(pData, len);
	r.SeekTo(off);

	uint32 startPos = r.Pos();
	uint32 nCount = 0;

	{
		// Counting pass
		// A node is bounds(24) + name + index(2) + flags(1) + numMDVerts(4) + MDVertList + numChildren(4)
		uint32 remaining = 1;
		while (remaining > 0 && !r.Bad() && nCount < ABC_MAX_NODES)
		{
			r.Skip(24);
			char szName[64];
			r.Str(szName, sizeof(szName));
			r.U16();
			r.U8();
			uint32 nMD = r.U32();
			if (nMD > ABC_MAX_VERTS)
				return false;
			r.Skip(2 * nMD);
			uint32 nKids = r.U32();
			if (nKids > ABC_MAX_NODES)
				return false;

			nCount++;
			remaining += nKids;
			remaining--;
		}

		if (r.Bad() || remaining != 0 || nCount == 0)
			return false;
	}

	LT_MEM_TRACK_ALLOC(m.pNodes = new ABCNode[nCount], LT_MEM_TYPE_MODEL);
	if (!m.pNodes)
		return false;
	m.nNodes = nCount;
	memset(m.pNodes, 0, sizeof(ABCNode) * nCount);

	r.SeekTo(startPos);
	for (uint32 i = 0; i < nCount; i++)
	{
		ABCNode &n = m.pNodes[i];
		r.Skip(24);
		r.Str(n.szName, sizeof(n.szName));
		n.iIndex = r.U16();
		n.nFlags = r.U8();
		n.nMDVerts = r.U32();
		if (n.nMDVerts)
		{
			LT_MEM_TRACK_ALLOC(n.pMDVerts = new uint16[n.nMDVerts], LT_MEM_TYPE_MODEL);
			if (!n.pMDVerts)
				return false;
			for (uint32 v = 0; v < n.nMDVerts; v++)
				n.pMDVerts[v] = r.U16();
		}
		n.nChildren = r.U32();
	}
	return !r.Bad();
}


static bool abc_ParseAnims(const uint8 *pData, uint32 len, ABCModel &m)
{
	uint32 off = abc_FindSection(pData, len, "Animation");
	if (off == 0xFFFFFFFF)
		return false;

	CABCReader r(pData, len);
	r.SeekTo(off);

	uint32 nAnims = r.U32();
	if (r.Bad() || nAnims == 0 || nAnims > ABC_MAX_ANIMS)
		return false;

	LT_MEM_TRACK_ALLOC(m.pAnims = new ABCAnim[nAnims], LT_MEM_TYPE_MODEL);
	if (!m.pAnims)
		return false;
	memset(m.pAnims, 0, sizeof(ABCAnim) * nAnims);
	m.nAnims = nAnims;

	for (uint32 a = 0; a < nAnims; a++)
	{
		ABCAnim &an = m.pAnims[a];

		r.Str(an.szName, sizeof(an.szName));
		an.nLength = r.U32();

		// The animation's visual bounds, not its dims.
		// The dims live in their own AnimDims section (shown later).
		LTVector vMin, vMax;
		vMin.x = r.F32(); vMin.y = r.F32(); vMin.z = r.F32();
		vMax.x = r.F32(); vMax.y = r.F32(); vMax.z = r.F32();

		an.vDims.x = (float)fabs(vMax.x - vMin.x) * 0.5f;
		an.vDims.y = (float)fabs(vMax.y - vMin.y) * 0.5f;
		an.vDims.z = (float)fabs(vMax.z - vMin.z) * 0.5f;

		uint32 nKF = r.U32();
		if (r.Bad() || nKF == 0 || nKF > ABC_MAX_KEYFRAMES)
			return false;
		an.nKeyFrames = nKF;

		LT_MEM_TRACK_ALLOC(an.pKeys = new ABCKey[nKF], LT_MEM_TYPE_MODEL);
		if (!an.pKeys)
			return false;

		for (uint32 k = 0; k < nKF; k++)
		{
			an.pKeys[k].nTime = r.U32();
			r.Skip(24);					// Per-keyframe bounds
			r.Str(an.pKeys[k].szString, sizeof(an.pKeys[k].szString));
		}

		LT_MEM_TRACK_ALLOC(an.pTracks = new ABCNodeTrack[m.nNodes], LT_MEM_TYPE_MODEL);
		if (!an.pTracks)
			return false;
		memset(an.pTracks, 0, sizeof(ABCNodeTrack) * m.nNodes);

		for (uint32 n = 0; n < m.nNodes; n++)
		{
			ABCNodeTrack &tr = an.pTracks[n];

			LT_MEM_TRACK_ALLOC(tr.pPos = new LTVector[nKF], LT_MEM_TYPE_MODEL);
			LT_MEM_TRACK_ALLOC(tr.pQuat = new float[nKF * 4], LT_MEM_TYPE_MODEL);
			if (!tr.pPos || !tr.pQuat)
				return false;

			for (uint32 k = 0; k < nKF; k++)
			{
				// The z negation here and on the quaternion below is LT1's model mirror, baked in at load
				const float fMirrorZ = g_bLT1ModelMirror ? -1.0f : 1.0f;

				tr.pPos[k].x = r.F32();
				tr.pPos[k].y = r.F32();
				tr.pPos[k].z = fMirrorZ * r.F32();

				// ABC stores the conjugate of the rotation this engine expects, 
				// and LT1's mirror negates x and y again, so only the z negation survives
				const float fXY = g_bLT1ModelMirror ? 1.0f : -1.0f;

				tr.pQuat[k * 4 + 0] = fXY * r.F32();
				tr.pQuat[k * 4 + 1] = fXY * r.F32();
				tr.pQuat[k * 4 + 2] =  -r.F32();
				tr.pQuat[k * 4 + 3] =        r.F32();
			}

			// Vertex-animation frames and the scale and transform pair that decompresses them, 
			// value * Scale + Transform per component in the node's own space.
			// Every frame is kept, since CD3DVAMesh lerps at draw time
			const uint32 nMD = m.pNodes[n].nMDVerts;
			uint8 *pRaw = LTNULL;
			if (nMD > 0)
			{
				if (m.pNodes[n].pMDVerts)
				{
					LT_MEM_TRACK_ALLOC(pRaw = new uint8[nMD * 3 * nKF], LT_MEM_TYPE_MODEL);
					if (!pRaw)
						return false;
					for (uint32 v = 0; v < nMD * 3 * nKF; v++)
						pRaw[v] = r.U8();
				}
				else
				{
					r.Skip(3 * nMD * nKF);
				}
			}

			LTVector vScale, vXlate;
			vScale.x = r.F32(); vScale.y = r.F32(); vScale.z = r.F32();
			vXlate.x = r.F32(); vXlate.y = r.F32(); vXlate.z = r.F32();

			if (pRaw)
			{
				const float fMirrorZ2 = g_bLT1ModelMirror ? -1.0f : 1.0f;

				// Every frame, decompressed into the track, with the same model
				// mirror on z as every other position in this file
				LT_MEM_TRACK_ALLOC(tr.pMDPos = new float[(size_t)nKF * nMD * 3], LT_MEM_TYPE_MODEL);
				if (!tr.pMDPos)
				{
					delete[] pRaw;
					return false;
				}

				for (uint32 k = 0; k < nKF; k++)
				{
					const uint8 *pF = pRaw + (size_t)k * nMD * 3;
					float *pOut = tr.pMDPos + (size_t)k * nMD * 3;
					for (uint32 v = 0; v < nMD; v++)
					{
						pOut[v * 3 + 0] = (float)pF[v * 3 + 0] * vScale.x + vXlate.x;
						pOut[v * 3 + 1] = (float)pF[v * 3 + 1] * vScale.y + vXlate.y;
						pOut[v * 3 + 2] = fMirrorZ2 *
						                  ((float)pF[v * 3 + 2] * vScale.z + vXlate.z);
					}
				}

				// Frame 0 of the first animation still goes into the base vertices as the rest pose
				if (a == 0 && m.pVerts)
				{
					for (uint32 v = 0; v < nMD; v++)
					{
						uint32 iVert = m.pNodes[n].pMDVerts[v];
						if (iVert >= m.nVerts)
							continue;
						ABCVert &vt = m.pVerts[iVert];
						vt.x = tr.pMDPos[v * 3 + 0];
						vt.y = tr.pMDPos[v * 3 + 1];
						vt.z = tr.pMDPos[v * 3 + 2];
					}
				}

				delete[] pRaw;
			}
		}

		if (r.Bad())
			return false;
	}

	return true;
}


static void abc_ParseAnimDims(const uint8 *pData, uint32 len, ABCModel &m)
{
	uint32 off = abc_FindSection(pData, len, "AnimDims");
	if (off == 0xFFFFFFFF)
		return;

	CABCReader r(pData, len);
	r.SeekTo(off);

	for (uint32 a = 0; a < m.nAnims; a++)
	{
		LTVector v;
		v.x = r.F32();
		v.y = r.F32();
		v.z = r.F32();

		// A short or corrupt section leaves every remaining animation on the fallback
		if (r.Bad())
			return;

		m.pAnims[a].vDims = v;
	}
}


// LTB image construction

// Composes a node's global transform from its parent's and its own first keyframe of the first animation, 
// which is where ABC keeps the bind pose.
static void abc_ComposeGlobal(const LTMatrix &mParent, const LTVector &vPos, const float *pQuat, LTMatrix &mOut)
{
	float x = pQuat[0], y = pQuat[1], z = pQuat[2], w = pQuat[3];

	float len = (float)sqrt(x * x + y * y + z * z + w * w);
	if (len > 0.0001f)
	{
		x /= len; y /= len; z /= len; w /= len;
	}
	else
	{
		x = y = z = 0.0f; w = 1.0f;
	}

	LTMatrix mLocal;
	mLocal.Identity();

	mLocal.m[0][0] = 1.0f - 2.0f * (y * y + z * z);
	mLocal.m[0][1] =        2.0f * (x * y - z * w);
	mLocal.m[0][2] =        2.0f * (x * z + y * w);
	mLocal.m[1][0] =        2.0f * (x * y + z * w);
	mLocal.m[1][1] = 1.0f - 2.0f * (x * x + z * z);
	mLocal.m[1][2] =        2.0f * (y * z - x * w);
	mLocal.m[2][0] =        2.0f * (x * z - y * w);
	mLocal.m[2][1] =        2.0f * (y * z + x * w);
	mLocal.m[2][2] = 1.0f - 2.0f * (x * x + y * y);

	mLocal.m[0][3] = vPos.x;
	mLocal.m[1][3] = vPos.y;
	mLocal.m[2][3] = vPos.z;

	mOut = mParent * mLocal;
}


// Composes every node's bind-pose global transform from animation 0's first keyframe (depth first).
// The same walk abc_WriteNode does
static void abc_ComposeBindGlobals_R(ABCModel &m, uint32 &iNode, const LTMatrix &mParent)
{
	if (iNode >= m.nNodes)
		return;

	const ABCNode &n = m.pNodes[iNode];
	uint32 self = iNode;
	iNode++;

	LTMatrix mGlobal;
	if (m.nAnims > 0 && m.pAnims[0].pTracks)
	{
		const ABCNodeTrack &tr = m.pAnims[0].pTracks[self];
		abc_ComposeGlobal(mParent, tr.pPos[0], tr.pQuat, mGlobal);
	}
	else
	{
		mGlobal = mParent;
	}

	m.pBindGlobals[self] = mGlobal;

	for (uint32 c = 0; c < n.nChildren; c++)
		abc_ComposeBindGlobals_R(m, iNode, mGlobal);
}


static bool abc_ComputeBindGlobals(ABCModel &m)
{
	if (m.nNodes == 0)
		return false;

	LT_MEM_TRACK_ALLOC(m.pBindGlobals = new LTMatrix[m.nNodes], LT_MEM_TYPE_MODEL);
	if (!m.pBindGlobals)
		return false;

	LTMatrix mIdent;
	mIdent.Identity();

	uint32 iNode = 0;
	abc_ComposeBindGlobals_R(m, iNode, mIdent);
	return iNode == m.nNodes;
}


// Writes one node and its children matching ModelNode::Load (depth first).
static void abc_WriteNode(CLTBWriter &w, const ABCModel &m, uint32 &iNode)
{
	if (iNode >= m.nNodes)
		return;

	const ABCNode &n = m.pNodes[iNode];
	uint32 self = iNode;
	iNode++;

	const LTMatrix &mGlobal = m.pBindGlobals[self];

	w.Str(n.szName);
	w.U16((uint16)self);

	// Not n.nFlags 
	// ABC's flag byte is 2 on most nodes, and Jupiter's only node flag is MNODE_ROTATIONONLY, 
	// so passing it through freezes the skeleton
	w.U8(0);

	for (uint32 i = 0; i < 4; i++)
		for (uint32 j = 0; j < 4; j++)
			w.F32(mGlobal.m[i][j]);

	w.U32(n.nChildren);

	for (uint32 c = 0; c < n.nChildren; c++)
		abc_WriteNode(w, m, iNode);
}

static void abc_NodeLocalAtFrame(const ABCAnim &an, uint32 iNode, uint32 k,
                                 LTMatrix &mOut)
{
	const ABCNodeTrack &tr = an.pTracks[iNode];
	const float *q = tr.pQuat + k * 4;

	const float s  = 2.0f / (q[0]*q[0] + q[1]*q[1] + q[2]*q[2] + q[3]*q[3]);
	const float xs = q[0]*s, ys = q[1]*s, zs = q[2]*s;
	const float wx = q[3]*xs, wy = q[3]*ys, wz = q[3]*zs;
	const float xx = q[0]*xs, xy = q[0]*ys, xz = q[0]*zs;
	const float yy = q[1]*ys, yz = q[1]*zs;
	const float zz = q[2]*zs;

	mOut.Identity();
	mOut.m[0][0] = 1.0f - (yy + zz);  mOut.m[0][1] = xy - wz;           mOut.m[0][2] = xz + wy;
	mOut.m[1][0] = xy + wz;           mOut.m[1][1] = 1.0f - (xx + zz);  mOut.m[1][2] = yz - wx;
	mOut.m[2][0] = xz - wy;           mOut.m[2][1] = yz + wx;           mOut.m[2][2] = 1.0f - (xx + yy);

	mOut.SetTranslation(tr.pPos[k].x, tr.pPos[k].y, tr.pPos[k].z);
}

// Inverse of a rotation-and-translation matrix.  
// Written out so the test harness links without LTMatrix
static void abc_RigidInverse(const LTMatrix &mIn, LTMatrix &mOut)
{
	mOut.Identity();
	for (uint32 r = 0; r < 3; r++)
		for (uint32 c = 0; c < 3; c++)
			mOut.m[r][c] = mIn.m[c][r];

	const float tx = mIn.m[0][3], ty = mIn.m[1][3], tz = mIn.m[2][3];
	mOut.m[0][3] = -(mOut.m[0][0] * tx + mOut.m[0][1] * ty + mOut.m[0][2] * tz);
	mOut.m[1][3] = -(mOut.m[1][0] * tx + mOut.m[1][1] * ty + mOut.m[1][2] * tz);
	mOut.m[2][3] = -(mOut.m[2][0] * tx + mOut.m[2][1] * ty + mOut.m[2][2] * tz);
}

static void abc_ComposeVABake_R(const ABCModel &m, const ABCAnim &an, uint32 k,
                                const uint8 *pIsVA, uint32 &iNode,
                                const LTMatrix &mBakeParent, bool bParentIdentity,
                                LTMatrix *pOut)
{
	if (iNode >= m.nNodes)
		return;

	const ABCNode &n = m.pNodes[iNode];
	const uint32 self = iNode;
	iNode++;

	LTMatrix mLocal;
	abc_NodeLocalAtFrame(an, self, k, mLocal);

	LTMatrix mBake;
	bool bIdentity;

	if (pIsVA[self])
	{
		mBake = bParentIdentity ? mLocal : (mBakeParent * mLocal);
		bIdentity = false;
		pOut[self] = mBake;
	}
	else if (bParentIdentity)
	{
		mBake.Identity();
		bIdentity = true;
	}
	else
	{
		LTMatrix mInv;
		abc_RigidInverse(mLocal, mInv);
		mBake = mInv * (mBakeParent * mLocal);
		bIdentity = false;
	}

	for (uint32 c = 0; c < n.nChildren; c++)
		abc_ComposeVABake_R(m, an, k, pIsVA, iNode, mBake, bIdentity, pOut);
}

// Fills pOut[node * nKeyFrames + key] for every vertex-animated node.
static bool abc_ComposeVABake(const ABCModel &m, const ABCAnim &an,
                              const uint8 *pIsVA, LTMatrix *pOut)
{
	LTMatrix mIdent;
	mIdent.Identity();

	for (uint32 k = 0; k < an.nKeyFrames; k++)
	{
		LTMatrix frame[ABC_MAX_NODES];
		for (uint32 i = 0; i < m.nNodes; i++)
			frame[i].Identity();

		uint32 iNode = 0;
		abc_ComposeVABake_R(m, an, k, pIsVA, iNode, mIdent, true, frame);
		if (iNode != m.nNodes)
			return false;

		for (uint32 n = 0; n < m.nNodes; n++)
			pOut[(size_t)n * an.nKeyFrames + k] = frame[n];
	}
	return true;
}

static uint32 abc_FindVertexAnimNodes(const ABCModel &m, uint8 *pIsVA)
{
	memset(pIsVA, 0, ABC_MAX_NODES);
	uint32 nFound = 0;

	for (uint32 n = 0; n < m.nNodes; n++)
	{
		if (!m.pNodes[n].nMDVerts || !m.pNodes[n].pMDVerts)
			continue;

		for (uint32 t = 0; t < m.nTris; t++)
		{
			const ABCTri &tri = m.pTris[t];
			if (tri.idx[0] >= m.nVerts || tri.idx[1] >= m.nVerts || tri.idx[2] >= m.nVerts)
				continue;
			const uint8 a = m.pVerts[tri.idx[0]].iNode;
			if (a != n)
				continue;
			if (m.pVerts[tri.idx[1]].iNode != a || m.pVerts[tri.idx[2]].iNode != a)
				continue;

			// Every corner must be in the node's MD list, 
			// or the animation would never write to the vertex the mesh draws.
			bool bAllMapped = true;
			for (uint32 c = 0; c < 3 && bAllMapped; c++)
			{
				bool bFound = false;
				for (uint32 v = 0; v < m.pNodes[n].nMDVerts && !bFound; v++)
					bFound = (m.pNodes[n].pMDVerts[v] == tri.idx[c]);
				bAllMapped = bFound;
			}
			if (bAllMapped)
			{
				pIsVA[n] = 1;
				nFound++;
				break;
			}
		}
	}
	return nFound;
}

// Does this triangle belong to any vertex-animated node?
// Callers pass the set abc_FindVertexAnimNodes filled
static bool abc_TriIsVertexAnim(const ABCModel &m, const ABCTri &tri, const uint8 *pIsVA)
{
	return pIsVA[m.pVerts[tri.idx[0]].iNode] != 0;
}

// Emits the vertex-animated render object as a CD3DVAMesh.
// The first nMD vertices must be the node's MD vertices in pMDVerts order, 
// since UpdateVA writes getValue(i) into vertex i. 
// A vertex whose corners carry different UVs becomes a duplicate with a DupMap entry
static bool abc_WriteVertexAnimObject(CLTBWriter &w, const ABCModel &m,
                                      uint32 iMDNode,
                                      uint32 &nUsedNodesOut, uint8 *pUsedNodes)
{
	const uint32 nMD = m.pNodes[iMDNode].nMDVerts;
	const uint16 *pMD = m.pNodes[iMDNode].pMDVerts;
	if (!nMD || !pMD)
		return false;

	// Model vertex index -> its slot in the undup run, so a triangle corner can find the base vertex it should share or duplicate.
	uint32 slotOf[ABC_MAX_VERTS];
	for (uint32 i = 0; i < m.nVerts && i < ABC_MAX_VERTS; i++)
		slotOf[i] = 0xFFFFFFFFu;
	for (uint32 v = 0; v < nMD; v++)
	{
		if (pMD[v] < m.nVerts)
			slotOf[pMD[v]] = v;
	}

	uint32 nTris = 0;
	for (uint32 t = 0; t < m.nTris; t++)
	{
		if (m.pVerts[m.pTris[t].idx[0]].iNode == iMDNode)
			nTris++;
	}
	if (nTris == 0)
		return false;

	const uint32 nMaxVerts = nMD + nTris * 3;
	if (nMaxVerts > ABC_MAX_VERTS)
		return false;

	float  *pOutVerts = LTNULL;
	uint16 *pOutIdx   = LTNULL;
	uint16 *pDupSrc   = LTNULL;
	uint16 *pDupDst   = LTNULL;
	LT_MEM_TRACK_ALLOC(pOutVerts = new float[(size_t)nMaxVerts * 8], LT_MEM_TYPE_MODEL);
	LT_MEM_TRACK_ALLOC(pOutIdx   = new uint16[(size_t)nTris * 3], LT_MEM_TYPE_MODEL);
	LT_MEM_TRACK_ALLOC(pDupSrc   = new uint16[(size_t)nTris * 3], LT_MEM_TYPE_MODEL);
	LT_MEM_TRACK_ALLOC(pDupDst   = new uint16[(size_t)nTris * 3], LT_MEM_TYPE_MODEL);
	if (!pOutVerts || !pOutIdx || !pDupSrc || !pDupDst)
	{
		delete[] pOutVerts; delete[] pOutIdx;
		delete[] pDupSrc;   delete[] pDupDst;
		return false;
	}

	// The undup run, in pMDVerts order. UVs are filled in as corners claim them. 
	// A slot with no corner yet takes the first one it sees.
	bool bHasUV[ABC_MAX_VERTS];
	for (uint32 v = 0; v < nMD; v++)
	{
		const ABCVert &vt = m.pVerts[pMD[v]];
		float *pv = pOutVerts + (size_t)v * 8;
		pv[0] = vt.x;  pv[1] = vt.y;  pv[2] = vt.z;
		pv[3] = vt.nx; pv[4] = vt.ny; pv[5] = vt.nz;
		pv[6] = 0.0f;  pv[7] = 0.0f;
		bHasUV[v] = false;
	}

	uint32 vOut = nMD, iOut = 0, nDup = 0;

	for (uint32 t = 0; t < m.nTris; t++)
	{
		const ABCTri &tri = m.pTris[t];
		if (m.pVerts[tri.idx[0]].iNode != iMDNode)
			continue;

		for (uint32 c = 0; c < 3; c++)
		{
			const uint32 iVert = tri.idx[c];
			const uint32 slot = (iVert < m.nVerts) ? slotOf[iVert] : 0xFFFFFFFFu;
			if (slot == 0xFFFFFFFFu)
			{
				// A corner on a deformation triangle whose vertex is not in the node's MD list.
				// Refuse, since the animation would never write to that vertex.
				delete[] pOutVerts; delete[] pOutIdx;
				delete[] pDupSrc;   delete[] pDupDst;
				return false;
			}

			float *pBase = pOutVerts + (size_t)slot * 8;
			if (!bHasUV[slot])
			{
				pBase[6] = tri.u[c];
				pBase[7] = tri.v[c];
				bHasUV[slot] = true;
				pOutIdx[iOut++] = (uint16)slot;
			}
			else if (pBase[6] == tri.u[c] && pBase[7] == tri.v[c])
			{
				pOutIdx[iOut++] = (uint16)slot;
			}
			else
			{
				const ABCVert &vt = m.pVerts[iVert];
				float *pv = pOutVerts + (size_t)vOut * 8;
				pv[0] = vt.x;  pv[1] = vt.y;  pv[2] = vt.z;
				pv[3] = vt.nx; pv[4] = vt.ny; pv[5] = vt.nz;
				pv[6] = tri.u[c]; pv[7] = tri.v[c];

				pDupSrc[nDup] = (uint16)slot;
				pDupDst[nDup] = (uint16)vOut;
				nDup++;

				pOutIdx[iOut++] = (uint16)vOut;
				vOut++;
			}
		}
	}

	// One line per vertex-animated piece (capped).
	// A model contributes one per deformation node.
	{
		static uint32 s_nVAPieces = 0;
		if (s_nVAPieces < 120)
		{
			++s_nVAPieces;
			dsi_ConsolePrint("VAMESH: node %u '%s', %u verts (%u undup), %u tris, %u dups",
				iMDNode, m.pNodes[iMDNode].szName, vOut, nMD, nTris, nDup);
		}
	}

	w.U32(ABC_RO_VAMESH);

	uint32 sizeOffset = w.Size();
	w.U32(0);								// iObjSize, patched below
	uint32 payloadStart = w.Size();

	w.U32(vOut);							// m_iVertCount
	w.U32(nMD);								// m_iUnDupVertCount
	w.U32(nTris);							// m_iPolyCount
	w.U32(1);								// m_iMaxBonesPerTri
	w.U32(1);								// m_iMaxBonesPerVert
	w.U32(ABC_STREAM_FLAGS);
	w.U32(0); w.U32(0); w.U32(0);
	w.U32(iMDNode);							// m_iAnimNodeIdx
	w.U32(iMDNode);							// m_iBoneEffector

	w.Write(pOutVerts, (size_t)vOut * ABC_VERTEX_SIZE);
	w.Write(pOutIdx, (size_t)nTris * 3 * sizeof(uint16));

	w.U32(nDup);							// m_iDupMapListCount
	for (uint32 i = 0; i < nDup; i++)
	{
		w.U16(pDupSrc[i]);
		w.U16(pDupDst[i]);
	}

	w.PatchU32(sizeOffset, w.Size() - payloadStart);

	delete[] pOutVerts; delete[] pOutIdx;
	delete[] pDupSrc;   delete[] pDupDst;

	nUsedNodesOut = 1;
	pUsedNodes[0] = (uint8)iMDNode;
	return true;
}

// Emits the D3D render object for the single piece.

// Triangles are grouped by node - one bone set per node - and vertices are re-emitted 
// in bone-set order so each set is contiguous
static bool abc_WriteRenderObject(CLTBWriter &w, const ABCModel &m, const uint8 *pIsVA,
                                  uint32 &nUsedNodesOut, uint8 *pUsedNodes)
{
	// Which nodes actually carry triangles, and how many each has
	uint32 triCount[ABC_MAX_NODES];
	memset(triCount, 0, sizeof(triCount));

	for (uint32 t = 0; t < m.nTris; t++)
	{
		const ABCTri &tri = m.pTris[t];
		if (tri.idx[0] >= m.nVerts || tri.idx[1] >= m.nVerts || tri.idx[2] >= m.nVerts)
			return false;

		// Deformation triangles belong to the vertex-animated piece
		if (abc_TriIsVertexAnim(m, tri, pIsVA))
			continue;

		uint8 a = m.pVerts[tri.idx[0]].iNode;
		uint8 b = m.pVerts[tri.idx[1]].iNode;
		uint8 c = m.pVerts[tri.idx[2]].iNode;

		// Every corner of a triangle has to sit on one node
		if (a != b || b != c)
			return false;
		if (a >= m.nNodes)
			return false;

		triCount[a]++;
	}

	uint32 nBoneSets = 0;
	for (uint32 n = 0; n < m.nNodes; n++)
	{
		if (triCount[n])
			nBoneSets++;
	}
	if (nBoneSets == 0)
		return false;

	bool bRigid = (nBoneSets == 1);

	// Build the re-ordered vertex and index arrays, one bone set at a time.
	uint32 nOutVerts = m.nTris * 3;
	if (nOutVerts > ABC_MAX_VERTS)
		return false;

	float  *pOutVerts = LTNULL;
	uint16 *pOutIdx = LTNULL;
	LT_MEM_TRACK_ALLOC(pOutVerts = new float[nOutVerts * 8], LT_MEM_TYPE_MODEL);
	LT_MEM_TRACK_ALLOC(pOutIdx = new uint16[m.nTris * 3], LT_MEM_TYPE_MODEL);
	if (!pOutVerts || !pOutIdx)
	{
		delete[] pOutVerts;
		delete[] pOutIdx;
		return false;
	}

	struct SetInfo { uint16 iFirstVert, nVerts; uint8 iBone; uint32 iIndexInto; };
	SetInfo sets[ABC_MAX_NODES];
	uint32 nSets = 0;

	uint32 vOut = 0, iOut = 0;

	for (uint32 n = 0; n < m.nNodes; n++)
	{
		if (!triCount[n])
			continue;

		SetInfo &s = sets[nSets++];
		s.iFirstVert = (uint16)vOut;
		s.iBone = (uint8)n;

		for (uint32 t = 0; t < m.nTris; t++)
		{
			const ABCTri &tri = m.pTris[t];
			if (m.pVerts[tri.idx[0]].iNode != n)
				continue;

			for (uint32 c = 0; c < 3; c++)
			{
				const ABCVert &v = m.pVerts[tri.idx[c]];
				float *p = pOutVerts + vOut * 8;

				// ABC stores each vertex in its node's space.
				// Jupiter wants model space, since the renderer applies AnimGlobal * inverse(BindGlobal) per bone
				const LTMatrix &mBind = m.pBindGlobals[n];

				LTVector vLocal(v.x, v.y, v.z);
				LTVector vModel;
				MatVMul(&vModel, &mBind, &vLocal);

				// Normals rotate but do not translate.
				LTVector vLocalNormal(v.nx, v.ny, v.nz);
				LTVector vNormal;
				MatVMul_3x3(&vNormal, &mBind, &vLocalNormal);
				float fLen = (float)sqrt(vNormal.x * vNormal.x +
				                         vNormal.y * vNormal.y +
				                         vNormal.z * vNormal.z);
				if (fLen > 0.0001f)
				{
					vNormal.x /= fLen; vNormal.y /= fLen; vNormal.z /= fLen;
				}
				else
				{
					vNormal.x = 0.0f; vNormal.y = 1.0f; vNormal.z = 0.0f;
				}

				p[0] = vModel.x;  p[1] = vModel.y;  p[2] = vModel.z;
				p[3] = vNormal.x; p[4] = vNormal.y; p[5] = vNormal.z;
				p[6] = tri.u[c];
				p[7] = tri.v[c];

				pOutIdx[iOut++] = (uint16)vOut;
				vOut++;
			}
		}

		s.nVerts = (uint16)(vOut - s.iFirstVert);

		// iIndexIntoIndexBuff is the offset one past this set's triangles
		// Render draws (thisSet.iIndexIntoIndexBuff - previousValue) / 3 of them
		s.iIndexInto = iOut;
	}

	nUsedNodesOut = 0;
	for (uint32 i = 0; i < nSets && nUsedNodesOut < 255; i++)
		pUsedNodes[nUsedNodesOut++] = sets[i].iBone;

	w.U32(bRigid ? ABC_RO_RIGIDMESH : ABC_RO_SKELMESH);

	uint32 sizeOffset = w.Size();
	w.U32(0);								// iObjSize, patched below
	uint32 payloadStart = w.Size();

	w.U32(vOut);							// m_iVertCount
	// The emitted triangle count; deformation triangles belong to the vertex-animated piece
	const uint32 nOutTris = iOut / 3;
	w.U32(nOutTris);						// m_iPolyCount
	w.U32(1);								// iMaxBonesPerTri
	w.U32(1);								// iMaxBonesPerVert

	if (bRigid)
	{
		w.U32(ABC_STREAM_FLAGS);
		w.U32(0); w.U32(0); w.U32(0);
		w.U32(sets[0].iBone);				// m_iBoneEffector
	}
	else
	{
		w.U8(0);							// m_bReIndexedBones (bool)
		w.U32(ABC_STREAM_FLAGS);
		w.U32(0); w.U32(0); w.U32(0);
		w.U8(0);							// bUseMatrixPalettes (bool)
	}

	w.Write(pOutVerts, vOut * ABC_VERTEX_SIZE);
	w.Write(pOutIdx, nOutTris * 3 * sizeof(uint16));

	if (!bRigid)
	{
		w.U32(nSets);
		for (uint32 i = 0; i < nSets; i++)
		{
			w.U16(sets[i].iFirstVert);
			w.U16(sets[i].nVerts);
			w.U8(sets[i].iBone);
			w.U8(0xFF); w.U8(0xFF); w.U8(0xFF);
			w.U32(sets[i].iIndexInto);
		}
	}

	w.PatchU32(sizeOffset, w.Size() - payloadStart);

	delete[] pOutVerts;
	delete[] pOutIdx;

	return !w.Bad();
}


static bool abc_WriteLTB(const ABCModel &m, CLTBWriter &w)
{
	// LTB_Header
	// Written as the engine reads it (a raw sizeof Read)
	LTB_Header hdr;
	memset(&hdr, 0, sizeof(hdr));
	hdr.m_iFileType = LTB_D3D_MODEL_FILE;
	hdr.m_iVersion = CD3D_LTB_LOAD_VERSION;
	w.Write(&hdr, sizeof(hdr));

	w.U32(MODEL_FILE_VERSION);

	// Which nodes are vertex-animated, 
	// resolved once and shared so the header and the writer agree
	uint8 isVANode[ABC_MAX_NODES];
	const uint32 nVAPieces = abc_FindVertexAnimNodes(m, isVANode);

	// ModelAllocations
	uint32 nTotalKeyFrames = 0;
	uint32 nAnimData = 0;
	for (uint32 a = 0; a < m.nAnims; a++)
	{
		nTotalKeyFrames += m.pAnims[a].nKeyFrames;
		nAnimData += m.nNodes * m.pAnims[a].nKeyFrames *
		             (uint32)(sizeof(LTVector) + sizeof(LTRotation));
	}

	uint32 nVertAnimData = 0;
	for (uint32 a = 0; a < m.nAnims; a++)
	{
		for (uint32 n = 0; n < m.nNodes; n++)
		{
			if (isVANode[n] &&
			    m.pNodes[n].nMDVerts && m.pAnims[a].pTracks[n].pMDPos)
				nVertAnimData += m.pAnims[a].nKeyFrames * m.pNodes[n].nMDVerts * 3 * (uint32)sizeof(float);
		}
	}

	uint32 nStrings = 2 + m.nNodes + m.nAnims + nTotalKeyFrames + 4;
	uint32 nStringLen = 64 * nStrings;

	w.U32(nTotalKeyFrames);			// m_nKeyFrames
	w.U32(m.nAnims);				// m_nParentAnims
	w.U32(m.nNodes);				// m_nNodes
	{
		// Must agree with the piece emission below, 
		// including the entirely-vertex-animated case.
		bool bSkel = false;
		for (uint32 t = 0; t < m.nTris && !bSkel; t++)
		{
			if (m.pTris[t].idx[0] < m.nVerts &&
			    !abc_TriIsVertexAnim(m, m.pTris[t], isVANode))
				bSkel = true;
		}
		w.U32((bSkel ? 1u : 0u) + nVAPieces);		// m_nPieces
	}
	w.U32(1); 					// m_nChildModels
	w.U32(m.nTris); 			// m_nTris
	w.U32(m.nVerts);			// m_nVerts
	w.U32(m.nVerts);			// m_nVertexWeights
	w.U32(1);					// m_nLODs
	w.U32(0);					// m_nSockets
	w.U32(0);					// m_nWeightSets
	w.U32(nStrings); 			// m_nStrings
	w.U32(nStringLen);			// m_StringLengths
	w.U32(nVertAnimData);		// m_VertAnimDataSize
	w.U32(nAnimData);			// m_nAnimData

	w.Str(m.szCommandString);

	// Visible radius: the furthest corner of the model's own bounds.
	float r = 0.0f;
	const float ext[6] =
	{
		m.vBoundsMin.x, m.vBoundsMin.y, m.vBoundsMin.z,
		m.vBoundsMax.x, m.vBoundsMax.y, m.vBoundsMax.z
	};
	for (uint32 i = 0; i < 3; i++)
	{
		float lo = (float)fabs(ext[i]), hi = (float)fabs(ext[i + 3]);
		float mx = (lo > hi) ? lo : hi;
		r += mx * mx;
	}
	w.F32((float)sqrt(r));

	w.U32(0);									// Num OBBs

	// Pieces:
	// One piece for the skeletal geometry and one per deformation node.
	// A piece carries one render object, and CD3DVAMesh::Load reads a single anim node index

	// A model can be entirely vertex-animated.
	// An empty skeletal piece gives LT_INVALIDFILE, 
	// so the skeletal piece is emitted only when it has geometry.
	bool bHasSkeletal = false;
	for (uint32 t = 0; t < m.nTris && !bHasSkeletal; t++)
	{
		const ABCTri &tri = m.pTris[t];
		if (tri.idx[0] < m.nVerts && !abc_TriIsVertexAnim(m, tri, isVANode))
			bHasSkeletal = true;
	}

	w.U32((bHasSkeletal ? 1u : 0u) + nVAPieces);

	if (bHasSkeletal)
	{
	w.Str("Piece");
	w.U32(1);									// LOD count
	w.F32(0.0f);								// LOD distance
	w.U32(0);									// unused min LOD offset
	w.U32(0);									// unused max LOD offset

	w.U32(1);									// m_nNumTextures
	w.I32(0); w.I32(0); w.I32(0); w.I32(0);		// m_iTextures[MAX_PIECE_TEXTURES]
	w.I32(0);									// m_iRenderStyle
	w.U8(0);									// m_nRenderPriority (uint8)

	uint32 nUsedNodes = 0;
	uint8 usedNodes[256];
	if (!abc_WriteRenderObject(w, m, isVANode, nUsedNodes, usedNodes))
		return false;

	w.U8((uint8)nUsedNodes);
	for (uint32 i = 0; i < nUsedNodes; i++)
		w.U8(usedNodes[i]);
	}

	for (uint32 iMDNode = 0; iMDNode < m.nNodes; iMDNode++)
	{
		if (!isVANode[iMDNode])
			continue;

		w.Str("VertexAnim");
		w.U32(1); 						// LOD count
		w.F32(0.0f);					// LOD distance
		w.U32(0);
		w.U32(0);

		w.U32(1);						// m_nNumTextures
		w.I32(0); w.I32(0); w.I32(0); w.I32(0);
		w.I32(0);						// m_iRenderStyle
		w.U8(0);						// m_nRenderPriority

		uint32 nVANodes = 0;
		uint8 vaNodes[256];
		if (!abc_WriteVertexAnimObject(w, m, iMDNode, nVANodes, vaNodes))
			return false;

		w.U8((uint8)nVANodes);
		for (uint32 i = 0; i < nVANodes; i++)
			w.U8(vaNodes[i]);
	}

	// Nodes
	uint32 iNode = 0;
	abc_WriteNode(w, m, iNode);
	if (iNode != m.nNodes)
		return false;

	// Weight sets
	w.U32(0);

	// Child models
	w.U32(1); // Just SELF

	// animations

	w.U32(m.nAnims);
	for (uint32 a = 0; a < m.nAnims; a++)
	{
		const ABCAnim &an = m.pAnims[a];

		w.F32(an.vDims.x); w.F32(an.vDims.y); w.F32(an.vDims.z);

		w.Str(an.szName);
		w.U32(0); // Compression type: none

		// m_InterpolationMS is the cross-fade time when switching to this animation, not its length
		w.U32(0);
		w.U32(an.nKeyFrames);

		for (uint32 k = 0; k < an.nKeyFrames; k++)
		{
			w.U32(an.pKeys[k].nTime);
			w.Str(an.pKeys[k].szString);
		}

		// The transform to bake into each vertex-animated node's positions, computed once per keyframe
		LTMatrix *pBake = NULL;
		if (nVAPieces > 0 && an.nKeyFrames > 0)
		{
			LT_MEM_TRACK_ALLOC(pBake = new LTMatrix[(size_t)m.nNodes * an.nKeyFrames],
			                   LT_MEM_TYPE_MODEL);
			if (!pBake || !abc_ComposeVABake(m, an, isVANode, pBake))
			{
				delete[] pBake;
				return false;
			}
		}

		// Node tracks, depth first, which for ABC is storage order.
		for (uint32 n = 0; n < m.nNodes; n++)
		{
			const ABCNodeTrack &tr = an.pTracks[n];
			const uint32 nMD = m.pNodes[n].nMDVerts;

			// Only the nodes a vertex-animated piece was built from get a
			// vertex channel. The rest keep their pos/quat track
			if (isVANode[n] && nMD > 0 && tr.pMDPos && pBake)
			{
				// A vertex-animated node
				w.U8(1);

				for (uint32 k = 0; k < an.nKeyFrames; k++)
				{
					const LTMatrix &mBake = pBake[(size_t)n * an.nKeyFrames + k];

					w.U32(nMD);
					for (uint32 v = 0; v < nMD; v++)
					{
						const float *pv = tr.pMDPos + ((size_t)k * nMD + v) * 3;
						LTVector vIn(pv[0], pv[1], pv[2]), vOutP;
						mBake.Apply(vIn, vOutP);
						w.F32(vOutP.x); w.F32(vOutP.y); w.F32(vOutP.z);
					}
				}
			}
			else
			{
				w.U8(0); // Not a vertex animation

				for (uint32 k = 0; k < an.nKeyFrames; k++)
				{
					w.F32(tr.pPos[k].x);
					w.F32(tr.pPos[k].y);
					w.F32(tr.pPos[k].z);
				}
				for (uint32 k = 0; k < an.nKeyFrames; k++)
				{
					w.F32(tr.pQuat[k * 4 + 0]);
					w.F32(tr.pQuat[k * 4 + 1]);
					w.F32(tr.pQuat[k * 4 + 2]);
					w.F32(tr.pQuat[k * 4 + 3]);
				}
			}
		}

		delete[] pBake;
	}

	// Sockets
	w.U32(0);

	// Animation bindings, one block per child model (Just SELF)
	w.U32(m.nAnims);
	for (uint32 a = 0; a < m.nAnims; a++)
	{
		const ABCAnim &an = m.pAnims[a];
		w.Str(an.szName);
		w.F32(an.vDims.x); w.F32(an.vDims.y); w.F32(an.vDims.z);
		w.F32(0.0f); w.F32(0.0f); w.F32(0.0f); // Translation
	}

	return !w.Bad();
}



// Entry points

bool abc_IsABCFilename(const char *pFilename)
{
	if (!pFilename)
		return false;

	const char *pExt = strrchr(pFilename, '.');
	return pExt && stricmp(pExt, ".abc") == 0;
}


LTRESULT abc_BuildLTBImage(ILTStream *pStream, uint8 **ppImage, uint32 *pImageSize)
{
	*ppImage = LTNULL;
	*pImageSize = 0;

	if (!pStream)
		return LT_INVALIDPARAMS;

	uint32 len = 0;
	if (pStream->GetLen(&len) != LT_OK || len == 0)
		return LT_INVALIDDATA;

	uint8 *pFile = LTNULL;
	LT_MEM_TRACK_ALLOC(pFile = new uint8[len], LT_MEM_TYPE_MODEL);
	if (!pFile)
		return LT_OUTOFMEMORY;

	if (pStream->SeekTo(0) != LT_OK || pStream->Read(pFile, len) != LT_OK)
	{
		delete[] pFile;
		return LT_INVALIDDATA;
	}

	LTRESULT dResult = LT_INVALIDDATA;

	{
		ABCModel model;

		if (abc_ParseHeader(pFile, len, model) &&
		    abc_ParseGeometry(pFile, len, model) &&
		    abc_ParseNodes(pFile, len, model) &&
		    abc_ParseAnims(pFile, len, model) &&
		    abc_ComputeBindGlobals(model))
		{
			// Optional, and only meaningful once the animations exist
			abc_ParseAnimDims(pFile, len, model);

			CLTBWriter w;
			if (abc_WriteLTB(model, w))
			{
				*ppImage = w.Detach(pImageSize);
				dResult = LT_OK;
			}
		}
	}

	delete[] pFile;
	return dResult;
}
