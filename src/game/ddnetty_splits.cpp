// ddnetty: data and math for the Splits feature.
#include "ddnetty_splits.h"

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <set>
#include <utility>

namespace DdnettySplits
{

	std::string PointToString(int Point)
	{
		if(Point == POINT_START)
			return "S";
		if(Point == POINT_FINISH)
			return "F";
		if(IsTimecp(Point))
			return "T" + std::to_string(Point);
		if(IsGate(Point))
			return "G" + std::to_string(GateIdOf(Point));
		return "?";
	}

	int PointFromString(const char *pStr)
	{
		if(!pStr || !pStr[0])
			return POINT_NONE;
		if(pStr[1] == '\0')
		{
			if(pStr[0] == 'S')
				return POINT_START;
			if(pStr[0] == 'F')
				return POINT_FINISH;
		}
		if(pStr[0] != 'T' && pStr[0] != 'G')
			return POINT_NONE;

		char *pEnd = nullptr;
		const long Value = std::strtol(pStr + 1, &pEnd, 10);
		if(pEnd == pStr + 1 || *pEnd != '\0' || Value < 0)
			return POINT_NONE;
		if(pStr[0] == 'T')
			return Value < NUM_TIMECPS ? (int)Value : POINT_NONE;
		return Value <= 1000000 ? GatePoint((int)Value) : POINT_NONE;
	}

	namespace
	{
		// reads the next whitespace separated token
		bool NextToken(const char *&pStr, std::string &Token)
		{
			while(*pStr == ' ' || *pStr == '\t')
				pStr++;
			if(*pStr == '\0' || *pStr == '\r' || *pStr == '\n')
				return false;
			const char *pStart = pStr;
			while(*pStr != '\0' && *pStr != ' ' && *pStr != '\t' && *pStr != '\r' && *pStr != '\n')
				pStr++;
			Token.assign(pStart, pStr - pStart);
			return true;
		}

		bool ParseInt(const std::string &Token, long long &Out)
		{
			char *pEnd = nullptr;
			Out = std::strtoll(Token.c_str(), &pEnd, 10);
			return pEnd != Token.c_str() && *pEnd == '\0';
		}

		bool ParseFloat(const std::string &Token, float &Out)
		{
			char *pEnd = nullptr;
			Out = std::strtof(Token.c_str(), &pEnd);
			return pEnd != Token.c_str() && *pEnd == '\0' && std::isfinite(Out);
		}

		SSegmentStats MakeStats(int From, int To, const std::vector<const SEffectiveAttempt *> &vpAttempts, const SOptions &Options)
		{
			SSegmentStats Stats;
			Stats.m_From = From;
			Stats.m_To = To;
			Stats.m_Count = (int)vpAttempts.size();
			if(vpAttempts.empty())
				return Stats;

			// best: every attempt, or only the ones entered like in a real run
			for(const SEffectiveAttempt *pEffective : vpAttempts)
			{
				if(Options.m_RunEntriesOnly && (pEffective->m_Attempt.m_Flags & ATTFLAGS_LOOSE_ENTRY))
					continue;
				if(Stats.m_Best < 0 || pEffective->m_Attempt.m_Millis < Stats.m_Best)
					Stats.m_Best = pEffective->m_Attempt.m_Millis;
			}
			Stats.m_Last = vpAttempts.back()->m_Attempt.m_Millis;
			Stats.m_LastTimestamp = vpAttempts.back()->m_Attempt.m_Timestamp;

			// average and variation: the last focused attempts
			std::vector<int> vFocused;
			for(const SEffectiveAttempt *pEffective : vpAttempts)
				if(pEffective->m_Focused)
					vFocused.push_back(pEffective->m_Attempt.m_Millis);
			Stats.m_Focused = (int)vFocused.size();
			if(vFocused.empty())
				return Stats;

			const size_t Window = (size_t)CSplitsData::AVERAGE_WINDOW;
			const size_t Start = vFocused.size() > Window ? vFocused.size() - Window : 0;
			const double Num = (double)(vFocused.size() - Start);
			double Sum = 0.0;
			for(size_t i = Start; i < vFocused.size(); i++)
				Sum += vFocused[i];
			const double Mean = Sum / Num;
			double Variance = 0.0;
			for(size_t i = Start; i < vFocused.size(); i++)
				Variance += (vFocused[i] - Mean) * (vFocused[i] - Mean);
			Variance /= Num;
			Stats.m_Mean = (int)std::lround(Mean);
			Stats.m_StdDev = (int)std::lround(std::sqrt(Variance));
			return Stats;
		}
	} // namespace

	const SSegmentStats *SAnalysis::Find(int From, int To) const
	{
		for(const SSegmentStats &Stats : m_vRouteStats)
			if(Stats.m_From == From && Stats.m_To == To)
				return &Stats;
		for(const SSegmentStats &Stats : m_vOtherStats)
			if(Stats.m_From == From && Stats.m_To == To)
				return &Stats;
		return nullptr;
	}

	int SAnalysis::RouteIndex(int Point, int FromIndex) const
	{
		for(size_t i = std::max(FromIndex, 0); i < m_vRoute.size(); i++)
			if(m_vRoute[i] == Point)
				return (int)i;
		return -1;
	}

	void CSplitsData::Clear()
	{
		m_vGates.clear();
		m_vAttempts.clear();
		m_TargetMillis = -1;
		m_SegmentDemos = false;
		m_Since = 0;
		m_FocusMarginMillis = -1;
	}

	bool CSplitsData::ParseLine(const char *pLine)
	{
		const char *pStr = pLine;
		std::string Kind;
		if(!NextToken(pStr, Kind) || Kind[0] == '#')
			return true; // empty line or comment

		std::string aTokens[6];
		if(Kind == "target")
		{
			std::string Token;
			long long Millis;
			if(!NextToken(pStr, Token) || !ParseInt(Token, Millis) || Millis < -1 || Millis > 24LL * 60 * 60 * 1000)
				return false;
			m_TargetMillis = Millis > 0 ? (int)Millis : -1;
			return true;
		}
		if(Kind == "since" || Kind == "focus")
		{
			std::string Token;
			long long Value;
			if(!NextToken(pStr, Token) || !ParseInt(Token, Value))
				return false;
			if(Kind == "since")
			{
				if(Value < 0)
					return false;
				m_Since = Value;
			}
			else
			{
				if(Value < -1 || Value > 10LL * 60 * 1000)
					return false;
				m_FocusMarginMillis = (int)Value;
			}
			return true;
		}
		if(Kind == "segdemos")
		{
			std::string Token;
			long long Value;
			if(!NextToken(pStr, Token) || !ParseInt(Token, Value) || (Value != 0 && Value != 1))
				return false;
			m_SegmentDemos = Value == 1;
			return true;
		}
		if(Kind == "gate")
		{
			for(int i = 0; i < 5; i++)
				if(!NextToken(pStr, aTokens[i]))
					return false;
			long long Id;
			SGate Gate;
			if(!ParseInt(aTokens[0], Id) || Id < 0 || Id > 1000000 ||
				!ParseFloat(aTokens[1], Gate.m_Ax) || !ParseFloat(aTokens[2], Gate.m_Ay) ||
				!ParseFloat(aTokens[3], Gate.m_Bx) || !ParseFloat(aTokens[4], Gate.m_By))
				return false;
			if(FindGate((int)Id))
				return false;
			Gate.m_Id = (int)Id;
			while(*pStr == ' ' || *pStr == '\t')
				pStr++;
			Gate.m_Name = pStr;
			while(!Gate.m_Name.empty() && (Gate.m_Name.back() == '\r' || Gate.m_Name.back() == '\n' || Gate.m_Name.back() == ' '))
				Gate.m_Name.pop_back();
			m_vGates.push_back(Gate);
			return true;
		}
		if(Kind == "att")
		{
			for(auto &Token : aTokens)
				if(!NextToken(pStr, Token))
					return false;
			long long RunId, Timestamp, Practice, Millis;
			if(!ParseInt(aTokens[0], RunId) || !ParseInt(aTokens[1], Timestamp) || !ParseInt(aTokens[2], Practice) || !ParseInt(aTokens[5], Millis))
				return false;
			if(Practice < 0 || Practice > 0xffff)
				return false;
			SAttempt Attempt;
			Attempt.m_From = PointFromString(aTokens[3].c_str());
			Attempt.m_To = PointFromString(aTokens[4].c_str());
			if(Attempt.m_From == POINT_NONE || Attempt.m_To == POINT_NONE || Attempt.m_From == Attempt.m_To)
				return false;
			if(RunId <= 0 || RunId > INT_MAX || Millis < 0 || Millis > 24LL * 60 * 60 * 1000)
				return false;
			Attempt.m_RunId = (int)RunId;
			Attempt.m_Timestamp = Timestamp;
			Attempt.m_Practice = (Practice & 1) != 0;
			Attempt.m_Flags = (int)(Practice & ~1LL);
			Attempt.m_Millis = (int)Millis;
			std::string Flags;
			if(NextToken(pStr, Flags))
				Attempt.m_FromGate = Flags.find('g') != std::string::npos;
			m_vAttempts.push_back(Attempt);
			return true;
		}
		return false;
	}

	std::string CSplitsData::FormatGate(const SGate &Gate)
	{
		char aBuf[128];
		std::snprintf(aBuf, sizeof(aBuf), "gate %d %.2f %.2f %.2f %.2f", Gate.m_Id, Gate.m_Ax, Gate.m_Ay, Gate.m_Bx, Gate.m_By);
		std::string Line = aBuf;
		if(!Gate.m_Name.empty())
			Line += " " + Gate.m_Name;
		return Line;
	}

	std::string CSplitsData::FormatAttempt(const SAttempt &Attempt)
	{
		char aBuf[128];
		std::snprintf(aBuf, sizeof(aBuf), "att %d %lld %d %s %s %d", Attempt.m_RunId, (long long)Attempt.m_Timestamp, (Attempt.m_Practice ? 1 : 0) | (Attempt.m_Flags & ~1),
			PointToString(Attempt.m_From).c_str(), PointToString(Attempt.m_To).c_str(), Attempt.m_Millis);
		std::string Line = aBuf;
		if(Attempt.m_FromGate)
			Line += " g";
		return Line;
	}

	std::string CSplitsData::Serialize() const
	{
		std::string Result = "# ddnetty splits v1\n";
		if(m_TargetMillis > 0)
			Result += "target " + std::to_string(m_TargetMillis) + "\n";
		if(m_SegmentDemos)
			Result += "segdemos 1\n";
		if(m_Since > 0)
			Result += "since " + std::to_string(m_Since) + "\n";
		if(m_FocusMarginMillis >= 0)
			Result += "focus " + std::to_string(m_FocusMarginMillis) + "\n";
		for(const SGate &Gate : m_vGates)
			Result += FormatGate(Gate) + "\n";
		for(const SAttempt &Attempt : m_vAttempts)
			Result += FormatAttempt(Attempt) + "\n";
		return Result;
	}

	int CSplitsData::NextRunId() const
	{
		int Max = 0;
		for(const SAttempt &Attempt : m_vAttempts)
			Max = std::max(Max, Attempt.m_RunId);
		return Max + 1;
	}

	int CSplitsData::NextGateId() const
	{
		int Max = 0;
		for(const SGate &Gate : m_vGates)
			Max = std::max(Max, Gate.m_Id);
		return Max + 1;
	}

	const SGate *CSplitsData::FindGate(int GateId) const
	{
		for(const SGate &Gate : m_vGates)
			if(Gate.m_Id == GateId)
				return &Gate;
		return nullptr;
	}

	SGate *CSplitsData::FindGate(int GateId)
	{
		for(SGate &Gate : m_vGates)
			if(Gate.m_Id == GateId)
				return &Gate;
		return nullptr;
	}

	bool CSplitsData::Includes(int Point, const SOptions &Options)
	{
		if(Point == POINT_START || Point == POINT_FINISH)
			return true;
		if(IsTimecp(Point))
			return Options.m_UseTimecps;
		if(IsGate(Point))
			return Options.m_UseGates;
		return false;
	}

	int CSplitsData::Reference(const SSegmentStats &Stats, const SOptions &Options)
	{
		if(Options.m_UseAverage && Stats.m_Mean >= 0)
			return Stats.m_Mean;
		return Stats.m_Best;
	}

	std::vector<SEffectiveAttempt> CSplitsData::EffectiveAttempts(const SOptions &Options) const
	{
		std::vector<SEffectiveAttempt> vResult;
		const size_t Num = m_vAttempts.size();
		size_t ChainStart = 0;
		while(ChainStart < Num)
		{
			// a chain is a run without gaps
			size_t ChainEnd = ChainStart + 1;
			while(ChainEnd < Num && m_vAttempts[ChainEnd].m_RunId == m_vAttempts[ChainStart].m_RunId && m_vAttempts[ChainEnd].m_From == m_vAttempts[ChainEnd - 1].m_To)
				ChainEnd++;

			bool Open = false;
			SEffectiveAttempt Current;
			auto &&OpenAt = [&](int Point, const SAttempt &Base, int64_t Timestamp) {
				Open = true;
				Current = SEffectiveAttempt();
				Current.m_Attempt = Base;
				Current.m_Attempt.m_From = Point;
				Current.m_Attempt.m_To = POINT_NONE;
				Current.m_Attempt.m_Millis = 0;
				Current.m_Attempt.m_Practice = false;
				Current.m_Attempt.m_Flags = 0;
				Current.m_Attempt.m_Timestamp = Timestamp;
			};

			for(size_t i = ChainStart; i < ChainEnd; i++)
			{
				const SAttempt &Attempt = m_vAttempts[i];
				if(!Open && Includes(Attempt.m_From, Options))
					OpenAt(Attempt.m_From, Attempt, Attempt.m_Timestamp);

				if(Open)
				{
					// how the segment was entered comes from its first piece, the rest adds up
					Current.m_Attempt.m_Flags |= Current.m_vRawIndices.empty() ? Attempt.m_Flags : (Attempt.m_Flags & ~ATTFLAGS_ENTRY);
					Current.m_Attempt.m_Millis += Attempt.m_Millis;
					Current.m_Attempt.m_Practice = Current.m_Attempt.m_Practice || Attempt.m_Practice;
					Current.m_vRawIndices.push_back(i);
				}

				if(Includes(Attempt.m_To, Options))
				{
					if(Open)
					{
						Current.m_Attempt.m_To = Attempt.m_To;
						const bool Dummy = (Current.m_Attempt.m_Flags & (ATTFLAG_DUMMY | ATTFLAG_SWITCHED)) != 0;
						if(Current.m_Attempt.m_From != Current.m_Attempt.m_To && (Options.m_CountPractice || !Current.m_Attempt.m_Practice) &&
							Current.m_Attempt.m_Timestamp >= Options.m_Since && (Options.m_CountDummy || !Dummy))
							vResult.push_back(Current);
					}
					const int64_t NextTimestamp = i + 1 < ChainEnd ? m_vAttempts[i + 1].m_Timestamp : Attempt.m_Timestamp;
					OpenAt(Attempt.m_To, Attempt, NextTimestamp);
				}
			}
			ChainStart = ChainEnd;
		}
		return vResult;
	}

	SAnalysis CSplitsData::Analyze(const SOptions &Options) const
	{
		SAnalysis Analysis;
		Analysis.m_vAttempts = EffectiveAttempts(Options);
		std::vector<SEffectiveAttempt> &vAttempts = Analysis.m_vAttempts;

		// runs: the latest complete one gives the route, the fastest real one is the best finish
		// and its times at each point are the pace the focus margin is measured against
		int64_t LatestCompleteTimestamp = INT64_MIN;
		std::vector<int> vLatestRoute;
		std::map<int, int> BestPace; // point -> run time of the best finish when it got there
		size_t RunStart = 0;
		while(RunStart < vAttempts.size())
		{
			size_t RunEnd = RunStart + 1;
			while(RunEnd < vAttempts.size() && vAttempts[RunEnd].m_Attempt.m_RunId == vAttempts[RunStart].m_Attempt.m_RunId && vAttempts[RunEnd].m_Attempt.m_From == vAttempts[RunEnd - 1].m_Attempt.m_To)
				RunEnd++;

			const bool FromStart = vAttempts[RunStart].m_Attempt.m_From == POINT_START;
			int Total = 0;
			bool Practice = false;
			for(size_t i = RunStart; i < RunEnd; i++)
			{
				vAttempts[i].m_EntryMillis = FromStart ? Total : -1;
				Total += vAttempts[i].m_Attempt.m_Millis;
				Practice = Practice || vAttempts[i].m_Attempt.m_Practice;
			}

			if(FromStart && vAttempts[RunEnd - 1].m_Attempt.m_To == POINT_FINISH)
			{
				std::vector<int> vRoute = {POINT_START};
				for(size_t i = RunStart; i < RunEnd; i++)
					vRoute.push_back(vAttempts[i].m_Attempt.m_To);
				if(!Practice && (Analysis.m_BestFinished < 0 || Total < Analysis.m_BestFinished))
				{
					Analysis.m_BestFinished = Total;
					BestPace.clear();
					for(size_t i = RunStart; i < RunEnd; i++)
						BestPace.emplace(vAttempts[i].m_Attempt.m_From, vAttempts[i].m_EntryMillis);
				}
				if(vAttempts[RunStart].m_Attempt.m_Timestamp >= LatestCompleteTimestamp)
				{
					LatestCompleteTimestamp = vAttempts[RunStart].m_Attempt.m_Timestamp;
					vLatestRoute = vRoute;
				}
			}
			RunStart = RunEnd;
		}

		// focused: entered like in a real run and, with a margin, not too far behind the best run
		for(SEffectiveAttempt &Effective : vAttempts)
		{
			Effective.m_Focused = (Effective.m_Attempt.m_Flags & ATTFLAGS_LOOSE_ENTRY) == 0;
			if(!Effective.m_Focused || Options.m_FocusMarginMillis <= 0 || BestPace.empty())
				continue;
			auto It = BestPace.find(Effective.m_Attempt.m_From);
			if(It == BestPace.end())
				continue; // not on the best run's route: nothing to compare with
			Effective.m_Focused = Effective.m_EntryMillis >= 0 && Effective.m_EntryMillis <= It->second + Options.m_FocusMarginMillis;
		}

		// group by segment, keeping the order the segments first appeared in
		std::map<std::pair<int, int>, std::vector<const SEffectiveAttempt *>> SegmentAttempts;
		std::vector<std::pair<int, int>> vSegmentOrder;
		for(const SEffectiveAttempt &Effective : vAttempts)
		{
			const std::pair<int, int> Key(Effective.m_Attempt.m_From, Effective.m_Attempt.m_To);
			auto &vpList = SegmentAttempts[Key];
			if(vpList.empty())
				vSegmentOrder.push_back(Key);
			vpList.push_back(&Effective);
		}

		if(!vLatestRoute.empty())
		{
			Analysis.m_vRoute = vLatestRoute;
		}
		else
		{
			// no complete run yet: follow the most used segment out of each point
			Analysis.m_vRoute.push_back(POINT_START);
			std::set<int> Visited = {POINT_START};
			int Current = POINT_START;
			while(Current != POINT_FINISH && Analysis.m_vRoute.size() < 256)
			{
				int BestNext = POINT_NONE;
				int BestCount = 0;
				int BestTime = INT_MAX;
				for(const auto &[Key, vpList] : SegmentAttempts)
				{
					if(Key.first != Current || Visited.count(Key.second))
						continue;
					int Best = INT_MAX;
					for(const SEffectiveAttempt *pEffective : vpList)
						Best = std::min(Best, pEffective->m_Attempt.m_Millis);
					if((int)vpList.size() > BestCount || ((int)vpList.size() == BestCount && Best < BestTime))
					{
						BestNext = Key.second;
						BestCount = (int)vpList.size();
						BestTime = Best;
					}
				}
				if(BestNext == POINT_NONE)
					break;
				Analysis.m_vRoute.push_back(BestNext);
				Visited.insert(BestNext);
				Current = BestNext;
			}
		}
		Analysis.m_RouteComplete = Analysis.m_vRoute.size() >= 2 && Analysis.m_vRoute.front() == POINT_START && Analysis.m_vRoute.back() == POINT_FINISH;

		// stats along the route
		std::set<std::pair<int, int>> RouteSegments;
		int BuiltRun = 0;
		for(size_t i = 0; i + 1 < Analysis.m_vRoute.size(); i++)
		{
			const std::pair<int, int> Key(Analysis.m_vRoute[i], Analysis.m_vRoute[i + 1]);
			RouteSegments.insert(Key);
			auto It = SegmentAttempts.find(Key);
			SSegmentStats Stats = It != SegmentAttempts.end() ? MakeStats(Key.first, Key.second, It->second, Options) : MakeStats(Key.first, Key.second, {}, Options);
			const int Ref = Reference(Stats, Options);
			if(Ref < 0)
				Analysis.m_BuiltRunMissing++;
			else
				BuiltRun += Ref;
			Analysis.m_vRouteStats.push_back(Stats);
		}
		if(Analysis.m_RouteComplete && Analysis.m_BuiltRunMissing == 0)
			Analysis.m_BuiltRun = BuiltRun;

		for(const auto &Key : vSegmentOrder)
			if(!RouteSegments.count(Key))
				Analysis.m_vOtherStats.push_back(MakeStats(Key.first, Key.second, SegmentAttempts[Key], Options));

		return Analysis;
	}

	int CSplitsData::Remaining(const SAnalysis &Analysis, int RouteIndex, const SOptions &Options)
	{
		if(!Analysis.m_RouteComplete || RouteIndex < 0 || RouteIndex >= (int)Analysis.m_vRoute.size())
			return -1;
		int Sum = 0;
		for(size_t i = RouteIndex; i < Analysis.m_vRouteStats.size(); i++)
		{
			const int Ref = Reference(Analysis.m_vRouteStats[i], Options);
			if(Ref < 0)
				return -1;
			Sum += Ref;
		}
		return Sum;
	}

	std::vector<SRun> CSplitsData::Runs() const
	{
		std::vector<SRun> vRuns;
		const size_t Num = m_vAttempts.size();
		size_t Start = 0;
		while(Start < Num)
		{
			size_t End = Start + 1;
			while(End < Num && m_vAttempts[End].m_RunId == m_vAttempts[Start].m_RunId && m_vAttempts[End].m_From == m_vAttempts[End - 1].m_To)
				End++;

			SRun Run;
			Run.m_RunId = m_vAttempts[Start].m_RunId;
			Run.m_Timestamp = m_vAttempts[Start].m_Timestamp;
			Run.m_From = m_vAttempts[Start].m_From;
			Run.m_To = m_vAttempts[End - 1].m_To;
			for(size_t i = Start; i < End; i++)
			{
				Run.m_Millis += m_vAttempts[i].m_Millis;
				Run.m_Practice = Run.m_Practice || m_vAttempts[i].m_Practice;
				Run.m_NumSegments++;
			}
			vRuns.push_back(Run);
			Start = End;
		}
		return vRuns;
	}

	void CSplitsData::DeleteRuns(const std::vector<int> &vRunIds)
	{
		const std::set<int> Ids(vRunIds.begin(), vRunIds.end());
		m_vAttempts.erase(std::remove_if(m_vAttempts.begin(), m_vAttempts.end(), [&](const SAttempt &Attempt) { return Ids.count(Attempt.m_RunId) > 0; }), m_vAttempts.end());
	}

	void CSplitsData::DeleteSegment(int From, int To, const SOptions &Options)
	{
		// delete practice attempts too, even if they're hidden right now
		SOptions AllOptions = Options;
		AllOptions.m_CountPractice = true;

		std::set<size_t> Remove;
		for(const SEffectiveAttempt &Effective : EffectiveAttempts(AllOptions))
			if(Effective.m_Attempt.m_From == From && Effective.m_Attempt.m_To == To)
				Remove.insert(Effective.m_vRawIndices.begin(), Effective.m_vRawIndices.end());

		std::vector<SAttempt> vKept;
		for(size_t i = 0; i < m_vAttempts.size(); i++)
			if(!Remove.count(i))
				vKept.push_back(m_vAttempts[i]);
		m_vAttempts = vKept;
		SplitRunsAtGaps();
	}

	int CSplitsData::ForgetPoints(const std::set<int> &Points)
	{
		int Dropped = 0;
		std::vector<SAttempt> vKept;
		const size_t Num = m_vAttempts.size();
		size_t ChainStart = 0;
		while(ChainStart < Num)
		{
			// a chain is a run without gaps
			size_t ChainEnd = ChainStart + 1;
			while(ChainEnd < Num && m_vAttempts[ChainEnd].m_RunId == m_vAttempts[ChainStart].m_RunId && m_vAttempts[ChainEnd].m_From == m_vAttempts[ChainEnd - 1].m_To)
				ChainEnd++;

			// the chain is a list of points with times in between: take the forgotten points out and add
			// up the times around them. A segment that then starts and ends at the same point (went back
			// through it) is dropped, like while recording: the segment starts again at the last crossing.
			std::vector<SAttempt> vChain(m_vAttempts.begin() + ChainStart, m_vAttempts.begin() + ChainEnd);
			bool Changed = true;
			while(Changed)
			{
				Changed = false;
				// at the edges there is nothing to merge with: the time before the first point is unknown
				while(!vChain.empty() && Points.count(vChain.front().m_From))
				{
					vChain.erase(vChain.begin());
					Dropped++;
					Changed = true;
				}
				while(!vChain.empty() && Points.count(vChain.back().m_To))
				{
					vChain.pop_back();
					Dropped++;
					Changed = true;
				}
				for(size_t i = 0; i + 1 < vChain.size(); i++)
				{
					if(!Points.count(vChain[i].m_To))
						continue;
					SAttempt &Merged = vChain[i];
					const SAttempt &Next = vChain[i + 1];
					Merged.m_To = Next.m_To;
					Merged.m_Millis += Next.m_Millis;
					Merged.m_Practice = Merged.m_Practice || Next.m_Practice;
					Merged.m_FromGate = true;
					vChain.erase(vChain.begin() + i + 1);
					if(Merged.m_From == Merged.m_To)
						vChain.erase(vChain.begin() + i);
					Changed = true;
					break;
				}
			}
			vKept.insert(vKept.end(), vChain.begin(), vChain.end());
			ChainStart = ChainEnd;
		}
		m_vAttempts = vKept;
		SplitRunsAtGaps();
		return Dropped;
	}

	int CSplitsData::ForgetPoint(int Point)
	{
		return ForgetPoints({Point});
	}

	void CSplitsData::DeleteGate(int GateId)
	{
		ForgetPoint(GatePoint(GateId));
		m_vGates.erase(std::remove_if(m_vGates.begin(), m_vGates.end(), [&](const SGate &Gate) { return Gate.m_Id == GateId; }), m_vGates.end());
	}

	std::set<int> CSplitsData::AllGatePoints() const
	{
		std::set<int> Points;
		for(const SGate &Gate : m_vGates)
			Points.insert(GatePoint(Gate.m_Id));
		// times that touch a checkpoint left over from an older version are merged too
		for(const SAttempt &Attempt : m_vAttempts)
		{
			if(IsGate(Attempt.m_From))
				Points.insert(Attempt.m_From);
			if(IsGate(Attempt.m_To))
				Points.insert(Attempt.m_To);
		}
		return Points;
	}

	void CSplitsData::DeleteAllGates()
	{
		ForgetPoints(AllGatePoints());
		m_vGates.clear();
	}

	int CSplitsData::GateDeleteCount(int GateId) const
	{
		CSplitsData Copy = *this;
		return Copy.ForgetPoint(GatePoint(GateId));
	}

	int CSplitsData::AllGatesDeleteCount() const
	{
		CSplitsData Copy = *this;
		return Copy.ForgetPoints(AllGatePoints());
	}

	std::vector<CSplitsData::SRunPoint> CSplitsData::FinishedRuns(bool IncludePractice) const
	{
		std::vector<SRunPoint> vResult;
		for(const SRun &Run : Runs())
		{
			if(!Run.Finished() || (Run.m_Practice && !IncludePractice))
				continue;
			SRunPoint Point;
			Point.m_Timestamp = Run.m_Timestamp;
			Point.m_Millis = Run.m_Millis;
			Point.m_Practice = Run.m_Practice;
			vResult.push_back(Point);
		}
		std::stable_sort(vResult.begin(), vResult.end(), [](const SRunPoint &A, const SRunPoint &B) { return A.m_Timestamp < B.m_Timestamp; });
		return vResult;
	}

	std::vector<CSplitsData::SSegmentLoss> CSplitsData::Losses(const SAnalysis &Analysis)
	{
		std::vector<SSegmentLoss> vResult;
		for(const SSegmentStats &Stats : Analysis.m_vRouteStats)
		{
			if(Stats.m_Count <= 0)
				continue;
			SSegmentLoss Loss;
			Loss.m_From = Stats.m_From;
			Loss.m_To = Stats.m_To;
			Loss.m_Best = Stats.m_Best;
			Loss.m_Mean = Stats.m_Mean;
			Loss.m_Loss = std::max(0, Stats.m_Mean - Stats.m_Best);
			vResult.push_back(Loss);
		}
		std::stable_sort(vResult.begin(), vResult.end(), [](const SSegmentLoss &A, const SSegmentLoss &B) { return A.m_Loss > B.m_Loss; });
		return vResult;
	}

	std::vector<CSplitsData::SHistoryPoint> CSplitsData::SegmentHistory(const SAnalysis &Analysis, int From, int To)
	{
		std::vector<SHistoryPoint> vResult;
		int Best = INT_MAX;
		for(const SEffectiveAttempt &Effective : Analysis.m_vAttempts)
		{
			const SAttempt &Attempt = Effective.m_Attempt;
			if(Attempt.m_From != From || Attempt.m_To != To)
				continue;
			SHistoryPoint Point;
			Point.m_Timestamp = Attempt.m_Timestamp;
			Point.m_Millis = Attempt.m_Millis;
			Point.m_Practice = Attempt.m_Practice;
			Point.m_NewBest = Attempt.m_Millis < Best;
			Best = std::min(Best, Attempt.m_Millis);
			vResult.push_back(Point);

			vResult.back().m_Focused = Effective.m_Focused;
			const size_t Start = vResult.size() > (size_t)AVERAGE_WINDOW ? vResult.size() - AVERAGE_WINDOW : 0;
			const double Num = (double)(vResult.size() - Start);
			double Sum = 0.0;
			for(size_t i = Start; i < vResult.size(); i++)
				Sum += vResult[i].m_Millis;
			const double Mean = Sum / Num;
			double Variance = 0.0;
			for(size_t i = Start; i < vResult.size(); i++)
				Variance += (vResult[i].m_Millis - Mean) * (vResult[i].m_Millis - Mean);
			vResult.back().m_MovingAverage = (int)(Sum / Num);
			vResult.back().m_MovingStdDev = (int)std::lround(std::sqrt(Variance / Num));
		}
		return vResult;
	}

	std::vector<CSplitsData::SIdealPoint> CSplitsData::IdealHistory(const SAnalysis &Analysis)
	{
		std::vector<SIdealPoint> vResult;
		if(!Analysis.m_RouteComplete)
			return vResult;
		std::map<std::pair<int, int>, size_t> RouteSegments;
		for(size_t i = 0; i + 1 < Analysis.m_vRoute.size(); i++)
			RouteSegments.emplace(std::make_pair(Analysis.m_vRoute[i], Analysis.m_vRoute[i + 1]), i);
		std::vector<int> vBest(RouteSegments.size(), -1);
		size_t Missing = vBest.size();
		long long Sum = 0;
		for(size_t i = 0; i < Analysis.m_vAttempts.size(); i++)
		{
			const SAttempt &Attempt = Analysis.m_vAttempts[i].m_Attempt;
			auto It = RouteSegments.find(std::make_pair(Attempt.m_From, Attempt.m_To));
			if(It == RouteSegments.end())
				continue;
			int &Best = vBest[It->second];
			if(Best >= 0 && Attempt.m_Millis >= Best)
				continue;
			if(Best < 0)
				Missing--;
			else
				Sum -= Best;
			Best = Attempt.m_Millis;
			Sum += Best;
			if(Missing == 0)
				vResult.push_back({(int)i, Attempt.m_Timestamp, (int)Sum});
		}
		return vResult;
	}

	std::vector<CSplitsData::SFocusGap> CSplitsData::FocusGaps(const SAnalysis &Analysis)
	{
		std::vector<SFocusGap> vResult;
		for(const SSegmentStats &Stats : Analysis.m_vRouteStats)
		{
			if(Stats.m_Count == 0)
				continue;
			SFocusGap Gap;
			Gap.m_From = Stats.m_From;
			Gap.m_To = Stats.m_To;
			Gap.m_Best = Stats.m_Best;
			Gap.m_MeanFocused = Stats.m_Mean;
			std::vector<int> vAll;
			for(const SEffectiveAttempt &Effective : Analysis.m_vAttempts)
				if(Effective.m_Attempt.m_From == Stats.m_From && Effective.m_Attempt.m_To == Stats.m_To)
					vAll.push_back(Effective.m_Attempt.m_Millis);
			const size_t Start = vAll.size() > (size_t)AVERAGE_WINDOW ? vAll.size() - AVERAGE_WINDOW : 0;
			long long Sum = 0;
			for(size_t i = Start; i < vAll.size(); i++)
				Sum += vAll[i];
			Gap.m_MeanAll = (int)std::lround((double)Sum / (double)(vAll.size() - Start));
			vResult.push_back(Gap);
		}
		return vResult;
	}

	namespace
	{
		// calls Fn(Begin, End) for every chain of attempts that starts at the start line
		template<typename F>
		void ForEachRunFromStart(const SAnalysis &Analysis, F &&Fn)
		{
			const auto &vAttempts = Analysis.m_vAttempts;
			size_t RunStart = 0;
			while(RunStart < vAttempts.size())
			{
				size_t RunEnd = RunStart + 1;
				while(RunEnd < vAttempts.size() && vAttempts[RunEnd].m_Attempt.m_RunId == vAttempts[RunStart].m_Attempt.m_RunId && vAttempts[RunEnd].m_Attempt.m_From == vAttempts[RunEnd - 1].m_Attempt.m_To)
					RunEnd++;
				if(vAttempts[RunStart].m_Attempt.m_From == POINT_START)
					Fn(RunStart, RunEnd);
				RunStart = RunEnd;
			}
		}
	} // namespace

	std::vector<int> CSplitsData::Reach(const SAnalysis &Analysis)
	{
		std::vector<int> vResult(Analysis.m_vRoute.size(), 0);
		if(vResult.empty())
			return vResult;
		ForEachRunFromStart(Analysis, [&](size_t Begin, size_t End) {
			// follow the route as far as the run went
			size_t Index = 0;
			vResult[0]++;
			for(size_t i = Begin; i < End && Index + 1 < Analysis.m_vRoute.size(); i++)
			{
				const SAttempt &Attempt = Analysis.m_vAttempts[i].m_Attempt;
				if(Attempt.m_From != Analysis.m_vRoute[Index] || Attempt.m_To != Analysis.m_vRoute[Index + 1])
					break;
				Index++;
				vResult[Index]++;
			}
		});
		return vResult;
	}

	std::vector<CSplitsData::SRunTimes> CSplitsData::RouteTimes(const SAnalysis &Analysis, int MaxRuns)
	{
		std::vector<SRunTimes> vResult;
		ForEachRunFromStart(Analysis, [&](size_t Begin, size_t End) {
			if(Analysis.m_vAttempts[End - 1].m_Attempt.m_To != POINT_FINISH)
				return;
			SRunTimes Run;
			Run.m_Timestamp = Analysis.m_vAttempts[Begin].m_Attempt.m_Timestamp;
			Run.m_vAtRoute.assign(Analysis.m_vRoute.size(), -1);
			if(!Run.m_vAtRoute.empty())
				Run.m_vAtRoute[0] = 0;
			int Time = 0;
			int Index = 0;
			for(size_t i = Begin; i < End; i++)
			{
				const SAttempt &Attempt = Analysis.m_vAttempts[i].m_Attempt;
				Time += Attempt.m_Millis;
				const int Next = Analysis.RouteIndex(Attempt.m_To, Index + 1);
				if(Next >= 0)
				{
					Run.m_vAtRoute[Next] = Time;
					Index = Next;
				}
			}
			Run.m_Millis = Time;
			vResult.push_back(Run);
		});
		if(MaxRuns >= 0 && (int)vResult.size() > MaxRuns)
			vResult.erase(vResult.begin(), vResult.end() - MaxRuns);
		return vResult;
	}

	CSplitsData::SHeatmap CSplitsData::Heatmap(const SAnalysis &Analysis, int64_t UtcOffsetSeconds)
	{
		SHeatmap Heat;
		std::map<std::pair<int, int>, int> Best;
		for(const SEffectiveAttempt &Effective : Analysis.m_vAttempts)
		{
			const SAttempt &Attempt = Effective.m_Attempt;
			const int64_t Local = Attempt.m_Timestamp + UtcOffsetSeconds;
			const int64_t Day = Local >= 0 ? Local / 86400 : (Local - 86399) / 86400;
			const int Weekday = (int)(((Day + 4) % 7 + 7) % 7); // 1970-01-01 was a Thursday
			const int Hour = (int)((Local - Day * 86400) / 3600);
			Heat.m_aaAttempts[Weekday][Hour]++;
			auto [It, New] = Best.emplace(std::make_pair(Attempt.m_From, Attempt.m_To), Attempt.m_Millis);
			if(!New && Attempt.m_Millis < It->second)
			{
				It->second = Attempt.m_Millis;
				Heat.m_aaRecords[Weekday][Hour]++;
			}
		}
		return Heat;
	}

	std::vector<CSplitsData::SDayActivity> CSplitsData::Activity(int64_t Now, int64_t UtcOffsetSeconds, int NumDays) const
	{
		std::vector<SDayActivity> vResult(std::max(NumDays, 0));
		if(vResult.empty())
			return vResult;
		auto &&DayOf = [&](int64_t Timestamp) {
			const int64_t Local = Timestamp + UtcOffsetSeconds;
			return Local >= 0 ? Local / 86400 : (Local - 86399) / 86400;
		};
		const int64_t Today = DayOf(Now);
		for(const SAttempt &Attempt : m_vAttempts)
		{
			const int64_t Ago = Today - DayOf(Attempt.m_Timestamp);
			if(Ago < 0 || Ago >= NumDays)
				continue;
			SDayActivity &Day = vResult[NumDays - 1 - Ago];
			if(Attempt.m_Practice)
				Day.m_Practice++;
			else
				Day.m_Real++;
		}
		return vResult;
	}

	CSplitsData::SSummary CSplitsData::Summarize(const SAnalysis &Analysis, bool IncludePractice) const
	{
		SSummary Summary;
		Summary.m_FinishedRuns = (int)FinishedRuns(IncludePractice).size();
		Summary.m_Attempts = (int)Analysis.m_vAttempts.size();
		for(const SAttempt &Attempt : m_vAttempts)
			if(IncludePractice || !Attempt.m_Practice)
				Summary.m_TotalMillis += Attempt.m_Millis;
		for(const SSegmentStats &Stats : Analysis.m_vRouteStats)
		{
			if(Stats.m_Count >= 3 && Stats.m_StdDev > Summary.m_WorstStdDev)
			{
				Summary.m_WorstStdDev = Stats.m_StdDev;
				Summary.m_WorstFrom = Stats.m_From;
				Summary.m_WorstTo = Stats.m_To;
			}
		}
		return Summary;
	}

	void CSplitsData::SplitRunsAtGaps()
	{
		int Next = NextRunId();
		int PrevOriginal = INT_MIN;
		int PrevTo = POINT_NONE;
		int Assigned = 0;
		for(SAttempt &Attempt : m_vAttempts)
		{
			const int Original = Attempt.m_RunId;
			if(Original != PrevOriginal)
				Assigned = Original;
			else if(Attempt.m_From != PrevTo)
				Assigned = Next++;
			PrevOriginal = Original;
			PrevTo = Attempt.m_To;
			Attempt.m_RunId = Assigned;
		}
	}

	void CSplitsRefs::Clear()
	{
		m_vRuns.clear();
		m_vSegments.clear();
	}

	bool CSplitsRefs::ParseLine(const char *pLine)
	{
		const char *pStr = pLine;
		std::string Kind;
		if(!NextToken(pStr, Kind) || Kind[0] == '#')
			return true;

		auto &&RestOfLine = [&]() {
			while(*pStr == ' ' || *pStr == '\t')
				pStr++;
			std::string Rest = pStr;
			while(!Rest.empty() && (Rest.back() == '\r' || Rest.back() == '\n' || Rest.back() == ' '))
				Rest.pop_back();
			return Rest;
		};

		std::string Token;
		long long Value;
		if(Kind == "ref")
		{
			SRefRun Run;
			if(!NextToken(pStr, Token) || !ParseInt(Token, Value) || Value <= 0 || Value > 24LL * 60 * 60 * 1000)
				return false;
			Run.m_Millis = (int)Value;
			if(!NextToken(pStr, Run.m_Date))
				return false;
			long long NumCps;
			if(!NextToken(pStr, Token) || !ParseInt(Token, NumCps) || NumCps < 0 || NumCps > NUM_TIMECPS)
				return false;
			for(long long i = 0; i < NumCps; i++)
			{
				if(!NextToken(pStr, Token) || !ParseInt(Token, Value) || Value < -1 || Value > Run.m_Millis)
					return false;
				Run.m_vCps.push_back((int)Value);
			}
			Run.m_Name = RestOfLine();
			if(Run.m_Name.empty())
				return false;
			auto It = std::upper_bound(m_vRuns.begin(), m_vRuns.end(), Run, [](const SRefRun &A, const SRefRun &B) { return A.m_Millis < B.m_Millis; });
			m_vRuns.insert(It, Run);
			return true;
		}
		if(Kind == "segbest")
		{
			SRefSegment Segment;
			std::string From, To;
			if(!NextToken(pStr, From) || !NextToken(pStr, To) || !NextToken(pStr, Token) || !ParseInt(Token, Value) || Value < 0 || Value > 24LL * 60 * 60 * 1000)
				return false;
			Segment.m_From = PointFromString(From.c_str());
			Segment.m_To = PointFromString(To.c_str());
			if(Segment.m_From == POINT_NONE || Segment.m_To == POINT_NONE || Segment.m_From == Segment.m_To)
				return false;
			Segment.m_Millis = (int)Value;
			if(!NextToken(pStr, Segment.m_Date))
				return false;
			Segment.m_Name = RestOfLine();
			for(SRefSegment &Existing : m_vSegments)
			{
				if(Existing.m_From == Segment.m_From && Existing.m_To == Segment.m_To)
				{
					if(Segment.m_Millis < Existing.m_Millis)
						Existing = Segment;
					return true;
				}
			}
			m_vSegments.push_back(Segment);
			return true;
		}
		return false;
	}

	const SRefRun *CSplitsRefs::FindRun(const char *pName) const
	{
		if(m_vRuns.empty())
			return nullptr;
		if(pName && pName[0])
			for(const SRefRun &Run : m_vRuns)
				if(Run.m_Name == pName)
					return &Run;
		return &m_vRuns.front();
	}

	const SRefSegment *CSplitsRefs::FindSegment(int From, int To) const
	{
		for(const SRefSegment &Segment : m_vSegments)
			if(Segment.m_From == From && Segment.m_To == To)
				return &Segment;
		return nullptr;
	}

	int CSplitsRefs::TimeAt(const SRefRun &Run, int Point)
	{
		if(Point == POINT_START)
			return 0;
		if(Point == POINT_FINISH)
			return Run.m_Millis;
		if(IsTimecp(Point) && Point < (int)Run.m_vCps.size())
			return Run.m_vCps[Point];
		return -1;
	}

} // namespace DdnettySplits
