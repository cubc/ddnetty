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
