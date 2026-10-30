// ddnetty: data and math for the Splits feature.
// Kept free of client dependencies so it can be unit tested.
#ifndef GAME_DDNETTY_SPLITS_H
#define GAME_DDNETTY_SPLITS_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace DdnettySplits
{

	// A "point" is anything a run can be split at.
	enum
	{
		POINT_NONE = -1,
		// 0 .. NUM_TIMECPS-1 are the map's time checkpoint tiles
		NUM_TIMECPS = 25,
		POINT_START = 10000,
		POINT_FINISH = 10001,
		// POINT_GATE_BASE + id are the player's own checkpoints (gates)
		POINT_GATE_BASE = 20000,
	};

	inline bool IsTimecp(int Point) { return Point >= 0 && Point < NUM_TIMECPS; }
	inline bool IsGate(int Point) { return Point >= POINT_GATE_BASE; }
	inline int GatePoint(int GateId) { return POINT_GATE_BASE + GateId; }
	inline int GateIdOf(int Point) { return Point - POINT_GATE_BASE; }

	// "S", "F", "T<cp>", "G<id>"
	std::string PointToString(int Point);
	int PointFromString(const char *pStr); // POINT_NONE if invalid

	// a line segment the player placed on the map
	struct SGate
	{
		int m_Id = 0;
		float m_Ax = 0.0f;
		float m_Ay = 0.0f;
		float m_Bx = 0.0f;
		float m_By = 0.0f;
		std::string m_Name;
	};

	// one timed segment between two consecutive points of a continuous run
	struct SAttempt
	{
		int m_RunId = 0;
		int64_t m_Timestamp = 0; // unix seconds
		bool m_Practice = false;
		int m_From = POINT_NONE;
		int m_To = POINT_NONE;
		int m_Millis = 0;
		bool m_FromGate = false; // made by merging around a deleted or moved checkpoint
	};

	struct SOptions
	{
		bool m_UseTimecps = true;
		bool m_UseGates = true;
		bool m_CountPractice = true;
		bool m_UseAverage = false; // reference = mean of the recent attempts instead of the best
	};

	struct SSegmentStats
	{
		int m_From = POINT_NONE;
		int m_To = POINT_NONE;
		int m_Count = 0;
		int m_Best = -1;
		int m_Last = -1;
		int m_Mean = -1; // mean of the last AVERAGE_WINDOW attempts
		int m_StdDev = -1; // standard deviation of the same window
		int64_t m_LastTimestamp = 0;
	};

	// a continuous sequence of attempts (no death or teleport in between)
	struct SRun
	{
		int m_RunId = 0;
		int64_t m_Timestamp = 0;
		bool m_Practice = false;
		int m_From = POINT_NONE;
		int m_To = POINT_NONE;
		int m_Millis = 0;
		int m_NumSegments = 0;
		bool Finished() const { return m_From == POINT_START && m_To == POINT_FINISH; }
	};

	// an attempt between two points that are both used by the current options;
	// points that aren't used are merged away
	struct SEffectiveAttempt
	{
		SAttempt m_Attempt;
		std::vector<size_t> m_vRawIndices;
	};

	struct SAnalysis
	{
		std::vector<SEffectiveAttempt> m_vAttempts;
		std::vector<int> m_vRoute; // points from start to finish (may be partial)
		bool m_RouteComplete = false; // route reaches the finish
		std::vector<SSegmentStats> m_vRouteStats; // one per consecutive pair of m_vRoute
		std::vector<SSegmentStats> m_vOtherStats; // segments that aren't on the route
		int m_BuiltRun = -1; // sum of the references along the route, -1 if incomplete
		int m_BuiltRunMissing = 0; // segments of the route without data
		int m_BestFinished = -1; // fastest complete non-practice run, -1 if none

		const SSegmentStats *Find(int From, int To) const;
		// first position of Point on the route at or after FromIndex, -1 if not found
		int RouteIndex(int Point, int FromIndex = 0) const;
	};

	class CSplitsData
	{
	public:
		static constexpr int AVERAGE_WINDOW = 10;

		std::vector<SGate> m_vGates;
		std::vector<SAttempt> m_vAttempts;
		int m_TargetMillis = -1; // custom target time for this map, -1 if not set

		void Clear();

		// file format: one "target ...", "gate ..." or "att ..." per line, "#" starts a comment
		bool ParseLine(const char *pLine);
		static std::string FormatGate(const SGate &Gate);
		static std::string FormatAttempt(const SAttempt &Attempt);
		std::string Serialize() const;

		int NextRunId() const;
		int NextGateId() const;
		const SGate *FindGate(int GateId) const;
		SGate *FindGate(int GateId);

		static bool Includes(int Point, const SOptions &Options);
		static int Reference(const SSegmentStats &Stats, const SOptions &Options);

		std::vector<SEffectiveAttempt> EffectiveAttempts(const SOptions &Options) const;
		SAnalysis Analyze(const SOptions &Options) const;
		// sum of the references from the route position RouteIndex to the finish, -1 if unknown
		static int Remaining(const SAnalysis &Analysis, int RouteIndex, const SOptions &Options);

		std::vector<SRun> Runs() const;

		void DeleteRuns(const std::vector<int> &vRunIds);
		void DeleteSegment(int From, int To, const SOptions &Options);
		// merges the segments around Point into one, dropping the ones that can't be merged
		void ForgetPoint(int Point);
		// deleting one checkpoint merges its times into the neighbouring segments;
		// deleting the last one removes everything that came from checkpoints
		void DeleteGate(int GateId);
		void DeleteAllGates();
		// how many saved times deleting this checkpoint removes (0 unless it's the last one)
		int GateDeleteCount(int GateId) const;
		int AllGatesDeleteCount() const;

		// --- data for the analysis charts ---
		struct SRunPoint
		{
			int64_t m_Timestamp = 0;
			int m_Millis = 0;
			bool m_Practice = false;
		};
		std::vector<SRunPoint> FinishedRuns(bool IncludePractice) const;

		struct SSegmentLoss
		{
			int m_From = POINT_NONE;
			int m_To = POINT_NONE;
			int m_Best = -1;
			int m_Mean = -1;
			int m_Loss = 0; // mean of the recent attempts minus the best
		};
		static std::vector<SSegmentLoss> Losses(const SAnalysis &Analysis); // route segments, biggest loss first

		struct SHistoryPoint
		{
			int64_t m_Timestamp = 0;
			int m_Millis = 0;
			bool m_Practice = false;
			bool m_NewBest = false; // fastest so far
			int m_MovingAverage = 0; // mean of the last AVERAGE_WINDOW attempts up to this one
		};
		static std::vector<SHistoryPoint> SegmentHistory(const SAnalysis &Analysis, int From, int To);

		struct SDayActivity
		{
			int m_Real = 0;
			int m_Practice = 0;
		};
		// attempts per local day, oldest first, the last entry is today
		std::vector<SDayActivity> Activity(int64_t Now, int64_t UtcOffsetSeconds, int NumDays) const;

		struct SSummary
		{
			int m_FinishedRuns = 0;
			int m_Attempts = 0;
			int64_t m_TotalMillis = 0;
			int m_WorstFrom = POINT_NONE; // route segment with the biggest variation
			int m_WorstTo = POINT_NONE;
			int m_WorstStdDev = -1;
		};
		SSummary Summarize(const SAnalysis &Analysis, bool IncludePractice) const;

	private:
		// gives the attempts after a gap inside a run a new run id
		void SplitRunsAtGaps();
	};

} // namespace DdnettySplits

#endif // GAME_DDNETTY_SPLITS_H
