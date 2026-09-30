
//
//
// D3D-specific sky drawing code.
//
//

#include "precompile.h"
#include "common_stuff.h"
#include "renderstruct.h"
#include "3d_ops.h"
#include "common_draw.h"
#include "drawsky.h"

#include "d3d_renderworld.h"

extern void d3d_DrawPolyGrid(const ViewParams &Params, LTObject *pGrid);
extern void d3d_DrawSprite(const ViewParams &Params, LTObject *pObj);

void d3d_DrawSkyObjects(const ViewParams& SkyParams)
{
	// Narrow the viewport to the sky rectangle or else D3D stretches the sky across the whole screen.
	// Clamped to the current viewport
	D3DVIEWPORT9 cOldViewport;
	bool bViewportSet = false;

	if (g_CV_SkyViewport.m_Val)
	{
		PD3DDEVICE->GetViewport(&cOldViewport);

		int32 nLeft   = LTMAX((int32)SkyParams.m_Rect.left,   (int32)cOldViewport.X);
		int32 nTop    = LTMAX((int32)SkyParams.m_Rect.top,    (int32)cOldViewport.Y);
		int32 nRight  = LTMIN((int32)SkyParams.m_Rect.right,  (int32)(cOldViewport.X + cOldViewport.Width));
		int32 nBottom = LTMIN((int32)SkyParams.m_Rect.bottom, (int32)(cOldViewport.Y + cOldViewport.Height));

		// Nothing of the sky rectangle is on the target
		if ((nRight <= nLeft) || (nBottom <= nTop))
			return;

		D3DVIEWPORT9 cViewPort;
		cViewPort.X      = (DWORD)nLeft;
		cViewPort.Y      = (DWORD)nTop;
		cViewPort.Width  = (DWORD)(nRight - nLeft);
		cViewPort.Height = (DWORD)(nBottom - nTop);
		cViewPort.MinZ   = cOldViewport.MinZ;
		cViewPort.MaxZ   = cOldViewport.MaxZ;

		if (SUCCEEDED(PD3DDEVICE->SetViewport(&cViewPort)))
			bViewportSet = true;
	}

	//disable reading/writing to the Z buffer
	StateSet ssZWrite(D3DRS_ZWRITEENABLE, 0);
	StateSet ssZRead(D3DRS_ZENABLE, D3DZB_FALSE);

	// Set the fog distances..
	StateSet ssFogStart(D3DRS_FOGSTART, *((uint32*)&g_CV_SkyFogNearZ.m_Val));
	StateSet ssFogEnd(D3DRS_FOGEND, *((uint32*)&g_CV_SkyFogFarZ.m_Val));

	//Preserve the old fog state so it can be properly restored
	uint32 oldFogEnable;
	D3D_CALL(PD3DDEVICE->GetRenderState(D3DRS_FOGENABLE, (DWORD *) &oldFogEnable));

	// Disable SPMT.
	StageStateSet tssColorOp1(1, D3DTSS_COLOROP, D3DTOP_DISABLE);

	for(uint32 i = 0; i < (uint32)g_pSceneDesc->m_nSkyObjects; i++)
	{
		LTObject *pSkyObject = g_pSceneDesc->m_SkyObjects[i];
		if (pSkyObject->m_Flags & FLAG_VISIBLE) 
		{
			if ((pSkyObject->m_ObjectType == OT_WORLDMODEL) && g_CV_DrawWorldModels.m_Val) 
			{
				if ((pSkyObject->m_Flags & FLAG_FOGDISABLE) && oldFogEnable) 
				{
					D3D_CALL(PD3DDEVICE->SetRenderState(D3DRS_FOGENABLE, 0));
				}
				
				// Setup translucency states for it.
				if (pSkyObject->IsTranslucent()) 
				{
					d3d_SetTranslucentObjectStates(!!(pSkyObject->m_Flags2 & FLAG2_ADDITIVE)); 
				}
				else 
				{
					d3d_UnsetTranslucentObjectStates(0);
				}

				if (g_Device.m_pRenderWorld)
				{
					WorldModelInstance *pInstance = (WorldModelInstance*)pSkyObject;
					CD3D_RenderWorld *pWorldModel = g_Device.m_pRenderWorld->FindWorldModel(pInstance->m_pOriginalBsp->m_WorldName);
					if (pWorldModel)
					{
						// LT1 transforms each sky polygon by the object's own matrix before drawing it.
						// A sky WorldModel with a Rotation property draws unrotated without this
						extern int32 g_bLT1SkyObjectTransform;
						if (g_bLT1SkyObjectTransform)
						{
							// Set explicitly as CD3D_RenderWorld::Draw only forces a cull mode for the main world,
							// so a sky WorldModel would inherit whatever drew last
							StateSet ssCull(D3DRS_CULLMODE, D3DCULL_CCW);

							ViewParams ModelParams = SkyParams;
							MatMul(&ModelParams.m_FullTransform,
								&SkyParams.m_FullTransform, &pInstance->m_Transform);
							ModelParams.m_mInvWorld = pInstance->m_BackTransform;
							d3d_SetD3DMat(D3DTS_WORLD, &pInstance->m_Transform);
							pWorldModel->Draw(ModelParams, true);
							d3d_SetD3DMat(D3DTS_WORLD, &SkyParams.m_mIdentity);
						}
						else
						{
							pWorldModel->Draw(SkyParams, true);
						}
					}
				}

				D3D_CALL(PD3DDEVICE->SetRenderState(D3DRS_FOGENABLE, oldFogEnable));
			}
			else if((pSkyObject->m_ObjectType == OT_POLYGRID) && g_CV_DrawPolyGrids.m_Val) 
			{
				// Set the states for it.
				if (pSkyObject->IsTranslucent())  
				{
					d3d_SetTranslucentObjectStates(); 
				}
				else 
				{
					d3d_UnsetTranslucentObjectStates(0);
				}

				d3d_DrawPolyGrid(SkyParams, pSkyObject); 
			}
			else if((pSkyObject->m_ObjectType == OT_SPRITE) && g_CV_DrawSprites.m_Val) 
			{
				d3d_SetTranslucentObjectStates();
				d3d_DrawSprite(SkyParams, pSkyObject); 
			} 
		}
	}

	// Unset translucent stuff.
	d3d_UnsetTranslucentObjectStates(0);

	// Put back what was there so CD3D_Device's cached viewport still describes the device
	if (bViewportSet)
		PD3DDEVICE->SetViewport(&cOldViewport);
}

