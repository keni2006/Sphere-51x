//
// CArray.cpp
// Copyright Menace Software (www.menasoft.com).
//

#ifdef GRAY_MAKER
#include "../graymaker/stdafx.h"
#include "../graymaker/graymaker.h"
#else
#include "graycom.h"
#endif

//***************************************************************************
// -CGObList

void CGObList::DeleteAll( void )
{
	// delete all entries.
	while (true)	// iterate the list.
	{
		CGObListRec * pRec = GetHead();
		if ( pRec == NULL ) 
			break;
		ASSERT( pRec->GetParent() == this );
		delete pRec;
	}
	m_iCount = 0;
	m_pHead = NULL;
	m_pTail = NULL;
}

void CGObList::OnRemoveOb( CGObListRec* pObRec )	// Override this = called when removed from list.
{
	// just remove from list. DON'T delete !
	if ( pObRec == NULL ) 
		return;
	ASSERT( pObRec->GetParent() == this );

	CGObListRec * pNext = pObRec->GetNext();
	CGObListRec * pPrev = pObRec->GetPrev();

	if ( pNext != NULL )
		pNext->m_pPrev = pPrev;
	else
		m_pTail = pPrev;
	if ( pPrev != NULL )
		pPrev->m_pNext = pNext;
	else
		m_pHead = pNext;

	pObRec->m_pNext = NULL;	// this should not really be necessary.
	pObRec->m_pPrev = NULL;
	pObRec->m_pParent = NULL;	// We are now unlinked.
	m_iCount --;
}

void CGObList::InsertAfter( CGObListRec * pNewRec, CGObListRec * pPrev )
{
	// Add after pPrev.
	// pPrev = NULL == add to the start.
	ASSERT( pNewRec != NULL );
	pNewRec->RemoveSelf();	// Get out of previous list first.
	ASSERT( pPrev != pNewRec );
	ASSERT( pNewRec->GetParent() == NULL );

	pNewRec->m_pParent = this;

	CGObListRec * pNext;
	if ( pPrev != NULL )		// Its the first.
	{
		ASSERT( pPrev->GetParent() == this );
		pNext = pPrev->GetNext();
		pPrev->m_pNext = pNewRec;
	}
	else
	{
		pNext = GetHead();
		m_pHead = pNewRec;
	}

	pNewRec->m_pPrev = pPrev;

	if ( pNext != NULL )
	{
		ASSERT( pNext->GetParent() == this );
		pNext->m_pPrev = pNewRec;
	}
	else
	{
		m_pTail = pNewRec;
	}

	pNewRec->m_pNext = pNext;
	m_iCount ++;
}

CGObListRec * CGObList::GetAt( int index ) const
{
	CGObListRec * pRec = GetHead();
	while ( index-- > 0 && pRec != NULL )
	{
		pRec = pRec->GetNext();
	}
	return( pRec );
}
