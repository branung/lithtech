#ifndef __MODEL_ABC_H__
#define __MODEL_ABC_H__

// Runtime support for LithTech 1.0 models (ABC version 6)

// Model::Load requires a .ltb extension and an LTB_D3D_MODEL_FILE header,
// so the original .abc files are rejected before the geometry is even read.
// This reads ABC directly, so the engine can be pointed at an original install.

#ifndef __GENLTSTREAM_H__
#include "genltstream.h"
#endif

// A read-only stream over the converted image, which it also owns.
// Model::Load keeps one on the stack, so every exit path frees the buffer.
class CModelABCStream : public CGenLTStream
{
public:

	CModelABCStream() : m_pData(LTNULL), m_Len(0), m_Pos(0), m_Status(LT_OK) {}
	~CModelABCStream() { delete[] m_pData; }

	// Takes ownership of pData.
	void Init(uint8 *pData, uint32 len)
	{
		delete[] m_pData;
		m_pData = pData;
		m_Len = len;
		m_Pos = 0;
		m_Status = LT_OK;
	}

	bool IsInited() const { return m_pData != LTNULL; }

	virtual void Release() {}

	virtual LTRESULT Read(void *pOut, uint32 size)
	{
		if (!m_pData || m_Pos + size > m_Len)
		{
			memset(pOut, 0, size);
			m_Status = LT_ERROR;
			return LT_ERROR;
		}
		memcpy(pOut, m_pData + m_Pos, size);
		m_Pos += size;
		return LT_OK;
	}

	virtual LTRESULT Write(const void *, uint32)  { return LT_ERROR; }
	virtual LTRESULT ErrorStatus()                { return m_Status; }
	virtual LTRESULT GetPos(uint32 *pPos)         { *pPos = m_Pos; return LT_OK; }
	virtual LTRESULT GetLen(uint32 *pLen)         { *pLen = m_Len; return LT_OK; }

	virtual LTRESULT SeekTo(uint32 offset)
	{
		if (offset > m_Len)
		{
			m_Status = LT_ERROR;
			return LT_ERROR;
		}
		m_Pos = offset;
		return LT_OK;
	}

private:

	uint8    *m_pData;
	uint32    m_Len;
	uint32    m_Pos;
	LTRESULT  m_Status;
};

// True if the filename ends in .abc (case-insensitive).
bool abc_IsABCFilename(const char *pFilename);

// Reads an ABC v6 model and produces the equivalent LTB image
LTRESULT abc_BuildLTBImage(ILTStream *pStream, uint8 **ppImage, uint32 *pImageSize);

#endif  // __MODEL_ABC_H__
