// ddnetty: in-game "DDNetty" page (last tab on the right of the in-game menu bar)

#include "menus.h"

#include <base/math.h>
#include <base/str.h>
#include <base/time.h>

#include <engine/console.h>
#include <engine/shared/config.h>
#include <engine/storage.h>

#include <game/client/components/ddnetty/splits.h>
#include <game/client/gameclient.h>
#include <game/client/ui.h>
#include <game/client/ui_scrollregion.h>
#include <game/ddnetty_splits.h>
#include <game/localization.h>

#include <algorithm>
#include <climits>
#include <ctime>

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
		"Splits",
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
	else if(m_DDNettyTab == DDNETTY_TAB_SPLITS)
		RenderDDNettySplits(MainView);
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
	// the slider shows 0..999; 0 is stored as 1 because a stored 0 means "off"
	static int s_FakePingValue = 999;
	if(g_Config.m_DtFakePing > 0)
		s_FakePingValue = g_Config.m_DtFakePing == 1 ? 0 : g_Config.m_DtFakePing;

	Left.HSplitTop(LineSize, &Button, &Left);
	static int s_FakePingCheckbox = 0;
	if(DoButton_CheckBox(&s_FakePingCheckbox, DtLocalize("Enable fake ping"), g_Config.m_DtFakePing > 0, &Button))
		g_Config.m_DtFakePing = g_Config.m_DtFakePing > 0 ? 0 : std::max(s_FakePingValue, 1);

	Left.HSplitTop(Spacing, nullptr, &Left);
	Left.HSplitTop(LineSize, &Button, &Left);
	if(Ui()->DoScrollbarOption(&s_FakePingValue, &s_FakePingValue, &Button, DtLocalize("Ping others see"), 0, 999, &CUi::ms_LinearScrollbarScale, 0u, " ms"))
	{
		if(g_Config.m_DtFakePing > 0)
			g_Config.m_DtFakePing = std::max(s_FakePingValue, 1);
	}

	// below the real ping: how close to the server's tick we dare to ack
	Left.HSplitTop(Spacing, nullptr, &Left);
	Left.HSplitTop(LineSize, &Button, &Left);
	Ui()->DoScrollbarOption(&g_Config.m_DtFakePingSafety, &g_Config.m_DtFakePingSafety, &Button, DtLocalize("Safety (lower ping)"), 0, 5, &CUi::ms_LinearScrollbarScale, 0u, " ticks");

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

	Left.HSplitTop(LineSize * 2.0f, &Label, &Left);
	SLabelProperties HintProps;
	HintProps.SetColor(ColorRGBA(1.0f, 1.0f, 1.0f, 0.6f));
	HintProps.m_MaxWidth = Label.w;
	Ui()->DoLabel(&Label, DtLocalize("Below your real ping it acks snapshots still on their way. Safety 0 goes lowest but can drop inputs; it backs off by itself if that happens."), 12.0f, TEXTALIGN_TL, HintProps);

	// --- BestClient network ---
	Left.HSplitTop(Spacing * 3.0f, nullptr, &Left);
	Left.HSplitTop(HeadlineSize + 5.0f, &Label, &Left);
	Ui()->DoLabel(&Label, "BestClient", HeadlineSize, TEXTALIGN_ML);
	Left.HSplitTop(Spacing, nullptr, &Left);
	Left.HSplitTop(LineSize, &Button, &Left);
	static int s_PresenceCheckbox = 0;
	if(DoButton_CheckBox(&s_PresenceCheckbox, DtLocalize("Show up for BestClient users"), g_Config.m_DtBestClientPresence, &Button))
		g_Config.m_DtBestClientPresence ^= 1;
	Left.HSplitTop(LineSize * 2.0f, &Label, &Left);
	Ui()->DoLabel(&Label, DtLocalize("BestClient marks DDNetty as FAKE because it isn't an official build. Off: nobody sees any icon on you, and you may stop seeing theirs."), 12.0f, TEXTALIGN_TL, HintProps);

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

	// --- Focus mode ---
	Right.HSplitTop(Spacing * 3.0f, nullptr, &Right);
	Right.HSplitTop(HeadlineSize + 5.0f, &Label, &Right);
	Ui()->DoLabel(&Label, DtLocalize("Focus mode"), HeadlineSize, TEXTALIGN_ML);
	Right.HSplitTop(Spacing, nullptr, &Right);

	Right.HSplitTop(LineSize, &Button, &Right);
	static int s_FocusCheckbox = 0;
	if(DoButton_CheckBox(&s_FocusCheckbox, DtLocalize("Enable focus mode"), GameClient()->m_Focus.Active(), &Button))
		GameClient()->m_Focus.SetActive(!GameClient()->m_Focus.Active());
	GameClient()->m_Tooltips.DoToolTip(&s_FocusCheckbox, &Button, DtLocalize("dt_focus toggles it, +dt_focus keeps it on only while the key is held."));
	Right.HSplitTop(Spacing, nullptr, &Right);
	static CButtonContainer s_FocusReader, s_FocusClear;
	DoLine_KeyReader(Right, s_FocusReader, s_FocusClear, DtLocalize("Toggle key"), "dt_focus");

	Right.HSplitTop(LineSize, &Button, &Right);
	CUIRect PresetA, PresetB;
	Button.VSplitMid(&PresetA, &PresetB, 5.0f);
	static CButtonContainer s_PresetGameButton, s_PresetLightButton;
	if(DoButton_Menu(&s_PresetGameButton, DtLocalize("Only the game"), 0, &PresetA))
		CFocus::ApplyPreset(CFocus::PRESET_ONLY_GAME);
	GameClient()->m_Tooltips.DoToolTip(&s_PresetGameButton, &PresetA, DtLocalize("Ticks everything: only the map and your tee are left."));
	if(DoButton_Menu(&s_PresetLightButton, DtLocalize("Light"), 0, &PresetB))
		CFocus::ApplyPreset(CFocus::PRESET_LIGHT);
	GameClient()->m_Tooltips.DoToolTip(&s_PresetLightButton, &PresetB, DtLocalize("Only chat, names and effects."));

	Right.HSplitTop(Spacing, nullptr, &Right);
	struct SFocusOption
	{
		CFocus::EItem m_Item;
		const char *m_pName;
	};
	const SFocusOption aFocusOptions[] = {
		{CFocus::ITEM_OTHERS, DtLocalize("Other players")},
		{CFocus::ITEM_CHAT, DtLocalize("Chat")},
		{CFocus::ITEM_NAMES, DtLocalize("Names")},
		{CFocus::ITEM_EFFECTS, DtLocalize("Effects")},
		{CFocus::ITEM_HUD, DtLocalize("HUD")},
		{CFocus::ITEM_SCOREBOARD, DtLocalize("Scoreboard")},
		{CFocus::ITEM_UI, DtLocalize("Kill feed and popups")},
		{CFocus::ITEM_MUSIC, DtLocalize("Music player")},
		{CFocus::ITEM_SPLITS, DtLocalize("Splits box")},
		{CFocus::ITEM_GATES, DtLocalize("My checkpoints")},
		{CFocus::ITEM_BROADCAST, DtLocalize("Broadcast and votes")},
		{CFocus::ITEM_NOTICES, DtLocalize("DDNetty messages")},
	};
	static int s_aFocusCheckboxes[std::size(aFocusOptions)];
	const size_t NumColumns = 3;
	for(size_t i = 0; i < std::size(aFocusOptions); i += NumColumns)
	{
		CUIRect Line, Col;
		Right.HSplitTop(LineSize, &Line, &Right);
		const float ColumnWidth = Line.w / NumColumns;
		for(size_t j = i; j < std::min(i + NumColumns, std::size(aFocusOptions)); j++)
		{
			Line.VSplitLeft(ColumnWidth, &Col, &Line);
			int *pValue = CFocus::ItemConfig(aFocusOptions[j].m_Item);
			if(DoButton_CheckBox(&s_aFocusCheckboxes[j], aFocusOptions[j].m_pName, *pValue, &Col))
				*pValue ^= 1;
			if(aFocusOptions[j].m_Item == CFocus::ITEM_OTHERS)
				GameClient()->m_Tooltips.DoToolTip(&s_aFocusCheckboxes[j], &Col, DtLocalize("Their tees, hooks, shots and names. Your team, your dummy, the ghost and whoever hooked with you in the last 30 s stay."));
		}
	}
	Right.HSplitTop(Spacing, nullptr, &Right);
	Right.HSplitTop(LineSize, &Button, &Right);
	static int s_FocusNoticeCheckbox = 0;
	if(DoButton_CheckBox(&s_FocusNoticeCheckbox, DtLocalize("Show \"Focus ON/OFF\" when toggling"), g_Config.m_DtFocusNotice, &Button))
		g_Config.m_DtFocusNotice ^= 1;
}

// --- Splits ---

void CMenus::RenderDDNettySplits(CUIRect MainView)
{
	const float FontSize = 14.0f;
	const float LineSize = 20.0f;
	const float Spacing = 5.0f;
	CSplits &Splits = GameClient()->m_Splits;

	CUIRect Row, Label, Button;
	if(!GameClient()->m_GameInfo.m_Race || !Splits.HasMap())
	{
		MainView.HSplitTop(LineSize, &Label, &MainView);
		Ui()->DoLabel(&Label, DtLocalize("Splits only work on race maps."), FontSize, TEXTALIGN_ML);
		return;
	}

	// a row of buttons where only one can be active
	auto &&DoChoices = [&](CUIRect *pRow, CButtonContainer *pButtons, const char **ppLabels, int Num, int Current, float Width) {
		int Result = Current;
		for(int i = 0; i < Num; i++)
		{
			pRow->VSplitLeft(Width, &Button, pRow);
			pRow->VSplitLeft(3.0f, nullptr, pRow);
			if(DoButton_Menu(&pButtons[i], ppLabels[i], Current == i, &Button))
				Result = i;
		}
		return Result;
	};

	// split points first: it changes everything below
	MainView.HSplitTop(26.0f, &Row, &MainView);
	Row.VSplitLeft(130.0f, &Label, &Row);
	Ui()->DoLabel(&Label, DtLocalize("Split points:"), 16.0f, TEXTALIGN_ML);
	static CButtonContainer s_aSourceButtons[3];
	const char *apSources[] = {DtLocalize("Map + mine"), DtLocalize("Map time checkpoints"), DtLocalize("My checkpoints")};
	g_Config.m_DtSplitsSource = DoChoices(&Row, s_aSourceButtons, apSources, 3, g_Config.m_DtSplitsSource, 190.0f);
	MainView.HSplitTop(Spacing * 2.0f, nullptr, &MainView);

	const DdnettySplits::SAnalysis &Analysis = Splits.Analysis();

	// summary
	int PlayerBest = Splits.PlayerBestMillis();
	if(PlayerBest < 0)
		PlayerBest = Analysis.m_BestFinished;
	char aTarget[32], aBest[32], aBuilt[32], aGain[32], aLine[256];
	CSplits::FormatMillis(Splits.TargetMillis(), aTarget, sizeof(aTarget));
	CSplits::FormatMillis(PlayerBest, aBest, sizeof(aBest));
	CSplits::FormatMillis(Analysis.m_BuiltRun, aBuilt, sizeof(aBuilt));
	if(PlayerBest >= 0 && Analysis.m_BuiltRun >= 0 && PlayerBest > Analysis.m_BuiltRun)
		CSplits::FormatMillis(PlayerBest - Analysis.m_BuiltRun, aGain, sizeof(aGain));
	else
		str_copy(aGain, "—");
	str_format(aLine, sizeof(aLine), DtLocalize("Target: %s   Your best: %s   Ideal run: %s   Possible gain: %s"), aTarget, aBest, aBuilt, aGain);
	MainView.HSplitTop(LineSize, &Label, &MainView);
	Ui()->DoLabel(&Label, aLine, FontSize, TEXTALIGN_ML);
	if(Analysis.m_BuiltRun < 0)
	{
		MainView.HSplitTop(LineSize - 4.0f, &Label, &MainView);
		SLabelProperties HintProps;
		HintProps.SetColor(ColorRGBA(1.0f, 1.0f, 1.0f, 0.6f));
		Ui()->DoLabel(&Label, Analysis.m_RouteComplete ? DtLocalize("The ideal run needs at least one attempt of every segment.") : DtLocalize("The ideal run needs one complete run from start to finish."), 11.0f, TEXTALIGN_ML, HintProps);
	}
	MainView.HSplitTop(Spacing, nullptr, &MainView);

	// options
	MainView.HSplitTop(LineSize, &Row, &MainView);
	Row.VSplitLeft(115.0f, &Label, &Row);
	Ui()->DoLabel(&Label, DtLocalize("Compare with:"), FontSize, TEXTALIGN_ML);
	CUIRect EditButton;
	Row.VSplitRight(170.0f, &Row, &EditButton);
	static CButtonContainer s_aBaseButtons[2];
	const char *apBases[] = {DtLocalize("Best"), DtLocalize("Average"), nullptr};
	g_Config.m_DtSplitsAverage = DoChoices(&Row, s_aBaseButtons, apBases, 2, g_Config.m_DtSplitsAverage, 90.0f);
	static CButtonContainer s_EditButton;
	if(DoButton_Menu(&s_EditButton, DtLocalize("Edit checkpoints"), Splits.EditMode(), &EditButton))
	{
		Splits.SetEditMode(true);
		SetActive(false);
	}
	GameClient()->m_Tooltips.DoToolTip(&s_EditButton, &EditButton, DtLocalize("Closes the menu. Drag with the mouse to draw a checkpoint across the path, right click deletes, Esc exits."));

	MainView.HSplitTop(Spacing, nullptr, &MainView);
	MainView.HSplitTop(LineSize, &Row, &MainView);
	static int s_RecordCheckbox, s_HudCheckbox, s_PracticeCheckbox, s_GatesCheckbox;
	Row.VSplitLeft(150.0f, &Button, &Row);
	if(DoButton_CheckBox(&s_RecordCheckbox, DtLocalize("Record splits"), g_Config.m_DtSplits, &Button))
		g_Config.m_DtSplits ^= 1;
	Row.VSplitLeft(150.0f, &Button, &Row);
	if(DoButton_CheckBox(&s_HudCheckbox, DtLocalize("Show on HUD"), g_Config.m_DtSplitsHud, &Button))
		g_Config.m_DtSplitsHud ^= 1;
	Row.VSplitLeft(150.0f, &Button, &Row);
	if(DoButton_CheckBox(&s_PracticeCheckbox, DtLocalize("Count practice"), g_Config.m_DtSplitsPractice, &Button))
		g_Config.m_DtSplitsPractice ^= 1;
	Row.VSplitLeft(280.0f, &Button, &Row);
	if(DoButton_CheckBox(&s_GatesCheckbox, DtLocalize("Show my checkpoints on the map"), g_Config.m_DtSplitsShowGates, &Button))
		g_Config.m_DtSplitsShowGates ^= 1;

	// demos of the best segments (and the ideal run built from them)
	MainView.HSplitTop(Spacing, nullptr, &MainView);
	MainView.HSplitTop(LineSize, &Row, &MainView);
	static int s_SegDemosCheckbox, s_SegDemosMapCheckbox, s_IdealDemoCheckbox;
	Row.VSplitLeft(260.0f, &Button, &Row);
	if(DoButton_CheckBox(&s_SegDemosCheckbox, DtLocalize("Save demos of the best segments"), g_Config.m_DtSplitsSegmentDemos, &Button))
	{
		g_Config.m_DtSplitsSegmentDemos ^= 1;
		Client()->DemoRecorder_UpdateReplayRecorder();
	}
	GameClient()->m_Tooltips.DoToolTip(&s_SegDemosCheckbox, &Button, DtLocalize("Every new best segment is cut out of the replay recorder into demos/segments when the run ends. Has to be turned on for each map too."));
	Row.VSplitLeft(130.0f, &Button, &Row);
	if(DoButton_CheckBox(&s_SegDemosMapCheckbox, DtLocalize("On this map"), Splits.SegmentDemosOnMap(), &Button))
		Splits.SetSegmentDemosOnMap(!Splits.SegmentDemosOnMap());
	Row.VSplitLeft(200.0f, &Button, &Row);
	if(DoButton_CheckBox(&s_IdealDemoCheckbox, DtLocalize("Build the ideal run demo"), g_Config.m_DtSplitsIdealDemo, &Button))
		g_Config.m_DtSplitsIdealDemo ^= 1;
	GameClient()->m_Tooltips.DoToolTip(&s_IdealDemoCheckbox, &Button, DtLocalize("When every segment of the route has a demo, they are stitched into one demo of your ideal run (with your team mate), in demos/ideal."));
	Row.VSplitLeft(5.0f, nullptr, &Row);
	Row.VSplitLeft(110.0f, &Button, &Row);
	static CButtonContainer s_SegFolderButton;
	if(DoButton_Menu(&s_SegFolderButton, DtLocalize("Open folder"), 0, &Button))
	{
		Storage()->CreateFolder("demos", IStorage::TYPE_SAVE);
		Storage()->CreateFolder("demos/segments", IStorage::TYPE_SAVE);
		Storage()->CreateFolder(Splits.SegmentFolder(), IStorage::TYPE_SAVE);
		char aFolder[IO_MAX_PATH_LENGTH];
		Storage()->GetCompletePath(IStorage::TYPE_SAVE, Splits.SegmentFolder(), aFolder, sizeof(aFolder));
		Client()->ViewFile(aFolder);
	}
	Row.VSplitLeft(5.0f, nullptr, &Row);
	Row.VSplitLeft(110.0f, &Button, &Row);
	static CButtonContainer s_BuildIdealButton;
	if(DoButton_Menu(&s_BuildIdealButton, DtLocalize("Build now"), Splits.IdealDemoBuilding(), &Button))
		Splits.RequestIdealDemo(true);
	if(Splits.IdealDemoStatus()[0] != '\0' && Splits.PendingSegmentDemos() == 0)
	{
		Row.VSplitLeft(8.0f, nullptr, &Row);
		SLabelProperties StatusProps;
		StatusProps.SetColor(ColorRGBA(1.0f, 1.0f, 1.0f, 0.7f));
		StatusProps.m_MaxWidth = Row.w;
		StatusProps.m_EllipsisAtEnd = true;
		Ui()->DoLabel(&Row, Splits.IdealDemoStatus(), 12.0f, TEXTALIGN_ML, StatusProps);
	}
	if(Splits.PendingSegmentDemos() > 0)
	{
		char aPending[64];
		str_format(aPending, sizeof(aPending), DtLocalize("%d segment demos waiting"), Splits.PendingSegmentDemos());
		Row.VSplitLeft(8.0f, nullptr, &Row);
		Ui()->DoLabel(&Row, aPending, 12.0f, TEXTALIGN_ML);
	}

	// focused runs: count from a moment on, and the average only from runs close to your best
	MainView.HSplitTop(Spacing, nullptr, &MainView);
	MainView.HSplitTop(LineSize, &Row, &MainView);
	Row.VSplitLeft(115.0f, &Label, &Row);
	Ui()->DoLabel(&Label, DtLocalize("Focused runs:"), FontSize, TEXTALIGN_ML);
	static CButtonContainer s_SinceButton, s_SinceClearButton;
	Row.VSplitLeft(140.0f, &Button, &Row);
	if(DoButton_Menu(&s_SinceButton, DtLocalize("Count from now"), 0, &Button))
		Splits.SetSince(time_timestamp());
	GameClient()->m_Tooltips.DoToolTip(&s_SinceButton, &Button, DtLocalize("Stats, best segments and the ideal run only use the times from now on. Nothing is deleted."));
	if(Splits.Since() > 0)
	{
		Row.VSplitLeft(5.0f, nullptr, &Row);
		Row.VSplitLeft(100.0f, &Button, &Row);
		if(DoButton_Menu(&s_SinceClearButton, DtLocalize("Show all"), 0, &Button))
			Splits.SetSince(0);
		char aDate[64], aSince[96];
		str_timestamp_ex((time_t)Splits.Since(), aDate, sizeof(aDate), "%d/%m %H:%M");
		str_format(aSince, sizeof(aSince), DtLocalize("since %s"), aDate);
		Row.VSplitLeft(8.0f, nullptr, &Row);
		Row.VSplitLeft(110.0f, &Label, &Row);
		Ui()->DoLabel(&Label, aSince, 12.0f, TEXTALIGN_ML);
	}
	{
		// the margin is chosen per map, in seconds; empty uses the default
		Row.VSplitLeft(15.0f, nullptr, &Row);
		Row.VSplitLeft(150.0f, &Label, &Row);
		Ui()->DoLabel(&Label, DtLocalize("Behind my best by at most"), 12.0f, TEXTALIGN_ML);
		static CLineInputBuffered<12> s_MarginInput;
		static int s_MarginShown = -2;
		if(s_MarginShown != Splits.FocusMarginOnMap() && !s_MarginInput.IsActive())
		{
			char aSeconds[16] = "";
			if(Splits.FocusMarginOnMap() >= 0)
				str_format(aSeconds, sizeof(aSeconds), "%g", Splits.FocusMarginOnMap() / 1000.0);
			s_MarginInput.Set(aSeconds);
			s_MarginShown = Splits.FocusMarginOnMap();
		}
		char aDefault[32];
		str_format(aDefault, sizeof(aDefault), "%g", g_Config.m_DtSplitsFocusMargin / 1000.0);
		s_MarginInput.SetEmptyText(aDefault);
		Row.VSplitLeft(50.0f, &Button, &Row);
		if(Ui()->DoEditBox(&s_MarginInput, &Button, FontSize))
		{
			if(s_MarginInput.IsEmpty())
			{
				Splits.SetFocusMarginOnMap(-1);
			}
			else
			{
				char *pEnd = nullptr;
				const double Seconds = std::strtod(s_MarginInput.GetString(), &pEnd);
				if(pEnd != s_MarginInput.GetString() && *pEnd == '\0' && Seconds >= 0.0 && Seconds <= 600.0)
					Splits.SetFocusMarginOnMap((int)std::lround(Seconds * 1000.0));
			}
			s_MarginShown = Splits.FocusMarginOnMap();
		}
		GameClient()->m_Tooltips.DoToolTip(&s_MarginInput, &Button, DtLocalize("Seconds, on this map. The average and the variation only count segments you entered at most this far behind your best run. 0 turns it off, empty uses the default."));
		Row.VSplitLeft(4.0f, nullptr, &Row);
		Row.VSplitLeft(14.0f, &Label, &Row);
		Ui()->DoLabel(&Label, "s", 12.0f, TEXTALIGN_ML);
	}

	MainView.HSplitTop(Spacing, nullptr, &MainView);
	MainView.HSplitTop(LineSize, &Row, &MainView);
	Row.VSplitLeft(115.0f, nullptr, &Row);
	static int s_RunEntriesCheckbox, s_CountDummyCheckbox, s_PbNoticeCheckbox, s_PbSoundCheckbox;
	Row.VSplitLeft(200.0f, &Button, &Row);
	if(DoButton_CheckBox(&s_RunEntriesCheckbox, DtLocalize("Best only from real runs"), g_Config.m_DtSplitsRunEntriesOnly, &Button))
		g_Config.m_DtSplitsRunEntriesOnly ^= 1;
	GameClient()->m_Tooltips.DoToolTip(&s_RunEntriesCheckbox, &Button, DtLocalize("Best segments and the ideal run leave out segments entered after starting in the middle, going back, waiting or crossing the checkpoint again."));
	Row.VSplitLeft(150.0f, &Button, &Row);
	if(DoButton_CheckBox(&s_CountDummyCheckbox, DtLocalize("Count dummy"), g_Config.m_DtSplitsCountDummy, &Button))
		g_Config.m_DtSplitsCountDummy ^= 1;
	Row.VSplitLeft(180.0f, &Button, &Row);
	if(DoButton_CheckBox(&s_PbNoticeCheckbox, DtLocalize("New best message"), g_Config.m_DtSplitsPbNotice, &Button))
		g_Config.m_DtSplitsPbNotice = g_Config.m_DtSplitsPbNotice ? 0 : 2;
	if(g_Config.m_DtSplitsPbNotice)
	{
		Row.VSplitLeft(100.0f, &Button, &Row);
		if(DoButton_CheckBox(&s_PbSoundCheckbox, DtLocalize("Sound"), g_Config.m_DtSplitsPbSound, &Button))
			g_Config.m_DtSplitsPbSound ^= 1;
	}

	// target time for the prediction
	MainView.HSplitTop(Spacing, nullptr, &MainView);
	MainView.HSplitTop(LineSize, &Row, &MainView);
	Row.VSplitLeft(115.0f, &Label, &Row);
	Ui()->DoLabel(&Label, DtLocalize("Target time:"), FontSize, TEXTALIGN_ML);
	static CButtonContainer s_aTargetButtons[4];
	const char *apTargets[] = {DtLocalize("Server record"), DtLocalize("My best"), DtLocalize("Custom"), DtLocalize("Top player")};
	g_Config.m_DtSplitsTargetMode = DoChoices(&Row, s_aTargetButtons, apTargets, 4, g_Config.m_DtSplitsTargetMode, 110.0f);
	if(g_Config.m_DtSplitsTargetMode == 3)
	{
		// pick one of the top players of the reference file
		const auto &vRuns = Splits.Refs().m_vRuns;
		const DdnettySplits::SRefRun *pRun = Splits.RefRun();
		Row.VSplitLeft(5.0f, nullptr, &Row);
		static CButtonContainer s_PrevRefButton, s_NextRefButton, s_ReloadRefsButton, s_RefsFolderButton;
		if(vRuns.empty())
		{
			Row.VSplitLeft(110.0f, &Button, &Row);
			if(DoButton_Menu(&s_RefsFolderButton, DtLocalize("Open folder"), 0, &Button))
			{
				Storage()->CreateFolder("ddnetty/splits/refs", IStorage::TYPE_SAVE);
				char aFolder[IO_MAX_PATH_LENGTH];
				Storage()->GetCompletePath(IStorage::TYPE_SAVE, "ddnetty/splits/refs", aFolder, sizeof(aFolder));
				Client()->ViewFile(aFolder);
			}
			Row.VSplitLeft(5.0f, nullptr, &Row);
			Row.VSplitLeft(80.0f, &Button, &Row);
			if(DoButton_Menu(&s_ReloadRefsButton, DtLocalize("Reload"), 0, &Button))
				Splits.LoadRefs();
			Row.VSplitLeft(8.0f, nullptr, &Row);
			char aHint[IO_MAX_PATH_LENGTH + 64];
			str_format(aHint, sizeof(aHint), DtLocalize("No reference file: %s"), Splits.RefsPath());
			SLabelProperties HintProps;
			HintProps.SetColor(ColorRGBA(1.0f, 1.0f, 1.0f, 0.6f));
			HintProps.m_MaxWidth = Row.w;
			HintProps.m_EllipsisAtEnd = true;
			Ui()->DoLabel(&Row, aHint, 12.0f, TEXTALIGN_ML, HintProps);
		}
		else
		{
			int Index = 0;
			for(size_t i = 0; i < vRuns.size(); i++)
				if(&vRuns[i] == pRun)
					Index = (int)i;
			Row.VSplitLeft(20.0f, &Button, &Row);
			if(DoButton_Menu(&s_PrevRefButton, "<", 0, &Button))
				str_copy(g_Config.m_DtSplitsRefPlayer, vRuns[(Index + vRuns.size() - 1) % vRuns.size()].m_Name.c_str());
			char aTime[32], aLabel[96];
			CSplits::FormatMillis(pRun->m_Millis, aTime, sizeof(aTime));
			str_format(aLabel, sizeof(aLabel), "%d. %s  %s", Index + 1, pRun->m_Name.c_str(), aTime);
			Row.VSplitLeft(190.0f, &Button, &Row);
			Ui()->DoLabel(&Button, aLabel, FontSize, TEXTALIGN_MC);
			Row.VSplitLeft(20.0f, &Button, &Row);
			if(DoButton_Menu(&s_NextRefButton, ">", 0, &Button))
				str_copy(g_Config.m_DtSplitsRefPlayer, vRuns[(Index + 1) % vRuns.size()].m_Name.c_str());
			Row.VSplitLeft(5.0f, nullptr, &Row);
			Row.VSplitLeft(80.0f, &Button, &Row);
			if(DoButton_Menu(&s_ReloadRefsButton, DtLocalize("Reload"), 0, &Button))
				Splits.LoadRefs();
		}
	}
	if(g_Config.m_DtSplitsTargetMode == 2)
	{
		// custom time for this map, saved with its splits
		static CLineInputBuffered<16> s_TargetInput;
		static int s_TargetShown = -2;
		if(s_TargetShown != Splits.CustomTarget() && !s_TargetInput.IsActive())
		{
			char aTime[32] = "";
			if(Splits.CustomTarget() > 0)
				CSplits::FormatMillis(Splits.CustomTarget(), aTime, sizeof(aTime));
			s_TargetInput.Set(aTime);
			s_TargetShown = Splits.CustomTarget();
		}
		Row.VSplitLeft(5.0f, nullptr, &Row);
		Row.VSplitLeft(90.0f, &Button, &Row);
		s_TargetInput.SetEmptyText("9:41.02");
		if(Ui()->DoEditBox(&s_TargetInput, &Button, FontSize))
		{
			const int Millis = CSplits::ParseTime(s_TargetInput.GetString());
			if(Millis > 0 || s_TargetInput.IsEmpty())
			{
				Splits.SetCustomTarget(Millis);
				s_TargetShown = Splits.CustomTarget();
			}
		}
	}

	// message when the prediction is slower than the target
	MainView.HSplitTop(Spacing, nullptr, &MainView);
	MainView.HSplitTop(LineSize, &Row, &MainView);
	static int s_WarningCheckbox;
	Row.VSplitLeft(200.0f, &Button, &Row);
	if(DoButton_CheckBox(&s_WarningCheckbox, DtLocalize("Warn when slower than target"), g_Config.m_DtSplitsWarning, &Button))
		g_Config.m_DtSplitsWarning ^= 1;
	if(g_Config.m_DtSplitsWarning)
	{
		static CLineInput s_MessageInput(g_Config.m_DtSplitsMessage, sizeof(g_Config.m_DtSplitsMessage));
		s_MessageInput.SetEmptyText(DtLocalize("Rank 1 unlikely."));
		Row.VSplitLeft(5.0f, nullptr, &Row);
		Row.VSplitLeft(250.0f, &Button, &Row);
		Ui()->DoEditBox(&s_MessageInput, &Button, FontSize);
	}
	Row.VSplitLeft(20.0f, nullptr, &Row);
	Row.VSplitLeft(260.0f, &Button, &Row);
	static int s_IdealCheckbox;
	if(DoButton_CheckBox(&s_IdealCheckbox, DtLocalize("Ideal run on the HUD"), g_Config.m_DtSplitsHudIdeal, &Button))
		g_Config.m_DtSplitsHudIdeal ^= 1;

	// views
	MainView.HSplitTop(Spacing * 2.0f, nullptr, &MainView);
	MainView.HSplitTop(LineSize, &Row, &MainView);
	static CButtonContainer s_aViewButtons[NUM_SPLITS_VIEWS];
	const char *apViews[] = {DtLocalize("Segments"), DtLocalize("Runs"), DtLocalize("Checkpoints"), DtLocalize("Analysis")};
	m_SplitsView = DoChoices(&Row, s_aViewButtons, apViews, NUM_SPLITS_VIEWS, m_SplitsView, 130.0f);
	MainView.HSplitTop(Spacing, nullptr, &MainView);

	if(m_SplitsView == SPLITS_VIEW_SEGMENTS)
		RenderSplitsSegments(MainView);
	else if(m_SplitsView == SPLITS_VIEW_RUNS)
		RenderSplitsRuns(MainView);
	else if(m_SplitsView == SPLITS_VIEW_GATES)
		RenderSplitsGates(MainView);
	else
		RenderSplitsAnalysis(MainView);
}

void CMenus::RenderSplitsSegments(CUIRect View)
{
	CSplits &Splits = GameClient()->m_Splits;
	const DdnettySplits::SAnalysis &Analysis = Splits.Analysis();
	const DdnettySplits::SOptions Options = Splits.Options();
	const float FontSize = 12.0f;
	const float RowHeight = 18.0f;

	std::vector<const DdnettySplits::SSegmentStats *> vpRows;
	for(const auto &Stats : Analysis.m_vRouteStats)
		vpRows.push_back(&Stats);
	const size_t NumRoute = vpRows.size();
	for(const auto &Stats : Analysis.m_vOtherStats)
		vpRows.push_back(&Stats);

	CUIRect Label;
	if(Analysis.m_vAttempts.empty())
	{
		View.HSplitTop(RowHeight, &Label, &View);
		Ui()->DoLabel(&Label, DtLocalize("No splits recorded on this map yet. Play through a time checkpoint or one of your checkpoints."), FontSize, TEXTALIGN_ML);
		return;
	}

	// columns: segment (flexible), best, average, variation, last, attempts, best of everyone else, clear button
	// only when the reference file has some of these segments (it only knows the map's time checkpoints)
	bool HasRefs = false;
	for(const auto *pStats : vpRows)
		HasRefs = HasRefs || Splits.Refs().FindSegment(pStats->m_From, pStats->m_To) != nullptr;
	const float aWidths[] = {80.0f, 80.0f, 80.0f, 70.0f, 90.0f, HasRefs ? 130.0f : 0.0f, 70.0f};
	auto &&SplitColumns = [&](CUIRect Row, CUIRect *pCols) {
		float Fixed = 0.0f;
		for(float Width : aWidths)
			Fixed += Width;
		Row.VSplitLeft(std::max(Row.w - Fixed, 50.0f), &pCols[0], &Row);
		for(int i = 0; i < (int)std::size(aWidths); i++)
			Row.VSplitLeft(aWidths[i], &pCols[i + 1], &Row);
	};

	CUIRect Headers, aCols[8];
	View.HSplitTop(RowHeight, &Headers, &View);
	Headers.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.25f), IGraphics::CORNER_T, 5.0f);
	Headers.VSplitLeft(5.0f, nullptr, &Headers);
	Headers.VSplitRight(20.0f, &Headers, nullptr); // scrollbar
	SplitColumns(Headers, aCols);
	const char *apCaptions[] = {DtLocalize("Segment"), DtLocalize("Best"), DtLocalize("Average"), DtLocalize("Variation"), DtLocalize("Last"), DtLocalize("Attempts"), DtLocalize("Best of all")};
	for(int i = 0; i < 7; i++)
		if(i < 6 || HasRefs)
			Ui()->DoLabel(&aCols[i], apCaptions[i], FontSize, TEXTALIGN_ML);
	static int s_AttemptsTooltip, s_BestOfAllTooltip;
	GameClient()->m_Tooltips.DoToolTip(&s_AttemptsTooltip, &aCols[5], DtLocalize("All attempts (focused ones in brackets: the average and the variation use those)"));
	if(HasRefs)
		GameClient()->m_Tooltips.DoToolTip(&s_BestOfAllTooltip, &aCols[6], DtLocalize("Fastest time of anyone else in this segment, from the map's reference file. Gold: yours is faster."));

	static CScrollRegion s_ScrollRegion;
	CScrollRegionParams ScrollParams;
	ScrollParams.m_ScrollUnit = RowHeight * 3.0f;
	CUIRect Content = View;
	s_ScrollRegion.Begin(&Content, &ScrollParams);

	static std::vector<CButtonContainer> s_vClearButtons;
	if(s_vClearButtons.size() != vpRows.size())
		s_vClearButtons.resize(vpRows.size());

	for(size_t i = 0; i < vpRows.size(); i++)
	{
		CUIRect Row;
		if(i == NumRoute)
		{
			// segments that aren't on the route (other paths, practice parts, ...)
			Content.HSplitTop(RowHeight + 4.0f, &Row, &Content);
			if(s_ScrollRegion.AddRect(Row))
			{
				SLabelProperties Props;
				Props.SetColor(ColorRGBA(1.0f, 1.0f, 1.0f, 0.6f));
				Row.VSplitLeft(5.0f, nullptr, &Row);
				Ui()->DoLabel(&Row, DtLocalize("Other segments (not on your route)"), FontSize, TEXTALIGN_ML, Props);
			}
		}

		Content.HSplitTop(RowHeight, &Row, &Content);
		if(!s_ScrollRegion.AddRect(Row))
			continue;
		if(i % 2 == 0)
			Row.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.05f), IGraphics::CORNER_NONE, 0.0f);
		Row.VSplitLeft(5.0f, nullptr, &Row);
		SplitColumns(Row, aCols);

		const DdnettySplits::SSegmentStats &Stats = *vpRows[i];
		const std::string Name = Splits.PointName(Stats.m_From) + " → " + Splits.PointName(Stats.m_To);
		char aBest[32], aMean[32], aStdDev[40], aLast[32], aCount[16];
		CSplits::FormatMillis(Stats.m_Best, aBest, sizeof(aBest));
		CSplits::FormatMillis(Stats.m_Mean, aMean, sizeof(aMean));
		if(Stats.m_StdDev >= 0)
		{
			char aValue[32];
			CSplits::FormatMillis(Stats.m_StdDev, aValue, sizeof(aValue));
			str_format(aStdDev, sizeof(aStdDev), "±%s", aValue);
		}
		else
			str_copy(aStdDev, "—");
		CSplits::FormatMillis(Stats.m_Last, aLast, sizeof(aLast));
		if(Stats.m_Focused != Stats.m_Count)
			str_format(aCount, sizeof(aCount), "%d (%d)", Stats.m_Count, Stats.m_Focused);
		else
			str_format(aCount, sizeof(aCount), "%d", Stats.m_Count);

		SLabelProperties NameProps;
		NameProps.m_MaxWidth = aCols[0].w - 5.0f;
		NameProps.m_EllipsisAtEnd = true;
		Ui()->DoLabel(&aCols[0], Name.c_str(), FontSize, TEXTALIGN_ML, NameProps);
		Ui()->DoLabel(&aCols[1], aBest, FontSize, TEXTALIGN_ML);
		Ui()->DoLabel(&aCols[2], aMean, FontSize, TEXTALIGN_ML);
		Ui()->DoLabel(&aCols[3], aStdDev, FontSize, TEXTALIGN_ML);
		Ui()->DoLabel(&aCols[4], aLast, FontSize, TEXTALIGN_ML);
		Ui()->DoLabel(&aCols[5], aCount, FontSize, TEXTALIGN_ML);
		if(const DdnettySplits::SRefSegment *pOther = HasRefs ? Splits.Refs().FindSegment(Stats.m_From, Stats.m_To) : nullptr)
		{
			char aOther[32], aCell[96];
			CSplits::FormatMillis(pOther->m_Millis, aOther, sizeof(aOther));
			str_format(aCell, sizeof(aCell), "%s %s", aOther, pOther->m_Name.c_str());
			SLabelProperties OtherProps;
			OtherProps.m_MaxWidth = aCols[6].w - 5.0f;
			OtherProps.m_EllipsisAtEnd = true;
			if(Stats.m_Best >= 0 && Stats.m_Best < pOther->m_Millis)
				OtherProps.SetColor(ColorRGBA(1.0f, 0.82f, 0.25f, 1.0f));
			Ui()->DoLabel(&aCols[6], aCell, FontSize, TEXTALIGN_ML, OtherProps);
		}

		if(Stats.m_Count > 0)
		{
			CUIRect ClearButton;
			aCols[7].Margin(1.0f, &ClearButton);
			if(DoButton_Menu(&s_vClearButtons[i], DtLocalize("Clear"), 0, &ClearButton))
			{
				m_SplitsPendingDelete = SPLITS_DELETE_SEGMENT;
				m_SplitsPendingFrom = Stats.m_From;
				m_SplitsPendingTo = Stats.m_To;
				char aMessage[256];
				str_format(aMessage, sizeof(aMessage), DtLocalize("Delete all %d attempts of %s?"), Stats.m_Count, Name.c_str());
				PopupConfirm(DtLocalize("Clear segment"), aMessage, Localize("Yes"), Localize("No"), &CMenus::PopupConfirmSplitsDelete);
			}
		}
	}
	s_ScrollRegion.End();
	(void)Options;
}

void CMenus::RenderSplitsRuns(CUIRect View)
{
	CSplits &Splits = GameClient()->m_Splits;
	const float FontSize = 12.0f;
	const float RowHeight = 18.0f;
	const float LineSize = 20.0f;

	// filter and sort
	std::vector<DdnettySplits::SRun> vRuns = Splits.Data().Runs();
	const int64_t Now = time_timestamp();
	const int64_t aPeriods[] = {24 * 60 * 60, 7 * 24 * 60 * 60, 30 * 24 * 60 * 60, -1};
	const int64_t Period = aPeriods[std::clamp(m_SplitsRunsPeriod, 0, 3)];
	vRuns.erase(std::remove_if(vRuns.begin(), vRuns.end(), [&](const DdnettySplits::SRun &Run) {
		if(Period > 0 && Now - Run.m_Timestamp > Period)
			return true;
		if(m_SplitsRunsType == 1 && !Run.Finished())
			return true;
		if(m_SplitsRunsType == 2 && !Run.m_Practice)
			return true;
		if(m_SplitsRunsType == 3 && Run.m_Practice)
			return true;
		return false;
	}),
		vRuns.end());
	if(m_SplitsRunsSort == 1)
		std::stable_sort(vRuns.begin(), vRuns.end(), [](const DdnettySplits::SRun &A, const DdnettySplits::SRun &B) { return A.m_Millis < B.m_Millis; });
	else
		std::stable_sort(vRuns.begin(), vRuns.end(), [](const DdnettySplits::SRun &A, const DdnettySplits::SRun &B) { return A.m_Timestamp > B.m_Timestamp; });

	CUIRect Row, Label, Button;
	auto &&DoChoices = [&](CUIRect *pRow, CButtonContainer *pButtons, const char **ppLabels, int Num, int Current, float Width) {
		int Result = Current;
		for(int i = 0; i < Num; i++)
		{
			pRow->VSplitLeft(Width, &Button, pRow);
			pRow->VSplitLeft(3.0f, nullptr, pRow);
			if(DoButton_Menu(&pButtons[i], ppLabels[i], Current == i, &Button))
				Result = i;
		}
		return Result;
	};

	// controls
	View.HSplitTop(LineSize, &Row, &View);
	Row.VSplitLeft(70.0f, &Label, &Row);
	Ui()->DoLabel(&Label, DtLocalize("Sort:"), FontSize, TEXTALIGN_ML);
	static CButtonContainer s_aSortButtons[2];
	const char *apSorts[] = {DtLocalize("Most recent"), DtLocalize("Fastest")};
	m_SplitsRunsSort = DoChoices(&Row, s_aSortButtons, apSorts, 2, m_SplitsRunsSort, 110.0f);
	Row.VSplitLeft(20.0f, nullptr, &Row);
	Row.VSplitLeft(70.0f, &Label, &Row);
	Ui()->DoLabel(&Label, DtLocalize("Period:"), FontSize, TEXTALIGN_ML);
	static CButtonContainer s_aPeriodButtons[4];
	const char *apPeriods[] = {DtLocalize("24 hours"), DtLocalize("7 days"), DtLocalize("30 days"), DtLocalize("All")};
	m_SplitsRunsPeriod = DoChoices(&Row, s_aPeriodButtons, apPeriods, 4, m_SplitsRunsPeriod, 80.0f);

	View.HSplitTop(5.0f, nullptr, &View);
	View.HSplitTop(LineSize, &Row, &View);
	Row.VSplitLeft(70.0f, &Label, &Row);
	Ui()->DoLabel(&Label, DtLocalize("Type:"), FontSize, TEXTALIGN_ML);
	static CButtonContainer s_aTypeButtons[4];
	const char *apTypes[] = {DtLocalize("All"), DtLocalize("Finished"), DtLocalize("Practice"), DtLocalize("Real")};
	m_SplitsRunsType = DoChoices(&Row, s_aTypeButtons, apTypes, 4, m_SplitsRunsType, 110.0f);
	View.HSplitTop(5.0f, nullptr, &View);

	// selection only counts the runs that are visible right now
	std::vector<int> vVisibleSelected;
	for(const auto &Run : vRuns)
		if(std::find(m_vSplitsSelectedRuns.begin(), m_vSplitsSelectedRuns.end(), Run.m_RunId) != m_vSplitsSelectedRuns.end())
			vVisibleSelected.push_back(Run.m_RunId);

	// footer
	CUIRect Footer;
	View.HSplitBottom(LineSize, &View, &Footer);
	View.HSplitBottom(5.0f, &View, nullptr);
	char aBuf[128];
	static CButtonContainer s_SelectAllButton, s_DeleteSelectedButton, s_DeleteFilteredButton;
	Footer.VSplitLeft(150.0f, &Button, &Footer);
	const bool AllSelected = !vRuns.empty() && vVisibleSelected.size() == vRuns.size();
	if(DoButton_Menu(&s_SelectAllButton, AllSelected ? DtLocalize("Select none") : DtLocalize("Select all"), 0, &Button))
	{
		m_vSplitsSelectedRuns.clear();
		if(!AllSelected)
			for(const auto &Run : vRuns)
				m_vSplitsSelectedRuns.push_back(Run.m_RunId);
	}
	Footer.VSplitLeft(5.0f, nullptr, &Footer);
	Footer.VSplitLeft(200.0f, &Button, &Footer);
	str_format(aBuf, sizeof(aBuf), DtLocalize("Delete selected (%d)"), (int)vVisibleSelected.size());
	if(DoButton_Menu(&s_DeleteSelectedButton, aBuf, 0, &Button) && !vVisibleSelected.empty())
	{
		m_SplitsPendingDelete = SPLITS_DELETE_RUNS;
		m_vSplitsPendingRuns = vVisibleSelected;
		char aMessage[256];
		str_format(aMessage, sizeof(aMessage), DtLocalize("Delete %d runs? This can't be undone."), (int)vVisibleSelected.size());
		PopupConfirm(DtLocalize("Delete runs"), aMessage, Localize("Yes"), Localize("No"), &CMenus::PopupConfirmSplitsDelete);
	}
	Footer.VSplitLeft(5.0f, nullptr, &Footer);
	Footer.VSplitLeft(220.0f, &Button, &Footer);
	str_format(aBuf, sizeof(aBuf), DtLocalize("Delete all filtered (%d)"), (int)vRuns.size());
	if(DoButton_Menu(&s_DeleteFilteredButton, aBuf, 0, &Button) && !vRuns.empty())
	{
		m_SplitsPendingDelete = SPLITS_DELETE_RUNS;
		m_vSplitsPendingRuns.clear();
		for(const auto &Run : vRuns)
			m_vSplitsPendingRuns.push_back(Run.m_RunId);
		char aMessage[256];
		str_format(aMessage, sizeof(aMessage), DtLocalize("Delete %d runs? This can't be undone."), (int)vRuns.size());
		PopupConfirm(DtLocalize("Delete runs"), aMessage, Localize("Yes"), Localize("No"), &CMenus::PopupConfirmSplitsDelete);
	}

	if(vRuns.empty())
	{
		View.HSplitTop(RowHeight, &Label, &View);
		Ui()->DoLabel(&Label, DtLocalize("No runs match these filters."), FontSize, TEXTALIGN_ML);
		return;
	}

	// columns: checkbox, date, segment (flexible), time, type
	auto &&SplitColumns = [&](CUIRect Line, CUIRect *pCols) {
		Line.VSplitLeft(25.0f, &pCols[0], &Line);
		Line.VSplitLeft(110.0f, &pCols[1], &Line);
		Line.VSplitRight(150.0f, &Line, &pCols[4]);
		Line.VSplitRight(90.0f, &Line, &pCols[3]);
		pCols[2] = Line;
	};

	CUIRect Headers, aCols[5];
	View.HSplitTop(RowHeight, &Headers, &View);
	Headers.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.25f), IGraphics::CORNER_T, 5.0f);
	Headers.VSplitLeft(5.0f, nullptr, &Headers);
	Headers.VSplitRight(20.0f, &Headers, nullptr);
	SplitColumns(Headers, aCols);
	Ui()->DoLabel(&aCols[1], DtLocalize("Date"), FontSize, TEXTALIGN_ML);
	Ui()->DoLabel(&aCols[2], DtLocalize("Segment"), FontSize, TEXTALIGN_ML);
	Ui()->DoLabel(&aCols[3], DtLocalize("Time"), FontSize, TEXTALIGN_ML);
	Ui()->DoLabel(&aCols[4], DtLocalize("Type"), FontSize, TEXTALIGN_ML);

	static CScrollRegion s_ScrollRegion;
	CScrollRegionParams ScrollParams;
	ScrollParams.m_ScrollUnit = RowHeight * 3.0f;
	CUIRect Content = View;
	s_ScrollRegion.Begin(&Content, &ScrollParams);

	static std::vector<char> s_vCheckboxIds;
	if(s_vCheckboxIds.size() != vRuns.size())
		s_vCheckboxIds.resize(vRuns.size());

	for(size_t i = 0; i < vRuns.size(); i++)
	{
		const DdnettySplits::SRun &Run = vRuns[i];
		CUIRect Line;
		Content.HSplitTop(RowHeight, &Line, &Content);
		if(!s_ScrollRegion.AddRect(Line))
			continue;
		if(i % 2 == 0)
			Line.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.05f), IGraphics::CORNER_NONE, 0.0f);
		Line.VSplitLeft(5.0f, nullptr, &Line);
		SplitColumns(Line, aCols);

		const auto It = std::find(m_vSplitsSelectedRuns.begin(), m_vSplitsSelectedRuns.end(), Run.m_RunId);
		const bool Selected = It != m_vSplitsSelectedRuns.end();
		if(DoButton_CheckBox(&s_vCheckboxIds[i], "", Selected, &aCols[0]))
		{
			if(Selected)
				m_vSplitsSelectedRuns.erase(It);
			else
				m_vSplitsSelectedRuns.push_back(Run.m_RunId);
		}

		char aDate[32], aTime[32], aType[64];
		str_timestamp_ex((time_t)Run.m_Timestamp, aDate, sizeof(aDate), "%d/%m %H:%M");
		CSplits::FormatMillis(Run.m_Millis, aTime, sizeof(aTime));
		if(Run.Finished())
			str_format(aType, sizeof(aType), "%s · %s", Run.m_Practice ? DtLocalize("practice") : DtLocalize("real"), DtLocalize("finished"));
		else
			str_copy(aType, Run.m_Practice ? DtLocalize("practice") : DtLocalize("real"));
		const std::string Segment = Splits.PointName(Run.m_From) + " → " + Splits.PointName(Run.m_To);

		Ui()->DoLabel(&aCols[1], aDate, FontSize, TEXTALIGN_ML);
		SLabelProperties SegmentProps;
		SegmentProps.m_MaxWidth = aCols[2].w - 5.0f;
		SegmentProps.m_EllipsisAtEnd = true;
		Ui()->DoLabel(&aCols[2], Segment.c_str(), FontSize, TEXTALIGN_ML, SegmentProps);
		Ui()->DoLabel(&aCols[3], aTime, FontSize, TEXTALIGN_ML);
		Ui()->DoLabel(&aCols[4], aType, FontSize, TEXTALIGN_ML);
	}
	s_ScrollRegion.End();
}

void CMenus::RenderSplitsGates(CUIRect View)
{
	CSplits &Splits = GameClient()->m_Splits;
	const auto &vGates = Splits.Data().m_vGates;
	const float FontSize = 12.0f;
	const float RowHeight = 20.0f;
	const float LineSize = 20.0f;

	CUIRect Label, Button;
	if(vGates.empty())
	{
		View.HSplitTop(RowHeight, &Label, &View);
		Ui()->DoLabel(&Label, DtLocalize("You have no checkpoints on this map yet."), FontSize, TEXTALIGN_ML);
		View.HSplitTop(RowHeight, &Label, &View);
		Ui()->DoLabel(&Label, DtLocalize("Click \"Edit checkpoints\" and drag with the mouse to draw one across the path."), FontSize, TEXTALIGN_ML);
		return;
	}

	if(!Splits.Data().FindGate(m_SplitsSelectedGate))
		m_SplitsSelectedGate = -1;

	// footer: rename / delete the selected checkpoint
	static CLineInputBuffered<33> s_NameInput;
	CUIRect Footer;
	View.HSplitBottom(LineSize, &View, &Footer);
	View.HSplitBottom(5.0f, &View, nullptr);
	if(m_SplitsSelectedGate >= 0)
	{
		Footer.VSplitLeft(60.0f, &Label, &Footer);
		Ui()->DoLabel(&Label, DtLocalize("Name:"), FontSize, TEXTALIGN_ML);
		Footer.VSplitLeft(200.0f, &Button, &Footer);
		Ui()->DoEditBox(&s_NameInput, &Button, FontSize);
		Footer.VSplitLeft(5.0f, nullptr, &Footer);
		Footer.VSplitLeft(100.0f, &Button, &Footer);
		static CButtonContainer s_RenameButton, s_DeleteButton;
		if(DoButton_Menu(&s_RenameButton, DtLocalize("Rename"), 0, &Button))
			Splits.RenameGate(m_SplitsSelectedGate, s_NameInput.GetString());
		Footer.VSplitLeft(5.0f, nullptr, &Footer);
		Footer.VSplitLeft(100.0f, &Button, &Footer);
		if(DoButton_Menu(&s_DeleteButton, Localize("Delete"), 0, &Button))
		{
			m_SplitsPendingDelete = SPLITS_DELETE_GATE;
			m_SplitsPendingGate = m_SplitsSelectedGate;
			char aMessage[256];
			const int Count = Splits.Data().GateDeleteCount(m_SplitsSelectedGate);
			if(Count > 0)
				str_format(aMessage, sizeof(aMessage), DtLocalize("Delete %s? Its segments are merged into the neighbouring ones, but %d times at the edge of a run can't be merged and are deleted."), Splits.PointName(DdnettySplits::GatePoint(m_SplitsSelectedGate)).c_str(), Count);
			else
				str_format(aMessage, sizeof(aMessage), DtLocalize("Delete the checkpoint %s? Its segments are merged into the neighbouring ones."), Splits.PointName(DdnettySplits::GatePoint(m_SplitsSelectedGate)).c_str());
			PopupConfirm(DtLocalize("Delete checkpoint"), aMessage, Localize("Yes"), Localize("No"), &CMenus::PopupConfirmSplitsDelete);
		}
	}
	else
	{
		SLabelProperties HintProps;
		HintProps.SetColor(ColorRGBA(1.0f, 1.0f, 1.0f, 0.6f));
		Ui()->DoLabel(&Footer, DtLocalize("Click a checkpoint to rename or delete it. Double click on the map renames too."), FontSize, TEXTALIGN_ML, HintProps);
	}

	// delete all checkpoints, together with the times that came from them
	CUIRect AllRow;
	View.HSplitBottom(LineSize, &View, &AllRow);
	View.HSplitBottom(5.0f, &View, nullptr);
	AllRow.VSplitRight(260.0f, nullptr, &AllRow);
	static CButtonContainer s_DeleteAllButton;
	if(DoButton_Menu(&s_DeleteAllButton, DtLocalize("Delete all checkpoints"), 0, &AllRow))
	{
		m_SplitsPendingDelete = SPLITS_DELETE_ALL_GATES;
		char aMessage[256];
		str_format(aMessage, sizeof(aMessage), DtLocalize("Delete all %d checkpoints? Their times are merged into the map's time checkpoints; %d times that can't be merged are deleted."), (int)vGates.size(), Splits.Data().AllGatesDeleteCount());
		PopupConfirm(DtLocalize("Delete all checkpoints"), aMessage, Localize("Yes"), Localize("No"), &CMenus::PopupConfirmSplitsDelete);
	}

	static CScrollRegion s_ScrollRegion;
	CScrollRegionParams ScrollParams;
	ScrollParams.m_ScrollUnit = RowHeight * 3.0f;
	CUIRect Content = View;
	s_ScrollRegion.Begin(&Content, &ScrollParams);

	static std::vector<CButtonContainer> s_vGateButtons;
	if(s_vGateButtons.size() != vGates.size())
		s_vGateButtons.resize(vGates.size());

	for(size_t i = 0; i < vGates.size(); i++)
	{
		const DdnettySplits::SGate &Gate = vGates[i];
		CUIRect Line;
		Content.HSplitTop(RowHeight, &Line, &Content);
		Content.HSplitTop(3.0f, nullptr, &Content);
		if(!s_ScrollRegion.AddRect(Line))
			continue;

		char aText[128];
		str_format(aText, sizeof(aText), "%s   (%d, %d)", Splits.PointName(DdnettySplits::GatePoint(Gate.m_Id)).c_str(),
			(int)((Gate.m_Ax + Gate.m_Bx) / 2.0f / 32.0f), (int)((Gate.m_Ay + Gate.m_By) / 2.0f / 32.0f));
		if(DoButton_Menu(&s_vGateButtons[i], aText, m_SplitsSelectedGate == Gate.m_Id, &Line))
		{
			m_SplitsSelectedGate = Gate.m_Id;
			s_NameInput.Set(Gate.m_Name.c_str());
		}
	}
	s_ScrollRegion.End();
}

void CMenus::PopupConfirmSplitsDelete()
{
	CSplits &Splits = GameClient()->m_Splits;
	if(m_SplitsPendingDelete == SPLITS_DELETE_RUNS)
		Splits.DeleteRuns(m_vSplitsPendingRuns);
	else if(m_SplitsPendingDelete == SPLITS_DELETE_SEGMENT)
		Splits.DeleteSegment(m_SplitsPendingFrom, m_SplitsPendingTo);
	else if(m_SplitsPendingDelete == SPLITS_DELETE_ALL_GATES)
	{
		Splits.DeleteAllGates();
		m_SplitsSelectedGate = -1;
	}
	else if(m_SplitsPendingDelete == SPLITS_DELETE_GATE)
	{
		Splits.DeleteGate(m_SplitsPendingGate);
		m_SplitsSelectedGate = -1;
	}
	m_SplitsPendingDelete = SPLITS_DELETE_NONE;
	m_vSplitsPendingRuns.clear();
	m_vSplitsSelectedRuns.clear();
}

// --- Analysis charts ---

namespace
{
	const ColorRGBA CHART_BG(0.0f, 0.0f, 0.0f, 0.25f);
	const ColorRGBA CHART_GRID(1.0f, 1.0f, 1.0f, 0.08f);
	const ColorRGBA CHART_BLUE(0.35f, 0.65f, 1.0f, 1.0f);
	const ColorRGBA CHART_GREEN(0.45f, 0.9f, 0.5f, 1.0f);
	const ColorRGBA CHART_ORANGE(1.0f, 0.65f, 0.25f, 1.0f);
	const ColorRGBA CHART_RED(1.0f, 0.4f, 0.4f, 1.0f);
	const ColorRGBA CHART_GREY(1.0f, 1.0f, 1.0f, 0.45f);
	const ColorRGBA CHART_GOLD(1.0f, 0.82f, 0.25f, 1.0f);

	int64_t LocalUtcOffset()
	{
		const time_t Now = time(nullptr);
		std::tm Local = *std::localtime(&Now);
		std::tm Utc = *std::gmtime(&Now);
		Local.tm_isdst = 0;
		Utc.tm_isdst = 0;
		return (int64_t)std::difftime(std::mktime(&Local), std::mktime(&Utc));
	}
} // namespace

void CMenus::RenderSplitsAnalysis(CUIRect View)
{
	CSplits &Splits = GameClient()->m_Splits;
	const DdnettySplits::SAnalysis &Analysis = Splits.Analysis();
	const DdnettySplits::SOptions Options = Splits.Options();
	const DdnettySplits::CSplitsData &Data = Splits.Data();
	const float FontSize = 12.0f;
	const float SmallFont = 10.0f;

	CUIRect Label;
	if(Data.m_vAttempts.empty())
	{
		View.HSplitTop(18.0f, &Label, &View);
		Ui()->DoLabel(&Label, DtLocalize("No splits recorded on this map yet. Play through a time checkpoint or one of your checkpoints."), FontSize, TEXTALIGN_ML);
		return;
	}

	// hover text, drawn last so it is on top of everything
	std::string HoverText;
	vec2 HoverPos;
	const vec2 Mouse = Ui()->MousePos();

	auto &&DrawLine = [&](vec2 A, vec2 B, float Width, ColorRGBA Color) {
		const vec2 Dir = B - A;
		const float Length = length(Dir);
		if(Length < 0.001f)
			return;
		const vec2 Normal = vec2(-Dir.y, Dir.x) / Length * (Width / 2.0f);
		Graphics()->SetColor(Color.r, Color.g, Color.b, Color.a);
		const IGraphics::CFreeformItem Item(A + Normal, B + Normal, A - Normal, B - Normal);
		Graphics()->QuadsDrawFreeform(&Item, 1);
	};
	auto &&DrawDashed = [&](vec2 A, vec2 B, float Width, ColorRGBA Color) {
		const float Length = distance(A, B);
		const int Num = std::max(1, (int)(Length / 8.0f));
		for(int i = 0; i < Num; i += 2)
			DrawLine(mix(A, B, (float)i / Num), mix(A, B, std::min(1.0f, (float)(i + 1) / Num)), Width, Color);
	};
	auto &&MixColor = [](ColorRGBA A, ColorRGBA B, float T) {
		return ColorRGBA(A.r + (B.r - A.r) * T, A.g + (B.g - A.g) * T, A.b + (B.b - A.b) * T, A.a + (B.a - A.a) * T);
	};
	auto &&DrawDot = [&](vec2 Pos, float Size, ColorRGBA Color) {
		DrawLine(Pos - vec2(Size / 2.0f, 0.0f), Pos + vec2(Size / 2.0f, 0.0f), Size, Color);
	};
	auto &&SmallText = [&](float X, float Y, const char *pText, ColorRGBA Color, bool AlignRight = false) {
		TextRender()->TextColor(Color);
		const float W = TextRender()->TextWidth(SmallFont, pText);
		TextRender()->Text(AlignRight ? X - W : X, Y, SmallFont, pText);
		TextRender()->TextColor(TextRender()->DefaultTextColor());
	};
	// chart box with a title, returns the plot area
	auto &&ChartBox = [&](CUIRect Box, const char *pTitle, CUIRect *pPlot) {
		Box.Draw(CHART_BG, IGraphics::CORNER_ALL, 5.0f);
		CUIRect Title;
		Box.Margin(6.0f, &Box);
		Box.HSplitTop(16.0f, &Title, &Box);
		Ui()->DoLabel(&Title, pTitle, 13.0f, TEXTALIGN_ML);
		Box.HSplitTop(4.0f, nullptr, &Box);
		*pPlot = Box;
	};
	// y axis with three labelled grid lines, returns the area for the data
	auto &&TimeAxis = [&](CUIRect Plot, int MinMs, int MaxMs, CUIRect *pData) {
		CUIRect Axis;
		Plot.VSplitLeft(55.0f, &Axis, &Plot);
		Plot.HSplitBottom(12.0f, &Plot, nullptr);
		Graphics()->TextureClear();
		Graphics()->QuadsBegin();
		for(int i = 0; i <= 2; i++)
		{
			const float Y = Plot.y + Plot.h * i / 2.0f;
			DrawLine(vec2(Plot.x, Y), vec2(Plot.x + Plot.w, Y), 1.0f, CHART_GRID);
		}
		Graphics()->QuadsEnd();
		for(int i = 0; i <= 2; i++)
		{
			char aTime[32];
			CSplits::FormatMillis(MaxMs - (MaxMs - MinMs) * i / 2, aTime, sizeof(aTime));
			SmallText(Axis.x + Axis.w - 4.0f, Plot.y + Plot.h * i / 2.0f - SmallFont / 2.0f, aTime, CHART_GREY, true);
		}
		*pData = Plot;
	};

	static CScrollRegion s_ScrollRegion;
	CScrollRegionParams ScrollParams;
	ScrollParams.m_ScrollUnit = 60.0f;
	CUIRect Content = View;
	s_ScrollRegion.Begin(&Content, &ScrollParams);
	const float Gap = 8.0f;

	// --- number cards ---
	{
		const auto Summary = Data.Summarize(Analysis, Options.m_CountPractice);
		int PlayerBest = Splits.PlayerBestMillis();
		if(PlayerBest < 0)
			PlayerBest = Analysis.m_BestFinished;
		char aaValues[6][64];
		CSplits::FormatMillis(PlayerBest, aaValues[0], sizeof(aaValues[0]));
		CSplits::FormatMillis(Analysis.m_BuiltRun, aaValues[1], sizeof(aaValues[1]));
		if(PlayerBest > 0 && Analysis.m_BuiltRun > 0 && PlayerBest > Analysis.m_BuiltRun)
			CSplits::FormatMillis(PlayerBest - Analysis.m_BuiltRun, aaValues[2], sizeof(aaValues[2]));
		else
			str_copy(aaValues[2], "—");
		str_format(aaValues[3], sizeof(aaValues[3]), "%d / %d", Summary.m_FinishedRuns, Summary.m_Attempts);
		const int Minutes = (int)(Summary.m_TotalMillis / 60000);
		str_format(aaValues[4], sizeof(aaValues[4]), "%dh %02dmin", Minutes / 60, Minutes % 60);
		if(Summary.m_WorstStdDev >= 0)
		{
			char aStd[32];
			CSplits::FormatMillis(Summary.m_WorstStdDev, aStd, sizeof(aStd));
			str_format(aaValues[5], sizeof(aaValues[5]), "%s (±%s)", (Splits.PointName(Summary.m_WorstFrom) + " → " + Splits.PointName(Summary.m_WorstTo)).c_str(), aStd);
		}
		else
			str_copy(aaValues[5], "—");
		const char *apLabels[6] = {DtLocalize("Your best"), DtLocalize("Ideal run"), DtLocalize("Possible gain"), DtLocalize("Finished runs / attempts"), DtLocalize("Time practicing"), DtLocalize("Least consistent segment")};

		CUIRect Cards;
		Content.HSplitTop(2 * 40.0f + 6.0f, &Cards, &Content);
		Content.HSplitTop(Gap, nullptr, &Content);
		if(s_ScrollRegion.AddRect(Cards))
		{
			for(int Row = 0; Row < 2; Row++)
			{
				CUIRect Line;
				Cards.HSplitTop(40.0f, &Line, &Cards);
				Cards.HSplitTop(6.0f, nullptr, &Cards);
				const float CardW = (Line.w - 12.0f) / 3.0f;
				for(int Col = 0; Col < 3; Col++)
				{
					CUIRect Card;
					Line.VSplitLeft(CardW, &Card, &Line);
					Line.VSplitLeft(6.0f, nullptr, &Line);
					Card.Draw(CHART_BG, IGraphics::CORNER_ALL, 5.0f);
					Card.Margin(5.0f, &Card);
					CUIRect Top, Bottom;
					Card.HSplitMid(&Top, &Bottom);
					SLabelProperties Props;
					Props.SetColor(CHART_GREY);
					Ui()->DoLabel(&Top, apLabels[Row * 3 + Col], SmallFont, TEXTALIGN_ML, Props);
					SLabelProperties ValueProps;
					ValueProps.m_MaxWidth = Bottom.w;
					ValueProps.m_EllipsisAtEnd = true;
					Ui()->DoLabel(&Bottom, aaValues[Row * 3 + Col], 14.0f, TEXTALIGN_ML, ValueProps);
				}
			}
		}
	}

	// --- 1: evolution of finished runs ---
	{
		CUIRect Box, Plot, Area;
		Content.HSplitTop(170.0f, &Box, &Content);
		Content.HSplitTop(Gap, nullptr, &Content);
		if(s_ScrollRegion.AddRect(Box))
		{
			CUIRect Toggle;
			Box.HSplitTop(0.0f, nullptr, &Box);
			ChartBox(Box, DtLocalize("Run evolution"), &Plot);
			// practice toggle in the title row
			Toggle = {Box.x + Box.w - 170.0f, Box.y + 5.0f, 165.0f, 16.0f};
			static int s_PracticeToggle;
			if(DoButton_CheckBox(&s_PracticeToggle, DtLocalize("Include practice"), m_SplitsAnalysisPractice, &Toggle))
				m_SplitsAnalysisPractice = !m_SplitsAnalysisPractice;

			const auto vRuns = Data.FinishedRuns(m_SplitsAnalysisPractice);
			int PlayerBest = Splits.PlayerBestMillis();
			if(PlayerBest < 0)
				PlayerBest = Analysis.m_BestFinished;
			const int Target = Splits.TargetMillis();
			if(vRuns.empty())
			{
				Ui()->DoLabel(&Plot, DtLocalize("Finish the map once to see your runs here."), FontSize, TEXTALIGN_MC);
			}
			else
			{
				int MinMs = INT_MAX, MaxMs = 0;
				for(const auto &Run : vRuns)
				{
					MinMs = std::min(MinMs, Run.m_Millis);
					MaxMs = std::max(MaxMs, Run.m_Millis);
				}
				for(int Ref : {PlayerBest, Analysis.m_BuiltRun, Target})
				{
					if(Ref > 0)
					{
						MinMs = std::min(MinMs, Ref);
						MaxMs = std::max(MaxMs, Ref);
					}
				}
				const int Pad = std::max(500, (MaxMs - MinMs) / 10);
				MinMs = std::max(0, MinMs - Pad);
				MaxMs += Pad;
				TimeAxis(Plot, MinMs, MaxMs, &Area);
				auto &&ToY = [&](int Ms) { return Area.y + Area.h * (1.0f - (float)(Ms - MinMs) / (float)(MaxMs - MinMs)); };
				auto &&ToX = [&](size_t i) { return vRuns.size() == 1 ? Area.x + Area.w / 2.0f : Area.x + Area.w * i / (float)(vRuns.size() - 1); };

				Graphics()->TextureClear();
				Graphics()->QuadsBegin();
				if(Target > 0)
					DrawLine(vec2(Area.x, ToY(Target)), vec2(Area.x + Area.w, ToY(Target)), 1.5f, CHART_RED);
				if(Analysis.m_BuiltRun > 0)
					DrawDashed(vec2(Area.x, ToY(Analysis.m_BuiltRun)), vec2(Area.x + Area.w, ToY(Analysis.m_BuiltRun)), 1.5f, CHART_BLUE);
				if(PlayerBest > 0)
					DrawLine(vec2(Area.x, ToY(PlayerBest)), vec2(Area.x + Area.w, ToY(PlayerBest)), 1.0f, ColorRGBA(1.0f, 1.0f, 1.0f, 0.6f));
				for(size_t i = 0; i + 1 < vRuns.size(); i++)
					DrawLine(vec2(ToX(i), ToY(vRuns[i].m_Millis)), vec2(ToX(i + 1), ToY(vRuns[i + 1].m_Millis)), 1.5f, ColorRGBA(1.0f, 1.0f, 1.0f, 0.35f));
				for(size_t i = 0; i < vRuns.size(); i++)
				{
					const vec2 P(ToX(i), ToY(vRuns[i].m_Millis));
					DrawDot(P, 5.0f, vRuns[i].m_Practice ? CHART_ORANGE : CHART_GREEN);
					if(distance(Mouse, P) < 6.0f && Ui()->MouseHovered(&Box))
					{
						char aTime[32], aDate[32], aBuf[96];
						CSplits::FormatMillis(vRuns[i].m_Millis, aTime, sizeof(aTime));
						str_timestamp_ex((time_t)vRuns[i].m_Timestamp, aDate, sizeof(aDate), "%d/%m %H:%M");
						str_format(aBuf, sizeof(aBuf), "%s · %s%s", aTime, aDate, vRuns[i].m_Practice ? " · practice" : "");
						HoverText = aBuf;
						HoverPos = P;
					}
				}
				Graphics()->QuadsEnd();

				// legend
				float LegendX = Area.x;
				const float LegendY = Area.y + Area.h + 1.0f;
				auto &&Legend = [&](const char *pText, ColorRGBA Color) {
					SmallText(LegendX, LegendY, pText, Color);
					LegendX += TextRender()->TextWidth(SmallFont, pText) + 14.0f;
				};
				Legend(DtLocalize("● run"), CHART_GREEN);
				if(m_SplitsAnalysisPractice)
					Legend(DtLocalize("● practice"), CHART_ORANGE);
				if(PlayerBest > 0)
					Legend(DtLocalize("— your best"), ColorRGBA(1.0f, 1.0f, 1.0f, 0.8f));
				if(Analysis.m_BuiltRun > 0)
					Legend(DtLocalize("- - ideal run"), CHART_BLUE);
				if(Target > 0)
					Legend(DtLocalize("— target"), CHART_RED);
			}
		}
	}

	// --- ideal run over time: a step each time a segment got faster ---
	{
		const auto vIdeal = DdnettySplits::CSplitsData::IdealHistory(Analysis);
		CUIRect Box, Plot, Area;
		Content.HSplitTop(170.0f, &Box, &Content);
		Content.HSplitTop(Gap, nullptr, &Content);
		if(s_ScrollRegion.AddRect(Box))
		{
			ChartBox(Box, DtLocalize("Ideal run over time"), &Plot);
			if(vIdeal.empty())
			{
				Ui()->DoLabel(&Plot, DtLocalize("Needs one attempt of every segment of your route."), FontSize, TEXTALIGN_MC);
			}
			else
			{
				int PlayerBest = Splits.PlayerBestMillis();
				if(PlayerBest < 0)
					PlayerBest = Analysis.m_BestFinished;
				const DdnettySplits::SRefRun *pTop = Splits.RefRun();
				const int TopMillis = pTop ? pTop->m_Millis : -1;
				int MinMs = INT_MAX, MaxMs = 0;
				for(const auto &Point : vIdeal)
				{
					MinMs = std::min(MinMs, Point.m_Millis);
					MaxMs = std::max(MaxMs, Point.m_Millis);
				}
				for(int Ref : {PlayerBest, TopMillis})
				{
					if(Ref > 0)
					{
						MinMs = std::min(MinMs, Ref);
						MaxMs = std::max(MaxMs, Ref);
					}
				}
				const int Pad = std::max(300, (MaxMs - MinMs) / 10);
				MinMs = std::max(0, MinMs - Pad);
				MaxMs += Pad;
				TimeAxis(Plot, MinMs, MaxMs, &Area);
				const int First = vIdeal.front().m_Attempt;
				const int Last = std::max(First + 1, (int)Analysis.m_vAttempts.size() - 1);
				auto &&ToY = [&](int Ms) { return Area.y + Area.h * (1.0f - (float)(Ms - MinMs) / (float)(MaxMs - MinMs)); };
				auto &&ToX = [&](int Attempt) { return Area.x + Area.w * (Attempt - First) / (float)(Last - First); };

				Graphics()->TextureClear();
				Graphics()->QuadsBegin();
				if(TopMillis > 0)
					DrawDashed(vec2(Area.x, ToY(TopMillis)), vec2(Area.x + Area.w, ToY(TopMillis)), 1.5f, CHART_GOLD);
				if(PlayerBest > 0)
					DrawLine(vec2(Area.x, ToY(PlayerBest)), vec2(Area.x + Area.w, ToY(PlayerBest)), 1.0f, ColorRGBA(1.0f, 1.0f, 1.0f, 0.6f));
				for(size_t i = 0; i < vIdeal.size(); i++)
				{
					const float X = ToX(vIdeal[i].m_Attempt);
					const float NextX = i + 1 < vIdeal.size() ? ToX(vIdeal[i + 1].m_Attempt) : Area.x + Area.w;
					const float Y = ToY(vIdeal[i].m_Millis);
					DrawLine(vec2(X, Y), vec2(NextX, Y), 2.0f, CHART_BLUE);
					if(i + 1 < vIdeal.size())
						DrawLine(vec2(NextX, Y), vec2(NextX, ToY(vIdeal[i + 1].m_Millis)), 2.0f, CHART_BLUE);
					const vec2 P(X, Y);
					DrawDot(P, 4.0f, CHART_BLUE);
					if(distance(Mouse, P) < 6.0f && Ui()->MouseHovered(&Box))
					{
						char aTime[32], aDate[32], aBuf[128];
						CSplits::FormatMillis(vIdeal[i].m_Millis, aTime, sizeof(aTime));
						str_timestamp_ex((time_t)vIdeal[i].m_Timestamp, aDate, sizeof(aDate), "%d/%m %H:%M");
						if(i > 0)
						{
							char aDiff[32];
							CSplits::FormatDiff(vIdeal[i].m_Millis - vIdeal[i - 1].m_Millis, aDiff, sizeof(aDiff));
							str_format(aBuf, sizeof(aBuf), DtLocalize("%s (%s) · attempt %d · %s"), aTime, aDiff, vIdeal[i].m_Attempt + 1, aDate);
						}
						else
							str_format(aBuf, sizeof(aBuf), DtLocalize("%s · attempt %d · %s"), aTime, vIdeal[i].m_Attempt + 1, aDate);
						HoverText = aBuf;
						HoverPos = P;
					}
				}
				Graphics()->QuadsEnd();

				float LegendX = Area.x;
				const float LegendY = Area.y + Area.h + 1.0f;
				auto &&Legend = [&](const char *pText, ColorRGBA Color) {
					SmallText(LegendX, LegendY, pText, Color);
					LegendX += TextRender()->TextWidth(SmallFont, pText) + 14.0f;
				};
				Legend(DtLocalize("— ideal run"), CHART_BLUE);
				if(PlayerBest > 0)
					Legend(DtLocalize("— your best"), ColorRGBA(1.0f, 1.0f, 1.0f, 0.8f));
				if(pTop)
				{
					char aTop[96];
					str_format(aTop, sizeof(aTop), "- - %s", pTop->m_Name.c_str());
					Legend(aTop, CHART_GOLD);
				}
			}
		}
	}

	// --- 2: where you lose time ---
	const auto vLosses = DdnettySplits::CSplitsData::Losses(Analysis);
	{
		const float BarH = 16.0f;
		const int NumBars = std::min((int)vLosses.size(), 12);
		CUIRect Box, Plot;
		Content.HSplitTop(40.0f + std::max(1, NumBars) * (BarH + 3.0f), &Box, &Content);
		Content.HSplitTop(Gap, nullptr, &Content);
		if(s_ScrollRegion.AddRect(Box))
		{
			ChartBox(Box, DtLocalize("Where you lose time (your average − your best)"), &Plot);
			if(vLosses.empty())
			{
				Ui()->DoLabel(&Plot, DtLocalize("Needs a few attempts of each segment."), FontSize, TEXTALIGN_MC);
			}
			else
			{
				const int MaxLoss = std::max(1, vLosses.front().m_Loss);
				static CButtonContainer s_aBarButtons[12];
				for(int i = 0; i < NumBars; i++)
				{
					const auto &Loss = vLosses[i];
					CUIRect Line, Name, Bar, Value;
					Plot.HSplitTop(BarH, &Line, &Plot);
					Plot.HSplitTop(3.0f, nullptr, &Plot);
					Line.VSplitLeft(170.0f, &Name, &Bar);
					Bar.VSplitRight(60.0f, &Bar, &Value);
					const bool Selected = Loss.m_From == m_SplitsAnalysisSegmentFrom && Loss.m_To == m_SplitsAnalysisSegmentTo;
					if(Ui()->MouseHovered(&Line) || Selected)
						Line.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, Selected ? 0.12f : 0.06f), IGraphics::CORNER_ALL, 3.0f);
					if(Ui()->DoButtonLogic(&s_aBarButtons[i], 0, &Line, BUTTONFLAG_LEFT))
					{
						m_SplitsAnalysisSegmentFrom = Loss.m_From;
						m_SplitsAnalysisSegmentTo = Loss.m_To;
					}
					SLabelProperties NameProps;
					NameProps.m_MaxWidth = Name.w - 6.0f;
					NameProps.m_EllipsisAtEnd = true;
					const std::string SegmentName = Splits.PointName(Loss.m_From) + " → " + Splits.PointName(Loss.m_To);
					Name.VSplitLeft(4.0f, nullptr, &Name);
					Ui()->DoLabel(&Name, SegmentName.c_str(), SmallFont + 1.0f, TEXTALIGN_ML, NameProps);
					CUIRect Fill = Bar;
					Fill.w = std::max(2.0f, Bar.w * Loss.m_Loss / (float)MaxLoss);
					Fill.Margin(2.0f, &Fill);
					const float Heat = Loss.m_Loss / (float)MaxLoss;
					Fill.Draw(ColorRGBA(1.0f, 0.75f - 0.4f * Heat, 0.3f, 0.85f), IGraphics::CORNER_ALL, 2.0f);
					char aDiff[32];
					CSplits::FormatDiff(Loss.m_Loss, aDiff, sizeof(aDiff));
					Value.VSplitLeft(6.0f, nullptr, &Value);
					Ui()->DoLabel(&Value, aDiff, SmallFont + 1.0f, TEXTALIGN_ML);
					if(Ui()->MouseHovered(&Line))
					{
						char aBest[32], aMean[32], aBuf[128];
						CSplits::FormatMillis(Loss.m_Best, aBest, sizeof(aBest));
						CSplits::FormatMillis(Loss.m_Mean, aMean, sizeof(aMean));
						str_format(aBuf, sizeof(aBuf), DtLocalize("best %s · average %s · click to see the history"), aBest, aMean);
						HoverText = aBuf;
						HoverPos = Mouse;
					}
				}
			}
		}
	}

	// --- average vs best, every attempt and only the focused ones ---
	{
		const auto vGaps = DdnettySplits::CSplitsData::FocusGaps(Analysis);
		const float BarH = 14.0f;
		const int NumBars = std::min((int)vGaps.size(), 40);
		CUIRect Box, Plot;
		Content.HSplitTop(52.0f + std::max(1, NumBars) * (BarH + 2.0f), &Box, &Content);
		Content.HSplitTop(Gap, nullptr, &Content);
		if(s_ScrollRegion.AddRect(Box))
		{
			ChartBox(Box, DtLocalize("Average − best: every attempt vs focused runs"), &Plot);
			if(vGaps.empty())
			{
				Ui()->DoLabel(&Plot, DtLocalize("Needs a few attempts of each segment."), FontSize, TEXTALIGN_MC);
			}
			else
			{
				int MaxGap = 1;
				for(const auto &GapItem : vGaps)
					MaxGap = std::max({MaxGap, GapItem.m_MeanAll - GapItem.m_Best, GapItem.m_MeanFocused - GapItem.m_Best});
				CUIRect LegendRow;
				Plot.HSplitBottom(12.0f, &Plot, &LegendRow);
				for(int i = 0; i < NumBars; i++)
				{
					const auto &GapItem = vGaps[i];
					CUIRect Line, Name, Bar, Value;
					Plot.HSplitTop(BarH, &Line, &Plot);
					Plot.HSplitTop(2.0f, nullptr, &Plot);
					Line.VSplitLeft(170.0f, &Name, &Bar);
					Bar.VSplitRight(110.0f, &Bar, &Value);
					SLabelProperties NameProps;
					NameProps.m_MaxWidth = Name.w - 6.0f;
					NameProps.m_EllipsisAtEnd = true;
					Name.VSplitLeft(4.0f, nullptr, &Name);
					Ui()->DoLabel(&Name, (Splits.PointName(GapItem.m_From) + " → " + Splits.PointName(GapItem.m_To)).c_str(), SmallFont, TEXTALIGN_ML, NameProps);
					CUIRect Top, Bottom;
					Bar.HSplitMid(&Top, &Bottom);
					const int AllGap = std::max(0, GapItem.m_MeanAll - GapItem.m_Best);
					const int FocusedGap = GapItem.m_MeanFocused >= 0 ? std::max(0, GapItem.m_MeanFocused - GapItem.m_Best) : -1;
					CUIRect{Top.x, Top.y + 0.5f, std::max(1.0f, Top.w * AllGap / (float)MaxGap), Top.h - 1.0f}.Draw(CHART_GREY, IGraphics::CORNER_NONE, 0.0f);
					if(FocusedGap >= 0)
						CUIRect{Bottom.x, Bottom.y + 0.5f, std::max(1.0f, Bottom.w * FocusedGap / (float)MaxGap), Bottom.h - 1.0f}.Draw(CHART_BLUE, IGraphics::CORNER_NONE, 0.0f);
					char aAll[32], aFocused[32], aValue[80];
					CSplits::FormatDiff(AllGap, aAll, sizeof(aAll));
					if(FocusedGap >= 0)
						CSplits::FormatDiff(FocusedGap, aFocused, sizeof(aFocused));
					else
						str_copy(aFocused, "—");
					str_format(aValue, sizeof(aValue), "%s / %s", aAll, aFocused);
					Value.VSplitLeft(6.0f, nullptr, &Value);
					Ui()->DoLabel(&Value, aValue, SmallFont, TEXTALIGN_ML);
				}
				float LegendX = LegendRow.x;
				auto &&Legend = [&](const char *pText, ColorRGBA Color) {
					SmallText(LegendX, LegendRow.y + 1.0f, pText, Color);
					LegendX += TextRender()->TextWidth(SmallFont, pText) + 14.0f;
				};
				Legend(DtLocalize("■ every attempt"), CHART_GREY);
				Legend(DtLocalize("■ focused runs"), CHART_BLUE);
			}
		}
	}

	// --- 3: history of one segment ---
	{
		if(!Analysis.Find(m_SplitsAnalysisSegmentFrom, m_SplitsAnalysisSegmentTo) && !vLosses.empty())
		{
			m_SplitsAnalysisSegmentFrom = vLosses.front().m_From;
			m_SplitsAnalysisSegmentTo = vLosses.front().m_To;
		}
		CUIRect Box, Plot, Area;
		Content.HSplitTop(170.0f, &Box, &Content);
		Content.HSplitTop(Gap, nullptr, &Content);
		if(s_ScrollRegion.AddRect(Box))
		{
			char aTitle[160];
			if(m_SplitsAnalysisSegmentFrom != DdnettySplits::POINT_NONE && Analysis.Find(m_SplitsAnalysisSegmentFrom, m_SplitsAnalysisSegmentTo))
				str_format(aTitle, sizeof(aTitle), DtLocalize("History: %s"), (Splits.PointName(m_SplitsAnalysisSegmentFrom) + " → " + Splits.PointName(m_SplitsAnalysisSegmentTo)).c_str());
			else
				str_copy(aTitle, DtLocalize("History of a segment"));
			ChartBox(Box, aTitle, &Plot);
			const auto vHistory = DdnettySplits::CSplitsData::SegmentHistory(Analysis, m_SplitsAnalysisSegmentFrom, m_SplitsAnalysisSegmentTo);
			if(vHistory.empty())
			{
				Ui()->DoLabel(&Plot, DtLocalize("Click a bar above to see a segment here."), FontSize, TEXTALIGN_MC);
			}
			else
			{
				int MinMs = INT_MAX, MaxMs = 0;
				for(const auto &Point : vHistory)
				{
					MinMs = std::min(MinMs, Point.m_Millis);
					MaxMs = std::max(MaxMs, Point.m_Millis);
				}
				const int Pad = std::max(100, (MaxMs - MinMs) / 10);
				MinMs = std::max(0, MinMs - Pad);
				MaxMs += Pad;
				TimeAxis(Plot, MinMs, MaxMs, &Area);
				auto &&ToY = [&](int Ms) { return Area.y + Area.h * (1.0f - (float)(Ms - MinMs) / (float)(MaxMs - MinMs)); };
				auto &&ToX = [&](size_t i) { return vHistory.size() == 1 ? Area.x + Area.w / 2.0f : Area.x + Area.w * i / (float)(vHistory.size() - 1); };

				// band of the focused runs: average ± variation
				if(const DdnettySplits::SSegmentStats *pStats = Analysis.Find(m_SplitsAnalysisSegmentFrom, m_SplitsAnalysisSegmentTo); pStats && pStats->m_Mean >= 0)
				{
					const float Top = std::clamp(ToY(pStats->m_Mean + pStats->m_StdDev), Area.y, Area.y + Area.h);
					const float Bottom = std::clamp(ToY(pStats->m_Mean - pStats->m_StdDev), Area.y, Area.y + Area.h);
					CUIRect{Area.x, Top, Area.w, std::max(1.0f, Bottom - Top)}.Draw(CHART_BLUE.WithAlpha(0.12f), IGraphics::CORNER_NONE, 0.0f);
				}
				Graphics()->TextureClear();
				Graphics()->QuadsBegin();
				// best so far (steps down) and moving average
				int Best = INT_MAX;
				for(size_t i = 0; i < vHistory.size(); i++)
				{
					const int PrevBest = Best;
					Best = std::min(Best, vHistory[i].m_Millis);
					if(i > 0)
					{
						DrawLine(vec2(ToX(i - 1), ToY(PrevBest)), vec2(ToX(i), ToY(PrevBest)), 1.5f, CHART_GREEN.WithAlpha(0.6f));
						if(Best != PrevBest)
							DrawLine(vec2(ToX(i), ToY(PrevBest)), vec2(ToX(i), ToY(Best)), 1.5f, CHART_GREEN.WithAlpha(0.6f));
						DrawLine(vec2(ToX(i - 1), ToY(vHistory[i - 1].m_MovingAverage)), vec2(ToX(i), ToY(vHistory[i].m_MovingAverage)), 2.0f, CHART_BLUE);
					}
				}
				for(size_t i = 0; i < vHistory.size(); i++)
				{
					const auto &Point = vHistory[i];
					const vec2 P(ToX(i), ToY(Point.m_Millis));
					ColorRGBA Color = Point.m_NewBest ? CHART_GREEN : (Point.m_Practice ? CHART_ORANGE.WithAlpha(0.7f) : ColorRGBA(1.0f, 1.0f, 1.0f, 0.55f));
					if(!Point.m_Focused && !Point.m_NewBest)
						Color = Color.WithMultipliedAlpha(0.35f);
					DrawDot(P, Point.m_NewBest ? 5.0f : 4.0f, Color);
					if(distance(Mouse, P) < 6.0f && Ui()->MouseHovered(&Box))
					{
						char aTime[32], aDate[32], aBuf[96];
						CSplits::FormatMillis(Point.m_Millis, aTime, sizeof(aTime));
						str_timestamp_ex((time_t)Point.m_Timestamp, aDate, sizeof(aDate), "%d/%m %H:%M");
						str_format(aBuf, sizeof(aBuf), "#%d · %s · %s%s", (int)i + 1, aTime, aDate, Point.m_Practice ? " · practice" : "");
						HoverText = aBuf;
						HoverPos = P;
					}
				}
				Graphics()->QuadsEnd();

				float LegendX = Area.x;
				const float LegendY = Area.y + Area.h + 1.0f;
				auto &&Legend = [&](const char *pText, ColorRGBA Color) {
					SmallText(LegendX, LegendY, pText, Color);
					LegendX += TextRender()->TextWidth(SmallFont, pText) + 14.0f;
				};
				Legend(DtLocalize("● new best"), CHART_GREEN);
				Legend(DtLocalize("● attempt"), ColorRGBA(1.0f, 1.0f, 1.0f, 0.7f));
				Legend(DtLocalize("● practice"), CHART_ORANGE);
				Legend(DtLocalize("— average of the last 10"), CHART_BLUE);
				Legend(DtLocalize("■ focused: average ± variation"), CHART_BLUE.WithAlpha(0.6f));
			}
		}
	}

	// --- consistency: variation of the last 10 passes of the same segment ---
	{
		const auto vHistory = DdnettySplits::CSplitsData::SegmentHistory(Analysis, m_SplitsAnalysisSegmentFrom, m_SplitsAnalysisSegmentTo);
		CUIRect Box, Plot, Area;
		Content.HSplitTop(130.0f, &Box, &Content);
		Content.HSplitTop(Gap, nullptr, &Content);
		if(s_ScrollRegion.AddRect(Box))
		{
			ChartBox(Box, DtLocalize("Consistency (variation of the last 10 passes)"), &Plot);
			if(vHistory.size() < 3)
			{
				Ui()->DoLabel(&Plot, DtLocalize("Needs a few passes of the segment above."), FontSize, TEXTALIGN_MC);
			}
			else
			{
				int MaxMs = 1;
				for(const auto &Point : vHistory)
					MaxMs = std::max(MaxMs, Point.m_MovingStdDev);
				MaxMs += MaxMs / 10 + 10;
				TimeAxis(Plot, 0, MaxMs, &Area);
				auto &&ToY = [&](int Ms) { return Area.y + Area.h * (1.0f - Ms / (float)MaxMs); };
				auto &&ToX = [&](size_t i) { return Area.x + Area.w * i / (float)(vHistory.size() - 1); };
				Graphics()->TextureClear();
				Graphics()->QuadsBegin();
				for(size_t i = 1; i < vHistory.size(); i++)
					DrawLine(vec2(ToX(i - 1), ToY(vHistory[i - 1].m_MovingStdDev)), vec2(ToX(i), ToY(vHistory[i].m_MovingStdDev)), 2.0f, CHART_ORANGE);
				Graphics()->QuadsEnd();
				for(size_t i = 0; i < vHistory.size(); i++)
				{
					const vec2 P(ToX(i), ToY(vHistory[i].m_MovingStdDev));
					if(std::abs(Mouse.x - P.x) < Area.w / (2.0f * vHistory.size()) + 0.5f && Ui()->MouseHovered(&Box))
					{
						char aStd[32], aBuf[96];
						CSplits::FormatMillis(vHistory[i].m_MovingStdDev, aStd, sizeof(aStd));
						str_format(aBuf, sizeof(aBuf), DtLocalize("#%d · ±%s"), (int)i + 1, aStd);
						HoverText = aBuf;
						HoverPos = P;
					}
				}
				SmallText(Area.x, Area.y + Area.h + 1.0f, DtLocalize("lower = more consistent"), CHART_GREY);
			}
		}
	}

	// the same stats with only the map's time checkpoints, to compare with the reference file
	const DdnettySplits::CSplitsRefs &Refs = Splits.Refs();
	static DdnettySplits::SAnalysis s_TimecpAnalysis;
	static size_t s_TimecpAttempts = SIZE_MAX;
	static DdnettySplits::SOptions s_TimecpOptions;
	DdnettySplits::SOptions TimecpOptions = Options;
	TimecpOptions.m_UseGates = false;
	TimecpOptions.m_UseTimecps = true;
	if(!Refs.Empty())
	{
		const bool Same = s_TimecpOptions.m_CountPractice == TimecpOptions.m_CountPractice && s_TimecpOptions.m_Since == TimecpOptions.m_Since &&
				  s_TimecpOptions.m_FocusMarginMillis == TimecpOptions.m_FocusMarginMillis && s_TimecpOptions.m_RunEntriesOnly == TimecpOptions.m_RunEntriesOnly &&
				  s_TimecpOptions.m_CountDummy == TimecpOptions.m_CountDummy;
		if(s_TimecpAttempts != Data.m_vAttempts.size() || !Same)
		{
			s_TimecpAnalysis = Data.Analyze(TimecpOptions);
			s_TimecpAttempts = Data.m_vAttempts.size();
			s_TimecpOptions = TimecpOptions;
		}
	}

	// --- you vs the best of everyone else, per segment ---
	if(!Refs.m_vSegments.empty())
	{
		const float BarH = 14.0f;
		const int NumBars = std::min((int)Refs.m_vSegments.size(), 30);
		CUIRect Box, Plot;
		Content.HSplitTop(40.0f + NumBars * (BarH + 2.0f), &Box, &Content);
		Content.HSplitTop(Gap, nullptr, &Content);
		if(s_ScrollRegion.AddRect(Box))
		{
			ChartBox(Box, DtLocalize("You vs the best of everyone else (map time checkpoints)"), &Plot);
			int MaxDiff = 100;
			for(const auto &Other : Refs.m_vSegments)
				if(const auto *pMine = s_TimecpAnalysis.Find(Other.m_From, Other.m_To); pMine && pMine->m_Best >= 0)
					MaxDiff = std::max(MaxDiff, std::abs(pMine->m_Best - Other.m_Millis));
			for(int i = 0; i < NumBars; i++)
			{
				const auto &Other = Refs.m_vSegments[i];
				CUIRect Line, Name, Bar, Value;
				Plot.HSplitTop(BarH, &Line, &Plot);
				Plot.HSplitTop(2.0f, nullptr, &Plot);
				Line.VSplitLeft(110.0f, &Name, &Bar);
				Bar.VSplitRight(150.0f, &Bar, &Value);
				Name.VSplitLeft(4.0f, nullptr, &Name);
				Ui()->DoLabel(&Name, (Splits.PointName(Other.m_From) + " → " + Splits.PointName(Other.m_To)).c_str(), SmallFont, TEXTALIGN_ML);
				const float Mid = Bar.x + Bar.w / 2.0f;
				CUIRect{Mid - 0.5f, Bar.y, 1.0f, Bar.h}.Draw(CHART_GRID, IGraphics::CORNER_NONE, 0.0f);
				const auto *pMine = s_TimecpAnalysis.Find(Other.m_From, Other.m_To);
				char aOther[32], aValue[96];
				CSplits::FormatMillis(Other.m_Millis, aOther, sizeof(aOther));
				if(pMine && pMine->m_Best >= 0)
				{
					const int Diff = pMine->m_Best - Other.m_Millis;
					const float W = std::max(1.0f, Bar.w / 2.0f * std::abs(Diff) / (float)MaxDiff);
					CUIRect{Diff <= 0 ? Mid - W : Mid, Bar.y + 1.0f, W, Bar.h - 2.0f}.Draw(Diff <= 0 ? CHART_GREEN : CHART_RED, IGraphics::CORNER_NONE, 0.0f);
					char aDiff[32];
					CSplits::FormatDiff(Diff, aDiff, sizeof(aDiff));
					str_format(aValue, sizeof(aValue), "%s  (%s %s)", aDiff, aOther, Other.m_Name.c_str());
				}
				else
					str_format(aValue, sizeof(aValue), "—  (%s %s)", aOther, Other.m_Name.c_str());
				SLabelProperties ValueProps;
				ValueProps.m_MaxWidth = Value.w - 6.0f;
				ValueProps.m_EllipsisAtEnd = true;
				Value.VSplitLeft(6.0f, nullptr, &Value);
				Ui()->DoLabel(&Value, aValue, SmallFont, TEXTALIGN_ML, ValueProps);
			}
		}
	}

	// --- number ghost: how far ahead or behind a top player each of your last runs was ---
	if(const DdnettySplits::SRefRun *pTop = Splits.RefRun())
	{
		const auto vRuns = DdnettySplits::CSplitsData::RouteTimes(s_TimecpAnalysis, 15);
		const auto &vRoute = s_TimecpAnalysis.m_vRoute;
		CUIRect Box, Plot, Area;
		Content.HSplitTop(180.0f, &Box, &Content);
		Content.HSplitTop(Gap, nullptr, &Content);
		if(s_ScrollRegion.AddRect(Box))
		{
			char aTitle[128];
			str_format(aTitle, sizeof(aTitle), DtLocalize("Number ghost: your last runs vs %s"), pTop->m_Name.c_str());
			ChartBox(Box, aTitle, &Plot);
			if(vRuns.empty() || vRoute.size() < 2)
			{
				Ui()->DoLabel(&Plot, DtLocalize("Finish the map once to see your runs here."), FontSize, TEXTALIGN_MC);
			}
			else
			{
				int MaxDiff = 200;
				for(const auto &Run : vRuns)
					for(size_t p = 0; p < vRoute.size(); p++)
						if(const int RefAt = DdnettySplits::CSplitsRefs::TimeAt(*pTop, vRoute[p]); RefAt >= 0 && Run.m_vAtRoute[p] >= 0)
							MaxDiff = std::max(MaxDiff, std::abs(Run.m_vAtRoute[p] - RefAt));
				MaxDiff += MaxDiff / 10;
				CUIRect Axis;
				Plot.VSplitLeft(55.0f, &Axis, &Area);
				Area.HSplitBottom(12.0f, &Area, nullptr);
				auto &&ToY = [&](int Diff) { return Area.y + Area.h * (0.5f - Diff / (2.0f * MaxDiff)); };
				auto &&ToX = [&](size_t p) { return Area.x + Area.w * p / (float)(vRoute.size() - 1); };
				Graphics()->TextureClear();
				Graphics()->QuadsBegin();
				DrawLine(vec2(Area.x, ToY(0)), vec2(Area.x + Area.w, ToY(0)), 1.0f, CHART_GOLD.WithAlpha(0.6f));
				for(size_t r = 0; r < vRuns.size(); r++)
				{
					const bool Latest = r + 1 == vRuns.size();
					const float Alpha = Latest ? 1.0f : 0.15f + 0.45f * r / (float)vRuns.size();
					vec2 Prev(-1.0f, -1.0f);
					for(size_t p = 0; p < vRoute.size(); p++)
					{
						const int RefAt = DdnettySplits::CSplitsRefs::TimeAt(*pTop, vRoute[p]);
						if(RefAt < 0 || vRuns[r].m_vAtRoute[p] < 0)
							continue;
						const int Diff = vRuns[r].m_vAtRoute[p] - RefAt;
						const vec2 P(ToX(p), ToY(Diff));
						if(Prev.x >= 0.0f)
							DrawLine(Prev, P, Latest ? 2.0f : 1.2f, (Latest ? CHART_BLUE : ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f)).WithAlpha(Alpha));
						Prev = P;
						if(distance(Mouse, P) < 5.0f && Ui()->MouseHovered(&Box))
						{
							char aDiff[32], aDate[32], aBuf[128];
							CSplits::FormatDiff(Diff, aDiff, sizeof(aDiff));
							str_timestamp_ex((time_t)vRuns[r].m_Timestamp, aDate, sizeof(aDate), "%d/%m %H:%M");
							str_format(aBuf, sizeof(aBuf), "%s %s · %s", Splits.PointName(vRoute[p]).c_str(), aDiff, aDate);
							HoverText = aBuf;
							HoverPos = P;
						}
					}
				}
				Graphics()->QuadsEnd();
				char aMax[32];
				CSplits::FormatDiff(MaxDiff, aMax, sizeof(aMax));
				SmallText(Axis.x + Axis.w - 4.0f, Area.y - SmallFont / 2.0f, aMax, CHART_RED, true);
				SmallText(Axis.x + Axis.w - 4.0f, ToY(0) - SmallFont / 2.0f, "0", CHART_GOLD, true);
				CSplits::FormatDiff(-MaxDiff, aMax, sizeof(aMax));
				SmallText(Axis.x + Axis.w - 4.0f, Area.y + Area.h - SmallFont / 2.0f, aMax, CHART_GREEN, true);
				for(size_t p = 0; p < vRoute.size(); p++)
					if(DdnettySplits::CSplitsRefs::TimeAt(*pTop, vRoute[p]) >= 0 && (vRoute.size() <= 14 || p % 2 == 0))
						SmallText(ToX(p) - 6.0f, Area.y + Area.h + 1.0f, Splits.PointName(vRoute[p]).c_str(), CHART_GREY);
			}
		}
	}

	// --- reach: how many runs from the start got to each point ---
	{
		const auto vReach = DdnettySplits::CSplitsData::Reach(Analysis);
		CUIRect Box, Plot;
		Content.HSplitTop(150.0f, &Box, &Content);
		Content.HSplitTop(Gap, nullptr, &Content);
		if(s_ScrollRegion.AddRect(Box))
		{
			ChartBox(Box, DtLocalize("Reach: runs from the start that got to each point"), &Plot);
			if(vReach.size() < 2 || vReach[0] == 0)
			{
				Ui()->DoLabel(&Plot, DtLocalize("Start a few runs from the start line to see this."), FontSize, TEXTALIGN_MC);
			}
			else
			{
				CUIRect Axis;
				Plot.HSplitBottom(12.0f, &Plot, &Axis);
				const float ColW = Plot.w / vReach.size();
				for(size_t i = 0; i < vReach.size(); i++)
				{
					CUIRect Col = {Plot.x + ColW * i + 1.0f, Plot.y, std::max(1.0f, ColW - 2.0f), Plot.h};
					const float H = Plot.h * vReach[i] / (float)vReach[0];
					const float Drop = i > 0 && vReach[i - 1] > 0 ? 1.0f - vReach[i] / (float)vReach[i - 1] : 0.0f;
					CUIRect{Col.x, Col.y + Col.h - H, Col.w, H}.Draw(MixColor(CHART_GREEN, CHART_RED, std::min(1.0f, Drop * 3.0f)).WithAlpha(0.85f), IGraphics::CORNER_NONE, 0.0f);
					if(Ui()->MouseHovered(&Col))
					{
						Col.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.06f), IGraphics::CORNER_NONE, 0.0f);
						char aBuf[160];
						if(i > 0)
							str_format(aBuf, sizeof(aBuf), DtLocalize("%s · %d runs (%d%%) · %d ended in the segment before"), Splits.PointName(Analysis.m_vRoute[i]).c_str(), vReach[i], (int)std::lround(100.0 * vReach[i] / vReach[0]), vReach[i - 1] - vReach[i]);
						else
							str_format(aBuf, sizeof(aBuf), DtLocalize("%s · %d runs"), Splits.PointName(Analysis.m_vRoute[i]).c_str(), vReach[i]);
						HoverText = aBuf;
						HoverPos = Mouse;
					}
				}
				SmallText(Axis.x, Axis.y + 1.0f, Splits.PointName(Analysis.m_vRoute.front()).c_str(), CHART_GREY);
				SmallText(Axis.x + Axis.w, Axis.y + 1.0f, Splits.PointName(Analysis.m_vRoute.back()).c_str(), CHART_GREY, true);
				char aMax[32];
				str_format(aMax, sizeof(aMax), "%d", vReach[0]);
				SmallText(Plot.x + Plot.w, Plot.y - SmallFont - 2.0f, aMax, CHART_GREY, true);
			}
		}
	}

	// --- 4: activity per day ---
	{
		const int NumDays = 30;
		CUIRect Box, Plot;
		Content.HSplitTop(140.0f, &Box, &Content);
		Content.HSplitTop(Gap, nullptr, &Content);
		if(s_ScrollRegion.AddRect(Box))
		{
			ChartBox(Box, DtLocalize("Activity (attempts per day, last 30 days)"), &Plot);
			const auto vDays = Data.Activity(time_timestamp(), LocalUtcOffset(), NumDays);
			int MaxCount = 1;
			for(const auto &Day : vDays)
				MaxCount = std::max(MaxCount, Day.m_Real + Day.m_Practice);
			CUIRect Axis;
			Plot.HSplitBottom(12.0f, &Plot, &Axis);
			const float ColW = Plot.w / NumDays;
			for(int i = 0; i < NumDays; i++)
			{
				const auto &Day = vDays[i];
				CUIRect Col = {Plot.x + ColW * i + 1.0f, Plot.y, ColW - 2.0f, Plot.h};
				const float RealH = Plot.h * Day.m_Real / (float)MaxCount;
				const float PracticeH = Plot.h * Day.m_Practice / (float)MaxCount;
				if(Day.m_Real > 0)
					CUIRect{Col.x, Col.y + Col.h - RealH, Col.w, RealH}.Draw(CHART_GREEN.WithAlpha(0.8f), IGraphics::CORNER_NONE, 0.0f);
				if(Day.m_Practice > 0)
					CUIRect{Col.x, Col.y + Col.h - RealH - PracticeH, Col.w, PracticeH}.Draw(CHART_ORANGE.WithAlpha(0.8f), IGraphics::CORNER_NONE, 0.0f);
				if(Ui()->MouseHovered(&Col))
				{
					Col.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.06f), IGraphics::CORNER_NONE, 0.0f);
					char aDate[32], aBuf[128];
					str_timestamp_ex((time_t)(time_timestamp() - (int64_t)(NumDays - 1 - i) * 86400), aDate, sizeof(aDate), "%d/%m");
					str_format(aBuf, sizeof(aBuf), DtLocalize("%s · %d real · %d practice"), aDate, Day.m_Real, Day.m_Practice);
					HoverText = aBuf;
					HoverPos = Mouse;
				}
			}
			SmallText(Axis.x, Axis.y + 1.0f, DtLocalize("30 days ago"), CHART_GREY);
			SmallText(Axis.x + Axis.w, Axis.y + 1.0f, DtLocalize("today"), CHART_GREY, true);
			char aMax[32];
			str_format(aMax, sizeof(aMax), "%d", MaxCount);
			SmallText(Plot.x + Plot.w, Plot.y - SmallFont - 2.0f, aMax, CHART_GREY, true);
		}
	}

	// --- hours: weekday x hour of the day ---
	{
		CUIRect Box, Plot;
		Content.HSplitTop(170.0f, &Box, &Content);
		Content.HSplitTop(Gap, nullptr, &Content);
		if(s_ScrollRegion.AddRect(Box))
		{
			ChartBox(Box, m_SplitsHeatmapRecords ? DtLocalize("When you beat your segments (weekday × hour)") : DtLocalize("When you play (weekday × hour)"), &Plot);
			CUIRect Toggle = {Box.x + Box.w - 170.0f, Box.y + 5.0f, 165.0f, 16.0f};
			static int s_HeatToggle;
			if(DoButton_CheckBox(&s_HeatToggle, DtLocalize("New bests"), m_SplitsHeatmapRecords, &Toggle))
				m_SplitsHeatmapRecords = !m_SplitsHeatmapRecords;
			const auto Heat = DdnettySplits::CSplitsData::Heatmap(Analysis, LocalUtcOffset());
			const auto &aaValues = m_SplitsHeatmapRecords ? Heat.m_aaRecords : Heat.m_aaAttempts;
			int MaxValue = 1;
			for(const auto &aDay : aaValues)
				for(int Value : aDay)
					MaxValue = std::max(MaxValue, Value);
			CUIRect Labels, Hours;
			Plot.VSplitLeft(30.0f, &Labels, &Plot);
			Plot.HSplitBottom(12.0f, &Plot, &Hours);
			const char *apDays[7] = {DtLocalize("Sun"), DtLocalize("Mon"), DtLocalize("Tue"), DtLocalize("Wed"), DtLocalize("Thu"), DtLocalize("Fri"), DtLocalize("Sat")};
			const float CellW = Plot.w / 24.0f;
			const float CellH = Plot.h / 7.0f;
			for(int Day = 0; Day < 7; Day++)
			{
				// monday first
				const int Row = (Day + 6) % 7;
				SmallText(Labels.x, Plot.y + CellH * Row + CellH / 2.0f - SmallFont / 2.0f, apDays[Day], CHART_GREY);
				for(int Hour = 0; Hour < 24; Hour++)
				{
					const int Value = aaValues[Day][Hour];
					CUIRect Cell = {Plot.x + CellW * Hour + 0.5f, Plot.y + CellH * Row + 0.5f, CellW - 1.0f, CellH - 1.0f};
					const float T = Value / (float)MaxValue;
					Cell.Draw(Value > 0 ? MixColor(CHART_BLUE.WithAlpha(0.25f), m_SplitsHeatmapRecords ? CHART_GREEN : CHART_ORANGE, T) : ColorRGBA(1.0f, 1.0f, 1.0f, 0.03f), IGraphics::CORNER_NONE, 0.0f);
					if(Ui()->MouseHovered(&Cell))
					{
						char aBuf[128];
						str_format(aBuf, sizeof(aBuf), DtLocalize("%s %02d:00 · %d attempts · %d new bests"), apDays[Day], Hour, Heat.m_aaAttempts[Day][Hour], Heat.m_aaRecords[Day][Hour]);
						HoverText = aBuf;
						HoverPos = Mouse;
					}
				}
			}
			for(int Hour = 0; Hour < 24; Hour += 3)
			{
				char aHour[8];
				str_format(aHour, sizeof(aHour), "%02d", Hour);
				SmallText(Plot.x + CellW * Hour, Hours.y + 1.0f, aHour, CHART_GREY);
			}
		}
	}

	s_ScrollRegion.End();

	// tooltip
	if(!HoverText.empty())
	{
		const float W = TextRender()->TextWidth(SmallFont + 1.0f, HoverText.c_str()) + 10.0f;
		CUIRect Tip = {HoverPos.x + 8.0f, HoverPos.y - 22.0f, W, 16.0f};
		const CUIRect *pScreen = Ui()->Screen();
		Tip.x = std::min(Tip.x, pScreen->x + pScreen->w - W - 2.0f);
		Tip.y = std::max(Tip.y, pScreen->y + 2.0f);
		Tip.Draw(ColorRGBA(0.05f, 0.05f, 0.08f, 0.92f), IGraphics::CORNER_ALL, 3.0f);
		CUIRect TipText;
		Tip.VMargin(5.0f, &TipText);
		Ui()->DoLabel(&TipText, HoverText.c_str(), SmallFont + 1.0f, TEXTALIGN_ML);
	}
	(void)Options;
}
