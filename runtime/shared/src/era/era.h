// The era sets. EngineEra names the engine line a game was built for, and selects that era's
// implementations of the engine routines where its behavior is too different to be a switch.
// The shared LT1 set sets it on both sides, as each has its own copy of the engine variables.
// An era routine still reads the vars that name its behaviors, so an A/B against Jupiter stays one console line.
// Jupiter (0) has no set: every entry is null, and each dispatch point falls through to Jupiter's own routine

#ifndef __ERA_H__
#define __ERA_H__

#ifndef __LTBASEDEFS_H__
#include "ltbasedefs.h"
#endif

struct CollideRequest;
struct CollideInfo;
struct MotionInfo;
class LTObject;
class MoveState;
struct LTRotation;

// EngineEra's values. Never renumbered, as configs and var tables write the number.
// 2 and 3 are kept for LT1.5 (KISS) and LT2 (NOLF), which this tree doesn't build
enum EngineEraId
{
	ENGINE_ERA_JUPITER = 0,   // no era set: Jupiter's own routines
	ENGINE_ERA_LT1     = 1,   // LithTech 1.0: Shogo, Blood 2
	ENGINE_ERA_COUNT
};

// True when it handled the move, false leaves it to Jupiter's walk
typedef bool (*EraCollideWithWorldFn)(CollideRequest &request, CollideInfo *pInfo);

// True when it took the step, false leaves it to Jupiter's CalcMotion
typedef bool (*EraCalcMotionFn)(MotionInfo *pInfo, LTObject *pObj, LTVector &dr, LTVector &v, LTVector &a,
	bool bApplyGravity, float dt);

// The object mover's three entries. Each is true when it handled the call, false leaves it to Jupiter's
typedef bool (*EraMoveObjectFn)(MoveState *pState, const LTVector &vNewPos, uint32 flags);
typedef bool (*EraChangeObjectDimensionsFn)(MoveState *pState, LTVector &newDims, LTBOOL bCollide,
	LTBOOL bTrivialReject, LTBOOL *pbRet);
typedef bool (*EraRotateWorldModelFn)(MoveState *pState, const LTRotation &cRotation, LTBOOL bDoCollisions);

// One era's implementations. A null entry is Jupiter's
struct EraImplSet
{
	const char					*m_pName;
	EraCollideWithWorldFn		m_pCollideWithWorld;		// top of CollideWithWorld (collision.cpp)
	EraCalcMotionFn				m_pCalcMotion;				// top of CalcMotion (motion.cpp)
	EraMoveObjectFn				m_pMoveObject;				// tops of MoveObject,
	EraChangeObjectDimensionsFn	m_pChangeObjectDimensions;	// ChangeObjectDimensions
	EraRotateWorldModelFn		m_pRotateWorldModel;		// and RotateWorldModel (moveobject.cpp)
};

// The set EngineEra selects. A value this build doesn't know selects Jupiter's and says so once
const EraImplSet &era_Get();

// The era implementations, one per module under era/
bool era_lt1_CollideWithWorld(CollideRequest &request, CollideInfo *pInfo);
bool era_lt1_CalcMotion(MotionInfo *pInfo, LTObject *pObj, LTVector &dr, LTVector &v, LTVector &a,
	bool bApplyGravity, float dt);
bool era_lt1_MoveObject(MoveState *pState, const LTVector &vNewPos, uint32 flags);
bool era_lt1_ChangeObjectDimensions(MoveState *pState, LTVector &newDims, LTBOOL bCollide,
	LTBOOL bTrivialReject, LTBOOL *pbRet);
bool era_lt1_RotateWorldModel(MoveState *pState, const LTRotation &cRotation, LTBOOL bDoCollisions);

#endif  // __ERA_H__
