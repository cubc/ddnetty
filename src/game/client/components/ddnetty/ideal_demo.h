// ddnetty: stitches the demos of the best segments into one demo of the ideal run.
// Runs as a job (see CSplits): reads every segment demo, keeps your tee and your team mate,
// shifts them in time so the segments follow each other and smooths the seams.
#ifndef GAME_CLIENT_COMPONENTS_DDNETTY_IDEAL_DEMO_H
#define GAME_CLIENT_COMPONENTS_DDNETTY_IDEAL_DEMO_H

#include <engine/client.h>
#include <engine/shared/jobs.h>

#include <functional>
#include <string>
#include <vector>

class CIdealDemoJob : public IJob
{
public:
	// one segment of the route, in route order
	struct SClip
	{
		std::string m_Demo; // segment demo (cut out of the replay recorder, keeps the game ticks)
		int m_Millis = 0;
		double m_StartTick = 0.0; // the segment inside the demo (fractional game ticks)
		double m_EndTick = 0.0;
		int m_MainId = -1; // client id of the tee whose segment it is
		int m_DuoId = -1; // its team mate, -1 on solo runs
	};

	struct SOptions
	{
		int m_BlendTicks = 25; // a seam is smoothed over this many ticks
		bool m_KeepShots = false; // weapon projectiles, lasers and their sounds
	};

	// reading and writing demos (IClient::DemoReadSnapshots / DemoWriteSnapshots in the game)
	using FRead = std::function<bool(const char *pPath, std::vector<IClient::SDemoSnapshot> &vOut)>;
	using FWrite = std::function<bool(const char *pDst, const char *pMapFromDemo, const std::vector<IClient::SDemoSnapshot> &vSnapshots)>;

	CIdealDemoJob(FRead Read, FWrite Write, std::vector<SClip> vClips, std::string Destination, SOptions Options);
	CIdealDemoJob(IClient *pClient, std::vector<SClip> vClips, std::string Destination, SOptions Options);

	// the stitching itself (Run calls it); public for tests
	void Build();

	// your tee at every tick of the ideal run (start to finish), for a ghost; same fields as CGhostCharacter
	struct SGhostTick
	{
		int m_X, m_Y, m_VelX, m_VelY, m_Angle, m_Direction, m_Weapon, m_HookState, m_HookX, m_HookY, m_AttackTick, m_Tick;
	};
	const std::vector<SGhostTick> &Ghost() const { return m_vGhost; }
	int GhostStartTick() const { return m_GhostStartTick; }
	// your tee's skin (from its client info): name, custom color, body and feet colors
	const std::vector<int> &GhostSkin() const { return m_vGhostSkin; }

	bool Success() const { return m_Success; }
	const std::string &Destination() const { return m_Destination; }
	const std::string &Error() const { return m_Error; }
	int TotalMillis() const { return m_TotalMillis; }

protected:
	void Run() override;

private:
	FRead m_Read;
	FWrite m_Write;
	std::vector<SClip> m_vClips;
	std::string m_Destination;
	SOptions m_Options;

	bool m_Success = false;
	std::string m_Error;
	int m_TotalMillis = 0;
	std::vector<SGhostTick> m_vGhost;
	int m_GhostStartTick = 0;
	std::vector<int> m_vGhostSkin; // m_aSkin[6], UseCustomColor, ColorBody, ColorFeet
};

#endif
