#pragma once
#include "BaseEdit.h"

namespace FilterEdit
{
class CFloatEdit : public CBaseEdit
{
public:
	enum e_Sign {eSigned, eUnsigned};

	CFloatEdit (const e_Sign eSign = eSigned,
		const unsigned int uiWhole = 19);
	virtual ~CFloatEdit ();

	void SetSigned();
	void SetUnsigned();
	void SetWhole(const unsigned int uiWhole);
	double GetValue () const;

protected:
	//{{AFX_MSG(CFloatEdit)
	afx_msg void OnKillFocus(CWnd* pNewWnd);
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

private:
	static bool m_bInitialised;
	static CRegEx m_SignedRegEx;
	static CRegEx m_UnsignedRegEx;

	static void CreateRegexes(const unsigned int uiWhole);
};
}
