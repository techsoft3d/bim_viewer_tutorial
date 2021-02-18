#include "StdAfx.h"
#include "UIntEdit.h"

namespace FilterEdit
{
bool CUIntEdit::m_bInitialised = false;
CRegEx CUIntEdit::m_RegEx;

CUIntEdit::CUIntEdit () :
	CBaseEdit (&m_RegEx)
{
	if (!m_bInitialised)
	{
		CString strRegEx (_T("0|[1-9][0-9]*"));

		m_RegEx.assign (strRegEx);
		m_bInitialised = true;
	}
}

CUIntEdit::~CUIntEdit ()
{
}

unsigned int CUIntEdit::GetValue () const
{
	CString strUInt;

	GetWindowText (strUInt);
	return _tstoi (strUInt);
}
}
