// This file can be included several times.
// DDNetty-only settings. Kept in a separate file so syncing with BestClient doesn't conflict.

#ifndef MACRO_CONFIG_INT
#error "The config macros must be defined"
#define MACRO_CONFIG_INT(Name, ScriptName, Def, Min, Max, Save, Desc) ;
#define MACRO_CONFIG_COL(Name, ScriptName, Def, Save, Desc) ;
#define MACRO_CONFIG_STR(Name, ScriptName, Len, Def, Save, Desc) ;
#endif

// Fake ping
MACRO_CONFIG_INT(DtFakePing, dt_fake_ping, 0, 0, 999, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Ping (ms) the server measures and shows on the scoreboard, without delaying your own input (0 = off, 999 = max)")

// Splits
MACRO_CONFIG_INT(DtSplits, dt_splits, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Record split times on race maps")
MACRO_CONFIG_INT(DtSplitsHud, dt_splits_hud, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show the segment time and the predicted final time after each split")
MACRO_CONFIG_INT(DtSplitsSource, dt_splits_source, 0, 0, 2, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Split points (0 = map time checkpoints and your checkpoints, 1 = only map time checkpoints, 2 = only your checkpoints)")
MACRO_CONFIG_INT(DtSplitsAverage, dt_splits_average, 0, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Compare against the average of your last attempts instead of your best")
MACRO_CONFIG_INT(DtSplitsPractice, dt_splits_practice, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Include practice attempts in the split stats")
MACRO_CONFIG_INT(DtSplitsShowGates, dt_splits_show_gates, 1, 0, 1, CFGFLAG_CLIENT | CFGFLAG_SAVE, "Show your split checkpoints on the map")
