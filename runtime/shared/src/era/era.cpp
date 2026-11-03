// The era sets, and the lookup EngineEra drives (era/era.h)

#include "bdefs.h"

#include "era/era.h"

extern int32 g_nEngineEra;   // engine_vars.cpp

// Indexed by EngineEraId. An entry is filled when that era's routine moves into its module under era/,
// and stays null (Jupiter's routine) until then
static const EraImplSet s_aEraSets[ENGINE_ERA_COUNT] =
{
	{ "Jupiter", LTNULL, LTNULL, LTNULL, LTNULL, LTNULL },
	{ "LT1",     LTNULL, LTNULL, LTNULL, LTNULL, LTNULL },
};

const EraImplSet &era_Get()
{
	const int32 nEra = g_nEngineEra;
	if (nEra >= 0 && nEra < ENGINE_ERA_COUNT)
		return s_aEraSets[nEra];

	// A value no build knows uses Jupiter's routines, with one line saying so rather than an index past the table
	static int32 s_nWarned = 0;
	if (s_nWarned != nEra)
	{
		s_nWarned = nEra;
		dsi_ConsolePrint("EngineEra %d is not an era this build knows; using Jupiter's routines", nEra);
	}
	return s_aEraSets[ENGINE_ERA_JUPITER];
}
