#pragma once
#include "BaseEdit.h"

namespace FilterEdit
{
class CUnitFloatEdit : public CBaseEdit
{
public:
	CUnitFloatEdit();
	virtual ~CUnitFloatEdit();

	double GetValue () const;

protected:
	//{{AFX_MSG(CFloatEdit)
	afx_msg void OnKillFocus(CWnd* pNewWnd);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

private:
	static bool initialised;
	static CRegEx regEx;
};
}
