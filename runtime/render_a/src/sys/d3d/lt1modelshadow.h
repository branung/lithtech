// LithTech 1.0 model shadows
#ifndef __LT1MODELSHADOW_H__
#define __LT1MODELSHADOW_H__

class ViewParams;
class BaseObjectSet;

// Triangle vertices gathered per model, 3 per triangle.
//ABC models cap at 65535 vertices
#define ABC_SHADOW_MAX_VERTS 65535

// Draw LT1 shadows for every FLAG_SHADOW model in the set, right after the set's own triangles
void d3d_DrawLT1ModelShadows(const ViewParams& Params, BaseObjectSet* pSet);

#endif
