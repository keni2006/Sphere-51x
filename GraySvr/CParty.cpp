/*#include "graysvr.h"	// predef header

///////////////////////////////////////////////////////////
// CCharRefArray

size_t CCharRefArray::FindChar(const CChar* pChar) const
{
	if (!pChar)
		return m_uidCharArray.BadIndex();

	CObjUID uid = pChar->GetUID();
	size_t iQty = m_uidCharArray.GetCount();
	for (size_t i = 0; i < iQty; ++i)
	{
		if (uid == m_uidCharArray[i])
			return i;
	}
	return m_uidCharArray.BadIndex();
}

size_t CCharRefArray::AttachChar(const CChar* pChar)
{
	size_t i = FindChar(pChar);
	if (i != m_uidCharArray.BadIndex())
		return i;
	m_uidCharArray.Add(pChar->GetUID());
	return m_uidCharArray.GetCount();
}

size_t CCharRefArray::InsertChar(const CChar* pChar, size_t i)
{
	size_t index = FindChar(pChar);
	if (index != m_uidCharArray.BadIndex())
	{
		if (index == i)	// already there
			return i;
		DetachChar(index);	// remove from list
	}

	if (!IsValidIndex(i))		// prevent from being inserted too high
		i = GetCharCount();

	m_uidCharArray.InsertAt(i, pChar->GetUID());
	return i;
}

void CCharRefArray::DetachChar(size_t i)
{
	m_uidCharArray.RemoveAt(i);
}

size_t CCharRefArray::DetachChar(const CChar* pChar)
{
	size_t i = FindChar(pChar);
	if (i != m_uidCharArray.BadIndex())
		DetachChar(i);
	return i;
}

void CCharRefArray::DeleteChars()
{
	size_t iQty = m_uidCharArray.GetCount();
	while (iQty > 0)
	{
		CChar* pChar = m_uidCharArray[--iQty].CharFind();
		if (pChar)
			pChar->Delete();
	}
	m_uidCharArray.RemoveAll();
}


void CCharRefArray::WritePartyChars(CScript& s)
{
	size_t iQty = m_uidCharArray.GetCount();
	for (size_t i = 0; i < iQty; ++i)		// write out links to all my chars
		s.WriteKeyHex("CHARUID", m_uidCharArray[i]);
}

///////////////////////////////////////////////////////////
// CPartyDef

CPartyDef::CPartyDef(CChar* pCharInviter, CChar* pCharAccept)
{
	AcceptMember(pCharInviter);
	AcceptMember(pCharAccept);
	SendAddList(NULL);		// send full list to all
	m_sName.Format("Party_0%", static_cast<DWORD>(pCharInviter->GetUID()));
}

size_t CPartyDef::AttachChar(CChar* pChar)
{
	if (pChar == NULL)
		return m_Chars.BadIndex();
	// RETURN:
	//  index of the char in the group. BadIndex = not in group.
	size_t i = m_Chars.AttachChar(pChar);
	//pChar->GetNotoColor NotoSave_Update();
	if (i != m_Chars.BadIndex())
	{
		if(pChar->m_pClient)
			pChar->m_pClient->addCharName(pChar);
		if(pChar->m_pPlayer)
			pChar->m_pPlayer->m_pParty = this;
	}
	return i;
}

size_t CPartyDef::DetachChar(CChar* pChar)
{
	// RETURN:
	//  index of the char in the group. BadIndex = not in group.
	size_t i = m_Chars.DetachChar(pChar);
	if (i != m_Chars.BadIndex())
	{
		// Remove map waypoint of party members
		if (pChar->m_pClient)
		{
			size_t iQty = m_Chars.GetCharCount();
			CChar* pPartyMember = NULL;
			for (size_t j = 0; j < iQty; ++j)
			{
				pPartyMember = m_Chars.GetChar(j).CharFind();
				if (!pPartyMember)
					continue;
			}
		}
		if (pChar->m_pPlayer)
			pChar->m_pPlayer->m_pParty = NULL;
		//pChar->DeleteKey("PARTY_LASTINVITE");
		//pChar->DeleteKey("PARTY_LASTINVITETIME");
		if (pChar->m_pClient)
			pChar->m_pClient->addCharName(pChar);
		//pChar->NotoSave_Update();
	}
	return i;
}

bool CPartyDef::SetMaster(CChar* pChar)
{
	if (!pChar || !IsInParty(pChar) || IsPartyMaster(pChar))
		return false;

	size_t i = m_Chars.InsertChar(pChar, 0);
	SendAddList(NULL);
	return (i == 0);
}

void CPartyDef::SetLootFlag(CChar* pChar, bool fSet)
{
	ASSERT(pChar);
	if (IsInParty(pChar))
	{
		//pChar->SetKeyNum("PARTY_CANLOOTME", fSet);
		//pChar->NotoSave_Update();
		pChar->SysMessage(fSet ? "Party members CAN loot you" : "Party members CANNOT loot you");
	}
}

bool CPartyDef::GetLootFlag(const CChar* pChar)
{
	ASSERT(pChar);
	//if (IsInParty(pChar))
		//return (pChar->GetKeyNum("PARTY_CANLOOTME") != 0);

	return false;
}

void CPartyDef::StatsUpdateAll(CChar* pCharSrc, PacketSend* pPacket)
{
	size_t iQty = m_Chars.GetCharCount();
	CChar* pChar = NULL;
	for (size_t i = 0; i < iQty; ++i)
	{
		pChar = m_Chars.GetChar(i).CharFind();
		if (!pChar || !pChar->m_pClient || (pChar == pCharSrc) || !pChar->CanSee(pCharSrc))
			continue;
		pPacket->send(pChar->m_pClient);
	}
}

void CPartyDef::SysMessageAll(LPCTSTR pszMsg)
{
	// SysMessage to all party members
	size_t iQty = m_Chars.GetCharCount();
	CChar* pChar = NULL;
	for (size_t i = 0; i < iQty; ++i)
	{
		pChar = m_Chars.GetChar(i).CharFind();
		if (!pChar || !pChar->m_pClient)
			continue;
		pChar->m_pClient->SysMessage(pszMsg);
	}
}

bool CPartyDef::SendMemberMsg(const CChar* pCharDest, const LPCTSTR* pMsg)
{
	if (!pCharDest)
	{
		SendAll(pMsg);
		return true;
	}

	if (pCharDest->m_pPlayer)
	{
		// Weirdness check.
		if (pCharDest->m_pPlayer->m_pParty != this)
		{
			if (DetachChar(pCharDest) != m_Chars.BadIndex())	// this is bad!
				return false;
			return true;
		}
	
		if (!m_Chars.IsCharIn(pCharDest))
		{
			pCharDest->m_pPlayer->m_pParty = NULL;
			return true;
		}

		if (pCharDest->m_pClient)
			pCharDest->m_pClient->SysMessage (pMsg);
	}
	return true;
}

void CPartyDef::SendAll(const LPCTSTR* pMsg)
{
	// Send this to all party members.
	size_t iQty = m_Chars.GetCharCount();
	CChar* pChar = NULL;
	for (size_t i = 0; i < iQty; ++i)
	{
		pChar = m_Chars.GetChar(i).CharFind();
		if (!pChar)
			continue;

		if (!SendMemberMsg(pChar, pMsg))
		{
			--iQty;
			--i;
		}
	}
}

bool CPartyDef::SendAddList(CChar* pCharDest)
{
	if (m_Chars.GetCharCount() <= 0)
		return false;

	PacketPartyList cmd(&m_Chars);
	if (pCharDest)
		SendMemberMsg(pCharDest, &cmd);
	else
		SendAll(&cmd);

	return true;
}

bool CPartyDef::SendRemoveList(CChar* pCharRemove)
{
	PacketPartyRemoveMember(pCharRemove, NULL);
	SendMemberMsg(pCharRemove, &cmd);
	return true;
}

bool CPartyDef::MessageEvent(CChar* pCharDest, CChar* pCharSrc, const NCHAR* pszMsg)
{
	if (!pszMsg || (pCharDest && !IsInParty(pCharDest)))
		return false;

	if (!pCharSrc)
		return false;

	TCHAR* pszText = Str_GetTemp();
	CvtNUNICODEToSystem(pszText, MAX_TALK_BUFFER, pszMsg, MAX_TALK_BUFFER);

	if (!m_pSpeechFunction.IsEmpty())
	{
		TRIGRET_TYPE tr = TRIGRET_RET_FALSE;
		CScriptTriggerArgs Args;
		Args.m_iN1 = pCharSrc->GetUID();
		Args.m_iN2 = pCharDest ? pCharDest->GetUID() : static_cast<CGrayUID>(UID_CLEAR);
		Args.m_s1 = pszText;
		Args.m_s1_raw = pszText;

		if (r_Call(m_pSpeechFunction, &g_Serv, &Args, NULL, &tr))
		{
			if (tr == TRIGRET_RET_TRUE)
				return false;
		}
	}

	if (g_Log.IsLoggedMask(LOGM_PLAYER_SPEAK))
		g_Log.Event(LOGM_PLAYER_SPEAK, "%lx:'%s' Says '%s' in party to '%s'\n", pCharSrc->m_pClient->GetSocketID(), pCharSrc->GetName(), pszText, pCharDest ? pCharDest->GetName() : "all");

	PacketPartyChat cmd(pCharSrc, pszMsg);

	if (pCharDest)
		SendMemberMsg(pCharDest, &cmd);
	else
		SendAll(&cmd);

	return true;
}

void CPartyDef::AcceptMember(CChar* pChar)
{
	// This person has accepted to be part of the party.
	ASSERT(pChar);

	pChar->m_pParty = this;
	AttachChar(pChar);
	SendAddList(NULL);
}

bool CPartyDef::RemoveMember(CChar* pChar, CChar* pCharSrc)
{
	// NOTE: remove of the master will cause the party to disband.

	if (!pChar || (m_Chars.GetCharCount() <= 0))
		return false;

	CChar* pCharMaster = GetMaster();
	if ((pCharSrc != pChar) && (pCharSrc != pCharMaster))
		return false;

	if (!pChar || !IsInParty(pChar))
		return false;

	if (pChar == pCharMaster)
		return Disband();

	if (pCharSrc && IsTrigUsed(TRIGGER_PARTYREMOVE))
	{
		CScriptTriggerArgs Args;
		if (pChar->OnTrigger(CTRIG_PartyRemove, pCharSrc, &Args) == TRIGRET_RET_TRUE)
			return false;
	}
	if (IsTrigUsed(TRIGGER_PARTYLEAVE))
	{
		if (pChar->OnTrigger(CTRIG_PartyLeave, pChar, 0) == TRIGRET_RET_TRUE)
			return false;
	}

	// Remove it from the party
	SendRemoveList(pChar);
	DetachChar(pChar);
	pChar->SysMessageDefault(DEFMSG_PARTY_REMOVED);

	TCHAR szMsg[EXPRESSION_MAX_KEY_LEN];
	snprintf(szMsg, sizeof(szMsg), g_Cfg.GetDefaultMsg(DEFMSG_PARTY_TARG_REMOVE_SUCCESS), pChar->GetName());
	SysMessageAll(szMsg);

	if (m_Chars.GetCharCount() <= 1)
	{
		// Disband the party
		SysMessageAll(g_Cfg.GetDefaultMsg(DEFMSG_PARTY_LAST_PERSON_LEFT));
		return Disband();
	}

	return true;
}

bool CPartyDef::Disband()
{
	// Make sure i am the master.
	if (m_Chars.GetCharCount() <= 0)
		return false;

	CChar* pCharMaster = GetMaster();
	if (pCharMaster && IsTrigUsed(TRIGGER_PARTYDISBAND))
	{
		CScriptTriggerArgs Args;
		if (pCharMaster->OnTrigger(CTRIG_PartyDisband, pCharMaster, &Args) == TRIGRET_RET_TRUE)
			return false;
	}

	SysMessageAll(g_Cfg.GetDefaultMsg(DEFMSG_PARTY_DISBANDED));

	CChar* pChar = NULL;
	for (int i = m_Chars.GetCharCount() - 1; i >= 0; --i)
	{
		pChar = m_Chars.GetChar(i).CharFind();
		if (!pChar)
			continue;

		if (IsTrigUsed(TRIGGER_PARTYREMOVE))
		{
			CScriptTriggerArgs Args;
			Args.m_iN1 = 1;
			pChar->OnTrigger(CTRIG_PartyRemove, pCharMaster, &Args);
		}

		SendRemoveList(pChar);
		DetachChar(pChar);
	}

	delete this;	// should remove itself from the world list.
	return true;
}

bool CPartyDef::DeclineEvent(CChar* pCharDecline, CChar* pCharInviter)	// static
{
	// This should happen after a timeout as well.
	// "You notify %s that you do not wish to join the party"

	if (!pCharInviter || !pCharDecline)
		return false;

	const CVarDefCont* pVar = pCharInviter->GetTagDefs()->GetKey("PARTY_LASTINVITE");
	if (!pVar || (static_cast<CGrayUID>(pVar->GetValNum()) != pCharDecline->GetUID()))
		return false;

	pCharInviter->DeleteKey("PARTY_LASTINVITE");
	pCharInviter->SysMessagef(g_Cfg.GetDefaultMsg(DEFMSG_PARTY_DECLINE), pCharDecline->GetName());
	pCharDecline->SysMessage(g_Cfg.GetDefaultMsg(DEFMSG_PARTY_DECLINE_NOTIFY));
	return true;
}

bool CPartyDef::AcceptEvent(CChar* pCharAccept, CChar* pCharInviter, bool fForced)	// static
{
	// We are accepting the invite to join a party
	// No security checks if bForced -> true !!!
	// Party master is only one that can add ! GetChar(0)

	if (!pCharInviter || !pCharInviter->m_pClient || !pCharAccept || !pCharAccept->m_pClient || (pCharInviter == pCharAccept))
		return false;

	if (!fForced)
	{
		const CVarDefCont* pVar = pCharInviter->GetTagDefs()->GetKey("PARTY_LASTINVITE");
		if (!pVar || (static_cast<CGrayUID>(pVar->GetValNum()) != pCharAccept->GetUID()))
			return false;

		pCharInviter->DeleteKey("PARTY_LASTINVITE");
		if (!pCharInviter->CanSee(pCharAccept))
			return false;
	}

	CPartyDef* pParty = pCharInviter->m_pParty;

	if (pCharAccept->m_pParty)	// already in a party
	{
		if (pCharAccept->m_pParty == pParty)	// already in this party
		{
			pCharInviter->SysMessageDefault(DEFMSG_PARTY_ALREADY_YOUR);
			return true;
		}

		if (fForced)
			pCharAccept->m_pParty->RemoveMember(pCharAccept);
		else
			return false;
	}

	TCHAR szMsg[EXPRESSION_MAX_KEY_LEN];
	snprintf(szMsg, sizeof(szMsg), g_Cfg.GetDefaultMsg(DEFMSG_PARTY_JOINED), pCharAccept->GetName());

	if (!pParty)
	{
		// Create the party now.
		pParty = new CPartyDef(pCharInviter, pCharAccept);
		ASSERT(pParty);
		g_World.m_Parties.InsertHead(pParty);
		pCharInviter->SysMessage(szMsg);
	}
	else
	{
		// Add to existing party
		if (pParty->IsPartyFull() || (!fForced && !pParty->IsPartyMaster(pCharInviter)))
			return false;

		pParty->SysMessageAll(szMsg);	// tell everyone already in the party about this
		pParty->AcceptMember(pCharAccept);
	}

	pCharAccept->SysMessageDefault(DEFMSG_PARTY_ADDED);
	return true;
}

enum PDV_TYPE
{
#define ADD(a,b) PDV_##a,
#include "../tables/CParty_functions.tbl"
#undef ADD
	PDV_QTY
};

const LPCTSTR CPartyDef::sm_szVerbKeys[PDV_QTY + 1] =
{
	#define ADD(a,b) b,
	#include "../tables/CParty_functions.tbl"
	#undef ADD
	NULL
};

enum PDC_TYPE
{
#define ADD(a,b) PDC_##a,
#include "../tables/CParty_props.tbl"
#undef ADD
	PDC_QTY
};

const LPCTSTR CPartyDef::sm_szLoadKeys[PDC_QTY + 1] =
{
	#define ADD(a,b) b,
	#include "../tables/CParty_props.tbl"
	#undef ADD
	NULL
};

bool CPartyDef::r_GetRef(LPCTSTR& pszKey, CScriptObj*& pRef)
{
	ADDTOCALLSTACK("CPartyDef::r_GetRef");
	if (!strnicmp("MASTER.", pszKey, 7))
	{
		pszKey += 7;
		CChar* pCharMaster = GetMaster();
		if (pCharMaster)
		{
			pRef = pCharMaster;
			return true;
		}
	}
	else if (!strnicmp("MEMBER.", pszKey, 7))
	{
		pszKey += 7;
		int i = Exp_GetVal(pszKey);
		SKIP_SEPARATORS(pszKey);
		if ((i < 0) || !m_Chars.IsValidIndex(i))
			return false;

		CChar* pMember = m_Chars.GetChar(i).CharFind();
		if (pMember)
		{
			pRef = pMember;
			return true;
		}
	}
	return false;
}

bool CPartyDef::r_LoadVal(CScript& s)
{
	EXC_TRY("LoadVal");
	LPCTSTR pszKey = s.GetKey();

	int index = FindTableHeadSorted(pszKey, sm_szLoadKeys, COUNTOF(sm_szLoadKeys) - 1);
	switch (index)
	{
	case PDC_SPEECHFILTER:
	{
		if (s.HasArgs())
		{
			LPCTSTR pszArg = s.GetArgStr();
			CResourceLink* m_pTestEvent = dynamic_cast<CResourceLink*>(g_Cfg.ResourceGetDefByName(RES_FUNCTION, pszArg));
			if (!m_pTestEvent)
				return false;

			m_pSpeechFunction = pszArg;
		}
		else
			m_pSpeechFunction.Empty();
		break;
	}
	case PDC_TAG0:
	case PDC_TAG:
	{
		bool fQuoted = false;
		pszKey += (index == PDC_TAG0) ? 5 : 4;
		m_TagDefs.SetStr(pszKey, fQuoted, s.GetArgStr(&fQuoted), (index == PDC_TAG0));
		break;
	}
	default:
	{
		return false;
	}
	}
	return true;
	EXC_CATCH;

	EXC_DEBUG_START;
	EXC_ADD_SCRIPT;
	EXC_DEBUG_END;
	return false;
}

bool CPartyDef::r_WriteVal(LPCTSTR pszKey, CGString& sVal, CTextConsole* pSrc)
{
	CScriptObj* pRef;
	if (r_GetRef(pszKey, pRef))
	{
		if (!pRef)		// good command but bad link.
		{
			sVal = "0";
			return true;
		}
		if (pszKey[0] == '\0')	// we where just testing the ref.
		{
			CObjBase* pObj = dynamic_cast<CObjBase*>(pRef);
			if (pObj)
				sVal.FormatHex(static_cast<DWORD>(pObj->GetUID()));
			else
				sVal = "1";
			return true;
		}
		return pRef->r_WriteVal(pszKey, sVal, pSrc);
	}

	switch (FindTableHeadSorted(pszKey, sm_szLoadKeys, COUNTOF(sm_szLoadKeys) - 1))
	{
	case PDC_ISSAMEPARTYOF:
	{
		pszKey += 13;
		GETNONWHITESPACE(pszKey);
		CChar* pChar = static_cast<CGrayUID>(Exp_GetLLVal(pszKey)).CharFind();
		sVal.FormatVal(pChar && (pChar->m_pParty == this));
		break;
	}
	case PDC_MEMBERS:
	{
		sVal.FormatVal(m_Chars.GetCharCount());
		break;
	}
	case PDC_SPEECHFILTER:
	{
		sVal = m_pSpeechFunction.IsEmpty() ? "" : static_cast<LPCTSTR>(m_pSpeechFunction);
		break;
	}
	case PDC_TAG:
	{
		if (pszKey[3] != '.')
			return false;
		pszKey += 4;
		sVal = m_TagDefs.GetKeyStr(pszKey, false);
		break;
	}
	case PDC_TAG0:
	{
		if (pszKey[4] != '.')
			return false;
		pszKey += 5;
		sVal = m_TagDefs.GetKeyStr(pszKey, true);
		break;
	}
	case PDC_TAGAT:
	{
		pszKey += 5;	// eat the 'TAGAT'
		if (*pszKey == '.')	// do we have an argument?
		{
			SKIP_SEPARATORS(pszKey);
			size_t iQty = static_cast<size_t>(Exp_GetLLVal(pszKey));
			if (iQty >= m_TagDefs.GetCount())
				return false;	// trying to get non-existant tag

			const CVarDefCont* pVar = m_TagDefs.GetAt(iQty);
			if (!pVar)
				return false;	// trying to get non-existant tag

			SKIP_SEPARATORS(pszKey);
			if (!*pszKey)
			{
				sVal.Format("%s=%s", pVar->GetKey(), pVar->GetValStr());
				return true;
			}
			else if (!strnicmp(pszKey, "KEY", 3))
			{
				sVal = pVar->GetKey();
				return true;
			}
			else if (!strnicmp(pszKey, "VAL", 3))
			{
				sVal = pVar->GetValStr();
				return true;
			}
		}
		return false;
	}
	case PDC_TAGCOUNT:
	{
		sVal.FormatVal(m_TagDefs.GetCount());
		break;
	}
	default:
	{
		return false;
	}
	}

	return true;
	EXC_CATCH;

	EXC_DEBUG_START;
	EXC_ADD_KEYRET(pSrc);
	EXC_DEBUG_END;
	return false;
}

bool CPartyDef::r_Verb(CScript& s, CTextConsole* pSrc)
{
	ASSERT(pSrc);

	LPCTSTR pszKey = s.GetKey();
	CScriptObj* pRef;
	if (r_GetRef(pszKey, pRef))
	{
		if (pszKey[0])
		{
			if (!pRef)
				return true;
			CScript script(pszKey, s.GetArgStr());
			return pRef->r_Verb(script, pSrc);
		}
	}

	int index = FindTableSorted(pszKey, sm_szVerbKeys, COUNTOF(sm_szVerbKeys) - 1);
	switch (index)
	{
	case PDV_ADDMEMBER:
	case PDV_ADDMEMBERFORCED:
	{
		CGrayUID uid = static_cast<CGrayUID>(s.GetArgVal());
		CChar* pCharAdd = uid.CharFind();
		if (!pCharAdd || IsInParty(pCharAdd))
			break;

		bool fForced = (index == PDV_ADDMEMBERFORCED);
		CChar* pCharMaster = GetMaster();
		if (pCharMaster && !fForced)
			pCharMaster->SetKeyNum("PARTY_LASTINVITE", static_cast<INT64>(uid));

		CPartyDef::AcceptEvent(pCharAdd, pCharMaster, fForced);
		break;
	}
	case PDV_CLEARTAGS:
	{
		LPCTSTR pszArg = s.GetArgStr();
		SKIP_SEPARATORS(pszArg);
		m_TagDefs.ClearKeys(pszArg);
		break;
	}
	case PDV_DISBAND:
	{
		Disband();
		break;
	}
	case PDV_CREATE:
	case PDV_MESSAGE:
	{
		break;
	}
	case PDV_REMOVEMEMBER:
	{
		CGrayUID uid;
		LPCTSTR pszArg = s.GetArgStr();
		if (*pszArg == '@')
		{
			++pszArg;
			int i = Exp_GetVal(pszArg);
			if ((i < 0) || !m_Chars.IsValidIndex(i))
				break;

			uid = m_Chars.GetChar(i);
		}
		else
			uid = static_cast<CGrayUID>(s.GetArgVal());

		RemoveMember(uid.CharFind());
		break;
	}
	case PDV_SETMASTER:
	{
		CGrayUID uid;
		LPCTSTR pszArg = s.GetArgStr();
		if (*pszArg == '@')
		{
			++pszArg;
			int i = Exp_GetVal(pszArg);
			if ((i < 0) || !m_Chars.IsValidIndex(i))
				break;

			uid = m_Chars.GetChar(i);
		}
		else
			uid = static_cast<CGrayUID>(s.GetArgVal());

		SetMaster(uid.CharFind());
		break;
	}
	case PDV_SYSMESSAGE:
	{
		CGrayUID uid;
		LPCTSTR pszArg = s.GetArgStr();
		TCHAR* pszUID = Str_GetTemp();
		size_t iLen = 0;

		if (*pszArg == '@')
		{
			++pszArg;
			if (*pszArg != '@')
			{
				LPCTSTR pszArgBackup = pszArg;
				while (*pszArg != ' ')
				{
					++pszArg;
					++iLen;
				}
				strncpy(pszUID, pszArgBackup, ++iLen);
				pszUID[iLen - 1] = '\0';

				int i = Exp_GetVal(pszUID);
				if ((i < 0) || !m_Chars.IsValidIndex(i))
					break;

				uid = m_Chars.GetChar(i);
			}
		}
		else
		{
			LPCTSTR pszArgBackup = pszArg;
			while (*pszArg != ' ')
			{
				++pszArg;
				++iLen;
			}
			strncpy(pszUID, pszArgBackup, ++iLen);
			pszUID[iLen - 1] = '\0';

			uid = static_cast<CGrayUID>(Exp_GetLLVal(pszUID));
		}

		SKIP_SEPARATORS(pszArg);
		if (uid)
		{
			CChar* pSend = uid.CharFind();
			if (pSend)
				pSend->SysMessage(pszArg);
		}
		else
			SysMessageAll(pszArg);
		break;
	}
	case PDV_TAGLIST:
	{
		m_TagDefs.DumpKeys(pSrc, "TAG.");
		break;
	}
	default:
	{
		return false;
	}
	}
	return true;
}

bool CPartyDef::r_Load(CScript& s)
{
	UNREFERENCED_PARAMETER(s);
	return false;
}*/
