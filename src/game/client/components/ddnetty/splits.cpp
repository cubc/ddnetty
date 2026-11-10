// ddnetty: split times per map, see splits.h
#include "splits.h"

#include "ideal_demo.h"

#include <base/io.h>
#include <base/log.h>
#include <base/math.h>
#include <base/mem.h>
#include <base/str.h>
#include <base/time.h>

#include <engine/engine.h>
#include <engine/ghost.h>
#include <engine/graphics.h>
#include <engine/keys.h>
#include <engine/map.h>
#include <engine/shared/config.h>
#include <engine/shared/jobs.h>
#include <engine/shared/linereader.h>
#include <engine/storage.h>
#include <engine/textrender.h>

#include <generated/protocol.h>

#include <game/client/components/ghost.h>
#include <game/client/components/hud_layout.h>
#include <game/client/gameclient.h>
#include <game/collision.h>
#include <game/gamecore.h>
#include <game/localization.h>
#include <game/mapitems.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>

using namespace DdnettySplits;

namespace
{
	// distance from P to the segment A-B
	float DistanceToSegment(vec2 P, vec2 A, vec2 B)
	{
		const vec2 AB = B - A;
		const float LengthSquared = dot(AB, AB);
		if(LengthSquared <= 0.0f)
			return distance(P, A);
		const float T = std::clamp(dot(P - A, AB) / LengthSquared, 0.0f, 1.0f);
		return distance(P, A + AB * T);
	}

	// where the movement P0 -> P1 crosses the segment A-B, as a fraction of the movement
	bool CrossesSegment(vec2 P0, vec2 P1, vec2 A, vec2 B, double &Frac)
	{
		const vec2 R = P1 - P0;
		const vec2 S = B - A;
		const float Denominator = R.x * S.y - R.y * S.x;
		if(std::fabs(Denominator) < 1e-6f)
			return false;
		const vec2 QP = A - P0;
		const float T = (QP.x * S.y - QP.y * S.x) / Denominator;
		const float U = (QP.x * R.y - QP.y * R.x) / Denominator;
		if(T <= 0.0f || T > 1.0f || U < 0.0f || U > 1.0f)
			return false;
		Frac = T;
		return true;
	}

	bool SameOptions(const SOptions &A, const SOptions &B)
	{
		return A.m_UseTimecps == B.m_UseTimecps && A.m_UseGates == B.m_UseGates && A.m_CountPractice == B.m_CountPractice && A.m_UseAverage == B.m_UseAverage &&
		       A.m_Since == B.m_Since && A.m_FocusMarginMillis == B.m_FocusMarginMillis && A.m_RunEntriesOnly == B.m_RunEntriesOnly && A.m_CountDummy == B.m_CountDummy;
	}
} // namespace

void CSplits::OnInit()
{
	// demos made by DDNetty live in their own folder; move the ones saved by older versions
	const char *apOld[] = {"demos/segments", "demos/ideal"};
	const char *apNew[] = {"demos/ddnetty/segments", "demos/ddnetty/ideal"};
	for(int i = 0; i < 2; i++)
	{
		if(!Storage()->FolderExists(apOld[i], IStorage::TYPE_SAVE) || Storage()->FolderExists(apNew[i], IStorage::TYPE_SAVE))
			continue;
		Storage()->CreateFolder("demos/ddnetty", IStorage::TYPE_SAVE);
		if(Storage()->RenameFile(apOld[i], apNew[i], IStorage::TYPE_SAVE))
			log_info("splits", "moved '%s' to '%s'", apOld[i], apNew[i]);
		else
			log_warn("splits", "could not move '%s' to '%s'", apOld[i], apNew[i]);
	}
}

void CSplits::CreateDemoFolders()
{
	Storage()->CreateFolder("demos", IStorage::TYPE_SAVE);
	Storage()->CreateFolder("demos/ddnetty", IStorage::TYPE_SAVE);
	Storage()->CreateFolder("demos/ddnetty/segments", IStorage::TYPE_SAVE);
	Storage()->CreateFolder("demos/ddnetty/ideal", IStorage::TYPE_SAVE);
}

void CSplits::OnConsoleInit()
{
	Console()->Register("dt_splits_edit", "", CFGFLAG_CLIENT, ConEdit, this, "Toggle the edit mode for your split checkpoints");
	Console()->Register("dt_splits_source_cycle", "", CFGFLAG_CLIENT, ConSourceCycle, this, "Cycle the split points: map time checkpoints + yours, only map time checkpoints, only yours");
	Console()->Register("dt_splits_build_ideal", "", CFGFLAG_CLIENT, ConBuildIdeal, this, "Build the ideal run demo from the demos of your best segments");
	Console()->Register("dt_splits_delete_all_checkpoints", "", CFGFLAG_CLIENT, ConDeleteAllGates, this, "Delete all your checkpoints on this map, merging their times into the map's time checkpoints");
}

void CSplits::ConSourceCycle(IConsole::IResult *pResult, void *pUserData)
{
	CSplits *pSelf = static_cast<CSplits *>(pUserData);
	g_Config.m_DtSplitsSource = (g_Config.m_DtSplitsSource + 1) % 3;
	const char *apNames[] = {DtLocalize("Split points: map + mine"), DtLocalize("Split points: map time checkpoints"), DtLocalize("Split points: my checkpoints")};
	pSelf->m_Notice = apNames[g_Config.m_DtSplitsSource];
	pSelf->m_NoticeUntil = time_get() + 2 * time_freq();
	pSelf->ResetRun();
}

void CSplits::ConDeleteAllGates(IConsole::IResult *pResult, void *pUserData)
{
	static_cast<CSplits *>(pUserData)->DeleteAllGates();
}

void CSplits::ConEdit(IConsole::IResult *pResult, void *pUserData)
{
	CSplits *pSelf = static_cast<CSplits *>(pUserData);
	pSelf->SetEditMode(!pSelf->m_EditMode);
}

void CSplits::OnMapLoad()
{
	Load();
}

void CSplits::OnReset()
{
	ResetRun();
	SetEditMode(false);
	m_HudDiffUntil = 0;
}

SOptions CSplits::Options() const
{
	SOptions Options;
	Options.m_UseTimecps = g_Config.m_DtSplitsSource != 2;
	Options.m_UseGates = g_Config.m_DtSplitsSource != 1;
	Options.m_CountPractice = g_Config.m_DtSplitsPractice != 0;
	Options.m_UseAverage = g_Config.m_DtSplitsAverage != 0;
	Options.m_Since = m_Data.m_Since;
	Options.m_FocusMarginMillis = FocusMargin();
	Options.m_RunEntriesOnly = g_Config.m_DtSplitsRunEntriesOnly != 0;
	Options.m_CountDummy = g_Config.m_DtSplitsCountDummy != 0;
	return Options;
}

int CSplits::FocusMargin() const
{
	return m_Data.m_FocusMarginMillis >= 0 ? m_Data.m_FocusMarginMillis : g_Config.m_DtSplitsFocusMargin;
}

void CSplits::SetSince(int64_t Timestamp)
{
	m_Data.m_Since = std::max<int64_t>(Timestamp, 0);
	m_AnalysisDirty = true;
	Save();
}

void CSplits::SetFocusMarginOnMap(int Millis)
{
	m_Data.m_FocusMarginMillis = Millis < 0 ? -1 : std::min(Millis, 10 * 60 * 1000);
	m_AnalysisDirty = true;
	Save();
}

int CSplits::RawBest(int From, int To) const
{
	int Best = -1;
	for(const SAttempt &Attempt : m_Data.m_vAttempts)
		if(Attempt.m_From == From && Attempt.m_To == To && (Best < 0 || Attempt.m_Millis < Best))
			Best = Attempt.m_Millis;
	return Best;
}

const SAnalysis &CSplits::Analysis()
{
	const SOptions Current = Options();
	if(m_AnalysisDirty || !SameOptions(Current, m_AnalysisOptions))
	{
		m_Analysis = m_Data.Analyze(Current);
		m_AnalysisOptions = Current;
		m_AnalysisDirty = false;
	}
	return m_Analysis;
}

void CSplits::Load()
{
	m_Data.Clear();
	m_AnalysisDirty = true;
	m_EditMode = false;
	m_DragMode = DRAG_NONE;
	m_Panning = false;
	ResetRun();

	char aMap[128];
	str_copy(aMap, GameClient()->Map()->BaseName());
	str_sanitize_filename(aMap);
	char aSha256[SHA256_MAXSTRSIZE];
	sha256_str(GameClient()->Map()->Sha256(), aSha256, sizeof(aSha256));
	str_format(m_aPath, sizeof(m_aPath), "ddnetty/splits/%s_%.8s.txt", aMap, aSha256);
	str_format(m_aSegmentFolder, sizeof(m_aSegmentFolder), "demos/ddnetty/segments/%s_%.8s", aMap, aSha256);

	Storage()->CreateFolder("ddnetty", IStorage::TYPE_SAVE);
	Storage()->CreateFolder("ddnetty/splits", IStorage::TYPE_SAVE);
	str_format(m_aRefsPath, sizeof(m_aRefsPath), "ddnetty/splits/refs/%s.txt", aMap);
	LoadRefs();

	CLineReader LineReader;
	if(!LineReader.OpenFile(Storage()->OpenFile(m_aPath, IOFLAG_READ, IStorage::TYPE_SAVE)))
		return;
	int NumInvalid = 0;
	while(const char *pLine = LineReader.Get())
	{
		if(!m_Data.ParseLine(pLine))
			NumInvalid++;
	}
	if(NumInvalid > 0)
		log_warn("splits", "ignored %d invalid lines in '%s'", NumInvalid, m_aPath);
}

void CSplits::LoadRefs()
{
	m_Refs.Clear();
	if(!m_aRefsPath[0])
		return;
	CLineReader LineReader;
	if(!LineReader.OpenFile(Storage()->OpenFile(m_aRefsPath, IOFLAG_READ, IStorage::TYPE_ALL)))
		return;
	int NumInvalid = 0;
	while(const char *pLine = LineReader.Get())
		if(!m_Refs.ParseLine(pLine))
			NumInvalid++;
	if(NumInvalid > 0)
		log_warn("splits", "ignored %d invalid lines in '%s'", NumInvalid, m_aRefsPath);
	log_info("splits", "loaded %d top player runs and %d segment bests from '%s'", (int)m_Refs.m_vRuns.size(), (int)m_Refs.m_vSegments.size(), m_aRefsPath);
}

const SRefRun *CSplits::RefRun() const
{
	return m_Refs.FindRun(g_Config.m_DtSplitsRefPlayer);
}

void CSplits::Save()
{
	if(!HasMap())
		return;
	IOHANDLE File = Storage()->OpenFile(m_aPath, IOFLAG_WRITE, IStorage::TYPE_SAVE);
	if(!File)
	{
		log_error("splits", "failed to open '%s' for writing", m_aPath);
		return;
	}
	const std::string Text = m_Data.Serialize();
	io_write(File, Text.data(), Text.size());
	io_close(File);
}

void CSplits::Append(const SAttempt &Attempt)
{
	IOHANDLE File = Storage()->OpenFile(m_aPath, IOFLAG_APPEND, IStorage::TYPE_SAVE);
	if(!File)
	{
		log_error("splits", "failed to open '%s' for appending", m_aPath);
		return;
	}
	const std::string Line = CSplitsData::FormatAttempt(Attempt);
	io_write(File, Line.data(), Line.size());
	io_write_newline(File);
	io_close(File);
}

void CSplits::ResetRun()
{
	FlushClips(); // a run that ends is the moment to cut its best segments out of the replay recorder
	m_TrackedClientId = -1;
	m_LastProcessedTick = -1;
	m_LastPoint = POINT_NONE;
	m_LastUsedPoint = POINT_NONE;
	m_RouteIndex = -1;
	m_StartTick = -1.0;
	m_StartClientId = -1;
	m_SwitchedTee = false;
	m_PrimeFromPrev = false;
	m_InTimecp = -1;
	m_InStart = false;
	m_InFinish = false;
	m_EntryFlags = 0;
	m_UsedEntryFlags = 0;
	m_vRecentPoints.clear();
	m_HudPrediction = -1;
	m_HudUnlikely = false;
	m_HudHasRefDiff = false;
	m_HudDiffUntil = 0;
}

bool CSplits::IsPractice() const
{
	const int LocalId = GameClient()->m_Snap.m_LocalClientId;
	if(LocalId < 0)
		return false;
	const auto &Character = GameClient()->m_Snap.m_aCharacters[LocalId];
	return Character.m_HasExtendedDisplayInfo && (Character.m_ExtendedData.m_Flags & CHARACTERFLAG_PRACTICE_MODE);
}

void CSplits::OnNewSnapshot()
{
	if(m_FlushClipsAtTick >= 0 && Client()->State() == IClient::STATE_ONLINE && Client()->GameTick(g_Config.m_ClDummy) >= m_FlushClipsAtTick)
		FlushClips();

	// ddnetty fake ping lock: the ping the server shows of us is only fresh while our
	// scoreboard or menu is open (that's when the server refreshes it for us)
	if(g_Config.m_DtFakePing > 0 && Client()->State() == IClient::STATE_ONLINE && (GameClient()->m_Scoreboard.IsActive() || GameClient()->m_Menus.IsActive()))
	{
		const int LocalId = GameClient()->m_Snap.m_LocalClientId;
		if(LocalId >= 0 && GameClient()->m_Snap.m_apPlayerInfos[LocalId])
			Client()->FakePingObserved(GameClient()->m_Snap.m_apPlayerInfos[LocalId]->m_Latency);
	}

	if(!g_Config.m_DtSplits || !HasMap() || Client()->State() != IClient::STATE_ONLINE || !GameClient()->m_GameInfo.m_Race)
	{
		ResetRun();
		return;
	}

	const auto &Snap = GameClient()->m_Snap;
	const int LocalId = Snap.m_LocalClientId;
	if(LocalId < 0 || Snap.m_SpecInfo.m_Active || !Snap.m_aCharacters[LocalId].m_Active)
	{
		ResetRun();
		return;
	}
	if(LocalId != m_TrackedClientId)
	{
		const auto &aLocalIds = GameClient()->m_aLocalIds;
		const bool IsOwnTee = m_TrackedClientId >= 0 && (aLocalIds[0] == m_TrackedClientId || aLocalIds[1] == m_TrackedClientId) && (aLocalIds[0] == LocalId || aLocalIds[1] == LocalId);
		if(IsOwnTee && m_LastPoint != POINT_NONE)
		{
			// switched between the main tee and the dummy in the middle of a run: the run goes on,
			// only the tile state of the new tee has to be learned again
			if(m_StartTick < 0.0 && GameClient()->LastRaceTick() >= 0)
				m_StartTick = GameClient()->LastRaceTick(); // still the value of the tee we are leaving
			m_SwitchedTee = true;
			m_PrimeFromPrev = true;
			m_LastProcessedTick = -1;
			m_TrackedClientId = LocalId;
		}
		else
		{
			ResetRun();
			m_TrackedClientId = LocalId;
		}
	}

	const CNetObj_Character &Prev = Snap.m_aCharacters[LocalId].m_Prev;
	const CNetObj_Character &Cur = Snap.m_aCharacters[LocalId].m_Cur;
	if(Cur.m_Tick <= Prev.m_Tick || Cur.m_Tick <= m_LastProcessedTick)
		return;

	const vec2 PrevPos((float)Prev.m_X, (float)Prev.m_Y);
	const vec2 Pos((float)Cur.m_X, (float)Cur.m_Y);
	CCollision *pCollision = GameClient()->Collision();

	auto &&PrimeAt = [&](vec2 Position) {
		const int Index = pCollision->GetPureMapIndex(Position);
		m_InStart = pCollision->GetTileIndex(Index) == TILE_START || pCollision->GetFrontTileIndex(Index) == TILE_START;
		m_InFinish = pCollision->GetTileIndex(Index) == TILE_FINISH || pCollision->GetFrontTileIndex(Index) == TILE_FINISH;
		int Cp = pCollision->IsTimeCheckpoint(Index);
		if(Cp < 0)
			Cp = pCollision->IsFrontTimeCheckpoint(Index);
		m_InTimecp = Cp;
	};

	if(m_LastProcessedTick < 0)
	{
		if(!m_PrimeFromPrev)
		{
			// first snapshot after a reset: only learn where we are
			PrimeAt(Pos);
			m_LastProcessedTick = Cur.m_Tick;
			return;
		}
		// first snapshot after switching tee: start from where the new tee was one step ago,
		// so a checkpoint crossed right at the switch still counts
		m_PrimeFromPrev = false;
		PrimeAt(PrevPos);
	}

	// teleports: map teleporters are part of the route, anything else (rescue, /tc, ...) breaks the run
	const float Ticks = (float)(Cur.m_Tick - Prev.m_Tick);
	const vec2 PrevVel(Prev.m_VelX / 256.0f, Prev.m_VelY / 256.0f);
	const vec2 Vel(Cur.m_VelX / 256.0f, Cur.m_VelY / 256.0f);
	const float Expected = std::max(length(PrevVel), length(Vel)) * Ticks + 48.0f;
	if(distance(PrevPos, Pos) > Expected)
	{
		bool MapTeleport = false;
		std::vector<int> vIndices = pCollision->GetMapIndices(PrevPos, PrevPos + PrevVel * (Ticks + 1.0f));
		vIndices.push_back(pCollision->GetPureMapIndex(PrevPos));
		for(const int Index : vIndices)
		{
			if(pCollision->IsTeleport(Index) || pCollision->IsEvilTeleport(Index) || pCollision->IsCheckTeleport(Index) || pCollision->IsCheckEvilTeleport(Index))
			{
				MapTeleport = true;
				break;
			}
		}
		if(!MapTeleport)
		{
			ResetRun();
			m_TrackedClientId = LocalId;
		}
		PrimeAt(Pos);
		m_LastProcessedTick = Cur.m_Tick;
		return;
	}

	ProcessStep(PrevPos, Pos, Prev.m_Tick, Cur.m_Tick);
	m_LastProcessedTick = Cur.m_Tick;
}

void CSplits::ProcessStep(vec2 PrevPos, vec2 Pos, int PrevTick, int CurTick)
{
	struct SEvent
	{
		double m_Frac;
		int m_Point;
	};
	std::vector<SEvent> vEvents;
	CCollision *pCollision = GameClient()->Collision();

	// tiles: sample the movement in small steps
	const int Steps = std::clamp((int)std::ceil(distance(PrevPos, Pos) / 8.0f), 1, 4096);
	for(int Step = 1; Step <= Steps; Step++)
	{
		const double Frac = (double)Step / Steps;
		const int Index = pCollision->GetPureMapIndex(mix(PrevPos, Pos, (float)Frac));

		const bool Start = pCollision->GetTileIndex(Index) == TILE_START || pCollision->GetFrontTileIndex(Index) == TILE_START;
		if(m_InStart && !Start) // the race starts when leaving the start tiles
			vEvents.push_back({(double)(Step - 1) / Steps, POINT_START});
		m_InStart = Start;

		const bool Finish = pCollision->GetTileIndex(Index) == TILE_FINISH || pCollision->GetFrontTileIndex(Index) == TILE_FINISH;
		if(Finish && !m_InFinish)
			vEvents.push_back({Frac, POINT_FINISH});
		m_InFinish = Finish;

		int Cp = pCollision->IsTimeCheckpoint(Index);
		if(Cp < 0)
			Cp = pCollision->IsFrontTimeCheckpoint(Index);
		if(Cp >= 0 && Cp < NUM_TIMECPS && Cp != m_InTimecp)
			vEvents.push_back({Frac, Cp});
		m_InTimecp = Cp;
	}

	// gates: exact crossing
	for(const SGate &Gate : m_Data.m_vGates)
	{
		double Frac;
		if(CrossesSegment(PrevPos, Pos, vec2(Gate.m_Ax, Gate.m_Ay), vec2(Gate.m_Bx, Gate.m_By), Frac))
			vEvents.push_back({Frac, GatePoint(Gate.m_Id)});
	}

	std::stable_sort(vEvents.begin(), vEvents.end(), [](const SEvent &A, const SEvent &B) { return A.m_Frac < B.m_Frac; });
	for(const SEvent &Event : vEvents)
		OnPoint(Event.m_Point, PrevTick + Event.m_Frac * (CurTick - PrevTick));
}

void CSplits::OnPoint(int Point, double Tick)
{
	const double TickSpeed = Client()->GameTickSpeed();
	const SOptions Opts = Options();

	if(Point == POINT_START)
	{
		// the other tee leaving its own start line while the run is under way is not a new run
		if(m_SwitchedTee && m_StartClientId >= 0 && m_StartClientId != m_TrackedClientId && m_LastPoint != POINT_NONE && m_LastPoint != POINT_START)
			return;

		// (re)starting the race begins a new run
		m_StartClientId = m_TrackedClientId;
		m_SwitchedTee = false;
		m_RunId = m_Data.NextRunId();
		m_LastPoint = POINT_START;
		m_LastPointTick = Tick;
		m_LastUsedPoint = POINT_START;
		m_LastUsedPointTick = Tick;
		m_RouteIndex = 0;
		m_StartTick = Tick;
		m_EntryFlags = 0;
		m_UsedEntryFlags = 0;
		m_vRecentPoints.assign(1, POINT_START);
		m_HudPrediction = -1;
		m_HudUnlikely = false;
		m_HudDiffUntil = 0;
		return;
	}

	// how a segment from this point is entered
	int NewEntryFlags = 0;
	if(Point == m_LastPoint)
	{
		m_EntryFlags |= ATTFLAG_RECROSSED; // the segment starts over
	}
	else if(m_LastPoint == POINT_NONE)
	{
		NewEntryFlags |= ATTFLAG_MID_START;
	}
	else
	{
		// went back: the point was crossed a moment ago already
		const size_t NumRecent = m_vRecentPoints.size();
		for(size_t i = NumRecent > 4 ? NumRecent - 4 : 0; i + 1 < NumRecent; i++)
			if(m_vRecentPoints[i] == Point)
				NewEntryFlags |= ATTFLAG_AFTER_BACK;
		// waited before the point: the hop into it took much longer than its best
		if(g_Config.m_DtSplitsWaitLimit > 0)
		{
			const int Hop = (int)std::lround((Tick - m_LastPointTick) * 1000.0 / TickSpeed);
			const int Best = RawBest(m_LastPoint, Point);
			if(Hop - std::max(Best, 0) > g_Config.m_DtSplitsWaitLimit * 1000)
				NewEntryFlags |= ATTFLAG_LONG_WAIT;
		}
	}

	// HUD: compare with the stats from before this attempt
	if(CSplitsData::Includes(Point, Opts))
	{
		const SAnalysis &Current = Analysis();
		// follow the position on the route, so a checkpoint crossed twice (circular maps) is told apart
		int RouteIndex = Current.RouteIndex(Point, m_RouteIndex >= 0 ? m_RouteIndex + 1 : 0);
		if(RouteIndex < 0 && m_RouteIndex < 0)
			RouteIndex = Current.RouteIndex(Point);
		m_RouteIndex = RouteIndex;

		if(m_LastUsedPoint != POINT_NONE && m_LastUsedPoint != Point)
		{
			const int Millis = (int)std::lround((Tick - m_LastUsedPointTick) * 1000.0 / TickSpeed);
			const SSegmentStats *pStats = Current.Find(m_LastUsedPoint, Point);
			const int Reference = pStats ? CSplitsData::Reference(*pStats, Opts) : -1;
			m_HudHasDiff = Reference >= 0;
			m_HudDiff = Reference >= 0 ? Millis - Reference : 0;
			// a new best segment: remember it, its demo is cut when the run ends
			const bool Counts = (Opts.m_CountPractice || !IsPractice()) && (!Opts.m_RunEntriesOnly || !(m_UsedEntryFlags & ATTFLAGS_LOOSE_ENTRY));
			const bool NewBest = Counts && (!pStats || pStats->m_Best < 0 || Millis < pStats->m_Best);
			if(NewBest && SegmentDemosActive())
				QueueClip(m_LastUsedPoint, Point, Millis, m_LastUsedPointTick, Tick);
			if(NewBest && pStats && pStats->m_Best >= 0)
			{
				int RouteSegment = -1;
				for(size_t i = 0; i < Current.m_vRouteStats.size(); i++)
					if(Current.m_vRouteStats[i].m_From == m_LastUsedPoint && Current.m_vRouteStats[i].m_To == Point)
						RouteSegment = (int)i;
				ShowPbNotice(m_LastUsedPoint, Point, Millis, pStats->m_Best, RouteSegment, (m_UsedEntryFlags & ATTFLAGS_LOOSE_ENTRY) != 0);
			}
			if(m_HudHasDiff)
				m_HudDiffUntil = time_get() + 4 * time_freq();
		}

		m_HudPrediction = -1;
		m_HudUnlikely = false;
		double RaceStart = -1.0;
		// after switching tee the server timer belongs to the other tee, so the run keeps its own clock
		if(m_SwitchedTee && m_StartTick >= 0.0)
			RaceStart = m_StartTick;
		else if(GameClient()->LastRaceTick() >= 0)
			RaceStart = GameClient()->LastRaceTick();
		else if(m_StartTick >= 0.0)
			RaceStart = m_StartTick;
		// a top player's time at this point (only the map's time checkpoints are in the database)
		m_HudHasRefDiff = false;
		if(g_Config.m_DtSplitsTargetMode == 3 && RaceStart >= 0.0)
		{
			const SRefRun *pRef = RefRun();
			const int RefAt = pRef ? CSplitsRefs::TimeAt(*pRef, Point) : -1;
			if(RefAt >= 0)
			{
				m_HudHasRefDiff = true;
				m_HudRefDiff = (int)std::lround((Tick - RaceStart) * 1000.0 / TickSpeed) - RefAt;
				m_HudDiffUntil = time_get() + 4 * time_freq();
			}
		}
		const int Remaining = CSplitsData::Remaining(Current, m_RouteIndex, Opts);
		if(Point != POINT_FINISH && RaceStart >= 0.0 && Remaining >= 0)
		{
			m_HudPrediction = (int)std::lround((Tick - RaceStart) * 1000.0 / TickSpeed) + Remaining;
			const int Target = TargetMillis();
			m_HudUnlikely = Target > 0 && m_HudPrediction > Target;
		}

		if(m_LastUsedPoint == Point)
			m_UsedEntryFlags |= ATTFLAG_RECROSSED;
		else
			m_UsedEntryFlags = NewEntryFlags;
		m_LastUsedPoint = Point;
		m_LastUsedPointTick = Tick;
	}

	if(m_LastPoint == POINT_NONE)
	{
		// first point after a reset (e.g. practicing a part of the map)
		m_RunId = m_Data.NextRunId();
		m_LastPoint = Point;
		m_LastPointTick = Tick;
		m_EntryFlags = NewEntryFlags;
		m_vRecentPoints.assign(1, Point);
		return;
	}
	if(Point == m_LastPoint)
	{
		// went back through the same point: the segment starts again
		m_LastPointTick = Tick;
		return;
	}

	SAttempt Attempt;
	Attempt.m_RunId = m_RunId;
	Attempt.m_Timestamp = time_timestamp();
	Attempt.m_Practice = IsPractice();
	Attempt.m_From = m_LastPoint;
	Attempt.m_To = Point;
	Attempt.m_Millis = (int)std::lround((Tick - m_LastPointTick) * 1000.0 / TickSpeed);
	// two points crossed at the same moment still give a (0 ms) segment, otherwise the run would have a gap
	Attempt.m_Millis = std::max(Attempt.m_Millis, 0);
	Attempt.m_Flags = m_EntryFlags;
	const auto &aLocalIds = GameClient()->m_aLocalIds;
	if(aLocalIds[1] >= 0 && m_TrackedClientId == aLocalIds[1])
		Attempt.m_Flags |= ATTFLAG_DUMMY;
	if(m_SwitchedTee)
		Attempt.m_Flags |= ATTFLAG_SWITCHED;
	m_Data.m_vAttempts.push_back(Attempt);
	Append(Attempt);
	m_AnalysisDirty = true;

	m_LastPoint = Point;
	m_LastPointTick = Tick;
	m_EntryFlags = NewEntryFlags;
	m_vRecentPoints.push_back(Point);
	if(m_vRecentPoints.size() > 8)
		m_vRecentPoints.erase(m_vRecentPoints.begin());
	if(Point == POINT_FINISH)
	{
		m_LastPoint = POINT_NONE;
		m_LastUsedPoint = POINT_NONE;
		if(!m_vPendingClips.empty())
			m_FlushClipsAtTick = (int)std::ceil(Tick) + Client()->GameTickSpeed(); // 1 s after the finish is in the demo too
	}
}

void CSplits::OnMessage(int MsgType, void *pRawMsg)
{
	const int LocalId = GameClient()->m_Snap.m_LocalClientId;
	if(MsgType == NETMSGTYPE_SV_KILLMSG)
	{
		const CNetMsg_Sv_KillMsg *pMsg = static_cast<const CNetMsg_Sv_KillMsg *>(pRawMsg);
		if(LocalId >= 0 && pMsg->m_Victim == LocalId)
			ResetRun();
	}
	else if(MsgType == NETMSGTYPE_SV_KILLMSGTEAM)
	{
		const CNetMsg_Sv_KillMsgTeam *pMsg = static_cast<const CNetMsg_Sv_KillMsgTeam *>(pRawMsg);
		if(LocalId >= 0 && GameClient()->m_Teams.Team(LocalId) == pMsg->m_Team)
			ResetRun();
	}
}

std::string CSplits::PointName(int Point) const
{
	if(Point == POINT_START)
		return DtLocalize("Start");
	if(Point == POINT_FINISH)
		return DtLocalize("Finish");
	char aBuf[64];
	if(IsTimecp(Point))
	{
		str_format(aBuf, sizeof(aBuf), "CP %d", Point + 1);
		return aBuf;
	}
	if(IsGate(Point))
	{
		const SGate *pGate = m_Data.FindGate(GateIdOf(Point));
		if(pGate && !pGate->m_Name.empty())
			return pGate->m_Name;
		str_format(aBuf, sizeof(aBuf), "#%d", GateIdOf(Point));
		return aBuf;
	}
	return "?";
}

int CSplits::RecordMillis() const
{
	if(GameClient()->m_MapBestTimeSeconds <= 0 && GameClient()->m_MapBestTimeMillis <= 0)
		return -1;
	return GameClient()->m_MapBestTimeSeconds * 1000 + GameClient()->m_MapBestTimeMillis;
}

int CSplits::PlayerBestMillis() const
{
	const float Seconds = GameClient()->m_Hud.GetPlayerRecordSeconds(g_Config.m_ClDummy);
	if(Seconds <= 0.0f)
		return -1;
	return (int)std::lround(Seconds * 1000.0f);
}

void CSplits::FormatMillis(int Millis, char *pBuf, int BufSize)
{
	if(Millis < 0)
	{
		str_copy(pBuf, "—", BufSize);
		return;
	}
	const int Centis = Millis / 10;
	if(Centis >= 6000)
		str_format(pBuf, BufSize, "%d:%02d.%02d", Centis / 6000, (Centis / 100) % 60, Centis % 100);
	else
		str_format(pBuf, BufSize, "%d.%02d", Centis / 100, Centis % 100);
}

void CSplits::FormatDiff(int Millis, char *pBuf, int BufSize)
{
	const int Centis = std::abs(Millis) / 10;
	str_format(pBuf, BufSize, "%c%d.%02d", Millis < 0 ? '-' : '+', Centis / 100, Centis % 100);
}

int CSplits::ParseTime(const char *pStr)
{
	// "m:ss.cc", "ss.cc" or "ss"
	int Minutes = 0;
	const char *pColon = str_find(pStr, ":");
	const char *pSeconds = pStr;
	if(pColon)
	{
		char aMinutes[16];
		str_truncate(aMinutes, sizeof(aMinutes), pStr, pColon - pStr);
		if(!str_toint(aMinutes, &Minutes) || Minutes < 0)
			return -1;
		pSeconds = pColon + 1;
	}
	float Seconds = 0.0f;
	if(!str_tofloat(pSeconds, &Seconds) || Seconds < 0.0f || (pColon && Seconds >= 60.0f))
		return -1;
	const int Millis = Minutes * 60000 + (int)std::lround(Seconds * 1000.0f);
	return Millis > 0 ? Millis : -1;
}

int CSplits::TargetMillis() const
{
	if(g_Config.m_DtSplitsTargetMode == 1)
	{
		const int Best = PlayerBestMillis();
		return Best > 0 ? Best : m_Analysis.m_BestFinished;
	}
	if(g_Config.m_DtSplitsTargetMode == 2)
		return m_Data.m_TargetMillis;
	if(g_Config.m_DtSplitsTargetMode == 3)
	{
		const SRefRun *pRun = RefRun();
		return pRun ? pRun->m_Millis : RecordMillis();
	}
	return RecordMillis();
}

const char *CSplits::WarningMessage() const
{
	return g_Config.m_DtSplitsMessage[0] ? g_Config.m_DtSplitsMessage : DtLocalize("Rank 1 unlikely.");
}

bool CSplits::HidesServerTimeCpDiff() const
{
	return g_Config.m_DtSplits && g_Config.m_DtSplitsHud && HasMap() && GameClient()->m_GameInfo.m_Race;
}

void CSplits::SetCustomTarget(int Millis)
{
	m_Data.m_TargetMillis = Millis > 0 ? Millis : -1;
	Save();
}

void CSplits::DeleteRuns(const std::vector<int> &vRunIds)
{
	m_Data.DeleteRuns(vRunIds);
	m_AnalysisDirty = true;
	ResetRun();
	Save();
}

void CSplits::DeleteSegment(int From, int To)
{
	m_Data.DeleteSegment(From, To, Options());
	m_AnalysisDirty = true;
	ResetRun();
	Save();
}

void CSplits::DeleteGate(int GateId)
{
	if(m_RenamingGate == GateId)
		StopRenaming(false);
	m_Data.DeleteGate(GateId);
	m_PendingDeleteGate = -1;
	GatesChanged();
}

void CSplits::DeleteAllGates()
{
	StopRenaming(false);
	m_Data.DeleteAllGates();
	m_PendingDeleteGate = -1;
	GatesChanged();
}

void CSplits::RenameGate(int GateId, const char *pName)
{
	SGate *pGate = m_Data.FindGate(GateId);
	if(!pGate)
		return;
	std::string Name = pName;
	std::replace(Name.begin(), Name.end(), '\n', ' ');
	std::replace(Name.begin(), Name.end(), '\r', ' ');
	if(Name.size() > 32)
		Name.resize(32);
	pGate->m_Name = Name;
	Save();
}

void CSplits::GatesChanged()
{
	m_AnalysisDirty = true;
	ResetRun();
	Save();
}

// --- edit mode ---

void CSplits::SetEditMode(bool EditMode)
{
	const bool WasEditing = m_EditMode;
	m_EditMode = EditMode && HasMap();
	m_DragMode = DRAG_NONE;
	m_DragGate = -1;
	m_Panning = false;
	if(m_EditMode && !WasEditing)
	{
		// start with the cursor where the tee is aiming
		m_EditCameraOffset = vec2(0.0f, 0.0f);
		m_EditCursor = GameClient()->m_Controls.m_aTargetPos[g_Config.m_ClDummy] - GameClient()->m_Camera.m_Center;
	}
	if(!m_EditMode)
	{
		m_EditCameraOffset = vec2(0.0f, 0.0f);
		StopRenaming(false);
		m_PendingDeleteGate = -1;
	}
}

void CSplits::StopRenaming(bool Save)
{
	if(m_RenamingGate < 0)
		return;
	if(Save)
		RenameGate(m_RenamingGate, m_RenameInput.GetString());
	m_RenameInput.Deactivate();
	m_RenamingGate = -1;
}

vec2 CSplits::VisibleHalfSize() const
{
	float Width, Height;
	Graphics()->CalcScreenParams(Graphics()->ScreenAspect(), GameClient()->m_Camera.m_Zoom, &Width, &Height);
	return vec2(Width, Height) / 2.0f;
}

vec2 CSplits::CursorPos() const
{
	if(m_EditMode)
		return GameClient()->m_Camera.m_Center + m_EditCursor;
	return GameClient()->m_Controls.m_aTargetPos[g_Config.m_ClDummy];
}

void CSplits::ApplyEditCamera(vec2 &Center) const
{
	if(m_EditMode)
		Center += m_EditCameraOffset;
}

bool CSplits::OnCursorMove(float x, float y, IInput::ECursorType CursorType)
{
	if(!m_EditMode || GameClient()->m_Menus.IsActive() || GameClient()->m_GameConsole.IsActive() || GameClient()->m_Chat.IsActive())
		return false;

	// move like the spectator free view: faster when zoomed out
	const float Factor = g_Config.m_InpMousesens / 100.0f * GameClient()->m_Camera.m_Zoom;
	const vec2 Delta = vec2(x, y) * Factor;
	if(m_Panning)
	{
		// dragging with the middle button moves the view, the cursor stays on the same spot of the screen
		m_EditCameraOffset -= Delta;
		return true;
	}
	m_EditCursor += Delta;

	// keep the cursor on screen, pushing the view when it reaches an edge
	const vec2 Half = VisibleHalfSize();
	const vec2 Clamped(std::clamp(m_EditCursor.x, -Half.x, Half.x), std::clamp(m_EditCursor.y, -Half.y, Half.y));
	m_EditCameraOffset += m_EditCursor - Clamped;
	m_EditCursor = Clamped;
	return true;
}

vec2 CSplits::SnapPos(vec2 Pos) const
{
	if(Input()->ShiftIsPressed())
		return Pos;
	const float Grid = 16.0f; // half a tile
	return vec2(std::round(Pos.x / Grid) * Grid, std::round(Pos.y / Grid) * Grid);
}

int CSplits::GateAt(vec2 Pos, float MaxDistance) const
{
	int Best = -1;
	float BestDistance = MaxDistance;
	for(const SGate &Gate : m_Data.m_vGates)
	{
		const float Distance = DistanceToSegment(Pos, vec2(Gate.m_Ax, Gate.m_Ay), vec2(Gate.m_Bx, Gate.m_By));
		if(Distance <= BestDistance)
		{
			Best = Gate.m_Id;
			BestDistance = Distance;
		}
	}
	return Best;
}

void CSplits::BeginDrag()
{
	const vec2 Pos = CursorPos();
	// grab the end of an existing checkpoint
	for(const SGate &Gate : m_Data.m_vGates)
	{
		if(distance(Pos, vec2(Gate.m_Ax, Gate.m_Ay)) <= 12.0f * GameClient()->m_Camera.m_Zoom)
		{
			m_DragMode = DRAG_END_A;
			m_DragGate = Gate.m_Id;
			return;
		}
		if(distance(Pos, vec2(Gate.m_Bx, Gate.m_By)) <= 12.0f * GameClient()->m_Camera.m_Zoom)
		{
			m_DragMode = DRAG_END_B;
			m_DragGate = Gate.m_Id;
			return;
		}
	}
	m_DragMode = DRAG_NEW;
	m_DragStart = SnapPos(Pos);
}

void CSplits::EndDrag()
{
	const vec2 End = SnapPos(CursorPos());
	if(m_DragMode == DRAG_NEW)
	{
		if(distance(m_DragStart, End) >= 16.0f)
		{
			SGate Gate;
			Gate.m_Id = m_Data.NextGateId();
			Gate.m_Ax = m_DragStart.x;
			Gate.m_Ay = m_DragStart.y;
			Gate.m_Bx = End.x;
			Gate.m_By = End.y;
			m_Data.m_vGates.push_back(Gate);
			GatesChanged();
		}
	}
	else if(m_DragMode == DRAG_END_A || m_DragMode == DRAG_END_B)
	{
		SGate *pGate = m_Data.FindGate(m_DragGate);
		if(pGate)
		{
			const vec2 Other = m_DragMode == DRAG_END_A ? vec2(pGate->m_Bx, pGate->m_By) : vec2(pGate->m_Ax, pGate->m_Ay);
			const vec2 Old = m_DragMode == DRAG_END_A ? vec2(pGate->m_Ax, pGate->m_Ay) : vec2(pGate->m_Bx, pGate->m_By);
			if(distance(End, Other) >= 16.0f && distance(End, Old) > 0.0f)
			{
				// the checkpoint moved: its old times don't mean anything anymore
				m_Data.ForgetPoint(GatePoint(pGate->m_Id));
				pGate = m_Data.FindGate(m_DragGate);
				if(m_DragMode == DRAG_END_A)
				{
					pGate->m_Ax = End.x;
					pGate->m_Ay = End.y;
				}
				else
				{
					pGate->m_Bx = End.x;
					pGate->m_By = End.y;
				}
				GatesChanged();
			}
		}
	}
	m_DragMode = DRAG_NONE;
	m_DragGate = -1;
}

bool CSplits::OnInput(const IInput::CEvent &Event)
{
	if(!m_EditMode)
		return false;
	if(GameClient()->m_Menus.IsActive() || GameClient()->m_GameConsole.IsActive() || GameClient()->m_Chat.IsActive())
		return false;

	if(m_RenamingGate >= 0)
	{
		// typing a checkpoint name: the keyboard belongs to the text box
		if(Event.m_Flags & IInput::FLAG_PRESS)
		{
			if(Event.m_Key == KEY_RETURN || Event.m_Key == KEY_KP_ENTER)
			{
				StopRenaming(true);
				return true;
			}
			if(Event.m_Key == KEY_ESCAPE)
			{
				StopRenaming(false);
				return true;
			}
		}
		if(Event.m_Key != KEY_MOUSE_1 && Event.m_Key != KEY_MOUSE_2 && Event.m_Key != KEY_MOUSE_3)
		{
			m_RenameInput.ProcessInput(Event);
			return true;
		}
		StopRenaming(true); // clicking somewhere else saves
	}

	if(Event.m_Key == KEY_ESCAPE && (Event.m_Flags & IInput::FLAG_PRESS))
	{
		SetEditMode(false);
		return true;
	}
	if(Event.m_Key == KEY_MOUSE_1)
	{
		if(Event.m_Flags & IInput::FLAG_PRESS)
		{
			// double click on a checkpoint renames it
			const int Gate = GateAt(CursorPos(), 12.0f * GameClient()->m_Camera.m_Zoom);
			const int64_t Now = time_get();
			if(Gate >= 0 && Gate == m_LastClickGate && Now - m_LastClickTime < time_freq() * 35 / 100)
			{
				m_DragMode = DRAG_NONE;
				m_LastClickGate = -1;
				const SGate *pGate = m_Data.FindGate(Gate);
				m_RenameInput.Set(pGate ? pGate->m_Name.c_str() : "");
				m_RenameInput.SelectAll();
				m_RenameInput.Activate(EInputPriority::CHAT);
				m_RenamingGate = Gate;
				return true;
			}
			m_LastClickGate = Gate;
			m_LastClickTime = Now;
			BeginDrag();
		}
		if((Event.m_Flags & IInput::FLAG_RELEASE) && m_DragMode != DRAG_NONE)
			EndDrag();
		return true;
	}
	if(Event.m_Key == KEY_MOUSE_3)
	{
		if(Event.m_Flags & IInput::FLAG_PRESS)
			m_Panning = true;
		if(Event.m_Flags & IInput::FLAG_RELEASE)
			m_Panning = false;
		return true;
	}
	if(Event.m_Key == KEY_MOUSE_WHEEL_UP || Event.m_Key == KEY_MOUSE_WHEEL_DOWN)
	{
		if(Event.m_Flags & IInput::FLAG_PRESS)
			Console()->ExecuteLine(Event.m_Key == KEY_MOUSE_WHEEL_UP ? "zoom+" : "zoom-", -1);
		return true;
	}
	if(Event.m_Key == KEY_MOUSE_2)
	{
		if(Event.m_Flags & IInput::FLAG_PRESS)
		{
			const int Gate = GateAt(CursorPos(), 12.0f * GameClient()->m_Camera.m_Zoom);
			if(Gate >= 0)
			{
				// the last checkpoint takes its times with it, so that needs a second right click
				const bool NeedsConfirm = m_Data.GateDeleteCount(Gate) > 0;
				if(!NeedsConfirm || (m_PendingDeleteGate == Gate && time_get() < m_PendingDeleteUntil))
					DeleteGate(Gate);
				else
				{
					m_PendingDeleteGate = Gate;
					m_PendingDeleteUntil = time_get() + 3 * time_freq();
				}
			}
		}
		return true;
	}
	return false;
}

// --- rendering ---

void CSplits::OnRender()
{
	UpdateClipJobs();
	if(Client()->State() != IClient::STATE_ONLINE || !HasMap())
		return;
	// ddnetty focus mode: the edit mode always shows the checkpoints
	if(m_EditMode || !GameClient()->m_Focus.Hides(CFocus::ITEM_GATES))
		RenderGates();
	if(!GameClient()->m_HudEditor.IsActive() && !GameClient()->m_Focus.Hides(CFocus::ITEM_SPLITS))
		RenderHudBox(false);
	RenderEditHelp();
	RenderRenameBox();
}

vec2 CSplits::WorldToHud(vec2 Pos, float HudWidth, float HudHeight) const
{
	const vec2 Half = VisibleHalfSize();
	const vec2 TopLeft = GameClient()->m_Camera.m_Center - Half;
	return vec2((Pos.x - TopLeft.x) / (Half.x * 2.0f) * HudWidth, (Pos.y - TopLeft.y) / (Half.y * 2.0f) * HudHeight);
}

void CSplits::RenderRenameBox()
{
	if(m_RenamingGate < 0)
		return;
	const SGate *pGate = m_Data.FindGate(m_RenamingGate);
	if(!pGate)
	{
		StopRenaming(false);
		return;
	}
	const float Height = 300.0f;
	const float Width = Height * Graphics()->ScreenAspect();
	Graphics()->MapScreen(CScreenRect(0.0f, 0.0f, Width, Height));

	const vec2 Middle = WorldToHud(vec2((pGate->m_Ax + pGate->m_Bx) / 2.0f, (pGate->m_Ay + pGate->m_By) / 2.0f), Width, Height);
	const float FontSize = 7.0f;
	CUIRect Box = {Middle.x - 50.0f, Middle.y - 22.0f, 100.0f, 11.0f};
	Box.x = std::clamp(Box.x, 2.0f, Width - Box.w - 2.0f);
	Box.y = std::clamp(Box.y, 2.0f, Height - Box.h - 2.0f);
	Box.Draw(ColorRGBA(0.0f, 0.0f, 0.0f, 0.75f), IGraphics::CORNER_ALL, 2.0f);
	CUIRect TextRect;
	Box.Margin(2.0f, &TextRect);
	TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
	m_RenameInput.Render(&TextRect, FontSize, TEXTALIGN_ML, true, -1.0f, 0.0f);

	const char *pHint = DtLocalize("Enter saves · Esc cancels");
	TextRender()->TextColor(1.0f, 1.0f, 1.0f, 0.7f);
	TextRender()->Text(Box.x + (Box.w - TextRender()->TextWidth(5.0f, pHint)) / 2.0f, Box.y + Box.h + 1.0f, 5.0f, pHint);
	TextRender()->TextColor(TextRender()->DefaultTextColor());
}

void CSplits::RenderGates()
{
	if(!m_EditMode && (!g_Config.m_DtSplitsShowGates || m_Data.m_vGates.empty()))
		return;

	const vec2 Center = GameClient()->m_Camera.m_Center;
	const float Zoom = GameClient()->m_Camera.m_Zoom;
	Graphics()->MapScreenToInterface(Center.x, Center.y, Zoom);

	const vec2 Cursor = SnapPos(CursorPos());
	const int Hovered = m_EditMode && m_DragMode == DRAG_NONE ? GateAt(CursorPos(), 12.0f * Zoom) : -1;

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
	auto &&DrawDot = [&](vec2 Pos, float Size, ColorRGBA Color) {
		DrawLine(Pos - vec2(Size / 2.0f, 0.0f), Pos + vec2(Size / 2.0f, 0.0f), Size, Color);
	};

	const ColorRGBA GateColor(0.3f, 0.85f, 1.0f, 0.8f);
	const ColorRGBA HoverColor(1.0f, 0.35f, 0.35f, 0.9f);
	const ColorRGBA DragColor(1.0f, 0.85f, 0.2f, 0.9f);
	// keep lines and handles the same size on screen when zooming
	const float LineWidth = 4.0f * std::max(Zoom, 1.0f);
	const float HandleSize = 8.0f * std::max(Zoom, 1.0f);

	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	for(const SGate &Gate : m_Data.m_vGates)
	{
		vec2 A(Gate.m_Ax, Gate.m_Ay);
		vec2 B(Gate.m_Bx, Gate.m_By);
		const bool PendingDelete = Gate.m_Id == m_PendingDeleteGate && time_get() < m_PendingDeleteUntil;
		ColorRGBA Color = Gate.m_Id == Hovered || PendingDelete ? HoverColor : GateColor;
		if(Gate.m_Id == m_DragGate)
		{
			Color = DragColor;
			if(m_DragMode == DRAG_END_A)
				A = Cursor;
			else if(m_DragMode == DRAG_END_B)
				B = Cursor;
		}
		DrawLine(A, B, LineWidth, Color);
		if(m_EditMode)
		{
			DrawDot(A, HandleSize, ColorRGBA(1.0f, 1.0f, 1.0f, 0.9f));
			DrawDot(B, HandleSize, ColorRGBA(1.0f, 1.0f, 1.0f, 0.9f));
		}
	}
	if(m_DragMode == DRAG_NEW)
		DrawLine(m_DragStart, Cursor, LineWidth, DragColor);
	if(m_EditMode)
	{
		// crosshair
		const float Size = 10.0f * Zoom;
		DrawLine(Cursor - vec2(Size, 0.0f), Cursor + vec2(Size, 0.0f), 2.0f * Zoom, DragColor);
		DrawLine(Cursor - vec2(0.0f, Size), Cursor + vec2(0.0f, Size), 2.0f * Zoom, DragColor);
	}
	Graphics()->QuadsEnd();

	// names above the middle of each checkpoint
	TextRender()->TextColor(1.0f, 1.0f, 1.0f, 0.9f);
	const float FontSize = 14.0f * std::max(Zoom, 1.0f);
	for(const SGate &Gate : m_Data.m_vGates)
	{
		const std::string Name = PointName(GatePoint(Gate.m_Id));
		const vec2 Middle((Gate.m_Ax + Gate.m_Bx) / 2.0f, (Gate.m_Ay + Gate.m_By) / 2.0f);
		const float Width = TextRender()->TextWidth(FontSize, Name.c_str());
		TextRender()->Text(Middle.x - Width / 2.0f, Middle.y - FontSize * 2.0f, FontSize, Name.c_str());
	}
	TextRender()->TextColor(TextRender()->DefaultTextColor());
}

void CSplits::ShowPbNotice(int From, int To, int Millis, int OldBest, int RouteSegment, bool LooseEntry)
{
	if(!g_Config.m_DtSplitsPbNotice)
		return;
	char aTime[32], aDiff[32];
	FormatMillis(Millis, aTime, sizeof(aTime));
	if(LooseEntry)
		str_append(aTime, "*");
	FormatDiff(Millis - OldBest, aDiff, sizeof(aDiff));
	char aBuf[256];
	const SRefSegment *pOther = g_Config.m_DtSplitsPbNotice >= 2 ? m_Refs.FindSegment(From, To) : nullptr;
	m_PbNoticeGold = pOther && Millis < pOther->m_Millis;
	if(m_PbNoticeGold)
	{
		char aOther[32];
		FormatMillis(pOther->m_Millis, aOther, sizeof(aOther));
		str_format(aBuf, sizeof(aBuf), DtLocalize("%s → %s  %s  best of all (before %s, %s)"), PointName(From).c_str(), PointName(To).c_str(), aTime, aOther, pOther->m_Name.c_str());
	}
	else if(LooseEntry)
		str_format(aBuf, sizeof(aBuf), DtLocalize("%s → %s  %s  new best (%s), loose entry"), PointName(From).c_str(), PointName(To).c_str(), aTime, aDiff);
	else
		str_format(aBuf, sizeof(aBuf), DtLocalize("%s → %s  %s  new best (%s)"), PointName(From).c_str(), PointName(To).c_str(), aTime, aDiff);
	m_aPbNotice[0] = aBuf;
	m_aPbNotice[1].clear();
	const SOptions Opts = Options();
	const SAnalysis &Current = Analysis();
	if(RouteSegment >= 0 && !Opts.m_UseAverage && Current.m_BuiltRun >= 0)
	{
		char aIdeal[32];
		FormatMillis(Current.m_BuiltRun - (OldBest - Millis), aIdeal, sizeof(aIdeal));
		str_format(aBuf, sizeof(aBuf), DtLocalize("Ideal run %s (%s)"), aIdeal, aDiff);
		m_aPbNotice[1] = aBuf;
	}
	m_PbNoticeUntil = time_get() + 3 * time_freq();
	if(g_Config.m_DtSplitsPbSound)
		GameClient()->m_Sounds.Play(CSounds::CHN_GUI, SOUND_CHAT_HIGHLIGHT, 0.6f);
}

void CSplits::RenderEditHelp()
{
	const bool HideNotices = GameClient()->m_Focus.Hides(CFocus::ITEM_NOTICES);
	const bool ShowNotice = time_get() < m_NoticeUntil && !m_Notice.empty() && !HideNotices;
	const bool ShowPb = time_get() < m_PbNoticeUntil && !m_aPbNotice[0].empty() && !HideNotices;
	if(!m_EditMode && !ShowNotice && !ShowPb)
		return;
	const float Height = 300.0f;
	const float Width = Height * Graphics()->ScreenAspect();
	Graphics()->MapScreen(CScreenRect(0.0f, 0.0f, Width, Height));
	auto &&DrawCentered = [&](float Y, float FontSize, const char *pText, ColorRGBA Color) {
		TextRender()->TextColor(Color);
		TextRender()->Text(Width / 2.0f - TextRender()->TextWidth(FontSize, pText) / 2.0f, Y, FontSize, pText);
	};
	if(ShowNotice)
		DrawCentered(100.0f, 8.0f, m_Notice.c_str(), ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f));
	if(ShowPb && !m_EditMode)
	{
		DrawCentered(84.0f, 8.0f, m_aPbNotice[0].c_str(), m_PbNoticeGold ? ColorRGBA(1.0f, 0.82f, 0.25f, 1.0f) : ColorRGBA(0.45f, 1.0f, 0.55f, 1.0f));
		if(!m_aPbNotice[1].empty())
			DrawCentered(93.0f, 6.5f, m_aPbNotice[1].c_str(), ColorRGBA(0.75f, 0.85f, 1.0f, 1.0f));
	}
	if(m_EditMode)
	{
		DrawCentered(75.0f, 7.0f, DtLocalize("Checkpoint edit mode: drag to create · drag an end to move · double click renames · right click deletes · Esc exits"), ColorRGBA(1.0f, 0.85f, 0.2f, 1.0f));
		DrawCentered(84.0f, 6.0f, DtLocalize("Mouse wheel zooms, hold the middle button to move the view. Shift places without snapping."), ColorRGBA(1.0f, 1.0f, 1.0f, 0.7f));
		if(m_PendingDeleteGate >= 0 && time_get() < m_PendingDeleteUntil)
		{
			char aBuf[160];
			str_format(aBuf, sizeof(aBuf), DtLocalize("Right click again: deletes this checkpoint and %d times that can't be merged"), m_Data.GateDeleteCount(m_PendingDeleteGate));
			DrawCentered(110.0f, 7.0f, aBuf, ColorRGBA(1.0f, 0.4f, 0.4f, 1.0f));
		}
	}
	TextRender()->TextColor(TextRender()->DefaultTextColor());
}

// --- HUD box (a BestClient HUD layout module, like the finish prediction) ---

bool CSplits::HudLines(SHudLines &Lines, bool Preview) const
{
	if(Preview)
	{
		Lines.m_ShowDiff = true;
		Lines.m_Diff = -20;
		Lines.m_Prediction = 598200;
		Lines.m_Unlikely = g_Config.m_DtSplitsWarning != 0;
		Lines.m_Ideal = g_Config.m_DtSplitsHudIdeal ? 592100 : -1;
		Lines.m_ShowRefDiff = g_Config.m_DtSplitsTargetMode == 3;
		Lines.m_RefDiff = -2410;
		Lines.m_RefName = "Oak";
		return true;
	}
	if(!g_Config.m_DtSplits || !g_Config.m_DtSplitsHud || !g_Config.m_ClShowhud || !GameClient()->m_GameInfo.m_Race)
		return false;
	Lines.m_ShowDiff = m_HudHasDiff && time_get() < m_HudDiffUntil;
	Lines.m_Diff = m_HudDiff;
	Lines.m_Prediction = m_HudPrediction;
	Lines.m_Unlikely = m_HudUnlikely && g_Config.m_DtSplitsWarning != 0;
	if(g_Config.m_DtSplitsHudIdeal && (Lines.m_ShowDiff || Lines.m_Prediction >= 0))
		Lines.m_Ideal = m_Analysis.m_BuiltRun;
	Lines.m_ShowRefDiff = m_HudHasRefDiff && time_get() < m_HudDiffUntil;
	if(Lines.m_ShowRefDiff)
	{
		const SRefRun *pRef = RefRun();
		Lines.m_ShowRefDiff = pRef != nullptr;
		Lines.m_RefDiff = m_HudRefDiff;
		if(pRef)
			Lines.m_RefName = pRef->m_Name;
	}
	return Lines.m_ShowDiff || Lines.m_Prediction >= 0 || Lines.m_ShowRefDiff;
}

CUIRect CSplits::HudRect(const SHudLines &Lines, float HudWidth, float HudHeight) const
{
	const auto Layout = HudLayout::Get(HudLayout::MODULE_DDNETTY_SPLITS, HudWidth, HudHeight);
	const float Scale = std::clamp(Layout.m_Scale / 100.0f, 0.25f, 3.0f);
	const float DiffFontSize = 6.5f * Scale;
	const float LineFontSize = 5.25f * Scale;
	const float PaddingX = 6.0f * Scale;
	const float PaddingY = 4.0f * Scale;
	const float Gap = 1.5f * Scale;

	char aSample[96];
	str_format(aSample, sizeof(aSample), DtLocalize("Prediction: %s"), "00:00.00");
	float Width = std::max(TextRender()->TextWidth(DiffFontSize, "+00.00"), TextRender()->TextWidth(LineFontSize, aSample));
	float Height = 0.0f;
	int NumLines = 0;
	if(Lines.m_ShowDiff)
	{
		Height += DiffFontSize;
		NumLines++;
	}
	if(Lines.m_Prediction >= 0)
	{
		Height += LineFontSize;
		NumLines++;
	}
	if(Lines.m_Ideal >= 0)
	{
		Height += LineFontSize;
		NumLines++;
	}
	if(Lines.m_ShowRefDiff)
	{
		char aRef[96];
		str_format(aRef, sizeof(aRef), DtLocalize("%s vs %s"), "+00.00", Lines.m_RefName.c_str());
		Width = std::max(Width, TextRender()->TextWidth(LineFontSize, aRef));
		Height += LineFontSize;
		NumLines++;
	}
	if(Lines.m_Unlikely)
	{
		Width = std::max(Width, TextRender()->TextWidth(LineFontSize, WarningMessage()));
		Height += LineFontSize;
		NumLines++;
	}
	if(NumLines > 1)
		Height += Gap * (NumLines - 1);

	CUIRect Rect = {Layout.m_X, Layout.m_Y, Width + PaddingX * 2.0f, Height + PaddingY * 2.0f};
	Rect.x = std::clamp(Rect.x, 0.0f, std::max(0.0f, HudWidth - Rect.w));
	Rect.y = std::clamp(Rect.y, 0.0f, std::max(0.0f, HudHeight - Rect.h));
	return Rect;
}

CUIRect CSplits::GetHudEditorRect() const
{
	SHudLines Lines;
	HudLines(Lines, true);
	const float HudHeight = HudLayout::CANVAS_HEIGHT;
	const float HudWidth = HudHeight * Graphics()->ScreenAspect();
	return HudRect(Lines, HudWidth, HudHeight);
}

void CSplits::RenderHudPreview()
{
	RenderHudBox(true);
}

void CSplits::RenderHudBox(bool Preview)
{
	if(!Preview && !HudLayout::IsEnabled(HudLayout::MODULE_DDNETTY_SPLITS))
		return;
	SHudLines Lines;
	if(!HudLines(Lines, Preview))
		return;

	const float HudHeight = HudLayout::CANVAS_HEIGHT;
	const float HudWidth = HudHeight * Graphics()->ScreenAspect();
	Graphics()->MapScreenToSize(HudWidth, HudHeight);

	const CUIRect Rect = HudRect(Lines, HudWidth, HudHeight);
	const auto Layout = HudLayout::Get(HudLayout::MODULE_DDNETTY_SPLITS, HudWidth, HudHeight);
	const float Scale = std::clamp(Layout.m_Scale / 100.0f, 0.25f, 3.0f);
	const float Alpha = HudLayout::AlphaFactor(HudLayout::MODULE_DDNETTY_SPLITS);
	const float DiffFontSize = 6.5f * Scale;
	const float LineFontSize = 5.25f * Scale;
	const float PaddingY = 4.0f * Scale;
	const float Gap = 1.5f * Scale;

	if(Layout.m_BackgroundEnabled)
	{
		const ColorRGBA Background = color_cast<ColorRGBA>(ColorHSLA(Layout.m_BackgroundColor, true)).WithMultipliedAlpha(Alpha);
		const int Corners = HudLayout::BackgroundCorners(IGraphics::CORNER_ALL, Rect.x, Rect.y, Rect.w, Rect.h, HudWidth, HudHeight);
		Graphics()->DrawRect(Rect.x, Rect.y, Rect.w, Rect.h, Background, Corners, 5.0f * Scale);
	}

	auto &&DrawCentered = [&](float Y, float FontSize, const char *pText, ColorRGBA Color) {
		TextRender()->TextColor(Color.WithMultipliedAlpha(Alpha));
		TextRender()->Text(Rect.x + (Rect.w - TextRender()->TextWidth(FontSize, pText)) / 2.0f, Y, FontSize, pText);
	};

	const ColorRGBA Green(0.5f, 1.0f, 0.5f, 1.0f);
	const ColorRGBA Red(1.0f, 0.5f, 0.5f, 1.0f);
	float Y = Rect.y + PaddingY;
	char aBuf[128], aTime[32];
	if(Lines.m_ShowDiff)
	{
		FormatDiff(Lines.m_Diff, aBuf, sizeof(aBuf));
		DrawCentered(Y, DiffFontSize, aBuf, Lines.m_Diff <= 0 ? Green : Red);
		Y += DiffFontSize + Gap;
	}
	if(Lines.m_Prediction >= 0)
	{
		FormatMillis(Lines.m_Prediction, aTime, sizeof(aTime));
		str_format(aBuf, sizeof(aBuf), DtLocalize("Prediction: %s"), aTime);
		const bool Slower = Preview ? Lines.m_Unlikely : m_HudUnlikely;
		DrawCentered(Y, LineFontSize, aBuf, Slower ? Red : ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f));
		Y += LineFontSize + Gap;
	}
	if(Lines.m_Ideal >= 0)
	{
		FormatMillis(Lines.m_Ideal, aTime, sizeof(aTime));
		str_format(aBuf, sizeof(aBuf), DtLocalize("Ideal: %s"), aTime);
		DrawCentered(Y, LineFontSize, aBuf, ColorRGBA(0.75f, 0.85f, 1.0f, 1.0f));
		Y += LineFontSize + Gap;
	}
	if(Lines.m_ShowRefDiff)
	{
		char aDiff[32];
		FormatDiff(Lines.m_RefDiff, aDiff, sizeof(aDiff));
		str_format(aBuf, sizeof(aBuf), DtLocalize("%s vs %s"), aDiff, Lines.m_RefName.c_str());
		DrawCentered(Y, LineFontSize, aBuf, Lines.m_RefDiff <= 0 ? Green : Red);
		Y += LineFontSize + Gap;
	}
	if(Lines.m_Unlikely)
		DrawCentered(Y, LineFontSize, WarningMessage(), ColorRGBA(1.0f, 0.35f, 0.35f, 1.0f));
	TextRender()->TextColor(TextRender()->DefaultTextColor());
}

// --- segment demos ---

bool CSplits::SegmentDemosActive() const
{
	return g_Config.m_DtSplits && g_Config.m_DtSplitsSegmentDemos && (g_Config.m_DtSplitsSegmentDemosAll || m_Data.m_SegmentDemos);
}

void CSplits::SetSegmentDemosOnMap(bool On)
{
	if(!HasMap() || m_Data.m_SegmentDemos == On)
		return;
	m_Data.m_SegmentDemos = On;
	Save();
	Client()->DemoRecorder_UpdateReplayRecorder(); // starts (or stops) the recorder the demos are cut from
}

int CSplits::DuoOf(int ClientId) const
{
	if(ClientId < 0 || ClientId >= MAX_CLIENTS)
		return -1;
	const int Team = GameClient()->m_Teams.Team(ClientId);
	if(Team <= TEAM_FLOCK || Team >= TEAM_SUPER)
	{
		// no team (duo maps are often played like that): whoever you have been hooking with
		return GameClient()->m_Focus.RecentPartner(CFocus::PARTNER_SECONDS * Client()->GameTickSpeed());
	}
	// the dummy first, then anyone else in the team
	for(int Local : GameClient()->m_aLocalIds)
		if(Local >= 0 && Local != ClientId && GameClient()->m_Teams.Team(Local) == Team && GameClient()->m_Snap.m_apPlayerInfos[Local])
			return Local;
	for(int i = 0; i < MAX_CLIENTS; i++)
		if(i != ClientId && GameClient()->m_Snap.m_apPlayerInfos[i] && GameClient()->m_Teams.Team(i) == Team)
			return i;
	return -1;
}

void CSplits::QueueClip(int From, int To, int Millis, double StartTick, double EndTick)
{
	// a faster try of the same segment in the same run replaces the slower one
	for(auto It = m_vPendingClips.begin(); It != m_vPendingClips.end(); ++It)
	{
		if(It->m_From == From && It->m_To == To)
		{
			if(It->m_Millis <= Millis)
				return;
			m_vPendingClips.erase(It);
			break;
		}
	}
	SPendingClip Clip;
	Clip.m_From = From;
	Clip.m_To = To;
	Clip.m_Millis = Millis;
	Clip.m_StartTick = StartTick;
	Clip.m_EndTick = EndTick;
	Clip.m_MainId = m_TrackedClientId;
	Clip.m_DuoId = DuoOf(m_TrackedClientId);
	if(Clip.m_MainId >= 0)
		Clip.m_MainName = GameClient()->m_aClients[Clip.m_MainId].m_aName;
	if(Clip.m_DuoId >= 0)
		Clip.m_DuoName = GameClient()->m_aClients[Clip.m_DuoId].m_aName;
	Clip.m_Practice = IsPractice();
	Clip.m_Timestamp = time_timestamp();
	m_vPendingClips.push_back(Clip);
}

void CSplits::FlushClips()
{
	m_FlushClipsAtTick = -1;
	if(m_vPendingClips.empty())
		return;
	std::vector<SPendingClip> vClips;
	vClips.swap(m_vPendingClips);
	if(!HasMap() || Client()->State() != IClient::STATE_ONLINE)
		return;

	CreateDemoFolders();
	Storage()->CreateFolder(m_aSegmentFolder, IStorage::TYPE_SAVE);

	const int Margin = Client()->GameTickSpeed(); // 1 s before and after the segment
	const int Now = Client()->GameTick(g_Config.m_ClDummy);
	char aMap[128];
	str_copy(aMap, GameClient()->Map()->BaseName());
	str_sanitize_filename(aMap);

	std::vector<IClient::SReplaySlice> vSlices;
	std::vector<std::string> vDemos;
	for(const SPendingClip &Clip : vClips)
	{
		IClient::SReplaySlice Slice;
		Slice.m_StartTick = (int)std::floor(Clip.m_StartTick) - Margin;
		Slice.m_EndTick = std::min((int)std::ceil(Clip.m_EndTick) + Margin, Now);
		char aTime[32];
		FormatMillis(Clip.m_Millis, aTime, sizeof(aTime));
		for(char *p = aTime; *p; p++)
			if(*p == ':')
				*p = '-';
		char aTimestamp[20];
		str_timestamp(aTimestamp, sizeof(aTimestamp));
		str_format(Slice.m_aDst, sizeof(Slice.m_aDst), "%s/%s_%s-%s_%s_%s.demo", m_aSegmentFolder, aMap, PointToString(Clip.m_From).c_str(), PointToString(Clip.m_To).c_str(), aTime, aTimestamp);
		vSlices.push_back(Slice);
		vDemos.emplace_back(Slice.m_aDst);
	}
	std::vector<std::shared_ptr<IJob>> vJobs = Client()->DemoRecorder_SaveReplaySlices(vSlices);
	if(vJobs.size() != vClips.size())
	{
		log_warn("splits", "segment demos: the replay recorder isn't recording, %d segment demos lost", (int)vClips.size());
		return;
	}
	for(size_t i = 0; i < vClips.size(); i++)
		m_vClipJobs.push_back({vJobs[i], vClips[i], vDemos[i]});
}

void CSplits::UpdateClipJobs()
{
	for(auto It = m_vClipJobs.begin(); It != m_vClipJobs.end();)
	{
		if(It->m_pJob->State() != IJob::STATE_DONE && It->m_pJob->State() != IJob::STATE_ABORTED)
		{
			++It;
			continue;
		}
		const SClipJob Job = *It;
		It = m_vClipJobs.erase(It);
		if(Job.m_pJob->State() != IJob::STATE_DONE || !Storage()->FileExists(Job.m_Demo.c_str(), IStorage::TYPE_SAVE))
		{
			log_warn("splits", "segment demo '%s' could not be saved", Job.m_Demo.c_str());
			continue;
		}
		// the info next to the demo: what it holds and which tees, for building the ideal run
		std::string Info = Job.m_Demo.substr(0, Job.m_Demo.size() - 5) + ".txt";
		IOHANDLE File = Storage()->OpenFile(Info.c_str(), IOFLAG_WRITE, IStorage::TYPE_SAVE);
		if(File)
		{
			const SPendingClip &C = Job.m_Clip;
			char aBuf[512];
			str_format(aBuf, sizeof(aBuf), "ddnetty clip v1\nfrom %s\nto %s\nmillis %d\nstart %.3f\nend %.3f\nmain %d %s\nduo %d %s\npractice %d\ntime %lld\n",
				PointToString(C.m_From).c_str(), PointToString(C.m_To).c_str(), C.m_Millis, C.m_StartTick, C.m_EndTick,
				C.m_MainId, C.m_MainName.c_str(), C.m_DuoId, C.m_DuoName.c_str(), C.m_Practice ? 1 : 0, (long long)C.m_Timestamp);
			io_write(File, aBuf, str_length(aBuf));
			io_close(File);
		}
		log_info("splits", "saved segment demo '%s'", Job.m_Demo.c_str());
		PruneClips(Job.m_Clip.m_From, Job.m_Clip.m_To);
		if(m_vClipJobs.empty())
			RequestIdealDemo();
	}
	UpdateIdealDemo();
}

void CSplits::PruneClips(int From, int To)
{
	// keep the fastest dt_splits_segment_demos_keep demos of the segment, move the rest to old/
	struct SFound
	{
		std::string m_Base; // without extension
		int m_Millis;
	};
	struct SCtx
	{
		CSplits *m_pThis;
		std::string m_Prefix;
		std::vector<SFound> m_vFound;
	} Ctx;
	Ctx.m_pThis = this;
	char aMap[128];
	str_copy(aMap, GameClient()->Map()->BaseName());
	str_sanitize_filename(aMap);
	Ctx.m_Prefix = std::string(aMap) + "_" + PointToString(From) + "-" + PointToString(To) + "_";
	Storage()->ListDirectory(IStorage::TYPE_SAVE, m_aSegmentFolder, [](const char *pName, int IsDir, int DirType, void *pUser) {
		SCtx *pCtx = static_cast<SCtx *>(pUser);
		if(IsDir || !str_startswith(pName, pCtx->m_Prefix.c_str()) || !str_endswith(pName, ".txt"))
			return 0;
		char aPath[IO_MAX_PATH_LENGTH];
		str_format(aPath, sizeof(aPath), "%s/%s", pCtx->m_pThis->m_aSegmentFolder, pName);
		CLineReader Reader;
		if(!Reader.OpenFile(pCtx->m_pThis->Storage()->OpenFile(aPath, IOFLAG_READ, IStorage::TYPE_SAVE)))
			return 0;
		int Millis = -1;
		while(const char *pLine = Reader.Get())
			if(str_startswith(pLine, "millis "))
				Millis = str_toint(pLine + 7);
		if(Millis >= 0)
			pCtx->m_vFound.push_back({std::string(aPath, str_length(aPath) - 4), Millis});
		return 0; }, &Ctx);
	std::sort(Ctx.m_vFound.begin(), Ctx.m_vFound.end(), [](const SFound &A, const SFound &B) { return A.m_Millis < B.m_Millis; });
	const size_t Keep = (size_t)std::max(1, g_Config.m_DtSplitsSegmentDemosKeep);
	if(Ctx.m_vFound.size() <= Keep)
		return;
	char aOld[IO_MAX_PATH_LENGTH];
	str_format(aOld, sizeof(aOld), "%s/old", m_aSegmentFolder);
	Storage()->CreateFolder(aOld, IStorage::TYPE_SAVE);
	for(size_t i = Keep; i < Ctx.m_vFound.size(); i++)
	{
		const std::string &Base = Ctx.m_vFound[i].m_Base;
		const char *pFile = Base.c_str() + str_length(m_aSegmentFolder) + 1;
		for(const char *pExt : {".demo", ".txt"})
		{
			char aSrc[IO_MAX_PATH_LENGTH], aDst[IO_MAX_PATH_LENGTH];
			str_format(aSrc, sizeof(aSrc), "%s%s", Base.c_str(), pExt);
			str_format(aDst, sizeof(aDst), "%s/%s%s", aOld, pFile, pExt);
			Storage()->RenameFile(aSrc, aDst, IStorage::TYPE_SAVE);
		}
	}
}

// --- the ideal run demo ---

void CSplits::ConBuildIdeal(IConsole::IResult *pResult, void *pUserData)
{
	static_cast<CSplits *>(pUserData)->RequestIdealDemo(true);
}

void CSplits::RequestIdealDemo(bool Manual)
{
	if(!HasMap() || (!Manual && !g_Config.m_DtSplitsIdealDemo))
		return;
	m_IdealManual = m_IdealManual || Manual;
	if(m_pIdealJob)
	{
		m_IdealAgain = true;
		return;
	}
	StartIdealDemo();
}

void CSplits::StartIdealDemo()
{
	const bool Manual = m_IdealManual;
	m_IdealManual = false;
	const SAnalysis &Current = Analysis();
	if(!Current.m_RouteComplete || Current.m_vRoute.size() < 2)
	{
		str_copy(m_aIdealStatus, DtLocalize("Ideal run demo: needs one complete run first"));
		return;
	}

	// what every segment demo of this map holds
	struct SInfo
	{
		std::string m_Demo;
		int m_From = POINT_NONE, m_To = POINT_NONE, m_Millis = -1, m_MainId = -1, m_DuoId = -1;
		double m_Start = 0.0, m_End = 0.0;
	};
	struct SCtx
	{
		CSplits *m_pThis;
		std::vector<SInfo> m_vInfos;
	} Ctx{this, {}};
	Storage()->ListDirectory(IStorage::TYPE_SAVE, m_aSegmentFolder, [](const char *pName, int IsDir, int DirType, void *pUser) {
		SCtx *pCtx = static_cast<SCtx *>(pUser);
		if(IsDir || !str_endswith(pName, ".txt"))
			return 0;
		char aPath[IO_MAX_PATH_LENGTH];
		str_format(aPath, sizeof(aPath), "%s/%s", pCtx->m_pThis->m_aSegmentFolder, pName);
		CLineReader Reader;
		if(!Reader.OpenFile(pCtx->m_pThis->Storage()->OpenFile(aPath, IOFLAG_READ, IStorage::TYPE_SAVE)))
			return 0;
		SInfo Info;
		Info.m_Demo = std::string(aPath, str_length(aPath) - 4) + ".demo";
		while(const char *pLine = Reader.Get())
		{
			if(str_startswith(pLine, "from "))
				Info.m_From = PointFromString(pLine + 5);
			else if(str_startswith(pLine, "to "))
				Info.m_To = PointFromString(pLine + 3);
			else if(str_startswith(pLine, "millis "))
				Info.m_Millis = str_toint(pLine + 7);
			else if(str_startswith(pLine, "start "))
				Info.m_Start = std::strtod(pLine + 6, nullptr);
			else if(str_startswith(pLine, "end "))
				Info.m_End = std::strtod(pLine + 4, nullptr);
			else if(str_startswith(pLine, "main "))
				Info.m_MainId = str_toint(pLine + 5);
			else if(str_startswith(pLine, "duo "))
				Info.m_DuoId = str_toint(pLine + 4);
		}
		if(Info.m_Millis >= 0 && Info.m_From != POINT_NONE && Info.m_To != POINT_NONE && Info.m_MainId >= 0 && pCtx->m_pThis->Storage()->FileExists(Info.m_Demo.c_str(), IStorage::TYPE_SAVE))
			pCtx->m_vInfos.push_back(Info);
		return 0; }, &Ctx);

	// the fastest demo of every segment of the route
	std::vector<CIdealDemoJob::SClip> vClips;
	int Missing = 0;
	for(size_t i = 0; i + 1 < Current.m_vRoute.size(); i++)
	{
		const SInfo *pBest = nullptr;
		for(const SInfo &Info : Ctx.m_vInfos)
			if(Info.m_From == Current.m_vRoute[i] && Info.m_To == Current.m_vRoute[i + 1] && (!pBest || Info.m_Millis < pBest->m_Millis))
				pBest = &Info;
		if(!pBest)
		{
			Missing++;
			continue;
		}
		CIdealDemoJob::SClip Clip;
		Clip.m_Demo = pBest->m_Demo;
		Clip.m_Millis = pBest->m_Millis;
		Clip.m_StartTick = pBest->m_Start;
		Clip.m_EndTick = pBest->m_End;
		Clip.m_MainId = pBest->m_MainId;
		Clip.m_DuoId = pBest->m_DuoId;
		vClips.push_back(Clip);
	}
	if(Missing > 0)
	{
		str_format(m_aIdealStatus, sizeof(m_aIdealStatus), DtLocalize("Ideal run demo: %d of %d segments still without a demo"), Missing, (int)Current.m_vRoute.size() - 1);
		if(Manual)
		{
			m_Notice = m_aIdealStatus;
			m_NoticeUntil = time_get() + 3 * time_freq();
		}
		return;
	}
	// a team mate in some segments but not in others: leave it out rather than show it half the time
	const bool AllDuo = std::all_of(vClips.begin(), vClips.end(), [](const CIdealDemoJob::SClip &C) { return C.m_DuoId >= 0; });
	if(!AllDuo)
		for(CIdealDemoJob::SClip &Clip : vClips)
			Clip.m_DuoId = -1;

	int Total = 0;
	for(const CIdealDemoJob::SClip &Clip : vClips)
		Total += Clip.m_Millis;
	char aMap[128];
	str_copy(aMap, GameClient()->Map()->BaseName());
	str_sanitize_filename(aMap);
	char aTime[32];
	FormatMillis(Total, aTime, sizeof(aTime));
	for(char *p = aTime; *p; p++)
		if(*p == ':')
			*p = '-';
	CreateDemoFolders();
	char aDst[IO_MAX_PATH_LENGTH];
	str_format(aDst, sizeof(aDst), "demos/ddnetty/ideal/%s_ideal_%s.demo", aMap, aTime);

	CIdealDemoJob::SOptions Options;
	Options.m_BlendTicks = g_Config.m_DtSplitsIdealDemoBlend;
	Options.m_KeepShots = g_Config.m_DtSplitsIdealDemoShots != 0;
	m_pIdealJob = std::make_shared<CIdealDemoJob>(Client(), vClips, aDst, Options);
	Engine()->AddJob(m_pIdealJob);
	str_copy(m_aIdealStatus, DtLocalize("Ideal run demo: building..."));
}

void CSplits::SaveIdealGhost()
{
	// the ghost recorder is shared with your own ghost: wait while it records a run
	IGhostRecorder *pRecorder = GameClient()->m_Ghost.GhostRecorder();
	if(!m_pGhostPending || !HasMap() || pRecorder->IsRecording())
		return;
	std::shared_ptr<CIdealDemoJob> pJob = m_pGhostPending;
	m_pGhostPending = nullptr;

	const char *pMap = GameClient()->Map()->BaseName();
	char aSha256[SHA256_MAXSTRSIZE];
	sha256_str(GameClient()->Map()->Sha256(), aSha256, sizeof(aSha256));
	const char *pOwner = "ideal run";

	// the newest ideal ghost replaces the older ones of the map
	const char *pGhostDir = GameClient()->m_Ghost.GetGhostDir();
	struct SCtx
	{
		CSplits *m_pThis;
		std::string m_Prefix;
		const char *m_pDir;
	} Ctx{this, std::string(pMap) + "_ideal_run_", pGhostDir};
	Storage()->ListDirectory(IStorage::TYPE_SAVE, pGhostDir, [](const char *pName, int IsDir, int DirType, void *pUser) {
		SCtx *pCtx = static_cast<SCtx *>(pUser);
		if(!IsDir && str_startswith(pName, pCtx->m_Prefix.c_str()) && str_endswith(pName, ".gho"))
		{
			char aPath[IO_MAX_PATH_LENGTH];
			str_format(aPath, sizeof(aPath), "%s/%s", pCtx->m_pDir, pName);
			pCtx->m_pThis->Storage()->RemoveFile(aPath, IStorage::TYPE_SAVE);
		}
		return 0; }, &Ctx);

	char aTimestamp[32];
	str_timestamp_format(aTimestamp, sizeof(aTimestamp), TimestampFormat::NOSPACE);
	const int Time = pJob->TotalMillis();
	char aPath[IO_MAX_PATH_LENGTH];
	str_format(aPath, sizeof(aPath), "%s/%s_ideal_run_%d.%03d_%s_%s.gho", pGhostDir, pMap, Time / 1000, Time % 1000, aTimestamp, aSha256);
	Storage()->CreateFolder(pGhostDir, IStorage::TYPE_SAVE);
	if(pRecorder->Start(aPath, pMap, GameClient()->Map()->Sha256(), pOwner) != 0)
	{
		log_warn("splits", "could not write the ideal run ghost '%s'", aPath);
		return;
	}
	const int StartTick = pJob->GhostStartTick();
	pRecorder->WriteData(GHOSTDATA_TYPE_START_TICK, &StartTick, sizeof(int));
	CGhostSkin Skin;
	StrToInts(Skin.m_aSkin, std::size(Skin.m_aSkin), "default");
	Skin.m_UseCustomColor = 0;
	Skin.m_ColorBody = 0;
	Skin.m_ColorFeet = 0;
	if(pJob->GhostSkin().size() == 9)
	{
		mem_copy(Skin.m_aSkin, pJob->GhostSkin().data(), sizeof(Skin.m_aSkin));
		Skin.m_UseCustomColor = pJob->GhostSkin()[6];
		Skin.m_ColorBody = pJob->GhostSkin()[7];
		Skin.m_ColorFeet = pJob->GhostSkin()[8];
	}
	pRecorder->WriteData(GHOSTDATA_TYPE_SKIN, &Skin, sizeof(CGhostSkin));
	for(const CIdealDemoJob::SGhostTick &Tick : pJob->Ghost())
	{
		CGhostCharacter Char;
		Char.m_X = Tick.m_X;
		Char.m_Y = Tick.m_Y;
		Char.m_VelX = Tick.m_VelX;
		Char.m_VelY = Tick.m_VelY;
		Char.m_Angle = Tick.m_Angle;
		Char.m_Direction = Tick.m_Direction;
		Char.m_Weapon = Tick.m_Weapon;
		Char.m_HookState = Tick.m_HookState;
		Char.m_HookX = Tick.m_HookX;
		Char.m_HookY = Tick.m_HookY;
		Char.m_AttackTick = Tick.m_AttackTick;
		Char.m_Tick = Tick.m_Tick;
		pRecorder->WriteData(GHOSTDATA_TYPE_CHARACTER, &Char, sizeof(CGhostCharacter));
	}
	pRecorder->Stop((int)pJob->Ghost().size(), Time);
	log_info("splits", "ideal run ghost saved to '%s'", aPath);
}

void CSplits::UpdateIdealDemo()
{
	if(m_pGhostPending)
		SaveIdealGhost();
	if(!m_pIdealJob || (m_pIdealJob->State() != IJob::STATE_DONE && m_pIdealJob->State() != IJob::STATE_ABORTED))
		return;
	std::shared_ptr<CIdealDemoJob> pJob = m_pIdealJob;
	m_pIdealJob = nullptr;
	char aTime[32];
	FormatMillis(pJob->TotalMillis(), aTime, sizeof(aTime));
	if(pJob->State() == IJob::STATE_DONE && pJob->Success())
	{
		// keep only the newest ideal demo of the map, older ones go to old/
		char aMap[128];
		str_copy(aMap, GameClient()->Map()->BaseName());
		str_sanitize_filename(aMap);
		struct SCtx
		{
			CSplits *m_pThis;
			std::string m_Prefix, m_Keep;
		} Ctx{this, std::string(aMap) + "_ideal_", pJob->Destination().substr(str_length("demos/ddnetty/ideal/"))};
		Storage()->ListDirectory(IStorage::TYPE_SAVE, "demos/ddnetty/ideal", [](const char *pName, int IsDir, int DirType, void *pUser) {
			SCtx *pCtx = static_cast<SCtx *>(pUser);
			if(IsDir || !str_startswith(pName, pCtx->m_Prefix.c_str()) || !str_endswith(pName, ".demo") || pCtx->m_Keep == pName)
				return 0;
			pCtx->m_pThis->Storage()->CreateFolder("demos/ddnetty/ideal/old", IStorage::TYPE_SAVE);
			char aSrc[IO_MAX_PATH_LENGTH], aDst[IO_MAX_PATH_LENGTH];
			str_format(aSrc, sizeof(aSrc), "demos/ddnetty/ideal/%s", pName);
			str_format(aDst, sizeof(aDst), "demos/ddnetty/ideal/old/%s", pName);
			pCtx->m_pThis->Storage()->RenameFile(aSrc, aDst, IStorage::TYPE_SAVE);
			return 0; }, &Ctx);
		str_format(m_aIdealStatus, sizeof(m_aIdealStatus), DtLocalize("Ideal run demo: %s saved"), aTime);
		log_info("splits", "ideal run demo saved to '%s'", pJob->Destination().c_str());
		m_Notice = m_aIdealStatus;
		m_NoticeUntil = time_get() + 3 * time_freq();
		if(g_Config.m_DtSplitsIdealGhost && !pJob->Ghost().empty())
		{
			m_pGhostPending = pJob;
			SaveIdealGhost();
		}
	}
	else
	{
		str_format(m_aIdealStatus, sizeof(m_aIdealStatus), DtLocalize("Ideal run demo failed: %s"), pJob->Error().c_str());
		log_warn("splits", "%s", m_aIdealStatus);
	}
	if(m_IdealAgain)
	{
		m_IdealAgain = false;
		StartIdealDemo();
	}
}
