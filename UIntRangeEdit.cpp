#include "StdAfx.h"
#include "UIntRangeEdit.h"

namespace FilterEdit
{
CUIntRangeEdit::CUIntRangeEdit () :
	CUIntEdit (),
	m_uiLower (0),
	m_uiHigher (0),
	m_uiInc (1)
{
}

CUIntRangeEdit::CUIntRangeEdit (const unsigned int uiLower,
	const unsigned int uiHigher) :
	CUIntEdit (),
	m_uiLower (uiLower),
	m_uiHigher (uiHigher),
	m_uiInc (1)
{
}

CUIntRangeEdit::~CUIntRangeEdit ()
{
}

void CUIntRangeEdit::SetRange (const unsigned int uiLower,
	const unsigned int uiHigher)
{
	m_uiLower = uiLower;
	m_uiHigher = uiHigher;
}

void CUIntRangeEdit::SetIncrement (const unsigned int uiInc)
{
	m_uiInc = uiInc;
}

void CUIntRangeEdit::Increment (int iInc)
{
	unsigned int uiVal = GetValue ();
	CString strUInt;

	uiVal += iInc > 0 ? m_uiInc : m_uiInc * -1;

	// Be helpful: Start at m_uiLower if no text in control
	if (iInc > 0 && uiVal < m_uiLower) uiVal = m_uiLower;

	strUInt.Format (_T("%d"), uiVal);

	if (SemanticCheck (strUInt))
	{
		SetWindowText (strUInt);
	}
}

bool CUIntRangeEdit::SemanticCheck (const CString &strText,
	CString * /*pstrErrorMsg = 0*/)
{
	unsigned int uiVal = _tstoi (strText);

	// Now we can do the semantic validation
	return uiVal >= m_uiLower && uiVal <= m_uiHigher;
}
}
