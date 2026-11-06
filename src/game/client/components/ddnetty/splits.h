// ddnetty: split times per map, using the map's time checkpoints and the
// player's own checkpoints (gates), with an edit mode to place the gates.
#ifndef GAME_CLIENT_COMPONENTS_DDNETTY_SPLITS_H
#define GAME_CLIENT_COMPONENTS_DDNETTY_SPLITS_H

#include <base/types.h>
#include <base/vmath.h>

#include <engine/console.h>

#include <game/client/component.h>
#include <game/client/lineinput.h>
#include <game/client/ui_rect.h>
#include <game/ddnetty_splits.h>

#include <memory>
#include <string>
#include <vector>

class IJob;
class CIdealDemoJob;

class CSplits : public CComponent
{
public:
	int Sizeof() const override { return sizeof(*this); }
	void OnInit() override;
	void OnConsoleInit() override;
	void OnMapLoad() override;
	void OnReset() override;
	void OnNewSnapshot() override;
	void OnMessage(int MsgType, void *pRawMsg) override;
	void OnRender() override;
	bool OnInput(const IInput::CEvent &Event) override;
	bool OnCursorMove(float x, float y, IInput::ECursorType CursorType) override;

	// used by the DDNetty menu page
	bool HasMap() const { return m_aPath[0] != '\0'; }
	DdnettySplits::SOptions Options() const;
	const DdnettySplits::SAnalysis &Analysis();
	const DdnettySplits::CSplitsData &Data() const { return m_Data; }
	// top players of the map (ddnetty/splits/refs/<map>.txt)
	const DdnettySplits::CSplitsRefs &Refs() const { return m_Refs; }
	const DdnettySplits::SRefRun *RefRun() const; // the one chosen with dt_splits_ref_player
	void LoadRefs();
	void CreateDemoFolders(); // demos/ddnetty/segments and demos/ddnetty/ideal
	const char *RefsPath() const { return m_aRefsPath; }
	void DeleteRuns(const std::vector<int> &vRunIds);
	void DeleteSegment(int From, int To);
	void DeleteGate(int GateId);
	void DeleteAllGates();
	void RenameGate(int GateId, const char *pName);
	void SetCustomTarget(int Millis); // -1 clears it
	int CustomTarget() const { return m_Data.m_TargetMillis; }
	void SetEditMode(bool EditMode);
	bool EditMode() const { return m_EditMode; }
	std::string PointName(int Point) const;
	int RecordMillis() const; // map record from the server, -1 if unknown
	int PlayerBestMillis() const; // personal best from the server, -1 if unknown
	int TargetMillis() const; // the time the prediction is compared with, -1 if unknown
	const char *WarningMessage() const;

	// segment demos: a demo of every new best segment, cut out of the replay recorder
	bool SegmentDemosOnMap() const { return m_Data.m_SegmentDemos; }
	void SetSegmentDemosOnMap(bool On);
	// focused runs: data counts from a moment on, and the average only from runs close to your best
	int64_t Since() const { return m_Data.m_Since; }
	void SetSince(int64_t Timestamp); // 0 shows everything again
	int FocusMarginOnMap() const { return m_Data.m_FocusMarginMillis; } // -1 = the default
	void SetFocusMarginOnMap(int Millis); // -1 goes back to the default
	int FocusMargin() const; // the margin in use on this map
	bool SegmentDemosActive() const;
	bool WantsReplayRecorder() const { return HasMap() && SegmentDemosActive(); }
	int PendingSegmentDemos() const { return (int)(m_vPendingClips.size() + m_vClipJobs.size()); }
	// the ideal run demo: your best segments stitched together
	void RequestIdealDemo(bool Manual = false);
	bool IdealDemoBuilding() const { return m_pIdealJob != nullptr; }
	const char *IdealDemoStatus() const { return m_aIdealStatus; }
	const char *SegmentFolder() const { return m_aSegmentFolder; }

	// used by other components
	bool HidesServerTimeCpDiff() const;
	void ApplyEditCamera(vec2 &Center) const;
	CUIRect GetHudEditorRect() const;
	void RenderHudPreview();

	static void FormatMillis(int Millis, char *pBuf, int BufSize); // 1:02.34 / 12.34
	static void FormatDiff(int Millis, char *pBuf, int BufSize); // +0.31 / -1.20
	static int ParseTime(const char *pStr); // "9:41.02" / "41.5" -> millis, -1 if invalid

private:
	DdnettySplits::CSplitsData m_Data;
	char m_aPath[IO_MAX_PATH_LENGTH] = "";
	DdnettySplits::CSplitsRefs m_Refs;
	char m_aRefsPath[IO_MAX_PATH_LENGTH] = "";

	DdnettySplits::SAnalysis m_Analysis;
	DdnettySplits::SOptions m_AnalysisOptions;
	bool m_AnalysisDirty = true;

	// the run being tracked right now
	int m_TrackedClientId = -1;
	int m_LastProcessedTick = -1;
	int m_RunId = 0;
	int m_LastPoint = DdnettySplits::POINT_NONE;
	double m_LastPointTick = 0.0;
	int m_LastUsedPoint = DdnettySplits::POINT_NONE; // last point used by the current options (for the HUD)
	double m_LastUsedPointTick = 0.0;
	int m_RouteIndex = -1; // where on the route the run is, -1 if unknown
	double m_StartTick = -1.0; // when the start line was left, -1 if unknown
	// switching between the main tee and the dummy keeps the run going
	int m_StartClientId = -1; // the tee that left the start line
	bool m_SwitchedTee = false; // the run has been carried over to the other tee at least once
	bool m_PrimeFromPrev = false; // right after a switch: the step before it still counts
	int m_InTimecp = -1;
	bool m_InStart = false;
	bool m_InFinish = false;
	// how the segment being timed was entered (ATTFLAG_*), for the raw and for the used points
	int m_EntryFlags = 0;
	int m_UsedEntryFlags = 0;
	std::vector<int> m_vRecentPoints; // the last points of this run, to notice going back
	int RawBest(int From, int To) const;

	// HUD
	int64_t m_HudDiffUntil = 0;
	int m_HudDiff = 0;
	bool m_HudHasDiff = false;
	int m_HudPrediction = -1;
	bool m_HudUnlikely = false;
	bool m_HudHasRefDiff = false; // compared with a top player at this point
	int m_HudRefDiff = 0;

	// edit mode
	enum
	{
		DRAG_NONE = 0,
		DRAG_NEW,
		DRAG_END_A,
		DRAG_END_B,
	};
	bool m_EditMode = false;
	int m_DragMode = DRAG_NONE;
	int m_DragGate = -1;
	vec2 m_DragStart = vec2(0.0f, 0.0f);
	bool m_Panning = false;
	// double click renames a checkpoint right on the map
	int64_t m_LastClickTime = 0;
	int m_LastClickGate = -1;
	int m_RenamingGate = -1;
	CLineInputBuffered<33> m_RenameInput;
	// deleting the last checkpoint removes its times, so it needs a second right click
	int m_PendingDeleteGate = -1;
	int64_t m_PendingDeleteUntil = 0;
	// short message in the middle of the screen (e.g. after cycling the split points)
	std::string m_Notice;
	int64_t m_NoticeUntil = 0;
	// new best segment message, two lines under the HUD
	std::string m_aPbNotice[2];
	int64_t m_PbNoticeUntil = 0;
	void ShowPbNotice(int From, int To, int Millis, int OldBest, int RouteSegment, bool LooseEntry);
	bool m_PbNoticeGold = false; // better than everyone in the reference file
	vec2 m_EditCameraOffset = vec2(0.0f, 0.0f); // added to the normal camera center
	vec2 m_EditCursor = vec2(0.0f, 0.0f); // relative to the camera center, in world units

	// segment demos
	struct SPendingClip
	{
		int m_From = DdnettySplits::POINT_NONE;
		int m_To = DdnettySplits::POINT_NONE;
		int m_Millis = 0;
		double m_StartTick = 0.0; // when the segment started (fractional game tick)
		double m_EndTick = 0.0;
		int m_MainId = -1; // the tee whose time it is
		int m_DuoId = -1; // its team mate, if any
		std::string m_MainName;
		std::string m_DuoName;
		bool m_Practice = false;
		int64_t m_Timestamp = 0;
	};
	struct SClipJob
	{
		std::shared_ptr<IJob> m_pJob;
		SPendingClip m_Clip;
		std::string m_Demo; // path of the demo being written
	};
	char m_aSegmentFolder[IO_MAX_PATH_LENGTH] = "";
	std::vector<SPendingClip> m_vPendingClips;
	std::vector<SClipJob> m_vClipJobs;
	int m_FlushClipsAtTick = -1;
	void QueueClip(int From, int To, int Millis, double StartTick, double EndTick);
	void FlushClips();
	void UpdateClipJobs();
	void PruneClips(int From, int To);
	int DuoOf(int ClientId) const;
	std::shared_ptr<CIdealDemoJob> m_pIdealJob;
	bool m_IdealAgain = false; // a new segment demo came in while building
	bool m_IdealManual = false;
	char m_aIdealStatus[256] = "";
	void StartIdealDemo();
	std::shared_ptr<CIdealDemoJob> m_pGhostPending; // ideal run waiting for the ghost recorder to be free
	void SaveIdealGhost();
	void UpdateIdealDemo();
	static void ConBuildIdeal(IConsole::IResult *pResult, void *pUserData);

	void Load();
	void Save();
	void Append(const DdnettySplits::SAttempt &Attempt);
	void ResetRun();
	void ProcessStep(vec2 PrevPos, vec2 Pos, int PrevTick, int CurTick);
	void OnPoint(int Point, double Tick);
	bool IsPractice() const;

	vec2 VisibleHalfSize() const;
	vec2 CursorPos() const;
	vec2 SnapPos(vec2 Pos) const;
	int GateAt(vec2 Pos, float MaxDistance) const;
	void BeginDrag();
	void EndDrag();
	void GatesChanged();

	void RenderGates();
	void RenderEditHelp();
	void RenderRenameBox();
	void StopRenaming(bool Save);
	vec2 WorldToHud(vec2 Pos, float HudWidth, float HudHeight) const;
	struct SHudLines
	{
		bool m_ShowDiff = false;
		int m_Diff = 0;
		int m_Prediction = -1;
		bool m_Unlikely = false;
		int m_Ideal = -1;
		bool m_ShowRefDiff = false;
		int m_RefDiff = 0;
		std::string m_RefName;
	};
	bool HudLines(SHudLines &Lines, bool Preview) const;
	CUIRect HudRect(const SHudLines &Lines, float HudWidth, float HudHeight) const;
	void RenderHudBox(bool Preview);

	static void ConEdit(IConsole::IResult *pResult, void *pUserData);
	static void ConSourceCycle(IConsole::IResult *pResult, void *pUserData);
	static void ConDeleteAllGates(IConsole::IResult *pResult, void *pUserData);
};

#endif // GAME_CLIENT_COMPONENTS_DDNETTY_SPLITS_H
