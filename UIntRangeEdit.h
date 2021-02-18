#pragma once
#include "UIntEdit.h"

namespace FilterEdit
{
class CUIntRangeEdit : public CUIntEdit
{
public:
	CUIntRangeEdit ();
	CUIntRangeEdit (const unsigned int uiLower, const unsigned int uiHigher);
	virtual ~CUIntRangeEdit ();
	void SetRange (const unsigned int uiLower, const unsigned int uiHigher);
	void SetIncrement (const unsigned int uiInc);
	virtual void Increment (const int iInc);

protected:
	virtual bool SemanticCheck (const CString &strText,
		CString *pstrErrorMsg = 0);

private:
	unsigned int m_uiLower;
	unsigned int m_uiHigher;
	unsigned int m_uiInc;
};
}
