// This file can be included several times.
// DDNetty-only settings. Kept in a separate file so syncing with BestClient doesn't conflict.

#ifndef MACRO_CONFIG_INT
#error "The config macros must be defined"
#define MACRO_CONFIG_INT(Name, ScriptName, Def, Min, Max, Save, Desc) ;
#define MACRO_CONFIG_COL(Name, ScriptName, Def, Save, Desc) ;
#define MACRO_CONFIG_STR(Name, ScriptName, Len, Def, Save, Desc) ;
#endif

// Fake ping
MACRO_CONFIG_INT(DtFakePing, dt_fake_ping, 0, 0, 999, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Ping (ms) the server measures and shows on the scoreboard, without delaying your own input (0 = off, 1 = 0 ms / lowest possible, 999 = max)")

// Splits
MACRO_CONFIG_INT(DtSplits, dt_splits, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Record split times on race maps")
MACRO_CONFIG_INT(DtSplitsHud, dt_splits_hud, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show the segment time and the predicted final time after each split")
MACRO_CONFIG_INT(DtSplitsSource, dt_splits_source, 0, 0, 2, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Split points (0 = map time checkpoints and your checkpoints, 1 = only map time checkpoints, 2 = only your checkpoints)")
MACRO_CONFIG_INT(DtSplitsAverage, dt_splits_average, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Compare against the average of your last attempts instead of your best")
MACRO_CONFIG_INT(DtSplitsPractice, dt_splits_practice, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Include practice attempts in the split stats")
MACRO_CONFIG_INT(DtSplitsShowGates, dt_splits_show_gates, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show your split checkpoints on the map")
MACRO_CONFIG_INT(DtSplitsTargetMode, dt_splits_target_mode, 0, 0, 3, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Target time for the prediction (0 = server record, 1 = your best, 2 = custom time per map, 3 = a top player from the map's reference file)")
MACRO_CONFIG_STR(DtSplitsRefPlayer, dt_splits_ref_player, 32, "", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Top player compared with in target mode 3 (empty = the fastest in the reference file)")
MACRO_CONFIG_INT(DtSplitsWarning, dt_splits_warning, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show a message when the prediction is slower than the target time")
MACRO_CONFIG_STR(DtSplitsMessage, dt_splits_message, 64, "", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Message shown when the prediction is slower than the target time (empty = default)")
MACRO_CONFIG_INT(DtSplitsHudIdeal, dt_splits_hud_ideal, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show your ideal run (sum of your best segments) in the splits HUD box")
MACRO_CONFIG_INT(DtSplitsSegmentDemos, dt_splits_segment_demos, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Save a demo of every new best segment (also needs to be turned on for each map)")
MACRO_CONFIG_INT(DtSplitsSegmentDemosKeep, dt_splits_segment_demos_keep, 1, 1, 10, CFGFLAG_CLIENT | CFGFLAG_SAVE, "How many demos to keep per segment (older ones move to the old folder)")
MACRO_CONFIG_INT(DtSplitsIdealDemo, dt_splits_ideal_demo, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Build the ideal run demo (your best segments stitched together) when every segment has a demo")
MACRO_CONFIG_INT(DtSplitsIdealDemoBlend, dt_splits_ideal_demo_blend, 25, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Ticks over which a seam of the ideal run demo is smoothed")
MACRO_CONFIG_INT(DtSplitsIdealDemoShots, dt_splits_ideal_demo_shots, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Keep weapon shots and their sounds in the ideal run demo")
MACRO_CONFIG_INT(DtSplitsIdealGhost, dt_splits_ideal_ghost, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Also save the ideal run as a ghost (\"ideal run\" in the ghost list) to race against")
MACRO_CONFIG_INT(DtSplitsFocusMargin, dt_splits_focus_margin, 3000, 0, 600000, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Average and variation only count segments entered at most this many ms behind your best run (0 = off, a map can set its own)")
MACRO_CONFIG_INT(DtSplitsRunEntriesOnly, dt_splits_run_entries_only, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Best segments and the ideal run only count segments entered like in a real run (not after a mid start, going back, waiting or crossing again)")
MACRO_CONFIG_INT(DtSplitsWaitLimit, dt_splits_wait_limit, 15, 0, 600, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Seconds over your best before a point after which the next segment counts as entered after waiting (0 = off)")
MACRO_CONFIG_INT(DtSplitsCountDummy, dt_splits_count_dummy, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Count runs made with the dummy or switching between your tees")
MACRO_CONFIG_INT(DtSplitsPbNotice, dt_splits_pb_notice, 2, 0, 2, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Message on a new best segment (0 = off, 1 = your best, 2 = also when it beats everyone in the reference file)")
MACRO_CONFIG_INT(DtSplitsPbSound, dt_splits_pb_sound, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Play a sound with the new best segment message")

// BestClient network
MACRO_CONFIG_INT(DtBestClientPresence, dt_bestclient_presence, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Announce yourself to the BestClient indicator server (it marks DDNetty as FAKE since it is not an official BestClient build)")
MACRO_CONFIG_INT(DtFakePingSafety, dt_fake_ping_safety, 1, 0, 5, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Fake ping below your real ping: ticks of distance from the server's tick (0 = lowest possible, may drop inputs)")

// ddnetty focus mode (p_focus_mode and its BestClient options are reused)
MACRO_CONFIG_INT(DtFocusNotice, dt_focus_notice, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show \"Focus ON/OFF\" in the corner for a second when the focus mode is toggled")
MACRO_CONFIG_INT(DtFocusHideOthers, dt_focus_hide_others, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Focus mode hides the other tees with their hooks and shots (your team, dummy and ghost stay)")
MACRO_CONFIG_INT(DtFocusHideSplits, dt_focus_hide_splits, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Focus mode hides the splits box")
MACRO_CONFIG_INT(DtFocusHideGates, dt_focus_hide_gates, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Focus mode hides your checkpoints on the map")
MACRO_CONFIG_INT(DtFocusHideBroadcast, dt_focus_hide_broadcast, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Focus mode hides the server broadcast and the vote window")
MACRO_CONFIG_INT(DtFocusHideNotices, dt_focus_hide_notices, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Focus mode hides the DDNetty messages (new best segment, ...)")
