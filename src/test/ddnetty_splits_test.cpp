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
	EXPECT_EQ(CSplitsData::Remaining(Analysis, 0, Options), 18000);
	EXPECT_EQ(CSplitsData::Remaining(Analysis, POINT_FINISH, Options), 0);

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
	Data.m_vAttempts.push_back(Att(1, 100, POINT_START, G1, 1000));
	Data.m_vAttempts.push_back(Att(1, 100, G1, POINT_FINISH, 2000));
	Data.m_vAttempts.push_back(Att(2, 200, G1, POINT_FINISH, 1500)); // started at the gate

	Data.DeleteGate(1);
	EXPECT_TRUE(Data.m_vGates.empty());
	ASSERT_EQ(Data.m_vAttempts.size(), 1u);
	EXPECT_EQ(Data.m_vAttempts[0].m_From, POINT_START);
	EXPECT_EQ(Data.m_vAttempts[0].m_To, POINT_FINISH);
	EXPECT_EQ(Data.m_vAttempts[0].m_Millis, 3000);
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
