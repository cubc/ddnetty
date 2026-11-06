// ddnetty: focus mode
#include "focus.h"

#include <base/time.h>

#include <engine/graphics.h>
#include <engine/shared/config.h>
#include <engine/textrender.h>

#include <game/client/gameclient.h>
#include <game/localization.h>

#include <algorithm>
#include <iterator>

void CFocus::OnConsoleInit()
{
	Console()->Register("dt_focus", "?i[on]", CFGFLAG_CLIENT, ConFocus, this, "Toggle the focus mode (or turn it on/off with 1/0)");
	Console()->Register("+dt_focus", "", CFGFLAG_CLIENT, ConFocusHold, this, "Focus mode while the key is held");
}

void CFocus::ConFocus(IConsole::IResult *pResult, void *pUserData)
{
	CFocus *pSelf = static_cast<CFocus *>(pUserData);
	pSelf->SetActive(pResult->NumArguments() > 0 ? pResult->GetInteger(0) != 0 : !pSelf->Active());
}

void CFocus::ConFocusHold(IConsole::IResult *pResult, void *pUserData)
{
	static_cast<CFocus *>(pUserData)->SetActive(pResult->GetInteger(0) != 0);
}

bool CFocus::Active() const
{
	return g_Config.m_ClFocusMode != 0;
}

void CFocus::SetActive(bool Active)
{
	if(Active == this->Active())
		return;
	g_Config.m_ClFocusMode = Active ? 1 : 0;
	if(g_Config.m_DtFocusNotice)
		m_NoticeUntil = time_get() + time_freq();
}

int *CFocus::ItemConfig(EItem Item)
{
	switch(Item)
	{
	case ITEM_CHAT: return &g_Config.m_ClFocusModeHideChat;
	case ITEM_NAMES: return &g_Config.m_ClFocusModeHideNames;
	case ITEM_EFFECTS: return &g_Config.m_ClFocusModeHideEffects;
	case ITEM_HUD: return &g_Config.m_ClFocusModeHideHud;
	case ITEM_SCOREBOARD: return &g_Config.m_ClFocusModeHideScoreboard;
	case ITEM_UI: return &g_Config.m_ClFocusModeHideUI;
	case ITEM_MUSIC: return &g_Config.m_ClFocusModeHideSongPlayer;
	case ITEM_OTHERS: return &g_Config.m_DtFocusHideOthers;
	case ITEM_SPLITS: return &g_Config.m_DtFocusHideSplits;
	case ITEM_GATES: return &g_Config.m_DtFocusHideGates;
	case ITEM_BROADCAST: return &g_Config.m_DtFocusHideBroadcast;
	case ITEM_NOTICES: return &g_Config.m_DtFocusHideNotices;
	case NUM_ITEMS: break;
	}
	return nullptr;
}

bool CFocus::Hides(EItem Item) const
{
	if(!Active())
		return false;
	const int *pValue = ItemConfig(Item);
	return pValue && *pValue != 0;
}

void CFocus::ApplyPreset(EPreset Preset)
{
	for(int i = 0; i < NUM_ITEMS; i++)
	{
		const EItem Item = (EItem)i;
		const bool On = Preset == PRESET_ONLY_GAME || Item == ITEM_CHAT || Item == ITEM_NAMES || Item == ITEM_EFFECTS;
		*ItemConfig(Item) = On ? 1 : 0;
	}
}

void CFocus::OnReset()
{
	std::fill(std::begin(m_aLastHookTick), std::end(m_aLastHookTick), 0);
}

void CFocus::OnMapLoad()
{
	OnReset();
}

void CFocus::OnNewSnapshot()
{
	// who hooks you or gets hooked by you: on a duo map played without /team this is the partner
	const auto &Snap = GameClient()->m_Snap;
	const int Tick = Client()->GameTick(g_Config.m_ClDummy);
	auto &&IsOwn = [&](int ClientId) {
		return ClientId >= 0 && (ClientId == Snap.m_LocalClientId || ClientId == GameClient()->m_aLocalIds[0] || ClientId == GameClient()->m_aLocalIds[1] ||
						(Snap.m_SpecInfo.m_Active && ClientId == Snap.m_SpecInfo.m_SpectatorId));
	};
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(!Snap.m_aCharacters[i].m_Active)
			continue;
		const int Hooked = Snap.m_aCharacters[i].m_Cur.m_HookedPlayer;
		if(Hooked < 0 || Hooked >= MAX_CLIENTS || Hooked == i)
			continue;
		if(IsOwn(i) && !IsOwn(Hooked))
			m_aLastHookTick[Hooked] = Tick;
		else if(IsOwn(Hooked) && !IsOwn(i))
			m_aLastHookTick[i] = Tick;
	}
}

int CFocus::RecentPartner(int WithinTicks) const
{
	const int Tick = Client()->GameTick(g_Config.m_ClDummy);
	int Best = -1;
	for(int i = 0; i < MAX_CLIENTS; i++)
	{
		if(m_aLastHookTick[i] <= 0 || Tick - m_aLastHookTick[i] > WithinTicks || !GameClient()->m_Snap.m_apPlayerInfos[i])
			continue;
		if(Best < 0 || m_aLastHookTick[i] > m_aLastHookTick[Best])
			Best = i;
	}
	return Best;
}

bool CFocus::HookedRecently(int ClientId, int WithinTicks) const
{
	if(ClientId < 0 || ClientId >= MAX_CLIENTS || m_aLastHookTick[ClientId] <= 0)
		return false;
	return Client()->GameTick(g_Config.m_ClDummy) - m_aLastHookTick[ClientId] <= WithinTicks;
}

bool CFocus::HidesTee(int ClientId) const
{
	if(ClientId < 0 || ClientId >= MAX_CLIENTS || !Hides(ITEM_OTHERS))
		return false;
	const auto &Snap = GameClient()->m_Snap;
	if(ClientId == GameClient()->m_aLocalIds[0] || ClientId == GameClient()->m_aLocalIds[1] || ClientId == Snap.m_LocalClientId)
		return false;
	// the tee you are watching
	if(Snap.m_SpecInfo.m_Active && Snap.m_SpecInfo.m_SpectatorId == ClientId)
		return false;
	// your team stays, so nobody invisible gets in your way on a team map
	const int Watched = Snap.m_SpecInfo.m_Active && Snap.m_SpecInfo.m_SpectatorId >= 0 ? Snap.m_SpecInfo.m_SpectatorId : Snap.m_LocalClientId;
	if(Watched >= 0)
	{
		const int Team = GameClient()->m_Teams.Team(Watched);
		if(Team != TEAM_FLOCK && Team != TEAM_SUPER && GameClient()->m_Teams.Team(ClientId) == Team)
			return false;
	}
	// without a team: whoever you have been hooking with stays too
	return !HookedRecently(ClientId, PARTNER_SECONDS * Client()->GameTickSpeed());
}

void CFocus::OnRender()
{
	if(time_get() >= m_NoticeUntil)
		return;
	const float Height = 300.0f;
	const float Width = Height * Graphics()->ScreenAspect();
	Graphics()->MapScreen(CScreenRect(0.0f, 0.0f, Width, Height));
	const char *pText = Active() ? DtLocalize("Focus ON") : DtLocalize("Focus OFF");
	const float FontSize = 7.0f;
	TextRender()->TextColor(Active() ? ColorRGBA(0.45f, 1.0f, 0.55f, 0.9f) : ColorRGBA(1.0f, 1.0f, 1.0f, 0.7f));
	TextRender()->Text(Width - TextRender()->TextWidth(FontSize, pText) - 6.0f, Height - FontSize - 6.0f, FontSize, pText);
	TextRender()->TextColor(TextRender()->DefaultTextColor());
}
