//
// CWorld.CPP
// Copyright Menace Software (www.menasoft.com).
//

#include "graysvr.h"	// predef header.
#include "MySqlStorageService.h"

bool World_fDeleteCycle = false;

static const SOUND_TYPE Sounds_Ghost[] =
{
	SOUND_GHOST_1,
	SOUND_GHOST_2,
	SOUND_GHOST_3,
	SOUND_GHOST_4,
	SOUND_GHOST_5,
};

//////////////////////////////////////////////////////////////////
// -CGMPage

CGMPage::CGMPage( const TCHAR * pszAccount ) :
	m_sAccount( pszAccount )
{
	m_pGMClient = NULL;
	m_lTime = g_World.GetTime();
	// Put at the end of the list.
	g_World.m_GMPages.InsertTail( this );
}

CGMPage::~CGMPage()
{
	if ( m_pGMClient )	// break the link to the client.
	{
		ASSERT( m_pGMClient->m_pGMPage == this );
		m_pGMClient->m_pGMPage = NULL;
		ClearGMHandler();
	}
}

void CGMPage::r_Write( CScript & s ) const
{
	s.WriteSection( "GMPAGE %s", GetName());
	s.WriteKey( "REASON", GetReason());
	s.WriteKeyHex( "TIME", m_lTime );
	s.WriteKey( "P", m_p.Write());
}

bool CGMPage::r_LoadVal( CScript & s )
{
	if ( s.IsKey( "REASON" ))
	{
		SetReason( s.GetArgStr());
	}
	else if ( s.IsKey( "TIME" ))
	{
		m_lTime = s.GetArgHex();
	}
	else if ( s.IsKey( "P" ))
	{
		m_p.Read( s.GetArgStr());
	}
	else
	{
		return( false );
	}
	return( true );
}

CAccount * CGMPage::FindAccount() const
{
	return( g_World.AccountFind( m_sAccount ));
}


//////////////////////////////////////////////////////////////////
// -CWorldSearch

CWorldSearch::CWorldSearch( const CPointMap p, int iDist ) :
	m_p( p ),
	m_iDist( iDist )
{
	// define a search of the world.
	m_fInertCharUse = false;
	m_pObj = m_pObjNext = NULL;
	m_fInertToggle = false;

	m_pSectorBase = m_pSector = p.GetSector();

	int i = p.m_x - iDist;
	i = max( i, 0 );
	m_rectSector.m_left = i &~ (SECTOR_SIZE_X-1);
	i = ( p.m_x + iDist ) ;
	m_rectSector.m_right = min( i, UO_SIZE_X ) | (SECTOR_SIZE_X-1);
	i = p.m_y - iDist;
	i = max( i, 0 );
	m_rectSector.m_top = i &~ (SECTOR_SIZE_Y-1);
	i = ( p.m_y + iDist ) ;
	m_rectSector.m_bottom = min( i, UO_SIZE_Y ) | (SECTOR_SIZE_Y-1);
	m_rectSector.NormalizeRect();

	// Get upper left of search rect.
	m_pntSector.m_x = m_rectSector.m_left;
	m_pntSector.m_y = m_rectSector.m_top;
	m_pntSector.m_z = 0;

	//if ( m_rectSector.m_left == 0x13e3 && m_rectSector.m_right == 0x147f &&
		//m_rectSector.m_top == 0x13 && m_rectSector.m_bottom == 0x7f )
	//{
	//	DEBUG_ERR(( "Test this\n" ));
	//}
}

bool CWorldSearch::GetNextSector()
{
	// Move search into near by CSector(s) if necessary

	if ( ! m_iDist )
		return( false );

	for ( ; m_pntSector.m_y < m_rectSector.m_bottom; m_pntSector.m_y += SECTOR_SIZE_Y )
	{
		while ( m_pntSector.m_x < m_rectSector.m_right )
		{
			m_pSector = m_pntSector.GetSector();
			m_pntSector.m_x += SECTOR_SIZE_X;
			if ( m_pSectorBase == m_pSector )
				continue;	// same as base.

			m_pObj = NULL;	// start at head of next Sector.
			return( true );
		}
		m_pntSector.m_x = m_rectSector.m_left;
	}
	return( false );	// done searching.
}

CItem * CWorldSearch::GetItem()
{
	while (true)
	{
		if ( m_pObj == NULL )
		{
			m_fInertToggle = false;
			m_pObj = STATIC_CAST <CObjBase*> ( m_pSector->m_Items_Inert.GetHead());
		}
		else
		{
			m_pObj = m_pObjNext;
		}
		if ( m_pObj == NULL )
		{
			if ( ! m_fInertToggle )
			{
				m_fInertToggle = true;
				m_pObj = STATIC_CAST <CObjBase*> ( m_pSector->m_Items_Timer.GetHead());
				if ( m_pObj != NULL )
					goto jumpover;
			}
			if ( GetNextSector())
				continue;
			return( NULL );
		}

jumpover:
		m_pObjNext = m_pObj->GetNext();
		if ( m_p.GetDist( m_pObj->GetTopPoint()) <= m_iDist )
			return( STATIC_CAST <CItem *> ( m_pObj ));
	}
}

CChar * CWorldSearch::GetChar()
{
	while (true)
	{
		if ( m_pObj == NULL )
		{
			m_fInertToggle = false;
			m_pObj = STATIC_CAST <CObjBase*> ( m_pSector->m_Chars.GetHead());
		}
		else
		{
			m_pObj = m_pObjNext;
		}
		if ( m_pObj == NULL )
		{
			if ( ! m_fInertToggle && m_fInertCharUse )
			{
				m_fInertToggle = true;
				m_pObj = STATIC_CAST <CObjBase*> ( m_pSector->m_Chars_Disconnect.GetHead());
				if ( m_pObj != NULL )
					goto jumpover;
			}
			if ( GetNextSector())
				continue;
			return( NULL );
		}

jumpover:
		m_pObjNext = m_pObj->GetNext();
		if ( m_p.GetDist( m_pObj->GetTopPoint()) <= m_iDist )
			return( STATIC_CAST <CChar *> ( m_pObj ));
	}
}

//////////////////////////////////////////////////////////////////
// -CWorld

CWorld::CWorld()
{
        m_iSaveCount = 0;
        m_iSaveStage = 0;
        m_fSaveForce = false;
        m_Clock_Sector = 0;
        m_Clock_Respawn = 0;

        m_Clock_PrevSys = 0;
        m_Clock_Time = 0;
        m_Clock_Startup = 0;
        m_fSavingStorage = false;
        m_fSaveFailed = false;
        m_fStorageSavePrepared = false;
        m_fStorageLoadPrepared = false;
        m_fStorageLoadFailed = false;
        m_iStorageLoadStage = 0;
        m_uStorageLoadObjectIndex = 0;
        m_uStorageLoadSectorIndex = 0;
        m_uStorageLoadGMPageIndex = 0;
        m_uStorageLoadServerIndex = 0;
}

CWorld::~CWorld()
{
	Close();
}

MySqlStorageService * CWorld::Storage()
{
	return m_pStorage.get();
}

const MySqlStorageService * CWorld::Storage() const
{
	return m_pStorage.get();
}

UINT CWorld::AllocUID(UINT dwIndex, CObjBase * pObj )
{
	if ( dwIndex == 0 )
	{
		// Find an unused UID slot. Start at some random point.
		UINT dwCount = GetUIDCount() - 1;
		UINT dwCountStart = dwCount;
		dwIndex = GetRandVal( dwCount ) + 1;
		while ( m_UIDs[dwIndex] != NULL )
		{
			if ( ! -- dwIndex )
			{
				dwIndex = dwCountStart;
			}
			if ( ! -- dwCount )
			{
				dwIndex = GetUIDCount();
				goto setcount;
			}
		}
	}
	else if ( dwIndex >= GetUIDCount())
	{
setcount:
		// We have run out of free UID's !!! Grow the array
		m_UIDs.SetCount(( dwIndex + 0x1000 ) &~ 0xFFF );
	}

	CObjBase * pObjPrv = m_UIDs[ dwIndex ];
	if ( pObjPrv != NULL )
	{
		// NOTE: We cannot use Delete() in here because the UID will still be assigned til the async cleanup time.
		DEBUG_ERR(( "UID conflict delete 0%lx, '%s'\n", dwIndex, pObjPrv->GetName()));
		NotifyStorageObjectRemoved( pObjPrv );
		delete pObjPrv;	// Delete()	will not work here !
		// ASSERT( 0 );
	}

	m_UIDs[ dwIndex ] = pObj;
	return( dwIndex );
}

///////////////////////////////////////////////
// Loading and Saving.

void CWorld::GetBackupName( CGString & sArchive, TCHAR chType ) const
{
	int iCount = m_iSaveCount;
	int iGroup = 0;
	for ( ; iGroup<g_Serv.m_iSaveBackupLevels; iGroup++ )
	{
		if ( iCount & 0x7 )
			break;
		iCount >>= 3;
	}
	sArchive.Format( "%s" GRAY_FILE "b%d%d%c%s",
		( chType == 'a' ) ? ((const TCHAR*) g_Serv.m_sAcctBaseDir ) : ((const TCHAR*) g_Serv.m_sWorldBaseDir ),
		iGroup, iCount&0x07,
		chType,
		( g_Serv.m_fSaveBinary && chType == 'w' ) ? GRAY_BINARY : GRAY_SCRIPT );
}

bool CWorld::SaveStage() // Save world state in stages.
{
        MySqlStorageService * pStorage = Storage();
        if ( pStorage && pStorage->IsEnabled())
        {
                return SaveStageStorage();
        }

        // Do the next stage of the save.
        // RETURN: true = continue;
        //  false = done.

        ASSERT( IsSaving());

	switch ( m_iSaveStage )
	{
	case -1:
		if ( ! g_Serv.m_fSaveGarbageCollect )
		{
			GarbageCollection_New();
			GarbageCollection_GMPages();
		}
		break;
	default:
		ASSERT( m_iSaveStage < SECTOR_QTY );
		// NPC Chars in the world secors and the stuff they are carrying.
		// Sector lighting info.
		m_Sectors[m_iSaveStage].r_Write( m_File );
		break;

	case SECTOR_QTY:
		{
			// GM_Pages.
			CGMPage * pPage = dynamic_cast <CGMPage*>( m_GMPages.GetHead());
			for ( ; pPage!= NULL; pPage = pPage->GetNext())
			{
				pPage->r_Write( m_File );
			}
		}
		break;

	case SECTOR_QTY+1:
		// Save all my servers some place.
		if ( g_Serv.m_fMainLogServer )
		{
			for ( int i=0; i<g_Serv.m_Servers.GetCount(); i++ )
			{
				CServRef * pServ = g_Serv.m_Servers[i];
				if ( pServ == NULL )
					continue;
				pServ->WriteCreated( m_File );
			}
		}
		break;

	case SECTOR_QTY+2:
		// Now make a backup of the account file.
		SaveAccounts();
		break;

	case SECTOR_QTY+3:
		{
			// EOF marker to show we reached the end.
			m_File.WriteSection( "EOF" );

			m_iSaveCount++;	// Save only counts if we get to the end winout trapping.
			m_Clock_Save = GetTime() + g_Serv.m_iSavePeriod;	// next save time.

			g_Log.Event( LOGM_SAVE, "World data saved (%s).\n", m_File.GetFilePath());

			// Now clean up all the held over UIDs
			for ( int i=1; i<GetUIDCount(); i++ )
			{
				if ( m_UIDs[i] == UID_PLACE_HOLDER )
					m_UIDs[i] = NULL;
			}

			m_File.Close();
		}
		return( false );	// done.
	}

	if ( g_Serv.m_iSaveBackgroundTime )
	{
		int iNextTime = g_Serv.m_iSaveBackgroundTime / SECTOR_QTY;
		if ( iNextTime > TICK_PER_SEC/2 ) iNextTime = TICK_PER_SEC/2;	// max out at 30 minutes or so.
		m_Clock_Save = GetTime() + iNextTime;
	}
	m_iSaveStage ++;
        return( true );
}

bool CWorld::BeginStorageSave()
{
        if ( m_fStorageSavePrepared )
                return true;

        MySqlStorageService * pStorage = Storage();
        if ( pStorage == NULL || ! pStorage->IsEnabled())
        {
                return false;
        }

        m_pSaveTransaction.reset( new MySqlStorageService::Transaction( *pStorage ));
        if ( ! m_pSaveTransaction || ! m_pSaveTransaction->IsActive())
        {
                g_Log.Event( LOGM_SAVE|LOGL_ERROR, "Failed to begin MySQL transaction for world save.\n" );
                if ( m_pSaveTransaction )
                {
                        m_pSaveTransaction->Rollback();
                }
                m_pSaveTransaction.reset();
                return false;
        }

        if ( ! pStorage->SetWorldSaveCompleted( false ))
        {
                g_Log.Event( LOGM_SAVE|LOGL_ERROR, "Failed to mark MySQL world save as in-progress.\n" );
                return false;
        }
        if ( ! pStorage->SetWorldSaveCount( m_iSaveCount ))
        {
                g_Log.Event( LOGM_SAVE|LOGL_ERROR, "Failed to update MySQL world save count baseline.\n" );
                return false;
        }
        if ( ! pStorage->ClearGMPages())
        {
                g_Log.Event( LOGM_SAVE|LOGL_ERROR, "Failed to clear MySQL GM pages before save.\n" );
                return false;
        }
        if ( ! pStorage->ClearServers())
        {
                g_Log.Event( LOGM_SAVE|LOGL_ERROR, "Failed to clear MySQL server list before save.\n" );
                return false;
        }

        m_fStorageSavePrepared = true;
        g_Log.Event( LOGM_SAVE, "MySQL world save transaction started.\n" );
        return true;
}

void CWorld::AbortStorageSave()
{
        MySqlStorageService * pStorage = Storage();
        bool fHadActiveTransaction = ( m_pSaveTransaction && m_pSaveTransaction->IsActive());
        if ( m_pSaveTransaction )
        {
                m_pSaveTransaction->Rollback();
                m_pSaveTransaction.reset();
        }
        if ( pStorage && pStorage->IsEnabled())
        {
                pStorage->SetWorldSaveCompleted( false );
        }
        if ( fHadActiveTransaction )
        {
                g_Log.Event( LOGM_SAVE|LOGL_WARN, "MySQL world save rolled back.\n" );
        }
        m_fSavingStorage = false;
        m_fStorageSavePrepared = false;
}

void CWorld::NotifyStorageObjectRemoved( CObjBase * pObj )
{
	if ( pObj == NULL )
		return;

	MySqlStorageService * pStorage = Storage();
	if ( pStorage == NULL || ! pStorage->IsEnabled())
		return;

	if ( pObj->IsStorageDeleted())
		return;

	if ( ! pStorage->DeleteObject( pObj ))
	{
		g_Log.Event( LOGM_SAVE|LOGL_WARN, "Failed to mark object 0%lx as deleted in MySQL.\n", (unsigned long) pObj->GetUID());
		return;
	}

	pObj->MarkDirty( StorageDirtyType_Delete );
}

void CWorld::FlushDeletedObjects()
{
	CObjBase * pDelete = STATIC_CAST <CObjBase*>( m_ObjDelete.GetHead());
	while ( pDelete != NULL )
	{
		CObjBase * pNext = STATIC_CAST <CObjBase*>( pDelete->GetNext());
		NotifyStorageObjectRemoved( pDelete );
		delete pDelete;
		pDelete = pNext;
	}
}

bool CWorld::SaveObjectToStorage( CObjBase * pObj )
{
	if ( pObj == NULL )
		return true;

	if ( ! g_Serv.m_fSaveGarbageCollect )
        {
                if ( FixObj( pObj ))
                {
                        return true;
                }
        }

        MySqlStorageService * pStorage = Storage();
        if ( pStorage == NULL || ! pStorage->IsEnabled())
        {
                        return false;
        }

        auto saveObject = [pStorage]( CObjBase * pTarget ) -> bool
        {
                if ( pTarget->IsChar())
                {
                        CChar * pChar = dynamic_cast<CChar*>( pTarget );
                        return ( pChar != NULL ) && pStorage->SaveChar( *pChar );
                }
                if ( pTarget->IsItem())
                {
                        CItem * pItem = dynamic_cast<CItem*>( pTarget );
                        return ( pItem != NULL ) && pStorage->SaveItem( *pItem );
                }
                return false;
        };

        if ( g_Serv.m_fSecure )
        {
                try
                {
                        if ( ! saveObject( pObj ))
                        {
                                g_Log.Event( LOGM_SAVE|LOGL_ERROR, "Failed to persist object 0%lx.\n", (unsigned long) pObj->GetUID());
                                return false;
                        }
                }
                catch (...)     // catch all
                {
                        g_Log.Event( LOGL_CRIT|LOGM_SAVE, "Save Object Exception!\n" );
                        return false;
                }
        }
        else
        {
                if ( ! saveObject( pObj ))
                {
                        g_Log.Event( LOGM_SAVE|LOGL_ERROR, "Failed to persist object 0%lx.\n", (unsigned long) pObj->GetUID());
                        return false;
                }
        }
        return true;
}

bool CWorld::SaveStorageSector( CSector & sector )
{
        if ( ! sector.MarkSaved())
                return true;

        MySqlStorageService * pStorage = Storage();
        if ( pStorage == NULL || ! pStorage->IsEnabled())
        {
                return false;
        }

        if ( ! pStorage->SaveSector( sector ))
        {
                CPointMap base = sector.GetBase();
                g_Log.Event( LOGM_SAVE|LOGL_ERROR, "Failed to persist sector %d,%d.\n", base.m_x, base.m_y );
                return false;
        }

        CChar * pCharNext;
        CChar * pChar = STATIC_CAST <CChar*>( sector.m_Chars.GetHead());
        for ( ; pChar != NULL; pChar = pCharNext )
        {
                pCharNext = pChar->GetNext();
                if ( ! SaveObjectToStorage( pChar ))
                        return false;
        }

        pChar = STATIC_CAST <CChar*> (sector.m_Chars_Disconnect.GetHead());
        for ( ; pChar != NULL; pChar = pCharNext )
        {
                pCharNext = pChar->GetNext();
                if ( ! SaveObjectToStorage( pChar ))
                        return false;
        }

        CItem * pItemNext;
        CItem * pItem = STATIC_CAST <CItem*> (sector.m_Items_Inert.GetHead());
        for ( ; pItem != NULL; pItem = pItemNext )
        {
                pItemNext = pItem->GetNext();
                if ( ! SaveObjectToStorage( pItem ))
                        return false;
        }

        pItem = STATIC_CAST <CItem*> (sector.m_Items_Timer.GetHead());
        for ( ; pItem != NULL; pItem = pItemNext )
        {
                pItemNext = pItem->GetNext();
                if ( ! SaveObjectToStorage( pItem ))
                        return false;
        }

        return true;
}

bool CWorld::SaveStorageGMPages()
{
        MySqlStorageService * pStorage = Storage();
        if ( pStorage == NULL || ! pStorage->IsEnabled())
        {
                return false;
        }

        CGMPage * pPage = dynamic_cast<CGMPage*>( m_GMPages.GetHead());
        for ( ; pPage != NULL; pPage = pPage->GetNext())
        {
                if ( ! pStorage->SaveGMPage( *pPage ))
                {
                        g_Log.Event( LOGM_SAVE|LOGL_ERROR, "Failed to persist GM page for account '%s'.\n", pPage->GetName());
                        return false;
                }
        }
        return true;
}

bool CWorld::SaveStorageServers()
{
        MySqlStorageService * pStorage = Storage();
        if ( pStorage == NULL || ! pStorage->IsEnabled())
        {
                return false;
        }

        if ( ! g_Serv.m_fMainLogServer )
        {
                return true;
        }

        for ( int i = 0; i < g_Serv.m_Servers.GetCount(); ++i )
        {
                CServRef * pServ = g_Serv.m_Servers[i];
                if ( pServ == NULL )
                        continue;
                if ( ! pStorage->SaveServer( *pServ ))
                {
                        g_Log.Event( LOGM_SAVE|LOGL_ERROR, "Failed to persist linked server '%s'.\n", pServ->GetName());
                        return false;
                }
        }
        return true;
}

bool CWorld::FinalizeStorageSave()
{
        MySqlStorageService * pStorage = Storage();
        if ( pStorage == NULL || ! pStorage->IsEnabled())
        {
                return false;
        }

        if ( ! pStorage->SetWorldSaveCount( m_iSaveCount + 1 ))
        {
                g_Log.Event( LOGM_SAVE|LOGL_ERROR, "Failed to update MySQL world save count.\n" );
                return false;
        }
        if ( ! pStorage->SetWorldSaveCompleted( true ))
        {
                g_Log.Event( LOGM_SAVE|LOGL_ERROR, "Failed to mark MySQL world save as completed.\n" );
                return false;
        }

        if ( m_pSaveTransaction )
        {
                if ( ! m_pSaveTransaction->Commit())
                {
                        g_Log.Event( LOGM_SAVE|LOGL_ERROR, "Failed to commit MySQL world save transaction.\n" );
                        m_pSaveTransaction->Rollback();
                        m_pSaveTransaction.reset();
                        return false;
                }
                m_pSaveTransaction.reset();
        }

        m_iSaveCount++;
        m_Clock_Save = GetTime() + g_Serv.m_iSavePeriod;

        for ( int i = 1; i < GetUIDCount(); ++i )
        {
                if ( m_UIDs[i] == UID_PLACE_HOLDER )
                        m_UIDs[i] = NULL;
        }

        g_Log.Event( LOGM_SAVE, "World data saved (MySQL).\n" );

        m_fSavingStorage = false;
        m_fStorageSavePrepared = false;
        return true;
}

void CWorld::ResetStorageLoadState()
{
        m_StorageLoadObjects.clear();
        m_StorageLoadSectors.clear();
        m_StorageLoadGMPages.clear();
        m_StorageLoadServers.clear();
        m_uStorageLoadObjectIndex = 0;
        m_uStorageLoadSectorIndex = 0;
        m_uStorageLoadGMPageIndex = 0;
        m_uStorageLoadServerIndex = 0;
        m_iStorageLoadStage = 0;
        m_fStorageLoadPrepared = false;
        m_fStorageLoadFailed = false;
}

bool CWorld::InitializeStorageLoad()
{
        if ( m_fStorageLoadPrepared )
                return true;

        MySqlStorageService * pStorage = Storage();
        if ( pStorage == NULL || ! pStorage->IsEnabled())
        {
                return false;
        }

        ResetStorageLoadState();

        int iSaveCount = 0;
        bool fCompleted = true;
        if ( ! pStorage->LoadWorldMetadata( iSaveCount, fCompleted ))
        {
                g_Log.Event( LOGM_INIT|LOGL_ERROR, "Failed to load world metadata from MySQL.\n" );
                m_fStorageLoadFailed = true;
                return false;
        }
        m_iSaveCount = iSaveCount;
        if ( ! fCompleted )
        {
                g_Log.Event( LOGM_INIT|LOGL_WARN, "Previous MySQL world save did not complete cleanly.\n" );
        }

        if ( ! pStorage->LoadSectors( m_StorageLoadSectors ))
        {
                g_Log.Event( LOGM_INIT|LOGL_ERROR, "Failed to load sector data from MySQL.\n" );
                m_fStorageLoadFailed = true;
                return false;
        }
        if ( ! pStorage->LoadWorldObjects( m_StorageLoadObjects ))
        {
                g_Log.Event( LOGM_INIT|LOGL_ERROR, "Failed to load world objects from MySQL.\n" );
                m_fStorageLoadFailed = true;
                return false;
        }
        if ( ! pStorage->LoadGMPages( m_StorageLoadGMPages ))
        {
                g_Log.Event( LOGM_INIT|LOGL_ERROR, "Failed to load GM pages from MySQL.\n" );
                m_fStorageLoadFailed = true;
                return false;
        }
        if ( ! pStorage->LoadServers( m_StorageLoadServers ))
        {
                g_Log.Event( LOGM_INIT|LOGL_ERROR, "Failed to load server list from MySQL.\n" );
                m_fStorageLoadFailed = true;
                return false;
        }

        m_fStorageLoadPrepared = true;
        m_fStorageLoadFailed = false;
        return true;
}

bool CWorld::SaveStageStorage()
{
        if ( ! m_fSavingStorage )
        {
                return false;
        }

        if ( ! BeginStorageSave())
        {
                m_fSaveFailed = true;
                AbortStorageSave();
                return false;
        }

        switch ( m_iSaveStage )
        {
        case -1:
                if ( ! g_Serv.m_fSaveGarbageCollect )
                {
                        GarbageCollection_New();
                        GarbageCollection_GMPages();
                }
                break;
        default:
                if ( m_iSaveStage < SECTOR_QTY )
                {
                        if ( ! SaveStorageSector( m_Sectors[m_iSaveStage] ))
                        {
                                m_fSaveFailed = true;
                                AbortStorageSave();
                                return false;
                        }
                }
                else if ( m_iSaveStage == SECTOR_QTY )
                {
                        if ( ! SaveStorageGMPages())
                        {
                                m_fSaveFailed = true;
                                AbortStorageSave();
                                return false;
                        }
                }
                else if ( m_iSaveStage == SECTOR_QTY + 1 )
                {
                        if ( ! SaveStorageServers())
                        {
                                m_fSaveFailed = true;
                                AbortStorageSave();
                                return false;
                        }
                }
                else if ( m_iSaveStage == SECTOR_QTY + 2 )
                {
                        if ( ! SaveAccounts())
                        {
                                m_fSaveFailed = true;
                                AbortStorageSave();
                                return false;
                        }
                }
                else if ( m_iSaveStage == SECTOR_QTY + 3 )
                {
                        if ( ! FinalizeStorageSave())
                        {
                                m_fSaveFailed = true;
                                AbortStorageSave();
                                return false;
                        }
                        return false;
                }
                break;
        }

        if ( g_Serv.m_iSaveBackgroundTime )
        {
                int iNextTime = g_Serv.m_iSaveBackgroundTime / SECTOR_QTY;
                if ( iNextTime > TICK_PER_SEC/2 ) iNextTime = TICK_PER_SEC/2;
                m_Clock_Save = GetTime() + iNextTime;
        }

        m_iSaveStage++;
        return true;
}

bool CWorld::LoadSectionFromStorage()
{
        MySqlStorageService * pStorage = Storage();
        if ( pStorage == NULL || ! pStorage->IsEnabled())
        {
                return false;
        }

        if ( m_fStorageLoadFailed )
        {
                return false;
        }

        if ( ! InitializeStorageLoad())
        {
                        return false;
        }

        while ( true )
        {
                switch ( m_iStorageLoadStage )
                {
                case 0:
                        if ( m_uStorageLoadSectorIndex < m_StorageLoadSectors.size())
                        {
                                const MySqlStorageService::SectorData & data = m_StorageLoadSectors[m_uStorageLoadSectorIndex++];
                                CPointMap base( data.m_iX1, data.m_iY1, 0 );
                                CSector * pSector = base.GetSector();
                                if ( pSector != NULL )
                                {
                                        if ( data.m_fHasLightOverride )
                                                pSector->SetLight( data.m_iLocalLight );
                                        else
                                                pSector->SetLight( -1 );

                                        if ( data.m_fHasRainOverride )
                                                pSector->SetWeatherChance( true, data.m_iRainChance );
                                        else
                                                pSector->SetWeatherChance( true, -1 );

                                        if ( data.m_fHasColdOverride )
                                                pSector->SetWeatherChance( false, data.m_iColdChance );
                                        else
                                                pSector->SetWeatherChance( false, -1 );
                                }
                                else
                                {
                                        g_Log.Event( LOGM_INIT|LOGL_WARN, "Unable to resolve sector (%d,%d) map %d while loading from MySQL.\n", data.m_iX1, data.m_iY1, data.m_iMapPlane );
                                }
                                return true;
                        }
                        m_iStorageLoadStage = 1;
                        continue;

                case 1:
                        if ( m_uStorageLoadObjectIndex < m_StorageLoadObjects.size())
                        {
                                const MySqlStorageService::WorldObjectRecord & record = m_StorageLoadObjects[m_uStorageLoadObjectIndex++];
                                UINT uid = (UINT) record.m_uid;
                                CObjBase * pObj = NULL;

                                if ( record.m_fIsChar )
                                {
                                        CChar * pChar = CChar::CreateBasic( (CREID_TYPE) record.m_iBaseId );
                                        if ( pChar == NULL )
                                        {
                                                g_Log.Event( LOGM_INIT|LOGL_ERROR, "Failed to instantiate character 0x%x for UID 0%llx.\n", record.m_iBaseId, record.m_uid );
                                                m_fStorageLoadFailed = true;
                                                return false;
                                        }
                                        pChar->SetPrivateUID( uid, false );
                                        pObj = pChar;
                                }
                                else
                                {
                                        CItem * pItem = CItem::CreateScript( (ITEMID_TYPE) record.m_iBaseId );
                                        if ( pItem == NULL )
                                        {
                                                g_Log.Event( LOGM_INIT|LOGL_ERROR, "Failed to instantiate item 0x%x for UID 0%llx.\n", record.m_iBaseId, record.m_uid );
                                                m_fStorageLoadFailed = true;
                                                return false;
                                        }
                                        pItem->SetPrivateUID( uid, true );
                                        pObj = pItem;
                                }

                                if ( ! pStorage->ApplyWorldObjectData( *pObj, record.m_sSerialized ))
                                {
                                        g_Log.Event( LOGM_INIT|LOGL_ERROR, "Failed to deserialize world object 0%llx from MySQL.\n", record.m_uid );
                                        FreeUID( uid & UID_MASK );
                                        delete pObj;
                                        m_fStorageLoadFailed = true;
                                        return false;
                                }
                                return true;
                        }
                        m_iStorageLoadStage = 2;
                        continue;

                case 2:
                        if ( m_uStorageLoadGMPageIndex < m_StorageLoadGMPages.size())
                        {
                                const MySqlStorageService::GMPageRecord & record = m_StorageLoadGMPages[m_uStorageLoadGMPageIndex++];
                                CGMPage * pPage = new CGMPage( record.m_sAccount );
                                pPage->SetReason( record.m_sReason );
                                pPage->m_lTime = record.m_lTime;
                                pPage->m_p.m_x = record.m_iPosX;
                                pPage->m_p.m_y = record.m_iPosY;
pPage->m_p.m_z = record.m_iPosZ;
                                return true;
                        }
                        m_iStorageLoadStage = 3;
                        continue;

                case 3:
                        if ( m_uStorageLoadServerIndex < m_StorageLoadServers.size())
                        {
                                const MySqlStorageService::ServerRecord & record = m_StorageLoadServers[m_uStorageLoadServerIndex++];
                                if ( ! record.m_sName.IsEmpty())
                                {
                                        CServRef * pServ = new CServRef( record.m_sName, SOCKET_LOCAL_ADDRESS );
                                        if ( ! record.m_sAddress.IsEmpty())
                                        {
                                                pServ->m_ip.SetHostStr( record.m_sAddress );
                                        }
                                        if ( record.m_iPort > 0 )
                                        {
                                                pServ->m_ip.SetPort( record.m_iPort );
                                        }
                                        pServ->m_TimeZone = (signed char) record.m_iTimeZone;
                                        pServ->SetStat( SERV_STAT_CLIENTS, record.m_iClientsAvg );
                                        pServ->m_sURL = record.m_sURL;
                                        pServ->m_sEMail = record.m_sEmail;
                                        pServ->m_sRegisterPassword = record.m_sRegisterPassword;
                                        pServ->m_sNotes = record.m_sNotes;
                                        pServ->m_sLang = record.m_sLanguage;
                                        pServ->m_sVersion = record.m_sVersion;
                                        pServ->m_eAccApp = (ACCAPP_TYPE) record.m_iAccApp;
                                        g_Serv.m_Servers.AddSortKey( pServ, pServ->GetName());
                                }
                                return true;
                        }
                        m_iStorageLoadStage = 4;
                        continue;

                default:
                        return false;
                }
        }
}

bool CWorld::LoadFromStorage()
{
        MySqlStorageService * pStorage = Storage();
        if ( pStorage == NULL || ! pStorage->IsEnabled())
        {
                return false;
        }

        if ( ! InitializeStorageLoad())
        {
                return false;
        }

        g_Serv.SysMessage( "World Is Loading...\n" );

        m_GMPages.DeleteAll();
        g_Serv.m_Servers.RemoveAll();

        size_t uTotal = m_StorageLoadSectors.size() + m_StorageLoadObjects.size() + m_StorageLoadGMPages.size() + m_StorageLoadServers.size();
        size_t uLastReported = 0;

        while ( true )
        {
                bool fContinue = false;
                if ( g_Serv.m_fSecure )
                {
                        try
                        {
                                fContinue = LoadSectionFromStorage();
                        }
                        catch (...)
                        {
                                g_Log.Event( LOGM_INIT|LOGL_CRIT, "Load Exception during MySQL world restore!\n" );
                                m_fStorageLoadFailed = true;
                                fContinue = false;
                        }
                }
                else
                {
                        fContinue = LoadSectionFromStorage();
                }

                if ( m_fStorageLoadFailed )
                {
                        break;
                }
                if ( ! fContinue )
                {
                        break;
                }

                if ( uTotal > 0 )
                {
                        size_t uProcessed = m_uStorageLoadSectorIndex + m_uStorageLoadObjectIndex + m_uStorageLoadGMPageIndex + m_uStorageLoadServerIndex;
                        if (( uProcessed != uLastReported ) && !( uProcessed & 0xFF ))
                        {
                                g_Serv.PrintPercent( static_cast<int>( uProcessed ), static_cast<int>( uTotal ));
                                uLastReported = uProcessed;
                        }
                }
        }

	bool fSuccess = ! m_fStorageLoadFailed;
	if ( fSuccess )
	{
		g_Log.Event( LOGM_INIT, "World data loaded (MySQL).\n" );
	}

	ResetStorageLoadState();
	return fSuccess;
}

void CWorld::SaveForce() // Save world state
{
        MySqlStorageService * pStorage = Storage();
        const bool fStorageEnabled = ( pStorage != NULL && pStorage->IsEnabled());

        if ( fStorageEnabled )
        {
                if ( ! m_fSavingStorage )
                {
                        if ( g_Serv.m_fSaveGarbageCollect )
                        {
                                GarbageCollection();
                        }

                        if ( ! BeginStorageSave())
                        {
                                g_Log.Event( LOGM_SAVE|LOGL_ERROR, "Failed to initialize MySQL world save.\n" );
                                AbortStorageSave();
                                return;
                        }

                        m_fSavingStorage = true;
                        m_fSaveParity = ! m_fSaveParity;
                        m_iSaveStage = -1;
                        m_fSaveFailed = false;
                        m_Clock_Save = 0;
                }

                m_fSaveForce = true;
                Broadcast( "World save has been initiated." );

                while ( SaveStage())
                {
                        if (! ( m_iSaveStage & 0xFF ))
                        {
                                g_Serv.PrintPercent( m_iSaveStage, SECTOR_QTY+3 );
#ifdef _WIN32
                                if ( g_Service.IsServiceStopPending())
                                {
                                        g_Service.ReportStatusToSCMgr(SERVICE_STOP_PENDING, NO_ERROR, 5000);
                                }
#endif
                        }
                }

                if ( m_fSaveFailed )
                {
                        if ( m_fSavingStorage )
                        {
                                AbortStorageSave();
                        }
                        g_Log.Event( LOGM_SAVE|LOGL_ERROR, "World save failed. Rolling back MySQL transaction.\n" );
                        Broadcast( "Save FAILED. Sphere is UNSTABLE!" );
                }
                else
                {
                        DEBUG_MSG(( "Save Done\n" ));
                }
                return;
        }

        m_fSaveForce = true;
        Broadcast( "World save has been initiated." );

        while ( SaveStage())
        {
                if (! ( m_iSaveStage & 0xFF ))
                {
                        g_Serv.PrintPercent( m_iSaveStage, SECTOR_QTY+3 );
#ifdef _WIN32
                        // Linux doesn't need to know about this
                        if ( g_Service.IsServiceStopPending())
                        {
                                g_Service.ReportStatusToSCMgr(SERVICE_STOP_PENDING, NO_ERROR, 5000);
                        }
#endif
                }
        }
        DEBUG_MSG(( "Save Done\n" ));
}

void CWorld::SaveTry( bool fForceImmediate ) // Save world state
{
        MySqlStorageService * pStorage = Storage();
        const bool fStorageEnabled = ( pStorage != NULL && pStorage->IsEnabled());

        if ( fStorageEnabled )
        {
                if ( m_fSavingStorage )
                {
                        if ( fForceImmediate )
                        {
                                SaveForce();
                        }
                        else if ( g_Serv.m_iSaveBackgroundTime )
                        {
                                SaveStage();
                        }
                        return;
                }

                if ( g_Serv.m_fSaveGarbageCollect )
                {
                        GarbageCollection();
                }

                if ( ! BeginStorageSave())
                {
                        g_Log.Event( LOGM_SAVE|LOGL_ERROR, "Failed to initialize MySQL world save.\n" );
                        AbortStorageSave();
                        return;
                }

                m_fSavingStorage = true;
                m_fSaveParity = ! m_fSaveParity;
                m_iSaveStage = -1;
                m_fSaveForce = false;
                m_fSaveFailed = false;
                m_Clock_Save = 0;

                if ( fForceImmediate || ! g_Serv.m_iSaveBackgroundTime )
                {
                        SaveForce();
                }
                return;
        }

        if ( m_File.IsFileOpen())
        {
                // Save is already active !
                ASSERT( IsSaving());
		if ( fForceImmediate )	// finish it now !
		{
			SaveForce();
		}
		else if ( g_Serv.m_iSaveBackgroundTime )
		{
			SaveStage();
		}
		return;
	}

	// Do the write async from here in the future.
	if ( g_Serv.m_fSaveGarbageCollect )
	{
		GarbageCollection();
	}

	// Determine the save name based on the time.
	// exponentially degrade the saves over time.
	CGString sArchive;
	GetBackupName( sArchive, 'w' );

	// remove previous archive of same name
	remove( sArchive );

	// rename previous save to archive name.
	CGString sSaveName;
	sSaveName.Format( "%s" GRAY_FILE "world%s",
		(const TCHAR*) g_Serv.m_sWorldBaseDir,
		g_Serv.m_fSaveBinary ? GRAY_BINARY : GRAY_SCRIPT );

	if ( rename( sSaveName, sArchive ))
	{
		// May not exist if this is the first time.
		g_Log.Event( LOGM_SAVE|LOGL_ERROR, "World Save Rename to '%s' FAILED\n", (const TCHAR*) sArchive );
	}

	if ( ! m_File.Open( sSaveName, OF_WRITE|((g_Serv.m_fSaveBinary)?OF_BINARY:OF_TEXT)))
	{
		g_Log.Event( LOGM_SAVE|LOGL_CRIT, "Save '%s' failed\n", sSaveName );
		return;
	}

	m_fSaveParity = ! m_fSaveParity; // Flip the parity of the save.
	m_iSaveStage = -1;
	m_fSaveForce = false;
	m_Clock_Save = 0;

	m_File.WriteKey( "TITLE", GRAY_TITLE " World Script" );
	m_File.WriteKey( "VERSION", GRAY_VERSION );
	m_File.WriteKeyVal( "TIME", GetTime());
	m_File.WriteKeyVal( "SAVECOUNT", m_iSaveCount );
	m_File.Flush();	// Force this out to the file now.

	if ( fForceImmediate || ! g_Serv.m_iSaveBackgroundTime )	// Save now !
	{
		SaveForce();
	}
}

void CWorld::Save( bool fForceImmediate ) // Save world state
{
	MySqlStorageService * pStorage = Storage();
	const bool fStorageEnabled = ( pStorage != NULL && pStorage->IsEnabled());

	if ( g_Serv.m_fSecure ) // enable the try code.
	{
		try
		{
			SaveTry(fForceImmediate);
		}
		catch (...) // catch all
		{
			g_Log.Event( LOGL_CRIT|LOGM_SAVE, "Save FAILED. " GRAY_TITLE " is UNSTABLE!\n" );
			Broadcast( "Save FAILED. " GRAY_TITLE " is UNSTABLE!" );
			if ( fStorageEnabled )
			{
				AbortStorageSave();
			}
			else
			{
				m_File.Close(); // close if not already closed.
			}
		}
	}
	else
	{
		SaveTry(fForceImmediate); // Save world state
	}
}

/////////////////////////////////////////////////////////////////////

bool CWorld::LoadSection()
{
        // Load another section of the *WORLD.SCP file.

        MySqlStorageService * pStorage = Storage();
        if ( pStorage && pStorage->IsEnabled())
        {
                return LoadSectionFromStorage();
        }

        if ( ! m_File.FindNextSection())
        {
		if ( m_File.IsSectionType( "EOF" ))
		{
			// The only valid way to end.
			m_File.Close();
			return( true );
		}

		g_Log.Event( LOGM_INIT|LOGL_CRIT, "No [EOF] marker. '%s' is corrupt!\n", m_File.GetFilePath());

		// Auto change to the most recent previous backup !
		// Try to load a backup file instead ?
		Close();
		LoadRegions();	// reload these as they are killed by the Close()

		CGString sArchive;
		GetBackupName( sArchive, 'w' );

		if ( ! sArchive.CompareNoCase( m_File.GetFilePath()))	// ! same file ?
		{
			return( false );
		}
		if ( ! m_File.Open( sArchive ))
		{
			g_Log.Event( LOGL_FATAL|LOGM_INIT, "No previous backup '%s'!\n", m_File.GetFilePath());
			return( false );
		}

		g_Log.Event( LOGM_INIT, "Loading previous backup '%s'\n", m_File.GetFilePath());

		// Find the size of the file.
		m_lLoadSize = m_File.GetLength();
		m_iSaveStage = 0;	// start over at the top of the count.

		// Read the header stuff first.
		return( CScriptObj::r_Load( m_File ));
	}

	if (! ( ++m_iSaveStage & 0xFF ))	// don't update too often
	{
		int iPercent = g_Serv.PrintPercent( m_File.GetPosition(), m_lLoadSize );
	}

	if ( m_File.IsSectionType( "SECTOR" ))
	{
		CPointMap p;
		p.Read( m_File.GetArgStr());
		return( p.GetSector()->r_Load(m_File));
	}
	else if ( m_File.IsSectionType( "WORLDCHAR" ))
	{
		return( CChar::CreateBasic((CREID_TYPE) m_File.GetArgHex())->r_Load(m_File));
	}
	else if ( m_File.IsSectionType( "WORLDITEM" ))
	{
		return( CItem::CreateScript((ITEMID_TYPE)m_File.GetArgHex())->r_Load(m_File));
	}
	else if ( m_File.IsSectionType( "GMPAGE" ))
	{
		return(( new CGMPage( m_File.GetArgStr()))->r_Load( m_File ));
	}
	else if ( m_File.IsSectionType( "SERVER" ))
	{
		CServRef * pServ = new CServRef( m_File.GetArgStr(), SOCKET_LOCAL_ADDRESS );
		pServ->r_Load( m_File );
		if ( pServ->GetName()[0] == '\0' )
		{
			delete pServ;
		}
		else
		{
			g_Serv.m_Servers.AddSortKey( pServ, pServ->GetName() );
		}
	}

	return( true );
}

bool CWorld::Load() // Load world from script
{
	DEBUG_CHECK( g_Serv.IsLoading());

	// The world has just started.
	m_UIDs.SetCount( 8*1024 );	// start count. (will grow as needed)
	m_Clock_Time = 0;

	// Load region info.
	if ( ! LoadRegions())
		return( false );

        bool fMySQLConnected = false;
        bool fStorageEnabled = false;
	const CServer::MySQLConfig & mySQLConfig = g_Serv.GetMySQLConfig();
	if ( mySQLConfig.m_fEnable )
	{
		if ( ! m_pStorage )
		{
			m_pStorage.reset( new MySqlStorageService());
		}

		if ( ! m_pStorage->Connect( mySQLConfig ))
		{
			const TCHAR * pszHost = mySQLConfig.m_sHost.IsEmpty() ? _TEXT("localhost") : (const TCHAR *) mySQLConfig.m_sHost;
			g_Log.Event( LOGM_INIT|LOGL_ERROR, "Failed to connect to MySQL server %s:%d (database '%s').\n", pszHost, mySQLConfig.m_iPort, (const TCHAR *) mySQLConfig.m_sDatabase );
			goto mysql_fail;
		}

		if ( m_pStorage && ! m_pStorage->EnsureSchema())
		{
			g_Log.Event( LOGM_INIT|LOGL_FATAL, "Failed to initialize MySQL schema.\n" );
			goto mysql_fail;
		}

                fMySQLConnected = true;
                fStorageEnabled = ( m_pStorage != NULL && m_pStorage->IsEnabled());
	}
	else if ( m_pStorage )
	{
		m_pStorage->Disconnect();
		m_pStorage.reset();
	}

	if ( ! LoadAccounts( false ))
		return( false );

	if ( fStorageEnabled )
	{
		if ( ! LoadFromStorage())
		{
			goto mysql_fail;
		}
	}
	else
	{
		CGString sSaveName;
		sSaveName.Format( "%s" GRAY_FILE "world", (const TCHAR*) g_Serv.m_sWorldBaseDir );

		if ( ! m_File.Open( sSaveName ))
		{
			g_Log.Event( LOGM_INIT|LOGL_ERROR, "No world data file\n" );
		}
		else
		{
			g_Serv.SysMessage( "World Is Loading...\n" );

			// Find the size of the file.
			m_lLoadSize = m_File.GetLength();
			m_iSaveStage = 0;	// Load stage as well.
			g_Log.SetScriptContext( &m_File );

			// Read the header stuff first.
			CScriptObj::r_Load( m_File );

			while ( m_File.IsFileOpen())
			{
				if ( g_Serv.m_fSecure )	// enable the try code.
				{
					try
					{
						if ( ! LoadSection())
						{
							g_Log.SetScriptContext( NULL );
							goto mysql_fail;
						}
					}
					catch (...)	// catch all
					{
						g_Log.Event( LOGM_INIT|LOGL_CRIT, "Load Exception line %d " GRAY_TITLE " is UNSTABLE!\n", m_File.GetLineNumber());
					}
				}
				else
				{
					if ( ! LoadSection())
					{
						g_Log.SetScriptContext( NULL );
						goto mysql_fail;
					}
				}
			}
			m_File.Close();
		}

			g_Log.SetScriptContext( NULL );
	}


	m_Clock_Startup = GetTime();
	m_Clock_Save = GetTime() + g_Serv.m_iSavePeriod;	// next save time.
	m_Clock_PrevSys = getclock();

	CItemBase::FindItemBase( ITEMID_ArrowX );	// Make sure these get loaded.
	CItemBase::FindItemBase( ITEMID_XBoltX );
	CItemBase::FindItemBase( ITEMID_Arrow );
	CItemBase::FindItemBase( ITEMID_XBolt );

	// Set all the sector light levels now that we know the time.
	for ( int j=0; j<SECTOR_QTY; j++ )
	{
		if ( ! m_Sectors[j].IsLightOverriden())
		{
			m_Sectors[j].SetLight(-1);
		}

		// Is this area too complex ?
		int iCount = m_Sectors[j].m_Items_Timer.GetCount() + m_Sectors[j].m_Items_Inert.GetCount();
		if ( iCount > 1024 )
		{
			DEBUG_ERR(( "Warning: %d items at %s, Sector too complex!\n", iCount, m_Sectors[j].GetBase().Write()));
		}
	}

	GarbageCollection();

	// Set the current version now.
	r_SetVal( "VERSION", GRAY_VERSION );	// Set m_iSaveVersion
	return( true );

mysql_fail:
	g_Log.SetScriptContext( NULL );
	if ( m_File.IsFileOpen())
	{
		m_File.Close();
	}
	if ( fMySQLConnected && m_pStorage )
	{
		m_pStorage->Disconnect();
		m_pStorage.reset();
	}
	return( false );
}

void CWorld::ReLoadBases()
{
	// After a pause/resync all the items need to resync their m_pDef pointers. (maybe)

	for ( int i=1; i<GetUIDCount(); i++ )
	{
		CObjBase * pObj = m_UIDs[i];
		if ( pObj == NULL || pObj == UID_PLACE_HOLDER ) 
			continue;	// not used.

		if ( pObj->IsItem())
		{
			CItem * pItem = dynamic_cast <CItem *>(pObj);
			ASSERT(pItem);
			pItem->SetBaseID( pItem->GetID()); // re-eval the m_pDef stuff.
		}
		else
		{
			CChar * pChar = dynamic_cast <CChar *>(pObj);
			ASSERT(pChar);
			pChar->SetID( pChar->GetID());	// re-eval the m_pDef stuff.
		}
	}
}

/////////////////////////////////////////////////////////////////

const TCHAR * CWorld::sm_Table[] =	// static
{
	"SAVECOUNT",
	"TIME",
	"TITLE",
	"VERSION",
};

bool CWorld::r_WriteVal( const TCHAR *pKey, CGString &sVal, CTextConsole * pSrc )
{
	switch ( FindTableSorted( pKey, sm_Table, COUNTOF(sm_Table)))
	{
	case 0: // "SAVECOUNT"
		sVal.FormatVal( m_iSaveCount );
		break;
	case 1:	// "TIME"
		sVal.FormatVal( GetTime());
		break;
	case 2: // 	"TITLE",
		sVal = GRAY_TITLE " World Script";
		break;
	case 3: // "VERSION"
		sVal = GRAY_VERSION;
		break;
	default:
		return( false );
	}
	return( true );
}

bool CWorld::r_LoadVal( CScript &s )
{
	switch ( FindTableSorted( s.GetKey(), sm_Table, COUNTOF(sm_Table)))
	{
	case 0: // "SAVECOUNT"
		m_iSaveCount = s.GetArgVal();
		break;
	case 1:	// "TIME"
		if ( m_Clock_Time )
		{
			DEBUG_MSG(( "Setting TIME while running is BAD!\n" ));
		}
		m_Clock_Time = s.GetArgVal();
		break;
	case 2: // 	"TITLE",
		return( false );
	case 3: // "VERSION"
		if ( s.m_pArg[0] == '0' ) 
			s.m_pArg++;
		m_iSaveVersion = s.GetArgVal();
		break;
	default:
		return( false );
	}

	return( true );
}

bool CWorld::r_Verb( CScript & s, CTextConsole * pSrc )
{
#ifdef COMMENT
	"SAVE",
	"BROADCAST"
#endif
	return( CScriptObj::r_Verb( s, pSrc ));
}

void CWorld::RespawnDeadNPCs()
{
	// Respawn dead NPC's
	for ( int i = 0; i<SECTOR_QTY; i++ )
	{
		m_Sectors[i].RespawnDeadNPCs();
	}
}

void CWorld::Restock()
{
	for ( int i = 0; i<SECTOR_QTY; i++ )
	{
		m_Sectors[i].Restock();
	}
}

void CWorld::Close()
{
	if ( IsSaving())	// Must complete save now !
	{
		Save( true );
	}
	m_File.Close();
	for ( int i = 0; i<SECTOR_QTY; i++ )
	{
		m_Sectors[i].Close();
	}

	if ( m_pStorage )
	{
		m_pStorage->Disconnect();
		m_pStorage.reset();
	}

	FlushDeletedObjects();	// clean up our delete list.
	m_ItemsNew.DeleteAll();
	m_CharsNew.DeleteAll();
	m_UIDs.RemoveAll();
	m_Regions.DeleteAll();
	m_GMPages.DeleteAll();
}

int CWorld::FixObjTry( CObjBase * pObj, int iUID )
{
	// RETURN: 0 = success.
	if ( iUID )
	{
		if (( pObj->GetUID() & UID_MASK ) != iUID )
		{
			// Miss linked in the UID table !!! BAD
			// Hopefully it was just not linked at all. else How the hell should i clean this up ???
			DEBUG_ERR(( "UID 0%x, '%s', Mislinked\n", iUID, pObj->GetName()));
			return( 0x7101 );
		}
	}
	return pObj->FixWeirdness();
}

int CWorld::FixObj( CObjBase * pObj, int iUID )
{
	// Attempt to fix problems with this item.
	// Ignore any children it may have for now.
	// RETURN: 0 = success.
	//  

	int iResultCode;
	if ( g_Serv.m_fSecure )	// enable the try code.
	{
		try
		{
			iResultCode = FixObjTry(pObj,iUID);
		}
		catch (...)	// catch all
		{
			iResultCode = 0xFFFF;	// bad mem ?
		}
	}
	else
	{
		iResultCode = FixObjTry(pObj,iUID);
	}
	if ( ! iResultCode ) 
		return( 0 );

#ifdef NDEBUG
	CItem * pItem = dynamic_cast <CItem*>(pObj);
	CChar * pChar = dynamic_cast <CChar*>(pObj);
#endif

	if ( g_Serv.m_fSecure )	// enable the try code.
	{
		try
		{
			iUID = pObj->GetUID();

			// is it a real error ?
			if ( pObj->IsItem())
			{
				CItem * pItem = dynamic_cast <CItem*>(pObj);
				if ( pItem && pItem->m_type == ITEM_EQ_MEMORY_OBJ )
				{
					pObj->Delete();
					return iResultCode;
				}
			}
			DEBUG_ERR(( "UID=0%x, id=0%x '%s', Invalid code=%0x\n", iUID, pObj->GetBaseID(), pObj->GetName(), iResultCode ));
			pObj->Delete();
		}
		catch (...)	// catch all
		{
			DEBUG_ERR(( "UID=0%x, Asserted cleanup\n", iUID ));
		}
	}
	else
	{
		// is it a real error ?
		iUID = pObj->GetUID();
		if ( pObj->IsItem())
		{
			CItem * pItem = dynamic_cast <CItem*>(pObj);
			if ( pItem && pItem->m_type == ITEM_EQ_MEMORY_OBJ )
			{
				pObj->Delete();
				return iResultCode;
			}
		}
		DEBUG_ERR(( "UID=0%x, id=0%x '%s', Invalid code=%0x\n", iUID, pObj->GetBaseID(), pObj->GetName(), iResultCode ));
		pObj->Delete();
	}
	return( iResultCode );
}

void CWorld::GarbageCollection_New()
{
	if ( m_ItemsNew.GetCount())
	{
		g_Log.Event( LOGL_ERROR, "%d Lost items deleted\n", m_ItemsNew.GetCount());
		m_ItemsNew.DeleteAll();
	}
	if ( m_CharsNew.GetCount())
	{
		g_Log.Event( LOGL_ERROR, "%d Lost chars deleted\n", m_CharsNew.GetCount());
		m_CharsNew.DeleteAll();
	}
}

void CWorld::GarbageCollection_GMPages()
{
	// Make sure all GM pages have accounts.
	CGMPage * pPage = dynamic_cast <CGMPage*>( m_GMPages.GetHead());
	while ( pPage!= NULL )
	{
		CGMPage * pPageNext = pPage->GetNext();
		if ( ! pPage->FindAccount()) // Open script file
		{
			DEBUG_ERR(( "GM Page has invalid account '%s'\n", pPage->GetName()));
			delete pPage;
		}
		pPage = pPageNext;
	}
}

void CWorld::GarbageCollection()
{
	g_Log.Flush();
	GarbageCollection_New();
	GarbageCollection_GMPages();

	// Go through the m_ppUIDs looking for Objects without links to reality.
	// This can take a while.

	int iCount = 0;
	int rCount = 0;
	for (int i = 1; i < GetUIDCount(); ++i)
	{
		CObjBase* pObj = m_UIDs[i];
		if (pObj == NULL || pObj == UID_PLACE_HOLDER)
			continue;	// not used. step forward until we find all the used numbers
		++rCount;
	}
	for ( int i=1; i<GetUIDCount(); ++i )
	{
		CObjBase * pObj = m_UIDs[i];
		if ( pObj == NULL || pObj == UID_PLACE_HOLDER ) 
			continue;	// not used.

		// Look for anomolies and fix them (that might mean delete it.)
		int iResultCode = FixObj( pObj, i );
		if ( iResultCode )
		{
		// Do an immediate delete here instead of Delete()
		NotifyStorageObjectRemoved( pObj );
		try
		{
			delete pObj;
		}
			catch(...)
			{
			}
			FreeUID(i);	// Get rid of junk uid if all fails..
			continue;
		}

		if (!(iCount & 0xFF))
		{
			g_Serv.PrintPercent(iCount, rCount);
		}
		iCount ++;
	}

	World_fDeleteCycle = true;
	FlushDeletedObjects();	// clean up our delete list.
	World_fDeleteCycle = false;

	if ( iCount != CObjBase::sm_iCount )	// All objects must be accounted for.
	{
		g_Log.Event( LOGL_ERROR, "Object memory leak %d!=%d\n", iCount, CObjBase::sm_iCount );
	}
	else
	{
		g_Log.Event( LOGL_EVENT, "%d Objects accounted for\n", iCount );
	}

	g_Log.Flush();
}

void CWorld::Speak( const CObjBaseTemplate * pSrc, const TCHAR * pText, COLOR_TYPE color, TALKMODE_TYPE mode, FONT_TYPE font )
{
	bool fSpeakAsGhost = false;	// I am a ghost ?
	int iHearRange = 0xFFFF;	// Broadcast i guess ?
	const CChar * pCharSrc = NULL;
	if ( pSrc != NULL )
	{
		pSrc = pSrc->GetTopLevelObj();
		switch ( mode )
		{
		case TALKMODE_YELL:
			iHearRange = pSrc->GetVisualRange() * 3;//just make it enough, 3 times the viewrange is good, don't exagerate
			break;
		case TALKMODE_BROADCAST:
			iHearRange = 0xFFFF;
			break;
		case TALKMODE_WHISPER:
			iHearRange = 3;
			break;
		default:
			iHearRange = UO_MAP_VIEW_SIZE;
			break;
		}
		if ( pSrc->IsChar())
		{
			// Are they dead ? Garble the text. unless we have SS
			pCharSrc = dynamic_cast <const CChar*> (pSrc);
			fSpeakAsGhost = pCharSrc->IsSpeakAsGhost();
		}
	}

	CGString sTextName;	// name labelled text.
	CGString sTextGhost; // ghost speak.

	for ( CClient * pClient = g_Serv.GetClientHead(); pClient!=NULL; pClient = pClient->GetNext())
	{
		if ( pClient->IsConsole())
		{
			if ( iHearRange != 0xFFFF ) 
				continue;	// not broadcast.
			pClient->SysMessage(pText);
			continue;
		}

		const TCHAR * pszSpeak = pText;
		if ( pSrc != NULL )
		{
			CChar * pChar = pClient->GetChar();
			if ( pChar == NULL ) 
				continue;	// not logged in.
			int iDist = pChar->GetTopDist3D( pSrc ) ;
			if ( iDist > iHearRange )
			{
				if ( ! pClient->IsPriv( PRIV_HEARALL ))	// in range based on mode ?
				{
					continue;
				}
				if ( pCharSrc && pCharSrc->GetPrivLevel() > pClient->GetPrivLevel())
				{
					continue;
				}
			}
			if ( iDist > UO_MAP_VIEW_SIZE )	// Must label the text.
			{
				if ( sTextName.IsEmpty())
				{
					if ( pClient->IsPriv( PRIV_HEARALL ))
					{
						sTextName.Format( "<%s [%lx]>%s", pSrc->GetName(), pSrc->GetUID(), pText );
					}
					else
					{
						sTextName.Format( "<%s>%s", pSrc->GetName(), pText );
					}
				}
				pszSpeak = sTextName;
			}
			else if ( fSpeakAsGhost && ! pChar->CanUnderstandGhost())
			{
				if ( sTextGhost.IsEmpty())	// Garble ghost.
				{
					sTextGhost = pText;
					for ( int i=0; i<sTextGhost.GetLength(); i++ )
					{
						if ( sTextGhost[i] != ' ' &&  sTextGhost[i] != '\t' )
						sTextGhost[i] = GetRandVal(2) ? 'O' : 'o';
					}
				}
				pszSpeak = sTextGhost;
			}
		}

		if ( fSpeakAsGhost && 
			g_Serv.m_fPlayerGhostSounds &&
			pSrc != NULL &&
			! pClient->GetChar()->CanUnderstandGhost())
		{
			pClient->addSound( Sounds_Ghost[ GetRandVal( COUNTOF( Sounds_Ghost )) ], pSrc );
		}

		pClient->addBark( pszSpeak, pSrc, color, mode, font );
	}
}

void CWorld::SpeakUNICODE( const CObjBaseTemplate * pSrc, const NCHAR * pText, COLOR_TYPE color, TALKMODE_TYPE mode, FONT_TYPE font, const char * pszLanguage )
{
	bool fSpeakAsGhost = false;	// I am a ghost ?
	int iHearRange = 0xFFFF;	// Broadcast i guess ?
	const CChar * pCharSrc = NULL;
	if ( pSrc != NULL )
	{
		pSrc = pSrc->GetTopLevelObj();
		switch ( mode )
		{
		case TALKMODE_YELL:
			iHearRange = pSrc->GetVisualRange() * 3;
			break;
		case TALKMODE_BROADCAST:
			iHearRange = 0xFFFF;
			break;
		case TALKMODE_WHISPER:
			iHearRange = 3;
			break;
		default:
			iHearRange = UO_MAP_VIEW_SIZE;
			break;
		}
		if ( pSrc->IsChar())
		{
			// Are they dead ? Garble the text. unless we have SS
			pCharSrc = dynamic_cast <const CChar*> (pSrc);
			fSpeakAsGhost = pCharSrc->IsSpeakAsGhost();
		}
	}

	//NCHAR wTextName[256];	// name labelled text.
	//NCHAR wTextGhost[256]; // ghost speak.

	for ( CClient * pClient = g_Serv.GetClientHead(); pClient!=NULL; pClient = pClient->GetNext())
	{
		if ( pClient->IsConsole())
		{
			if ( iHearRange != 0xFFFF )
				continue;
			// pClient->SysMessage(pText);
			continue;
		}
		const NCHAR * pszSpeak = pText;
		if ( pSrc != NULL )
		{
			CChar * pChar = pClient->GetChar();
			if ( pChar == NULL )
				continue;	// not logged in.
			int iDist = pChar->GetTopDist3D( pSrc ) ;
			if ( iDist > iHearRange )
			{
				if ( ! pClient->IsPriv( PRIV_HEARALL ))	// in range based on mode ?
				{
					continue;
				}
				if ( pCharSrc && pCharSrc->GetPrivLevel() > pClient->GetPrivLevel())
				{
					continue;
				}
			}

#if 0
			if ( iDist > UO_MAP_VIEW_SIZE )	// Must label the text.
			{
				if ( sTextName.IsEmpty())
				{
					if ( pClient->IsPriv( PRIV_HEARALL ))
					{
						sTextName.Format( "<%s [%lx]>%s", pSrc->GetName(), pSrc->GetUID(), pText );
					}
					else
					{
						sTextName.Format( "<%s>%s", pSrc->GetName(), pText );
					}
				}
				pszSpeak = sTextName;
			}
			else if ( fSpeakAsGhost && ! pChar->CanUnderstandGhost())
			{
				if ( sTextGhost.IsEmpty())	// Garble ghost.
				{
					sTextGhost = pText;
					for ( int i=0; i<sTextGhost.GetLength(); i++ )
					{
						if ( sTextGhost[i] != ' ' && sTextGhost[i] != '\t' )
							sTextGhost[i] = GetRandVal(2) ? 'O' : 'o';
					}
				}
				pszSpeak = sTextGhost;
			}
#endif

		}

		if ( fSpeakAsGhost && 
			g_Serv.m_fPlayerGhostSounds &&
			pSrc != NULL &&
			! pClient->GetChar()->CanUnderstandGhost())
		{
			pClient->addSound( Sounds_Ghost[ GetRandVal( COUNTOF( Sounds_Ghost )) ], pSrc );
		}

		pClient->addBarkUNICODE( pszSpeak, pSrc, color, mode, font, pszLanguage );
	}
}

void CWorld::OverHeadMessage(const CObjBaseTemplate* pSrc, const TCHAR* pText, COLOR_TYPE color, FONT_TYPE font, bool sendtoself)
{
	if (pSrc == NULL)
		return;
	const CChar* pCharSrc = NULL;
	pSrc = pSrc->GetTopLevelObj();
	if (pSrc->IsChar())
	{
		pCharSrc = dynamic_cast <const CChar*> (pSrc);
	}

	for (CClient* pClient = g_Serv.GetClientHead(); pClient != NULL; pClient = pClient->GetNext())
	{
		if (pSrc != NULL)
		{
			CChar* pChar = pClient->GetChar();
			if (pChar == NULL)
				continue;	// not logged in.
			int iDist = pChar->GetTopDist3D(pSrc);
			if (iDist > UO_MAP_VIEW_SIZE || (!sendtoself && pChar == pSrc))	// Must label the text.
			{
				continue;
			}
		}

		pClient->addBark(pText, pSrc, color, TALKMODE_SYSTEM, font);
	}
}

void CWorld::OverHeadMessageUNICODE(const CObjBaseTemplate* pSrc, const NCHAR* pText, COLOR_TYPE color, FONT_TYPE font, bool sendtoself)
{
	if (pSrc == NULL)
		return;
	const CChar* pCharSrc = NULL;
	pSrc = pSrc->GetTopLevelObj();
	if (pSrc->IsChar())
	{
		pCharSrc = dynamic_cast <const CChar*> (pSrc);
	}

	for (CClient* pClient = g_Serv.GetClientHead(); pClient != NULL; pClient = pClient->GetNext())
	{
		if (pSrc != NULL)
		{
			CChar* pChar = pClient->GetChar();
			if (pChar == NULL)
				continue;	// not logged in.
			int iDist = pChar->GetTopDist3D(pSrc);
			if (iDist > UO_MAP_VIEW_SIZE || (!sendtoself && pChar == pSrc))	// Must label the text.
			{
				continue;
			}
		}

		pClient->addBarkUNICODE(pText, pSrc, color, TALKMODE_SYSTEM, font);
	}
}

void CWorld::Broadcast( const TCHAR *pMsg ) // System broadcast in bold text
{
	Speak( NULL, pMsg, COLOR_TEXT_DEF, TALKMODE_BROADCAST, FONT_BOLD );
	g_Serv.SocketsFlush();
}

CItem * CWorld::Explode( CChar * pSrc, CPointMap p, int iDist, int iDamage, WORD wFlags )
{
	// Purple potions and dragons fire.
	// degrade damage the farther away we are. ???

	CItem * pItem = CItem::CreateBase( ITEMID_FX_EXPLODE_3 );
	pItem->m_Attr |= ATTR_MOVE_NEVER | ATTR_CAN_DECAY;
	pItem->m_type = ITEM_EXPLOSION;
	pItem->m_uidLink = pSrc ? pSrc->GetUID() : ((CObjUID) UID_CLEAR );
	pItem->m_itExplode.m_iDamage = iDamage;
	pItem->m_itExplode.m_wFlags = wFlags | DAMAGE_GENERAL | DAMAGE_HIT;
	pItem->m_itExplode.m_iDist = iDist;
	pItem->MoveTo( p );
	pItem->SetTimeout( 1 );	// Immediate.
	pItem->Sound( 0x207 );

	return( pItem );
}

//////////////////////////////////////////////////////////////////
// Game time.

#ifndef _WIN32
unsigned long int getclock()
{
	// clock_t times(struct tms *buf);
    struct timeval tv;
    gettimeofday(&tv, NULL);
    unsigned long TempTime;
    TempTime = ((((tv.tv_sec - 912818000) * CLOCKS_PER_SEC) +
		tv.tv_usec / CLOCKS_PER_SEC));
	return (TempTime);
}
#endif

UINT CWorld::GetGameWorldTime(UINT basetime ) const
{
	// basetime = TICK_PER_SEC time.
	// Get the time of the day in GameWorld minutes
	// 8 real world seconds = 1 game minute.
	// 1 real minute = 7.5 game minutes
	// 3.2 hours = 1 game day.
	return( basetime / g_Serv.m_iGameMinuteLength );
}

UINT CWorld::GetNextNewMoon( bool bMoonIndex ) const
{
	// "Predict" the next new moon for this moon
	// Get the period
	UINT iSynodic = bMoonIndex ? FELUCCA_SYNODIC_PERIOD : TRAMMEL_SYNODIC_PERIOD;

	// Add a "month" to the current game time
	UINT iNextMonth = g_World.GetTime() + iSynodic;

	// Get the game time when this cycle will start
	UINT iNewStart = (UINT) (iNextMonth -
		(double) (iNextMonth % iSynodic));

	// Convert to ticks
	return( iNewStart * g_Serv.m_iGameMinuteLength * TICK_PER_SEC );
}

int CWorld::GetMoonPhase (bool bMoonIndex) const
{
	// bMoonIndex is FALSE if we are looking for the phase of Trammel,
	// TRUE if we are looking for the phase of Felucca.

	// There are 8 distinct moon phases:  New, Waxing Crescent, First Quarter, Waxing Gibbous,
	// Full, Waning Gibbous, Third Quarter, and Waning Crescent

	// To calculate the phase, we use the following formula:
	//				CurrentTime % SynodicPeriod
	//	Phase = 	-----------------------------------------     * 8
	//			              SynodicPeriod
	//

	DWORD dwCurrentTime = GetGameWorldTime();

	if (!bMoonIndex)
	{
		// Trammel
		return( IMULDIV( dwCurrentTime % TRAMMEL_SYNODIC_PERIOD, 8, TRAMMEL_SYNODIC_PERIOD ));
	}
	else
	{
		// Luna2
		return( IMULDIV( dwCurrentTime % FELUCCA_SYNODIC_PERIOD, 8, FELUCCA_SYNODIC_PERIOD ));
	}
}

const TCHAR * CWorld::GetGameTime() const
{
	return( GetTimeMinDesc( GetGameWorldTime()));
}

void CWorld::OnTick()
{
	// Set the game time from the real world clock. getclock()
	// Do this once per tick.
	// 256 real secs = 1 GRAYhour. 19 light levels. check every 10 minutes or so.

	if ( g_Serv.m_fResyncPause )
		return;

	DWORD Clock_Sys = getclock();	// get the system time.
	int iTimeSysDiff = Clock_Sys - m_Clock_PrevSys;
	iTimeSysDiff = IMULDIV( TICK_PER_SEC, iTimeSysDiff, CLOCKS_PER_SEC );

	if ( iTimeSysDiff <= 0 )
	{
		if ( iTimeSysDiff == 0 ) // time is less than TICK_PER_SEC
		{
			return;
		}

		DEBUG_ERR(( "WARNING:system clock 0%xh overflow - recycle\n", Clock_Sys ));
		m_Clock_PrevSys = Clock_Sys;
		// just wait til next cycle and we should be ok
		return;
	}

	m_Clock_PrevSys = Clock_Sys;

	// NOTE: WARNING: time_t is signed !
	time_t Clock_New = m_Clock_Time + iTimeSysDiff;
	if ( Clock_New <= m_Clock_Time )	// should not happen (overflow)
	{
		if ( m_Clock_Time == Clock_New )
		{
			// Weird. This should never happen ?!
			g_Log.Event( LOGL_CRIT, "Clock corruption?" );
			return;	
		}

		// Someone has probably messed with the "TIME" value.
		// Very critical !
		g_Log.Event( LOGL_CRIT, "Clock overflow, reset from 0%x to 0%x\n", m_Clock_Time, Clock_New );
		m_Clock_Time = Clock_New;	// this may cause may strange things.
		return;
	}

	m_Clock_Time = Clock_New;

	if ( m_Clock_Sector <= GetTime())
	{
		// Only need a SECTOR_TICK_PERIOD tick to do world stuff.
		m_Clock_Sector = GetTime() + SECTOR_TICK_PERIOD;	// Next hit time.
		m_Sector_Pulse ++;
		for ( int i=0; i<SECTOR_QTY; i++ )
		{
			m_Sectors[i].OnTick( m_Sector_Pulse );
		}

		World_fDeleteCycle = true;
		FlushDeletedObjects();	// clean up our delete list.
		World_fDeleteCycle = false;
	}
	if ( m_Clock_Save <= GetTime())
	{
		// Auto save world
		m_Clock_Save = GetTime() + g_Serv.m_iSavePeriod;
		g_Log.Flush();
		Save( false );
	}
	if ( m_Clock_Respawn <= GetTime())
	{
		// Time to regen all the dead NPC's in the world.
		m_Clock_Respawn = GetTime() + (20*60*TICK_PER_SEC);
		RespawnDeadNPCs();
	}
}

