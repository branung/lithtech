// LT1's model warble: the underwater ripple that scales every model vertex by a factor between WarbleScale and 1
#ifndef __LT1MODELWARBLE_H__
#define __LT1MODELWARBLE_H__

#ifndef __VERTEXBUFFERCONTROLLER_H__
#include "VertexBufferController.h"
#endif

class ILTStream;
struct BoneSetListItem;

// Advances the clock once per scene, whether the warble is on or not, as LT1's did
void lt1warble_Tick(float fFrameTime);

// ModelWarble is set, the only test LT1 makes: every model warbles, the player view weapon included
bool lt1warble_On();

// The scale for a vertex whose phase is nPhase, its LT1 vertex index from the LOD's first vertex, mod 32
float lt1warble_K(uint32 nPhase);

// What a converted LT1 mesh needs to warble: each vertex's LT1 phase and the point it scales about.
// Only the ABC converter writes it, so a mesh from any other source has none and is never touched
class CLT1WarbleData
{
public:
	CLT1WarbleData();
	~CLT1WarbleData()			{ Free(); }

	void		Free();
	void		FreeDeviceObjects()		{ m_VB.FreeAll(); m_bVB = false; }

	// Reads the block if the render object has one before iObjEnd, or leaves the stream where it was
	bool		LoadOptional(ILTStream& File, uint32 iObjEnd);

	bool		Has() const				{ return m_pPhase != NULL; }
	uint32		GetPhaseCount() const	{ return m_nPhase; }

	// Skeletal and rigid meshes: one origin per bone set, the node's bind position, where LT1's node space
	// scale is centered. Maps each vertex to its set; a rigid mesh passes no sets and uses origin 0
	bool		BindSets(const BoneSetListItem* pSets, uint32 nSets, uint32 nVerts);

	// The warbled copy of stream 0, rebuilt at most once per scene since every input is the same for every
	// instance and pass. NULL when the copy can't be made, and the caller draws unwarbled
	VertexBufferController*	GetCopy(const uint8* pSrcVerts, uint32 nVerts, uint32 iStreamFlags,
								VERTEX_BLEND_TYPE eVertType, bool bSWVertProc,
								const uint8* pIndexData, uint32 nIndices);

	// Vertex animated meshes: scales the first nVerts positions in place about the node's per animation
	// translate, blended as the frame is
	bool		GetVAOrigin(uint32 iPrevAnim, uint32 iCurAnim, float fPercent, LTVector& vOut) const;
	void		ApplyVA(uint8* pVerts, uint32 iVertSize, uint32 nVerts, const LTVector& vOrigin) const;

private:
	uint8*					m_pPhase;		// per vertex
	uint32					m_nPhase;
	LTVector*				m_pOrigin;		// per bone set, or per animation for a VA mesh
	uint32					m_nOrigin;
	uint8*					m_pSetOf;		// per vertex: which origin
	VertexBufferController	m_VB;
	bool					m_bVB;
	uint32					m_iScene;		// the scene m_VB was last written for
};

#endif
