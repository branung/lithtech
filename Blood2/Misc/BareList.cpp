// CBareList's five out-of-line members.

#include "BareList.h"


void CBareList::InsertFirst(CBareListItem* pItem)
{
	pItem->m_pNext = m_pFirst;
	pItem->m_pPrev = 0;

	if (m_pFirst)
		m_pFirst->m_pPrev = pItem;
	else
		m_pLast = pItem;

	m_pFirst = pItem;
	m_nCount++;
}


void CBareList::InsertLast(CBareListItem* pItem)
{
	pItem->m_pNext = 0;
	pItem->m_pPrev = m_pLast;

	if (m_pLast)
		m_pLast->m_pNext = pItem;
	else
		m_pFirst = pItem;

	m_pLast = pItem;
	m_nCount++;
}


// A NULL pBeforeItem inserts at the start, as the header says.
// The 1998 library had no return after that and went on to dereference the NULL, which crashed.
// Blood II never calls this, so that path never ran
void CBareList::InsertAfter(CBareListItem* pBeforeItem, CBareListItem* pNewItem)
{
	if (!pBeforeItem)
	{
		InsertFirst(pNewItem);
		return;
	}

	if (pBeforeItem->m_pNext)
		pBeforeItem->m_pNext->m_pPrev = pNewItem;
	else
		m_pLast = pNewItem;

	pNewItem->m_pPrev = pBeforeItem;
	pNewItem->m_pNext = pBeforeItem->m_pNext;
	pBeforeItem->m_pNext = pNewItem;
	m_nCount++;
}


// A NULL pAfterItem inserts at the end, with the same missing return fixed.
// CTeamMgr's sort calls this, but always with an item
void CBareList::InsertBefore(CBareListItem* pAfterItem, CBareListItem* pNewItem)
{
	if (!pAfterItem)
	{
		InsertLast(pNewItem);
		return;
	}

	if (pAfterItem->m_pPrev)
		pAfterItem->m_pPrev->m_pNext = pNewItem;
	else
		m_pFirst = pNewItem;

	pNewItem->m_pNext = pAfterItem;
	pNewItem->m_pPrev = pAfterItem->m_pPrev;
	pAfterItem->m_pPrev = pNewItem;
	m_nCount++;
}


// Unlinks only. As in the original, it doesn't free the item or clear the item's own links,
// so callers that read GetNext() before deleting keep working
void CBareList::Delete(CBareListItem* pItem)
{
	if (pItem->m_pPrev)
		pItem->m_pPrev->m_pNext = pItem->m_pNext;
	else
		m_pFirst = pItem->m_pNext;

	if (pItem->m_pNext)
		pItem->m_pNext->m_pPrev = pItem->m_pPrev;
	else
		m_pLast = pItem->m_pPrev;

	m_nCount--;
}
