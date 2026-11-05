#include <game/ddnetty_splits.h>

#include <gtest/gtest.h>

using namespace DdnettySplits;

namespace
{
	SAttempt Att(int RunId, int64_t Timestamp, int From, int To, int Millis, bool Practice = false)
	{
		SAttempt Attempt;
		Attempt.m_RunId = RunId;
		Attempt.m_Timestamp = Timestamp;
		Attempt.m_From = From;
		Attempt.m_To = To;
		Attempt.m_Millis = Millis;
		Attempt.m_Practice = Practice;
		return Attempt;
	}

	const int G1 = GatePoint(1);
} // namespace

TEST(DdnettySplits, PointStrings)
{
	EXPECT_EQ(PointToString(POINT_START), "S");
	EXPECT_EQ(PointToString(POINT_FINISH), "F");
	EXPECT_EQ(PointToString(3), "T3");
	EXPECT_EQ(PointToString(G1), "G1");
	EXPECT_EQ(PointFromString("S"), POINT_START);
	EXPECT_EQ(PointFromString("F"), POINT_FINISH);
	EXPECT_EQ(PointFromString("T24"), 24);
	EXPECT_EQ(PointFromString("T25"), POINT_NONE);
	EXPECT_EQ(PointFromString("G7"), GatePoint(7));
	EXPECT_EQ(PointFromString("X"), POINT_NONE);
	EXPECT_EQ(PointFromString("G"), POINT_NONE);
	EXPECT_EQ(PointFromString("T1a"), POINT_NONE);
}

TEST(DdnettySplits, SerializeRoundTrip)
{
	CSplitsData Data;
	SGate Gate;
	Gate.m_Id = 1;
	Gate.m_Ax = 32.0f;
	Gate.m_Ay = 64.5f;
	Gate.m_Bx = 32.0f;
	Gate.m_By = 160.0f;
	Gate.m_Name = "big jump";
	Data.m_vGates.push_back(Gate);
	Data.m_vAttempts.push_back(Att(1, 1000, POINT_START, G1, 1500));
	Data.m_vAttempts.push_back(Att(1, 1000, G1, POINT_FINISH, 2500, true));

	CSplitsData Loaded;
	const std::string Text = Data.Serialize();
	size_t Pos = 0;
	while(Pos < Text.size())
	{
		size_t End = Text.find('\n', Pos);
		EXPECT_TRUE(Loaded.ParseLine(Text.substr(Pos, End - Pos).c_str()));
		Pos = End + 1;
	}
	ASSERT_EQ(Loaded.m_vGates.size(), 1u);
	EXPECT_EQ(Loaded.m_vGates[0].m_Name, "big jump");
	EXPECT_FLOAT_EQ(Loaded.m_vGates[0].m_Ay, 64.5f);
	ASSERT_EQ(Loaded.m_vAttempts.size(), 2u);
	EXPECT_EQ(Loaded.m_vAttempts[1].m_From, G1);
	EXPECT_EQ(Loaded.m_vAttempts[1].m_Millis, 2500);
	EXPECT_TRUE(Loaded.m_vAttempts[1].m_Practice);

	EXPECT_FALSE(Loaded.ParseLine("att 1 2 0 S S 10")); // same point
	EXPECT_FALSE(Loaded.ParseLine("att x 2 0 S F 10"));
	EXPECT_FALSE(Loaded.ParseLine("gate 1 0 0 1 1")); // duplicate id
	EXPECT_TRUE(Loaded.ParseLine("# comment"));
	EXPECT_TRUE(Loaded.ParseLine(""));
}

TEST(DdnettySplits, BuiltRunAndPrediction)
{
	CSplitsData Data;
	// run 1: S -> T0 -> F in 10 + 20
	Data.m_vAttempts.push_back(Att(1, 100, POINT_START, 0, 10000));
	Data.m_vAttempts.push_back(Att(1, 100, 0, POINT_FINISH, 20000));
	// run 2: S -> T0 -> F in 12 + 18
	Data.m_vAttempts.push_back(Att(2, 200, POINT_START, 0, 12000));
	Data.m_vAttempts.push_back(Att(2, 200, 0, POINT_FINISH, 18000));

	SOptions Options;
	SAnalysis Analysis = Data.Analyze(Options);
	ASSERT_TRUE(Analysis.m_RouteComplete);
	EXPECT_EQ(Analysis.m_vRoute, (std::vector<int>{POINT_START, 0, POINT_FINISH}));
	EXPECT_EQ(Analysis.m_BuiltRun, 28000); // best of each segment
	EXPECT_EQ(Analysis.m_BestFinished, 30000);
	EXPECT_EQ(CSplitsData::Remaining(Analysis, Analysis.RouteIndex(0), Options), 18000);
	EXPECT_EQ(CSplitsData::Remaining(Analysis, Analysis.RouteIndex(POINT_FINISH), Options), 0);

	Options.m_UseAverage = true;
	Analysis = Data.Analyze(Options);
	EXPECT_EQ(Analysis.m_BuiltRun, 11000 + 19000);
	const SSegmentStats *pStats = Analysis.Find(POINT_START, 0);
	ASSERT_NE(pStats, nullptr);
	EXPECT_EQ(pStats->m_Count, 2);
	EXPECT_EQ(pStats->m_StdDev, 1000);
	EXPECT_EQ(pStats->m_Last, 12000);
}

TEST(DdnettySplits, SourceFilterMergesPoints)
{
	CSplitsData Data;
	Data.m_vAttempts.push_back(Att(1, 100, POINT_START, 0, 1000));
	Data.m_vAttempts.push_back(Att(1, 100, 0, G1, 2000));
	Data.m_vAttempts.push_back(Att(1, 100, G1, POINT_FINISH, 3000));

	SOptions OnlyGates;
	OnlyGates.m_UseTimecps = false;
	SAnalysis Analysis = Data.Analyze(OnlyGates);
	EXPECT_EQ(Analysis.m_vRoute, (std::vector<int>{POINT_START, G1, POINT_FINISH}));
	ASSERT_NE(Analysis.Find(POINT_START, G1), nullptr);
	EXPECT_EQ(Analysis.Find(POINT_START, G1)->m_Best, 3000);

	SOptions OnlyTimecps;
	OnlyTimecps.m_UseGates = false;
	Analysis = Data.Analyze(OnlyTimecps);
	ASSERT_NE(Analysis.Find(0, POINT_FINISH), nullptr);
	EXPECT_EQ(Analysis.Find(0, POINT_FINISH)->m_Best, 5000);
}

TEST(DdnettySplits, PracticeFilterAndPartialRuns)
{
	CSplitsData Data;
	// practice-only repetitions of one segment, starting mid map
	Data.m_vAttempts.push_back(Att(1, 100, 2, 3, 5000, true));
	Data.m_vAttempts.push_back(Att(2, 110, 2, 3, 4000, true));

	SOptions Options;
	SAnalysis Analysis = Data.Analyze(Options);
	EXPECT_FALSE(Analysis.m_RouteComplete);
	EXPECT_EQ(Analysis.m_BuiltRun, -1);
	ASSERT_NE(Analysis.Find(2, 3), nullptr);
	EXPECT_EQ(Analysis.Find(2, 3)->m_Best, 4000);

	Options.m_CountPractice = false;
	Analysis = Data.Analyze(Options);
	EXPECT_EQ(Analysis.Find(2, 3), nullptr);

	const std::vector<SRun> vRuns = Data.Runs();
	ASSERT_EQ(vRuns.size(), 2u);
	EXPECT_TRUE(vRuns[0].m_Practice);
	EXPECT_FALSE(vRuns[0].Finished());
}

TEST(DdnettySplits, ForgetPointMergesAround)
{
	CSplitsData Data;
	SGate Gate;
	Gate.m_Id = 1;
	Data.m_vGates.push_back(Gate);
	Gate.m_Id = 2;
	Data.m_vGates.push_back(Gate); // another checkpoint stays, so deleting gate 1 merges
	Data.m_vAttempts.push_back(Att(1, 100, POINT_START, G1, 1000));
	Data.m_vAttempts.push_back(Att(1, 100, G1, POINT_FINISH, 2000));
	Data.m_vAttempts.push_back(Att(2, 200, G1, POINT_FINISH, 1500)); // started at the gate

	EXPECT_EQ(Data.GateDeleteCount(1), 1); // run 2 started at the gate: nothing to merge it with
	Data.DeleteGate(1);
	ASSERT_EQ(Data.m_vGates.size(), 1u);
	ASSERT_EQ(Data.m_vAttempts.size(), 1u);
	EXPECT_EQ(Data.m_vAttempts[0].m_From, POINT_START);
	EXPECT_EQ(Data.m_vAttempts[0].m_To, POINT_FINISH);
	EXPECT_EQ(Data.m_vAttempts[0].m_Millis, 3000);
	EXPECT_TRUE(Data.m_vAttempts[0].m_FromGate);
	EXPECT_NE(CSplitsData::FormatAttempt(Data.m_vAttempts[0]).find(" g"), std::string::npos);
}

TEST(DdnettySplits, DeletingGatesKeepsTimecps)
{
	const int G2 = GatePoint(2);
	CSplitsData Data;
	SGate Gate;
	Gate.m_Id = 1;
	Data.m_vGates.push_back(Gate);
	Gate.m_Id = 2;
	Data.m_vGates.push_back(Gate);
	// run 1: checkpoints between the map's time checkpoints
	Data.m_vAttempts.push_back(Att(1, 100, POINT_START, 0, 1000));
	Data.m_vAttempts.push_back(Att(1, 100, 0, G1, 1000));
	Data.m_vAttempts.push_back(Att(1, 100, G1, 1, 1000));
	Data.m_vAttempts.push_back(Att(1, 100, 1, G2, 1000));
	Data.m_vAttempts.push_back(Att(1, 100, G2, POINT_FINISH, 1000));
	// run 2 never touched a checkpoint
	Data.m_vAttempts.push_back(Att(2, 200, POINT_START, 0, 1500));
	Data.m_vAttempts.push_back(Att(2, 200, 0, 1, 2500));
	Data.m_vAttempts.push_back(Att(2, 200, 1, POINT_FINISH, 900));

	EXPECT_EQ(Data.GateDeleteCount(1), 0);
	Data.DeleteGate(1);
	ASSERT_EQ(Data.m_vGates.size(), 1u);
	EXPECT_EQ(Data.GateDeleteCount(2), 0);
	EXPECT_EQ(Data.AllGatesDeleteCount(), 0);
	Data.DeleteGate(2); // the last one

	EXPECT_TRUE(Data.m_vGates.empty());
	const SAnalysis Analysis = Data.Analyze(SOptions());
	ASSERT_NE(Analysis.Find(0, 1), nullptr);
	EXPECT_EQ(Analysis.Find(0, 1)->m_Count, 2);
	EXPECT_EQ(Analysis.Find(0, 1)->m_Best, 2000);
	ASSERT_NE(Analysis.Find(1, POINT_FINISH), nullptr);
	EXPECT_EQ(Analysis.Find(1, POINT_FINISH)->m_Count, 2);
	const std::vector<SRun> vRuns = Data.Runs();
	ASSERT_EQ(vRuns.size(), 2u);
	EXPECT_TRUE(vRuns[0].Finished());
	EXPECT_EQ(vRuns[0].m_Millis, 5000);

	// "delete all" merges the same way, and old files without the mark still load
	CSplitsData All;
	All.m_vGates.push_back(Gate);
	All.m_vAttempts.push_back(Att(1, 100, POINT_START, 0, 1000));
	All.m_vAttempts.push_back(Att(1, 100, 0, G2, 1000));
	All.m_vAttempts.push_back(Att(1, 100, G2, 0, 500)); // went back through the time checkpoint
	All.m_vAttempts.push_back(Att(1, 100, 0, POINT_FINISH, 1000));
	All.m_vAttempts.push_back(Att(2, 200, G2, POINT_FINISH, 700)); // started at the checkpoint
	EXPECT_EQ(All.AllGatesDeleteCount(), 1);
	All.DeleteAllGates();
	ASSERT_EQ(All.m_vAttempts.size(), 2u);
	EXPECT_EQ(All.m_vAttempts[1].m_From, 0);
	EXPECT_EQ(All.m_vAttempts[1].m_To, POINT_FINISH);
	EXPECT_EQ(All.Runs().size(), 1u);

	CSplitsData Old;
	EXPECT_TRUE(Old.ParseLine("att 1 100 0 S F 3000"));
	EXPECT_FALSE(Old.m_vAttempts[0].m_FromGate);
	EXPECT_TRUE(Old.ParseLine("att 2 100 0 S F 3000 g"));
	EXPECT_TRUE(Old.m_vAttempts[1].m_FromGate);
	EXPECT_TRUE(Old.ParseLine("att 3 100 0 S T1 0")); // two points crossed at the same moment
}

TEST(DdnettySplits, DeleteSegmentSplitsRun)
{
	CSplitsData Data;
	Data.m_vAttempts.push_back(Att(1, 100, POINT_START, 0, 1000));
	Data.m_vAttempts.push_back(Att(1, 100, 0, 1, 2000));
	Data.m_vAttempts.push_back(Att(1, 100, 1, POINT_FINISH, 3000));

	Data.DeleteSegment(0, 1, SOptions());
	ASSERT_EQ(Data.m_vAttempts.size(), 2u);
	const std::vector<SRun> vRuns = Data.Runs();
	ASSERT_EQ(vRuns.size(), 2u);
	EXPECT_NE(vRuns[0].m_RunId, vRuns[1].m_RunId);
	EXPECT_EQ(vRuns[1].m_From, 1);

	Data.DeleteRuns({vRuns[0].m_RunId});
	ASSERT_EQ(Data.m_vAttempts.size(), 1u);
	EXPECT_EQ(Data.m_vAttempts[0].m_From, 1);
}

TEST(DdnettySplits, RouteFollowsLatestCompleteRun)
{
	CSplitsData Data;
	Data.m_vAttempts.push_back(Att(1, 100, POINT_START, 0, 1000));
	Data.m_vAttempts.push_back(Att(1, 100, 0, POINT_FINISH, 1000));
	// newer run that skips T0 and uses T1
	Data.m_vAttempts.push_back(Att(2, 200, POINT_START, 1, 1500));
	Data.m_vAttempts.push_back(Att(2, 200, 1, POINT_FINISH, 1500));

	const SAnalysis Analysis = Data.Analyze(SOptions());
	EXPECT_EQ(Analysis.m_vRoute, (std::vector<int>{POINT_START, 1, POINT_FINISH}));
	EXPECT_EQ(Analysis.m_vOtherStats.size(), 2u);
}

TEST(DdnettySplits, RepeatedPointUsesRoutePosition)
{
	// circular map: the gate is crossed twice, S -> G1 -> T0 -> G1 -> F
	CSplitsData Data;
	Data.m_vAttempts.push_back(Att(1, 100, POINT_START, G1, 1000));
	Data.m_vAttempts.push_back(Att(1, 100, G1, 0, 2000));
	Data.m_vAttempts.push_back(Att(1, 100, 0, G1, 3000));
	Data.m_vAttempts.push_back(Att(1, 100, G1, POINT_FINISH, 4000));

	const SOptions Options;
	const SAnalysis Analysis = Data.Analyze(Options);
	ASSERT_TRUE(Analysis.m_RouteComplete);
	EXPECT_EQ(Analysis.m_vRoute, (std::vector<int>{POINT_START, G1, 0, G1, POINT_FINISH}));
	EXPECT_EQ(Analysis.m_BuiltRun, 10000);

	const int First = Analysis.RouteIndex(G1);
	const int Second = Analysis.RouteIndex(G1, First + 1);
	EXPECT_EQ(First, 1);
	EXPECT_EQ(Second, 3);
	EXPECT_EQ(CSplitsData::Remaining(Analysis, First, Options), 9000);
	EXPECT_EQ(CSplitsData::Remaining(Analysis, Second, Options), 4000);
	EXPECT_EQ(CSplitsData::Remaining(Analysis, -1, Options), -1);
}

TEST(DdnettySplits, TargetTime)
{
	CSplitsData Data;
	EXPECT_TRUE(Data.ParseLine("target 581020"));
	EXPECT_EQ(Data.m_TargetMillis, 581020);
	EXPECT_NE(Data.Serialize().find("target 581020\n"), std::string::npos);
	EXPECT_FALSE(Data.ParseLine("target abc"));
	Data.Clear();
	EXPECT_EQ(Data.m_TargetMillis, -1);
	EXPECT_EQ(Data.Serialize().find("target"), std::string::npos);
}

TEST(DdnettySplits, ChartData)
{
	CSplitsData Data;
	const int64_t Day = 86400;
	const int64_t Now = 100 * Day + 3600;
	// two finished runs and one practice part, on different days
	Data.m_vAttempts.push_back(Att(1, Now - 2 * Day, POINT_START, 0, 10000));
	Data.m_vAttempts.push_back(Att(1, Now - 2 * Day, 0, POINT_FINISH, 20000));
	Data.m_vAttempts.push_back(Att(2, Now, POINT_START, 0, 12000));
	Data.m_vAttempts.push_back(Att(2, Now, 0, POINT_FINISH, 16000));
	Data.m_vAttempts.push_back(Att(3, Now, 0, POINT_FINISH, 18000, true));

	const auto vRuns = Data.FinishedRuns(true);
	ASSERT_EQ(vRuns.size(), 2u);
	EXPECT_EQ(vRuns[0].m_Millis, 30000);
	EXPECT_EQ(vRuns[1].m_Millis, 28000);

	const SOptions Options;
	const SAnalysis Analysis = Data.Analyze(Options);
	const auto vLosses = CSplitsData::Losses(Analysis);
	ASSERT_EQ(vLosses.size(), 2u);
	EXPECT_EQ(vLosses[0].m_From, 0); // T0 -> F: best 16, mean 18 -> loses 2s
	EXPECT_EQ(vLosses[0].m_Loss, 2000);
	EXPECT_EQ(vLosses[1].m_Loss, 1000);

	const auto vHistory = CSplitsData::SegmentHistory(Analysis, 0, POINT_FINISH);
	ASSERT_EQ(vHistory.size(), 3u);
	EXPECT_TRUE(vHistory[0].m_NewBest);
	EXPECT_TRUE(vHistory[1].m_NewBest);
	EXPECT_FALSE(vHistory[2].m_NewBest);
	EXPECT_TRUE(vHistory[2].m_Practice);
	EXPECT_EQ(vHistory[2].m_MovingAverage, 18000);

	const auto vDays = Data.Activity(Now, 0, 7);
	ASSERT_EQ(vDays.size(), 7u);
	EXPECT_EQ(vDays[6].m_Real, 2);
	EXPECT_EQ(vDays[6].m_Practice, 1);
	EXPECT_EQ(vDays[4].m_Real, 2);
	EXPECT_EQ(vDays[5].m_Real, 0);

	const auto Summary = Data.Summarize(Analysis, true);
	EXPECT_EQ(Summary.m_FinishedRuns, 2);
	EXPECT_EQ(Summary.m_Attempts, 5);
	EXPECT_EQ(Summary.m_TotalMillis, 76000);
	EXPECT_EQ(Summary.m_WorstFrom, 0); // T0 -> F has 3 attempts
}

TEST(DdnettySplits, SegmentDemosFlag)
{
	CSplitsData Data;
	EXPECT_FALSE(Data.m_SegmentDemos);
	EXPECT_TRUE(Data.ParseLine("segdemos 1"));
	EXPECT_TRUE(Data.m_SegmentDemos);
	EXPECT_NE(Data.Serialize().find("segdemos 1\n"), std::string::npos);

	CSplitsData Copy;
	for(const char *pLine : {"# ddnetty splits v1", "segdemos 1"})
		EXPECT_TRUE(Copy.ParseLine(pLine));
	EXPECT_TRUE(Copy.m_SegmentDemos);

	EXPECT_TRUE(Data.ParseLine("segdemos 0"));
	EXPECT_FALSE(Data.m_SegmentDemos);
	EXPECT_EQ(Data.Serialize().find("segdemos"), std::string::npos);
	EXPECT_FALSE(Data.ParseLine("segdemos 2"));
	EXPECT_FALSE(Data.ParseLine("segdemos"));

	Data.ParseLine("segdemos 1");
	Data.Clear();
	EXPECT_FALSE(Data.m_SegmentDemos);
}

TEST(DdnettySplits, EntryFlagsRoundTrip)
{
	CSplitsData Data;
	SAttempt Attempt = Att(4, 1000, 2, 3, 900, true);
	Attempt.m_Flags = ATTFLAG_AFTER_BACK | ATTFLAG_DUMMY;
	Data.m_vAttempts.push_back(Attempt);
	Data.m_Since = 123456;
	Data.m_FocusMarginMillis = 2500;

	CSplitsData Loaded;
	const std::string Text = Data.Serialize();
	size_t Pos = 0;
	while(Pos < Text.size())
	{
		size_t End = Text.find('\n', Pos);
		EXPECT_TRUE(Loaded.ParseLine(Text.substr(Pos, End - Pos).c_str()));
		Pos = End == std::string::npos ? Text.size() : End + 1;
	}
	ASSERT_EQ(Loaded.m_vAttempts.size(), 1u);
	EXPECT_TRUE(Loaded.m_vAttempts[0].m_Practice);
	EXPECT_EQ(Loaded.m_vAttempts[0].m_Flags, ATTFLAG_AFTER_BACK | ATTFLAG_DUMMY);
	EXPECT_EQ(Loaded.m_Since, 123456);
	EXPECT_EQ(Loaded.m_FocusMarginMillis, 2500);

	// files without the new lines keep the defaults
	CSplitsData Old;
	EXPECT_TRUE(Old.ParseLine("att 1 1000 0 S F 5000"));
	EXPECT_EQ(Old.m_vAttempts[0].m_Flags, 0);
	EXPECT_EQ(Old.m_Since, 0);
	EXPECT_EQ(Old.m_FocusMarginMillis, -1);
	EXPECT_FALSE(Old.ParseLine("focus -5"));
}

TEST(DdnettySplits, SinceAndDummyFilters)
{
	CSplitsData Data;
	Data.m_vAttempts.push_back(Att(1, 100, POINT_START, POINT_FINISH, 5000));
	Data.m_vAttempts.push_back(Att(2, 200, POINT_START, POINT_FINISH, 6000));
	SAttempt Dummy = Att(3, 300, POINT_START, POINT_FINISH, 4000);
	Dummy.m_Flags = ATTFLAG_DUMMY;
	Data.m_vAttempts.push_back(Dummy);

	SOptions Options;
	EXPECT_EQ(Data.Analyze(Options).m_BestFinished, 4000);
	Options.m_CountDummy = false;
	EXPECT_EQ(Data.Analyze(Options).m_BestFinished, 5000);
	Options.m_Since = 150;
	EXPECT_EQ(Data.Analyze(Options).m_BestFinished, 6000);
}

TEST(DdnettySplits, FocusedRuns)
{
	// route S -> T0 -> F, best run 10 + 10
	CSplitsData Data;
	Data.m_vAttempts.push_back(Att(1, 100, POINT_START, 0, 10000));
	Data.m_vAttempts.push_back(Att(1, 100, 0, POINT_FINISH, 10000));
	// entered T0 2 s behind: focused with a 3 s margin
	Data.m_vAttempts.push_back(Att(2, 200, POINT_START, 0, 12000));
	Data.m_vAttempts.push_back(Att(2, 200, 0, POINT_FINISH, 11000));
	// entered T0 10 s behind: not focused
	Data.m_vAttempts.push_back(Att(3, 300, POINT_START, 0, 20000));
	Data.m_vAttempts.push_back(Att(3, 300, 0, POINT_FINISH, 15000));
	// started at T0 (practicing the part): loose entry, but a very fast time
	SAttempt Mid = Att(4, 400, 0, POINT_FINISH, 9000);
	Mid.m_Flags = ATTFLAG_MID_START;
	Data.m_vAttempts.push_back(Mid);

	SOptions Options;
	Options.m_FocusMarginMillis = 0;
	{
		const SAnalysis Analysis = Data.Analyze(Options);
		const SSegmentStats *pStats = Analysis.Find(0, POINT_FINISH);
		ASSERT_TRUE(pStats);
		EXPECT_EQ(pStats->m_Count, 4);
		EXPECT_EQ(pStats->m_Best, 9000);
		EXPECT_EQ(pStats->m_Focused, 3); // the mid start never counts for the average
		EXPECT_EQ(pStats->m_Mean, 12000);
		EXPECT_EQ(pStats->m_Last, 9000);
	}
	Options.m_FocusMarginMillis = 3000;
	{
		const SAnalysis Analysis = Data.Analyze(Options);
		const SSegmentStats *pStats = Analysis.Find(0, POINT_FINISH);
		ASSERT_TRUE(pStats);
		EXPECT_EQ(pStats->m_Focused, 2);
		EXPECT_EQ(pStats->m_Mean, 10500);
		EXPECT_EQ(pStats->m_Best, 9000);
		EXPECT_EQ(Analysis.m_BuiltRun, 10000 + 9000);
		// the first segment is always entered right at the start
		EXPECT_EQ(Analysis.Find(POINT_START, 0)->m_Focused, 3);
		// entry times
		ASSERT_EQ(Analysis.m_vAttempts.size(), 7u);
		EXPECT_EQ(Analysis.m_vAttempts[3].m_EntryMillis, 12000);
		EXPECT_EQ(Analysis.m_vAttempts[6].m_EntryMillis, -1);
	}
	Options.m_RunEntriesOnly = true;
	{
		const SAnalysis Analysis = Data.Analyze(Options);
		EXPECT_EQ(Analysis.Find(0, POINT_FINISH)->m_Best, 10000);
		EXPECT_EQ(Analysis.m_BuiltRun, 20000);
	}
}

TEST(DdnettySplits, EntryFlagsComeFromTheFirstPiece)
{
	// a gate inside the segment: with gates off the two pieces are merged,
	// how the segment was entered comes from the first one, the dummy flag from any
	CSplitsData Data;
	Data.m_vAttempts.push_back(Att(1, 100, POINT_START, 0, 1000));
	SAttempt A = Att(1, 100, 0, G1, 1000);
	SAttempt B = Att(1, 100, G1, POINT_FINISH, 1000);
	B.m_Flags = ATTFLAG_LONG_WAIT | ATTFLAG_SWITCHED;
	Data.m_vAttempts.push_back(A);
	Data.m_vAttempts.push_back(B);

	SOptions Options;
	Options.m_UseGates = false;
	const std::vector<SEffectiveAttempt> vAttempts = Data.EffectiveAttempts(Options);
	ASSERT_EQ(vAttempts.size(), 2u);
	EXPECT_EQ(vAttempts[1].m_Attempt.m_Flags, ATTFLAG_SWITCHED);
	Options.m_CountDummy = false;
	EXPECT_EQ(Data.EffectiveAttempts(Options).size(), 1u);
}

TEST(DdnettySplits, Refs)
{
	CSplitsRefs Refs;
	EXPECT_TRUE(Refs.ParseLine("# comment"));
	EXPECT_TRUE(Refs.ParseLine("ref 141440 2024-09-05 3 6740 21780 -1 Oak"));
	EXPECT_TRUE(Refs.ParseLine("ref 136480 2026-11-04 3 6563 20316 36012 1000"));
	EXPECT_TRUE(Refs.ParseLine("ref 146100 2023-08-01 0 Poop the second"));
	EXPECT_TRUE(Refs.ParseLine("segbest T6 T7 9060 2026-03-01 Poop"));
	EXPECT_TRUE(Refs.ParseLine("segbest T6 T7 9100 2026-03-02 slower"));
	EXPECT_FALSE(Refs.ParseLine("ref 141440 2024-09-05 3 6740 21780 Oak"));
	EXPECT_FALSE(Refs.ParseLine("segbest T6 T6 9060 2026-03-01 x"));
	EXPECT_FALSE(Refs.ParseLine("ref 100 2024-01-01 1 50"));

	ASSERT_EQ(Refs.m_vRuns.size(), 3u);
	EXPECT_EQ(Refs.m_vRuns[0].m_Name, "1000"); // a name made of digits, fastest first
	EXPECT_EQ(Refs.FindRun("")->m_Name, "1000");
	EXPECT_EQ(Refs.FindRun("Poop the second")->m_Millis, 146100);
	EXPECT_EQ(Refs.FindRun("nobody")->m_Name, "1000");

	const SRefRun *pOak = Refs.FindRun("Oak");
	ASSERT_TRUE(pOak);
	EXPECT_EQ(CSplitsRefs::TimeAt(*pOak, POINT_START), 0);
	EXPECT_EQ(CSplitsRefs::TimeAt(*pOak, 1), 21780);
	EXPECT_EQ(CSplitsRefs::TimeAt(*pOak, 2), -1);
	EXPECT_EQ(CSplitsRefs::TimeAt(*pOak, 5), -1);
	EXPECT_EQ(CSplitsRefs::TimeAt(*pOak, G1), -1);
	EXPECT_EQ(CSplitsRefs::TimeAt(*pOak, POINT_FINISH), 141440);

	ASSERT_EQ(Refs.m_vSegments.size(), 1u);
	EXPECT_EQ(Refs.FindSegment(6, 7)->m_Millis, 9060);
	EXPECT_EQ(Refs.FindSegment(6, 7)->m_Name, "Poop");
	EXPECT_EQ(Refs.FindSegment(7, 8), nullptr);
}

TEST(DdnettySplits, NewChartData)
{
	CSplitsData Data;
	// run 1: 10 + 10, run 2: 9 + 12, run 3 dies after the first segment (8), run 4: 11 + 9
	Data.m_vAttempts.push_back(Att(1, 100, POINT_START, 0, 10000));
	Data.m_vAttempts.push_back(Att(1, 100, 0, POINT_FINISH, 10000));
	Data.m_vAttempts.push_back(Att(2, 200, POINT_START, 0, 9000));
	Data.m_vAttempts.push_back(Att(2, 200, 0, POINT_FINISH, 12000));
	Data.m_vAttempts.push_back(Att(3, 300, POINT_START, 0, 8000));
	Data.m_vAttempts.push_back(Att(4, 400, POINT_START, 0, 11000));
	Data.m_vAttempts.push_back(Att(4, 400, 0, POINT_FINISH, 9000));
	SOptions Options;
	Options.m_FocusMarginMillis = 0;
	const SAnalysis Analysis = Data.Analyze(Options);

	const auto vIdeal = CSplitsData::IdealHistory(Analysis);
	ASSERT_EQ(vIdeal.size(), 4u);
	EXPECT_EQ(vIdeal[0].m_Millis, 20000);
	EXPECT_EQ(vIdeal[1].m_Millis, 19000);
	EXPECT_EQ(vIdeal[2].m_Millis, 18000);
	EXPECT_EQ(vIdeal[3].m_Millis, 17000);

	const auto vReach = CSplitsData::Reach(Analysis);
	ASSERT_EQ(vReach.size(), 3u);
	EXPECT_EQ(vReach[0], 4);
	EXPECT_EQ(vReach[1], 4);
	EXPECT_EQ(vReach[2], 3);

	const auto vTimes = CSplitsData::RouteTimes(Analysis, 2);
	ASSERT_EQ(vTimes.size(), 2u);
	EXPECT_EQ(vTimes[0].m_vAtRoute[1], 9000);
	EXPECT_EQ(vTimes[1].m_vAtRoute[1], 11000);
	EXPECT_EQ(vTimes[1].m_vAtRoute[2], 20000);

	const auto vGaps = CSplitsData::FocusGaps(Analysis);
	ASSERT_EQ(vGaps.size(), 2u);
	EXPECT_EQ(vGaps[0].m_Best, 8000);
	EXPECT_EQ(vGaps[0].m_MeanAll, 9500);

	// 1970-01-01 00:00 UTC was a Thursday
	const auto Heat = CSplitsData::Heatmap(Analysis, 0);
	EXPECT_EQ(Heat.m_aaAttempts[4][0], 7);
	EXPECT_EQ(Heat.m_aaRecords[4][0], 3); // 9.0, 8.0 and 9.0 beat the earlier bests

	const auto vHistory = CSplitsData::SegmentHistory(Analysis, POINT_START, 0);
	ASSERT_EQ(vHistory.size(), 4u);
	EXPECT_EQ(vHistory[1].m_MovingStdDev, 500);
}
