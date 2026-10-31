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

	// target time for the prediction
	MainView.HSplitTop(Spacing, nullptr, &MainView);
	MainView.HSplitTop(LineSize, &Row, &MainView);
	Row.VSplitLeft(115.0f, &Label, &Row);
	Ui()->DoLabel(&Label, DtLocalize("Target time:"), FontSize, TEXTALIGN_ML);
	static CButtonContainer s_aTargetButtons[3];
	const char *apTargets[] = {DtLocalize("Server record"), DtLocalize("My best"), DtLocalize("Custom")};
	g_Config.m_DtSplitsTargetMode = DoChoices(&Row, s_aTargetButtons, apTargets, 3, g_Config.m_DtSplitsTargetMode, 120.0f);
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

	// columns: segment (flexible), best, average, variation, last, attempts, clear button
	const float aWidths[] = {90.0f, 90.0f, 80.0f, 90.0f, 75.0f, 70.0f};
	auto &&SplitColumns = [&](CUIRect Row, CUIRect *pCols) {
		float Fixed = 0.0f;
		for(float Width : aWidths)
			Fixed += Width;
		Row.VSplitLeft(std::max(Row.w - Fixed, 50.0f), &pCols[0], &Row);
		for(int i = 0; i < (int)std::size(aWidths); i++)
			Row.VSplitLeft(aWidths[i], &pCols[i + 1], &Row);
	};

	CUIRect Headers, aCols[7];
	View.HSplitTop(RowHeight, &Headers, &View);
	Headers.Draw(ColorRGBA(1.0f, 1.0f, 1.0f, 0.25f), IGraphics::CORNER_T, 5.0f);
	Headers.VSplitLeft(5.0f, nullptr, &Headers);
	Headers.VSplitRight(20.0f, &Headers, nullptr); // scrollbar
	SplitColumns(Headers, aCols);
	const char *apCaptions[] = {DtLocalize("Segment"), DtLocalize("Best"), DtLocalize("Average"), DtLocalize("Variation"), DtLocalize("Last"), DtLocalize("Attempts")};
	for(int i = 0; i < 6; i++)
		Ui()->DoLabel(&aCols[i], apCaptions[i], FontSize, TEXTALIGN_ML);

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

		if(Stats.m_Count > 0)
		{
			CUIRect ClearButton;
			aCols[6].Margin(1.0f, &ClearButton);
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
				str_format(aMessage, sizeof(aMessage), DtLocalize("Delete %s? It's your last checkpoint, so the %d times that came from your checkpoints are deleted too."), Splits.PointName(DdnettySplits::GatePoint(m_SplitsSelectedGate)).c_str(), Count);
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
		str_format(aMessage, sizeof(aMessage), DtLocalize("Delete all %d checkpoints and the %d times that came from them? Times from the map's time checkpoints stay."), (int)vGates.size(), Splits.Data().AllGatesDeleteCount());
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
