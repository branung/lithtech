// ----------------------------------------------------------------
//  d3d_model_load.cpp
// lithtech (c) 2000
// ----------------------------------------------------------------

#include "bdefs.h"
#include "model.h"
#include "model_ops.h"
#include "iltstream.h"
#include "conparse.h"
#include "ltb.h"

#if defined(_WINDOWS) || defined(USE_DXVK)
#include "render.h"
#endif

// ------------------------------------------------------------------------
// D3D specific object creation routine.
// ------------------------------------------------------------------------
CDIModelDrawable *ModelPiece::CreateModelRenderObject( uint32 type )
{
#if !defined(DE_SERVER_COMPILE) && !defined(DE_HEADLESS_CLIENT)
			// Note : The renderer can create render objects without being initialized
			if (r_GetRenderStruct()->m_bLoaded)
				return (CDIModelDrawable*)r_GetRenderStruct()->CreateRenderObject((enum CRenderObject::RENDER_OBJECT_TYPES)type);

			// With the renderer shut down the dummy below skips this piece's
			// geometry and ignores polygons, and g_ModelMgr caches the model that way
			// for the session so it never draws.
			{
				static bool s_bWarned = false;
				if (!s_bWarned)
				{
					s_bWarned = true;
					dsi_ConsolePrint("LT ERROR: loading model geometry with the renderer shut down! "
						"Something loaded a world between ShutdownRender and SetRenderMode.");
				}
			}
		
#endif
		// default or server option
		CDIModelDrawable* p;
		LT_MEM_TRACK_ALLOC(p = new CDIModelDrawable(),LT_MEM_TYPE_MODEL);
		return p;	// Create a dummy Drawable object (it knows how to load - or, really, skip by a load)...

}
