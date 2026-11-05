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
MACRO_CONFIG_INT(DtSplitsTargetMode, dt_splits_target_mode, 0, 0, 2, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Target time for the prediction (0 = server record, 1 = your best, 2 = custom time per map)")
MACRO_CONFIG_INT(DtSplitsWarning, dt_splits_warning, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show a message when the prediction is slower than the target time")
MACRO_CONFIG_STR(DtSplitsMessage, dt_splits_message, 64, "", CFGFLAG_CLIENT | CFGFLAG_SAVE, "Message shown when the prediction is slower than the target time (empty = default)")
MACRO_CONFIG_INT(DtSplitsHudIdeal, dt_splits_hud_ideal, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show your ideal run (sum of your best segments) in the splits HUD box")
MACRO_CONFIG_INT(DtSplitsSegmentDemos, dt_splits_segment_demos, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Save a demo of every new best segment (also needs to be turned on for each map)")
MACRO_CONFIG_INT(DtSplitsSegmentDemosKeep, dt_splits_segment_demos_keep, 1, 1, 10, CFGFLAG_CLIENT | CFGFLAG_SAVE, "How many demos to keep per segment (older ones move to the old folder)")
MACRO_CONFIG_INT(DtSplitsIdealDemo, dt_splits_ideal_demo, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Build the ideal run demo (your best segments stitched together) when every segment has a demo")
MACRO_CONFIG_INT(DtSplitsIdealDemoBlend, dt_splits_ideal_demo_blend, 25, 0, 100, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Ticks over which a seam of the ideal run demo is smoothed")
MACRO_CONFIG_INT(DtSplitsIdealDemoShots, dt_splits_ideal_demo_shots, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Keep weapon shots and their sounds in the ideal run demo")
MACRO_CONFIG_INT(DtSplitsIdealGhost, dt_splits_ideal_ghost, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Also save the ideal run as a ghost (\"ideal run\" in the ghost list) to race against")

// BestClient network
MACRO_CONFIG_INT(DtBestClientPresence, dt_bestclient_presence, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Announce yourself to the BestClient indicator server (it marks DDNetty as FAKE since it is not an official BestClient build)")
MACRO_CONFIG_INT(DtFakePingSafety, dt_fake_ping_safety, 1, 0, 5, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Fake ping below your real ping: ticks of distance from the server's tick (0 = lowest possible, may drop inputs)")
