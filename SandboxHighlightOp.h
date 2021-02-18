#pragma once

#include "sprk_ops.h"

#define WM_SANDBOX_OP ( WM_USER + 1 )

class CHPSView;

class SandboxHighlightOperator : public HPS::SelectOperator
{
public:
	SandboxHighlightOperator(
		CHPSView * in_cview);
	virtual ~SandboxHighlightOperator();

	virtual HPS::UTF8 GetName() const { return "MFC_SandboxHighlightOperator"; }

	virtual bool OnMouseDown(HPS::MouseState const & in_state);
	virtual bool OnTouchDown(HPS::TouchState const & in_state);

	static ::HPS::Selection::Level SelectionLevel;

private:

	void HighlightCommon();

	CHPSView * cview;
};
