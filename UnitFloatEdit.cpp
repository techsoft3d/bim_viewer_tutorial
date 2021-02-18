#include "StdAfx.h"
#include "UnitFloatEdit.h"

#include <sstream>

namespace FilterEdit
{
bool CUnitFloatEdit::initialised = false;
CRegEx CUnitFloatEdit::regEx;

CUnitFloatEdit::CUnitFloatEdit()
	: CBaseEdit(&regEx)
{
	if (!initialised)
	{
		CString strRegEx(_T("0\\.?|0?\\.[0-9]*|1\\.?0*"));

		regEx.assign(strRegEx);
		initialised = true;
	}
}

CUnitFloatEdit::~CUnitFloatEdit()
{
}

double CUnitFloatEdit::GetValue () const
{
	CString strFloat;
	std::basic_stringstream<TCHAR> ss;
	double dValue = 0;

	GetWindowText (strFloat);
	ss << static_cast<const TCHAR *> (strFloat);
	ss >> dValue;
	return dValue;
}

BEGIN_MESSAGE_MAP(CUnitFloatEdit, CBaseEdit)
	ON_WM_KILLFOCUS()
END_MESSAGE_MAP()

void CUnitFloatEdit::OnKillFocus (CWnd *pNewWnd)
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
