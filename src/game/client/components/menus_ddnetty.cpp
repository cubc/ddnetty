// ddnetty: in-game "DDNetty" page (last tab on the right of the in-game menu bar)

#include "menus.h"

#include <base/math.h>
#include <base/str.h>

#include <engine/console.h>
#include <engine/shared/config.h>
#include <engine/storage.h>

#include <game/client/gameclient.h>
#include <game/client/ui.h>
#include <game/localization.h>

void CMenus::RenderDDNetty(CUIRect MainView)
{
	// background, same style as the other in-game pages
	MainView.Draw(ms_ColorTabbarActive, IGraphics::CORNER_B, 10.0f);
	MainView.Margin(10.0f, &MainView);

	// sub-tab bar
	CUIRect TabBar, Button;
	MainView.HSplitTop(24.0f, &TabBar, &MainView);
	MainView.HSplitTop(10.0f, nullptr, &MainView);

	static CButtonContainer s_aTabButtons[NUM_DDNETTY_TABS];
	const char *apTabNames[NUM_DDNETTY_TABS] = {
		DtLocalize("General"),
	};
	for(int Tab = 0; Tab < NUM_DDNETTY_TABS; Tab++)
	{
		TabBar.VSplitLeft(120.0f, &Button, &TabBar);
		TabBar.VSplitLeft(5.0f, nullptr, &TabBar);
		if(DoButton_Menu(&s_aTabButtons[Tab], apTabNames[Tab], m_DDNettyTab == Tab, &Button))
			m_DDNettyTab = Tab;
	}

	if(m_DDNettyTab == DDNETTY_TAB_GENERAL)
		RenderDDNettyGeneral(MainView);
}

void CMenus::RenderDDNettyGeneral(CUIRect MainView)
{
	const float HeadlineSize = 20.0f;
	const float FontSize = 14.0f;
	const float LineSize = 20.0f;
	const float Spacing = 5.0f;

	CUIRect Left, Right, Label, Button;
	MainView.VSplitMid(&Left, &Right, 20.0f);

	// --- Fake ping ---
	Left.HSplitTop(HeadlineSize + 5.0f, &Label, &Left);
	Ui()->DoLabel(&Label, DtLocalize("Fake ping"), HeadlineSize, TEXTALIGN_ML);
	Left.HSplitTop(Spacing, nullptr, &Left);

	// The slider edits a remembered value, so turning the fake ping off (0)
	// keeps the last chosen ping for when it's turned back on.
	static int s_FakePingValue = 999;
	if(g_Config.m_DtFakePing > 0)
		s_FakePingValue = g_Config.m_DtFakePing;

	Left.HSplitTop(LineSize, &Button, &Left);
	static int s_FakePingCheckbox = 0;
	if(DoButton_CheckBox(&s_FakePingCheckbox, DtLocalize("Enable fake ping"), g_Config.m_DtFakePing > 0, &Button))
		g_Config.m_DtFakePing = g_Config.m_DtFakePing > 0 ? 0 : s_FakePingValue;

	Left.HSplitTop(Spacing, nullptr, &Left);
	Left.HSplitTop(LineSize, &Button, &Left);
	if(Ui()->DoScrollbarOption(&s_FakePingValue, &s_FakePingValue, &Button, DtLocalize("Ping others see"), 1, 999, &CUi::ms_LinearScrollbarScale, 0u, " ms"))
	{
		if(g_Config.m_DtFakePing > 0)
			g_Config.m_DtFakePing = s_FakePingValue;
	}

	// real ping vs. what the scoreboard shows
	char aReal[64], aShown[64], aLine[160];
	const int RealPing = Client()->RealPingMs();
	if(RealPing >= 0)
		str_format(aReal, sizeof(aReal), DtLocalize("Real ping: %d ms"), RealPing);
	else
		str_format(aReal, sizeof(aReal), "%s", DtLocalize("Real ping: unknown"));

	const int LocalId = GameClient()->m_Snap.m_LocalClientId;
	const CNetObj_PlayerInfo *pLocalInfo = LocalId >= 0 ? GameClient()->m_Snap.m_apPlayerInfos[LocalId] : nullptr;
	if(pLocalInfo)
		str_format(aShown, sizeof(aShown), DtLocalize("Scoreboard: %d ms"), std::clamp(pLocalInfo->m_Latency, 0, 999));
	else
		str_format(aShown, sizeof(aShown), "%s", DtLocalize("Scoreboard: unknown"));
	str_format(aLine, sizeof(aLine), "%s  ·  %s", aReal, aShown);

	Left.HSplitTop(Spacing, nullptr, &Left);
	Left.HSplitTop(LineSize, &Label, &Left);
	Ui()->DoLabel(&Label, aLine, FontSize, TEXTALIGN_ML);

	Left.HSplitTop(LineSize, &Label, &Left);
	SLabelProperties HintProps;
	HintProps.SetColor(ColorRGBA(1.0f, 1.0f, 1.0f, 0.6f));
	HintProps.m_MaxWidth = Label.w;
	Ui()->DoLabel(&Label, DtLocalize("Fake ping only raises your ping, it can't lower it."), 12.0f, TEXTALIGN_ML, HintProps);

	// --- Console ---
	Right.HSplitTop(HeadlineSize + 5.0f, &Label, &Right);
	Ui()->DoLabel(&Label, DtLocalize("Console"), HeadlineSize, TEXTALIGN_ML);
	Right.HSplitTop(Spacing, nullptr, &Right);

	Right.HSplitTop(LineSize, &Label, &Right);
	Ui()->DoLabel(&Label, DtLocalize("Ctrl+Shift+C copies the whole console"), FontSize, TEXTALIGN_ML);

	Right.HSplitTop(Spacing * 2, nullptr, &Right);
	Right.HSplitTop(LineSize, &Button, &Right);
	static CButtonContainer s_CopyConsoleButton;
	if(DoButton_Menu(&s_CopyConsoleButton, DtLocalize("Copy console"), 0, &Button))
		Console()->ExecuteLine("copy_local_console", -1);

	Right.HSplitTop(Spacing, nullptr, &Right);
	Right.HSplitTop(LineSize, &Button, &Right);
	static CButtonContainer s_SaveConsoleButton;
	if(DoButton_Menu(&s_SaveConsoleButton, DtLocalize("Save console to file"), 0, &Button))
		Console()->ExecuteLine("dump_local_console", -1);

	Right.HSplitTop(Spacing, nullptr, &Right);
	Right.HSplitTop(LineSize, &Button, &Right);
	static CButtonContainer s_OpenDumpsButton;
	char aDumpsPath[IO_MAX_PATH_LENGTH];
	Storage()->GetCompletePath(IStorage::TYPE_SAVE, "dumps", aDumpsPath, sizeof(aDumpsPath));
	if(DoButton_Menu(&s_OpenDumpsButton, DtLocalize("Open console folder"), 0, &Button))
	{
		Storage()->CreateFolder("dumps", IStorage::TYPE_SAVE);
		Client()->ViewFile(aDumpsPath);
	}

	Right.HSplitTop(Spacing * 2, nullptr, &Right);
	Right.HSplitTop(LineSize, &Label, &Right);
	Ui()->DoLabel(&Label, DtLocalize("Saved consoles folder:"), FontSize, TEXTALIGN_ML);
	Right.HSplitTop(LineSize, &Label, &Right);
	SLabelProperties PathProps;
	PathProps.SetColor(ColorRGBA(1.0f, 1.0f, 1.0f, 0.6f));
	PathProps.m_MaxWidth = Label.w;
	PathProps.m_EllipsisAtEnd = true;
	Ui()->DoLabel(&Label, aDumpsPath, 12.0f, TEXTALIGN_ML, PathProps);
}
