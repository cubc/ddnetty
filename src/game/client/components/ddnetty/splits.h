// ddnetty: split times per map, using the map's time checkpoints and the
// player's own checkpoints (gates), with an edit mode to place the gates.
#ifndef GAME_CLIENT_COMPONENTS_DDNETTY_SPLITS_H
#define GAME_CLIENT_COMPONENTS_DDNETTY_SPLITS_H

#include <base/types.h>
#include <base/vmath.h>

#include <engine/console.h>

#include <game/client/component.h>
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

	// used by the DDNetty menu page
	bool HasMap() const { return m_aPath[0] != '\0'; }
	DdnettySplits::SOptions Options() const;
	const DdnettySplits::SAnalysis &Analysis();
	const DdnettySplits::CSplitsData &Data() const { return m_Data; }
	void DeleteRuns(const std::vector<int> &vRunIds);
	void DeleteSegment(int From, int To);
	void DeleteGate(int GateId);
	void RenameGate(int GateId, const char *pName);
	void SetEditMode(bool EditMode);
	bool EditMode() const { return m_EditMode; }
	std::string PointName(int Point) const;
	int RecordMillis() const; // map record, -1 if unknown
	int PlayerBestMillis() const; // personal best from the server, -1 if unknown

	static void FormatMillis(int Millis, char *pBuf, int BufSize); // 1:02.34 / 12.34
	static void FormatDiff(int Millis, char *pBuf, int BufSize); // +0.31 / -1.20

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
	double m_StartTick = -1.0; // when the start line was left, -1 if unknown
	int m_InTimecp = -1;
	bool m_InStart = false;
	bool m_InFinish = false;

	// HUD after each split
	int64_t m_HudShowUntil = 0;
	std::string m_HudSegment;
	int m_HudMillis = -1;
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

	void Load();
	void Save();
	void Append(const DdnettySplits::SAttempt &Attempt);
	void ResetRun();
	void ProcessStep(vec2 PrevPos, vec2 Pos, int PrevTick, int CurTick);
	void OnPoint(int Point, double Tick);
	bool IsPractice() const;

	vec2 CursorPos() const;
	vec2 SnapPos(vec2 Pos) const;
	int GateAt(vec2 Pos, float MaxDistance) const;
	void BeginDrag();
	void EndDrag();
	void GatesChanged();

	void RenderGates();
	void RenderHud();

	static void ConEdit(IConsole::IResult *pResult, void *pUserData);
};

#endif // GAME_CLIENT_COMPONENTS_DDNETTY_SPLITS_H
