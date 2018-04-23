/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

#include <gnmx/common.h>

#include <gnmx/cue.h>

using namespace sce::Gnm;
using namespace sce::Gnmx;

void ConstantUpdateEngine::preDispatchDraw(GnmxDrawCommandBuffer *dcb, GnmxDispatchCommandBuffer *acb, GnmxConstantCommandBuffer *ccb,
	sce::Gnm::DispatchOrderedAppendMode *pOutOrderedAppendMode, uint32_t *pOutDispatchDrawIndexDeallocationMask, uint32_t *pOutSgprKrbLoc, uint32_t *pOutSgprInstanceCs,
	sce::Gnm::DispatchDrawMode *pOutDispatchDrawMode, uint32_t *pOutSgprVrbLoc, uint32_t *pOutSgprInstanceVs)
{
	SCE_GNM_ASSERT_MSG(m_dcb && m_ccb && m_acb, "No dcb, ccb, or acb are linked to this object -- Please call: bindCommandBuffers() API.");
	SCE_GNM_ASSERT_MSG(dcb == m_dcb, "Invalid dcb pointer -- the dcb pointer must match the one linked to this object.");
	SCE_GNM_ASSERT_MSG(ccb == m_ccb, "Invalid ccb pointer -- the ccb pointer must match the one linked to this object.");
	SCE_GNM_ASSERT_MSG(acb == m_acb, "Invalid acb pointer -- the acb pointer must match the one linked to this object.");

	preDispatchDraw(pOutOrderedAppendMode, pOutDispatchDrawIndexDeallocationMask, pOutSgprKrbLoc, pOutSgprInstanceCs, pOutDispatchDrawMode, pOutSgprVrbLoc, pOutSgprInstanceVs);
}
void ConstantUpdateEngine::postDispatchDraw(GnmxDrawCommandBuffer *dcb, GnmxDispatchCommandBuffer *acb, GnmxConstantCommandBuffer *ccb)
{
	SCE_GNM_ASSERT_MSG(m_dcb && m_ccb && m_acb, "No dcb, ccb, or acb are linked to this object -- Please call: bindCommandBuffers() API.");
	SCE_GNM_ASSERT_MSG(dcb == m_dcb, "Invalid dcb pointer -- the dcb pointer must match the one linked to this object.");
	SCE_GNM_ASSERT_MSG(ccb == m_ccb, "Invalid ccb pointer -- the ccb pointer must match the one linked to this object.");
	SCE_GNM_ASSERT_MSG(acb == m_acb, "Invalid acb pointer -- the acb pointer must match the one linked to this object.");

	postDispatchDraw();
}
