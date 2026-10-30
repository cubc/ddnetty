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

		SSegmentStats MakeStats(int From, int To, const std::vector<const SAttempt *> &vpAttempts)
		{
			SSegmentStats Stats;
			Stats.m_From = From;
			Stats.m_To = To;
			Stats.m_Count = (int)vpAttempts.size();
			if(vpAttempts.empty())
				return Stats;

			Stats.m_Best = INT_MAX;
			for(const SAttempt *pAttempt : vpAttempts)
				Stats.m_Best = std::min(Stats.m_Best, pAttempt->m_Millis);
			Stats.m_Last = vpAttempts.back()->m_Millis;
			Stats.m_LastTimestamp = vpAttempts.back()->m_Timestamp;

			const size_t Window = (size_t)CSplitsData::AVERAGE_WINDOW;
			const size_t Start = vpAttempts.size() > Window ? vpAttempts.size() - Window : 0;
			const double Num = (double)(vpAttempts.size() - Start);
			double Sum = 0.0;
			for(size_t i = Start; i < vpAttempts.size(); i++)
				Sum += vpAttempts[i]->m_Millis;
			const double Mean = Sum / Num;
			double Variance = 0.0;
			for(size_t i = Start; i < vpAttempts.size(); i++)
				Variance += (vpAttempts[i]->m_Millis - Mean) * (vpAttempts[i]->m_Millis - Mean);
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
			SAttempt Attempt;
			Attempt.m_From = PointFromString(aTokens[3].c_str());
			Attempt.m_To = PointFromString(aTokens[4].c_str());
			if(Attempt.m_From == POINT_NONE || Attempt.m_To == POINT_NONE || Attempt.m_From == Attempt.m_To)
				return false;
			if(RunId <= 0 || RunId > INT_MAX || Millis < 0 || Millis > 24LL * 60 * 60 * 1000)
				return false;
			Attempt.m_RunId = (int)RunId;
			Attempt.m_Timestamp = Timestamp;
			Attempt.m_Practice = Practice != 0;
			Attempt.m_Millis = (int)Millis;
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
		std::snprintf(aBuf, sizeof(aBuf), "att %d %lld %d %s %s %d", Attempt.m_RunId, (long long)Attempt.m_Timestamp, Attempt.m_Practice ? 1 : 0,
			PointToString(Attempt.m_From).c_str(), PointToString(Attempt.m_To).c_str(), Attempt.m_Millis);
		return aBuf;
	}

	std::string CSplitsData::Serialize() const
	{
		std::string Result = "# ddnetty splits v1\n";
		if(m_TargetMillis > 0)
			Result += "target " + std::to_string(m_TargetMillis) + "\n";
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
				Current.m_Attempt.m_Timestamp = Timestamp;
			};

			for(size_t i = ChainStart; i < ChainEnd; i++)
			{
				const SAttempt &Attempt = m_vAttempts[i];
				if(!Open && Includes(Attempt.m_From, Options))
					OpenAt(Attempt.m_From, Attempt, Attempt.m_Timestamp);

				if(Open)
				{
					Current.m_Attempt.m_Millis += Attempt.m_Millis;
					Current.m_Attempt.m_Practice = Current.m_Attempt.m_Practice || Attempt.m_Practice;
					Current.m_vRawIndices.push_back(i);
				}

				if(Includes(Attempt.m_To, Options))
				{
					if(Open)
					{
						Current.m_Attempt.m_To = Attempt.m_To;
						if(Current.m_Attempt.m_From != Current.m_Attempt.m_To && (Options.m_CountPractice || !Current.m_Attempt.m_Practice))
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
		const std::vector<SEffectiveAttempt> &vAttempts = Analysis.m_vAttempts;

		// group by segment, keeping the order the segments first appeared in
		std::map<std::pair<int, int>, std::vector<const SAttempt *>> SegmentAttempts;
		std::vector<std::pair<int, int>> vSegmentOrder;
		for(const SEffectiveAttempt &Effective : vAttempts)
		{
			const std::pair<int, int> Key(Effective.m_Attempt.m_From, Effective.m_Attempt.m_To);
			auto &vpList = SegmentAttempts[Key];
			if(vpList.empty())
				vSegmentOrder.push_back(Key);
			vpList.push_back(&Effective.m_Attempt);
		}

		// complete runs: the latest one gives the route, the fastest real one is the best finish
		int64_t LatestCompleteTimestamp = INT64_MIN;
		std::vector<int> vLatestRoute;
		size_t RunStart = 0;
		while(RunStart < vAttempts.size())
		{
			size_t RunEnd = RunStart + 1;
			while(RunEnd < vAttempts.size() && vAttempts[RunEnd].m_Attempt.m_RunId == vAttempts[RunStart].m_Attempt.m_RunId && vAttempts[RunEnd].m_Attempt.m_From == vAttempts[RunEnd - 1].m_Attempt.m_To)
				RunEnd++;

			if(vAttempts[RunStart].m_Attempt.m_From == POINT_START && vAttempts[RunEnd - 1].m_Attempt.m_To == POINT_FINISH)
			{
				int Total = 0;
				bool Practice = false;
				std::vector<int> vRoute = {POINT_START};
				for(size_t i = RunStart; i < RunEnd; i++)
				{
					Total += vAttempts[i].m_Attempt.m_Millis;
					Practice = Practice || vAttempts[i].m_Attempt.m_Practice;
					vRoute.push_back(vAttempts[i].m_Attempt.m_To);
				}
				if(!Practice && (Analysis.m_BestFinished < 0 || Total < Analysis.m_BestFinished))
					Analysis.m_BestFinished = Total;
				if(vAttempts[RunStart].m_Attempt.m_Timestamp >= LatestCompleteTimestamp)
				{
					LatestCompleteTimestamp = vAttempts[RunStart].m_Attempt.m_Timestamp;
					vLatestRoute = vRoute;
				}
			}
			RunStart = RunEnd;
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
					for(const SAttempt *pAttempt : vpList)
						Best = std::min(Best, pAttempt->m_Millis);
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
			SSegmentStats Stats = It != SegmentAttempts.end() ? MakeStats(Key.first, Key.second, It->second) : MakeStats(Key.first, Key.second, {});
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
				Analysis.m_vOtherStats.push_back(MakeStats(Key.first, Key.second, SegmentAttempts[Key]));

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

	void CSplitsData::ForgetPoint(int Point)
	{
		std::vector<SAttempt> vKept;
		for(size_t i = 0; i < m_vAttempts.size(); i++)
		{
			const SAttempt &Attempt = m_vAttempts[i];
			if(Attempt.m_To == Point)
			{
				if(i + 1 < m_vAttempts.size() && m_vAttempts[i + 1].m_RunId == Attempt.m_RunId && m_vAttempts[i + 1].m_From == Point)
				{
					const SAttempt &Next = m_vAttempts[i + 1];
					SAttempt Merged = Attempt;
					Merged.m_To = Next.m_To;
					Merged.m_Millis += Next.m_Millis;
					Merged.m_Practice = Merged.m_Practice || Next.m_Practice;
					if(Merged.m_From != Merged.m_To)
						vKept.push_back(Merged);
					i++;
				}
				continue; // nothing to merge with: drop it
			}
			if(Attempt.m_From == Point)
				continue; // the run started at this point: drop it
			vKept.push_back(Attempt);
		}
		m_vAttempts = vKept;
		SplitRunsAtGaps();
	}

	void CSplitsData::DeleteGate(int GateId)
	{
		ForgetPoint(GatePoint(GateId));
		m_vGates.erase(std::remove_if(m_vGates.begin(), m_vGates.end(), [&](const SGate &Gate) { return Gate.m_Id == GateId; }), m_vGates.end());
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

} // namespace DdnettySplits
