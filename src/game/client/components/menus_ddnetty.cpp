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

	const DdnettySplits::SAnalysis &Analysis = Splits.Analysis();

	// summary
	int PlayerBest = Splits.PlayerBestMillis();
	if(PlayerBest < 0)
		PlayerBest = Analysis.m_BestFinished;
	char aRecord[32], aBest[32], aBuilt[32], aGain[32], aLine[256];
	CSplits::FormatMillis(Splits.RecordMillis(), aRecord, sizeof(aRecord));
	CSplits::FormatMillis(PlayerBest, aBest, sizeof(aBest));
	CSplits::FormatMillis(Analysis.m_BuiltRun, aBuilt, sizeof(aBuilt));
	if(PlayerBest >= 0 && Analysis.m_BuiltRun >= 0 && PlayerBest > Analysis.m_BuiltRun)
		CSplits::FormatMillis(PlayerBest - Analysis.m_BuiltRun, aGain, sizeof(aGain));
	else
		str_copy(aGain, "—");
	str_format(aLine, sizeof(aLine), DtLocalize("Record: %s   Your best: %s   Built run: %s   Possible gain: %s"), aRecord, aBest, aBuilt, aGain);
	MainView.HSplitTop(LineSize, &Label, &MainView);
	Ui()->DoLabel(&Label, aLine, FontSize, TEXTALIGN_ML);
	if(Analysis.m_BuiltRun < 0)
	{
		MainView.HSplitTop(LineSize - 4.0f, &Label, &MainView);
		SLabelProperties HintProps;
		HintProps.SetColor(ColorRGBA(1.0f, 1.0f, 1.0f, 0.6f));
		Ui()->DoLabel(&Label, Analysis.m_RouteComplete ? DtLocalize("The built run needs at least one attempt of every segment.") : DtLocalize("The built run needs one complete run from start to finish."), 11.0f, TEXTALIGN_ML, HintProps);
	}
	MainView.HSplitTop(Spacing, nullptr, &MainView);

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

	// options
	MainView.HSplitTop(LineSize, &Row, &MainView);
	Row.VSplitLeft(115.0f, &Label, &Row);
	Ui()->DoLabel(&Label, DtLocalize("Split points:"), FontSize, TEXTALIGN_ML);
	static CButtonContainer s_aSourceButtons[3];
	const char *apSources[] = {DtLocalize("Map + mine"), DtLocalize("Map time checkpoints"), DtLocalize("My checkpoints")};
	g_Config.m_DtSplitsSource = DoChoices(&Row, s_aSourceButtons, apSources, 3, g_Config.m_DtSplitsSource, 150.0f);

	MainView.HSplitTop(Spacing, nullptr, &MainView);
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

	// views
	MainView.HSplitTop(Spacing * 2.0f, nullptr, &MainView);
	MainView.HSplitTop(LineSize, &Row, &MainView);
	static CButtonContainer s_aViewButtons[NUM_SPLITS_VIEWS];
	const char *apViews[] = {DtLocalize("Segments"), DtLocalize("Runs"), DtLocalize("Checkpoints")};
	m_SplitsView = DoChoices(&Row, s_aViewButtons, apViews, NUM_SPLITS_VIEWS, m_SplitsView, 130.0f);
	MainView.HSplitTop(Spacing, nullptr, &MainView);

	if(m_SplitsView == SPLITS_VIEW_SEGMENTS)
		RenderSplitsSegments(MainView);
	else if(m_SplitsView == SPLITS_VIEW_RUNS)
		RenderSplitsRuns(MainView);
	else
		RenderSplitsGates(MainView);
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
			str_format(aMessage, sizeof(aMessage), DtLocalize("Delete the checkpoint %s? Its segments are merged into the neighbouring ones."), Splits.PointName(DdnettySplits::GatePoint(m_SplitsSelectedGate)).c_str());
			PopupConfirm(DtLocalize("Delete checkpoint"), aMessage, Localize("Yes"), Localize("No"), &CMenus::PopupConfirmSplitsDelete);
		}
	}
	else
	{
		SLabelProperties HintProps;
		HintProps.SetColor(ColorRGBA(1.0f, 1.0f, 1.0f, 0.6f));
		Ui()->DoLabel(&Footer, DtLocalize("Click a checkpoint to rename or delete it."), FontSize, TEXTALIGN_ML, HintProps);
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
	else if(m_SplitsPendingDelete == SPLITS_DELETE_GATE)
	{
		Splits.DeleteGate(m_SplitsPendingGate);
		m_SplitsSelectedGate = -1;
	}
	m_SplitsPendingDelete = SPLITS_DELETE_NONE;
	m_vSplitsPendingRuns.clear();
	m_vSplitsSelectedRuns.clear();
}
