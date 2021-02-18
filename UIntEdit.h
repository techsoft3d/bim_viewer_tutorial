#pragma once
#include "BaseEdit.h"

namespace FilterEdit
{
class CUIntEdit : public CBaseEdit
{
public:
	CUIntEdit ();
	virtual ~CUIntEdit ();
	unsigned int GetValue () const;

private:
	static bool m_bInitialised;
	static CRegEx m_RegEx;
};
}
