// LithTech 1.0 scalar, constant and math spellings, expressed in terms of the Jupiter SDK

#ifndef __DE_TYPES_H__
#define __DE_TYPES_H__

#ifndef __LTBASEDEFS_H__
#include "ltbasedefs.h"
#endif

#ifndef __LTBASETYPES_H__
#include "ltbasetypes.h"
#endif

#ifndef __LTVECTOR_H__
#include "ltvector.h"
#endif

#ifndef __ILTSTREAM_H__
#include "iltstream.h"
#endif

#ifndef __LTROTATION_H__
#include "ltrotation.h"
#endif

#ifndef __LTMATRIX_H__
#include "ltmatrix.h"
#endif

#ifndef __LTPLANE_H__
#include "ltplane.h"
#endif

#ifndef __LTLINK_H__
#include "ltlink.h"
#endif

#ifndef __LTRECT_H__
#include "ltrect.h"
#endif


/*
    Scalars

    LT1 spelled most of these as macros.
    They are typedefs here, and the widths follow Jupiter:

    - DBOOL   was 'char'          -> LTBOOL (unsigned int)
    - DDWORD  was 'unsigned long' -> uint32

    NOTE on DDWORD: 1998 code stores pointers in it in places,
    which is a truncation bug on x64 and the compiler says so.
*/
typedef LTBOOL      DBOOL;
typedef uint8       DBYTE;
typedef uint16      D_WORD;
typedef uint32      DDWORD;
typedef float       DFLOAT;
typedef double      DDOUBLE;
typedef LTRESULT    DRESULT;


// Constants

#define DNULL       LTNULL
#define DTRUE       LTTRUE
#define DFALSE      LTFALSE


// Result codes

#define DE_OK               LT_OK
#define DE_ERROR            LT_ERROR
#define DE_INSIDE           LT_INSIDE
#define DE_OUTSIDE          LT_OUTSIDE        // 65 in both
#define DE_INVALIDPARAMS    LT_INVALIDPARAMS


/*
    Math types

    DVector -> LTVector is a clean mapping, and Jupiter still ships the VEC_* macro family LT1 code is written against.

    DRotation -> LTRotation is also exact,
    only the names differ:

        m_Vec.x -> m_Quat[0]
        m_Vec.y -> m_Quat[1]
        m_Vec.z -> m_Quat[2]
        m_Spin  -> m_Quat[3]
*/
typedef LTVector    DVector;
typedef LTRotation  DRotation;
typedef LTMatrix    DMatrix;
typedef LTPlane     DPlane;

/*
    2D types

    All four are direct mappings
*/

typedef LTFloatPt   DFloatPt;
typedef LTIntPt     DIntPt;
typedef LTWarpPt    DWarpPt;
typedef LTRect      DRect;

/*
    File streams
    
    Same class renamed, declaring the same methods in the same order with uint32 where LT1 said unsigned long.
    Jupiter inserts WriteStream between GetLen and Write and makes ReadString/WriteString pure (neither is reachable from game code).

    OpenFile had a slight drift: LT1 took "char *pFilename", 
    Jupiter takes "const char *", which is what every existing call site converts to.
*/
typedef ILTStream   DStream;

// LT1's list types
typedef LTLink      DLink;
typedef LTList      DList;
typedef CheapLTLink CheapDLink;

#define ROT_INIT(r)         { (r).Init(); }
#define ROT_COPY(dest, src) { (dest) = (src); }

// LT1's plane helper
#define DIST_TO_PLANE(vec, plane) \
    (VEC_DOT((plane).m_Normal, (vec)) - (plane).m_Dist)

#define PLANE_COPY(dest, src) \
    {\
        VEC_COPY((dest).m_Normal, (src).m_Normal)\
        (dest).m_Dist = (src).m_Dist;\
    }

#define PLANE_SET(plane, _x, _y, _z, _dist) \
    {\
        (plane).m_Normal.x = (_x);\
        (plane).m_Normal.y = (_y);\
        (plane).m_Normal.z = (_z);\
        (plane).m_Dist = (_dist);\
    }

// LT1's basetypes_de.h min/max macros
#define DMIN(a,b) ((a) < (b) ? (a) : (b))
#define DMAX(a,b) ((a) > (b) ? (a) : (b))

// Jupiter has no LTCLAMP, so this is an added in
// It evaluates 'a' up to three times as LT1's did
#define DCLAMP(a, min, max) ((a) < (min) ? (min) : ((a) > (max) ? (max) : (a)))


/*
    Engine hints Jupiter dropped
    
    Defined as 0 so ORing them changes nothing.
    LT1's bit values must not be copied: 
    Jupiter reuses them (1<<0 is its MESSAGE_GUARANTEED, 1<<3 FLAG_CASTSHADOWS), 
    so game code gets Jupiter's.
*/

// In LT1: let the engine batch small messages.
// Jupiter has no batching control, and sending is not guaranteed by default.
#define MESSAGE_NAGGLE          0
#define MESSAGE_NAGGLEFAST      0
#define MESSAGE_NAGGLEMASK      0

// In LT1: bypass the engine's outgoing bandwidth throttle
#define MESSAGE_NOTHROTTLE      0

// In LT1: a 200ms blend between model animations
#define FLAG_ANIMTRANSITION     0

// In LT1: gouraud shade this model
#define FLAG_MODELGOURAUDSHADE  0

// In LT1: draw this model as wireframe.  
// A debug view Jupiter dropped.
#define FLAG_MODELWIREFRAME     0

/*
    LT1 values with no Jupiter counterpart
    OBJSTATE_AUTODEACTIVATE_NOW was 3, past the end of Jupiter's OBJSTATE_ enum
    Jupiter deactivates objects itself and has no PingObjects, so it maps to OBJSTATE_ACTIVE.
*/

#define OBJSTATE_AUTODEACTIVATE_NOW OBJSTATE_ACTIVE

// In LT1: also save portal states.
//There are no portal states to save.
#define SAVEOBJECTS_SAVEPORTALS 0

// In LT1: stream this sound from disk.
// Jupiter decides streaming itself and doesn't expose a flag for it
#define PLAYSOUND_FILESTREAM    0

// In LT1: this light casts on solid geometry only / renders as fog volume.
// Jupiter expresses both through object properties
#define FLAG_SOLIDLIGHT         0
#define FLAG_FOGLIGHT           0

// In LT1: environment map (chrome) this model. 
// Jupiter expresses this through render styles rather than a bit. A chrome prop renders untinted.
#define FLAG_ENVIRONMENTMAP     0

// In LT1: tint the model in the team color/keep it across a world switch.
// Their bits are Jupiter's FLAG_SPRITEBIAS and FLAG_TOUCHABLE, so both are 0.
#define FLAG_MODELTINT           0
// LT1 never read FLAG_KEEPALIVE. CF_ALWAYSLOAD and CF_STATIC decide that.
#define FLAG_KEEPALIVE           0


// Sound handle.. same handle, different name

typedef HLTSOUND    HSOUNDDE;


#define FLIPSCREEN_CANDRAWCONSOLE   0

#endif // __DE_TYPES_H__
