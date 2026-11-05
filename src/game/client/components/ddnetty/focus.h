// ddnetty: focus mode. A bind turns it on and off; with every option on only the map and your
// own tee are left on screen. Builds on BestClient's p_focus_mode (chat, names, effects, HUD,
// scoreboard, UI, music player) and adds other players, splits, checkpoints, broadcast/votes
// and the DDNetty messages. Every check goes through Hides().
#ifndef GAME_CLIENT_COMPONENTS_DDNETTY_FOCUS_H
#define GAME_CLIENT_COMPONENTS_DDNETTY_FOCUS_H

#include <engine/console.h>
#include <engine/shared/protocol.h>

#include <game/client/component.h>

#include <cstdint>

class CFocus : public CComponent
{
public:
	enum EItem
	{
		// BestClient's options
		ITEM_CHAT = 0,
		ITEM_NAMES,
		ITEM_EFFECTS,
		ITEM_HUD,
		ITEM_SCOREBOARD,
		ITEM_UI,
		ITEM_MUSIC,
		// DDNetty's
		ITEM_OTHERS, // other tees with their hooks, shots and weapons (not your team, dummy or ghost)
		ITEM_SPLITS, // splits box and the time at the checkpoints
		ITEM_GATES, // your checkpoints drawn on the map
		ITEM_BROADCAST, // server broadcast and the vote window
		ITEM_NOTICES, // DDNetty messages (new best segment, ...)
		NUM_ITEMS,
	};

	enum EPreset
	{
		PRESET_ONLY_GAME = 0, // everything
		PRESET_LIGHT, // chat, names and effects
	};

	int Sizeof() const override { return sizeof(*this); }
	void OnConsoleInit() override;
	void OnReset() override;
	void OnMapLoad() override;
	void OnNewSnapshot() override;
	void OnRender() override;

	// partners: tees that hooked you or that you hooked (duo maps are often played without /team)
	static constexpr int PARTNER_SECONDS = 30;
	int RecentPartner(int WithinTicks) const; // the latest one, -1 if none
	bool HookedRecently(int ClientId, int WithinTicks) const;

	bool Active() const;
	void SetActive(bool Active);
	bool Hides(EItem Item) const;
	// the tee (and its hook, shots and name) is hidden: someone else, not in your team
	bool HidesTee(int ClientId) const;
	static int *ItemConfig(EItem Item);
	static void ApplyPreset(EPreset Preset);

private:
	static void ConFocus(IConsole::IResult *pResult, void *pUserData);
	static void ConFocusHold(IConsole::IResult *pResult, void *pUserData);
	int64_t m_NoticeUntil = 0;
	int m_aLastHookTick[MAX_CLIENTS] = {}; // game tick of the last hook between you and each tee, 0 = never
};

#endif
