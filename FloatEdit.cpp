#include "StdAfx.h"
#include "FloatEdit.h"

#include <sstream>

namespace FilterEdit
{
bool CFloatEdit::m_bInitialised = false;
CRegEx CFloatEdit::m_SignedRegEx;
CRegEx CFloatEdit::m_UnsignedRegEx;

CFloatEdit::CFloatEdit (const e_Sign eSign/* = eSigned*/,
	const unsigned int uiWhole/* = 19*/) :
	CBaseEdit (eSign == eSigned ? &m_SignedRegEx : &m_UnsignedRegEx)
{
	if (!m_bInitialised)
	{
		CreateRegexes(uiWhole);
		m_bInitialised = true;
	}
}

CFloatEdit::~CFloatEdit ()
{
}

void CFloatEdit::SetSigned()
{
	SetRegEx (&m_SignedRegEx);
}

void CFloatEdit::SetUnsigned()
{
	SetRegEx (&m_UnsignedRegEx);
}

void CFloatEdit::SetWhole(const unsigned int uiWhole)
{
	CreateRegexes(uiWhole);
}

double CFloatEdit::GetValue () const
{
	CString strFloat;
	std::basic_stringstream<TCHAR> ss;
	double dValue = 0;

	GetWindowText (strFloat);
	ss << static_cast<const TCHAR *> (strFloat);
	ss >> dValue;
	return dValue;
}

void CFloatEdit::CreateRegexes(const unsigned int uiWhole)
{
	TCHAR szSeparator[4] = _T("");
	CString strSeparator;
	CString strSignedRegEx;
	CString strUnsignedRegEx;

	::GetLocaleInfo (LOCALE_USER_DEFAULT, LOCALE_SDECIMAL, szSeparator, 4);
	strSeparator = szSeparator;
	EscapeString(strSeparator);
	strSignedRegEx.Format(_T("[-+]?([0-9]{1,%d}(%s[0-9]*)?")
		_T("|%s[0-9]+)"), uiWhole, strSeparator.GetString(), strSeparator.GetString());
	strUnsignedRegEx.Format(_T("([0-9]{1,%d}(%s[0-9]*)?")
		_T("|%s[0-9]+)"), uiWhole, strSeparator.GetString(), strSeparator.GetString());
	m_SignedRegEx.assign (strSignedRegEx);
	m_UnsignedRegEx.assign (strUnsignedRegEx);
}

BEGIN_MESSAGE_MAP(CFloatEdit, CBaseEdit)
	ON_WM_KILLFOCUS()
END_MESSAGE_MAP()

void CFloatEdit::OnKillFocus (CWnd *pNewWnd)
{
	if (!GetAllowEmpty() || GetWindowTextLength() > 0)
	{
		CString strFloat;
		CString strLeft;
		CString strRight;
		int iDot = -1;

		GetWindowText (strFloat);
		iDot = strFloat.Find ('.');	

		if (iDot > -1)
		{
			strLeft = strFloat.Left(iDot);
			strRight = strFloat.Mid(iDot + 1);
		}
		else
		{
			strLeft = strFloat;
		}

		if (strLeft.IsEmpty()) strLeft = '0';

		if (strRight.IsEmpty())
		{
			strRight = _T("00");
		}
		else if (strRight.GetLength () == 1)
		{
			strRight += '0';
		}

		SetWindowText (strLeft + '.' + strRight);
	}

	CBaseEdit::OnKillFocus (pNewWnd);
}
}
