#pragma once

#include "PluginBase/Modules.h"

class C_BaseEntity;
class CGlowObjectManager;
class CViewSetup;
class CMatRenderContextPtr;

class Graphics final : public Module
{
public:
	Graphics();
	~Graphics();

	static Graphics* GetModule() { return Modules().GetModule<Graphics>(); }

	ConVar* GetDebugGlowConVar() const { return ce_graphics_debug_glow; }

private:
	ConVar* ce_graphics_disable_prop_fades;
	ConVar* ce_graphics_debug_glow;

	int m_ComputeEntityFadeHook;
	unsigned char ComputeEntityFadeOveride(C_BaseEntity* entity, float minDist, float maxDist, float fadeScale);

	int m_ApplyEntityGlowEffectsHook;
	void ApplyEntityGlowEffectsOverride(CGlowObjectManager* pThis, const CViewSetup* pSetup, int nSplitScreenSlot, CMatRenderContextPtr& pRenderContext, float flBloomScale, int x, int y, int w, int h);
};