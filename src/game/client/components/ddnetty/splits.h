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

#include <string>
#include <vector>

class CSplits : public CComponent
{
public:
	int Sizeof() const override { return sizeof(*this); }
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
	int m_InTimecp = -1;
	bool m_InStart = false;
	bool m_InFinish = false;

	// HUD
	int64_t m_HudDiffUntil = 0;
	int m_HudDiff = 0;
	bool m_HudHasDiff = false;
	int m_HudPrediction = -1;
	bool m_HudUnlikely = false;

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
	vec2 m_EditCameraOffset = vec2(0.0f, 0.0f); // added to the normal camera center
	vec2 m_EditCursor = vec2(0.0f, 0.0f); // relative to the camera center, in world units

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
	};
	bool HudLines(SHudLines &Lines, bool Preview) const;
	CUIRect HudRect(const SHudLines &Lines, float HudWidth, float HudHeight) const;
	void RenderHudBox(bool Preview);

	static void ConEdit(IConsole::IResult *pResult, void *pUserData);
	static void ConSourceCycle(IConsole::IResult *pResult, void *pUserData);
	static void ConDeleteAllGates(IConsole::IResult *pResult, void *pUserData);
};

#endif // GAME_CLIENT_COMPONENTS_DDNETTY_SPLITS_H
