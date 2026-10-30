// ddnetty: split times per map, see splits.h
#include "splits.h"

#include <base/io.h>
#include <base/log.h>
#include <base/math.h>
#include <base/str.h>
#include <base/time.h>

#include <engine/graphics.h>
#include <engine/keys.h>
#include <engine/map.h>
#include <engine/shared/config.h>
#include <engine/shared/linereader.h>
#include <engine/storage.h>
#include <engine/textrender.h>

#include <generated/protocol.h>

#include <game/client/gameclient.h>
#include <game/collision.h>
#include <game/localization.h>
#include <game/mapitems.h>

#include <algorithm>
#include <cmath>

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
		return A.m_UseTimecps == B.m_UseTimecps && A.m_UseGates == B.m_UseGates && A.m_CountPractice == B.m_CountPractice && A.m_UseAverage == B.m_UseAverage;
	}
} // namespace

void CSplits::OnConsoleInit()
{
	Console()->Register("dt_splits_edit", "", CFGFLAG_CLIENT, ConEdit, this, "Toggle the edit mode for your split checkpoints");
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
	m_EditMode = false;
	m_DragMode = DRAG_NONE;
	m_HudShowUntil = 0;
}

SOptions CSplits::Options() const
{
	SOptions Options;
	Options.m_UseTimecps = g_Config.m_DtSplitsSource != 2;
	Options.m_UseGates = g_Config.m_DtSplitsSource != 1;
	Options.m_CountPractice = g_Config.m_DtSplitsPractice != 0;
	Options.m_UseAverage = g_Config.m_DtSplitsAverage != 0;
	return Options;
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
	ResetRun();

	char aMap[128];
	str_copy(aMap, GameClient()->Map()->BaseName());
	str_sanitize_filename(aMap);
	char aSha256[SHA256_MAXSTRSIZE];
	sha256_str(GameClient()->Map()->Sha256(), aSha256, sizeof(aSha256));
	str_format(m_aPath, sizeof(m_aPath), "ddnetty/splits/%s_%.8s.txt", aMap, aSha256);

	Storage()->CreateFolder("ddnetty", IStorage::TYPE_SAVE);
	Storage()->CreateFolder("ddnetty/splits", IStorage::TYPE_SAVE);

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
	m_TrackedClientId = -1;
	m_LastProcessedTick = -1;
	m_LastPoint = POINT_NONE;
	m_LastUsedPoint = POINT_NONE;
	m_StartTick = -1.0;
	m_InTimecp = -1;
	m_InStart = false;
	m_InFinish = false;
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
		// switched to the dummy (or back): start over
		ResetRun();
		m_TrackedClientId = LocalId;
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
		// first snapshot after a reset: only learn where we are
		PrimeAt(Pos);
		m_LastProcessedTick = Cur.m_Tick;
		return;
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
		// (re)starting the race begins a new run
		m_RunId = m_Data.NextRunId();
		m_LastPoint = POINT_START;
		m_LastPointTick = Tick;
		m_LastUsedPoint = POINT_START;
		m_LastUsedPointTick = Tick;
		m_StartTick = Tick;
		return;
	}

	// HUD: compare with the stats from before this attempt
	if(CSplitsData::Includes(Point, Opts))
	{
		if(m_LastUsedPoint != POINT_NONE && m_LastUsedPoint != Point)
		{
			const int Millis = (int)std::lround((Tick - m_LastUsedPointTick) * 1000.0 / TickSpeed);
			const SAnalysis &Current = Analysis();
			const SSegmentStats *pStats = Current.Find(m_LastUsedPoint, Point);
			const int Reference = pStats ? CSplitsData::Reference(*pStats, Opts) : -1;

			m_HudSegment = PointName(m_LastUsedPoint) + " → " + PointName(Point);
			m_HudMillis = Millis;
			m_HudHasDiff = Reference >= 0;
			m_HudDiff = Reference >= 0 ? Millis - Reference : 0;
			m_HudPrediction = -1;
			m_HudUnlikely = false;

			double RaceStart = -1.0;
			if(GameClient()->LastRaceTick() >= 0)
				RaceStart = GameClient()->LastRaceTick();
			else if(m_StartTick >= 0.0)
				RaceStart = m_StartTick;
			const int Remaining = CSplitsData::Remaining(Current, Point, Opts);
			if(Point != POINT_FINISH && RaceStart >= 0.0 && Remaining >= 0)
			{
				m_HudPrediction = (int)std::lround((Tick - RaceStart) * 1000.0 / TickSpeed) + Remaining;
				const int Record = RecordMillis();
				m_HudUnlikely = Record > 0 && m_HudPrediction > Record;
			}
			m_HudShowUntil = time_get() + 3 * time_freq();
		}
		m_LastUsedPoint = Point;
		m_LastUsedPointTick = Tick;
	}

	if(m_LastPoint == POINT_NONE)
	{
		// first point after a reset (e.g. practicing a part of the map)
		m_RunId = m_Data.NextRunId();
		m_LastPoint = Point;
		m_LastPointTick = Tick;
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
	if(Attempt.m_Millis > 0)
	{
		m_Data.m_vAttempts.push_back(Attempt);
		Append(Attempt);
		m_AnalysisDirty = true;
	}

	m_LastPoint = Point;
	m_LastPointTick = Tick;
	if(Point == POINT_FINISH)
	{
		m_LastPoint = POINT_NONE;
		m_LastUsedPoint = POINT_NONE;
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
	m_Data.DeleteGate(GateId);
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
	m_EditMode = EditMode && HasMap();
	m_DragMode = DRAG_NONE;
	m_DragGate = -1;
}

vec2 CSplits::CursorPos() const
{
	return GameClient()->m_Controls.m_aTargetPos[g_Config.m_ClDummy];
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
		if(distance(Pos, vec2(Gate.m_Ax, Gate.m_Ay)) <= 12.0f)
		{
			m_DragMode = DRAG_END_A;
			m_DragGate = Gate.m_Id;
			return;
		}
		if(distance(Pos, vec2(Gate.m_Bx, Gate.m_By)) <= 12.0f)
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

	if(Event.m_Key == KEY_ESCAPE && (Event.m_Flags & IInput::FLAG_PRESS))
	{
		SetEditMode(false);
		return true;
	}
	if(Event.m_Key == KEY_MOUSE_1)
	{
		if(Event.m_Flags & IInput::FLAG_PRESS)
			BeginDrag();
		if((Event.m_Flags & IInput::FLAG_RELEASE) && m_DragMode != DRAG_NONE)
			EndDrag();
		return true;
	}
	if(Event.m_Key == KEY_MOUSE_2)
	{
		if(Event.m_Flags & IInput::FLAG_PRESS)
		{
			const int Gate = GateAt(CursorPos(), 12.0f);
			if(Gate >= 0)
				DeleteGate(Gate);
		}
		return true;
	}
	return false;
}

// --- rendering ---

void CSplits::OnRender()
{
	if(Client()->State() != IClient::STATE_ONLINE || !HasMap())
		return;
	RenderGates();
	RenderHud();
}

void CSplits::RenderGates()
{
	if(!m_EditMode && (!g_Config.m_DtSplitsShowGates || m_Data.m_vGates.empty()))
		return;

	const vec2 Center = GameClient()->m_Camera.m_Center;
	Graphics()->MapScreenToInterface(Center.x, Center.y, GameClient()->m_Camera.m_Zoom);

	const vec2 Cursor = SnapPos(CursorPos());
	const int Hovered = m_EditMode && m_DragMode == DRAG_NONE ? GateAt(CursorPos(), 12.0f) : -1;

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

	Graphics()->TextureClear();
	Graphics()->QuadsBegin();
	for(const SGate &Gate : m_Data.m_vGates)
	{
		vec2 A(Gate.m_Ax, Gate.m_Ay);
		vec2 B(Gate.m_Bx, Gate.m_By);
		ColorRGBA Color = Gate.m_Id == Hovered ? HoverColor : GateColor;
		if(Gate.m_Id == m_DragGate)
		{
			Color = DragColor;
			if(m_DragMode == DRAG_END_A)
				A = Cursor;
			else if(m_DragMode == DRAG_END_B)
				B = Cursor;
		}
		DrawLine(A, B, 4.0f, Color);
		if(m_EditMode)
		{
			DrawDot(A, 8.0f, ColorRGBA(1.0f, 1.0f, 1.0f, 0.9f));
			DrawDot(B, 8.0f, ColorRGBA(1.0f, 1.0f, 1.0f, 0.9f));
		}
	}
	if(m_DragMode == DRAG_NEW)
		DrawLine(m_DragStart, Cursor, 4.0f, DragColor);
	if(m_EditMode)
		DrawDot(Cursor, 6.0f, DragColor);
	Graphics()->QuadsEnd();

	// names above the middle of each checkpoint
	TextRender()->TextColor(1.0f, 1.0f, 1.0f, 0.9f);
	for(const SGate &Gate : m_Data.m_vGates)
	{
		const std::string Name = PointName(GatePoint(Gate.m_Id));
		const float FontSize = 14.0f;
		const vec2 Middle((Gate.m_Ax + Gate.m_Bx) / 2.0f, (Gate.m_Ay + Gate.m_By) / 2.0f);
		const float Width = TextRender()->TextWidth(FontSize, Name.c_str());
		TextRender()->Text(Middle.x - Width / 2.0f, Middle.y - 28.0f, FontSize, Name.c_str());
	}
	TextRender()->TextColor(TextRender()->DefaultTextColor());
}

void CSplits::RenderHud()
{
	const float Height = 300.0f;
	const float Width = Height * Graphics()->ScreenAspect();
	Graphics()->MapScreen(CScreenRect(0.0f, 0.0f, Width, Height));

	auto &&DrawCentered = [&](float Y, float FontSize, const char *pText, ColorRGBA Color) {
		TextRender()->TextColor(Color);
		TextRender()->Text(Width / 2.0f - TextRender()->TextWidth(FontSize, pText) / 2.0f, Y, FontSize, pText);
	};

	float Y = 48.0f;
	if(g_Config.m_DtSplitsHud && g_Config.m_ClShowhud && time_get() < m_HudShowUntil && !m_HudSegment.empty())
	{
		const float FontSize = 8.0f;
		char aTime[32], aDiff[32], aLine[256];
		FormatMillis(m_HudMillis, aTime, sizeof(aTime));
		str_format(aLine, sizeof(aLine), "%s: %s", m_HudSegment.c_str(), aTime);
		if(m_HudHasDiff)
		{
			FormatDiff(m_HudDiff, aDiff, sizeof(aDiff));
			// time in white, difference in green/red right after it
			char aDiffPart[40];
			str_format(aDiffPart, sizeof(aDiffPart), "  %s", aDiff);
			const float LineWidth = TextRender()->TextWidth(FontSize, aLine);
			const float DiffWidth = TextRender()->TextWidth(FontSize, aDiffPart);
			const float X = Width / 2.0f - (LineWidth + DiffWidth) / 2.0f;
			TextRender()->TextColor(1.0f, 1.0f, 1.0f, 1.0f);
			TextRender()->Text(X, Y, FontSize, aLine);
			if(m_HudDiff <= 0)
				TextRender()->TextColor(0.5f, 1.0f, 0.5f, 1.0f);
			else
				TextRender()->TextColor(1.0f, 0.5f, 0.5f, 1.0f);
			TextRender()->Text(X + LineWidth, Y, FontSize, aDiffPart);
		}
		else
		{
			DrawCentered(Y, FontSize, aLine, ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f));
		}
		Y += 11.0f;

		if(m_HudPrediction >= 0)
		{
			FormatMillis(m_HudPrediction, aTime, sizeof(aTime));
			str_format(aLine, sizeof(aLine), DtLocalize("Prediction: %s"), aTime);
			DrawCentered(Y, FontSize, aLine, m_HudUnlikely ? ColorRGBA(1.0f, 0.5f, 0.5f, 1.0f) : ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f));
			Y += 11.0f;
		}
		if(m_HudUnlikely)
		{
			DrawCentered(Y, 9.0f, DtLocalize("Rank 1 unlikely."), ColorRGBA(1.0f, 0.35f, 0.35f, 1.0f));
			Y += 12.0f;
		}
	}

	if(m_EditMode)
	{
		Y = std::max(Y, 75.0f);
		DrawCentered(Y, 7.0f, DtLocalize("Checkpoint edit mode: drag to create · drag an end to move · right click deletes · Esc exits"), ColorRGBA(1.0f, 0.85f, 0.2f, 1.0f));
		DrawCentered(Y + 9.0f, 6.0f, DtLocalize("Hold Shift to place without snapping. Moving or deleting a checkpoint resets its times."), ColorRGBA(1.0f, 1.0f, 1.0f, 0.7f));
	}
	TextRender()->TextColor(TextRender()->DefaultTextColor());
}
