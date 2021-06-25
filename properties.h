#include "afxpropertygridctrl.h"
#include "FloatEdit.h"
#include "UnitFloatEdit.h"
#include "UIntRangeEdit.h"

namespace Property
{
	// deque doesn't specialize the bool type, so we use it to store lists of boolean values
	typedef std::deque<bool, HPS::Allocator<bool>> BoolDeque;

	CString UTF8ToCString(
		HPS::UTF8 const & utf8String)
	{
		HPS::WCharArray wchars;
		utf8String.ToWStr(wchars);
		return wchars.data();
	}

	class RootProperty
	{
	public:
		RootProperty(
			CMFCPropertyGridCtrl & ctrl)
			: ctrl(ctrl)
		{}

		virtual void Apply()
		{
			// override to apply changes
		}

	protected:
		CMFCPropertyGridCtrl & ctrl;
	};

	class BaseProperty : public CMFCPropertyGridProperty
	{
	public:
		// "Group" property - name only, no value
		BaseProperty(
			CString const & title)
			: CMFCPropertyGridProperty(title)
		{}

		// Property with a name and value
		BaseProperty(
			CString const & title,
			COleVariant const & value)
			: CMFCPropertyGridProperty(title, value, NULL, 0)
		{}

		// Show this property and any sub-properties that are enabled.
		// Disabled sub-properties will not be shown.
		virtual void SmartShow(
			bool show = true)
		{
			CMFCPropertyGridProperty::Show(show ? TRUE : FALSE);
			if (show)
			{
				int subItemCount = GetSubItemsCount();
				for (int i = 0; i < subItemCount; ++i)
				{
					auto subItem = static_cast<BaseProperty *>(GetSubItem(i));
					bool subItemShow = show;
					if (subItem->IsEnabled() == FALSE)
						subItemShow = false;
					subItem->SmartShow(subItemShow);
				}
			}
		}

		// Invoked by a child when it needs to notify it's parent it was modified
		virtual void OnChildChanged()
		{
			auto parent = static_cast<BaseProperty *>(GetParent());
			if (parent)
				parent->OnChildChanged();
		}
	};

	template <typename Type>
	class ImmutableTypeProperty : public BaseProperty
	{
	public:
		ImmutableTypeProperty(
			CString const & name,
			Type typeValue)
			: BaseProperty(name, (_variant_t)typeValue)
		{
			AllowEdit(FALSE);
		}
	};

	typedef ImmutableTypeProperty<unsigned int> ImmutableUnsignedIntProperty;
	typedef ImmutableTypeProperty<float> ImmutableFloatProperty;

	class ImmutableUTF8Property : public BaseProperty
	{
	public:
		ImmutableUTF8Property(
			CString const & name,
			HPS::UTF8 const & utf8Value)
			: BaseProperty(name, _T(""))
		{
			CString value = UTF8ToCString(utf8Value);
			SetValue(value);
			AllowEdit(FALSE);
		}
	};

	// Some values (e.g., size_t or intptr_t on 64-bit builds) won't display properly in a property grid.
	// For immutable values we want to display that are of these types, we can convert them to strings and store them as immutable strings.
	template <typename Type>
	class ImmutableTypeAsUTF8Property : public ImmutableUTF8Property
	{
	public:
		ImmutableTypeAsUTF8Property(
			CString const & name,
			Type typeValue)
			: ImmutableUTF8Property(name, std::to_wstring(typeValue).c_str())
		{}
	};

	typedef ImmutableTypeAsUTF8Property<intptr_t> ImmutableIntPtrTProperty;
	typedef ImmutableTypeAsUTF8Property<size_t> ImmutableSizeTProperty;

	class ImmutableBoolProperty : public BaseProperty
	{
	public:
		ImmutableBoolProperty(
			CString const & name,
			bool boolValue)
			: BaseProperty(name, _T(""))
		{
			// MFC will assert if AllowEdit is invoked on a property which has a value of type bool.
			// So we convert the bool to a string and make the string immutable.
			TCHAR const * boolString = boolValue ? _T("True") : _T("False");
			SetValue(boolString);
			AllowEdit(FALSE);
		}
	};

	template <
		typename Kit,
		typename ArrayValue,
		bool (Kit::*ShowArray)(std::vector<ArrayValue, HPS::Allocator<ArrayValue>> &) const
	>
	class ImmutableArraySizeProperty : public BaseProperty
	{
	private:
		typedef std::vector<ArrayValue, HPS::Allocator<ArrayValue>> ArrayType;

	public:
		ImmutableArraySizeProperty(
			CString const & name,
			CString const & sizeName,
			Kit const & kit)
			: BaseProperty(name)
		{
			ArrayType values;
			(kit.*ShowArray)(values);
			AddSubItem(new ImmutableSizeTProperty(sizeName, values.size()));
		}
	};

	class ImmutablePointProperty : public BaseProperty
	{
	public:
		ImmutablePointProperty(
			CString const & name,
			HPS::Point const & point)
			: BaseProperty(name)
		{
			AddSubItem(new ImmutableFloatProperty(_T("X"), point.x));
			AddSubItem(new ImmutableFloatProperty(_T("Y"), point.y));
			AddSubItem(new ImmutableFloatProperty(_T("Z"), point.z));
		}
	};

	class ImmutableSimpleSphereProperty : public BaseProperty
	{
	public:
		ImmutableSimpleSphereProperty(
			CString const & name,
			HPS::SimpleSphere const & sphere)
			: BaseProperty(name)
		{
			AddSubItem(new ImmutablePointProperty(_T("Center"), sphere.center));
			AddSubItem(new ImmutableFloatProperty(_T("Radius"), sphere.radius));
		}
	};

	class ImmutableSimpleCuboidProperty : public BaseProperty
	{
	public:
		ImmutableSimpleCuboidProperty(
			CString const & name,
			HPS::SimpleCuboid const & cuboid)
			: BaseProperty(name)
		{
			AddSubItem(new ImmutablePointProperty(_T("Min"), cuboid.min));
			AddSubItem(new ImmutablePointProperty(_T("Max"), cuboid.max));
		}
	};

	class BoolProperty : public BaseProperty
	{
	public:
		BoolProperty(
			CString const & name,
			bool & boolValue)
			: BaseProperty(name, (_variant_t)boolValue)
			, boolValue(boolValue)
		{}

		BOOL OnUpdateValue() override
		{
			BOOL ret = CMFCPropertyGridProperty::OnUpdateValue();
			if (ret == TRUE)
			{
				if (GetBoolFromValue())
					OnChildChanged();
			}
			return ret;
		}

	protected:
		virtual bool GetBoolFromValue()
		{
			_variant_t value = GetValue();
			if (V_VT(&value) != VT_BOOL)
			{
				ASSERT(0);
				return false;
			}

			bool newBoolValue;
			if ((BOOL)value == FALSE)
				newBoolValue = false;
			else
				newBoolValue = true;

			if (boolValue == newBoolValue)
				return false;

			boolValue = newBoolValue;
			return true;
		}

	protected:
		bool & boolValue;
	};

	// A boolean property that disables it's sibling properties when false and enables them when true.
	class ConditionalBoolProperty : public BoolProperty
	{
	public:
		ConditionalBoolProperty(
			CString const & name,
			bool & boolValue)
			: BoolProperty(name, boolValue)
		{}

		BOOL OnUpdateValue() override
		{
			BOOL ret = CMFCPropertyGridProperty::OnUpdateValue();
			if (ret == TRUE)
			{
				if (GetBoolFromValue())
				{
					EnableValidProperties();
					OnChildChanged();
				}
			}
			return ret;
		}

		virtual void EnableValidProperties()
		{
			CMFCPropertyGridProperty * parent = GetParent();
			BOOL enableValue = (boolValue ? TRUE : FALSE);
			int siblingCount = parent->GetSubItemsCount() - 1;
			for (int i = siblingCount; i > 0; --i)
			{
				CMFCPropertyGridProperty * sibling = parent->GetSubItem(i);
				sibling->Enable(enableValue);
			}
		}

		void OnChildChanged() override
		{
			auto parent = static_cast<BaseProperty *>(GetParent());
			parent->SmartShow();

			BaseProperty::OnChildChanged();
		}
	};

	template <
		typename Type,
		VARENUM VarEnumType
	>
	class EditableTypeProperty : public BaseProperty
	{
	public:
		EditableTypeProperty(
			CString const & name,
			Type & typeValue)
			: BaseProperty(name, (_variant_t)typeValue)
			, typeValue(typeValue)
		{}

		BOOL OnEndEdit() override
		{
			if (GetNewValue())
				OnChildChanged();

			return CMFCPropertyGridProperty::OnEndEdit();
		}

	private:
		bool GetNewValue()
		{
			_variant_t value = GetValue();
			if (V_VT(&value) != VarEnumType)
			{
				ASSERT(0);
				return false;
			}

			auto newTypeValue = static_cast<Type>(value);
			if (typeValue == newTypeValue)
				return false;

			typeValue = newTypeValue;
			return true;
		}

	protected:
		Type & typeValue;
	};

	typedef EditableTypeProperty<int, VT_I4> IntProperty;
	typedef EditableTypeProperty<unsigned int, VT_UINT> UnsignedIntProperty;
	typedef EditableTypeProperty<HPS::byte, VT_UI1> ByteProperty;

	class SByteProperty : public BaseProperty
	{
	public:
		SByteProperty(
			CString const & name,
			HPS::sbyte & sbyteValue)
			: BaseProperty(name, static_cast<_variant_t>(static_cast<char>(sbyteValue)))
			, sbyteValue(sbyteValue)
		{}

		BOOL OnEndEdit() override
		{
			if (GetNewValue())
				OnChildChanged();

			return CMFCPropertyGridProperty::OnEndEdit();
		}

	private:
		bool GetNewValue()
		{
			_variant_t value = GetValue();
			if (V_VT(&value) != VT_I1)
			{
				ASSERT(0);
				return false;
			}

			auto newSbyteValue = static_cast<HPS::sbyte>(static_cast<char>(value));
			if (sbyteValue == newSbyteValue)
				return false;

			sbyteValue = newSbyteValue;
			return true;
		}

	protected:
		HPS::sbyte & sbyteValue;
	};

	template <
		typename Type,
		VARENUM VarEnumType
	>
	class BaseFloatProperty : public EditableTypeProperty<Type, VarEnumType>
	{
	public:
		BaseFloatProperty(
			CString const & name,
			Type & typeValue)
			: EditableTypeProperty<Type, VarEnumType>(name, typeValue)
		{}

		CWnd * CreateInPlaceEdit(
			CRect rectEdit,
			BOOL & defaultFormat) override
		{
			FilterEdit::CFloatEdit * floatEdit = new FilterEdit::CFloatEdit();

			DWORD dwStyle = WS_VISIBLE | WS_CHILD | ES_AUTOHSCROLL;
			if (!this->m_bEnabled || !this->m_bAllowEdit)
			{
				dwStyle |= ES_READONLY;
			}
			floatEdit->Create(dwStyle, rectEdit, this->m_pWndList, AFX_PROPLIST_ID_INPLACE);

			defaultFormat = TRUE;
			return floatEdit;
		}
	};

	typedef BaseFloatProperty<float, VT_R4> FloatProperty;
	typedef BaseFloatProperty<double, VT_R8> DoubleProperty;

	class UnitFloatProperty : public FloatProperty
	{
	public:
		UnitFloatProperty(
			CString const & name,
			float & floatValue)
			: FloatProperty(name, floatValue)
		{}

		CWnd * CreateInPlaceEdit(
			CRect rectEdit,
			BOOL & defaultFormat) override
		{
			FilterEdit::CUnitFloatEdit * unitFloatEdit = new FilterEdit::CUnitFloatEdit();

			DWORD dwStyle = WS_VISIBLE | WS_CHILD | ES_AUTOHSCROLL;
			if (!m_bEnabled || !m_bAllowEdit)
			{
				dwStyle |= ES_READONLY;
			}
			unitFloatEdit->Create(dwStyle, rectEdit, m_pWndList, AFX_PROPLIST_ID_INPLACE);

			defaultFormat = TRUE;
			return unitFloatEdit;
		}
	};

	class UnsignedFloatProperty : public FloatProperty
	{
	public:
		UnsignedFloatProperty(
			CString const & name,
			float & floatValue)
			: FloatProperty(name, floatValue)
		{}

		CWnd * CreateInPlaceEdit(
			CRect rectEdit,
			BOOL & defaultFormat) override
		{
			auto floatEdit = static_cast<FilterEdit::CFloatEdit *>(FloatProperty::CreateInPlaceEdit(rectEdit, defaultFormat));
			floatEdit->SetUnsigned();
			return floatEdit;
		}
	};

	class RGBAColorProperty : public BaseProperty
	{
	public:
		RGBAColorProperty(
			CString const & name,
			HPS::RGBAColor & color)
			: BaseProperty(name)
		{
			AddSubItem(new UnitFloatProperty(_T("Red"), color.red));
			AddSubItem(new UnitFloatProperty(_T("Green"), color.green));
			AddSubItem(new UnitFloatProperty(_T("Blue"), color.blue));
			AddSubItem(new UnitFloatProperty(_T("Alpha"), color.alpha));
		}
	};

	class RGBColorProperty : public BaseProperty
	{
	public:
		RGBColorProperty(
			CString const & name,
			HPS::RGBColor & color)
			: BaseProperty(name)
		{
			AddSubItem(new UnitFloatProperty(_T("Red"), color.red));
			AddSubItem(new UnitFloatProperty(_T("Green"), color.green));
			AddSubItem(new UnitFloatProperty(_T("Blue"), color.blue));
		}
	};

	class PointProperty : public BaseProperty
	{
	public:
		PointProperty(
			CString const & name,
			HPS::Point & point)
			: BaseProperty(name)
		{
			AddSubItem(new FloatProperty(_T("X"), point.x));
			AddSubItem(new FloatProperty(_T("Y"), point.y));
			AddSubItem(new FloatProperty(_T("Z"), point.z));
		}
	};

	class VectorProperty : public BaseProperty
	{
	public:
		VectorProperty(
			CString const & name,
			HPS::Vector & vec)
			: BaseProperty(name)
		{
			AddSubItem(new FloatProperty(_T("X"), vec.x));
			AddSubItem(new FloatProperty(_T("Y"), vec.y));
			AddSubItem(new FloatProperty(_T("Z"), vec.z));
		}
	};

	class PlaneProperty : public BaseProperty
	{
	public:
		PlaneProperty(
			CString const & name,
			HPS::Plane & plane)
			: BaseProperty(name)
		{
			AddSubItem(new FloatProperty(_T("A"), plane.a));
			AddSubItem(new FloatProperty(_T("B"), plane.b));
			AddSubItem(new FloatProperty(_T("C"), plane.c));
			AddSubItem(new FloatProperty(_T("D"), plane.d));
		}
	};

	class RectangleProperty : public BaseProperty
	{
	public:
		RectangleProperty(
			CString const & name,
			HPS::Rectangle & rectangle)
			: BaseProperty(name)
		{
			AddSubItem(new FloatProperty(_T("Left"), rectangle.left));
			AddSubItem(new FloatProperty(_T("Right"), rectangle.right));
			AddSubItem(new FloatProperty(_T("Bottom"), rectangle.bottom));
			AddSubItem(new FloatProperty(_T("Top"), rectangle.top));
		}
	};

	class IntRectangleProperty : public BaseProperty
	{
	public:
		IntRectangleProperty(
			CString const & name,
			HPS::IntRectangle & rectangle)
			: BaseProperty(name)
		{
			AddSubItem(new IntProperty(_T("Left"), rectangle.left));
			AddSubItem(new IntProperty(_T("Right"), rectangle.right));
			AddSubItem(new IntProperty(_T("Bottom"), rectangle.bottom));
			AddSubItem(new IntProperty(_T("Top"), rectangle.top));
		}
	};

	class SimpleSphereProperty : public BaseProperty
	{
	public:
		SimpleSphereProperty(
			CString const & name,
			HPS::SimpleSphere & sphere)
			: BaseProperty(name)
		{
			AddSubItem(new PointProperty(_T("Center"), sphere.center));
			AddSubItem(new UnsignedFloatProperty(_T("Radius"), sphere.radius));
		}
	};

	class SimpleCuboidProperty : public BaseProperty
	{
	public:
		SimpleCuboidProperty(
			CString const & name,
			HPS::SimpleCuboid & cuboid)
			: BaseProperty(name)
		{
			AddSubItem(new PointProperty(_T("Min"), cuboid.min));
			AddSubItem(new PointProperty(_T("Max"), cuboid.max));
		}
	};

	class GlyphPointProperty : public BaseProperty
	{
	public:
		GlyphPointProperty(
			CString const & name,
			HPS::GlyphPoint & point)
			: BaseProperty(name)
		{
			AddSubItem(new SByteProperty(_T("X"), point.x));
			AddSubItem(new SByteProperty(_T("Y"), point.y));
		}
	};

	class UTF8Property : public BaseProperty
	{
	public:
		UTF8Property(
			CString const & name,
			HPS::UTF8 & utf8Value)
			: BaseProperty(name, _T(""))
			, utf8Value(utf8Value)
		{
			CString value = UTF8ToCString(this->utf8Value);
			SetValue(value);
		}

		BOOL OnEndEdit() override
		{
			if (GetUTF8FromValue())
				OnChildChanged();

			return CMFCPropertyGridProperty::OnEndEdit();
		}

	private:
		bool GetUTF8FromValue()
		{
			_variant_t value = GetValue();
			if (V_VT(&value) != VT_BSTR)
			{
				ASSERT(0);
				return false;
			}

			HPS::UTF8 newUtf8Value((TCHAR const *)(_bstr_t)value);
			if (utf8Value == newUtf8Value)
				return false;

			utf8Value = newUtf8Value;
			return true;
		}

	private:
		HPS::UTF8 & utf8Value;
	};

	template <typename EnumType>
	class BaseEnumProperty : public BaseProperty
	{
	protected:
		typedef std::vector<EnumType, HPS::Allocator<EnumType>> EnumTypeArray;

	public:
		BaseEnumProperty(
			CString const & name,
			EnumType & enumValue)
			: BaseProperty(name, _T(""))
			, enumValue(enumValue)
		{
			// You can use the drop-down list to change values, but you can't modify the text.
			AllowEdit(FALSE);
		}

		CComboBox * CreateCombo(
			CWnd * pWndParent,
			CRect rect) override
		{
			CComboBox * comboBox = BaseProperty::CreateCombo(pWndParent, rect);
			AdjustComboBoxWidth(comboBox);
			return comboBox;
		}

		BOOL OnUpdateValue() override
		{
			BOOL ret = CMFCPropertyGridProperty::OnUpdateValue();
			if (ret == TRUE)
			{
				if (GetTypeFromValue())
				{
					EnableValidProperties();
					OnChildChanged();
				}
			}
			return ret;
		}

		void OnChildChanged() override
		{
			auto parent = static_cast<BaseProperty *>(GetParent());
			parent->SmartShow();

			BaseProperty::OnChildChanged();
		}

		virtual void EnableValidProperties()
		{
			// Derived classes should override to toggle valid properties on enum value changes if necessary.
		}

	protected:
		void InitializeEnumValues(
			EnumTypeArray const & enumValues,
			HPS::UTF8Array const & enumStrings)
		{
			_enumValues = enumValues;

			ASSERT(_enumValues.size() == enumStrings.size());
			for (size_t i = 0; i < _enumValues.size(); ++i)
			{
				CString optionName = UTF8ToCString(enumStrings[i]);
				AddOption(optionName);
				if (_enumValues[i] == enumValue)
					SetValue(optionName);
			}
		}

		virtual bool GetTypeFromValue()
		{
			_variant_t value = GetValue();
			if (V_VT(&value) != VT_BSTR)
			{
				ASSERT(0);
				return false;
			}

			bool foundValidValue = false;
			EnumType newEnumValue = enumValue;
			CString valueString = (TCHAR const *)(_bstr_t)value;
			for (int i = 0; i < GetOptionCount(); ++i)
			{
				CString ithOption = GetOption(i);
				if (ithOption == valueString)
				{
					foundValidValue = true;
					newEnumValue = _enumValues[i];
					break;
				}
			}
			if (!foundValidValue)
			{
				ASSERT(0);
				return false;
			}

			if (enumValue == newEnumValue)
				return false;

			enumValue = newEnumValue;
			return true;
		}

		virtual void AdjustComboBoxWidth(
			CComboBox * comboBox)
		{
			// https://msdn.microsoft.com/en-us/library/a66b856s.aspx

			// Find the longest string in the combo box.
			CString str;
			CSize sz;
			int dx = 0;
			TEXTMETRIC tm;
			CDC * pDC = comboBox->GetDC();
			CFont * pFont = m_pWndList->GetFont();

			// Select the listbox font, save the old font
			CFont* pOldFont = pDC->SelectObject(pFont);
			// Get the text metrics for avg char width
			pDC->GetTextMetrics(&tm);

			for (int i = 0; i < GetOptionCount(); i++)
			{
				str = GetOption(i);
				sz = pDC->GetTextExtent(str);

				// Add the avg width to prevent clipping
				sz.cx += tm.tmAveCharWidth;

				if (sz.cx > dx)
					dx = sz.cx;
			}
			// Select the old font back into the DC
			pDC->SelectObject(pOldFont);
			comboBox->ReleaseDC(pDC);

			// Adjust the width for the vertical scroll bar and the left and right border.
			dx += ::GetSystemMetrics(SM_CXVSCROLL) + 2 * ::GetSystemMetrics(SM_CXEDGE);

			// Set the width of the list box so that every item is completely visible.
			comboBox->SetDroppedWidth(dx);
		}

	protected:
		EnumType & enumValue;

	private:
		EnumTypeArray _enumValues;
	};

	// A (group) property with a checkbox that sets/unsets a component of an attribute.
	class SettableProperty : public BaseProperty
	{
	public:
		SettableProperty(
			CString const & title)
			: BaseProperty(title)
			, _isSet(false)
		{
			checkBoxRect.SetRectEmpty();
		}

		void OnDrawName(
			CDC * pDC,
			CRect rect) override
		{
			checkBoxRect = rect;
			checkBoxRect.DeflateRect(1, 1);

			checkBoxRect.right = checkBoxRect.left + checkBoxRect.Height();

			OnDrawCheckBox(pDC, checkBoxRect, _isSet);

			rect.left = checkBoxRect.right;
			BaseProperty::OnDrawName(pDC, rect);
		}

		void OnClickName(
			CPoint point) override
		{
			if (IsEnabled() && checkBoxRect.PtInRect(point))
			{
				IsSet(!_isSet);
				m_pWndList->InvalidateRect(checkBoxRect);
			}
		}

		BOOL OnDblClk(
			CPoint point) override
		{
			if (IsEnabled() && checkBoxRect.PtInRect(point))
				return TRUE;

			IsSet(!_isSet);
			m_pWndList->InvalidateRect(checkBoxRect);
			return TRUE;
		}

		BOOL PushChar(
			UINT nChar) override
		{
			if (nChar == VK_SPACE)
				OnDblClk(CPoint(-1, -1));

			return TRUE;
		}

		void SmartShow(
			bool show = true) override
		{
			BaseProperty::SmartShow(show);
			ShowEnabledChildren();
		}

		void OnChildChanged() override
		{
			if (_isSet)
				Set();
			else
				Unset();
			BaseProperty::OnChildChanged();
		}

		void IsSet(
			bool isSet)
		{
			_isSet = isSet;
			ShowEnabledChildren();
			OnChildChanged();
		}

		bool IsSet() const
		{
			return _isSet;
		}

	protected:
		virtual void Set() = 0;
		virtual void Unset() = 0;

		virtual void ShowEnabledChildren()
		{
			int childCount = GetSubItemsCount();
			for (int i = childCount - 1; i >= 0; --i)
			{
				auto child = static_cast<BaseProperty *>(GetSubItem(i));
				bool show = _isSet;

				if (child->IsEnabled() == FALSE)
				{
					// We only want to show children that are enabled.
					show = false;
				}

				child->SmartShow(show);
			}
		}

	private:
		void OnDrawCheckBox(
			CDC * pDC,
			CRect rect,
			BOOL isChecked)
		{
			COLORREF clrTextOld = pDC->GetTextColor();

			CMFCVisualManager::GetInstance()->OnDrawCheckBox(pDC, rect, FALSE, isChecked, IsEnabled());

			pDC->SetTextColor(clrTextOld);
		}

	private:
		CRect checkBoxRect;
		bool _isSet;
	};

	class ArraySizeProperty : public BaseProperty
	{
	public:
		ArraySizeProperty(
			CString const & name,
			unsigned int & arraySize,
			unsigned int minValue = 1,
			unsigned int maxValue = std::numeric_limits<int>::max())	// Intentionally INT_MAX not UINT_MAX
			: BaseProperty(name, (_variant_t)arraySize)
			, arraySize(arraySize)
			, minValue(minValue)
			, maxValue(maxValue)
		{
			// Even though the MFC spin control is supposed to work with UINTs, it stores the bounds internally as INTs.
			// As such, the "unbounded" upper limit should be INT_MAX (otherwise the spin control won't work properly).
			this->maxValue = HPS::Clamp(maxValue, maxValue, static_cast<unsigned int>(std::numeric_limits<int>::max()));
			EnableSpinControl(TRUE, this->minValue, this->maxValue);
		}

		CWnd * CreateInPlaceEdit(
			CRect rectEdit,
			BOOL & defaultFormat) override
		{
			FilterEdit::CUIntRangeEdit * uintRangeEdit = new FilterEdit::CUIntRangeEdit(minValue, maxValue);

			DWORD dwStyle = WS_VISIBLE | WS_CHILD | ES_AUTOHSCROLL;
			if (!m_bEnabled || !m_bAllowEdit)
			{
				dwStyle |= ES_READONLY;
			}
			uintRangeEdit->Create(dwStyle, rectEdit, m_pWndList, AFX_PROPLIST_ID_INPLACE);

			defaultFormat = TRUE;
			return uintRangeEdit;
		}

		BOOL OnEndEdit() override
		{
			if (GetSizeFromValue())
				OnChildChanged();

			return CMFCPropertyGridProperty::OnEndEdit();
		}

	private:
		bool GetSizeFromValue()
		{
			_variant_t value = GetValue();
			if (V_VT(&value) != VT_UINT)
			{
				ASSERT(0);
				return false;
			}

			auto newArraySize = static_cast<unsigned int>(value);
			if (newArraySize == 0)
				return false;
			if (arraySize == newArraySize)
				return false;

			arraySize = newArraySize;
			return true;
		}

	private:
		unsigned int & arraySize;
		unsigned int minValue;
		unsigned int maxValue;
	};

	// An array property that *can't* be set/unset
	class ArrayProperty : public BaseProperty
	{
	private:
		enum PropertyTypeIndex
		{
			CountPropertyIndex = 0,
			FirstItemIndex,
		};

	public:
		ArrayProperty(
			CString const & name)
			: BaseProperty(name)
		{}

	protected:
		virtual void ResizeArrays() = 0;

		virtual bool AddOrDeleteItems(
			unsigned int newItemCount,
			unsigned int oldItemCount)
		{
			if (newItemCount == oldItemCount)
				return false;

			ResizeArrays();

			if (newItemCount < oldItemCount)
			{
				unsigned int startIndex = FirstItemIndex + newItemCount;
				unsigned int endIndex = FirstItemIndex + oldItemCount - 1;
				DeleteItems(startIndex, endIndex);
			}
			else
			{
				if (oldItemCount > 0)
				{
					// Delete the old items because they may have references bound to invalid addresses if
					// the underlying arrays had to be re-allocated.
					unsigned int startIndex = FirstItemIndex;
					unsigned int endIndex = FirstItemIndex + oldItemCount - 1;
					DeleteItems(startIndex, endIndex);
				}

				AddItems();

				CMFCPropertyGridCtrl * owningCtrl = m_pWndList;
				if (owningCtrl)
				{
					// We've added one or more new sub-properties, but this property has already been added to our control.
					// So we need to make sure those new sub-properties get the pointer to the owning control so they can get
					// handled correctly by MFC.
					SetOwnerList(owningCtrl);

					// TODO: better way to do this?
					// MFC won't always show children correctly when we add new child properties and invoke Show.
					// To work around this, we collapse and expand this property.
					Expand(FALSE);
					Expand(TRUE);
				}
			}

			return true;
		}

		virtual void AddItems() = 0;

		virtual void DeleteItems(
			unsigned int startIndex,
			unsigned int endIndex)
		{
			CMFCPropertyGridCtrl * owningCtrl = m_pWndList;
			for (unsigned int i = endIndex; i >= startIndex; --i)
			{
				auto item = GetSubItem(i);
				owningCtrl->DeleteProperty(item);
			}
		}
	};

	// An array property that *can* be set/unset
	class SettableArrayProperty : public SettableProperty
	{
	public:
		SettableArrayProperty(
			CString const & name,
			int firstItemIndex = 1)
			: SettableProperty(name)
			, firstItemIndex(firstItemIndex)
		{}

	protected:
		virtual void ResizeArrays() = 0;

		virtual bool AddOrDeleteItems(
			unsigned int newItemCount,
			unsigned int oldItemCount)
		{
			if (newItemCount == oldItemCount)
				return false;

			ResizeArrays();

			if (newItemCount < oldItemCount)
			{
				unsigned int startIndex = firstItemIndex + newItemCount;
				unsigned int endIndex = firstItemIndex + oldItemCount - 1;
				DeleteItems(startIndex, endIndex);
			}
			else
			{
				if (oldItemCount > 0)
				{
					// Delete the old items because they may have references bound to invalid addresses if
					// the underlying arrays had to be re-allocated.
					unsigned int startIndex = firstItemIndex;
					unsigned int endIndex = firstItemIndex + oldItemCount - 1;
					DeleteItems(startIndex, endIndex);
				}

				AddItems();

				CMFCPropertyGridCtrl * owningCtrl = m_pWndList;
				if (owningCtrl)
				{
					// We've added one or more new sub-properties, but this property has already been added to our control.
					// So we need to make sure those new sub-properties get the pointer to the owning control so they can get
					// handled correctly by MFC.
					SetOwnerList(owningCtrl);

					ShowEnabledChildren();

					// TODO: better way to do this?
					// MFC won't always show children correctly when we add new child properties and invoke Show.
					// To work around this, we collapse and expand this property.
					Expand(FALSE);
					Expand(TRUE);
				}
			}

			return true;
		}

		virtual void AddItems() = 0;

		virtual void DeleteItems(
			unsigned int startIndex,
			unsigned int endIndex)
		{
			CMFCPropertyGridCtrl * owningCtrl = m_pWndList;
			for (unsigned int i = endIndex; i >= startIndex; --i)
			{
				auto item = GetSubItem(i);
				owningCtrl->DeleteProperty(item);
			}
		}

	protected:
		int firstItemIndex;
	};

	class WindowDriverProperty : public BaseEnumProperty<HPS::Window::Driver>
	{
	public:
		WindowDriverProperty(
			HPS::Window::Driver & enumValue,
			CString const & name = _T("Driver"))
			: BaseEnumProperty(name, enumValue)
		{
			EnumTypeArray enumValues(7); HPS::UTF8Array enumStrings(7);
			enumValues[0] = HPS::Window::Driver::Default3D; enumStrings[0] = "Default3D";
			enumValues[1] = HPS::Window::Driver::OpenGL; enumStrings[1] = "OpenGL";
			enumValues[2] = HPS::Window::Driver::OpenGL2; enumStrings[2] = "OpenGL2";
			enumValues[3] = HPS::Window::Driver::DirectX11; enumStrings[3] = "DirectX11";
			enumValues[4] = HPS::Window::Driver::OpenGL1Deprecated; enumStrings[4] = "OpenGL1Deprecated";
			enumValues[5] = HPS::Window::Driver::Vulkan; enumStrings[5] = "Vulkan";
			enumValues[6] = HPS::Window::Driver::OpenGL2Mesa; enumStrings[6] = "OpenGL2Mesa";
			InitializeEnumValues(enumValues, enumStrings);
		}
	};

	class WindowUpdateTypeProperty : public BaseEnumProperty<HPS::Window::UpdateType>
	{
	public:
		WindowUpdateTypeProperty(
			HPS::Window::UpdateType & enumValue,
			CString const & name = _T("UpdateType"))
			: BaseEnumProperty(name, enumValue)
		{
			EnumTypeArray enumValues(5); HPS::UTF8Array enumStrings(5);
			enumValues[0] = HPS::Window::UpdateType::Default; enumStrings[0] = "Default";
			enumValues[1] = HPS::Window::UpdateType::Complete; enumStrings[1] = "Complete";
			enumValues[2] = HPS::Window::UpdateType::Refresh; enumStrings[2] = "Refresh";
			enumValues[3] = HPS::Window::UpdateType::CompileOnly; enumStrings[3] = "CompileOnly";
			enumValues[4] = HPS::Window::UpdateType::Exhaustive; enumStrings[4] = "Exhaustive";
			InitializeEnumValues(enumValues, enumStrings);
		}
	};

	class SubwindowTypeProperty : public BaseEnumProperty<HPS::Subwindow::Type>
	{
	public:
		SubwindowTypeProperty(
			HPS::Subwindow::Type & enumValue,
			CString const & name = _T("Type"))
			: BaseEnumProperty(name, enumValue)
		{
			EnumTypeArray enumValues(2); HPS::UTF8Array enumStrings(2);
			enumValues[0] = HPS::Subwindow::Type::Standard; enumStrings[0] = "Standard";
			enumValues[1] = HPS::Subwindow::Type::Lightweight; enumStrings[1] = "Lightweight";
			InitializeEnumValues(enumValues, enumStrings);
		}
	};

	class SubwindowBorderProperty : public BaseEnumProperty<HPS::Subwindow::Border>
	{
	public:
		SubwindowBorderProperty(
			HPS::Subwindow::Border & enumValue,
			CString const & name = _T("Border"))
			: BaseEnumProperty(name, enumValue)
		{
			EnumTypeArray enumValues(5); HPS::UTF8Array enumStrings(5);
			enumValues[0] = HPS::Subwindow::Border::None; enumStrings[0] = "None";
			enumValues[1] = HPS::Subwindow::Border::Inset; enumStrings[1] = "Inset";
			enumValues[2] = HPS::Subwindow::Border::InsetBold; enumStrings[2] = "InsetBold";
			enumValues[3] = HPS::Subwindow::Border::Overlay; enumStrings[3] = "Overlay";
			enumValues[4] = HPS::Subwindow::Border::OverlayBold; enumStrings[4] = "OverlayBold";
			InitializeEnumValues(enumValues, enumStrings);
		}
	};

	class SubwindowRenderingAlgorithmProperty : public BaseEnumProperty<HPS::Subwindow::RenderingAlgorithm>
	{
	public:
		SubwindowRenderingAlgorithmProperty(
			HPS::Subwindow::RenderingAlgorithm & enumValue,
			CString const & name = _T("RenderingAlgorithm"))
			: BaseEnumProperty(name, enumValue)
		{
			EnumTypeArray enumValues(4); HPS::UTF8Array enumStrings(4);
			enumValues[0] = HPS::Subwindow::RenderingAlgorithm::ZBuffer; enumStrings[0] = "ZBuffer";
			enumValues[1] = HPS::Subwindow::RenderingAlgorithm::HiddenLine; enumStrings[1] = "HiddenLine";
			enumValues[2] = HPS::Subwindow::RenderingAlgorithm::FastHiddenLine; enumStrings[2] = "FastHiddenLine";
			enumValues[3] = HPS::Subwindow::RenderingAlgorithm::Priority; enumStrings[3] = "Priority";
			InitializeEnumValues(enumValues, enumStrings);
		}
	};

	class ShellComponentProperty : public BaseEnumProperty<HPS::Shell::Component>
	{
	public:
		ShellComponentProperty(
			HPS::Shell::Component & enumValue,
			CString const & name = _T("Component"))
			: BaseEnumProperty(name, enumValue)
		{
			EnumTypeArray enumValues(3); HPS::UTF8Array enumStrings(3);
			enumValues[0] = HPS::Shell::Component::Faces; enumStrings[0] = "Faces";
			enumValues[1] = HPS::Shell::Component::Edges; enumStrings[1] = "Edges";
			enumValues[2] = HPS::Shell::Component::Vertices; enumStrings[2] = "Vertices";
			InitializeEnumValues(enumValues, enumStrings);
		}
	};

	class ShellHandednessOptimizationProperty : public BaseEnumProperty<HPS::Shell::HandednessOptimization>
	{
	public:
		ShellHandednessOptimizationProperty(
			HPS::Shell::HandednessOptimization & enumValue,
			CString const & name = _T("HandednessOptimization"))
			: BaseEnumProperty(name, enumValue)
		{
			EnumTypeArray enumValues(3); HPS::UTF8Array enumStrings(3);
			enumValues[0] = HPS::Shell::HandednessOptimization::None; enumStrings[0] = "None";
			enumValues[1] = HPS::Shell::HandednessOptimization::Fix; enumStrings[1] = "Fix";
			enumValues[2] = HPS::Shell::HandednessOptimization::Reverse; enumStrings[2] = "Reverse";
			InitializeEnumValues(enumValues, enumStrings);
		}
	};

	class MeshComponentProperty : public BaseEnumProperty<HPS::Mesh::Component>
	{
	public:
		MeshComponentProperty(
			HPS::Mesh::Component & enumValue,
			CString const & name = _T("Component"))
			: BaseEnumProperty(name, enumValue)
		{
			EnumTypeArray enumValues(3); HPS::UTF8Array enumStrings(3);
			enumValues[0] = HPS::Mesh::Component::Faces; enumStrings[0] = "Faces";
			enumValues[1] = HPS::Mesh::Component::Edges; enumStrings[1] = "Edges";
			enumValues[2] = HPS::Mesh::Component::Vertices; enumStrings[2] = "Vertices";
			InitializeEnumValues(enumValues, enumStrings);
		}
	};

	class InfiniteLineTypeProperty : public BaseEnumProperty<HPS::InfiniteLine::Type>
	{
	public:
		InfiniteLineTypeProperty(
			HPS::InfiniteLine::Type & enumValue,
			CString const & name = _T("Type"))
			: BaseEnumProperty(name, enumValue)
		{
			EnumTypeArray enumValues(2); HPS::UTF8Array enumStrings(2);
			enumValues[0] = HPS::InfiniteLine::Type::Line; enumStrings[0] = "Line";
			enumValues[1] = HPS::InfiniteLine::Type::Ray; enumStrings[1] = "Ray";
			InitializeEnumValues(enumValues, enumStrings);
		}
	};

	class TrimTypeProperty : public BaseEnumProperty<HPS::Trim::Type>
	{
	public:
		TrimTypeProperty(
			HPS::Trim::Type & enumValue,
			CString const & name = _T("Type"))
			: BaseEnumProperty(name, enumValue)
		{
			EnumTypeArray enumValues(2); HPS::UTF8Array enumStrings(2);
			enumValues[0] = HPS::Trim::Type::Line; enumStrings[0] = "Line";
			enumValues[1] = HPS::Trim::Type::Curve; enumStrings[1] = "Curve";
			InitializeEnumValues(enumValues, enumStrings);
		}
	};

	class TrimOperationProperty : public BaseEnumProperty<HPS::Trim::Operation>
	{
	public:
		TrimOperationProperty(
			HPS::Trim::Operation & enumValue,
			CString const & name = _T("Operation"))
			: BaseEnumProperty(name, enumValue)
		{
			EnumTypeArray enumValues(2); HPS::UTF8Array enumStrings(2);
			enumValues[0] = HPS::Trim::Operation::Keep; enumStrings[0] = "Keep";
			enumValues[1] = HPS::Trim::Operation::Remove; enumStrings[1] = "Remove";
			InitializeEnumValues(enumValues, enumStrings);
		}
	};

	class SpotlightOuterConeUnitsProperty : public BaseEnumProperty<HPS::Spotlight::OuterConeUnits>
	{
	public:
		SpotlightOuterConeUnitsProperty(
			HPS::Spotlight::OuterConeUnits & enumValue,
			CString const & name = _T("OuterConeUnits"))
			: BaseEnumProperty(name, enumValue)
		{
			EnumTypeArray enumValues(2); HPS::UTF8Array enumStrings(2);
			enumValues[0] = HPS::Spotlight::OuterConeUnits::Degrees; enumStrings[0] = "Degrees";
			enumValues[1] = HPS::Spotlight::OuterConeUnits::FieldRadius; enumStrings[1] = "FieldRadius";
			InitializeEnumValues(enumValues, enumStrings);
		}
	};

	class SpotlightInnerConeUnitsProperty : public BaseEnumProperty<HPS::Spotlight::InnerConeUnits>
	{
	public:
		SpotlightInnerConeUnitsProperty(
			HPS::Spotlight::InnerConeUnits & enumValue,
			CString const & name = _T("InnerConeUnits"))
			: BaseEnumProperty(name, enumValue)
		{
			EnumTypeArray enumValues(3); HPS::UTF8Array enumStrings(3);
			enumValues[0] = HPS::Spotlight::InnerConeUnits::Degrees; enumStrings[0] = "Degrees";
			enumValues[1] = HPS::Spotlight::InnerConeUnits::FieldRadius; enumStrings[1] = "FieldRadius";
			enumValues[2] = HPS::Spotlight::InnerConeUnits::Percent; enumStrings[2] = "Percent";
			InitializeEnumValues(enumValues, enumStrings);
		}
	};

	class CylinderComponentProperty : public BaseEnumProperty<HPS::Cylinder::Component>
	{
	public:
		CylinderComponentProperty(
			HPS::Cylinder::Component & enumValue,
			CString const & name = _T("Component"))
			: BaseEnumProperty(name, enumValue)
		{
			EnumTypeArray enumValues(2); HPS::UTF8Array enumStrings(2);
			enumValues[0] = HPS::Cylinder::Component::Faces; enumStrings[0] = "Faces";
			enumValues[1] = HPS::Cylinder::Component::Edges; enumStrings[1] = "Edges";
			InitializeEnumValues(enumValues, enumStrings);
		}
	};

	class CylinderCappingProperty : public BaseEnumProperty<HPS::Cylinder::Capping>
	{
	public:
		CylinderCappingProperty(
			HPS::Cylinder::Capping & enumValue,
			CString const & name = _T("Capping"))
			: BaseEnumProperty(name, enumValue)
		{
			EnumTypeArray enumValues(4); HPS::UTF8Array enumStrings(4);
			enumValues[0] = HPS::Cylinder::Capping::None; enumStrings[0] = "None";
			enumValues[1] = HPS::Cylinder::Capping::First; enumStrings[1] = "First";
			enumValues[2] = HPS::Cylinder::Capping::Last; enumStrings[2] = "Last";
			enumValues[3] = HPS::Cylinder::Capping::Both; enumStrings[3] = "Both";
			InitializeEnumValues(enumValues, enumStrings);
		}
	};

	class CylinderOrientationProperty : public BaseEnumProperty<HPS::Cylinder::Orientation>
	{
	public:
		CylinderOrientationProperty(
			HPS::Cylinder::Orientation & enumValue,
			CString const & name = _T("Orientation"))
			: BaseEnumProperty(name, enumValue)
		{
			EnumTypeArray enumValues(8); HPS::UTF8Array enumStrings(8);
			enumValues[0] = HPS::Cylinder::Orientation::Default; enumStrings[0] = "Default";
			enumValues[1] = HPS::Cylinder::Orientation::DefaultRadii; enumStrings[1] = "DefaultRadii";
			enumValues[2] = HPS::Cylinder::Orientation::InvertRadii; enumStrings[2] = "InvertRadii";
			enumValues[3] = HPS::Cylinder::Orientation::InvertRadiiOnly; enumStrings[3] = "InvertRadiiOnly";
			enumValues[4] = HPS::Cylinder::Orientation::DefaultColors; enumStrings[4] = "DefaultColors";
			enumValues[5] = HPS::Cylinder::Orientation::InvertColors; enumStrings[5] = "InvertColors";
			enumValues[6] = HPS::Cylinder::Orientation::InvertColorsOnly; enumStrings[6] = "InvertColorsOnly";
			enumValues[7] = HPS::Cylinder::Orientation::InvertAll; enumStrings[7] = "InvertAll";
			InitializeEnumValues(enumValues, enumStrings);
		}
	};

	class HighlightSearchScopeProperty : public BaseEnumProperty<HPS::HighlightSearch::Scope>
	{
	public:
		HighlightSearchScopeProperty(
			HPS::HighlightSearch::Scope & enumValue,
			CString const & name = _T("Scope"))
			: BaseEnumProperty(name, enumValue)
		{
			EnumTypeArray enumValues(3); HPS::UTF8Array enumStrings(3);
			enumValues[0] = HPS::HighlightSearch::Scope::AtOrAbovePath; enumStrings[0] = "AtOrAbovePath";
			enumValues[1] = HPS::HighlightSearch::Scope::AtOrBelowPath; enumStrings[1] = "AtOrBelowPath";
			enumValues[2] = HPS::HighlightSearch::Scope::ExactPath; enumStrings[2] = "ExactPath";
			InitializeEnumValues(enumValues, enumStrings);
		}
	};

	class MaterialTextureParameterizationProperty : public BaseEnumProperty<HPS::Material::Texture::Parameterization>
	{
	public:
		MaterialTextureParameterizationProperty(
			HPS::Material::Texture::Parameterization & enumValue,
			CString const & name = _T("Parameterization"))
			: BaseEnumProperty(name, enumValue)
		{
			EnumTypeArray enumValues(9); HPS::UTF8Array enumStrings(9);
			enumValues[0] = HPS::Material::Texture::Parameterization::Cylinder; enumStrings[0] = "Cylinder";
			enumValues[1] = HPS::Material::Texture::Parameterization::PhysicalReflection; enumStrings[1] = "PhysicalReflection";
			enumValues[2] = HPS::Material::Texture::Parameterization::Object; enumStrings[2] = "Object";
			enumValues[3] = HPS::Material::Texture::Parameterization::NaturalUV; enumStrings[3] = "NaturalUV";
			enumValues[4] = HPS::Material::Texture::Parameterization::ReflectionVector; enumStrings[4] = "ReflectionVector";
			enumValues[5] = HPS::Material::Texture::Parameterization::SurfaceNormal; enumStrings[5] = "SurfaceNormal";
			enumValues[6] = HPS::Material::Texture::Parameterization::Sphere; enumStrings[6] = "Sphere";
			enumValues[7] = HPS::Material::Texture::Parameterization::UV; enumStrings[7] = "UV";
			enumValues[8] = HPS::Material::Texture::Parameterization::World; enumStrings[8] = "World";
			InitializeEnumValues(enumValues, enumStrings);
		}
	};

	class MaterialTextureTilingProperty : public BaseEnumProperty<HPS::Material::Texture::Tiling>
	{
	public:
		MaterialTextureTilingProperty(
			HPS::Material::Texture::Tiling & enumValue,
			CString const & name = _T("Tiling"))
			: BaseEnumProperty(name, enumValue)
		{
			EnumTypeArray enumValues(4); HPS::UTF8Array enumStrings(4);
			enumValues[0] = HPS::Material::Texture::Tiling::Clamp; enumStrings[0] = "Clamp";
			enumValues[1] = HPS::Material::Texture::Tiling::Repeat; enumStrings[1] = "Repeat";
			enumValues[2] = HPS::Material::Texture::Tiling::Reflect; enumStrings[2] = "Reflect";
			enumValues[3] = HPS::Material::Texture::Tiling::Trim; enumStrings[3] = "Trim";
			InitializeEnumValues(enumValues, enumStrings);
		}
	};

	class MaterialTextureInterpolationProperty : public BaseEnumProperty<HPS::Material::Texture::Interpolation>
	{
	public:
		MaterialTextureInterpolationProperty(
			HPS::Material::Texture::Interpolation & enumValue,
			CString const & name = _T("Interpolation"))
			: BaseEnumProperty(name, enumValue)
		{
			EnumTypeArray enumValues(2); HPS::UTF8Array enumStrings(2);
			enumValues[0] = HPS::Material::Texture::Interpolation::None; enumStrings[0] = "None";
			enumValues[1] = HPS::Material::Texture::Interpolation::Bilinear; enumStrings[1] = "Bilinear";
			InitializeEnumValues(enumValues, enumStrings);
		}
	};

	class MaterialTextureDecimationProperty : public BaseEnumProperty<HPS::Material::Texture::Decimation>
	{
	public:
		MaterialTextureDecimationProperty(
			HPS::Material::Texture::Decimation & enumValue,
			CString const & name = _T("Decimation"))
			: BaseEnumProperty(name, enumValue)
		{
			EnumTypeArray enumValues(3); HPS::UTF8Array enumStrings(3);
			enumValues[0] = HPS::Material::Texture::Decimation::None; enumStrings[0] = "None";
			enumValues[1] = HPS::Material::Texture::Decimation::Anisotropic; enumStrings[1] = "Anisotropic";
			enumValues[2] = HPS::Material::Texture::Decimation::Mipmap; enumStrings[2] = "Mipmap";
			InitializeEnumValues(enumValues, enumStrings);
		}
	};

	class MaterialTextureChannelMappingProperty : public BaseEnumProperty<HPS::Material::Texture::ChannelMapping>
	{
	public:
		MaterialTextureChannelMappingProperty(
			HPS::Material::Texture::ChannelMapping & enumValue,
			CString const & name = _T("ChannelMapping"))
			: BaseEnumProperty(name, enumValue)
		{
			EnumTypeArray enumValues(7); HPS::UTF8Array enumStrings(7);
			enumValues[0] = HPS::Material::Texture::ChannelMapping::Red; enumStrings[0] = "Red";
			enumValues[1] = HPS::Material::Texture::ChannelMapping::Green; enumStrings[1] = "Green";
			enumValues[2] = HPS::Material::Texture::ChannelMapping::Blue; enumStrings[2] = "Blue";
			enumValues[3] = HPS::Material::Texture::ChannelMapping::Alpha; enumStrings[3] = "Alpha";
			enumValues[4] = HPS::Material::Texture::ChannelMapping::Zero; enumStrings[4] = "Zero";
			enumValues[5] = HPS::Material::Texture::ChannelMapping::One; enumStrings[5] = "One";
			enumValues[6] = HPS::Material::Texture::ChannelMapping::Luminance; enumStrings[6] = "Luminance";
			InitializeEnumValues(enumValues, enumStrings);
		}
	};

	class PostProcessEffectsAmbientOcclusionQualityProperty : public BaseEnumProperty<HPS::PostProcessEffects::AmbientOcclusion::Quality>
	{
	public:
		PostProcessEffectsAmbientOcclusionQualityProperty(
			HPS::PostProcessEffects::AmbientOcclusion::Quality & enumValue,
			CString const & name = _T("Quality"))
			: BaseEnumProperty(name, enumValue)
		{
			EnumTypeArray enumValues(2); HPS::UTF8Array enumStrings(2);
			enumValues[0] = HPS::PostProcessEffects::AmbientOcclusion::Quality::Fastest; enumStrings[0] = "Fastest";
			enumValues[1] = HPS::PostProcessEffects::AmbientOcclusion::Quality::Nicest; enumStrings[1] = "Nicest";
			InitializeEnumValues(enumValues, enumStrings);
		}
	};

	class PostProcessEffectsBloomShapeProperty : public BaseEnumProperty<HPS::PostProcessEffects::Bloom::Shape>
	{
	public:
		PostProcessEffectsBloomShapeProperty(
			HPS::PostProcessEffects::Bloom::Shape & enumValue,
			CString const & name = _T("Shape"))
			: BaseEnumProperty(name, enumValue)
		{
			EnumTypeArray enumValues(2); HPS::UTF8Array enumStrings(2);
			enumValues[0] = HPS::PostProcessEffects::Bloom::Shape::Star; enumStrings[0] = "Star";
			enumValues[1] = HPS::PostProcessEffects::Bloom::Shape::Radial; enumStrings[1] = "Radial";
			InitializeEnumValues(enumValues, enumStrings);
		}
	};

	class PerformanceDisplayListsProperty : public BaseEnumProperty<HPS::Performance::DisplayLists>
	{
	public:
		PerformanceDisplayListsProperty(
			HPS::Performance::DisplayLists & enumValue,
			CString const & name = _T("DisplayLists"))
			: BaseEnumProperty(name, enumValue)
		{
			EnumTypeArray enumValues(3); HPS::UTF8Array enumStrings(3);
			enumValues[0] = HPS::Performance::DisplayLists::None; enumStrings[0] = "None";
			enumValues[1] = HPS::Performance::DisplayLists::Geometry; enumStrings[1] = "Geometry";
			enumValues[2] = HPS::Performance::DisplayLists::Segment; enumStrings[2] = "Segment";
			InitializeEnumValues(enumValues, enumStrings);
		}
	};

	class PerformanceStaticModelProperty : public BaseEnumProperty<HPS::Performance::StaticModel>
	{
	public:
		PerformanceStaticModelProperty(
			HPS::Performance::StaticModel & enumValue,
			CString const & name = _T("StaticModel"))
			: BaseEnumProperty(name, enumValue)
		{
			EnumTypeArray enumValues(3); HPS::UTF8Array enumStrings(3);
			enumValues[0] = HPS::Performance::StaticModel::None; enumStrings[0] = "None";
			enumValues[1] = HPS::Performance::StaticModel::Attribute; enumStrings[1] = "Attribute";
			enumValues[2] = HPS::Performance::StaticModel::AttributeSpatial; enumStrings[2] = "AttributeSpatial";
			InitializeEnumValues(enumValues, enumStrings);
		}
	};

	class PerformanceStaticConditionsProperty : public BaseEnumProperty<HPS::Performance::StaticConditions>
	{
	public:
		PerformanceStaticConditionsProperty(
			HPS::Performance::StaticConditions & enumValue,
			CString const & name = _T("StaticConditions"))
			: BaseEnumProperty(name, enumValue)
		{
			EnumTypeArray enumValues(2); HPS::UTF8Array enumStrings(2);
			enumValues[0] = HPS::Performance::StaticConditions::Independent; enumStrings[0] = "Independent";
			enumValues[1] = HPS::Performance::StaticConditions::Single; enumStrings[1] = "Single";
			InitializeEnumValues(enumValues, enumStrings);
		}
	};

	class AttributeLockTypeProperty : public BaseEnumProperty<HPS::AttributeLock::Type>
	{
	public:
		AttributeLockTypeProperty(
			HPS::AttributeLock::Type & enumValue,
			CString const & name = _T("Type"))
			: BaseEnumProperty(name, enumValue)
		{
			EnumTypeArray enumValues(115); HPS::UTF8Array enumStrings(115);
			enumValues[0] = HPS::AttributeLock::Type::Everything; enumStrings[0] = "Everything";
			enumValues[1] = HPS::AttributeLock::Type::Visibility; enumStrings[1] = "Visibility";
			enumValues[2] = HPS::AttributeLock::Type::VisibilityCuttingSections; enumStrings[2] = "VisibilityCuttingSections";
			enumValues[3] = HPS::AttributeLock::Type::VisibilityCutEdges; enumStrings[3] = "VisibilityCutEdges";
			enumValues[4] = HPS::AttributeLock::Type::VisibilityCutFaces; enumStrings[4] = "VisibilityCutFaces";
			enumValues[5] = HPS::AttributeLock::Type::VisibilityWindows; enumStrings[5] = "VisibilityWindows";
			enumValues[6] = HPS::AttributeLock::Type::VisibilityText; enumStrings[6] = "VisibilityText";
			enumValues[7] = HPS::AttributeLock::Type::VisibilityLines; enumStrings[7] = "VisibilityLines";
			enumValues[8] = HPS::AttributeLock::Type::VisibilityEdgeLights; enumStrings[8] = "VisibilityEdgeLights";
			enumValues[9] = HPS::AttributeLock::Type::VisibilityMarkerLights; enumStrings[9] = "VisibilityMarkerLights";
			enumValues[10] = HPS::AttributeLock::Type::VisibilityFaceLights; enumStrings[10] = "VisibilityFaceLights";
			enumValues[11] = HPS::AttributeLock::Type::VisibilityGenericEdges; enumStrings[11] = "VisibilityGenericEdges";
			enumValues[12] = HPS::AttributeLock::Type::VisibilityHardEdges; enumStrings[12] = "VisibilityHardEdges";
			enumValues[13] = HPS::AttributeLock::Type::VisibilityAdjacentEdges; enumStrings[13] = "VisibilityAdjacentEdges";
			enumValues[14] = HPS::AttributeLock::Type::VisibilityInteriorSilhouetteEdges; enumStrings[14] = "VisibilityInteriorSilhouetteEdges";
			enumValues[15] = HPS::AttributeLock::Type::VisibilityShadowEmitting; enumStrings[15] = "VisibilityShadowEmitting";
			enumValues[16] = HPS::AttributeLock::Type::VisibilityShadowReceiving; enumStrings[16] = "VisibilityShadowReceiving";
			enumValues[17] = HPS::AttributeLock::Type::VisibilityShadowCasting; enumStrings[17] = "VisibilityShadowCasting";
			enumValues[18] = HPS::AttributeLock::Type::VisibilityMarkers; enumStrings[18] = "VisibilityMarkers";
			enumValues[19] = HPS::AttributeLock::Type::VisibilityVertices; enumStrings[19] = "VisibilityVertices";
			enumValues[20] = HPS::AttributeLock::Type::VisibilityFaces; enumStrings[20] = "VisibilityFaces";
			enumValues[21] = HPS::AttributeLock::Type::VisibilityPerimeterEdges; enumStrings[21] = "VisibilityPerimeterEdges";
			enumValues[22] = HPS::AttributeLock::Type::VisibilityNonCulledEdges; enumStrings[22] = "VisibilityNonCulledEdges";
			enumValues[23] = HPS::AttributeLock::Type::VisibilityMeshQuadEdges; enumStrings[23] = "VisibilityMeshQuadEdges";
			enumValues[24] = HPS::AttributeLock::Type::VisibilityCutGeometry; enumStrings[24] = "VisibilityCutGeometry";
			enumValues[25] = HPS::AttributeLock::Type::VisibilityEdges; enumStrings[25] = "VisibilityEdges";
			enumValues[26] = HPS::AttributeLock::Type::VisibilityGeometry; enumStrings[26] = "VisibilityGeometry";
			enumValues[27] = HPS::AttributeLock::Type::VisibilityLights; enumStrings[27] = "VisibilityLights";
			enumValues[28] = HPS::AttributeLock::Type::VisibilityShadows; enumStrings[28] = "VisibilityShadows";
			enumValues[29] = HPS::AttributeLock::Type::Material; enumStrings[29] = "Material";
			enumValues[30] = HPS::AttributeLock::Type::MaterialGeometry; enumStrings[30] = "MaterialGeometry";
			enumValues[31] = HPS::AttributeLock::Type::MaterialCutGeometry; enumStrings[31] = "MaterialCutGeometry";
			enumValues[32] = HPS::AttributeLock::Type::MaterialAmbientLightUpColor; enumStrings[32] = "MaterialAmbientLightUpColor";
			enumValues[33] = HPS::AttributeLock::Type::MaterialAmbientLightDownColor; enumStrings[33] = "MaterialAmbientLightDownColor";
			enumValues[34] = HPS::AttributeLock::Type::MaterialAmbientLightColor; enumStrings[34] = "MaterialAmbientLightColor";
			enumValues[35] = HPS::AttributeLock::Type::MaterialWindowColor; enumStrings[35] = "MaterialWindowColor";
			enumValues[36] = HPS::AttributeLock::Type::MaterialWindowContrastColor; enumStrings[36] = "MaterialWindowContrastColor";
			enumValues[37] = HPS::AttributeLock::Type::MaterialLightColor; enumStrings[37] = "MaterialLightColor";
			enumValues[38] = HPS::AttributeLock::Type::MaterialLineColor; enumStrings[38] = "MaterialLineColor";
			enumValues[39] = HPS::AttributeLock::Type::MaterialMarkerColor; enumStrings[39] = "MaterialMarkerColor";
			enumValues[40] = HPS::AttributeLock::Type::MaterialTextColor; enumStrings[40] = "MaterialTextColor";
			enumValues[41] = HPS::AttributeLock::Type::MaterialCutEdgeColor; enumStrings[41] = "MaterialCutEdgeColor";
			enumValues[42] = HPS::AttributeLock::Type::MaterialVertex; enumStrings[42] = "MaterialVertex";
			enumValues[43] = HPS::AttributeLock::Type::MaterialVertexDiffuse; enumStrings[43] = "MaterialVertexDiffuse";
			enumValues[44] = HPS::AttributeLock::Type::MaterialVertexDiffuseColor; enumStrings[44] = "MaterialVertexDiffuseColor";
			enumValues[45] = HPS::AttributeLock::Type::MaterialVertexDiffuseAlpha; enumStrings[45] = "MaterialVertexDiffuseAlpha";
			enumValues[46] = HPS::AttributeLock::Type::MaterialVertexDiffuseTexture; enumStrings[46] = "MaterialVertexDiffuseTexture";
			enumValues[47] = HPS::AttributeLock::Type::MaterialVertexSpecular; enumStrings[47] = "MaterialVertexSpecular";
			enumValues[48] = HPS::AttributeLock::Type::MaterialVertexMirror; enumStrings[48] = "MaterialVertexMirror";
			enumValues[49] = HPS::AttributeLock::Type::MaterialVertexTransmission; enumStrings[49] = "MaterialVertexTransmission";
			enumValues[50] = HPS::AttributeLock::Type::MaterialVertexEmission; enumStrings[50] = "MaterialVertexEmission";
			enumValues[51] = HPS::AttributeLock::Type::MaterialVertexEnvironment; enumStrings[51] = "MaterialVertexEnvironment";
			enumValues[52] = HPS::AttributeLock::Type::MaterialVertexBump; enumStrings[52] = "MaterialVertexBump";
			enumValues[53] = HPS::AttributeLock::Type::MaterialVertexGloss; enumStrings[53] = "MaterialVertexGloss";
			enumValues[54] = HPS::AttributeLock::Type::MaterialEdge; enumStrings[54] = "MaterialEdge";
			enumValues[55] = HPS::AttributeLock::Type::MaterialEdgeDiffuse; enumStrings[55] = "MaterialEdgeDiffuse";
			enumValues[56] = HPS::AttributeLock::Type::MaterialEdgeDiffuseColor; enumStrings[56] = "MaterialEdgeDiffuseColor";
			enumValues[57] = HPS::AttributeLock::Type::MaterialEdgeDiffuseAlpha; enumStrings[57] = "MaterialEdgeDiffuseAlpha";
			enumValues[58] = HPS::AttributeLock::Type::MaterialEdgeDiffuseTexture; enumStrings[58] = "MaterialEdgeDiffuseTexture";
			enumValues[59] = HPS::AttributeLock::Type::MaterialEdgeSpecular; enumStrings[59] = "MaterialEdgeSpecular";
			enumValues[60] = HPS::AttributeLock::Type::MaterialEdgeMirror; enumStrings[60] = "MaterialEdgeMirror";
			enumValues[61] = HPS::AttributeLock::Type::MaterialEdgeTransmission; enumStrings[61] = "MaterialEdgeTransmission";
			enumValues[62] = HPS::AttributeLock::Type::MaterialEdgeEmission; enumStrings[62] = "MaterialEdgeEmission";
			enumValues[63] = HPS::AttributeLock::Type::MaterialEdgeEnvironment; enumStrings[63] = "MaterialEdgeEnvironment";
			enumValues[64] = HPS::AttributeLock::Type::MaterialEdgeBump; enumStrings[64] = "MaterialEdgeBump";
			enumValues[65] = HPS::AttributeLock::Type::MaterialEdgeGloss; enumStrings[65] = "MaterialEdgeGloss";
			enumValues[66] = HPS::AttributeLock::Type::MaterialFace; enumStrings[66] = "MaterialFace";
			enumValues[67] = HPS::AttributeLock::Type::MaterialFaceDiffuse; enumStrings[67] = "MaterialFaceDiffuse";
			enumValues[68] = HPS::AttributeLock::Type::MaterialFaceDiffuseColor; enumStrings[68] = "MaterialFaceDiffuseColor";
			enumValues[69] = HPS::AttributeLock::Type::MaterialFaceDiffuseAlpha; enumStrings[69] = "MaterialFaceDiffuseAlpha";
			enumValues[70] = HPS::AttributeLock::Type::MaterialFaceDiffuseTexture; enumStrings[70] = "MaterialFaceDiffuseTexture";
			enumValues[71] = HPS::AttributeLock::Type::MaterialFaceSpecular; enumStrings[71] = "MaterialFaceSpecular";
			enumValues[72] = HPS::AttributeLock::Type::MaterialFaceMirror; enumStrings[72] = "MaterialFaceMirror";
			enumValues[73] = HPS::AttributeLock::Type::MaterialFaceTransmission; enumStrings[73] = "MaterialFaceTransmission";
			enumValues[74] = HPS::AttributeLock::Type::MaterialFaceEmission; enumStrings[74] = "MaterialFaceEmission";
			enumValues[75] = HPS::AttributeLock::Type::MaterialFaceEnvironment; enumStrings[75] = "MaterialFaceEnvironment";
			enumValues[76] = HPS::AttributeLock::Type::MaterialFaceBump; enumStrings[76] = "MaterialFaceBump";
			enumValues[77] = HPS::AttributeLock::Type::MaterialFaceGloss; enumStrings[77] = "MaterialFaceGloss";
			enumValues[78] = HPS::AttributeLock::Type::MaterialBackFace; enumStrings[78] = "MaterialBackFace";
			enumValues[79] = HPS::AttributeLock::Type::MaterialBackFaceDiffuse; enumStrings[79] = "MaterialBackFaceDiffuse";
			enumValues[80] = HPS::AttributeLock::Type::MaterialBackFaceDiffuseColor; enumStrings[80] = "MaterialBackFaceDiffuseColor";
			enumValues[81] = HPS::AttributeLock::Type::MaterialBackFaceDiffuseAlpha; enumStrings[81] = "MaterialBackFaceDiffuseAlpha";
			enumValues[82] = HPS::AttributeLock::Type::MaterialBackFaceDiffuseTexture; enumStrings[82] = "MaterialBackFaceDiffuseTexture";
			enumValues[83] = HPS::AttributeLock::Type::MaterialBackFaceSpecular; enumStrings[83] = "MaterialBackFaceSpecular";
			enumValues[84] = HPS::AttributeLock::Type::MaterialBackFaceMirror; enumStrings[84] = "MaterialBackFaceMirror";
			enumValues[85] = HPS::AttributeLock::Type::MaterialBackFaceTransmission; enumStrings[85] = "MaterialBackFaceTransmission";
			enumValues[86] = HPS::AttributeLock::Type::MaterialBackFaceEmission; enumStrings[86] = "MaterialBackFaceEmission";
			enumValues[87] = HPS::AttributeLock::Type::MaterialBackFaceEnvironment; enumStrings[87] = "MaterialBackFaceEnvironment";
			enumValues[88] = HPS::AttributeLock::Type::MaterialBackFaceBump; enumStrings[88] = "MaterialBackFaceBump";
			enumValues[89] = HPS::AttributeLock::Type::MaterialBackFaceGloss; enumStrings[89] = "MaterialBackFaceGloss";
			enumValues[90] = HPS::AttributeLock::Type::MaterialFrontFace; enumStrings[90] = "MaterialFrontFace";
			enumValues[91] = HPS::AttributeLock::Type::MaterialFrontFaceDiffuse; enumStrings[91] = "MaterialFrontFaceDiffuse";
			enumValues[92] = HPS::AttributeLock::Type::MaterialFrontFaceDiffuseColor; enumStrings[92] = "MaterialFrontFaceDiffuseColor";
			enumValues[93] = HPS::AttributeLock::Type::MaterialFrontFaceDiffuseAlpha; enumStrings[93] = "MaterialFrontFaceDiffuseAlpha";
			enumValues[94] = HPS::AttributeLock::Type::MaterialFrontFaceDiffuseTexture; enumStrings[94] = "MaterialFrontFaceDiffuseTexture";
			enumValues[95] = HPS::AttributeLock::Type::MaterialFrontFaceSpecular; enumStrings[95] = "MaterialFrontFaceSpecular";
			enumValues[96] = HPS::AttributeLock::Type::MaterialFrontFaceMirror; enumStrings[96] = "MaterialFrontFaceMirror";
			enumValues[97] = HPS::AttributeLock::Type::MaterialFrontFaceTransmission; enumStrings[97] = "MaterialFrontFaceTransmission";
			enumValues[98] = HPS::AttributeLock::Type::MaterialFrontFaceEmission; enumStrings[98] = "MaterialFrontFaceEmission";
			enumValues[99] = HPS::AttributeLock::Type::MaterialFrontFaceEnvironment; enumStrings[99] = "MaterialFrontFaceEnvironment";
			enumValues[100] = HPS::AttributeLock::Type::MaterialFrontFaceBump; enumStrings[100] = "MaterialFrontFaceBump";
			enumValues[101] = HPS::AttributeLock::Type::MaterialFrontFaceGloss; enumStrings[101] = "MaterialFrontFaceGloss";
			enumValues[102] = HPS::AttributeLock::Type::MaterialCutFace; enumStrings[102] = "MaterialCutFace";
			enumValues[103] = HPS::AttributeLock::Type::MaterialCutFaceDiffuse; enumStrings[103] = "MaterialCutFaceDiffuse";
			enumValues[104] = HPS::AttributeLock::Type::MaterialCutFaceDiffuseColor; enumStrings[104] = "MaterialCutFaceDiffuseColor";
			enumValues[105] = HPS::AttributeLock::Type::MaterialCutFaceDiffuseAlpha; enumStrings[105] = "MaterialCutFaceDiffuseAlpha";
			enumValues[106] = HPS::AttributeLock::Type::MaterialCutFaceDiffuseTexture; enumStrings[106] = "MaterialCutFaceDiffuseTexture";
			enumValues[107] = HPS::AttributeLock::Type::MaterialCutFaceSpecular; enumStrings[107] = "MaterialCutFaceSpecular";
			enumValues[108] = HPS::AttributeLock::Type::MaterialCutFaceMirror; enumStrings[108] = "MaterialCutFaceMirror";
			enumValues[109] = HPS::AttributeLock::Type::MaterialCutFaceTransmission; enumStrings[109] = "MaterialCutFaceTransmission";
			enumValues[110] = HPS::AttributeLock::Type::MaterialCutFaceEmission; enumStrings[110] = "MaterialCutFaceEmission";
			enumValues[111] = HPS::AttributeLock::Type::MaterialCutFaceEnvironment; enumStrings[111] = "MaterialCutFaceEnvironment";
			enumValues[112] = HPS::AttributeLock::Type::MaterialCutFaceBump; enumStrings[112] = "MaterialCutFaceBump";
			enumValues[113] = HPS::AttributeLock::Type::MaterialCutFaceGloss; enumStrings[113] = "MaterialCutFaceGloss";
			enumValues[114] = HPS::AttributeLock::Type::Camera; enumStrings[114] = "Camera";
			InitializeEnumValues(enumValues, enumStrings);
		}
	};

	class DrawingHandednessProperty : public BaseEnumProperty<HPS::Drawing::Handedness>
	{
	public:
		DrawingHandednessProperty(
			HPS::Drawing::Handedness & enumValue,
			CString const & name = _T("Handedness"))
			: BaseEnumProperty(name, enumValue)
		{
			EnumTypeArray enumValues(3); HPS::UTF8Array enumStrings(3);
			enumValues[0] = HPS::Drawing::Handedness::None; enumStrings[0] = "None";
			enumValues[1] = HPS::Drawing::Handedness::Left; enumStrings[1] = "Left";
			enumValues[2] = HPS::Drawing::Handedness::Right; enumStrings[2] = "Right";
			InitializeEnumValues(enumValues, enumStrings);
		}
	};

	class DrawingOverlayProperty : public BaseEnumProperty<HPS::Drawing::Overlay>
	{
	public:
		DrawingOverlayProperty(
			HPS::Drawing::Overlay & enumValue,
			CString const & name = _T("Overlay"))
			: BaseEnumProperty(name, enumValue)
		{
			EnumTypeArray enumValues(4); HPS::UTF8Array enumStrings(4);
			enumValues[0] = HPS::Drawing::Overlay::None; enumStrings[0] = "None";
			enumValues[1] = HPS::Drawing::Overlay::Default; enumStrings[1] = "Default";
			enumValues[2] = HPS::Drawing::Overlay::WithZValues; enumStrings[2] = "WithZValues";
			enumValues[3] = HPS::Drawing::Overlay::InPlace; enumStrings[3] = "InPlace";
			InitializeEnumValues(enumValues, enumStrings);
		}
	};

	class DrawingClipOperationProperty : public BaseEnumProperty<HPS::Drawing::ClipOperation>
	{
	public:
		DrawingClipOperationProperty(
			HPS::Drawing::ClipOperation & enumValue,
			CString const & name = _T("ClipOperation"))
			: BaseEnumProperty(name, enumValue)
		{
			EnumTypeArray enumValues(2); HPS::UTF8Array enumStrings(2);
			enumValues[0] = HPS::Drawing::ClipOperation::Keep; enumStrings[0] = "Keep";
			enumValues[1] = HPS::Drawing::ClipOperation::Remove; enumStrings[1] = "Remove";
			InitializeEnumValues(enumValues, enumStrings);
		}
	};

	class DrawingClipSpaceProperty : public BaseEnumProperty<HPS::Drawing::ClipSpace>
	{
	public:
		DrawingClipSpaceProperty(
			HPS::Drawing::ClipSpace & enumValue,
			CString const & name = _T("ClipSpace"))
			: BaseEnumProperty(name, enumValue)
		{
			EnumTypeArray enumValues(3); HPS::UTF8Array enumStrings(3);
			enumValues[0] = HPS::Drawing::ClipSpace::Window; enumStrings[0] = "Window";
			enumValues[1] = HPS::Drawing::ClipSpace::World; enumStrings[1] = "World";
			enumValues[2] = HPS::Drawing::ClipSpace::Object; enumStrings[2] = "Object";
			InitializeEnumValues(enumValues, enumStrings);
		}
	};

	class HiddenLineAlgorithmProperty : public BaseEnumProperty<HPS::HiddenLine::Algorithm>
	{
	public:
		HiddenLineAlgorithmProperty(
			HPS::HiddenLine::Algorithm & enumValue,
			CString const & name = _T("Algorithm"))
			: BaseEnumProperty(name, enumValue)
		{
			EnumTypeArray enumValues(3); HPS::UTF8Array enumStrings(3);
			enumValues[0] = HPS::HiddenLine::Algorithm::None; enumStrings[0] = "None";
			enumValues[1] = HPS::HiddenLine::Algorithm::ZBuffer; enumStrings[1] = "ZBuffer";
			enumValues[2] = HPS::HiddenLine::Algorithm::ZSort; enumStrings[2] = "ZSort";
			InitializeEnumValues(enumValues, enumStrings);
		}
	};

	class SelectionLevelProperty : public BaseEnumProperty<HPS::Selection::Level>
	{
	public:
		SelectionLevelProperty(
			HPS::Selection::Level & enumValue,
			CString const & name = _T("Level"))
			: BaseEnumProperty(name, enumValue)
		{
			EnumTypeArray enumValues(3); HPS::UTF8Array enumStrings(3);
			enumValues[0] = HPS::Selection::Level::Segment; enumStrings[0] = "Segment";
			enumValues[1] = HPS::Selection::Level::Entity; enumStrings[1] = "Entity";
			enumValues[2] = HPS::Selection::Level::Subentity; enumStrings[2] = "Subentity";
			InitializeEnumValues(enumValues, enumStrings);
		}
	};

	class SelectionSortingProperty : public BaseEnumProperty<HPS::Selection::Sorting>
	{
	public:
		SelectionSortingProperty(
			HPS::Selection::Sorting & enumValue,
			CString const & name = _T("Sorting"))
			: BaseEnumProperty(name, enumValue)
		{
			EnumTypeArray enumValues(4); HPS::UTF8Array enumStrings(4);
			enumValues[0] = HPS::Selection::Sorting::Off; enumStrings[0] = "Off";
			enumValues[1] = HPS::Selection::Sorting::Proximity; enumStrings[1] = "Proximity";
			enumValues[2] = HPS::Selection::Sorting::ZSorting; enumStrings[2] = "ZSorting";
			enumValues[3] = HPS::Selection::Sorting::Default; enumStrings[3] = "Default";
			InitializeEnumValues(enumValues, enumStrings);
		}
	};

	class SelectionAlgorithmProperty : public BaseEnumProperty<HPS::Selection::Algorithm>
	{
	public:
		SelectionAlgorithmProperty(
			HPS::Selection::Algorithm & enumValue,
			CString const & name = _T("Algorithm"))
			: BaseEnumProperty(name, enumValue)
		{
			EnumTypeArray enumValues(2); HPS::UTF8Array enumStrings(2);
			enumValues[0] = HPS::Selection::Algorithm::Visual; enumStrings[0] = "Visual";
			enumValues[1] = HPS::Selection::Algorithm::Analytic; enumStrings[1] = "Analytic";
			InitializeEnumValues(enumValues, enumStrings);
		}
	};

	class SelectionGranularityProperty : public BaseEnumProperty<HPS::Selection::Granularity>
	{
	public:
		SelectionGranularityProperty(
			HPS::Selection::Granularity & enumValue,
			CString const & name = _T("Granularity"))
			: BaseEnumProperty(name, enumValue)
		{
			EnumTypeArray enumValues(2); HPS::UTF8Array enumStrings(2);
			enumValues[0] = HPS::Selection::Granularity::General; enumStrings[0] = "General";
			enumValues[1] = HPS::Selection::Granularity::Detailed; enumStrings[1] = "Detailed";
			InitializeEnumValues(enumValues, enumStrings);
		}
	};

	class CameraProjectionProperty : public BaseEnumProperty<HPS::Camera::Projection>
	{
	public:
		CameraProjectionProperty(
			HPS::Camera::Projection & enumValue,
			CString const & name = _T("Projection"))
			: BaseEnumProperty(name, enumValue)
		{
			EnumTypeArray enumValues(4); HPS::UTF8Array enumStrings(4);
			enumValues[0] = HPS::Camera::Projection::Default; enumStrings[0] = "Default";
			enumValues[1] = HPS::Camera::Projection::Perspective; enumStrings[1] = "Perspective";
			enumValues[2] = HPS::Camera::Projection::Orthographic; enumStrings[2] = "Orthographic";
			enumValues[3] = HPS::Camera::Projection::Stretched; enumStrings[3] = "Stretched";
			InitializeEnumValues(enumValues, enumStrings);
		}
	};

	class SelectabilityValueProperty : public BaseEnumProperty<HPS::Selectability::Value>
	{
	public:
		SelectabilityValueProperty(
			HPS::Selectability::Value & enumValue,
			CString const & name = _T("Value"))
			: BaseEnumProperty(name, enumValue)
		{
			EnumTypeArray enumValues(3); HPS::UTF8Array enumStrings(3);
			enumValues[0] = HPS::Selectability::Value::Off; enumStrings[0] = "Off";
			enumValues[1] = HPS::Selectability::Value::On; enumStrings[1] = "On";
			enumValues[2] = HPS::Selectability::Value::ForcedOn; enumStrings[2] = "ForcedOn";
			InitializeEnumValues(enumValues, enumStrings);
		}
	};

	class TransparencyMethodProperty : public BaseEnumProperty<HPS::Transparency::Method>
	{
	public:
		TransparencyMethodProperty(
			HPS::Transparency::Method & enumValue,
			CString const & name = _T("Method"))
			: BaseEnumProperty(name, enumValue)
		{
			EnumTypeArray enumValues(3); HPS::UTF8Array enumStrings(3);
			enumValues[0] = HPS::Transparency::Method::None; enumStrings[0] = "None";
			enumValues[1] = HPS::Transparency::Method::Blended; enumStrings[1] = "Blended";
			enumValues[2] = HPS::Transparency::Method::ScreenDoor; enumStrings[2] = "ScreenDoor";
			InitializeEnumValues(enumValues, enumStrings);
		}
	};

	class TransparencyAlgorithmProperty : public BaseEnumProperty<HPS::Transparency::Algorithm>
	{
	public:
		TransparencyAlgorithmProperty(
			HPS::Transparency::Algorithm & enumValue,
			CString const & name = _T("Algorithm"))
			: BaseEnumProperty(name, enumValue)
		{
			EnumTypeArray enumValues(6); HPS::UTF8Array enumStrings(6);
			enumValues[0] = HPS::Transparency::Algorithm::None; enumStrings[0] = "None";
			enumValues[1] = HPS::Transparency::Algorithm::Painters; enumStrings[1] = "Painters";
			enumValues[2] = HPS::Transparency::Algorithm::ZSortNicest; enumStrings[2] = "ZSortNicest";
			enumValues[3] = HPS::Transparency::Algorithm::ZSortFastest; enumStrings[3] = "ZSortFastest";
			enumValues[4] = HPS::Transparency::Algorithm::DepthPeeling; enumStrings[4] = "DepthPeeling";
			enumValues[5] = HPS::Transparency::Algorithm::WeightedBlended; enumStrings[5] = "WeightedBlended";
			InitializeEnumValues(enumValues, enumStrings);
		}
	};

	class TransparencyAreaUnitsProperty : public BaseEnumProperty<HPS::Transparency::AreaUnits>
	{
	public:
		TransparencyAreaUnitsProperty(
			HPS::Transparency::AreaUnits & enumValue,
			CString const & name = _T("AreaUnits"))
			: BaseEnumProperty(name, enumValue)
		{
			EnumTypeArray enumValues(2); HPS::UTF8Array enumStrings(2);
			enumValues[0] = HPS::Transparency::AreaUnits::Percent; enumStrings[0] = "Percent";
			enumValues[1] = HPS::Transparency::AreaUnits::Pixels; enumStrings[1] = "Pixels";
			InitializeEnumValues(enumValues, enumStrings);
		}
	};

	class TransparencyPreferenceProperty : public BaseEnumProperty<HPS::Transparency::Preference>
	{
	public:
		TransparencyPreferenceProperty(
			HPS::Transparency::Preference & enumValue,
			CString const & name = _T("Preference"))
			: BaseEnumProperty(name, enumValue)
		{
			EnumTypeArray enumValues(2); HPS::UTF8Array enumStrings(2);
			enumValues[0] = HPS::Transparency::Preference::Nicest; enumStrings[0] = "Nicest";
			enumValues[1] = HPS::Transparency::Preference::Fastest; enumStrings[1] = "Fastest";
			InitializeEnumValues(enumValues, enumStrings);
		}
	};

	class MarkerDrawingPreferenceProperty : public BaseEnumProperty<HPS::Marker::DrawingPreference>
	{
	public:
		MarkerDrawingPreferenceProperty(
			HPS::Marker::DrawingPreference & enumValue,
			CString const & name = _T("DrawingPreference"))
			: BaseEnumProperty(name, enumValue)
		{
			EnumTypeArray enumValues(2); HPS::UTF8Array enumStrings(2);
			enumValues[0] = HPS::Marker::DrawingPreference::Nicest; enumStrings[0] = "Nicest";
			enumValues[1] = HPS::Marker::DrawingPreference::Fastest; enumStrings[1] = "Fastest";
			InitializeEnumValues(enumValues, enumStrings);
		}
	};

	class MarkerSizeUnitsProperty : public BaseEnumProperty<HPS::Marker::SizeUnits>
	{
	public:
		MarkerSizeUnitsProperty(
			HPS::Marker::SizeUnits & enumValue,
			CString const & name = _T("SizeUnits"))
			: BaseEnumProperty(name, enumValue)
		{
			EnumTypeArray enumValues(7); HPS::UTF8Array enumStrings(7);
			enumValues[0] = HPS::Marker::SizeUnits::ScaleFactor; enumStrings[0] = "ScaleFactor";
			enumValues[1] = HPS::Marker::SizeUnits::ObjectSpace; enumStrings[1] = "ObjectSpace";
			enumValues[2] = HPS::Marker::SizeUnits::SubscreenRelative; enumStrings[2] = "SubscreenRelative";
			enumValues[3] = HPS::Marker::SizeUnits::WindowRelative; enumStrings[3] = "WindowRelative";
			enumValues[4] = HPS::Marker::SizeUnits::WorldSpace; enumStrings[4] = "WorldSpace";
			enumValues[5] = HPS::Marker::SizeUnits::Points; enumStrings[5] = "Points";
			enumValues[6] = HPS::Marker::SizeUnits::Pixels; enumStrings[6] = "Pixels";
			InitializeEnumValues(enumValues, enumStrings);
		}
	};

	class LightingInterpolationAlgorithmProperty : public BaseEnumProperty<HPS::Lighting::InterpolationAlgorithm>
	{
	public:
		LightingInterpolationAlgorithmProperty(
			HPS::Lighting::InterpolationAlgorithm & enumValue,
			CString const & name = _T("InterpolationAlgorithm"))
			: BaseEnumProperty(name, enumValue)
		{
			EnumTypeArray enumValues(3); HPS::UTF8Array enumStrings(3);
			enumValues[0] = HPS::Lighting::InterpolationAlgorithm::Phong; enumStrings[0] = "Phong";
			enumValues[1] = HPS::Lighting::InterpolationAlgorithm::Gouraud; enumStrings[1] = "Gouraud";
			enumValues[2] = HPS::Lighting::InterpolationAlgorithm::Flat; enumStrings[2] = "Flat";
			InitializeEnumValues(enumValues, enumStrings);
		}
	};

	class TextAlignmentProperty : public BaseEnumProperty<HPS::Text::Alignment>
	{
	public:
		TextAlignmentProperty(
			HPS::Text::Alignment & enumValue,
			CString const & name = _T("Alignment"))
			: BaseEnumProperty(name, enumValue)
		{
			EnumTypeArray enumValues(9); HPS::UTF8Array enumStrings(9);
			enumValues[0] = HPS::Text::Alignment::TopLeft; enumStrings[0] = "TopLeft";
			enumValues[1] = HPS::Text::Alignment::CenterLeft; enumStrings[1] = "CenterLeft";
			enumValues[2] = HPS::Text::Alignment::BottomLeft; enumStrings[2] = "BottomLeft";
			enumValues[3] = HPS::Text::Alignment::TopCenter; enumStrings[3] = "TopCenter";
			enumValues[4] = HPS::Text::Alignment::Center; enumStrings[4] = "Center";
			enumValues[5] = HPS::Text::Alignment::BottomCenter; enumStrings[5] = "BottomCenter";
			enumValues[6] = HPS::Text::Alignment::TopRight; enumStrings[6] = "TopRight";
			enumValues[7] = HPS::Text::Alignment::CenterRight; enumStrings[7] = "CenterRight";
			enumValues[8] = HPS::Text::Alignment::BottomRight; enumStrings[8] = "BottomRight";
			InitializeEnumValues(enumValues, enumStrings);
		}
	};

	class TextReferenceFrameProperty : public BaseEnumProperty<HPS::Text::ReferenceFrame>
	{
	public:
		TextReferenceFrameProperty(
			HPS::Text::ReferenceFrame & enumValue,
			CString const & name = _T("ReferenceFrame"))
			: BaseEnumProperty(name, enumValue)
		{
			EnumTypeArray enumValues(2); HPS::UTF8Array enumStrings(2);
			enumValues[0] = HPS::Text::ReferenceFrame::WorldAligned; enumStrings[0] = "WorldAligned";
			enumValues[1] = HPS::Text::ReferenceFrame::PathAligned; enumStrings[1] = "PathAligned";
			InitializeEnumValues(enumValues, enumStrings);
		}
	};

	class TextJustificationProperty : public BaseEnumProperty<HPS::Text::Justification>
	{
	public:
		TextJustificationProperty(
			HPS::Text::Justification & enumValue,
			CString const & name = _T("Justification"))
			: BaseEnumProperty(name, enumValue)
		{
			EnumTypeArray enumValues(3); HPS::UTF8Array enumStrings(3);
			enumValues[0] = HPS::Text::Justification::Left; enumStrings[0] = "Left";
			enumValues[1] = HPS::Text::Justification::Right; enumStrings[1] = "Right";
			enumValues[2] = HPS::Text::Justification::Center; enumStrings[2] = "Center";
			InitializeEnumValues(enumValues, enumStrings);
		}
	};

	class TextTransformProperty : public BaseEnumProperty<HPS::Text::Transform>
	{
	public:
		TextTransformProperty(
			HPS::Text::Transform & enumValue,
			CString const & name = _T("Transform"))
			: BaseEnumProperty(name, enumValue)
		{
			EnumTypeArray enumValues(5); HPS::UTF8Array enumStrings(5);
			enumValues[0] = HPS::Text::Transform::Transformable; enumStrings[0] = "Transformable";
			enumValues[1] = HPS::Text::Transform::NonTransformable; enumStrings[1] = "NonTransformable";
			enumValues[2] = HPS::Text::Transform::CharacterPositionOnly; enumStrings[2] = "CharacterPositionOnly";
			enumValues[3] = HPS::Text::Transform::CharacterPositionAdjusted; enumStrings[3] = "CharacterPositionAdjusted";
			enumValues[4] = HPS::Text::Transform::NonScalingTransformable; enumStrings[4] = "NonScalingTransformable";
			InitializeEnumValues(enumValues, enumStrings);
		}
	};

	class TextRendererProperty : public BaseEnumProperty<HPS::Text::Renderer>
	{
	public:
		TextRendererProperty(
			HPS::Text::Renderer & enumValue,
			CString const & name = _T("Renderer"))
			: BaseEnumProperty(name, enumValue)
		{
			EnumTypeArray enumValues(3); HPS::UTF8Array enumStrings(3);
			enumValues[0] = HPS::Text::Renderer::Default; enumStrings[0] = "Default";
			enumValues[1] = HPS::Text::Renderer::Driver; enumStrings[1] = "Driver";
			enumValues[2] = HPS::Text::Renderer::Truetype; enumStrings[2] = "Truetype";
			InitializeEnumValues(enumValues, enumStrings);
		}
	};

	class TextPreferenceProperty : public BaseEnumProperty<HPS::Text::Preference>
	{
	public:
		TextPreferenceProperty(
			HPS::Text::Preference & enumValue,
			CString const & name = _T("Preference"))
			: BaseEnumProperty(name, enumValue)
		{
			EnumTypeArray enumValues(4); HPS::UTF8Array enumStrings(4);
			enumValues[0] = HPS::Text::Preference::Default; enumStrings[0] = "Default";
			enumValues[1] = HPS::Text::Preference::Vector; enumStrings[1] = "Vector";
			enumValues[2] = HPS::Text::Preference::Raster; enumStrings[2] = "Raster";
			enumValues[3] = HPS::Text::Preference::Exterior; enumStrings[3] = "Exterior";
			InitializeEnumValues(enumValues, enumStrings);
		}
	};

	class TextSizeUnitsProperty : public BaseEnumProperty<HPS::Text::SizeUnits>
	{
	public:
		TextSizeUnitsProperty(
			HPS::Text::SizeUnits & enumValue,
			CString const & name = _T("SizeUnits"))
			: BaseEnumProperty(name, enumValue)
		{
			EnumTypeArray enumValues(6); HPS::UTF8Array enumStrings(6);
			enumValues[0] = HPS::Text::SizeUnits::ObjectSpace; enumStrings[0] = "ObjectSpace";
			enumValues[1] = HPS::Text::SizeUnits::SubscreenRelative; enumStrings[1] = "SubscreenRelative";
			enumValues[2] = HPS::Text::SizeUnits::WindowRelative; enumStrings[2] = "WindowRelative";
			enumValues[3] = HPS::Text::SizeUnits::WorldSpace; enumStrings[3] = "WorldSpace";
			enumValues[4] = HPS::Text::SizeUnits::Points; enumStrings[4] = "Points";
			enumValues[5] = HPS::Text::SizeUnits::Pixels; enumStrings[5] = "Pixels";
			InitializeEnumValues(enumValues, enumStrings);
		}
	};

	class TextSizeToleranceUnitsProperty : public BaseEnumProperty<HPS::Text::SizeToleranceUnits>
	{
	public:
		TextSizeToleranceUnitsProperty(
			HPS::Text::SizeToleranceUnits & enumValue,
			CString const & name = _T("SizeToleranceUnits"))
			: BaseEnumProperty(name, enumValue)
		{
			EnumTypeArray enumValues(7); HPS::UTF8Array enumStrings(7);
			enumValues[0] = HPS::Text::SizeToleranceUnits::ObjectSpace; enumStrings[0] = "ObjectSpace";
			enumValues[1] = HPS::Text::SizeToleranceUnits::SubscreenRelative; enumStrings[1] = "SubscreenRelative";
			enumValues[2] = HPS::Text::SizeToleranceUnits::WindowRelative; enumStrings[2] = "WindowRelative";
			enumValues[3] = HPS::Text::SizeToleranceUnits::WorldSpace; enumStrings[3] = "WorldSpace";
			enumValues[4] = HPS::Text::SizeToleranceUnits::Points; enumStrings[4] = "Points";
			enumValues[5] = HPS::Text::SizeToleranceUnits::Pixels; enumStrings[5] = "Pixels";
			enumValues[6] = HPS::Text::SizeToleranceUnits::Percent; enumStrings[6] = "Percent";
			InitializeEnumValues(enumValues, enumStrings);
		}
	};

	class TextMarginUnitsProperty : public BaseEnumProperty<HPS::Text::MarginUnits>
	{
	public:
		TextMarginUnitsProperty(
			HPS::Text::MarginUnits & enumValue,
			CString const & name = _T("MarginUnits"))
			: BaseEnumProperty(name, enumValue)
		{
			EnumTypeArray enumValues(7); HPS::UTF8Array enumStrings(7);
			enumValues[0] = HPS::Text::MarginUnits::ObjectSpace; enumStrings[0] = "ObjectSpace";
			enumValues[1] = HPS::Text::MarginUnits::SubscreenRelative; enumStrings[1] = "SubscreenRelative";
			enumValues[2] = HPS::Text::MarginUnits::WindowRelative; enumStrings[2] = "WindowRelative";
			enumValues[3] = HPS::Text::MarginUnits::WorldSpace; enumStrings[3] = "WorldSpace";
			enumValues[4] = HPS::Text::MarginUnits::Points; enumStrings[4] = "Points";
			enumValues[5] = HPS::Text::MarginUnits::Pixels; enumStrings[5] = "Pixels";
			enumValues[6] = HPS::Text::MarginUnits::Percent; enumStrings[6] = "Percent";
			InitializeEnumValues(enumValues, enumStrings);
		}
	};

	class TextGreekingUnitsProperty : public BaseEnumProperty<HPS::Text::GreekingUnits>
	{
	public:
		TextGreekingUnitsProperty(
			HPS::Text::GreekingUnits & enumValue,
			CString const & name = _T("GreekingUnits"))
			: BaseEnumProperty(name, enumValue)
		{
			EnumTypeArray enumValues(6); HPS::UTF8Array enumStrings(6);
			enumValues[0] = HPS::Text::GreekingUnits::ObjectSpace; enumStrings[0] = "ObjectSpace";
			enumValues[1] = HPS::Text::GreekingUnits::SubscreenRelative; enumStrings[1] = "SubscreenRelative";
			enumValues[2] = HPS::Text::GreekingUnits::WindowRelative; enumStrings[2] = "WindowRelative";
			enumValues[3] = HPS::Text::GreekingUnits::WorldSpace; enumStrings[3] = "WorldSpace";
			enumValues[4] = HPS::Text::GreekingUnits::Points; enumStrings[4] = "Points";
			enumValues[5] = HPS::Text::GreekingUnits::Pixels; enumStrings[5] = "Pixels";
			InitializeEnumValues(enumValues, enumStrings);
		}
	};

	class TextGreekingModeProperty : public BaseEnumProperty<HPS::Text::GreekingMode>
	{
	public:
		TextGreekingModeProperty(
			HPS::Text::GreekingMode & enumValue,
			CString const & name = _T("GreekingMode"))
			: BaseEnumProperty(name, enumValue)
		{
			EnumTypeArray enumValues(3); HPS::UTF8Array enumStrings(3);
			enumValues[0] = HPS::Text::GreekingMode::Nothing; enumStrings[0] = "Nothing";
			enumValues[1] = HPS::Text::GreekingMode::Lines; enumStrings[1] = "Lines";
			enumValues[2] = HPS::Text::GreekingMode::Box; enumStrings[2] = "Box";
			InitializeEnumValues(enumValues, enumStrings);
		}
	};

	class TextRegionAlignmentProperty : public BaseEnumProperty<HPS::Text::RegionAlignment>
	{
	public:
		TextRegionAlignmentProperty(
			HPS::Text::RegionAlignment & enumValue,
			CString const & name = _T("RegionAlignment"))
			: BaseEnumProperty(name, enumValue)
		{
			EnumTypeArray enumValues(3); HPS::UTF8Array enumStrings(3);
			enumValues[0] = HPS::Text::RegionAlignment::Top; enumStrings[0] = "Top";
			enumValues[1] = HPS::Text::RegionAlignment::Center; enumStrings[1] = "Center";
			enumValues[2] = HPS::Text::RegionAlignment::Bottom; enumStrings[2] = "Bottom";
			InitializeEnumValues(enumValues, enumStrings);
		}
	};

	class TextLeaderLineSpaceProperty : public BaseEnumProperty<HPS::Text::LeaderLineSpace>
	{
	public:
		TextLeaderLineSpaceProperty(
			HPS::Text::LeaderLineSpace & enumValue,
			CString const & name = _T("LeaderLineSpace"))
			: BaseEnumProperty(name, enumValue)
		{
			EnumTypeArray enumValues(2); HPS::UTF8Array enumStrings(2);
			enumValues[0] = HPS::Text::LeaderLineSpace::Object; enumStrings[0] = "Object";
			enumValues[1] = HPS::Text::LeaderLineSpace::World; enumStrings[1] = "World";
			InitializeEnumValues(enumValues, enumStrings);
		}
	};

	class TextRegionFittingProperty : public BaseEnumProperty<HPS::Text::RegionFitting>
	{
	public:
		TextRegionFittingProperty(
			HPS::Text::RegionFitting & enumValue,
			CString const & name = _T("RegionFitting"))
			: BaseEnumProperty(name, enumValue)
		{
			EnumTypeArray enumValues(6); HPS::UTF8Array enumStrings(6);
			enumValues[0] = HPS::Text::RegionFitting::Left; enumStrings[0] = "Left";
			enumValues[1] = HPS::Text::RegionFitting::Center; enumStrings[1] = "Center";
			enumValues[2] = HPS::Text::RegionFitting::Right; enumStrings[2] = "Right";
			enumValues[3] = HPS::Text::RegionFitting::Spacing; enumStrings[3] = "Spacing";
			enumValues[4] = HPS::Text::RegionFitting::Width; enumStrings[4] = "Width";
			enumValues[5] = HPS::Text::RegionFitting::Auto; enumStrings[5] = "Auto";
			InitializeEnumValues(enumValues, enumStrings);
		}
	};

	class LineCoordinateSpaceProperty : public BaseEnumProperty<HPS::Line::CoordinateSpace>
	{
	public:
		LineCoordinateSpaceProperty(
			HPS::Line::CoordinateSpace & enumValue,
			CString const & name = _T("CoordinateSpace"))
			: BaseEnumProperty(name, enumValue)
		{
			EnumTypeArray enumValues(4); HPS::UTF8Array enumStrings(4);
			enumValues[0] = HPS::Line::CoordinateSpace::Object; enumStrings[0] = "Object";
			enumValues[1] = HPS::Line::CoordinateSpace::World; enumStrings[1] = "World";
			enumValues[2] = HPS::Line::CoordinateSpace::NormalizedInnerWindow; enumStrings[2] = "NormalizedInnerWindow";
			enumValues[3] = HPS::Line::CoordinateSpace::NormalizedInnerPixel; enumStrings[3] = "NormalizedInnerPixel";
			InitializeEnumValues(enumValues, enumStrings);
		}
	};

	class LineSizeUnitsProperty : public BaseEnumProperty<HPS::Line::SizeUnits>
	{
	public:
		LineSizeUnitsProperty(
			HPS::Line::SizeUnits & enumValue,
			CString const & name = _T("SizeUnits"))
			: BaseEnumProperty(name, enumValue)
		{
			EnumTypeArray enumValues(7); HPS::UTF8Array enumStrings(7);
			enumValues[0] = HPS::Line::SizeUnits::ScaleFactor; enumStrings[0] = "ScaleFactor";
			enumValues[1] = HPS::Line::SizeUnits::ObjectSpace; enumStrings[1] = "ObjectSpace";
			enumValues[2] = HPS::Line::SizeUnits::SubscreenRelative; enumStrings[2] = "SubscreenRelative";
			enumValues[3] = HPS::Line::SizeUnits::WindowRelative; enumStrings[3] = "WindowRelative";
			enumValues[4] = HPS::Line::SizeUnits::WorldSpace; enumStrings[4] = "WorldSpace";
			enumValues[5] = HPS::Line::SizeUnits::Points; enumStrings[5] = "Points";
			enumValues[6] = HPS::Line::SizeUnits::Pixels; enumStrings[6] = "Pixels";
			InitializeEnumValues(enumValues, enumStrings);
		}
	};

	class EdgeSizeUnitsProperty : public BaseEnumProperty<HPS::Edge::SizeUnits>
	{
	public:
		EdgeSizeUnitsProperty(
			HPS::Edge::SizeUnits & enumValue,
			CString const & name = _T("SizeUnits"))
			: BaseEnumProperty(name, enumValue)
		{
			EnumTypeArray enumValues(7); HPS::UTF8Array enumStrings(7);
			enumValues[0] = HPS::Edge::SizeUnits::ScaleFactor; enumStrings[0] = "ScaleFactor";
			enumValues[1] = HPS::Edge::SizeUnits::ObjectSpace; enumStrings[1] = "ObjectSpace";
			enumValues[2] = HPS::Edge::SizeUnits::SubscreenRelative; enumStrings[2] = "SubscreenRelative";
			enumValues[3] = HPS::Edge::SizeUnits::WindowRelative; enumStrings[3] = "WindowRelative";
			enumValues[4] = HPS::Edge::SizeUnits::WorldSpace; enumStrings[4] = "WorldSpace";
			enumValues[5] = HPS::Edge::SizeUnits::Points; enumStrings[5] = "Points";
			enumValues[6] = HPS::Edge::SizeUnits::Pixels; enumStrings[6] = "Pixels";
			InitializeEnumValues(enumValues, enumStrings);
		}
	};

	class CuttingSectionModeProperty : public BaseEnumProperty<HPS::CuttingSection::Mode>
	{
	public:
		CuttingSectionModeProperty(
			HPS::CuttingSection::Mode & enumValue,
			CString const & name = _T("Mode"))
			: BaseEnumProperty(name, enumValue)
		{
			EnumTypeArray enumValues(4); HPS::UTF8Array enumStrings(4);
			enumValues[0] = HPS::CuttingSection::Mode::None; enumStrings[0] = "None";
			enumValues[1] = HPS::CuttingSection::Mode::Round; enumStrings[1] = "Round";
			enumValues[2] = HPS::CuttingSection::Mode::Square; enumStrings[2] = "Square";
			enumValues[3] = HPS::CuttingSection::Mode::Plane; enumStrings[3] = "Plane";
			InitializeEnumValues(enumValues, enumStrings);
		}
	};

	class CuttingSectionCappingLevelProperty : public BaseEnumProperty<HPS::CuttingSection::CappingLevel>
	{
	public:
		CuttingSectionCappingLevelProperty(
			HPS::CuttingSection::CappingLevel & enumValue,
			CString const & name = _T("CappingLevel"))
			: BaseEnumProperty(name, enumValue)
		{
			EnumTypeArray enumValues(3); HPS::UTF8Array enumStrings(3);
			enumValues[0] = HPS::CuttingSection::CappingLevel::Entity; enumStrings[0] = "Entity";
			enumValues[1] = HPS::CuttingSection::CappingLevel::Segment; enumStrings[1] = "Segment";
			enumValues[2] = HPS::CuttingSection::CappingLevel::SegmentTree; enumStrings[2] = "SegmentTree";
			InitializeEnumValues(enumValues, enumStrings);
		}
	};

	class CuttingSectionCappingUsageProperty : public BaseEnumProperty<HPS::CuttingSection::CappingUsage>
	{
	public:
		CuttingSectionCappingUsageProperty(
			HPS::CuttingSection::CappingUsage & enumValue,
			CString const & name = _T("CappingUsage"))
			: BaseEnumProperty(name, enumValue)
		{
			EnumTypeArray enumValues(3); HPS::UTF8Array enumStrings(3);
			enumValues[0] = HPS::CuttingSection::CappingUsage::Off; enumStrings[0] = "Off";
			enumValues[1] = HPS::CuttingSection::CappingUsage::On; enumStrings[1] = "On";
			enumValues[2] = HPS::CuttingSection::CappingUsage::Visibility; enumStrings[2] = "Visibility";
			InitializeEnumValues(enumValues, enumStrings);
		}
	};

	class CuttingSectionToleranceUnitsProperty : public BaseEnumProperty<HPS::CuttingSection::ToleranceUnits>
	{
	public:
		CuttingSectionToleranceUnitsProperty(
			HPS::CuttingSection::ToleranceUnits & enumValue,
			CString const & name = _T("ToleranceUnits"))
			: BaseEnumProperty(name, enumValue)
		{
			EnumTypeArray enumValues(2); HPS::UTF8Array enumStrings(2);
			enumValues[0] = HPS::CuttingSection::ToleranceUnits::Percent; enumStrings[0] = "Percent";
			enumValues[1] = HPS::CuttingSection::ToleranceUnits::WorldSpace; enumStrings[1] = "WorldSpace";
			InitializeEnumValues(enumValues, enumStrings);
		}
	};

	class CuttingSectionCuttingLevelProperty : public BaseEnumProperty<HPS::CuttingSection::CuttingLevel>
	{
	public:
		CuttingSectionCuttingLevelProperty(
			HPS::CuttingSection::CuttingLevel & enumValue,
			CString const & name = _T("CuttingLevel"))
			: BaseEnumProperty(name, enumValue)
		{
			EnumTypeArray enumValues(2); HPS::UTF8Array enumStrings(2);
			enumValues[0] = HPS::CuttingSection::CuttingLevel::Global; enumStrings[0] = "Global";
			enumValues[1] = HPS::CuttingSection::CuttingLevel::Local; enumStrings[1] = "Local";
			InitializeEnumValues(enumValues, enumStrings);
		}
	};

	class CuttingSectionMaterialPreferenceProperty : public BaseEnumProperty<HPS::CuttingSection::MaterialPreference>
	{
	public:
		CuttingSectionMaterialPreferenceProperty(
			HPS::CuttingSection::MaterialPreference & enumValue,
			CString const & name = _T("MaterialPreference"))
			: BaseEnumProperty(name, enumValue)
		{
			EnumTypeArray enumValues(2); HPS::UTF8Array enumStrings(2);
			enumValues[0] = HPS::CuttingSection::MaterialPreference::Explicit; enumStrings[0] = "Explicit";
			enumValues[1] = HPS::CuttingSection::MaterialPreference::Implicit; enumStrings[1] = "Implicit";
			InitializeEnumValues(enumValues, enumStrings);
		}
	};

	class LinePatternSizeUnitsProperty : public BaseEnumProperty<HPS::LinePattern::SizeUnits>
	{
	public:
		LinePatternSizeUnitsProperty(
			HPS::LinePattern::SizeUnits & enumValue,
			CString const & name = _T("SizeUnits"))
			: BaseEnumProperty(name, enumValue)
		{
			EnumTypeArray enumValues(7); HPS::UTF8Array enumStrings(7);
			enumValues[0] = HPS::LinePattern::SizeUnits::ObjectSpace; enumStrings[0] = "ObjectSpace";
			enumValues[1] = HPS::LinePattern::SizeUnits::SubscreenRelative; enumStrings[1] = "SubscreenRelative";
			enumValues[2] = HPS::LinePattern::SizeUnits::WindowRelative; enumStrings[2] = "WindowRelative";
			enumValues[3] = HPS::LinePattern::SizeUnits::WorldSpace; enumStrings[3] = "WorldSpace";
			enumValues[4] = HPS::LinePattern::SizeUnits::Points; enumStrings[4] = "Points";
			enumValues[5] = HPS::LinePattern::SizeUnits::Pixels; enumStrings[5] = "Pixels";
			enumValues[6] = HPS::LinePattern::SizeUnits::ScaleFactor; enumStrings[6] = "ScaleFactor";
			InitializeEnumValues(enumValues, enumStrings);
		}
	};

	class LinePatternInsetBehaviorProperty : public BaseEnumProperty<HPS::LinePattern::InsetBehavior>
	{
	public:
		LinePatternInsetBehaviorProperty(
			HPS::LinePattern::InsetBehavior & enumValue,
			CString const & name = _T("InsetBehavior"))
			: BaseEnumProperty(name, enumValue)
		{
			EnumTypeArray enumValues(3); HPS::UTF8Array enumStrings(3);
			enumValues[0] = HPS::LinePattern::InsetBehavior::Overlap; enumStrings[0] = "Overlap";
			enumValues[1] = HPS::LinePattern::InsetBehavior::Trim; enumStrings[1] = "Trim";
			enumValues[2] = HPS::LinePattern::InsetBehavior::Inline; enumStrings[2] = "Inline";
			InitializeEnumValues(enumValues, enumStrings);
		}
	};

	class LinePatternJoinProperty : public BaseEnumProperty<HPS::LinePattern::Join>
	{
	public:
		LinePatternJoinProperty(
			HPS::LinePattern::Join & enumValue,
			CString const & name = _T("Join"))
			: BaseEnumProperty(name, enumValue)
		{
			EnumTypeArray enumValues(3); HPS::UTF8Array enumStrings(3);
			enumValues[0] = HPS::LinePattern::Join::Mitre; enumStrings[0] = "Mitre";
			enumValues[1] = HPS::LinePattern::Join::Round; enumStrings[1] = "Round";
			enumValues[2] = HPS::LinePattern::Join::Bevel; enumStrings[2] = "Bevel";
			InitializeEnumValues(enumValues, enumStrings);
		}
	};

	class LinePatternCapProperty : public BaseEnumProperty<HPS::LinePattern::Cap>
	{
	public:
		LinePatternCapProperty(
			HPS::LinePattern::Cap & enumValue,
			CString const & name = _T("Cap"))
			: BaseEnumProperty(name, enumValue)
		{
			EnumTypeArray enumValues(4); HPS::UTF8Array enumStrings(4);
			enumValues[0] = HPS::LinePattern::Cap::Butt; enumStrings[0] = "Butt";
			enumValues[1] = HPS::LinePattern::Cap::Square; enumStrings[1] = "Square";
			enumValues[2] = HPS::LinePattern::Cap::Round; enumStrings[2] = "Round";
			enumValues[3] = HPS::LinePattern::Cap::Mitre; enumStrings[3] = "Mitre";
			InitializeEnumValues(enumValues, enumStrings);
		}
	};

	class LinePatternJustificationProperty : public BaseEnumProperty<HPS::LinePattern::Justification>
	{
	public:
		LinePatternJustificationProperty(
			HPS::LinePattern::Justification & enumValue,
			CString const & name = _T("Justification"))
			: BaseEnumProperty(name, enumValue)
		{
			EnumTypeArray enumValues(2); HPS::UTF8Array enumStrings(2);
			enumValues[0] = HPS::LinePattern::Justification::Center; enumStrings[0] = "Center";
			enumValues[1] = HPS::LinePattern::Justification::Stretch; enumStrings[1] = "Stretch";
			InitializeEnumValues(enumValues, enumStrings);
		}
	};

	class GlyphFillProperty : public BaseEnumProperty<HPS::Glyph::Fill>
	{
	public:
		GlyphFillProperty(
			HPS::Glyph::Fill & enumValue,
			CString const & name = _T("Fill"))
			: BaseEnumProperty(name, enumValue)
		{
			EnumTypeArray enumValues(4); HPS::UTF8Array enumStrings(4);
			enumValues[0] = HPS::Glyph::Fill::None; enumStrings[0] = "None";
			enumValues[1] = HPS::Glyph::Fill::Continuous; enumStrings[1] = "Continuous";
			enumValues[2] = HPS::Glyph::Fill::New; enumStrings[2] = "New";
			enumValues[3] = HPS::Glyph::Fill::NewLoop; enumStrings[3] = "NewLoop";
			InitializeEnumValues(enumValues, enumStrings);
		}
	};

	class ConditionIntrinsicProperty : public BaseEnumProperty<HPS::Condition::Intrinsic>
	{
	public:
		ConditionIntrinsicProperty(
			HPS::Condition::Intrinsic & enumValue,
			CString const & name = _T("Intrinsic"))
			: BaseEnumProperty(name, enumValue)
		{
			EnumTypeArray enumValues(6); HPS::UTF8Array enumStrings(6);
			enumValues[0] = HPS::Condition::Intrinsic::Extent; enumStrings[0] = "Extent";
			enumValues[1] = HPS::Condition::Intrinsic::DrawPass; enumStrings[1] = "DrawPass";
			enumValues[2] = HPS::Condition::Intrinsic::InnerPixelWidth; enumStrings[2] = "InnerPixelWidth";
			enumValues[3] = HPS::Condition::Intrinsic::InnerPixelHeight; enumStrings[3] = "InnerPixelHeight";
			enumValues[4] = HPS::Condition::Intrinsic::Selection; enumStrings[4] = "Selection";
			enumValues[5] = HPS::Condition::Intrinsic::QuickMovesProbe; enumStrings[5] = "QuickMovesProbe";
			InitializeEnumValues(enumValues, enumStrings);
		}
	};

	class GridTypeProperty : public BaseEnumProperty<HPS::Grid::Type>
	{
	public:
		GridTypeProperty(
			HPS::Grid::Type & enumValue,
			CString const & name = _T("Type"))
			: BaseEnumProperty(name, enumValue)
		{
			EnumTypeArray enumValues(2); HPS::UTF8Array enumStrings(2);
			enumValues[0] = HPS::Grid::Type::Quadrilateral; enumStrings[0] = "Quadrilateral";
			enumValues[1] = HPS::Grid::Type::Radial; enumStrings[1] = "Radial";
			InitializeEnumValues(enumValues, enumStrings);
		}
	};

	class GPUPreferenceProperty : public BaseEnumProperty<HPS::GPU::Preference>
	{
	public:
		GPUPreferenceProperty(
			HPS::GPU::Preference & enumValue,
			CString const & name = _T("Preference"))
			: BaseEnumProperty(name, enumValue)
		{
			EnumTypeArray enumValues(4); HPS::UTF8Array enumStrings(4);
			enumValues[0] = HPS::GPU::Preference::HighPerformance; enumStrings[0] = "HighPerformance";
			enumValues[1] = HPS::GPU::Preference::Integrated; enumStrings[1] = "Integrated";
			enumValues[2] = HPS::GPU::Preference::Specific; enumStrings[2] = "Specific";
			enumValues[3] = HPS::GPU::Preference::Default; enumStrings[3] = "Default";
			InitializeEnumValues(enumValues, enumStrings);
		}
	};

	class DriverEventStereoMatrixProperty : public BaseEnumProperty<HPS::DriverEvent::StereoMatrix>
	{
	public:
		DriverEventStereoMatrixProperty(
			HPS::DriverEvent::StereoMatrix & enumValue,
			CString const & name = _T("StereoMatrix"))
			: BaseEnumProperty(name, enumValue)
		{
			EnumTypeArray enumValues(4); HPS::UTF8Array enumStrings(4);
			enumValues[0] = HPS::DriverEvent::StereoMatrix::ViewLeft; enumStrings[0] = "ViewLeft";
			enumValues[1] = HPS::DriverEvent::StereoMatrix::ViewRight; enumStrings[1] = "ViewRight";
			enumValues[2] = HPS::DriverEvent::StereoMatrix::ProjectionLeft; enumStrings[2] = "ProjectionLeft";
			enumValues[3] = HPS::DriverEvent::StereoMatrix::ProjectionRight; enumStrings[3] = "ProjectionRight";
			InitializeEnumValues(enumValues, enumStrings);
		}
	};

	class DrawWindowEventBackgroundTextureFormatProperty : public BaseEnumProperty<HPS::DrawWindowEvent::BackgroundTextureFormat>
	{
	public:
		DrawWindowEventBackgroundTextureFormatProperty(
			HPS::DrawWindowEvent::BackgroundTextureFormat & enumValue,
			CString const & name = _T("BackgroundTextureFormat"))
			: BaseEnumProperty(name, enumValue)
		{
			EnumTypeArray enumValues(4); HPS::UTF8Array enumStrings(4);
			enumValues[0] = HPS::DrawWindowEvent::BackgroundTextureFormat::RGBA; enumStrings[0] = "RGBA";
			enumValues[1] = HPS::DrawWindowEvent::BackgroundTextureFormat::BGRA; enumStrings[1] = "BGRA";
			enumValues[2] = HPS::DrawWindowEvent::BackgroundTextureFormat::ImageExternal; enumStrings[2] = "ImageExternal";
			enumValues[3] = HPS::DrawWindowEvent::BackgroundTextureFormat::LumaChromaPair; enumStrings[3] = "LumaChromaPair";
			InitializeEnumValues(enumValues, enumStrings);
		}
	};

	class ShaderParameterizationProperty : public BaseEnumProperty<HPS::Shader::Parameterization>
	{
	public:
		ShaderParameterizationProperty(
			HPS::Shader::Parameterization & enumValue,
			CString const & name = _T("Parameterization"))
			: BaseEnumProperty(name, enumValue)
		{
			EnumTypeArray enumValues(9); HPS::UTF8Array enumStrings(9);
			enumValues[0] = HPS::Shader::Parameterization::Cylinder; enumStrings[0] = "Cylinder";
			enumValues[1] = HPS::Shader::Parameterization::PhysicalReflection; enumStrings[1] = "PhysicalReflection";
			enumValues[2] = HPS::Shader::Parameterization::Object; enumStrings[2] = "Object";
			enumValues[3] = HPS::Shader::Parameterization::NaturalUV; enumStrings[3] = "NaturalUV";
			enumValues[4] = HPS::Shader::Parameterization::ReflectionVector; enumStrings[4] = "ReflectionVector";
			enumValues[5] = HPS::Shader::Parameterization::SurfaceNormal; enumStrings[5] = "SurfaceNormal";
			enumValues[6] = HPS::Shader::Parameterization::Sphere; enumStrings[6] = "Sphere";
			enumValues[7] = HPS::Shader::Parameterization::UV; enumStrings[7] = "UV";
			enumValues[8] = HPS::Shader::Parameterization::World; enumStrings[8] = "World";
			InitializeEnumValues(enumValues, enumStrings);
		}
	};

	class HardcopyBackgroundPreferenceProperty : public BaseEnumProperty<HPS::Hardcopy::BackgroundPreference>
	{
	public:
		HardcopyBackgroundPreferenceProperty(
			HPS::Hardcopy::BackgroundPreference & enumValue,
			CString const & name = _T("BackgroundPreference"))
			: BaseEnumProperty(name, enumValue)
		{
			EnumTypeArray enumValues(2); HPS::UTF8Array enumStrings(2);
			enumValues[0] = HPS::Hardcopy::BackgroundPreference::UseBackgroundColor; enumStrings[0] = "UseBackgroundColor";
			enumValues[1] = HPS::Hardcopy::BackgroundPreference::ForceSolidWhite; enumStrings[1] = "ForceSolidWhite";
			InitializeEnumValues(enumValues, enumStrings);
		}
	};

	class HardcopyRenderingAlgorithmProperty : public BaseEnumProperty<HPS::Hardcopy::RenderingAlgorithm>
	{
	public:
		HardcopyRenderingAlgorithmProperty(
			HPS::Hardcopy::RenderingAlgorithm & enumValue,
			CString const & name = _T("RenderingAlgorithm"))
			: BaseEnumProperty(name, enumValue)
		{
			EnumTypeArray enumValues(2); HPS::UTF8Array enumStrings(2);
			enumValues[0] = HPS::Hardcopy::RenderingAlgorithm::TwoPassPrint; enumStrings[0] = "TwoPassPrint";
			enumValues[1] = HPS::Hardcopy::RenderingAlgorithm::SinglePassPrint; enumStrings[1] = "SinglePassPrint";
			InitializeEnumValues(enumValues, enumStrings);
		}
	};

	class HardcopyPDFFontPreferenceProperty : public BaseEnumProperty<HPS::Hardcopy::PDFFontPreference>
	{
	public:
		HardcopyPDFFontPreferenceProperty(
			HPS::Hardcopy::PDFFontPreference & enumValue,
			CString const & name = _T("PDFFontPreference"))
			: BaseEnumProperty(name, enumValue)
		{
			EnumTypeArray enumValues(2); HPS::UTF8Array enumStrings(2);
			enumValues[0] = HPS::Hardcopy::PDFFontPreference::DoNotEmbedFonts; enumStrings[0] = "DoNotEmbedFonts";
			enumValues[1] = HPS::Hardcopy::PDFFontPreference::EmbedFonts; enumStrings[1] = "EmbedFonts";
			InitializeEnumValues(enumValues, enumStrings);
		}
	};

	class SimpleMaterialTypeProperty : public BaseEnumProperty<HPS::Material::Type>
	{
	private:
		enum PropertyTypeIndex
		{
			TypePropertyIndex = 0,
			RedPropertyIndex,
			GreenPropertyIndex,
			BluePropertyIndex,
			AlphaPropertyIndex,
			IndexPropertyIndex
		};

	public:
		SimpleMaterialTypeProperty(
			HPS::Material::Type & type)
			: BaseEnumProperty(_T("Type"), type)
		{
			HPS::MaterialTypeArray enumValues(2); HPS::UTF8Array enumStrings(2);
			enumValues[0] = HPS::Material::Type::RGBAColor; enumStrings[0] = "RGBAColor";
			enumValues[1] = HPS::Material::Type::MaterialIndex; enumStrings[1] = "Material Index";
			InitializeEnumValues(enumValues, enumStrings);
		}

		void EnableValidProperties() override
		{
			CMFCPropertyGridProperty * parent = GetParent();
			auto redSibling = static_cast<BaseProperty *>(parent->GetSubItem(RedPropertyIndex));
			auto greenSibling = static_cast<BaseProperty *>(parent->GetSubItem(GreenPropertyIndex));
			auto blueSibling = static_cast<BaseProperty *>(parent->GetSubItem(BluePropertyIndex));
			auto alphaSibling = static_cast<BaseProperty *>(parent->GetSubItem(AlphaPropertyIndex));
			auto indexSibling = static_cast<BaseProperty *>(parent->GetSubItem(IndexPropertyIndex));
			if (enumValue == HPS::Material::Type::RGBAColor)
			{
				redSibling->Enable(TRUE);
				greenSibling->Enable(TRUE);
				blueSibling->Enable(TRUE);
				alphaSibling->Enable(TRUE);
				indexSibling->Enable(FALSE);
			}
			else if (enumValue == HPS::Material::Type::MaterialIndex)
			{
				redSibling->Enable(FALSE);
				greenSibling->Enable(FALSE);
				blueSibling->Enable(FALSE);
				alphaSibling->Enable(FALSE);
				indexSibling->Enable(TRUE);
			}
		}
	};

	template <
		typename Kit,
		bool (Kit::*ShowFunction)(HPS::Material::Type &, HPS::RGBAColor &, float &) const,
		Kit & (Kit::*SetColorFunction)(HPS::RGBAColor const &),
		Kit & (Kit::*SetIndexFunction)(float),
		Kit & (Kit::*UnsetFunction)()
	>
	class SimpleMaterialProperty : public SettableProperty
	{
	public:
		SimpleMaterialProperty(
			CString const & name,
			Kit & kit)
			: SettableProperty(name)
			, kit(kit)
		{
			bool isSet = (this->kit.*ShowFunction)(type, color, materialIndex);
			if (isSet)
			{
				if (type == HPS::Material::Type::RGBAColor)
					materialIndex = 0;
				else if (type == HPS::Material::Type::MaterialIndex)
					color = HPS::RGBAColor::Black();
			}
			else
			{
				type = HPS::Material::Type::RGBAColor;
				color = HPS::RGBAColor::Black();
				materialIndex = 0;
			}

			auto typeChild = new SimpleMaterialTypeProperty(type);
			AddSubItem(typeChild);

			AddSubItem(new UnitFloatProperty(_T("Red"), color.red));
			AddSubItem(new UnitFloatProperty(_T("Green"), color.green));
			AddSubItem(new UnitFloatProperty(_T("Blue"), color.blue));
			AddSubItem(new UnitFloatProperty(_T("Alpha"), color.alpha));
			AddSubItem(new UnsignedFloatProperty(_T("Material Index"), materialIndex));

			typeChild->EnableValidProperties();

			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			if (type == HPS::Material::Type::RGBAColor)
				(kit.*SetColorFunction)(color);
			else if (type == HPS::Material::Type::MaterialIndex)
				(kit.*SetIndexFunction)(materialIndex);
		}

		void Unset() override
		{
			(kit.*UnsetFunction)();
		}

	private:
		Kit & kit;
		HPS::Material::Type type;
		HPS::RGBAColor color;
		float materialIndex;
	};

	typedef SimpleMaterialProperty <
		HPS::MaterialMappingKit,
		&HPS::MaterialMappingKit::ShowLineColor,
		&HPS::MaterialMappingKit::SetLineColor,
		&HPS::MaterialMappingKit::SetLineMaterialByIndex,
		&HPS::MaterialMappingKit::UnsetLineColor
	> BaseLineColorProperty;
	class LineColorProperty : public BaseLineColorProperty
	{
	public:
		LineColorProperty(
			HPS::MaterialMappingKit & materialMapping)
			: BaseLineColorProperty(_T("LineColor"), materialMapping)
		{}
	};

	typedef SimpleMaterialProperty <
		HPS::MaterialMappingKit,
		&HPS::MaterialMappingKit::ShowTextColor,
		&HPS::MaterialMappingKit::SetTextColor,
		&HPS::MaterialMappingKit::SetTextMaterialByIndex,
		&HPS::MaterialMappingKit::UnsetTextColor
	> BaseTextColorProperty;
	class TextColorProperty : public BaseTextColorProperty
	{
	public:
		TextColorProperty(
			HPS::MaterialMappingKit & materialMapping)
			: BaseTextColorProperty(_T("TextColor"), materialMapping)
		{}
	};

	typedef SimpleMaterialProperty <
		HPS::MaterialMappingKit,
		&HPS::MaterialMappingKit::ShowMarkerColor,
		&HPS::MaterialMappingKit::SetMarkerColor,
		&HPS::MaterialMappingKit::SetMarkerMaterialByIndex,
		&HPS::MaterialMappingKit::UnsetMarkerColor
	> BaseMarkerColorProperty;
	class MarkerColorProperty : public BaseMarkerColorProperty
	{
	public:
		MarkerColorProperty(
			HPS::MaterialMappingKit & materialMapping)
			: BaseMarkerColorProperty(_T("MarkerColor"), materialMapping)
		{}
	};

	typedef SimpleMaterialProperty <
		HPS::MaterialMappingKit,
		&HPS::MaterialMappingKit::ShowWindowColor,
		&HPS::MaterialMappingKit::SetWindowColor,
		&HPS::MaterialMappingKit::SetWindowMaterialByIndex,
		&HPS::MaterialMappingKit::UnsetWindowColor
	> BaseWindowColorProperty;
	class WindowColorProperty : public BaseWindowColorProperty
	{
	public:
		WindowColorProperty(
			HPS::MaterialMappingKit & materialMapping)
			: BaseWindowColorProperty(_T("WindowColor"), materialMapping)
		{}
	};

	typedef SimpleMaterialProperty <
		HPS::MaterialMappingKit,
		&HPS::MaterialMappingKit::ShowWindowContrastColor,
		&HPS::MaterialMappingKit::SetWindowContrastColor,
		&HPS::MaterialMappingKit::SetWindowContrastMaterialByIndex,
		&HPS::MaterialMappingKit::UnsetWindowContrastColor
	> BaseWindowContrastColorProperty;
	class WindowContrastColorProperty : public BaseWindowContrastColorProperty
	{
	public:
		WindowContrastColorProperty(
			HPS::MaterialMappingKit & materialMapping)
			: BaseWindowContrastColorProperty(_T("WindowContrastColor"), materialMapping)
		{}
	};

	typedef SimpleMaterialProperty <
		HPS::MaterialMappingKit,
		&HPS::MaterialMappingKit::ShowLightColor,
		&HPS::MaterialMappingKit::SetLightColor,
		&HPS::MaterialMappingKit::SetLightMaterialByIndex,
		&HPS::MaterialMappingKit::UnsetLightColor
	> BaseLightColorProperty;
	class LightColorProperty : public BaseLightColorProperty
	{
	public:
		LightColorProperty(
			HPS::MaterialMappingKit & materialMapping)
			: BaseLightColorProperty(_T("LightColor"), materialMapping)
		{}
	};

	typedef SimpleMaterialProperty <
		HPS::MaterialMappingKit,
		&HPS::MaterialMappingKit::ShowCutEdgeColor,
		&HPS::MaterialMappingKit::SetCutEdgeColor,
		&HPS::MaterialMappingKit::SetCutEdgeMaterialByIndex,
		&HPS::MaterialMappingKit::UnsetCutEdgeColor
	> BaseCutEdgeColorProperty;
	class CutEdgeColorProperty : public BaseCutEdgeColorProperty
	{
	public:
		CutEdgeColorProperty(
			HPS::MaterialMappingKit & materialMapping)
			: BaseCutEdgeColorProperty(_T("CutEdgeColor"), materialMapping)
		{}
	};

	typedef SimpleMaterialProperty <
		HPS::MaterialMappingKit,
		&HPS::MaterialMappingKit::ShowAmbientLightUpColor,
		&HPS::MaterialMappingKit::SetAmbientLightUpColor,
		&HPS::MaterialMappingKit::SetAmbientLightUpMaterialByIndex,
		&HPS::MaterialMappingKit::UnsetAmbientLightUpColor
	> BaseAmbientLightUpColorProperty;
	class AmbientLightUpColorProperty : public BaseAmbientLightUpColorProperty
	{
	public:
		AmbientLightUpColorProperty(
			HPS::MaterialMappingKit & materialMapping)
			: BaseAmbientLightUpColorProperty(_T("AmbientLightUpColor"), materialMapping)
		{}
	};

	typedef SimpleMaterialProperty <
		HPS::MaterialMappingKit,
		&HPS::MaterialMappingKit::ShowAmbientLightDownColor,
		&HPS::MaterialMappingKit::SetAmbientLightDownColor,
		&HPS::MaterialMappingKit::SetAmbientLightDownMaterialByIndex,
		&HPS::MaterialMappingKit::UnsetAmbientLightDownColor
	> BaseAmbientLightDownColorProperty;
	class AmbientLightDownColorProperty : public BaseAmbientLightDownColorProperty
	{
	public:
		AmbientLightDownColorProperty(
			HPS::MaterialMappingKit & materialMapping)
			: BaseAmbientLightDownColorProperty(_T("AmbientLightDownColor"), materialMapping)
		{}
	};

	typedef SimpleMaterialProperty <
		HPS::TextKit,
		&HPS::TextKit::ShowColor,
		&HPS::TextKit::SetColor,
		&HPS::TextKit::SetColorByIndex,
		&HPS::TextKit::UnsetColor
	> BaseTextKitColorProperty;
	class TextKitColorProperty : public BaseTextKitColorProperty
	{
	public:
		TextKitColorProperty(
			HPS::TextKit & kit)
			: BaseTextKitColorProperty(_T("Color"), kit)
		{}
	};

	typedef SimpleMaterialProperty <
		HPS::DistantLightKit,
		&HPS::DistantLightKit::ShowColor,
		&HPS::DistantLightKit::SetColor,
		&HPS::DistantLightKit::SetColorByIndex,
		&HPS::DistantLightKit::UnsetColor
	> BaseDistantLightKitColorProperty;
	class DistantLightKitColorProperty : public BaseDistantLightKitColorProperty
	{
	public:
		DistantLightKitColorProperty(
			HPS::DistantLightKit & kit)
			: BaseDistantLightKitColorProperty(_T("Color"), kit)
		{}
	};

	typedef SimpleMaterialProperty <
		HPS::SpotlightKit,
		&HPS::SpotlightKit::ShowColor,
		&HPS::SpotlightKit::SetColor,
		&HPS::SpotlightKit::SetColorByIndex,
		&HPS::SpotlightKit::UnsetColor
	> BaseSpotlightKitColorProperty;
	class SpotlightKitColorProperty : public BaseSpotlightKitColorProperty
	{
	public:
		SpotlightKitColorProperty(
			HPS::SpotlightKit & kit)
			: BaseSpotlightKitColorProperty(_T("Color"), kit)
		{}
	};

	class DiffuseColorTypeProperty : public BaseEnumProperty<HPS::Material::Type>
	{
	private:
		enum PropertyTypeIndex
		{
			TypePropertyIndex = 0,
			RedPropertyIndex,
			GreenPropertyIndex,
			BluePropertyIndex,
			AlphaPropertyIndex
		};

	public:
		DiffuseColorTypeProperty(
			HPS::Material::Type & type)
			: BaseEnumProperty(_T("Type"), type)
		{
			HPS::MaterialTypeArray enumValues(3); HPS::UTF8Array enumStrings(3);
			enumValues[0] = HPS::Material::Type::RGBAColor; enumStrings[0] = "RGBAColor";
			enumValues[1] = HPS::Material::Type::RGBColor; enumStrings[1] = "RGBColor";
			enumValues[2] = HPS::Material::Type::DiffuseChannelAlpha; enumStrings[2] = "Alpha";
			InitializeEnumValues(enumValues, enumStrings);
		}

		void EnableValidProperties() override
		{
			CMFCPropertyGridProperty * parent = GetParent();
			auto redSibling = static_cast<BaseProperty *>(parent->GetSubItem(RedPropertyIndex));
			auto greenSibling = static_cast<BaseProperty *>(parent->GetSubItem(GreenPropertyIndex));
			auto blueSibling = static_cast<BaseProperty *>(parent->GetSubItem(BluePropertyIndex));
			auto alphaSibling = static_cast<BaseProperty *>(parent->GetSubItem(AlphaPropertyIndex));
			if (enumValue == HPS::Material::Type::RGBAColor)
			{
				redSibling->Enable(TRUE);
				greenSibling->Enable(TRUE);
				blueSibling->Enable(TRUE);
				alphaSibling->Enable(TRUE);
			}
			else if (enumValue == HPS::Material::Type::RGBColor)
			{
				redSibling->Enable(TRUE);
				greenSibling->Enable(TRUE);
				blueSibling->Enable(TRUE);
				alphaSibling->Enable(FALSE);
			}
			else if (enumValue == HPS::Material::Type::DiffuseChannelAlpha)
			{
				redSibling->Enable(FALSE);
				greenSibling->Enable(FALSE);
				blueSibling->Enable(FALSE);
				alphaSibling->Enable(TRUE);
			}
		}
	};

	class DiffuseColorProperty : public SettableProperty
	{
	public:
		DiffuseColorProperty(
			HPS::MaterialKit & material)
			: SettableProperty(_T("Diffuse Color"))
			, material(material)
		{
			bool isSet = false;
			if (this->material.ShowDiffuseColor(rgbColor))
			{
				isSet = true;
				type = HPS::Material::Type::RGBColor;
				alpha = 1;
			}
			if (this->material.ShowDiffuseAlpha(alpha))
			{
				isSet = true;
				if (type == HPS::Material::Type::RGBColor)
					type = HPS::Material::Type::RGBAColor;
				else
				{
					type = HPS::Material::Type::DiffuseChannelAlpha;
					rgbColor = HPS::RGBColor::Black();
				}
			}

			if (!isSet)
			{
				type = HPS::Material::Type::RGBAColor;
				rgbColor = HPS::RGBColor::Black();
				alpha = 1;
			}

			auto typeChild = new DiffuseColorTypeProperty(type);
			AddSubItem(typeChild);

			AddSubItem(new UnitFloatProperty(_T("Red"), rgbColor.red));
			AddSubItem(new UnitFloatProperty(_T("Green"), rgbColor.green));
			AddSubItem(new UnitFloatProperty(_T("Blue"), rgbColor.blue));
			AddSubItem(new UnitFloatProperty(_T("Alpha"), alpha));

			typeChild->EnableValidProperties();

			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			// Unset the diffuse color first because we don't want to have multiple sets accumulate.
			// E.g., if diffuse RGB gets set, then the type is switched to alpha and the alpha
			// gets set, we should end up with *only* the alpha set, not the RGB + alpha set.

			if (type == HPS::Material::Type::RGBAColor)
				material.UnsetDiffuseColor().SetDiffuseColor(HPS::RGBAColor(rgbColor, alpha));
			else if (type == HPS::Material::Type::RGBColor)
				material.UnsetDiffuseColor().SetDiffuseColor(rgbColor);
			else if (type == HPS::Material::Type::DiffuseChannelAlpha)
				material.UnsetDiffuseColor().SetDiffuseAlpha(alpha);
		}

		void Unset() override
		{
			material.UnsetDiffuseColor();
		}

	private:
		HPS::MaterialKit & material;
		HPS::Material::Type type;
		HPS::RGBColor rgbColor;
		float alpha;
	};

	class DiffuseTextureLayerTypeProperty : public BaseEnumProperty<HPS::Material::Type>
	{
	private:
		enum PropertyTypeIndex
		{
			TypePropertyIndex = 0,
			TexturePropertyIndex,
			ColorPropertyIndex,
		};

	public:
		DiffuseTextureLayerTypeProperty(
			HPS::Material::Type & type)
			: BaseEnumProperty(_T("Type"), type)
		{
			HPS::MaterialTypeArray enumValues(3); HPS::UTF8Array enumStrings(3);
			enumValues[0] = HPS::Material::Type::None; enumStrings[0] = "None";
			enumValues[1] = HPS::Material::Type::TextureName; enumStrings[1] = "Texture";
			enumValues[2] = HPS::Material::Type::ModulatedTexture; enumStrings[2] = "Modulated Texture";
			InitializeEnumValues(enumValues, enumStrings);
		}

		void EnableValidProperties() override
		{
			CMFCPropertyGridProperty * parent = GetParent();
			auto textureSibling = static_cast<BaseProperty *>(parent->GetSubItem(TexturePropertyIndex));
			auto colorSibling = static_cast<BaseProperty *>(parent->GetSubItem(ColorPropertyIndex));
			if (enumValue == HPS::Material::Type::None)
			{
				textureSibling->Enable(FALSE);
				colorSibling->Enable(FALSE);
			}
			else if (enumValue == HPS::Material::Type::TextureName)
			{
				textureSibling->Enable(TRUE);
				colorSibling->Enable(FALSE);
			}
			else if (enumValue == HPS::Material::Type::ModulatedTexture)
			{
				textureSibling->Enable(TRUE);
				colorSibling->Enable(TRUE);
			}
		}
	};

	class DiffuseTextureLayerProperty : public BaseProperty
	{
	public:
		DiffuseTextureLayerProperty(
			unsigned int layer,
			HPS::Material::Type & type,
			HPS::RGBAColor & modulationColor,
			HPS::UTF8 & textureName)
			: BaseProperty(_T(""))
			, type(type)
			, modulationColor(modulationColor)
			, textureName(textureName)
		{
			std::wstring name = L"Layer " + std::to_wstring(layer);
			SetName(name.c_str());

			auto typeChild = new DiffuseTextureLayerTypeProperty(type);
			AddSubItem(typeChild);

			AddSubItem(new UTF8Property(_T("Texture"), textureName));
			AddSubItem(new RGBAColorProperty(_T("Modulation Color"), modulationColor));

			typeChild->EnableValidProperties();
		}

	private:
		HPS::Material::Type & type;
		HPS::RGBAColor & modulationColor;
		HPS::UTF8 & textureName;
	};

	class DiffuseTextureProperty : public SettableArrayProperty
	{
	public:
		DiffuseTextureProperty(
			HPS::MaterialKit & material)
			: SettableArrayProperty(_T("Diffuse Texture"))
			, material(material)
		{
			bool isSet = this->material.ShowDiffuseTexture(types, modulationColors, textureNames);

			if (isSet)
			{
				layerCount = static_cast<unsigned int>(types.size());
				for (unsigned int layer = 0; layer < layerCount; ++layer)
				{
					if (types[layer] == HPS::Material::Type::None)
					{
						modulationColors[layer] = HPS::RGBAColor::Black();
						textureNames[layer] = "texture";
					}
					else if (types[layer] == HPS::Material::Type::TextureName)
						modulationColors[layer] = HPS::RGBAColor::Black();
				}
			}
			else
			{
				layerCount = 1;
				ResizeArrays();
			}

			AddSubItem(new ArraySizeProperty(_T("Layer Count"), layerCount, 1, 8));
			AddItems();

			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			if (layerCount < 1)
				return;

			AddOrDeleteItems(layerCount, static_cast<unsigned int>(types.size()));

			material.UnsetDiffuseTexture();
			for (size_t layer = 0; layer < types.size(); ++layer)
			{
				HPS::Material::Type layerType = types[layer];
				if (layerType == HPS::Material::Type::None)
					continue;

				if (layerType == HPS::Material::Type::TextureName)
					material.SetDiffuseTexture(textureNames[layer], layer);
				else if (layerType == HPS::Material::Type::ModulatedTexture)
					material.SetDiffuseTexture(textureNames[layer], modulationColors[layer], layer);
				else
					ASSERT(0);
			}
		}

		void Unset() override
		{
			material.UnsetDiffuseTexture();
		}

		void ResizeArrays() override
		{
			types.resize(layerCount, HPS::Material::Type::TextureName);
			modulationColors.resize(layerCount, HPS::RGBAColor::Black());
			textureNames.resize(layerCount, "texture");
		}

		void AddItems() override
		{
			for (unsigned int layer = 0; layer < layerCount; ++layer)
			{
				auto newLayer = new DiffuseTextureLayerProperty(layer, types[layer], modulationColors[layer], textureNames[layer]);
				AddSubItem(newLayer);
			}
		}

	private:
		HPS::MaterialKit & material;
		unsigned int layerCount;
		HPS::MaterialTypeArray types;
		HPS::RGBAColorArray modulationColors;
		HPS::UTF8Array textureNames;
	};

	class EnvironmentTypeProperty : public BaseEnumProperty<HPS::Material::Type>
	{
	private:
		enum PropertyTypeIndex
		{
			TypePropertyIndex = 0,
			TexturePropertyIndex,
			CubeMapPropertyIndex,
			ColorPropertyIndex,
		};

	public:
		EnvironmentTypeProperty(
			HPS::Material::Type & type)
			: BaseEnumProperty(_T("Type"), type)
		{
			HPS::MaterialTypeArray enumValues(5); HPS::UTF8Array enumStrings(5);
			enumValues[0] = HPS::Material::Type::TextureName; enumStrings[0] = "Texture";
			enumValues[1] = HPS::Material::Type::ModulatedTexture; enumStrings[1] = "Modulated Texture";
			enumValues[2] = HPS::Material::Type::CubeMapName; enumStrings[2] = "Cube Map";
			enumValues[3] = HPS::Material::Type::ModulatedCubeMap; enumStrings[3] = "Modulated Cube Map";
			enumValues[4] = HPS::Material::Type::None; enumStrings[4] = "None";
			InitializeEnumValues(enumValues, enumStrings);
		}

		void EnableValidProperties() override
		{
			CMFCPropertyGridProperty * parent = GetParent();
			auto textureSibling = static_cast<BaseProperty *>(parent->GetSubItem(TexturePropertyIndex));
			auto cubeMapSibling = static_cast<BaseProperty *>(parent->GetSubItem(CubeMapPropertyIndex));
			auto colorSibling = static_cast<BaseProperty *>(parent->GetSubItem(ColorPropertyIndex));
			if (enumValue == HPS::Material::Type::None)
			{
				textureSibling->Enable(FALSE);
				cubeMapSibling->Enable(FALSE);
				colorSibling->Enable(FALSE);
			}
			else if (enumValue == HPS::Material::Type::TextureName)
			{
				textureSibling->Enable(TRUE);
				cubeMapSibling->Enable(FALSE);
				colorSibling->Enable(FALSE);
			}
			else if (enumValue == HPS::Material::Type::ModulatedTexture)
			{
				textureSibling->Enable(TRUE);
				cubeMapSibling->Enable(FALSE);
				colorSibling->Enable(TRUE);
			}
			else if (enumValue == HPS::Material::Type::CubeMapName)
			{
				textureSibling->Enable(FALSE);
				cubeMapSibling->Enable(TRUE);
				colorSibling->Enable(FALSE);
			}
			else if (enumValue == HPS::Material::Type::ModulatedCubeMap)
			{
				textureSibling->Enable(FALSE);
				cubeMapSibling->Enable(TRUE);
				colorSibling->Enable(TRUE);
			}
		}
	};

	class EnvironmentProperty : public SettableProperty
	{
	public:
		EnvironmentProperty(
			HPS::MaterialKit & material)
			: SettableProperty(_T("Environment"))
			, material(material)
		{
			HPS::UTF8 textureOrCubeMapName;
			bool isSet = this->material.ShowEnvironment(type, modulationColor, textureOrCubeMapName);
			if (isSet)
			{
				if (type == HPS::Material::Type::None)
				{
					textureName = "texture";
					cubeMapName = "cubemap";
					modulationColor = HPS::RGBAColor::Black();
				}
				else if (type == HPS::Material::Type::TextureName || type == HPS::Material::Type::ModulatedTexture)
				{
					textureName = textureOrCubeMapName;
					cubeMapName = "cubemap";
					if (type == HPS::Material::Type::TextureName)
						modulationColor = HPS::RGBAColor::Black();
				}
				else if (type == HPS::Material::Type::CubeMapName || type == HPS::Material::Type::ModulatedCubeMap)
				{
					cubeMapName = textureOrCubeMapName;
					textureName = "texture";
					if (type == HPS::Material::Type::CubeMapName)
						modulationColor = HPS::RGBAColor::Black();
				}
			}
			else
			{
				type = HPS::Material::Type::TextureName;
				textureName = "texture";
				cubeMapName = "cubemap";
				modulationColor = HPS::RGBAColor::Black();
			}

			auto typeChild = new EnvironmentTypeProperty(type);
			AddSubItem(typeChild);

			AddSubItem(new UTF8Property(_T("Texture"), textureName));
			AddSubItem(new UTF8Property(_T("CubeMap"), cubeMapName));
			AddSubItem(new RGBAColorProperty(_T("ModulationColor"), modulationColor));

			typeChild->EnableValidProperties();

			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			if (type == HPS::Material::Type::None)
				material.SetEnvironmentTexture();
			else if (type == HPS::Material::Type::TextureName)
				material.SetEnvironmentTexture(textureName);
			else if (type == HPS::Material::Type::ModulatedTexture)
				material.SetEnvironmentTexture(textureName, modulationColor);
			else if (type == HPS::Material::Type::CubeMapName)
				material.SetEnvironmentCubeMap(cubeMapName);
			else if (type == HPS::Material::Type::ModulatedCubeMap)
				material.SetEnvironmentCubeMap(cubeMapName, modulationColor);
		}

		void Unset() override
		{
			material.UnsetEnvironment();
		}

	private:
		HPS::MaterialKit & material;
		HPS::Material::Type type;
		HPS::RGBAColor modulationColor;
		HPS::UTF8 textureName;
		HPS::UTF8 cubeMapName;
	};

	class TransmissionTypeProperty : public BaseEnumProperty<HPS::Material::Type>
	{
	private:
		enum PropertyTypeIndex
		{
			TypePropertyIndex = 0,
			TexturePropertyIndex,
			ColorPropertyIndex,
		};

	public:
		TransmissionTypeProperty(
			HPS::Material::Type & type)
			: BaseEnumProperty(_T("Type"), type)
		{
			HPS::MaterialTypeArray enumValues(2); HPS::UTF8Array enumStrings(2);
			enumValues[0] = HPS::Material::Type::TextureName; enumStrings[0] = "Texture";
			enumValues[1] = HPS::Material::Type::ModulatedTexture; enumStrings[1] = "Modulated Texture";
			InitializeEnumValues(enumValues, enumStrings);
		}

		void EnableValidProperties() override
		{
			CMFCPropertyGridProperty * parent = GetParent();
			auto textureSibling = static_cast<BaseProperty *>(parent->GetSubItem(TexturePropertyIndex));
			auto colorSibling = static_cast<BaseProperty *>(parent->GetSubItem(ColorPropertyIndex));
			if (enumValue == HPS::Material::Type::TextureName)
			{
				textureSibling->Enable(TRUE);
				colorSibling->Enable(FALSE);
			}
			else if (enumValue == HPS::Material::Type::ModulatedTexture)
			{
				textureSibling->Enable(TRUE);
				colorSibling->Enable(TRUE);
			}
		}
	};

	class TransmissionProperty : public SettableProperty
	{
	public:
		TransmissionProperty(
			HPS::MaterialKit & material)
			: SettableProperty(_T("Transmission"))
			, material(material)
		{
			bool isSet = this->material.ShowTransmission(type, modulationColor, textureName);
			if (isSet)
			{
				if (type == HPS::Material::Type::TextureName)
					modulationColor = HPS::RGBAColor::Black();
			}
			else
			{
				type = HPS::Material::Type::TextureName;
				textureName = "texture";
				modulationColor = HPS::RGBAColor::Black();
			}

			auto typeChild = new TransmissionTypeProperty(type);
			AddSubItem(typeChild);

			AddSubItem(new UTF8Property(_T("Texture"), textureName));
			AddSubItem(new RGBAColorProperty(_T("Modulation Color"), modulationColor));

			typeChild->EnableValidProperties();

			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			if (type == HPS::Material::Type::TextureName)
				material.SetTransmission(textureName);
			else if (type == HPS::Material::Type::ModulatedTexture)
				material.SetTransmission(textureName, modulationColor);
		}

		void Unset() override
		{
			material.UnsetTransmission();
		}

	private:
		HPS::MaterialKit & material;
		HPS::Material::Type type;
		HPS::RGBAColor modulationColor;
		HPS::UTF8 textureName;
	};

	class ModulatedChannelTypeProperty : public BaseEnumProperty<HPS::Material::Type>
	{
	private:
		enum PropertyTypeIndex
		{
			TypePropertyIndex = 0,
			TexturePropertyIndex,
			ColorPropertyIndex,
		};

	public:
		ModulatedChannelTypeProperty(
			HPS::Material::Type & type)
			: BaseEnumProperty(_T("Type"), type)
		{
			HPS::MaterialTypeArray enumValues(3); HPS::UTF8Array enumStrings(3);
			enumValues[0] = HPS::Material::Type::RGBAColor; enumStrings[0] = "RGBAColor";
			enumValues[1] = HPS::Material::Type::TextureName; enumStrings[1] = "Texture";
			enumValues[2] = HPS::Material::Type::ModulatedTexture; enumStrings[2] = "Modulated Texture";
			InitializeEnumValues(enumValues, enumStrings);
		}

		void EnableValidProperties() override
		{
			CMFCPropertyGridProperty * parent = GetParent();
			auto textureSibling = static_cast<BaseProperty *>(parent->GetSubItem(TexturePropertyIndex));
			auto colorSibling = static_cast<BaseProperty *>(parent->GetSubItem(ColorPropertyIndex));
			if (enumValue == HPS::Material::Type::RGBAColor)
			{
				textureSibling->Enable(FALSE);
				colorSibling->Enable(TRUE);
			}
			else if (enumValue == HPS::Material::Type::TextureName)
			{
				textureSibling->Enable(TRUE);
				colorSibling->Enable(FALSE);
			}
			else if (enumValue == HPS::Material::Type::ModulatedTexture)
			{
				textureSibling->Enable(TRUE);
				colorSibling->Enable(TRUE);
			}
		}
	};

	template <
		bool (HPS::MaterialKit::*ShowFunction)(HPS::Material::Type &, HPS::RGBAColor &, HPS::UTF8 &) const,
		HPS::MaterialKit & (HPS::MaterialKit::*SetColorFunction)(HPS::RGBAColor const &),
		HPS::MaterialKit & (HPS::MaterialKit::*SetTextureFunction)(char const *),
		HPS::MaterialKit & (HPS::MaterialKit::*SetModulatedTextureFunction)(char const *, HPS::RGBAColor const &),
		HPS::MaterialKit & (HPS::MaterialKit::*UnsetFunction)()
	>
	class ModulatedChannelProperty : public SettableProperty
	{
	public:
		ModulatedChannelProperty(
			CString const & name,
			HPS::MaterialKit & material)
			: SettableProperty(name)
			, material(material)
		{
			bool isSet = (this->material.*ShowFunction)(type, color, textureName);
			if (isSet)
			{
				if (type == HPS::Material::Type::RGBAColor)
					textureName = "texture";
				else if (type == HPS::Material::Type::TextureName)
					color = HPS::RGBAColor::Black();
			}
			else
			{
				type = HPS::Material::Type::RGBAColor;
				color = HPS::RGBAColor::Black();
				textureName = "texture";
			}

			auto typeChild = new ModulatedChannelTypeProperty(type);
			AddSubItem(typeChild);

			AddSubItem(new UTF8Property(_T("Texture"), textureName));
			AddSubItem(new RGBAColorProperty(_T("Color"), color));

			typeChild->EnableValidProperties();

			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			if (type == HPS::Material::Type::RGBAColor)
				(material.*SetColorFunction)(color);
			else if (type == HPS::Material::Type::TextureName)
				(material.*SetTextureFunction)(textureName);
			else if (type == HPS::Material::Type::ModulatedTexture)
				(material.*SetModulatedTextureFunction)(textureName, color);
		}

		void Unset() override
		{
			(material.*UnsetFunction)();
		}

	private:
		HPS::MaterialKit & material;
		HPS::Material::Type type;
		HPS::RGBAColor color;
		HPS::UTF8 textureName;
	};

	typedef ModulatedChannelProperty <
		&HPS::MaterialKit::ShowSpecular,
		&HPS::MaterialKit::SetSpecular,
		&HPS::MaterialKit::SetSpecular,
		&HPS::MaterialKit::SetSpecular,
		&HPS::MaterialKit::UnsetSpecular
	> SpecularPropertyBase;
	class SpecularProperty : public SpecularPropertyBase
	{
	public:
		SpecularProperty(
			HPS::MaterialKit & material)
			: SpecularPropertyBase(_T("Specular"), material)
		{}
	};

	typedef ModulatedChannelProperty <
		&HPS::MaterialKit::ShowMirror,
		&HPS::MaterialKit::SetMirror,
		&HPS::MaterialKit::SetMirror,
		&HPS::MaterialKit::SetMirror,
		&HPS::MaterialKit::UnsetMirror
	> MirrorPropertyBase;
	class MirrorProperty : public MirrorPropertyBase
	{
	public:
		MirrorProperty(
			HPS::MaterialKit & material)
			: MirrorPropertyBase(_T("Mirror"), material)
		{}
	};

	typedef ModulatedChannelProperty <
		&HPS::MaterialKit::ShowEmission,
		&HPS::MaterialKit::SetEmission,
		&HPS::MaterialKit::SetEmission,
		&HPS::MaterialKit::SetEmission,
		&HPS::MaterialKit::UnsetEmission
	> EmissionPropertyBase;
	class EmissionProperty : public EmissionPropertyBase
	{
	public:
		EmissionProperty(
			HPS::MaterialKit & material)
			: EmissionPropertyBase(_T("Emission"), material)
		{}
	};

	class BumpProperty : public SettableProperty
	{
	public:
		BumpProperty(
			HPS::MaterialKit & material)
			: SettableProperty(_T("Bump Texture"))
			, material(material)
		{
			bool isSet = material.ShowBump(textureName);
			if (!isSet)
				textureName = "texture";

			AddSubItem(new UTF8Property(_T("Name"), textureName));

			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			material.SetBump(textureName);
		}

		void Unset() override
		{
			material.UnsetBump();
		}

	private:
		HPS::MaterialKit & material;
		HPS::UTF8 textureName;
	};

	class GlossProperty : public SettableProperty
	{
	public:
		GlossProperty(
			HPS::MaterialKit & material)
			: SettableProperty(_T("Gloss"))
			, material(material)
			, glossValue(1)
		{
			bool isSet = material.ShowGloss(glossValue);

			AddSubItem(new FloatProperty(_T("Value"), glossValue));

			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			material.SetGloss(glossValue);
		}

		void Unset() override
		{
			material.UnsetGloss();
		}

	private:
		HPS::MaterialKit & material;
		float glossValue;
	};

	class ShaderProperty : public SettableProperty
	{
	public:
		ShaderProperty(
			HPS::MaterialKit & material)
			: SettableProperty(_T("Shader"))
			, material(material)
		{
			bool isSet = material.ShowShader(shaderName);
			if (!isSet)
				shaderName = "shader";

			AddSubItem(new UTF8Property(_T("Name"), shaderName));

			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			material.SetShader(shaderName);
		}

		void Unset() override
		{
			material.UnsetShader();
		}

	private:
		HPS::MaterialKit & material;
		HPS::UTF8 shaderName;
	};

	class MaterialProperty : public BaseProperty
	{
	public:
		MaterialProperty(
			CString const & name,
			HPS::MaterialKit & material)
			: BaseProperty(name)
			, material(material)
		{
			AddSubItem(new DiffuseColorProperty(material));
			AddSubItem(new DiffuseTextureProperty(material));
			AddSubItem(new EnvironmentProperty(material));
			AddSubItem(new SpecularProperty(material));
			AddSubItem(new MirrorProperty(material));
			AddSubItem(new EmissionProperty(material));
			AddSubItem(new TransmissionProperty(material));
			AddSubItem(new BumpProperty(material));
			AddSubItem(new GlossProperty(material));
			AddSubItem(new ShaderProperty(material));
		}

	private:
		HPS::MaterialKit & material;
	};

	class ComplexMaterialTypeProperty : public BaseEnumProperty<HPS::Material::Type>
	{
	private:
		enum PropertyTypeIndex
		{
			TypePropertyIndex = 0,
			MaterialPropertyIndex,
			IndexPropertyIndex
		};

	public:
		ComplexMaterialTypeProperty(
			HPS::Material::Type & type)
			: BaseEnumProperty(_T("Type"), type)
		{
			HPS::MaterialTypeArray enumValues(2); HPS::UTF8Array enumStrings(2);
			enumValues[0] = HPS::Material::Type::FullMaterial; enumStrings[0] = "Full Material";
			enumValues[1] = HPS::Material::Type::MaterialIndex; enumStrings[1] = "Material Index";
			InitializeEnumValues(enumValues, enumStrings);
		}

		void EnableValidProperties() override
		{
			CMFCPropertyGridProperty * parent = GetParent();
			auto materialSibling = static_cast<BaseProperty *>(parent->GetSubItem(MaterialPropertyIndex));
			auto indexSibling = static_cast<BaseProperty *>(parent->GetSubItem(IndexPropertyIndex));
			if (enumValue == HPS::Material::Type::FullMaterial)
			{
				materialSibling->Enable(TRUE);
				indexSibling->Enable(FALSE);
			}
			else if (enumValue == HPS::Material::Type::MaterialIndex)
			{
				materialSibling->Enable(FALSE);
				indexSibling->Enable(TRUE);
			}
		}
	};

	template <
		bool (HPS::MaterialMappingKit::*ShowFunction)(HPS::Material::Type &, HPS::MaterialKit &, float &) const,
		HPS::MaterialMappingKit & (HPS::MaterialMappingKit::*SetMaterialFunction)(HPS::MaterialKit const &),
		HPS::MaterialMappingKit & (HPS::MaterialMappingKit::*SetIndexFunction)(float),
		HPS::MaterialMappingKit & (HPS::MaterialMappingKit::*UnsetFunction)()
	>
	class ComplexMaterialProperty : public SettableProperty
	{
	public:
		ComplexMaterialProperty(
			CString const & name,
			HPS::MaterialMappingKit & materialMapping)
			: SettableProperty(name)
			, materialMapping(materialMapping)
		{
			bool isSet = (this->materialMapping.*ShowFunction)(type, material, materialIndex);
			if (isSet)
			{
				if (type == HPS::Material::Type::FullMaterial)
					materialIndex = 1;
			}
			else
			{
				type = HPS::Material::Type::FullMaterial;
				materialIndex = 1;
			}

			auto typeChild = new ComplexMaterialTypeProperty(type);
			AddSubItem(typeChild);

			AddSubItem(new MaterialProperty(_T("Material Kit"), material));
			AddSubItem(new FloatProperty(_T("Material Index"), materialIndex));

			typeChild->EnableValidProperties();

			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			if (type == HPS::Material::Type::FullMaterial)
				(materialMapping.*SetMaterialFunction)(material);
			else if (type == HPS::Material::Type::MaterialIndex)
				(materialMapping.*SetIndexFunction)(materialIndex);
		}

		void Unset() override
		{
			(materialMapping.*UnsetFunction)();
		}

	private:
		HPS::MaterialMappingKit & materialMapping;
		HPS::Material::Type type;
		HPS::MaterialKit material;
		float materialIndex;
	};

	typedef ComplexMaterialProperty <
		&HPS::MaterialMappingKit::ShowFrontFaceMaterial,
		&HPS::MaterialMappingKit::SetFrontFaceMaterial,
		&HPS::MaterialMappingKit::SetFrontFaceMaterialByIndex,
		&HPS::MaterialMappingKit::UnsetFrontFaceMaterial
	> BaseFrontFaceMaterialProperty;
	class FrontFaceMaterialProperty : public BaseFrontFaceMaterialProperty
	{
	public:
		FrontFaceMaterialProperty(
			HPS::MaterialMappingKit & materialMapping)
			: BaseFrontFaceMaterialProperty(_T("Front Face Material"), materialMapping)
		{}
	};

	typedef ComplexMaterialProperty <
		&HPS::MaterialMappingKit::ShowBackFaceMaterial,
		&HPS::MaterialMappingKit::SetBackFaceMaterial,
		&HPS::MaterialMappingKit::SetBackFaceMaterialByIndex,
		&HPS::MaterialMappingKit::UnsetBackFaceMaterial
	> BaseBackFaceMaterialProperty;
	class BackFaceMaterialProperty : public BaseBackFaceMaterialProperty
	{
	public:
		BackFaceMaterialProperty(
			HPS::MaterialMappingKit & materialMapping)
			: BaseBackFaceMaterialProperty(_T("Back Face Material"), materialMapping)
		{}
	};

	typedef ComplexMaterialProperty <
		&HPS::MaterialMappingKit::ShowEdgeMaterial,
		&HPS::MaterialMappingKit::SetEdgeMaterial,
		&HPS::MaterialMappingKit::SetEdgeMaterialByIndex,
		&HPS::MaterialMappingKit::UnsetEdgeMaterial
	> BaseEdgeMaterialProperty;
	class EdgeMaterialProperty : public BaseEdgeMaterialProperty
	{
	public:
		EdgeMaterialProperty(
			HPS::MaterialMappingKit & materialMapping)
			: BaseEdgeMaterialProperty(_T("Edge Material"), materialMapping)
		{}
	};

	typedef ComplexMaterialProperty <
		&HPS::MaterialMappingKit::ShowVertexMaterial,
		&HPS::MaterialMappingKit::SetVertexMaterial,
		&HPS::MaterialMappingKit::SetVertexMaterialByIndex,
		&HPS::MaterialMappingKit::UnsetVertexMaterial
	> BaseVertexMaterialProperty;
	class VertexMaterialProperty : public BaseVertexMaterialProperty
	{
	public:
		VertexMaterialProperty(
			HPS::MaterialMappingKit & materialMapping)
			: BaseVertexMaterialProperty(_T("Vertex Material"), materialMapping)
		{}
	};

	typedef ComplexMaterialProperty <
		&HPS::MaterialMappingKit::ShowCutFaceMaterial,
		&HPS::MaterialMappingKit::SetCutFaceMaterial,
		&HPS::MaterialMappingKit::SetCutFaceMaterialByIndex,
		&HPS::MaterialMappingKit::UnsetCutFaceMaterial
	> BaseCutFaceMaterialProperty;
	class CutFaceMaterialProperty : public BaseCutFaceMaterialProperty
	{
	public:
		CutFaceMaterialProperty(
			HPS::MaterialMappingKit & materialMapping)
			: BaseCutFaceMaterialProperty(_T("CutFace Material"), materialMapping)
		{}
	};

	class MaterialMappingKitProperty : public RootProperty
	{
	public:
		MaterialMappingKitProperty(
			CMFCPropertyGridCtrl & ctrl,
			HPS::SegmentKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.ShowMaterialMapping(kit);
			ctrl.AddProperty(new FrontFaceMaterialProperty(kit));
			ctrl.AddProperty(new BackFaceMaterialProperty(kit));
			ctrl.AddProperty(new EdgeMaterialProperty(kit));
			ctrl.AddProperty(new VertexMaterialProperty(kit));
			ctrl.AddProperty(new LineColorProperty(kit));
			ctrl.AddProperty(new TextColorProperty(kit));
			ctrl.AddProperty(new MarkerColorProperty(kit));
			ctrl.AddProperty(new LightColorProperty(kit));
			ctrl.AddProperty(new WindowColorProperty(kit));
			ctrl.AddProperty(new WindowContrastColorProperty(kit));
			ctrl.AddProperty(new AmbientLightUpColorProperty(kit));
			ctrl.AddProperty(new AmbientLightDownColorProperty(kit));
			ctrl.AddProperty(new CutFaceMaterialProperty(kit));
			ctrl.AddProperty(new CutEdgeColorProperty(kit));
		}

		void Apply() override
		{
			key.UnsetMaterialMapping();
			key.SetMaterialMapping(kit);
		}

	private:
		HPS::SegmentKey key;
		HPS::MaterialMappingKit kit;
	};

	template <typename Kit>
	class PriorityProperty : public SettableProperty
	{
	public:
		PriorityProperty(
			Kit & kit)
			: SettableProperty(_T("Priority"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowPriority(priority);
			if (!isSet)
				priority = 0;
			AddSubItem(new IntProperty(_T("Value"), priority));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetPriority(priority);
		}

		void Unset() override
		{
			kit.UnsetPriority();
		}

	private:
		Kit & kit;
		int priority;
	};

	typedef PriorityProperty<HPS::ShellKit> ShellKitPriorityProperty;
	typedef PriorityProperty<HPS::MeshKit> MeshKitPriorityProperty;
	typedef PriorityProperty<HPS::CylinderKit> CylinderKitPriorityProperty;
	typedef PriorityProperty<HPS::LineKit> LineKitPriorityProperty;
	typedef PriorityProperty<HPS::NURBSCurveKit> NURBSCurveKitPriorityProperty;
	typedef PriorityProperty<HPS::NURBSSurfaceKit> NURBSSurfaceKitPriorityProperty;
	typedef PriorityProperty<HPS::PolygonKit> PolygonKitPriorityProperty;
	typedef PriorityProperty<HPS::TextKit> TextKitPriorityProperty;
	typedef PriorityProperty<HPS::MarkerKit> MarkerKitPriorityProperty;
	typedef PriorityProperty<HPS::DistantLightKit> DistantLightKitPriorityProperty;
	typedef PriorityProperty<HPS::CuttingSectionKit> CuttingSectionKitPriorityProperty;
	typedef PriorityProperty<HPS::SphereKit> SphereKitPriorityProperty;
	typedef PriorityProperty<HPS::CircleKit> CircleKitPriorityProperty;
	typedef PriorityProperty<HPS::CircularArcKit> CircularArcKitPriorityProperty;
	typedef PriorityProperty<HPS::CircularWedgeKit> CircularWedgeKitPriorityProperty;
	typedef PriorityProperty<HPS::InfiniteLineKit> InfiniteLineKitPriorityProperty;
	typedef PriorityProperty<HPS::SpotlightKit> SpotlightKitPriorityProperty;
	typedef PriorityProperty<HPS::EllipseKit> EllipseKitPriorityProperty;
	typedef PriorityProperty<HPS::EllipticalArcKit> EllipticalArcKitPriorityProperty;
	typedef PriorityProperty<HPS::GridKit> GridKitPriorityProperty;

	class SingleUserDataProperty : public BaseProperty
	{
	public:
		SingleUserDataProperty(
			size_t index,
			intptr_t dataIndex,
			size_t dataByteCount)
			: BaseProperty(_T(""))
		{
			std::wstring name = L"Data " + std::to_wstring(index);
			SetName(name.c_str());

			AddSubItem(new ImmutableIntPtrTProperty(_T("Index"), dataIndex));
			AddSubItem(new ImmutableSizeTProperty(_T("Byte Count"), dataByteCount));
		}
	};

	template <typename Kit>
	class UserDataProperty : public SettableProperty
	{
	public:
		UserDataProperty(
			Kit & kit)
			: SettableProperty(_T("User Data"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowUserData(indices, data);
			if (isSet)
			{
				AddSubItem(new ImmutableSizeTProperty(_T("Count"), indices.size()));
				for (size_t i = 0; i < indices.size(); ++i)
					AddSubItem(new SingleUserDataProperty(i, indices[i], data[i].size()));
			}
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetUserData(indices, data);
		}

		void Unset() override
		{
			kit.UnsetAllUserData();
		}

	private:
		Kit & kit;
		HPS::IntPtrTArray indices;
		HPS::ByteArrayArray data;
	};

	typedef UserDataProperty<HPS::ShellKit> ShellKitUserDataProperty;
	typedef UserDataProperty<HPS::MeshKit> MeshKitUserDataProperty;
	typedef UserDataProperty<HPS::CylinderKit> CylinderKitUserDataProperty;
	typedef UserDataProperty<HPS::LineKit> LineKitUserDataProperty;
	typedef UserDataProperty<HPS::NURBSCurveKit> NURBSCurveKitUserDataProperty;
	typedef UserDataProperty<HPS::NURBSSurfaceKit> NURBSSurfaceKitUserDataProperty;
	typedef UserDataProperty<HPS::PolygonKit> PolygonKitUserDataProperty;
	typedef UserDataProperty<HPS::TextKit> TextKitUserDataProperty;
	typedef UserDataProperty<HPS::MarkerKit> MarkerKitUserDataProperty;
	typedef UserDataProperty<HPS::DistantLightKit> DistantLightKitUserDataProperty;
	typedef UserDataProperty<HPS::CuttingSectionKit> CuttingSectionKitUserDataProperty;
	typedef UserDataProperty<HPS::SphereKit> SphereKitUserDataProperty;
	typedef UserDataProperty<HPS::CircleKit> CircleKitUserDataProperty;
	typedef UserDataProperty<HPS::CircularArcKit> CircularArcKitUserDataProperty;
	typedef UserDataProperty<HPS::CircularWedgeKit> CircularWedgeKitUserDataProperty;
	typedef UserDataProperty<HPS::InfiniteLineKit> InfiniteLineKitUserDataProperty;
	typedef UserDataProperty<HPS::SpotlightKit> SpotlightKitUserDataProperty;
	typedef UserDataProperty<HPS::EllipseKit> EllipseKitUserDataProperty;
	typedef UserDataProperty<HPS::EllipticalArcKit> EllipticalArcKitUserDataProperty;
	typedef UserDataProperty<HPS::GridKit> GridKitUserDataProperty;

	class TextRotationProperty : public BaseEnumProperty<HPS::Text::Rotation>
	{
	private:
		enum PropertyTypeIndex
		{
			RotationPropertyIndex = 0,
			AnglePropertyIndex,
		};

	public:
		TextRotationProperty(
			HPS::Text::Rotation & enumValue)
			: BaseEnumProperty(_T("Rotation"), enumValue)
		{
			EnumTypeArray enumValues(3); HPS::UTF8Array enumStrings(3);
			enumValues[0] = HPS::Text::Rotation::None; enumStrings[0] = "None";
			enumValues[1] = HPS::Text::Rotation::Rotate; enumStrings[1] = "Rotate";
			enumValues[2] = HPS::Text::Rotation::FollowPath; enumStrings[2] = "FollowPath";
			InitializeEnumValues(enumValues, enumStrings);
		}

		void EnableValidProperties() override
		{
			CMFCPropertyGridProperty * parent = GetParent();
			auto angleSibling = static_cast<BaseProperty *>(parent)->GetSubItem(AnglePropertyIndex);
			if (enumValue == HPS::Text::Rotation::None || enumValue == HPS::Text::Rotation::FollowPath)
				angleSibling->Enable(FALSE);
			else if (enumValue == HPS::Text::Rotation::Rotate)
				angleSibling->Enable(TRUE);
		}
	};

	template <typename Kit>
	class RotationProperty : public SettableProperty
	{
	public:
		RotationProperty(
			Kit & kit)
			: SettableProperty(_T("Rotation"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowRotation(rotation, angle);
			if (isSet)
			{
				if (rotation != HPS::Text::Rotation::Rotate)
					angle = 0.0f;
			}
			else
			{
				rotation = HPS::Text::Rotation::FollowPath;
				angle = 0.0f;
			}
			auto rotationChild = new TextRotationProperty(rotation);
			AddSubItem(rotationChild);
			AddSubItem(new FloatProperty(_T("Angle"), angle));
			rotationChild->EnableValidProperties();
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetRotation(rotation, angle);
		}

		void Unset() override
		{
			kit.UnsetRotation();
		}

	private:
		Kit & kit;
		HPS::Text::Rotation rotation;
		float angle;
	};

	typedef RotationProperty<HPS::TextKit> TextKitRotationProperty;
	typedef RotationProperty<HPS::TextAttributeKit> TextAttributeKitRotationProperty;

	class SingleAttributeLockProperty : public BaseProperty
	{
	public:
		SingleAttributeLockProperty(
			unsigned int lock,
			HPS::AttributeLock::Type & type,
			bool & state)
			: BaseProperty(_T(""))
			, type(type)
			, state(state)
		{
			std::wstring name = L"Lock " + std::to_wstring(lock);
			SetName(name.c_str());

			AddSubItem(new AttributeLockTypeProperty(type));
			AddSubItem(new BoolProperty(_T("State"), state));
		}

	private:
		HPS::AttributeLock::Type & type;
		bool & state;
	};

	class AttributeLockKitLockProperty : public SettableArrayProperty
	{
	public:
		AttributeLockKitLockProperty(
			HPS::AttributeLockKit & kit)
			: SettableArrayProperty(_T("Locks"))
			, kit(kit)
		{
			HPS::BoolArray statesArray;
			bool isSet = this->kit.ShowLock(types, statesArray);

			if (isSet)
			{
				lockCount = static_cast<unsigned int>(types.size());
				states.assign(statesArray.begin(), statesArray.end());
			}
			else
			{
				lockCount = 1;
				ResizeArrays();
			}

			AddSubItem(new ArraySizeProperty(_T("Count"), lockCount));
			AddItems();

			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			if (lockCount < 1)
				return;

			AddOrDeleteItems(lockCount, static_cast<unsigned int>(types.size()));

			HPS::BoolArray statesArray(states.begin(), states.end());
			kit.UnsetLock(HPS::AttributeLock::Type::Everything).SetLock(types, statesArray);
		}

		void Unset() override
		{
			kit.UnsetLock(HPS::AttributeLock::Type::Everything);
		}

		void ResizeArrays() override
		{
			types.resize(lockCount, HPS::AttributeLock::Type::Everything);
			states.resize(lockCount, false);
		}

		void AddItems() override
		{
			for (unsigned int lock = 0; lock < lockCount; ++lock)
			{
				auto newLock = new SingleAttributeLockProperty(lock, types[lock], states[lock]);
				AddSubItem(newLock);
			}
		}

	private:
		HPS::AttributeLockKit & kit;
		unsigned int lockCount;
		HPS::AttributeLockTypeArray types;
		BoolDeque states;
	};

	class AttributeLockKitSubsegmentLockOverrideProperty : public SettableArrayProperty
	{
	public:
		AttributeLockKitSubsegmentLockOverrideProperty(
			HPS::AttributeLockKit & kit)
			: SettableArrayProperty(_T("Subsegment Overrides"))
			, kit(kit)
		{
			HPS::BoolArray statesArray;
			bool isSet = this->kit.ShowSubsegmentLockOverride(types, statesArray);

			if (isSet)
			{
				lockCount = static_cast<unsigned int>(types.size());
				states.assign(statesArray.begin(), statesArray.end());
			}
			else
			{
				lockCount = 1;
				ResizeArrays();
			}

			AddSubItem(new ArraySizeProperty(_T("Count"), lockCount));
			AddItems();

			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			if (lockCount < 1)
				return;

			AddOrDeleteItems(lockCount, static_cast<unsigned int>(types.size()));

			HPS::BoolArray statesArray(states.begin(), states.end());
			kit.UnsetSubsegmentLockOverride(HPS::AttributeLock::Type::Everything).SetSubsegmentLockOverride(types, statesArray);
		}

		void Unset() override
		{
			kit.UnsetSubsegmentLockOverride(HPS::AttributeLock::Type::Everything);
		}

		void ResizeArrays() override
		{
			types.resize(lockCount, HPS::AttributeLock::Type::Everything);
			states.resize(lockCount, false);
		}

		void AddItems() override
		{
			for (unsigned int lock = 0; lock < lockCount; ++lock)
			{
				auto newLock = new SingleAttributeLockProperty(lock, types[lock], states[lock]);
				AddSubItem(newLock);
			}
		}

	private:
		HPS::AttributeLockKit & kit;
		unsigned int lockCount;
		HPS::AttributeLockTypeArray types;
		BoolDeque states;
	};

	class AttributeLockKitProperty : public RootProperty
	{
	public:
		AttributeLockKitProperty(
			CMFCPropertyGridCtrl & ctrl,
			HPS::SegmentKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.ShowAttributeLock(kit);
			ctrl.AddProperty(new AttributeLockKitLockProperty(kit));
			ctrl.AddProperty(new AttributeLockKitSubsegmentLockOverrideProperty(kit));
		}

		void Apply() override
		{
			key.UnsetAttributeLock();
			key.SetAttributeLock(kit);
		}

	private:
		HPS::SegmentKey key;
		HPS::AttributeLockKit kit;
	};

	class ContourLineModeProperty : public BaseEnumProperty<HPS::ContourLine::Mode>
	{
	private:
		enum PropertyTypeIndex
		{
			ModePropertyIndex = 0,
			IntervalPropertyIndex,
			OffsetPropertyIndex,
			PositionsPropertyIndex,
		};

	public:
		ContourLineModeProperty(
			HPS::ContourLine::Mode & enumValue)
			: BaseEnumProperty(_T("Mode"), enumValue)
		{
			EnumTypeArray enumValues(2); HPS::UTF8Array enumStrings(2);
			enumValues[0] = HPS::ContourLine::Mode::Repeating; enumStrings[0] = "Repeating";
			enumValues[1] = HPS::ContourLine::Mode::Explicit; enumStrings[1] = "Explicit";
			InitializeEnumValues(enumValues, enumStrings);
		}

		void EnableValidProperties() override
		{
			CMFCPropertyGridProperty * parent = GetParent();
			auto intervalSibling = static_cast<BaseProperty *>(parent->GetSubItem(IntervalPropertyIndex));
			auto offsetSibling = static_cast<BaseProperty *>(parent->GetSubItem(OffsetPropertyIndex));
			auto positionsSibling = static_cast<BaseProperty *>(parent->GetSubItem(PositionsPropertyIndex));
			if (enumValue == HPS::ContourLine::Mode::Repeating)
			{
				intervalSibling->Enable(TRUE);
				offsetSibling->Enable(TRUE);
				positionsSibling->Enable(FALSE);
			}
			else if (enumValue == HPS::ContourLine::Mode::Explicit)
			{
				intervalSibling->Enable(FALSE);
				offsetSibling->Enable(FALSE);
				positionsSibling->Enable(TRUE);
			}
		}
	};

	class ContourLineKitPositionsArrayProperty : public ArrayProperty
	{
	public:
		ContourLineKitPositionsArrayProperty(
			HPS::FloatArray & positions)
			: ArrayProperty(_T("Positions"))
			, positions(positions)
		{
			positionCount = static_cast<unsigned int>(positions.size());
			AddSubItem(new ArraySizeProperty(_T("Count"), positionCount));
			AddItems();
		}

		void OnChildChanged() override
		{
			AddOrDeleteItems(positionCount, static_cast<unsigned int>(positions.size()));
			ArrayProperty::OnChildChanged();
		}

	protected:
		void ResizeArrays() override
		{
			positions.resize(positionCount, 0.0f);
		}

		void AddItems() override
		{
			for (unsigned int i = 0; i < positionCount; ++i)
			{
				std::wstring itemName = L"Position " + std::to_wstring(i);
				AddSubItem(new FloatProperty(itemName.c_str(), positions[i]));
			}
		}

	private:
		unsigned int positionCount;
		HPS::FloatArray & positions;
	};

	class ContourLineKitPositionsProperty : public SettableProperty
	{
	public:
		ContourLineKitPositionsProperty(
			HPS::ContourLineKit & kit)
			: SettableProperty(_T("Positions"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowPositions(mode, positions);
			if (isSet)
			{
				if (mode == HPS::ContourLine::Mode::Repeating)
				{
					interval = positions[0];
					offset = positions[1];
					positions.resize(1, 0.0f);
				}
				else if (mode == HPS::ContourLine::Mode::Explicit)
				{
					interval = 1.0f;
					offset = 0.0f;
				}
			}
			else
			{
				mode = HPS::ContourLine::Mode::Repeating;
				interval = 1.0f;
				offset = 0.0f;
				positions.resize(1, 0.0f);
			}

			auto modeChild = new ContourLineModeProperty(mode);
			AddSubItem(modeChild);
			AddSubItem(new UnsignedFloatProperty(_T("Interval"), interval));
			AddSubItem(new FloatProperty(_T("Offset"), offset));
			AddSubItem(new ContourLineKitPositionsArrayProperty(positions));
			modeChild->EnableValidProperties();
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			if (mode == HPS::ContourLine::Mode::Repeating)
				kit.SetPositions(interval, offset);
			else if (mode == HPS::ContourLine::Mode::Explicit)
				kit.SetPositions(positions);
		}

		void Unset() override
		{
			kit.UnsetPositions();
		}

	private:
		HPS::ContourLineKit & kit;
		HPS::ContourLine::Mode mode;
		float interval;
		float offset;
		HPS::FloatArray positions;
	};

	class ContourLineKitColorsProperty : public SettableArrayProperty
	{
	public:
		ContourLineKitColorsProperty(
			HPS::ContourLineKit & kit)
			: SettableArrayProperty(_T("Colors"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowColors(colors);
			if (isSet)
				colorCount = static_cast<unsigned int>(colors.size());
			else
			{
				colorCount = 1;
				ResizeArrays();
			}
			AddSubItem(new ArraySizeProperty(_T("Count"), colorCount));
			AddItems();
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			if (colorCount < 1)
				return;
			AddOrDeleteItems(colorCount, static_cast<unsigned int>(colors.size()));
			kit.SetColors(colors);
		}

		void Unset() override
		{
			kit.UnsetColors();
		}

		void ResizeArrays() override
		{
			colors.resize(colorCount, HPS::RGBColor::Black());
		}

		void AddItems() override
		{
			for (unsigned int i = 0; i < colorCount; ++i)
			{
				std::wstring itemName = L"Color " + std::to_wstring(i);
				AddSubItem(new RGBColorProperty(itemName.c_str(), colors[i]));
			}
		}

	private:
		HPS::ContourLineKit & kit;
		unsigned int colorCount;
		HPS::RGBColorArray colors;
	};

	class ContourLineKitPatternsProperty : public SettableArrayProperty
	{
	public:
		ContourLineKitPatternsProperty(
			HPS::ContourLineKit & kit)
			: SettableArrayProperty(_T("Patterns"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowPatterns(patterns);
			if (isSet)
				patternCount = static_cast<unsigned int>(patterns.size());
			else
			{
				patternCount = 1;
				ResizeArrays();
			}
			AddSubItem(new ArraySizeProperty(_T("Count"), patternCount));
			AddItems();
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			if (patternCount < 1)
				return;
			AddOrDeleteItems(patternCount, static_cast<unsigned int>(patterns.size()));
			kit.SetPatterns(patterns);
		}

		void Unset() override
		{
			kit.UnsetPatterns();
		}

		void ResizeArrays() override
		{
			patterns.resize(patternCount);
		}

		void AddItems() override
		{
			for (unsigned int i = 0; i < patternCount; ++i)
			{
				std::wstring itemName = L"Pattern " + std::to_wstring(i);
				AddSubItem(new UTF8Property(itemName.c_str(), patterns[i]));
			}
		}

	private:
		HPS::ContourLineKit & kit;
		unsigned int patternCount;
		HPS::UTF8Array patterns;
	};

	class SingleContourLineWeightProperty : public BaseProperty
	{
	public:
		SingleContourLineWeightProperty(
			unsigned int index,
			float & weight,
			HPS::Line::SizeUnits & units)
			: BaseProperty(_T(""))
			, weight(weight)
			, units(units)
		{
			std::wstring name = L"Weight " + std::to_wstring(index);
			SetName(name.c_str());
			AddSubItem(new FloatProperty(_T("Weight"), weight));
			AddSubItem(new LineSizeUnitsProperty(units));
		}

	private:
		float & weight;
		HPS::Line::SizeUnits & units;
	};

	class ContourLineKitWeightsProperty : public SettableArrayProperty
	{
	public:
		ContourLineKitWeightsProperty(
			HPS::ContourLineKit & kit)
			: SettableArrayProperty(_T("Weights"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowWeights(weights, units);
			if (isSet)
				weightCount = static_cast<unsigned int>(weights.size());
			else
			{
				weightCount = 1;
				ResizeArrays();
			}
			AddSubItem(new ArraySizeProperty(_T("Count"), weightCount));
			AddItems();
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			if (weightCount < 1)
				return;
			AddOrDeleteItems(weightCount, static_cast<unsigned int>(weights.size()));
			kit.SetWeights(weights, units);
		}

		void Unset() override
		{
			kit.UnsetWeights();
		}

		void ResizeArrays() override
		{
			weights.resize(weightCount, 1.0f);
			units.resize(weightCount, HPS::Line::SizeUnits::ScaleFactor);
		}

		void AddItems() override
		{
			for (unsigned int i = 0; i < weightCount; ++i)
				AddSubItem(new SingleContourLineWeightProperty(i, weights[i], units[i]));
		}

	private:
		HPS::ContourLineKit & kit;
		unsigned int weightCount;
		HPS::FloatArray weights;
		HPS::LineSizeUnitsArray units;
	};


	class GlyphColorSourceProperty : public BaseEnumProperty<HPS::Glyph::ColorSource>
	{
	private:
		enum PropertyTypeIndex
		{
			SourcePropertyIndex = 0,
			IndexPropertyIndex,
			ColorPropertyIndex,
		};

	public:
		GlyphColorSourceProperty(
			HPS::Glyph::ColorSource & enumValue)
			: BaseEnumProperty(_T("Source"), enumValue)
		{
			EnumTypeArray enumValues(3); HPS::UTF8Array enumStrings(3);
			enumValues[0] = HPS::Glyph::ColorSource::Default; enumStrings[0] = "Default";
			enumValues[1] = HPS::Glyph::ColorSource::Explicit; enumStrings[1] = "Explicit";
			enumValues[2] = HPS::Glyph::ColorSource::Indexed; enumStrings[2] = "Indexed";
			InitializeEnumValues(enumValues, enumStrings);
		}

		void EnableValidProperties() override
		{
			CMFCPropertyGridProperty * parent = GetParent();
			auto indexSibling = static_cast<BaseProperty *>(parent)->GetSubItem(IndexPropertyIndex);
			auto colorSibling = static_cast<BaseProperty *>(parent)->GetSubItem(ColorPropertyIndex);
			if (enumValue == HPS::Glyph::ColorSource::Default)
			{
				indexSibling->Enable(FALSE);
				colorSibling->Enable(FALSE);
			}
			else if (enumValue == HPS::Glyph::ColorSource::Explicit)
			{
				indexSibling->Enable(FALSE);
				colorSibling->Enable(TRUE);
			}
			else if (enumValue == HPS::Glyph::ColorSource::Indexed)
			{
				indexSibling->Enable(TRUE);
				colorSibling->Enable(FALSE);
			}
		}
	};

	class GlyphElementColorProperty : public SettableProperty
	{
	public:
		GlyphElementColorProperty(
			HPS::GlyphElement & element)
			: SettableProperty(_T("Color"))
			, element(element)
		{
			bool isSet = this->element.ShowColor(_source, _index, _color);
			if (!isSet)
			{
				_source = HPS::Glyph::ColorSource::Default;
				_index = 0;
				_color = HPS::RGBAColor::Black();
			}
			auto sourceChild = new GlyphColorSourceProperty(_source);
			AddSubItem(sourceChild);
			AddSubItem(new ByteProperty(_T("Index"), _index));
			AddSubItem(new RGBAColorProperty(_T("Color"), _color));
			sourceChild->EnableValidProperties();
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			if (_source == HPS::Glyph::ColorSource::Default)
				element.SetNormalColor();
			else if (_source == HPS::Glyph::ColorSource::Explicit)
				element.SetExplicitColor(_color);
			else if (_source == HPS::Glyph::ColorSource::Indexed)
				element.SetIndexedColor(_index);
		}

		void Unset() override
		{
			ASSERT(0); // can't be unset
		}

	private:
		HPS::GlyphElement & element;
		HPS::Glyph::ColorSource _source;
		HPS::byte _index;
		HPS::RGBAColor _color;
	};

	class LinePatternModifierProperty : public BaseEnumProperty<HPS::LinePattern::Modifier>
	{
	private:
		enum PropertyTypeIndex
		{
			ModifierPropertyIndex = 0,
			GlyphPropertyIndex,
			EnumPropertyIndex,
		};

	public:
		LinePatternModifierProperty(
			HPS::LinePattern::Modifier & enumValue)
			: BaseEnumProperty(_T("Modifier"), enumValue)
		{
			EnumTypeArray enumValues(2); HPS::UTF8Array enumStrings(2);
			enumValues[0] = HPS::LinePattern::Modifier::GlyphName; enumStrings[0] = "GlyphName";
			enumValues[1] = HPS::LinePattern::Modifier::Enumerated; enumStrings[1] = "Enumerated";
			InitializeEnumValues(enumValues, enumStrings);
		}

		void EnableValidProperties() override
		{
			CMFCPropertyGridProperty * parent = GetParent();
			auto glyphSibling = static_cast<BaseProperty *>(parent)->GetSubItem(GlyphPropertyIndex);
			auto enumSibling = static_cast<BaseProperty *>(parent)->GetSubItem(EnumPropertyIndex);
			if (enumValue == HPS::LinePattern::Modifier::GlyphName)
			{
				glyphSibling->Enable(TRUE);
				enumSibling->Enable(FALSE);
			}
			else if (enumValue == HPS::LinePattern::Modifier::Enumerated)
			{
				glyphSibling->Enable(FALSE);
				enumSibling->Enable(TRUE);
			}
		}
	};

	template <
		typename EnumType,
		typename EnumTypeProperty,
		EnumType defaultEnumValue,
		bool (HPS::LinePatternOptionsKit::*ShowFunction)(HPS::LinePattern::Modifier &, HPS::UTF8 &, EnumType &) const,
		HPS::LinePatternOptionsKit & (HPS::LinePatternOptionsKit::*SetGlyph)(char const *),
		HPS::LinePatternOptionsKit & (HPS::LinePatternOptionsKit::*SetEnumType)(EnumType),
		HPS::LinePatternOptionsKit & (HPS::LinePatternOptionsKit::*UnsetFunction)()
	>
	class CapOrJoinProperty : public SettableProperty
	{
	public:
		CapOrJoinProperty(
			CString const & name,
			HPS::LinePatternOptionsKit & kit)
			: SettableProperty(name)
			, kit(kit)
		{
			bool isSet = (this->kit.*ShowFunction)(_modifier, _glyph, _type);
			if (isSet)
			{
				if (_modifier == HPS::LinePattern::Modifier::GlyphName)
					_type = defaultEnumValue;
				else if (_modifier == HPS::LinePattern::Modifier::Enumerated)
					_glyph = "glyph";
			}
			else
			{
				_modifier = HPS::LinePattern::Modifier::Enumerated;
				_glyph = "glyph";
				_type = defaultEnumValue;
			}
			auto modifierProperty = new LinePatternModifierProperty(_modifier);
			AddSubItem(modifierProperty);
			AddSubItem(new UTF8Property(_T("Glyph"), _glyph));
			AddSubItem(new EnumTypeProperty(_type));
			modifierProperty->EnableValidProperties();
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			if (_modifier == HPS::LinePattern::Modifier::GlyphName)
				(kit.*SetGlyph)(_glyph);
			else if (_modifier == HPS::LinePattern::Modifier::Enumerated)
				(kit.*SetEnumType)(_type);
		}

		void Unset() override
		{
			(kit.*UnsetFunction)();
		}

	private:
		HPS::LinePatternOptionsKit & kit;
		HPS::LinePattern::Modifier _modifier;
		HPS::UTF8 _glyph;
		EnumType _type;
	};

	typedef CapOrJoinProperty <
		HPS::LinePattern::Cap,
		LinePatternCapProperty,
		HPS::LinePattern::Cap::Butt,
		&HPS::LinePatternOptionsKit::ShowStartCap,
		&HPS::LinePatternOptionsKit::SetStartCap,
		&HPS::LinePatternOptionsKit::SetStartCap,
		&HPS::LinePatternOptionsKit::UnsetStartCap
	> BaseLinePatternOptionsKitStartCapProperty;
	class LinePatternOptionsKitStartCapProperty : public BaseLinePatternOptionsKitStartCapProperty
	{
	public:
		LinePatternOptionsKitStartCapProperty(
			HPS::LinePatternOptionsKit & kit)
			: BaseLinePatternOptionsKitStartCapProperty(_T("Start Cap"), kit)
		{}
	};

	typedef CapOrJoinProperty <
		HPS::LinePattern::Cap,
		LinePatternCapProperty,
		HPS::LinePattern::Cap::Butt,
		&HPS::LinePatternOptionsKit::ShowEndCap,
		&HPS::LinePatternOptionsKit::SetEndCap,
		&HPS::LinePatternOptionsKit::SetEndCap,
		&HPS::LinePatternOptionsKit::UnsetEndCap
	> BaseLinePatternOptionsKitEndCapProperty;
	class LinePatternOptionsKitEndCapProperty : public BaseLinePatternOptionsKitEndCapProperty
	{
	public:
		LinePatternOptionsKitEndCapProperty(
			HPS::LinePatternOptionsKit & kit)
			: BaseLinePatternOptionsKitEndCapProperty(_T("End Cap"), kit)
		{}
	};

	typedef CapOrJoinProperty <
		HPS::LinePattern::Join,
		LinePatternJoinProperty,
		HPS::LinePattern::Join::Mitre,
		&HPS::LinePatternOptionsKit::ShowJoin,
		&HPS::LinePatternOptionsKit::SetJoin,
		&HPS::LinePatternOptionsKit::SetJoin,
		&HPS::LinePatternOptionsKit::UnsetJoin
	> BaseLinePatternOptionsKitJoinProperty;
	class LinePatternOptionsKitJoinProperty : public BaseLinePatternOptionsKitJoinProperty
	{
	public:
		LinePatternOptionsKitJoinProperty(
			HPS::LinePatternOptionsKit & kit)
			: BaseLinePatternOptionsKitJoinProperty(_T("Join"), kit)
		{}
	};

	class LinePatternOptionsKitInnerCapProperty : public SettableProperty
	{
	public:
		LinePatternOptionsKitInnerCapProperty(
			HPS::LinePatternOptionsKit & kit)
			: SettableProperty(_T("InnerCap"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowInnerCap(_type);
			if (!isSet)
				_type = HPS::LinePattern::Cap::Butt;
			AddSubItem(new LinePatternCapProperty(_type));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetInnerCap(_type);
		}

		void Unset() override
		{
			kit.UnsetInnerCap();
		}

	private:
		HPS::LinePatternOptionsKit & kit;
		HPS::LinePattern::Cap _type;
	};

	class LinePatternOptionsKitProperty : public BaseProperty
	{
	public:
		LinePatternOptionsKitProperty(
			CString const & name,
			HPS::LinePatternOptionsKit & kit)
			: BaseProperty(name)
		{
			AddSubItem(new LinePatternOptionsKitStartCapProperty(kit));
			AddSubItem(new LinePatternOptionsKitEndCapProperty(kit));
			AddSubItem(new LinePatternOptionsKitInnerCapProperty(kit));
			AddSubItem(new LinePatternOptionsKitJoinProperty(kit));
		}
	};

	class LineAttributeKitPatternProperty : public SettableProperty
	{
	public:
		LineAttributeKitPatternProperty(
			HPS::LineAttributeKit & kit)
			: SettableProperty(_T("Pattern"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowPattern(_pattern, _options);
			if (!isSet)
			{
				_pattern = "pattern";
				_options = HPS::LinePatternOptionsKit();
			}
			AddSubItem(new UTF8Property(_T("Pattern"), _pattern));
			AddSubItem(new LinePatternOptionsKitProperty(_T("Options"), _options));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetPattern(_pattern, _options);
		}

		void Unset() override
		{
			kit.UnsetPattern();
		}

	private:
		HPS::LineAttributeKit & kit;
		HPS::UTF8 _pattern;
		HPS::LinePatternOptionsKit _options;
	};

	class LineAttributeKitWeightProperty : public SettableProperty
	{
	public:
		LineAttributeKitWeightProperty(
			HPS::LineAttributeKit & kit)
			: SettableProperty(_T("Weight"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowWeight(_weight, _units);
			if (!isSet)
			{
				_weight = 1.0f;
				_units = HPS::Line::SizeUnits::ScaleFactor;
			}
			AddSubItem(new FloatProperty(_T("Weight"), _weight));
			AddSubItem(new LineSizeUnitsProperty(_units));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetWeight(_weight, _units);
		}

		void Unset() override
		{
			kit.UnsetWeight();
		}

	private:
		HPS::LineAttributeKit & kit;
		float _weight;
		HPS::Line::SizeUnits _units;
	};

	class LineAttributeKitProperty : public RootProperty
	{
	public:
		LineAttributeKitProperty(
			CMFCPropertyGridCtrl & ctrl,
			HPS::SegmentKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.ShowLineAttribute(kit);
			ctrl.AddProperty(new LineAttributeKitPatternProperty(kit));
			ctrl.AddProperty(new LineAttributeKitWeightProperty(kit));
		}

		void Apply() override
		{
			key.UnsetLineAttribute();
			key.SetLineAttribute(kit);
		}

	private:
		HPS::SegmentKey key;
		HPS::LineAttributeKit kit;
	};

	class SubwindowKitSubwindowProperty : public SettableProperty
	{
	public:
		SubwindowKitSubwindowProperty(
			HPS::SubwindowKit & kit)
			: SettableProperty(_T("Subwindow"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowSubwindow(_subwindow_position, _subwindow_offsets, _subwindow_type);
			if (!isSet)
			{
				_subwindow_position = HPS::Rectangle(-1.0f, 1.0f, -1.0f, 1.0f);
				_subwindow_offsets = HPS::IntRectangle::Zero();
				_subwindow_type = HPS::Subwindow::Type::Standard;
			}
			AddSubItem(new RectangleProperty(_T("Subwindow Position"), _subwindow_position));
			AddSubItem(new IntRectangleProperty(_T("Subwindow Offsets"), _subwindow_offsets));
			AddSubItem(new SubwindowTypeProperty(_subwindow_type));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetSubwindow(_subwindow_position, _subwindow_offsets, _subwindow_type);
		}

		void Unset() override
		{
			kit.UnsetSubwindow();
		}

	private:
		HPS::SubwindowKit & kit;
		HPS::Rectangle _subwindow_position;
		HPS::IntRectangle _subwindow_offsets;
		HPS::Subwindow::Type _subwindow_type;
	};

	class SubwindowBackgroundProperty : public BaseEnumProperty<HPS::Subwindow::Background>
	{
	private:
		enum PropertyTypeIndex
		{
			TypePropertyIndex = 0,
			NamePropertyIndex,
		};

	public:
		SubwindowBackgroundProperty(
			HPS::Subwindow::Background & enumValue)
			: BaseEnumProperty(_T("Type"), enumValue)
		{
			EnumTypeArray enumValues(14); HPS::UTF8Array enumStrings(14);
			enumValues[0] = HPS::Subwindow::Background::SolidColor; enumStrings[0] = "SolidColor";
			enumValues[1] = HPS::Subwindow::Background::Image; enumStrings[1] = "Image";
			enumValues[2] = HPS::Subwindow::Background::Cubemap; enumStrings[2] = "Cubemap";
			enumValues[3] = HPS::Subwindow::Background::Blend; enumStrings[3] = "Blend";
			enumValues[4] = HPS::Subwindow::Background::Transparent; enumStrings[4] = "Transparent";
			enumValues[5] = HPS::Subwindow::Background::Interactive; enumStrings[5] = "Interactive";
			enumValues[6] = HPS::Subwindow::Background::GradientTopToBottom; enumStrings[6] = "GradientTopToBottom";
			enumValues[7] = HPS::Subwindow::Background::GradientBottomToTop; enumStrings[7] = "GradientBottomToTop";
			enumValues[8] = HPS::Subwindow::Background::GradientLeftToRight; enumStrings[8] = "GradientLeftToRight";
			enumValues[9] = HPS::Subwindow::Background::GradientRightToLeft; enumStrings[9] = "GradientRightToLeft";
			enumValues[10] = HPS::Subwindow::Background::GradientTopLeftToBottomRight; enumStrings[10] = "GradientTopLeftToBottomRight";
			enumValues[11] = HPS::Subwindow::Background::GradientTopRightToBottomLeft; enumStrings[11] = "GradientTopRightToBottomLeft";
			enumValues[12] = HPS::Subwindow::Background::GradientBottomLeftToTopRight; enumStrings[12] = "GradientBottomLeftToTopRight";
			enumValues[13] = HPS::Subwindow::Background::GradientBottomRightToTopLeft; enumStrings[13] = "GradientBottomRightToTopLeft";
			InitializeEnumValues(enumValues, enumStrings);
		}

		void EnableValidProperties() override
		{
			CMFCPropertyGridProperty * parent = GetParent();
			auto nameSibling = static_cast<BaseProperty *>(parent)->GetSubItem(NamePropertyIndex);
			if (enumValue == HPS::Subwindow::Background::Image || enumValue == HPS::Subwindow::Background::Cubemap)
				nameSibling->Enable(TRUE);
			else
				nameSibling->Enable(FALSE);
		}
	};

	class SubwindowKitBackgroundProperty : public SettableProperty
	{
	public:
		SubwindowKitBackgroundProperty(
			HPS::SubwindowKit & kit)
			: SettableProperty(_T("Background"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowBackground(_bg_type, _definition_name);
			if (!isSet)
			{
				_bg_type = HPS::Subwindow::Background::SolidColor;
				_definition_name = "definition_name";
			}
			auto backgroundChild = new SubwindowBackgroundProperty(_bg_type);
			AddSubItem(backgroundChild);
			AddSubItem(new UTF8Property(_T("Definition Name"), _definition_name));
			backgroundChild->EnableValidProperties();
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetBackground(_bg_type, _definition_name);
		}

		void Unset() override
		{
			kit.UnsetBackground();
		}

	private:
		HPS::SubwindowKit & kit;
		HPS::Subwindow::Background _bg_type;
		HPS::UTF8 _definition_name;
	};

	class SubwindowKitBorderProperty : public SettableProperty
	{
	public:
		SubwindowKitBorderProperty(
			HPS::SubwindowKit & kit)
			: SettableProperty(_T("Border"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowBorder(_border_type);
			if (!isSet)
			{
				_border_type = HPS::Subwindow::Border::None;
			}
			AddSubItem(new SubwindowBorderProperty(_border_type));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetBorder(_border_type);
		}

		void Unset() override
		{
			kit.UnsetBorder();
		}

	private:
		HPS::SubwindowKit & kit;
		HPS::Subwindow::Border _border_type;
	};

	class SubwindowKitRenderingAlgorithmProperty : public SettableProperty
	{
	public:
		SubwindowKitRenderingAlgorithmProperty(
			HPS::SubwindowKit & kit)
			: SettableProperty(_T("RenderingAlgorithm"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowRenderingAlgorithm(_hsra);
			if (!isSet)
			{
				_hsra = HPS::Subwindow::RenderingAlgorithm::ZBuffer;
			}
			AddSubItem(new SubwindowRenderingAlgorithmProperty(_hsra));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetRenderingAlgorithm(_hsra);
		}

		void Unset() override
		{
			kit.UnsetRenderingAlgorithm();
		}

	private:
		HPS::SubwindowKit & kit;
		HPS::Subwindow::RenderingAlgorithm _hsra;
	};

	class SubwindowKitModelCompareModeProperty : public SettableProperty
	{
	public:
		SubwindowKitModelCompareModeProperty(
			HPS::SubwindowKit & kit)
			: SettableProperty(_T("ModelCompareMode"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowModelCompareMode(state, source1, source2);
			HPS::UTF8 source1Name;
			HPS::UTF8 source2Name;
			if (isSet && state)
			{
				source1Name = source1.Name();
				source2Name = source2.Name();
			}
			else
			{
				state = false;
				source1 = HPS::SegmentKey();
				source1Name = "No Segment";
				source2 = HPS::SegmentKey();
				source2Name = "No Segment";
			}
			auto boolProperty = new ConditionalBoolProperty(_T("State"), state);
			AddSubItem(boolProperty);
			AddSubItem(new ImmutableUTF8Property(_T("Source1"), source1Name));
			AddSubItem(new ImmutableUTF8Property(_T("Source2"), source2Name));
			boolProperty->EnableValidProperties();
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetModelCompareMode(state, source1, source2);
		}

		void Unset() override
		{
			kit.UnsetModelCompareMode();
		}

	private:
		HPS::SubwindowKit & kit;
		bool state;
		HPS::SegmentKey source1;
		HPS::SegmentKey source2;
	};

	class SubwindowKitProperty : public RootProperty
	{
	public:
		SubwindowKitProperty(
			CMFCPropertyGridCtrl & ctrl,
			HPS::SegmentKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.ShowSubwindow(kit);
			ctrl.AddProperty(new SubwindowKitSubwindowProperty(kit));
			ctrl.AddProperty(new SubwindowKitBackgroundProperty(kit));
			ctrl.AddProperty(new SubwindowKitBorderProperty(kit));
			ctrl.AddProperty(new SubwindowKitRenderingAlgorithmProperty(kit));
			ctrl.AddProperty(new SubwindowKitModelCompareModeProperty(kit));
		}

		void Apply() override
		{
			key.UnsetSubwindow();
			key.SetSubwindow(kit);
		}

	private:
		HPS::SegmentKey key;
		HPS::SubwindowKit kit;
	};

	template <
		typename Kit,
		bool (Kit::*ShowMatrix)(HPS::MatrixKit &) const,
		Kit & (Kit::*SetMatrix)(HPS::MatrixKit const &),
		Kit & (Kit::*UnsetMatrix)()
	>
	class MatrixKitProperty : public SettableProperty
	{
	public:
		MatrixKitProperty(
			CString const & name,
			Kit & kit)
			: SettableProperty(name)
			, kit(kit)
		{
			bool isSet = (this->kit.*ShowMatrix)(matrix);
			matrix.ShowElements(elements);
			for (size_t i = 0; i < elements.size(); ++i)
			{
				auto ithName = std::to_wstring(i);
				AddSubItem(new FloatProperty(ithName.c_str(), elements[i]));
			}
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			matrix.SetElements(elements);
			(kit.*SetMatrix)(matrix);
		}

		void Unset() override
		{
			(kit.*UnsetMatrix)();
		}

	private:
		Kit & kit;
		HPS::MatrixKit matrix;
		HPS::FloatArray elements;
	};

	typedef MatrixKitProperty <
		HPS::TextKit,
		&HPS::TextKit::ShowModellingMatrix,
		&HPS::TextKit::SetModellingMatrix,
		&HPS::TextKit::UnsetModellingMatrix
	> BaseTextKitModellingMatrixProperty;
	class TextKitModellingMatrixProperty : public BaseTextKitModellingMatrixProperty
	{
	public:
		TextKitModellingMatrixProperty(
			HPS::TextKit & kit)
			: BaseTextKitModellingMatrixProperty(_T("ModellingMatrix"), kit)
		{}
	};

	typedef MatrixKitProperty <
		HPS::TextureOptionsKit,
		&HPS::TextureOptionsKit::ShowTransformMatrix,
		&HPS::TextureOptionsKit::SetTransformMatrix,
		&HPS::TextureOptionsKit::UnsetTransformMatrix
	> BaseTextureOptionsKitTransformMatrixProperty;
	class TextureOptionsKitTransformMatrixProperty : public BaseTextureOptionsKitTransformMatrixProperty
	{
	public:
		TextureOptionsKitTransformMatrixProperty(
			HPS::TextureOptionsKit & kit)
			: BaseTextureOptionsKitTransformMatrixProperty(_T("TransformMatrix"), kit)
		{}
	};

	typedef MatrixKitProperty <
		HPS::ShaderKit,
		&HPS::ShaderKit::ShowTransformMatrix,
		&HPS::ShaderKit::SetTransformMatrix,
		&HPS::ShaderKit::UnsetTransformMatrix
	> BaseShaderKitTransformMatrixProperty;
	class ShaderKitTransformMatrixProperty : public BaseShaderKitTransformMatrixProperty
	{
	public:
		ShaderKitTransformMatrixProperty(
			HPS::ShaderKit & kit)
			: BaseShaderKitTransformMatrixProperty(_T("TransformMatrix"), kit)
		{}
	};

	class SingleTextMarginProperty : public BaseProperty
	{
	public:
		SingleTextMarginProperty(
			unsigned int margin,
			float & size,
			HPS::Text::MarginUnits & units)
			: BaseProperty(_T(""))
			, size(size)
			, units(units)
		{
			std::wstring name = L"Margin " + std::to_wstring(margin);
			SetName(name.c_str());
			AddSubItem(new FloatProperty(_T("Size"), size));
			AddSubItem(new TextMarginUnitsProperty(units));
		}

	private:
		float & size;
		HPS::Text::MarginUnits & units;
	};

	template <typename Kit>
	class BackgroundMarginsProperty : public SettableArrayProperty
	{
	public:
		BackgroundMarginsProperty(
			Kit & kit)
			: SettableArrayProperty(_T("BackgroundMargins"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowBackgroundMargins(_sizes, _units);
			if (isSet)
				marginCount = static_cast<unsigned int>(_sizes.size());
			else
			{
				marginCount = 1;
				ResizeArrays();
			}
			AddSubItem(new ArraySizeProperty(_T("Count"), marginCount));
			AddItems();
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			if (marginCount < 1)
				return;
			AddOrDeleteItems(marginCount, static_cast<unsigned int>(_sizes.size()));
			kit.SetBackgroundMargins(_sizes, _units);
		}

		void Unset() override
		{
			kit.UnsetBackgroundMargins();
		}

		void ResizeArrays() override
		{
			_sizes.resize(marginCount, 0.0f);
			_units.resize(marginCount, HPS::Text::MarginUnits::Percent);
		}

		void AddItems() override
		{
			for (unsigned int margin = 0; margin < marginCount; ++margin)
				AddSubItem(new SingleTextMarginProperty(margin, _sizes[margin], _units[margin]));
		}

	private:
		Kit & kit;
		unsigned int marginCount;
		HPS::FloatArray _sizes;
		HPS::TextMarginUnitsArray _units;
	};

	typedef BackgroundMarginsProperty<HPS::TextKit> TextKitBackgroundMarginsProperty;
	typedef BackgroundMarginsProperty<HPS::TextAttributeKit> TextAttributeKitBackgroundMarginsProperty;

	class TextKitLeaderLinesProperty : public SettableArrayProperty
	{
	private:
		enum PropertyTypeIndex
		{
			SpacePropertyIndex = 0,
			CountPropertyIndex,
			FirstItemIndex,
		};

	public:
		TextKitLeaderLinesProperty(
			HPS::TextKit & kit)
			: SettableArrayProperty(_T("LeaderLines"), FirstItemIndex)
			, kit(kit)
		{
			bool isSet = this->kit.ShowLeaderLines(_positions, _space);
			if (isSet)
				_position_count = static_cast<unsigned int>(_positions.size());
			else
			{
				_position_count = 1;
				ResizeArrays();
				_space = HPS::Text::LeaderLineSpace::Object;
			}
			AddSubItem(new TextLeaderLineSpaceProperty(_space));
			AddSubItem(new ArraySizeProperty(_T("Count"), _position_count));
			AddItems();
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			if (_position_count < 1)
				return;
			AddOrDeleteItems(_position_count, static_cast<unsigned int>(_positions.size()));
			kit.SetLeaderLines(_positions, _space);
		}

		void Unset() override
		{
			kit.UnsetLeaderLines();
		}

		void ResizeArrays() override
		{
			_positions.resize(_position_count, HPS::Point::Origin());
		}

		void AddItems() override
		{
			for (unsigned int position = 0; position < _position_count; ++position)
			{
				std::wstring itemName = L"Position " + std::to_wstring(position);
				AddSubItem(new PointProperty(itemName.c_str(), _positions[position]));
			}
		}

	private:
		HPS::TextKit & kit;
		unsigned int _position_count;
		HPS::PointArray _positions;
		HPS::Text::LeaderLineSpace _space;
	};

	enum class RegionPointCount
	{
		Two = 2,
		Three = 3,
	};

	class RegionPointCountProperty : public BaseEnumProperty<RegionPointCount>
	{
	private:
		enum PropertyTypeIndex
		{
			CountPropertyIndex = 0,
			Point0PropertyIndex,
			Point1PropertyIndex,
			Point2PropertyIndex,
		};

	public:
		RegionPointCountProperty(
			RegionPointCount & count)
			: BaseEnumProperty(_T("Count"), count)
		{
			EnumTypeArray enumValues(2); HPS::UTF8Array enumStrings(2);
			enumValues[0] = RegionPointCount::Two; enumStrings[0] = "2";
			enumValues[1] = RegionPointCount::Three; enumStrings[1] = "3";
			InitializeEnumValues(enumValues, enumStrings);
		}

		void EnableValidProperties() override
		{
			CMFCPropertyGridProperty * parent = GetParent();
			auto point2Sibling = static_cast<BaseProperty *>(parent->GetSubItem(Point2PropertyIndex));
			if (enumValue == RegionPointCount::Two)
				point2Sibling->Enable(FALSE);
			else if (enumValue == RegionPointCount::Three)
				point2Sibling->Enable(TRUE);
		}
	};

	class TextKitRegionProperty : public SettableProperty
	{
	public:
		TextKitRegionProperty(
			HPS::TextKit & kit)
			: SettableProperty(_T("Region"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowRegion(_region_points, _region_alignment, _region_fitting, _region_adjust_direction, _region_relative_coordinates, _region_window_space);
			if (isSet)
			{
				_region_point_count = static_cast<RegionPointCount>(_region_points.size());
				if (_region_point_count == RegionPointCount::Two)
					_region_points.resize(3, HPS::Point::Origin());
			}
			else
			{
				_region_point_count = RegionPointCount::Three;
				_region_points.resize(3, HPS::Point::Origin());
				_region_points[1] = HPS::Point(1, 0, 0);
				_region_points[2] = HPS::Point(0, 1, 0);
				_region_alignment = HPS::Text::RegionAlignment::Bottom;
				_region_fitting = HPS::Text::RegionFitting::Left;
				_region_adjust_direction = true;
				_region_relative_coordinates = true;
				_region_window_space = false;
			}
			auto countChild = new RegionPointCountProperty(_region_point_count);
			AddSubItem(countChild);
			AddSubItem(new PointProperty(_T("Region Point 0"), _region_points[0]));
			AddSubItem(new PointProperty(_T("Region Point 1"), _region_points[1]));
			AddSubItem(new PointProperty(_T("Region Point 2"), _region_points[2]));
			AddSubItem(new TextRegionAlignmentProperty(_region_alignment));
			AddSubItem(new TextRegionFittingProperty(_region_fitting));
			AddSubItem(new BoolProperty(_T("Region Adjust Direction"), _region_adjust_direction));
			AddSubItem(new BoolProperty(_T("Region Relative Coordinates"), _region_relative_coordinates));
			AddSubItem(new BoolProperty(_T("Region Window Space"), _region_window_space));
			countChild->EnableValidProperties();
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			// Use the overload that takes a count in case the number of region points is less than the size of the region point array.
			auto count = static_cast<size_t>(_region_point_count);
			kit.SetRegion(count, _region_points.data(), _region_alignment, _region_fitting, _region_adjust_direction, _region_relative_coordinates, _region_window_space);
		}

		void Unset() override
		{
			kit.UnsetRegion();
		}

	private:
		HPS::TextKit & kit;
		RegionPointCount _region_point_count;
		HPS::PointArray _region_points;
		HPS::Text::RegionAlignment _region_alignment;
		HPS::Text::RegionFitting _region_fitting;
		bool _region_adjust_direction;
		bool _region_relative_coordinates;
		bool _region_window_space;
	};

	class CuttingSectionKitPlanesProperty : public SettableArrayProperty
	{
	public:
		CuttingSectionKitPlanesProperty(
			HPS::CuttingSectionKit & kit)
			: SettableArrayProperty(_T("Planes"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowPlanes(planes);
			if (isSet)
				planeCount = static_cast<unsigned int>(planes.size());
			else
			{
				planeCount = 1;
				ResizeArrays();
			}
			AddSubItem(new ArraySizeProperty(_T("Count"), planeCount));
			AddItems();
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			if (planeCount < 1)
				return;
			AddOrDeleteItems(planeCount, static_cast<unsigned int>(planes.size()));
			kit.SetPlanes(planes);
		}

		void Unset() override
		{
			kit.UnsetPlanes();
		}

		void ResizeArrays() override
		{
			planes.resize(planeCount, HPS::Plane::Zero());
		}

		void AddItems() override
		{
			for (unsigned int i = 0; i < planeCount; ++i)
			{
				std::wstring itemName = L"Plane " + std::to_wstring(i);
				AddSubItem(new PlaneProperty(itemName.c_str(), planes[i]));
			}
		}

	private:
		HPS::CuttingSectionKit & kit;
		unsigned int planeCount;
		HPS::PlaneArray planes;
	};

	class CuttingSectionKitVisualizationProperty : public BaseProperty
	{
	public:
		CuttingSectionKitVisualizationProperty(
			HPS::CuttingSectionKit & kit)
			: BaseProperty(_T("Visualization"))
			, kit(kit)
		{
			if (!this->kit.ShowVisualization(_mode, _color, _scale))
			{
				_mode = HPS::CuttingSection::Mode::None;
				_color = HPS::RGBAColor(0, 0, 0, 0.25f);
				_scale = 1.0f;
			}
			AddSubItem(new CuttingSectionModeProperty(_mode));
			AddSubItem(new RGBAColorProperty(_T("Color"), _color));
			AddSubItem(new FloatProperty(_T("Scale"), _scale));
		}

		void OnChildChanged() override
		{
			kit.SetVisualization(_mode, _color, _scale);
			BaseProperty::OnChildChanged();
		}

	private:
		HPS::CuttingSectionKit & kit;
		HPS::CuttingSection::Mode _mode;
		HPS::RGBAColor _color;
		float _scale;
	};

	class CuttingSectionKitProperty : public RootProperty
	{
	public:
		CuttingSectionKitProperty(
			CMFCPropertyGridCtrl & ctrl,
			HPS::CuttingSectionKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.Show(kit);
			ctrl.AddProperty(new CuttingSectionKitPlanesProperty(kit));
			ctrl.AddProperty(new CuttingSectionKitVisualizationProperty(kit));
			ctrl.AddProperty(new CuttingSectionKitPriorityProperty(kit));
			ctrl.AddProperty(new CuttingSectionKitUserDataProperty(kit));
		}

		void Apply() override
		{
			key.Consume(kit);
		}

	private:
		HPS::CuttingSectionKey key;
		HPS::CuttingSectionKit kit;
	};

	class ClipRegionLoopProperty : public ArrayProperty
	{
	public:
		ClipRegionLoopProperty(
			CString const & name,
			HPS::PointArray & loopPoints)
			: ArrayProperty(name)
			, loopPoints(loopPoints)
		{
			pointCount = static_cast<unsigned int>(loopPoints.size());
			AddSubItem(new ArraySizeProperty(_T("Point Count"), pointCount));
			AddItems();
		}

		void OnChildChanged() override
		{
			AddOrDeleteItems(pointCount, static_cast<unsigned int>(loopPoints.size()));
			ArrayProperty::OnChildChanged();
		}

	protected:
		void ResizeArrays() override
		{
			loopPoints.resize(pointCount, HPS::Point::Origin());
		}

		void AddItems() override
		{
			for (unsigned int i = 0; i < pointCount; ++i)
			{
				std::wstring itemName = L"Point " + std::to_wstring(i);
				AddSubItem(new PointProperty(itemName.c_str(), loopPoints[i]));
			}
		}

	private:
		unsigned int pointCount;
		HPS::PointArray & loopPoints;
	};

	class DrawingAttributeKitClipRegionProperty : public SettableArrayProperty
	{
	private:
		enum PropertyTypeIndex
		{
			SpacePropertyIndex = 0,
			OperationPropertyIndex,
			CountPropertyIndex,
			FirstItemIndex,
		};

	public:
		DrawingAttributeKitClipRegionProperty(
			HPS::DrawingAttributeKit & kit)
			: SettableArrayProperty(_T("LeaderLines"), FirstItemIndex)
			, kit(kit)
		{
			bool isSet = this->kit.ShowClipRegion(_loops, _space, _operation);
			if (isSet)
				_loop_count = static_cast<unsigned int>(_loops.size());
			else
			{
				_loop_count = 1;
				ResizeArrays();
				_space = HPS::Drawing::ClipSpace::World;
				_operation = HPS::Drawing::ClipOperation::Keep;
			}
			AddSubItem(new DrawingClipSpaceProperty(_space));
			AddSubItem(new DrawingClipOperationProperty(_operation));
			AddSubItem(new ArraySizeProperty(_T("Loop Count"), _loop_count));
			AddItems();
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			if (_loop_count < 1)
				return;
			AddOrDeleteItems(_loop_count, static_cast<unsigned int>(_loops.size()));
			kit.SetClipRegion(_loops, _space, _operation);
		}

		void Unset() override
		{
			kit.UnsetClipRegion();
		}

		void ResizeArrays() override
		{
			_loops.resize(_loop_count, HPS::PointArray(1, HPS::Point::Origin()));
		}

		void AddItems() override
		{
			for (unsigned int loop = 0; loop < _loop_count; ++loop)
			{
				std::wstring itemName = L"Loop " + std::to_wstring(loop);
				AddSubItem(new ClipRegionLoopProperty(itemName.c_str(), _loops[loop]));
			}
		}

	private:
		HPS::DrawingAttributeKit & kit;
		unsigned int _loop_count;
		HPS::PointArrayArray _loops;
		HPS::Drawing::ClipSpace _space;
		HPS::Drawing::ClipOperation _operation;
	};


	template <typename Kit>
	class GeometryPointsProperty : public BaseProperty
	{
	public:
		GeometryPointsProperty(
			Kit const & kit)
			: BaseProperty(_T("Points"))
		{
			AddSubItem(new ImmutableSizeTProperty(_T("Count"), kit.GetPointCount()));
		}
	};
	typedef GeometryPointsProperty<HPS::ShellKit> ShellKitPointsProperty;
	typedef GeometryPointsProperty<HPS::CylinderKit> CylinderKitPointsProperty;
	typedef GeometryPointsProperty<HPS::LineKit> LineKitPointsProperty;
	typedef GeometryPointsProperty<HPS::NURBSCurveKit> NURBSCurveKitPointsProperty;
	typedef GeometryPointsProperty<HPS::NURBSSurfaceKit> NURBSSurfaceKitPointsProperty;
	typedef GeometryPointsProperty<HPS::PolygonKit> PolygonKitPointsProperty;

	class ShellKitFacelistProperty : public BaseProperty
	{
	public:
		ShellKitFacelistProperty(
			HPS::ShellKit const & kit)
			: BaseProperty(_T("Facelist"))
		{
			HPS::IntArray facelist;
			kit.ShowFacelist(facelist);
			AddSubItem(new ImmutableSizeTProperty(_T("Facelist Size"), facelist.size()));
			AddSubItem(new ImmutableSizeTProperty(_T("Face Count"), kit.GetFaceCount()));
		}
	};

	template <typename Kit>
	class PolyhedronMaterialMappingProperty : public SettableProperty
	{
	public:
		PolyhedronMaterialMappingProperty(
			Kit & kit)
			: SettableProperty(_T("MaterialMapping"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowMaterialMapping(materialMapping);
			AddSubItem(new FrontFaceMaterialProperty(materialMapping));
			AddSubItem(new BackFaceMaterialProperty(materialMapping));
			AddSubItem(new EdgeMaterialProperty(materialMapping));
			AddSubItem(new VertexMaterialProperty(materialMapping));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetMaterialMapping(materialMapping);
		}

		void Unset() override
		{
			kit.UnsetMaterialMapping();
		}

	private:
		Kit & kit;
		HPS::MaterialMappingKit materialMapping;
	};
	typedef PolyhedronMaterialMappingProperty<HPS::ShellKit> ShellKitMaterialMappingProperty;
	typedef PolyhedronMaterialMappingProperty<HPS::MeshKit> MeshKitMaterialMappingProperty;

	template <
		typename Kit,
		typename Component>
	HPS::UTF8 getVertexColorQuantityForComponent(
		Kit const & kit,
		Component componentType)
	{
		HPS::MaterialTypeArray types;
		HPS::RGBColorArray rgbColors;
		HPS::RGBAColorArray rgbaColors;
		HPS::FloatArray indices;
		if (kit.ShowVertexColors(componentType, types, rgbColors, rgbaColors, indices))
		{
			if (std::find(types.begin(), types.end(), HPS::Material::Type::None) == types.end())
				return "All";
			else
				return "Some";
		}
		else
			return "None";
	}

	class ShellKitVertexColorsProperty : public BaseProperty
	{
	public:
		ShellKitVertexColorsProperty(
			HPS::ShellKit const & kit)
			: BaseProperty(_T("VertexColors"))
		{
			HPS::UTF8 faceQuantity = getVertexColorQuantityForComponent(kit, HPS::Shell::Component::Faces);
			HPS::UTF8 edgeQuantity = getVertexColorQuantityForComponent(kit, HPS::Shell::Component::Edges);
			HPS::UTF8 vertexQuantity = getVertexColorQuantityForComponent(kit, HPS::Shell::Component::Vertices);
			AddSubItem(new ImmutableUTF8Property(_T("Faces"), faceQuantity));
			AddSubItem(new ImmutableUTF8Property(_T("Edges"), edgeQuantity));
			AddSubItem(new ImmutableUTF8Property(_T("Vertices"), vertexQuantity));
		}
	};

	template <
		typename Kit,
		bool (Kit::*ShowNormals)(HPS::BoolArray &, HPS::VectorArray &) const
	>
	class PolyhedronNormalsProperty : public BaseProperty
	{
	public:
		PolyhedronNormalsProperty(
			CString const & name,
			Kit const & kit)
			: BaseProperty(name)
		{
			HPS::UTF8 normalQuantity;
			HPS::BoolArray validities;
			HPS::VectorArray normals;
			if ((kit.*ShowNormals)(validities, normals))
			{
				if (std::find(validities.begin(), validities.end(), false) == validities.end())
					normalQuantity = "All";
				else
					normalQuantity = "Some";
			}
			else
				normalQuantity = "None";
			AddSubItem(new ImmutableUTF8Property(_T("Quantity"), normalQuantity));
		}
	};

	typedef PolyhedronNormalsProperty <
		HPS::ShellKit,
		&HPS::ShellKit::ShowVertexNormals
	> BaseShellKitVertexNormalsProperty;
	class ShellKitVertexNormalsProperty : public BaseShellKitVertexNormalsProperty
	{
	public:
		ShellKitVertexNormalsProperty(
			HPS::ShellKit const & kit)
			: BaseShellKitVertexNormalsProperty(_T("VertexNormals"), kit)
		{}
	};

	typedef PolyhedronNormalsProperty <
		HPS::ShellKit,
		&HPS::ShellKit::ShowFaceNormals
	> BaseShellKitFaceNormalsProperty;
	class ShellKitFaceNormalsProperty : public BaseShellKitFaceNormalsProperty
	{
	public:
		ShellKitFaceNormalsProperty(
			HPS::ShellKit const & kit)
			: BaseShellKitFaceNormalsProperty(_T("FaceNormals"), kit)
		{}
	};

	typedef PolyhedronNormalsProperty <
		HPS::MeshKit,
		&HPS::MeshKit::ShowVertexNormals
	> BaseMeshKitVertexNormalsProperty;
	class MeshKitVertexNormalsProperty : public BaseMeshKitVertexNormalsProperty
	{
	public:
		MeshKitVertexNormalsProperty(
			HPS::MeshKit const & kit)
			: BaseMeshKitVertexNormalsProperty(_T("VertexNormals"), kit)
		{}
	};

	typedef PolyhedronNormalsProperty <
		HPS::MeshKit,
		&HPS::MeshKit::ShowFaceNormals
	> BaseMeshKitFaceNormalsProperty;
	class MeshKitFaceNormalsProperty : public BaseMeshKitFaceNormalsProperty
	{
	public:
		MeshKitFaceNormalsProperty(
			HPS::MeshKit const & kit)
			: BaseMeshKitFaceNormalsProperty(_T("FaceNormals"), kit)
		{}
	};

	template <typename Kit>
	class PolyhedronVertexParametersProperty : public BaseProperty
	{
	public:
		PolyhedronVertexParametersProperty(
			Kit const & kit)
			: BaseProperty(_T("VertexParameters"))
		{
			size_t paramWidth;
			HPS::UTF8 paramQuantity;
			HPS::BoolArray validities;
			HPS::FloatArray params;
			if (kit.ShowVertexParameters(validities, params, paramWidth))
			{
				if (std::find(validities.begin(), validities.end(), false) == validities.end())
					paramQuantity = "All";
				else
					paramQuantity = "Some";
			}
			else
			{
				paramWidth = 0;
				paramQuantity = "None";
			}
			AddSubItem(new ImmutableUTF8Property(_T("Quantity"), paramQuantity));
			if (paramWidth > 0)
				AddSubItem(new ImmutableSizeTProperty(_T("Width"), paramWidth));
		}
	};
	typedef PolyhedronVertexParametersProperty<HPS::ShellKit> ShellKitVertexParametersProperty;
	typedef PolyhedronVertexParametersProperty<HPS::MeshKit> MeshKitVertexParametersProperty;

	template <
		typename Kit,
		bool (Kit::*ShowVisibilities)(HPS::BoolArray &, HPS::BoolArray &) const
	>
	class PolyhedronVisibilitiesProperty : public BaseProperty
	{
	public:
		PolyhedronVisibilitiesProperty(
			CString const & name,
			Kit const & kit)
			: BaseProperty(name)
		{
			HPS::UTF8 visibilityQuantity;
			HPS::BoolArray validities;
			HPS::BoolArray visibilities;
			if ((kit.*ShowVisibilities)(validities, visibilities))
			{
				if (std::find(validities.begin(), validities.end(), false) == validities.end())
					visibilityQuantity = "All";
				else
					visibilityQuantity = "Some";
			}
			else
				visibilityQuantity = "None";
			AddSubItem(new ImmutableUTF8Property(_T("Quantity"), visibilityQuantity));
		}
	};

	typedef PolyhedronVisibilitiesProperty <
		HPS::ShellKit,
		&HPS::ShellKit::ShowVertexVisibilities
	> BaseShellKitVertexVisibilitiesProperty;
	class ShellKitVertexVisibilitiesProperty : public BaseShellKitVertexVisibilitiesProperty
	{
	public:
		ShellKitVertexVisibilitiesProperty(
			HPS::ShellKit const & kit)
			: BaseShellKitVertexVisibilitiesProperty(_T("VertexVisibilities"), kit)
		{}
	};

	typedef PolyhedronVisibilitiesProperty <
		HPS::ShellKit,
		&HPS::ShellKit::ShowFaceVisibilities
	> BaseShellKitFaceVisibilitiesProperty;
	class ShellKitFaceVisibilitiesProperty : public BaseShellKitFaceVisibilitiesProperty
	{
	public:
		ShellKitFaceVisibilitiesProperty(
			HPS::ShellKit const & kit)
			: BaseShellKitFaceVisibilitiesProperty(_T("FaceVisibilities"), kit)
		{}
	};

	typedef PolyhedronVisibilitiesProperty <
		HPS::MeshKit,
		&HPS::MeshKit::ShowVertexVisibilities
	> BaseMeshKitVertexVisibilitiesProperty;
	class MeshKitVertexVisibilitiesProperty : public BaseMeshKitVertexVisibilitiesProperty
	{
	public:
		MeshKitVertexVisibilitiesProperty(
			HPS::MeshKit const & kit)
			: BaseMeshKitVertexVisibilitiesProperty(_T("VertexVisibilities"), kit)
		{}
	};

	typedef PolyhedronVisibilitiesProperty <
		HPS::MeshKit,
		&HPS::MeshKit::ShowFaceVisibilities
	> BaseMeshKitFaceVisibilitiesProperty;
	class MeshKitFaceVisibilitiesProperty : public BaseMeshKitFaceVisibilitiesProperty
	{
	public:
		MeshKitFaceVisibilitiesProperty(
			HPS::MeshKit const & kit)
			: BaseMeshKitFaceVisibilitiesProperty(_T("FaceVisibilities"), kit)
		{}
	};

	template <typename Kit>
	class PolyhedronFaceColorsProperty : public BaseProperty
	{
	public:
		PolyhedronFaceColorsProperty(
			Kit const & kit)
			: BaseProperty(_T("FaceColors"))
		{
			HPS::UTF8 colorQuantity;
			HPS::MaterialTypeArray types;
			HPS::RGBColorArray rgbColors;
			HPS::FloatArray indices;
			if (kit.ShowFaceColors(types, rgbColors, indices))
			{
				if (std::find(types.begin(), types.end(), HPS::Material::Type::None) == types.end())
					colorQuantity = "All";
				else
					colorQuantity = "Some";
			}
			else
				colorQuantity = "None";
			AddSubItem(new ImmutableUTF8Property(_T("Quantity"), colorQuantity));
		}
	};
	typedef PolyhedronFaceColorsProperty<HPS::ShellKit> ShellKitFaceColorsProperty;
	typedef PolyhedronFaceColorsProperty<HPS::MeshKit> MeshKitFaceColorsProperty;

	class ShellKitProperty : public RootProperty
	{
	public:
		ShellKitProperty(
			CMFCPropertyGridCtrl & ctrl,
			HPS::ShellKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.Show(kit);
			ctrl.AddProperty(new ShellKitPointsProperty(kit));
			ctrl.AddProperty(new ShellKitFacelistProperty(kit));
			ctrl.AddProperty(new ShellKitVertexColorsProperty(kit));
			ctrl.AddProperty(new ShellKitVertexNormalsProperty(kit));
			ctrl.AddProperty(new ShellKitVertexParametersProperty(kit));
			ctrl.AddProperty(new ShellKitVertexVisibilitiesProperty(kit));
			ctrl.AddProperty(new ShellKitFaceColorsProperty(kit));
			ctrl.AddProperty(new ShellKitFaceNormalsProperty(kit));
			ctrl.AddProperty(new ShellKitFaceVisibilitiesProperty(kit));
			ctrl.AddProperty(new ShellKitPriorityProperty(kit));
			ctrl.AddProperty(new ShellKitUserDataProperty(kit));
			ctrl.AddProperty(new ShellKitMaterialMappingProperty(kit));
		}

		void Apply() override
		{
			key.Consume(kit);
		}

	private:
		HPS::ShellKey key;
		HPS::ShellKit kit;
	};

	class MeshKitPointsProperty : public BaseProperty
	{
	public:
		MeshKitPointsProperty(
			HPS::MeshKit const & kit)
			: BaseProperty(_T("Points"))
		{
			size_t rows;
			size_t columns;
			kit.ShowRows(rows);
			kit.ShowColumns(columns);
			AddSubItem(new ImmutableSizeTProperty(_T("Count"), kit.GetPointCount()));
			AddSubItem(new ImmutableSizeTProperty(_T("Rows"), rows));
			AddSubItem(new ImmutableSizeTProperty(_T("Columns"), columns));
		}
	};

	class MeshKitVertexColorsProperty : public BaseProperty
	{
	public:
		MeshKitVertexColorsProperty(
			HPS::MeshKit const & kit)
			: BaseProperty(_T("VertexColors"))
		{
			HPS::UTF8 faceQuantity = getVertexColorQuantityForComponent(kit, HPS::Mesh::Component::Faces);
			HPS::UTF8 edgeQuantity = getVertexColorQuantityForComponent(kit, HPS::Mesh::Component::Edges);
			HPS::UTF8 vertexQuantity = getVertexColorQuantityForComponent(kit, HPS::Mesh::Component::Vertices);
			AddSubItem(new ImmutableUTF8Property(_T("Faces"), faceQuantity));
			AddSubItem(new ImmutableUTF8Property(_T("Edges"), edgeQuantity));
			AddSubItem(new ImmutableUTF8Property(_T("Vertices"), vertexQuantity));
		}
	};

	class MeshKitProperty : public RootProperty
	{
	public:
		MeshKitProperty(
			CMFCPropertyGridCtrl & ctrl,
			HPS::MeshKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.Show(kit);
			ctrl.AddProperty(new MeshKitPointsProperty(kit));
			ctrl.AddProperty(new MeshKitVertexColorsProperty(kit));
			ctrl.AddProperty(new MeshKitVertexNormalsProperty(kit));
			ctrl.AddProperty(new MeshKitVertexParametersProperty(kit));
			ctrl.AddProperty(new MeshKitVertexVisibilitiesProperty(kit));
			ctrl.AddProperty(new MeshKitFaceColorsProperty(kit));
			ctrl.AddProperty(new MeshKitFaceNormalsProperty(kit));
			ctrl.AddProperty(new MeshKitFaceVisibilitiesProperty(kit));
			ctrl.AddProperty(new MeshKitPriorityProperty(kit));
			ctrl.AddProperty(new MeshKitUserDataProperty(kit));
			ctrl.AddProperty(new MeshKitMaterialMappingProperty(kit));
		}

		void Apply() override
		{
			key.Consume(kit);
		}

	private:
		HPS::MeshKey key;
		HPS::MeshKit kit;
	};

	typedef ImmutableArraySizeProperty <
		HPS::CylinderKit,
		float,
		&HPS::CylinderKit::ShowRadii
	> BaseCylinderKitRadiiProperty;
	class CylinderKitRadiiProperty : public BaseCylinderKitRadiiProperty
	{
	public:
		CylinderKitRadiiProperty(
			HPS::CylinderKit const & kit)
			: BaseCylinderKitRadiiProperty(_T("Radii"), _T("Count"), kit)
		{}
	};

	class CylinderKitCapsProperty : public BaseProperty
	{
	public:
		CylinderKitCapsProperty(
			HPS::CylinderKit & kit)
			: BaseProperty(_T("Caps"))
			, kit(kit)
		{
			this->kit.ShowCaps(capping);
			AddSubItem(new CylinderCappingProperty(capping));
		}

		void OnChildChanged() override
		{
			kit.SetCaps(capping);
			BaseProperty::OnChildChanged();
		}

	private:
		HPS::CylinderKit & kit;
		HPS::Cylinder::Capping capping;
	};

	class CylinderKitVertexColorsProperty : public BaseProperty
	{
	public:
		CylinderKitVertexColorsProperty(
			HPS::CylinderKit const & kit)
			: BaseProperty(_T("VertexColors"))
		{
			HPS::UTF8 faceQuantity = getVertexColorQuantityForComponent(kit, HPS::Cylinder::Component::Faces);
			HPS::UTF8 edgeQuantity = getVertexColorQuantityForComponent(kit, HPS::Cylinder::Component::Edges);
			AddSubItem(new ImmutableUTF8Property(_T("Faces"), faceQuantity));
			AddSubItem(new ImmutableUTF8Property(_T("Edges"), edgeQuantity));
		}

	private:
		HPS::UTF8 getVertexColorQuantityForComponent(
			HPS::CylinderKit const & kit,
			HPS::Cylinder::Component componentType)
		{
			HPS::MaterialTypeArray types;
			HPS::RGBColorArray rgbColors;
			HPS::FloatArray indices;
			if (kit.ShowVertexColors(componentType, types, rgbColors, indices))
			{
				if (std::find(types.begin(), types.end(), HPS::Material::Type::None) == types.end())
					return "All";
				else
					return "Some";
			}
			else
				return "None";
		}
	};

	class CylinderKitProperty : public RootProperty
	{
	public:
		CylinderKitProperty(
			CMFCPropertyGridCtrl & ctrl,
			HPS::CylinderKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.Show(kit);
			ctrl.AddProperty(new CylinderKitPointsProperty(kit));
			ctrl.AddProperty(new CylinderKitRadiiProperty(kit));
			ctrl.AddProperty(new CylinderKitCapsProperty(kit));
			ctrl.AddProperty(new CylinderKitVertexColorsProperty(kit));
			ctrl.AddProperty(new CylinderKitPriorityProperty(kit));
			ctrl.AddProperty(new CylinderKitUserDataProperty(kit));
		}

		void Apply() override
		{
			key.Consume(kit);
		}

	private:
		HPS::CylinderKey key;
		HPS::CylinderKit kit;
	};

	class LineKitProperty : public RootProperty
	{
	public:
		LineKitProperty(
			CMFCPropertyGridCtrl & ctrl,
			HPS::LineKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.Show(kit);
			ctrl.AddProperty(new LineKitPointsProperty(kit));
			ctrl.AddProperty(new LineKitPriorityProperty(kit));
			ctrl.AddProperty(new LineKitUserDataProperty(kit));
		}

		void Apply() override
		{
			key.Consume(kit);
		}

	private:
		HPS::LineKey key;
		HPS::LineKit kit;
	};

	class NURBSCurveKitDegreeProperty : public BaseProperty
	{
	public:
		NURBSCurveKitDegreeProperty(
			HPS::NURBSCurveKit const & kit)
			: BaseProperty(_T("Degree"))
		{
			size_t degree;
			kit.ShowDegree(degree);
			AddSubItem(new ImmutableSizeTProperty(_T("Degree"), degree));
		}
	};

	typedef ImmutableArraySizeProperty <
		HPS::NURBSCurveKit,
		float,
		&HPS::NURBSCurveKit::ShowWeights
	> BaseNURBSCurveKitWeightsProperty;
	class NURBSCurveKitWeightsProperty : public BaseNURBSCurveKitWeightsProperty
	{
	public:
		NURBSCurveKitWeightsProperty(
			HPS::NURBSCurveKit const & kit)
			: BaseNURBSCurveKitWeightsProperty(_T("Weights"), _T("Count"), kit)
		{}
	};

	typedef ImmutableArraySizeProperty <
		HPS::NURBSCurveKit,
		float,
		&HPS::NURBSCurveKit::ShowKnots
	> BaseNURBSCurveKitKnotsProperty;
	class NURBSCurveKitKnotsProperty : public BaseNURBSCurveKitKnotsProperty
	{
	public:
		NURBSCurveKitKnotsProperty(
			HPS::NURBSCurveKit const & kit)
			: BaseNURBSCurveKitKnotsProperty(_T("Knots"), _T("Count"), kit)
		{}
	};

	class NURBSCurveKitParametersProperty : public BaseProperty
	{
	public:
		NURBSCurveKitParametersProperty(
			HPS::NURBSCurveKit & kit)
			: BaseProperty(_T("Parameters"))
			, kit(kit)
		{
			this->kit.ShowParameters(start, end);
			AddSubItem(new FloatProperty(_T("Start"), start));
			AddSubItem(new FloatProperty(_T("End"), end));
		}

		void OnChildChanged() override
		{
			kit.SetParameters(start, end);
			BaseProperty::OnChildChanged();
		}

	private:
		HPS::NURBSCurveKit & kit;
		float start;
		float end;
	};

	class NURBSCurveKitProperty : public RootProperty
	{
	public:
		NURBSCurveKitProperty(
			CMFCPropertyGridCtrl & ctrl,
			HPS::NURBSCurveKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.Show(kit);
			ctrl.AddProperty(new NURBSCurveKitDegreeProperty(kit));
			ctrl.AddProperty(new NURBSCurveKitPointsProperty(kit));
			ctrl.AddProperty(new NURBSCurveKitWeightsProperty(kit));
			ctrl.AddProperty(new NURBSCurveKitKnotsProperty(kit));
			ctrl.AddProperty(new NURBSCurveKitParametersProperty(kit));
			ctrl.AddProperty(new NURBSCurveKitPriorityProperty(kit));
			ctrl.AddProperty(new NURBSCurveKitUserDataProperty(kit));
		}

		void Apply() override
		{
			key.Consume(kit);
		}

	private:
		HPS::NURBSCurveKey key;
		HPS::NURBSCurveKit kit;
	};

	class NURBSSurfaceKitUProperty : public BaseProperty
	{
	public:
		NURBSSurfaceKitUProperty(
			HPS::NURBSSurfaceKit const & kit)
			: BaseProperty(_T("U"))
		{
			size_t degree;
			kit.ShowUDegree(degree);
			size_t count;
			kit.ShowUCount(count);
			AddSubItem(new ImmutableSizeTProperty(_T("Degree"), degree));
			AddSubItem(new ImmutableSizeTProperty(_T("Count"), count));
		}
	};

	class NURBSSurfaceKitVProperty : public BaseProperty
	{
	public:
		NURBSSurfaceKitVProperty(
			HPS::NURBSSurfaceKit const & kit)
			: BaseProperty(_T("V"))
		{
			size_t degree;
			kit.ShowVDegree(degree);
			size_t count;
			kit.ShowVCount(count);
			AddSubItem(new ImmutableSizeTProperty(_T("Degree"), degree));
			AddSubItem(new ImmutableSizeTProperty(_T("Count"), count));
		}
	};

	typedef ImmutableArraySizeProperty <
		HPS::NURBSSurfaceKit,
		float,
		&HPS::NURBSSurfaceKit::ShowWeights
	> BaseNURBSSurfaceKitWeightsProperty;
	class NURBSSurfaceKitWeightsProperty : public BaseNURBSSurfaceKitWeightsProperty
	{
	public:
		NURBSSurfaceKitWeightsProperty(
			HPS::NURBSSurfaceKit const & kit)
			: BaseNURBSSurfaceKitWeightsProperty(_T("Weights"), _T("Count"), kit)
		{}
	};

	class NURBSSurfaceKitKnotsProperty : public BaseProperty
	{
	public:
		NURBSSurfaceKitKnotsProperty(
			HPS::NURBSSurfaceKit const & kit)
			: BaseProperty(_T("Knots"))
		{
			HPS::FloatArray uKnots;
			HPS::FloatArray vKnots;
			kit.ShowUKnots(uKnots);
			kit.ShowVKnots(vKnots);
			AddSubItem(new ImmutableSizeTProperty(_T("U Count"), uKnots.size()));
			AddSubItem(new ImmutableSizeTProperty(_T("V Count"), vKnots.size()));
		}
	};

	class NURBSSurfaceKitTrimsProperty : public BaseProperty
	{
	public:
		NURBSSurfaceKitTrimsProperty(
			HPS::NURBSSurfaceKit const & kit)
			: BaseProperty(_T("Trims"))
		{
			HPS::TrimKitArray trims;
			kit.ShowTrims(trims);
			AddSubItem(new ImmutableSizeTProperty(_T("Count"), trims.size()));
		}
	};

	class NURBSSurfaceKitProperty : public RootProperty
	{
	public:
		NURBSSurfaceKitProperty(
			CMFCPropertyGridCtrl & ctrl,
			HPS::NURBSSurfaceKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.Show(kit);
			ctrl.AddProperty(new NURBSSurfaceKitUProperty(kit));
			ctrl.AddProperty(new NURBSSurfaceKitVProperty(kit));
			ctrl.AddProperty(new NURBSSurfaceKitPointsProperty(kit));
			ctrl.AddProperty(new NURBSSurfaceKitWeightsProperty(kit));
			ctrl.AddProperty(new NURBSSurfaceKitKnotsProperty(kit));
			ctrl.AddProperty(new NURBSSurfaceKitTrimsProperty(kit));
			ctrl.AddProperty(new NURBSSurfaceKitPriorityProperty(kit));
			ctrl.AddProperty(new NURBSSurfaceKitUserDataProperty(kit));
		}

		void Apply() override
		{
			key.Consume(kit);
		}

	private:
		HPS::NURBSSurfaceKey key;
		HPS::NURBSSurfaceKit kit;
	};

	class PolygonKitProperty : public RootProperty
	{
	public:
		PolygonKitProperty(
			CMFCPropertyGridCtrl & ctrl,
			HPS::PolygonKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.Show(kit);
			ctrl.AddProperty(new PolygonKitPointsProperty(kit));
			ctrl.AddProperty(new PolygonKitPriorityProperty(kit));
			ctrl.AddProperty(new PolygonKitUserDataProperty(kit));
		}

		void Apply() override
		{
			key.Consume(kit);
		}

	private:
		HPS::PolygonKey key;
		HPS::PolygonKit kit;
	};

	template <typename Definition>
	class DefinitionNameProperty : public BaseProperty
	{
	public:
		DefinitionNameProperty(
			Definition const & def)
			: BaseProperty(_T("Name"))
		{
			AddSubItem(new ImmutableUTF8Property(_T("Value"), def.Name()));
		}
	};

	class ImageKitSizeProperty : public BaseProperty
	{
	public:
		ImageKitSizeProperty(
			HPS::ImageKit const & kit)
			: BaseProperty(_T("Size"))
		{
			unsigned int width;
			unsigned int height;
			kit.ShowSize(width, height);
			AddSubItem(new ImmutableUnsignedIntProperty(_T("Width"), width));
			AddSubItem(new ImmutableUnsignedIntProperty(_T("Height"), height));
		}
	};

	typedef ImmutableArraySizeProperty <
		HPS::ImageKit,
		HPS::byte,
		&HPS::ImageKit::ShowData
	> BaseImageKitDataProperty;
	class ImageKitDataProperty : public BaseImageKitDataProperty
	{
	public:
		ImageKitDataProperty(
			HPS::ImageKit const & kit)
			: BaseImageKitDataProperty(_T("Data"), _T("Byte Count"), kit)
		{}
	};

	class ImageKitFormatProperty : public BaseProperty
	{
	public:
		ImageKitFormatProperty(
			HPS::ImageKit const & kit)
			: BaseProperty(_T("Format"))
		{
			HPS::Image::Format format;
			kit.ShowFormat(format);
			HPS::UTF8 formatString;
			switch (format)
			{
				case HPS::Image::Format::RGB: formatString = "RGB"; break;
				case HPS::Image::Format::RGBA: formatString = "RGBA"; break;
				case HPS::Image::Format::ARGB: formatString = "ARGB"; break;
				case HPS::Image::Format::Mapped8: formatString = "Mapped8"; break;
				case HPS::Image::Format::Grayscale: formatString = "Grayscale"; break;
				case HPS::Image::Format::Bmp: formatString = "Bmp"; break;
				case HPS::Image::Format::Jpeg: formatString = "Jpeg"; break;
				case HPS::Image::Format::Png: formatString = "Png"; break;
				case HPS::Image::Format::Targa: formatString = "Targa"; break;
				case HPS::Image::Format::DXT1: formatString = "DXT1"; break;
				case HPS::Image::Format::DXT3: formatString = "DXT3"; break;
				case HPS::Image::Format::DXT5: formatString = "DXT5"; break;
				default: ASSERT(0);
			}
			AddSubItem(new ImmutableUTF8Property(_T("Format"), formatString));
		}
	};

	class ImageKitDownSamplingProperty : public BaseProperty
	{
	public:
		ImageKitDownSamplingProperty(
			HPS::ImageKit & kit)
			: BaseProperty(_T("DownSampling"))
			, kit(kit)
		{
			this->kit.ShowDownSampling(_state);
			AddSubItem(new BoolProperty(_T("State"), _state));
		}

		void OnChildChanged() override
		{
			kit.SetDownSampling(_state);
			BaseProperty::OnChildChanged();
		}

	private:
		HPS::ImageKit & kit;
		bool _state;
	};

	class ImageKitCompressionQualityProperty : public BaseProperty
	{
	public:
		ImageKitCompressionQualityProperty(
			HPS::ImageKit & kit)
			: BaseProperty(_T("CompressionQuality"))
			, kit(kit)
		{
			this->kit.ShowCompressionQuality(_quality);
			AddSubItem(new UnitFloatProperty(_T("Quality"), _quality));
		}

		void OnChildChanged() override
		{
			kit.SetCompressionQuality(_quality);
			BaseProperty::OnChildChanged();
		}

	private:
		HPS::ImageKit & kit;
		float _quality;
	};

	class ImageDefinitionProperty : public RootProperty
	{
	public:
		ImageDefinitionProperty(
			CMFCPropertyGridCtrl & ctrl,
			HPS::ImageDefinition const & definition)
			: RootProperty(ctrl)
			, definition(definition)
		{
			this->definition.Show(kit);
			ctrl.AddProperty(new DefinitionNameProperty<HPS::ImageDefinition>(this->definition));
			ctrl.AddProperty(new ImageKitSizeProperty(kit));
			ctrl.AddProperty(new ImageKitDataProperty(kit));
			ctrl.AddProperty(new ImageKitFormatProperty(kit));
			ctrl.AddProperty(new ImageKitDownSamplingProperty(kit));
			ctrl.AddProperty(new ImageKitCompressionQualityProperty(kit));
		}

		void Apply() override
		{
			definition.Set(kit);
		}

	private:
		HPS::ImageDefinition definition;
		HPS::ImageKit kit;
	};

	class SegmentNameProperty : public BaseProperty
	{
	public:
		SegmentNameProperty(
			HPS::SegmentKey const & key)
			: BaseProperty(_T("Name"))
		{
			name = key.Name();
			AddSubItem(new UTF8Property(_T("Value"), name));
		}

		HPS::UTF8 GetName() const
		{
			return name;
		}

	private:
		HPS::UTF8 name;
	};

	class NetBoundingProperty : public BaseProperty
	{
	public:
		NetBoundingProperty(
			HPS::KeyPath const & keyPath)
			: BaseProperty(_T("Net Bounding"))
		{
			bool validBounding = false;
			HPS::BoundingKit boundingKit;
			if (keyPath.ShowNetBounding(boundingKit))
			{
				HPS::SimpleSphere sphere;
				HPS::SimpleCuboid cuboid;
				if (boundingKit.ShowVolume(sphere, cuboid))
				{
					validBounding = true;
					AddSubItem(new ImmutableSimpleSphereProperty(_T("Sphere"), sphere));
					AddSubItem(new ImmutableSimpleCuboidProperty(_T("Cuboid"), cuboid));
				}
			}

			if (!validBounding)
				AddSubItem(new ImmutableUTF8Property(_T("None"), HPS::UTF8()));
		}
	};

	class SegmentContentCountProperty : public BaseProperty
	{
	private:
		struct SearchTypeData
		{
			SearchTypeData()
			{}

			SearchTypeData(
				CString const & name,
				size_t count)
				: name(name)
				, count(count)
			{}

			CString name;
			size_t count;
		};

		typedef std::map <
			HPS::Search::Type,
			SearchTypeData,
			std::less<HPS::Search::Type>,
			HPS::Allocator<std::pair<HPS::Search::Type const, SearchTypeData>>
		> SearchTypeDataMap;

	public:
		SegmentContentCountProperty(
			HPS::SegmentKey const & key)
			: BaseProperty(_T("Contents"))
		{
			CountContents(key);
			for (auto const & countPair : countMap)
			{
				SearchTypeData const & typeData = countPair.second;
				if (typeData.count > 0)
					AddSubItem(new ImmutableSizeTProperty(typeData.name, typeData.count));
			}
		}

	private:
		void CountContents(
			HPS::SegmentKey const & key)
		{
			HPS::Search::Type types[24] = {
				HPS::Search::Type::Segment,
				HPS::Search::Type::Include,
				HPS::Search::Type::CuttingSection,
				HPS::Search::Type::Shell,
				HPS::Search::Type::Mesh,
				HPS::Search::Type::Grid,
				HPS::Search::Type::NURBSSurface,
				HPS::Search::Type::Cylinder,
				HPS::Search::Type::Sphere,
				HPS::Search::Type::Polygon,
				HPS::Search::Type::Circle,
				HPS::Search::Type::CircularWedge,
				HPS::Search::Type::Ellipse,
				HPS::Search::Type::Line,
				HPS::Search::Type::NURBSCurve,
				HPS::Search::Type::CircularArc,
				HPS::Search::Type::EllipticalArc,
				HPS::Search::Type::InfiniteLine,
				HPS::Search::Type::InfiniteRay,
				HPS::Search::Type::Marker,
				HPS::Search::Type::Text,
				HPS::Search::Type::Reference,
				HPS::Search::Type::DistantLight,
				HPS::Search::Type::Spotlight
			};

			HPS::SearchResults searchResults;
			if (key.Find(24, types, HPS::Search::Space::SegmentOnly, searchResults) > 0)
			{
				countMap[HPS::Search::Type::Segment] = SearchTypeData(_T("Subsegments"), 0);
				countMap[HPS::Search::Type::Include] = SearchTypeData(_T("Includes"), 0);
				countMap[HPS::Search::Type::CuttingSection] = SearchTypeData(_T("Cutting Sections"), 0);
				countMap[HPS::Search::Type::Shell] = SearchTypeData(_T("Shells"), 0);
				countMap[HPS::Search::Type::Mesh] = SearchTypeData(_T("Meshes"), 0);
				countMap[HPS::Search::Type::Grid] = SearchTypeData(_T("Grids"), 0);
				countMap[HPS::Search::Type::NURBSSurface] = SearchTypeData(_T("NURBS Surfaces"), 0);
				countMap[HPS::Search::Type::Cylinder] = SearchTypeData(_T("Cylinders"), 0);
				countMap[HPS::Search::Type::Sphere] = SearchTypeData(_T("Spheres"), 0);
				countMap[HPS::Search::Type::Polygon] = SearchTypeData(_T("Polygons"), 0);
				countMap[HPS::Search::Type::Circle] = SearchTypeData(_T("Circles"), 0);
				countMap[HPS::Search::Type::CircularWedge] = SearchTypeData(_T("Circular Wedges"), 0);
				countMap[HPS::Search::Type::Ellipse] = SearchTypeData(_T("Ellipses"), 0);
				countMap[HPS::Search::Type::Line] = SearchTypeData(_T("Lines"), 0);
				countMap[HPS::Search::Type::NURBSCurve] = SearchTypeData(_T("NURBS Curves"), 0);
				countMap[HPS::Search::Type::CircularArc] = SearchTypeData(_T("Circular Arcs"), 0);
				countMap[HPS::Search::Type::EllipticalArc] = SearchTypeData(_T("Elliptical Arcs"), 0);
				countMap[HPS::Search::Type::InfiniteLine] = SearchTypeData(_T("Infinite Lines"), 0);
				countMap[HPS::Search::Type::InfiniteRay] = SearchTypeData(_T("Infinite Rays"), 0);
				countMap[HPS::Search::Type::Marker] = SearchTypeData(_T("Markers"), 0);
				countMap[HPS::Search::Type::Text] = SearchTypeData(_T("Text"), 0);
				countMap[HPS::Search::Type::Reference] = SearchTypeData(_T("References"), 0);
				countMap[HPS::Search::Type::DistantLight] = SearchTypeData(_T("Distant Lights"), 0);
				countMap[HPS::Search::Type::Spotlight] = SearchTypeData(_T("Spotlights"), 0);

				HPS::SearchResultsIterator it = searchResults.GetIterator();
				while (it.IsValid())
				{
					countMap[it.GetResultTypes().front()].count++;
					++it;
				}
			}
		}

	private:
		SearchTypeDataMap countMap;
	};

	class SegmentKeyProperty : public RootProperty
	{
	private:
		enum PropertyTypeIndex
		{
			NamePropertyIndex = 0,
		};

	public:
		SegmentKeyProperty(
			CMFCPropertyGridCtrl & ctrl,
			HPS::SegmentKey const & key,
			HPS::KeyPath const & keyPath)
			: RootProperty(ctrl)
			, key(key)
		{
			ctrl.AddProperty(new SegmentNameProperty(this->key));
			ctrl.AddProperty(new NetBoundingProperty(keyPath));
			ctrl.AddProperty(new SegmentContentCountProperty(this->key));
		}

		void Apply() override
		{
			auto nameChild = static_cast<SegmentNameProperty *>(ctrl.GetProperty(NamePropertyIndex));
			if (key.Name() != nameChild->GetName())
				key.SetName(nameChild->GetName());
		}

	private:
		HPS::SegmentKey key;
	};

	class WindowInfoKitPhysicalPixelsProperty : public BaseProperty
	{
	public:
		WindowInfoKitPhysicalPixelsProperty(
			HPS::WindowInfoKit const & kit)
			: BaseProperty(_T("PhysicalPixels"))
		{
			unsigned int _width;
			unsigned int _height;
			kit.ShowPhysicalPixels(_width, _height);
			AddSubItem(new ImmutableUnsignedIntProperty(_T("Width"), _width));
			AddSubItem(new ImmutableUnsignedIntProperty(_T("Height"), _height));
		}
	};

	class WindowInfoKitPhysicalSizeProperty : public BaseProperty
	{
	public:
		WindowInfoKitPhysicalSizeProperty(
			HPS::WindowInfoKit const & kit)
			: BaseProperty(_T("PhysicalSize"))
		{
			float _width;
			float _height;
			kit.ShowPhysicalSize(_width, _height);
			AddSubItem(new ImmutableFloatProperty(_T("Width"), _width));
			AddSubItem(new ImmutableFloatProperty(_T("Height"), _height));
		}
	};

	class WindowInfoKitWindowPixelsProperty : public BaseProperty
	{
	public:
		WindowInfoKitWindowPixelsProperty(
			HPS::WindowInfoKit const & kit)
			: BaseProperty(_T("WindowPixels"))
		{
			unsigned int _width;
			unsigned int _height;
			kit.ShowWindowPixels(_width, _height);
			AddSubItem(new ImmutableUnsignedIntProperty(_T("Width"), _width));
			AddSubItem(new ImmutableUnsignedIntProperty(_T("Height"), _height));
		}
	};

	class WindowInfoKitWindowSizeProperty : public BaseProperty
	{
	public:
		WindowInfoKitWindowSizeProperty(
			HPS::WindowInfoKit const & kit)
			: BaseProperty(_T("WindowSize"))
		{
			float _width;
			float _height;
			kit.ShowWindowSize(_width, _height);
			AddSubItem(new ImmutableFloatProperty(_T("Width"), _width));
			AddSubItem(new ImmutableFloatProperty(_T("Height"), _height));
		}
	};

	class WindowInfoKitResolutionProperty : public BaseProperty
	{
	public:
		WindowInfoKitResolutionProperty(
			HPS::WindowInfoKit const & kit)
			: BaseProperty(_T("Resolution"))
		{
			float _horizontal;
			float _vertical;
			kit.ShowResolution(_horizontal, _vertical);
			AddSubItem(new ImmutableFloatProperty(_T("Horizontal"), _horizontal));
			AddSubItem(new ImmutableFloatProperty(_T("Vertical"), _vertical));
		}
	};

	class WindowInfoKitWindowAspectRatioProperty : public BaseProperty
	{
	public:
		WindowInfoKitWindowAspectRatioProperty(
			HPS::WindowInfoKit const & kit)
			: BaseProperty(_T("WindowAspectRatio"))
		{
			float _window_aspect;
			kit.ShowWindowAspectRatio(_window_aspect);
			AddSubItem(new ImmutableFloatProperty(_T("Window Aspect"), _window_aspect));
		}
	};

	class WindowInfoKitPixelAspectRatioProperty : public BaseProperty
	{
	public:
		WindowInfoKitPixelAspectRatioProperty(
			HPS::WindowInfoKit const & kit)
			: BaseProperty(_T("PixelAspectRatio"))
		{
			float _pixel_aspect;
			kit.ShowPixelAspectRatio(_pixel_aspect);
			AddSubItem(new ImmutableFloatProperty(_T("Pixel Aspect"), _pixel_aspect));
		}
	};

	class ApplicationWindowOptionsKitDriverProperty : public BaseProperty
	{
	public:
		ApplicationWindowOptionsKitDriverProperty(
			HPS::ApplicationWindowOptionsKit const & kit)
			: BaseProperty(_T("Driver"))
		{
			HPS::Window::Driver driver;
			kit.ShowDriver(driver);
			HPS::UTF8 driverString;
			switch (driver)
			{
				case HPS::Window::Driver::Default3D: driverString = "Default3D"; break;
				case HPS::Window::Driver::OpenGL: driverString = "OpenGL"; break;
				case HPS::Window::Driver::OpenGL2: driverString = "OpenGL2"; break;
				case HPS::Window::Driver::DirectX11: driverString = "DirectX11"; break;
				default: ASSERT(0);
			}
			AddSubItem(new ImmutableUTF8Property(_T("Driver"), driverString));
		}
	};

	class ApplicationWindowOptionsKitAntiAliasCapableProperty : public BaseProperty
	{
	public:
		ApplicationWindowOptionsKitAntiAliasCapableProperty(
			HPS::ApplicationWindowOptionsKit const & kit)
			: BaseProperty(_T("AntiAliasCapable"))
		{
			bool state;
			unsigned int samples;
			kit.ShowAntiAliasCapable(state, samples);
			AddSubItem(new ImmutableBoolProperty(_T("State"), state));
			if (state)
				AddSubItem(new ImmutableUnsignedIntProperty(_T("Samples"), samples));
		}
	};

	class ApplicationWindowOptionsKitPlatformDataProperty : public BaseProperty
	{
	public:
		ApplicationWindowOptionsKitPlatformDataProperty(
			HPS::ApplicationWindowOptionsKit const & kit)
			: BaseProperty(_T("PlatformData"))
		{
			intptr_t platformData;
			kit.ShowPlatformData(platformData);
			AddSubItem(new ImmutableIntPtrTProperty(_T("Value"), platformData));
		}
	};

	class ApplicationWindowOptionsKitFramebufferRetentionProperty : public BaseProperty
	{
	public:
		ApplicationWindowOptionsKitFramebufferRetentionProperty(
			HPS::ApplicationWindowOptionsKit const & kit)
			: BaseProperty(_T("FramebufferRetention"))
		{
			bool retain;
			kit.ShowFramebufferRetention(retain);
			AddSubItem(new ImmutableBoolProperty(_T("Retain"), retain));
		}
	};

	class ApplicationWindowKeyProperty : public SegmentKeyProperty
	{
	public:
		ApplicationWindowKeyProperty(
			CMFCPropertyGridCtrl & ctrl,
			HPS::ApplicationWindowKey const & key)
			: SegmentKeyProperty(ctrl, key, HPS::KeyPath(1, &key))
		{
			// window info
			{
				HPS::WindowInfoKit kit;
				key.ShowWindowInfo(kit);
				ctrl.AddProperty(new WindowInfoKitPhysicalPixelsProperty(kit));
				ctrl.AddProperty(new WindowInfoKitPhysicalSizeProperty(kit));
				ctrl.AddProperty(new WindowInfoKitWindowPixelsProperty(kit));
				ctrl.AddProperty(new WindowInfoKitWindowSizeProperty(kit));
				ctrl.AddProperty(new WindowInfoKitResolutionProperty(kit));
				ctrl.AddProperty(new WindowInfoKitWindowAspectRatioProperty(kit));
				ctrl.AddProperty(new WindowInfoKitPixelAspectRatioProperty(kit));
			}

			// application window options
			{
				HPS::ApplicationWindowOptionsKit kit;
				key.ShowWindowOptions(kit);
				ctrl.AddProperty(new ApplicationWindowOptionsKitDriverProperty(kit));
				ctrl.AddProperty(new ApplicationWindowOptionsKitAntiAliasCapableProperty(kit));
				ctrl.AddProperty(new ApplicationWindowOptionsKitPlatformDataProperty(kit));
				ctrl.AddProperty(new ApplicationWindowOptionsKitFramebufferRetentionProperty(kit));
			}
		}
	};

	class SelectionOptionsKitProximityProperty : public BaseProperty
	{
	public:
		SelectionOptionsKitProximityProperty(
			HPS::SelectionOptionsKit & kit)
			: BaseProperty(_T("Proximity"))
			, kit(kit)
		{
			this->kit.ShowProximity(_proximity);
			AddSubItem(new FloatProperty(_T("Proximity"), _proximity));
		}

		void OnChildChanged() override
		{
			kit.SetProximity(_proximity);
			BaseProperty::OnChildChanged();
		}

	private:
		HPS::SelectionOptionsKit & kit;
		float _proximity;
	};

	class SelectionOptionsKitLevelProperty : public BaseProperty
	{
	public:
		SelectionOptionsKitLevelProperty(
			HPS::SelectionOptionsKit & kit)
			: BaseProperty(_T("Level"))
			, kit(kit)
		{
			this->kit.ShowLevel(_level);
			AddSubItem(new SelectionLevelProperty(_level));
		}

		void OnChildChanged() override
		{
			kit.SetLevel(_level);
			BaseProperty::OnChildChanged();
		}

	private:
		HPS::SelectionOptionsKit & kit;
		HPS::Selection::Level _level;
	};

	class SelectionOptionsKitInternalLimitProperty : public BaseProperty
	{
	public:
		SelectionOptionsKitInternalLimitProperty(
			HPS::SelectionOptionsKit & kit)
			: BaseProperty(_T("InternalLimit"))
			, kit(kit)
		{
			size_t _limit_st;
			this->kit.ShowInternalLimit(_limit_st);
			_limit = static_cast<unsigned int>(_limit_st);
			AddSubItem(new UnsignedIntProperty(_T("Limit"), _limit));
		}

		void OnChildChanged() override
		{
			kit.SetInternalLimit(_limit);
			BaseProperty::OnChildChanged();
		}

	private:
		HPS::SelectionOptionsKit & kit;
		unsigned int _limit;
	};

	class SelectionOptionsKitRelatedLimitProperty : public BaseProperty
	{
	public:
		SelectionOptionsKitRelatedLimitProperty(
			HPS::SelectionOptionsKit & kit)
			: BaseProperty(_T("RelatedLimit"))
			, kit(kit)
		{
			size_t _limit_st;
			this->kit.ShowRelatedLimit(_limit_st);
			_limit = static_cast<unsigned int>(_limit_st);
			AddSubItem(new UnsignedIntProperty(_T("Limit"), _limit));
		}

		void OnChildChanged() override
		{
			kit.SetRelatedLimit(_limit);
			BaseProperty::OnChildChanged();
		}

	private:
		HPS::SelectionOptionsKit & kit;
		unsigned int _limit;
	};

	class SelectionOptionsKitSortingProperty : public BaseProperty
	{
	public:
		SelectionOptionsKitSortingProperty(
			HPS::SelectionOptionsKit & kit)
			: BaseProperty(_T("Sorting"))
			, kit(kit)
		{
			this->kit.ShowSorting(_sorting);
			AddSubItem(new SelectionSortingProperty(_sorting));
		}

		void OnChildChanged() override
		{
			kit.SetSorting(_sorting);
			BaseProperty::OnChildChanged();
		}

	private:
		HPS::SelectionOptionsKit & kit;
		HPS::Selection::Sorting _sorting;
	};

	class SelectionOptionsKitAlgorithmProperty : public BaseProperty
	{
	public:
		SelectionOptionsKitAlgorithmProperty(
			HPS::SelectionOptionsKit & kit)
			: BaseProperty(_T("Algorithm"))
			, kit(kit)
		{
			this->kit.ShowAlgorithm(_algorithm);
			AddSubItem(new SelectionAlgorithmProperty(_algorithm));
		}

		void OnChildChanged() override
		{
			kit.SetAlgorithm(_algorithm);
			BaseProperty::OnChildChanged();
		}

	private:
		HPS::SelectionOptionsKit & kit;
		HPS::Selection::Algorithm _algorithm;
	};

	class SelectionOptionsKitGranularityProperty : public BaseProperty
	{
	public:
		SelectionOptionsKitGranularityProperty(
			HPS::SelectionOptionsKit & kit)
			: BaseProperty(_T("Granularity"))
			, kit(kit)
		{
			this->kit.ShowGranularity(_granularity);
			AddSubItem(new SelectionGranularityProperty(_granularity));
		}

		void OnChildChanged() override
		{
			kit.SetGranularity(_granularity);
			BaseProperty::OnChildChanged();
		}

	private:
		HPS::SelectionOptionsKit & kit;
		HPS::Selection::Granularity _granularity;
	};

	class SelectionOptionsKitExtentCullingRespectedProperty : public BaseProperty
	{
	public:
		SelectionOptionsKitExtentCullingRespectedProperty(
			HPS::SelectionOptionsKit & kit)
			: BaseProperty(_T("ExtentCullingRespected"))
			, kit(kit)
		{
			this->kit.ShowExtentCullingRespected(_state);
			AddSubItem(new BoolProperty(_T("State"), _state));
		}

		void OnChildChanged() override
		{
			kit.SetExtentCullingRespected(_state);
			BaseProperty::OnChildChanged();
		}

	private:
		HPS::SelectionOptionsKit & kit;
		bool _state;
	};

	class SelectionOptionsKitDeferralExtentCullingRespectedProperty : public BaseProperty
	{
	public:
		SelectionOptionsKitDeferralExtentCullingRespectedProperty(
			HPS::SelectionOptionsKit & kit)
			: BaseProperty(_T("DeferralExtentCullingRespected"))
			, kit(kit)
		{
			this->kit.ShowDeferralExtentCullingRespected(_state);
			AddSubItem(new BoolProperty(_T("State"), _state));
		}

		void OnChildChanged() override
		{
			kit.SetDeferralExtentCullingRespected(_state);
			BaseProperty::OnChildChanged();
		}

	private:
		HPS::SelectionOptionsKit & kit;
		bool _state;
	};

	class SelectionOptionsKitFrustumCullingRespectedProperty : public BaseProperty
	{
	public:
		SelectionOptionsKitFrustumCullingRespectedProperty(
			HPS::SelectionOptionsKit & kit)
			: BaseProperty(_T("FrustumCullingRespected"))
			, kit(kit)
		{
			this->kit.ShowFrustumCullingRespected(_state);
			AddSubItem(new BoolProperty(_T("State"), _state));
		}

		void OnChildChanged() override
		{
			kit.SetFrustumCullingRespected(_state);
			BaseProperty::OnChildChanged();
		}

	private:
		HPS::SelectionOptionsKit & kit;
		bool _state;
	};

	class SelectionOptionsKitVectorCullingRespectedProperty : public BaseProperty
	{
	public:
		SelectionOptionsKitVectorCullingRespectedProperty(
			HPS::SelectionOptionsKit & kit)
			: BaseProperty(_T("VectorCullingRespected"))
			, kit(kit)
		{
			this->kit.ShowVectorCullingRespected(_state);
			AddSubItem(new BoolProperty(_T("State"), _state));
		}

		void OnChildChanged() override
		{
			kit.SetVectorCullingRespected(_state);
			BaseProperty::OnChildChanged();
		}

	private:
		HPS::SelectionOptionsKit & kit;
		bool _state;
	};

	class SelectionOptionsKitProperty : public RootProperty
	{
	public:
		SelectionOptionsKitProperty(
			CMFCPropertyGridCtrl & ctrl,
			HPS::WindowKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.ShowSelectionOptions(kit);
			ctrl.AddProperty(new SelectionOptionsKitProximityProperty(kit));
			ctrl.AddProperty(new SelectionOptionsKitLevelProperty(kit));
			ctrl.AddProperty(new SelectionOptionsKitInternalLimitProperty(kit));
			ctrl.AddProperty(new SelectionOptionsKitRelatedLimitProperty(kit));
			ctrl.AddProperty(new SelectionOptionsKitSortingProperty(kit));
			ctrl.AddProperty(new SelectionOptionsKitAlgorithmProperty(kit));
			ctrl.AddProperty(new SelectionOptionsKitGranularityProperty(kit));
			ctrl.AddProperty(new SelectionOptionsKitExtentCullingRespectedProperty(kit));
			ctrl.AddProperty(new SelectionOptionsKitDeferralExtentCullingRespectedProperty(kit));
			ctrl.AddProperty(new SelectionOptionsKitFrustumCullingRespectedProperty(kit));
			ctrl.AddProperty(new SelectionOptionsKitVectorCullingRespectedProperty(kit));
		}

		void Apply() override
		{
			key.SetSelectionOptions(kit);
		}

	private:
		HPS::WindowKey key;
		HPS::SelectionOptionsKit kit;
	};

	class CameraKitUpVectorProperty : public BaseProperty
	{
	public:
		CameraKitUpVectorProperty(
			HPS::CameraKit & kit)
			: BaseProperty(_T("UpVector"))
			, kit(kit)
		{
			this->kit.ShowUpVector(_up_vector);
			AddSubItem(new VectorProperty(_T("Up Vector"), _up_vector));
		}

		void OnChildChanged() override
		{
			kit.SetUpVector(_up_vector);
			BaseProperty::OnChildChanged();
		}

	private:
		HPS::CameraKit & kit;
		HPS::Vector _up_vector;
	};

	class CameraKitPositionProperty : public BaseProperty
	{
	public:
		CameraKitPositionProperty(
			HPS::CameraKit & kit)
			: BaseProperty(_T("Position"))
			, kit(kit)
		{
			this->kit.ShowPosition(_position);
			AddSubItem(new PointProperty(_T("Position"), _position));
		}

		void OnChildChanged() override
		{
			kit.SetPosition(_position);
			BaseProperty::OnChildChanged();
		}

	private:
		HPS::CameraKit & kit;
		HPS::Point _position;
	};

	class CameraKitTargetProperty : public BaseProperty
	{
	public:
		CameraKitTargetProperty(
			HPS::CameraKit & kit)
			: BaseProperty(_T("Target"))
			, kit(kit)
		{
			this->kit.ShowTarget(_target);
			AddSubItem(new PointProperty(_T("Target"), _target));
		}

		void OnChildChanged() override
		{
			kit.SetTarget(_target);
			BaseProperty::OnChildChanged();
		}

	private:
		HPS::CameraKit & kit;
		HPS::Point _target;
	};

	class CameraKitProjectionProperty : public BaseProperty
	{
	public:
		CameraKitProjectionProperty(
			HPS::CameraKit & kit)
			: BaseProperty(_T("Projection"))
			, kit(kit)
		{
			this->kit.ShowProjection(_type, _oblique_y_skew, _oblique_x_skew);
			AddSubItem(new CameraProjectionProperty(_type));
			AddSubItem(new FloatProperty(_T("Oblique Y Skew"), _oblique_y_skew));
			AddSubItem(new FloatProperty(_T("Oblique X Skew"), _oblique_x_skew));
		}

		void OnChildChanged() override
		{
			kit.SetProjection(_type, _oblique_y_skew, _oblique_x_skew);
			BaseProperty::OnChildChanged();
		}

	private:
		HPS::CameraKit & kit;
		HPS::Camera::Projection _type;
		float _oblique_y_skew;
		float _oblique_x_skew;
	};

	class CameraKitFieldProperty : public BaseProperty
	{
	public:
		CameraKitFieldProperty(
			HPS::CameraKit & kit)
			: BaseProperty(_T("Field"))
			, kit(kit)
		{
			this->kit.ShowField(_width, _height);
			AddSubItem(new UnsignedFloatProperty(_T("Width"), _width));
			AddSubItem(new UnsignedFloatProperty(_T("Height"), _height));
		}

		void OnChildChanged() override
		{
			kit.SetField(_width, _height);
			BaseProperty::OnChildChanged();
		}

	private:
		HPS::CameraKit & kit;
		float _width;
		float _height;
	};

	class CameraKitNearLimitProperty : public BaseProperty
	{
	public:
		CameraKitNearLimitProperty(
			HPS::CameraKit & kit)
			: BaseProperty(_T("NearLimit"))
			, kit(kit)
		{
			this->kit.ShowNearLimit(_near_limit);
			AddSubItem(new FloatProperty(_T("Near Limit"), _near_limit));
		}

		void OnChildChanged() override
		{
			kit.SetNearLimit(_near_limit);
			BaseProperty::OnChildChanged();
		}

	private:
		HPS::CameraKit & kit;
		float _near_limit;
	};

	class CameraKitProperty : public RootProperty
	{
	public:
		CameraKitProperty(
			CMFCPropertyGridCtrl & ctrl,
			HPS::SegmentKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			if (!this->key.ShowCamera(kit))
				kit = HPS::CameraKit::GetDefault();

			ctrl.AddProperty(new CameraKitPositionProperty(kit));
			ctrl.AddProperty(new CameraKitTargetProperty(kit));
			ctrl.AddProperty(new CameraKitUpVectorProperty(kit));
			ctrl.AddProperty(new CameraKitProjectionProperty(kit));
			ctrl.AddProperty(new CameraKitFieldProperty(kit));
			ctrl.AddProperty(new CameraKitNearLimitProperty(kit));
		}

		void Apply() override
		{
			key.SetCamera(kit);
		}

	private:
		HPS::SegmentKey key;
		HPS::CameraKit kit;
	};

	class TextureOptionsKitDecalProperty : public BaseProperty
	{
	public:
		TextureOptionsKitDecalProperty(
			HPS::TextureOptionsKit & kit)
			: BaseProperty(_T("Decal"))
			, kit(kit)
		{
			this->kit.ShowDecal(_state);
			AddSubItem(new BoolProperty(_T("State"), _state));
		}

		void OnChildChanged() override
		{
			kit.SetDecal(_state);
			BaseProperty::OnChildChanged();
		}

	private:
		HPS::TextureOptionsKit & kit;
		bool _state;
	};

	class TextureOptionsKitDownSamplingProperty : public BaseProperty
	{
	public:
		TextureOptionsKitDownSamplingProperty(
			HPS::TextureOptionsKit & kit)
			: BaseProperty(_T("DownSampling"))
			, kit(kit)
		{
			this->kit.ShowDownSampling(_state);
			AddSubItem(new BoolProperty(_T("State"), _state));
		}

		void OnChildChanged() override
		{
			kit.SetDownSampling(_state);
			BaseProperty::OnChildChanged();
		}

	private:
		HPS::TextureOptionsKit & kit;
		bool _state;
	};

	class TextureOptionsKitModulationProperty : public BaseProperty
	{
	public:
		TextureOptionsKitModulationProperty(
			HPS::TextureOptionsKit & kit)
			: BaseProperty(_T("Modulation"))
			, kit(kit)
		{
			this->kit.ShowModulation(_state);
			AddSubItem(new BoolProperty(_T("State"), _state));
		}

		void OnChildChanged() override
		{
			kit.SetModulation(_state);
			BaseProperty::OnChildChanged();
		}

	private:
		HPS::TextureOptionsKit & kit;
		bool _state;
	};

	class TextureOptionsKitParameterOffsetProperty : public BaseProperty
	{
	public:
		TextureOptionsKitParameterOffsetProperty(
			HPS::TextureOptionsKit & kit)
			: BaseProperty(_T("ParameterOffset"))
			, kit(kit)
		{
			size_t _offset_st;
			this->kit.ShowParameterOffset(_offset_st);
			_offset = static_cast<unsigned int>(_offset_st);
			AddSubItem(new UnsignedIntProperty(_T("Offset"), _offset));
		}

		void OnChildChanged() override
		{
			kit.SetParameterOffset(_offset);
			BaseProperty::OnChildChanged();
		}

	private:
		HPS::TextureOptionsKit & kit;
		unsigned int _offset;
	};

	class TextureOptionsKitParameterizationSourceProperty : public BaseProperty
	{
	public:
		TextureOptionsKitParameterizationSourceProperty(
			HPS::TextureOptionsKit & kit)
			: BaseProperty(_T("ParameterizationSource"))
			, kit(kit)
		{
			this->kit.ShowParameterizationSource(_source);
			AddSubItem(new MaterialTextureParameterizationProperty(_source));
		}

		void OnChildChanged() override
		{
			kit.SetParameterizationSource(_source);
			BaseProperty::OnChildChanged();
		}

	private:
		HPS::TextureOptionsKit & kit;
		HPS::Material::Texture::Parameterization _source;
	};

	class TextureOptionsKitTilingProperty : public BaseProperty
	{
	public:
		TextureOptionsKitTilingProperty(
			HPS::TextureOptionsKit & kit)
			: BaseProperty(_T("Tiling"))
			, kit(kit)
		{
			this->kit.ShowTiling(_tiling);
			AddSubItem(new MaterialTextureTilingProperty(_tiling));
		}

		void OnChildChanged() override
		{
			kit.SetTiling(_tiling);
			BaseProperty::OnChildChanged();
		}

	private:
		HPS::TextureOptionsKit & kit;
		HPS::Material::Texture::Tiling _tiling;
	};

	class TextureOptionsKitInterpolationFilterProperty : public BaseProperty
	{
	public:
		TextureOptionsKitInterpolationFilterProperty(
			HPS::TextureOptionsKit & kit)
			: BaseProperty(_T("InterpolationFilter"))
			, kit(kit)
		{
			this->kit.ShowInterpolationFilter(_filter);
			AddSubItem(new MaterialTextureInterpolationProperty(_filter));
		}

		void OnChildChanged() override
		{
			kit.SetInterpolationFilter(_filter);
			BaseProperty::OnChildChanged();
		}

	private:
		HPS::TextureOptionsKit & kit;
		HPS::Material::Texture::Interpolation _filter;
	};

	class TextureOptionsKitDecimationFilterProperty : public BaseProperty
	{
	public:
		TextureOptionsKitDecimationFilterProperty(
			HPS::TextureOptionsKit & kit)
			: BaseProperty(_T("DecimationFilter"))
			, kit(kit)
		{
			this->kit.ShowDecimationFilter(_filter);
			AddSubItem(new MaterialTextureDecimationProperty(_filter));
		}

		void OnChildChanged() override
		{
			kit.SetDecimationFilter(_filter);
			BaseProperty::OnChildChanged();
		}

	private:
		HPS::TextureOptionsKit & kit;
		HPS::Material::Texture::Decimation _filter;
	};

	template <typename DefinitionType>
	class TextureOrCubemapProperty : public RootProperty
	{
	public:
		TextureOrCubemapProperty(
			CMFCPropertyGridCtrl & ctrl,
			DefinitionType const & definition)
			: RootProperty(ctrl)
			, definition(definition)
		{
			this->definition.ShowOptions(kit);
			ctrl.AddProperty(new DefinitionNameProperty<DefinitionType>(this->definition));
			ctrl.AddProperty(new TextureOptionsKitDecalProperty(kit));
			ctrl.AddProperty(new TextureOptionsKitDownSamplingProperty(kit));
			ctrl.AddProperty(new TextureOptionsKitModulationProperty(kit));
			ctrl.AddProperty(new TextureOptionsKitParameterOffsetProperty(kit));
			ctrl.AddProperty(new TextureOptionsKitParameterizationSourceProperty(kit));
			ctrl.AddProperty(new TextureOptionsKitTilingProperty(kit));
			ctrl.AddProperty(new TextureOptionsKitInterpolationFilterProperty(kit));
			ctrl.AddProperty(new TextureOptionsKitDecimationFilterProperty(kit));
			ctrl.AddProperty(new TextureOptionsKitTransformMatrixProperty(kit));
		}

		void Apply() override
		{
			definition.SetOptions(kit);
		}

	private:
		DefinitionType definition;
		HPS::TextureOptionsKit kit;
	};

	typedef TextureOrCubemapProperty<HPS::TextureDefinition> TextureDefinitionProperty;
	typedef TextureOrCubemapProperty<HPS::CubeMapDefinition> CubeMapDefinitionProperty;

	class ShaderKitSourceProperty : public BaseProperty
	{
	public:
		ShaderKitSourceProperty(
			HPS::ShaderKit const & kit)
			: BaseProperty(_T("Source"))
		{
			HPS::UTF8 _source;
			kit.ShowSource(_source);
			AddSubItem(new ImmutableSizeTProperty(_T("Byte Count"), _source.GetLength()));
		}
	};

	class ShaderKitMultitextureProperty : public BaseProperty
	{
	public:
		ShaderKitMultitextureProperty(
			HPS::ShaderKit & kit)
			: BaseProperty(_T("Multitexture"))
			, kit(kit)
		{
			this->kit.ShowMultitexture(_state);
			AddSubItem(new BoolProperty(_T("State"), _state));
		}

		void OnChildChanged() override
		{
			kit.SetMultitexture(_state);
			BaseProperty::OnChildChanged();
		}

	private:
		HPS::ShaderKit & kit;
		bool _state;
	};

	class ShaderKitParameterizationSourceProperty : public BaseProperty
	{
	public:
		ShaderKitParameterizationSourceProperty(
			HPS::ShaderKit & kit)
			: BaseProperty(_T("ParameterizationSource"))
			, kit(kit)
		{
			this->kit.ShowParameterizationSource(_source);
			AddSubItem(new ShaderParameterizationProperty(_source));
		}

		void OnChildChanged() override
		{
			kit.SetParameterizationSource(_source);
			BaseProperty::OnChildChanged();
		}

	private:
		HPS::ShaderKit & kit;
		HPS::Shader::Parameterization _source;
	};

	class ShaderDefinitionProperty : public RootProperty
	{
	public:
		ShaderDefinitionProperty(
			CMFCPropertyGridCtrl & ctrl,
			HPS::ShaderDefinition const & definition)
			: RootProperty(ctrl)
			, definition(definition)
		{
			this->definition.Show(kit);
			ctrl.AddProperty(new DefinitionNameProperty<HPS::ShaderDefinition>(definition));
			ctrl.AddProperty(new ShaderKitSourceProperty(kit));
			ctrl.AddProperty(new ShaderKitMultitextureProperty(kit));
			ctrl.AddProperty(new ShaderKitParameterizationSourceProperty(kit));
			ctrl.AddProperty(new ShaderKitTransformMatrixProperty(kit));
		}

		void Apply() override
		{
			definition.Set(kit);
		}

	private:
		HPS::ShaderDefinition definition;
		HPS::ShaderKit kit;
	};

	enum class BoundingVolumeType
	{
		Sphere,
		Cuboid,
	};

	class BoundingVolumeTypeProperty : public BaseEnumProperty<BoundingVolumeType>
	{
	private:
		enum PropertyTypeIndex
		{
			TypePropertyIndex = 0,
			SpherePropertyIndex,
			CuboidPropertyIndex,
		};

	public:
		BoundingVolumeTypeProperty(
			BoundingVolumeType & type)
			: BaseEnumProperty(_T("Type"), type)
		{
			EnumTypeArray enumValues(2); HPS::UTF8Array enumStrings(2);
			enumValues[0] = BoundingVolumeType::Sphere; enumStrings[0] = "Sphere";
			enumValues[1] = BoundingVolumeType::Cuboid; enumStrings[1] = "Cuboid";
			InitializeEnumValues(enumValues, enumStrings);
		}

		void EnableValidProperties() override
		{
			CMFCPropertyGridProperty * parent = GetParent();
			auto sphereSibling = static_cast<BaseProperty *>(parent->GetSubItem(SpherePropertyIndex));
			auto cuboidSibling = static_cast<BaseProperty *>(parent->GetSubItem(CuboidPropertyIndex));
			if (enumValue == BoundingVolumeType::Sphere)
			{
				sphereSibling->Enable(TRUE);
				cuboidSibling->Enable(FALSE);
			}
			else if (enumValue == BoundingVolumeType::Cuboid)
			{
				sphereSibling->Enable(FALSE);
				cuboidSibling->Enable(TRUE);
			}
		}
	};

	class BoundingKitVolumeProperty : public SettableProperty
	{
	public:
		BoundingKitVolumeProperty(
			HPS::BoundingKit & kit)
			: SettableProperty(_T("Volume"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowVolume(sphere, cuboid);
			if (!isSet)
			{
				sphere = HPS::SimpleSphere(HPS::Point::Origin(), 1);
				cuboid = HPS::SimpleCuboid(HPS::Point(-1, -1, -1), HPS::Point(1, 1, 1));
			}
			type = BoundingVolumeType::Cuboid;
			auto typeChild = new BoundingVolumeTypeProperty(type);
			AddSubItem(typeChild);
			AddSubItem(new SimpleSphereProperty(_T("Sphere"), sphere));
			AddSubItem(new SimpleCuboidProperty(_T("Cuboid"), cuboid));
			typeChild->EnableValidProperties();
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			if (type == BoundingVolumeType::Sphere)
				kit.SetVolume(sphere);
			else if (type == BoundingVolumeType::Cuboid)
				kit.SetVolume(cuboid);
		}

		void Unset() override
		{
			kit.UnsetVolume();
		}

	private:
		HPS::BoundingKit & kit;
		BoundingVolumeType type;
		HPS::SimpleSphere sphere;
		HPS::SimpleCuboid cuboid;
	};

	class VisibilityKitCuttingSectionsProperty : public SettableProperty
	{
	public:
		VisibilityKitCuttingSectionsProperty(
			HPS::VisibilityKit & kit)
			: SettableProperty(_T("CuttingSections"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowCuttingSections(_state);
			if (!isSet)
			{
				_state = true;
			}
			AddSubItem(new BoolProperty(_T("State"), _state));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetCuttingSections(_state);
		}

		void Unset() override
		{
			kit.UnsetCuttingSections();
		}

	private:
		HPS::VisibilityKit & kit;
		bool _state;
	};

	class VisibilityKitCutEdgesProperty : public SettableProperty
	{
	public:
		VisibilityKitCutEdgesProperty(
			HPS::VisibilityKit & kit)
			: SettableProperty(_T("CutEdges"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowCutEdges(_state);
			if (!isSet)
			{
				_state = true;
			}
			AddSubItem(new BoolProperty(_T("State"), _state));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetCutEdges(_state);
		}

		void Unset() override
		{
			kit.UnsetCutEdges();
		}

	private:
		HPS::VisibilityKit & kit;
		bool _state;
	};

	class VisibilityKitCutFacesProperty : public SettableProperty
	{
	public:
		VisibilityKitCutFacesProperty(
			HPS::VisibilityKit & kit)
			: SettableProperty(_T("CutFaces"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowCutFaces(_state);
			if (!isSet)
			{
				_state = true;
			}
			AddSubItem(new BoolProperty(_T("State"), _state));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetCutFaces(_state);
		}

		void Unset() override
		{
			kit.UnsetCutFaces();
		}

	private:
		HPS::VisibilityKit & kit;
		bool _state;
	};

	class VisibilityKitWindowsProperty : public SettableProperty
	{
	public:
		VisibilityKitWindowsProperty(
			HPS::VisibilityKit & kit)
			: SettableProperty(_T("Windows"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowWindows(_state);
			if (!isSet)
			{
				_state = true;
			}
			AddSubItem(new BoolProperty(_T("State"), _state));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetWindows(_state);
		}

		void Unset() override
		{
			kit.UnsetWindows();
		}

	private:
		HPS::VisibilityKit & kit;
		bool _state;
	};

	class VisibilityKitTextProperty : public SettableProperty
	{
	public:
		VisibilityKitTextProperty(
			HPS::VisibilityKit & kit)
			: SettableProperty(_T("Text"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowText(_state);
			if (!isSet)
			{
				_state = true;
			}
			AddSubItem(new BoolProperty(_T("State"), _state));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetText(_state);
		}

		void Unset() override
		{
			kit.UnsetText();
		}

	private:
		HPS::VisibilityKit & kit;
		bool _state;
	};

	class VisibilityKitLinesProperty : public SettableProperty
	{
	public:
		VisibilityKitLinesProperty(
			HPS::VisibilityKit & kit)
			: SettableProperty(_T("Lines"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowLines(_state);
			if (!isSet)
			{
				_state = true;
			}
			AddSubItem(new BoolProperty(_T("State"), _state));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetLines(_state);
		}

		void Unset() override
		{
			kit.UnsetLines();
		}

	private:
		HPS::VisibilityKit & kit;
		bool _state;
	};

	class VisibilityKitEdgeLightsProperty : public SettableProperty
	{
	public:
		VisibilityKitEdgeLightsProperty(
			HPS::VisibilityKit & kit)
			: SettableProperty(_T("EdgeLights"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowEdgeLights(_state);
			if (!isSet)
			{
				_state = true;
			}
			AddSubItem(new BoolProperty(_T("State"), _state));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetEdgeLights(_state);
		}

		void Unset() override
		{
			kit.UnsetEdgeLights();
		}

	private:
		HPS::VisibilityKit & kit;
		bool _state;
	};

	class VisibilityKitMarkerLightsProperty : public SettableProperty
	{
	public:
		VisibilityKitMarkerLightsProperty(
			HPS::VisibilityKit & kit)
			: SettableProperty(_T("MarkerLights"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowMarkerLights(_state);
			if (!isSet)
			{
				_state = true;
			}
			AddSubItem(new BoolProperty(_T("State"), _state));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetMarkerLights(_state);
		}

		void Unset() override
		{
			kit.UnsetMarkerLights();
		}

	private:
		HPS::VisibilityKit & kit;
		bool _state;
	};

	class VisibilityKitFaceLightsProperty : public SettableProperty
	{
	public:
		VisibilityKitFaceLightsProperty(
			HPS::VisibilityKit & kit)
			: SettableProperty(_T("FaceLights"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowFaceLights(_state);
			if (!isSet)
			{
				_state = true;
			}
			AddSubItem(new BoolProperty(_T("State"), _state));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetFaceLights(_state);
		}

		void Unset() override
		{
			kit.UnsetFaceLights();
		}

	private:
		HPS::VisibilityKit & kit;
		bool _state;
	};

	class VisibilityKitGenericEdgesProperty : public SettableProperty
	{
	public:
		VisibilityKitGenericEdgesProperty(
			HPS::VisibilityKit & kit)
			: SettableProperty(_T("GenericEdges"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowGenericEdges(_state);
			if (!isSet)
			{
				_state = true;
			}
			AddSubItem(new BoolProperty(_T("State"), _state));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetGenericEdges(_state);
		}

		void Unset() override
		{
			kit.UnsetGenericEdges();
		}

	private:
		HPS::VisibilityKit & kit;
		bool _state;
	};

	class VisibilityKitInteriorSilhouetteEdgesProperty : public SettableProperty
	{
	public:
		VisibilityKitInteriorSilhouetteEdgesProperty(
			HPS::VisibilityKit & kit)
			: SettableProperty(_T("InteriorSilhouetteEdges"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowInteriorSilhouetteEdges(_state);
			if (!isSet)
			{
				_state = true;
			}
			AddSubItem(new BoolProperty(_T("State"), _state));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetInteriorSilhouetteEdges(_state);
		}

		void Unset() override
		{
			kit.UnsetInteriorSilhouetteEdges();
		}

	private:
		HPS::VisibilityKit & kit;
		bool _state;
	};

	class VisibilityKitAdjacentEdgesProperty : public SettableProperty
	{
	public:
		VisibilityKitAdjacentEdgesProperty(
			HPS::VisibilityKit & kit)
			: SettableProperty(_T("AdjacentEdges"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowAdjacentEdges(_state);
			if (!isSet)
			{
				_state = true;
			}
			AddSubItem(new BoolProperty(_T("State"), _state));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetAdjacentEdges(_state);
		}

		void Unset() override
		{
			kit.UnsetAdjacentEdges();
		}

	private:
		HPS::VisibilityKit & kit;
		bool _state;
	};

	class VisibilityKitHardEdgesProperty : public SettableProperty
	{
	public:
		VisibilityKitHardEdgesProperty(
			HPS::VisibilityKit & kit)
			: SettableProperty(_T("HardEdges"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowHardEdges(_state);
			if (!isSet)
			{
				_state = true;
			}
			AddSubItem(new BoolProperty(_T("State"), _state));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetHardEdges(_state);
		}

		void Unset() override
		{
			kit.UnsetHardEdges();
		}

	private:
		HPS::VisibilityKit & kit;
		bool _state;
	};

	class VisibilityKitMeshQuadEdgesProperty : public SettableProperty
	{
	public:
		VisibilityKitMeshQuadEdgesProperty(
			HPS::VisibilityKit & kit)
			: SettableProperty(_T("MeshQuadEdges"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowMeshQuadEdges(_state);
			if (!isSet)
			{
				_state = true;
			}
			AddSubItem(new BoolProperty(_T("State"), _state));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetMeshQuadEdges(_state);
		}

		void Unset() override
		{
			kit.UnsetMeshQuadEdges();
		}

	private:
		HPS::VisibilityKit & kit;
		bool _state;
	};

	class VisibilityKitNonCulledEdgesProperty : public SettableProperty
	{
	public:
		VisibilityKitNonCulledEdgesProperty(
			HPS::VisibilityKit & kit)
			: SettableProperty(_T("NonCulledEdges"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowNonCulledEdges(_state);
			if (!isSet)
			{
				_state = true;
			}
			AddSubItem(new BoolProperty(_T("State"), _state));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetNonCulledEdges(_state);
		}

		void Unset() override
		{
			kit.UnsetNonCulledEdges();
		}

	private:
		HPS::VisibilityKit & kit;
		bool _state;
	};

	class VisibilityKitPerimeterEdgesProperty : public SettableProperty
	{
	public:
		VisibilityKitPerimeterEdgesProperty(
			HPS::VisibilityKit & kit)
			: SettableProperty(_T("PerimeterEdges"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowPerimeterEdges(_state);
			if (!isSet)
			{
				_state = true;
			}
			AddSubItem(new BoolProperty(_T("State"), _state));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetPerimeterEdges(_state);
		}

		void Unset() override
		{
			kit.UnsetPerimeterEdges();
		}

	private:
		HPS::VisibilityKit & kit;
		bool _state;
	};

	class VisibilityKitFacesProperty : public SettableProperty
	{
	public:
		VisibilityKitFacesProperty(
			HPS::VisibilityKit & kit)
			: SettableProperty(_T("Faces"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowFaces(_state);
			if (!isSet)
			{
				_state = true;
			}
			AddSubItem(new BoolProperty(_T("State"), _state));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetFaces(_state);
		}

		void Unset() override
		{
			kit.UnsetFaces();
		}

	private:
		HPS::VisibilityKit & kit;
		bool _state;
	};

	class VisibilityKitVerticesProperty : public SettableProperty
	{
	public:
		VisibilityKitVerticesProperty(
			HPS::VisibilityKit & kit)
			: SettableProperty(_T("Vertices"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowVertices(_state);
			if (!isSet)
			{
				_state = true;
			}
			AddSubItem(new BoolProperty(_T("State"), _state));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetVertices(_state);
		}

		void Unset() override
		{
			kit.UnsetVertices();
		}

	private:
		HPS::VisibilityKit & kit;
		bool _state;
	};

	class VisibilityKitMarkersProperty : public SettableProperty
	{
	public:
		VisibilityKitMarkersProperty(
			HPS::VisibilityKit & kit)
			: SettableProperty(_T("Markers"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowMarkers(_state);
			if (!isSet)
			{
				_state = true;
			}
			AddSubItem(new BoolProperty(_T("State"), _state));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetMarkers(_state);
		}

		void Unset() override
		{
			kit.UnsetMarkers();
		}

	private:
		HPS::VisibilityKit & kit;
		bool _state;
	};

	class VisibilityKitShadowCastingProperty : public SettableProperty
	{
	public:
		VisibilityKitShadowCastingProperty(
			HPS::VisibilityKit & kit)
			: SettableProperty(_T("ShadowCasting"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowShadowCasting(_state);
			if (!isSet)
			{
				_state = true;
			}
			AddSubItem(new BoolProperty(_T("State"), _state));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetShadowCasting(_state);
		}

		void Unset() override
		{
			kit.UnsetShadowCasting();
		}

	private:
		HPS::VisibilityKit & kit;
		bool _state;
	};

	class VisibilityKitShadowReceivingProperty : public SettableProperty
	{
	public:
		VisibilityKitShadowReceivingProperty(
			HPS::VisibilityKit & kit)
			: SettableProperty(_T("ShadowReceiving"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowShadowReceiving(_state);
			if (!isSet)
			{
				_state = true;
			}
			AddSubItem(new BoolProperty(_T("State"), _state));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetShadowReceiving(_state);
		}

		void Unset() override
		{
			kit.UnsetShadowReceiving();
		}

	private:
		HPS::VisibilityKit & kit;
		bool _state;
	};

	class VisibilityKitShadowEmittingProperty : public SettableProperty
	{
	public:
		VisibilityKitShadowEmittingProperty(
			HPS::VisibilityKit & kit)
			: SettableProperty(_T("ShadowEmitting"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowShadowEmitting(_state);
			if (!isSet)
			{
				_state = true;
			}
			AddSubItem(new BoolProperty(_T("State"), _state));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetShadowEmitting(_state);
		}

		void Unset() override
		{
			kit.UnsetShadowEmitting();
		}

	private:
		HPS::VisibilityKit & kit;
		bool _state;
	};

	class VisibilityKitLeaderLinesProperty : public SettableProperty
	{
	public:
		VisibilityKitLeaderLinesProperty(
			HPS::VisibilityKit & kit)
			: SettableProperty(_T("LeaderLines"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowLeaderLines(_state);
			if (!isSet)
			{
				_state = true;
			}
			AddSubItem(new BoolProperty(_T("State"), _state));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetLeaderLines(_state);
		}

		void Unset() override
		{
			kit.UnsetLeaderLines();
		}

	private:
		HPS::VisibilityKit & kit;
		bool _state;
	};

	class VisibilityKitProperty : public RootProperty
	{
	public:
		VisibilityKitProperty(
			CMFCPropertyGridCtrl & ctrl,
			HPS::SegmentKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.ShowVisibility(kit);
			ctrl.AddProperty(new VisibilityKitFacesProperty(kit));
			ctrl.AddProperty(new VisibilityKitLinesProperty(kit));
			ctrl.AddProperty(new VisibilityKitGenericEdgesProperty(kit));
			ctrl.AddProperty(new VisibilityKitInteriorSilhouetteEdgesProperty(kit));
			ctrl.AddProperty(new VisibilityKitAdjacentEdgesProperty(kit));
			ctrl.AddProperty(new VisibilityKitHardEdgesProperty(kit));
			ctrl.AddProperty(new VisibilityKitMeshQuadEdgesProperty(kit));
			ctrl.AddProperty(new VisibilityKitNonCulledEdgesProperty(kit));
			ctrl.AddProperty(new VisibilityKitPerimeterEdgesProperty(kit));
			ctrl.AddProperty(new VisibilityKitTextProperty(kit));
			ctrl.AddProperty(new VisibilityKitLeaderLinesProperty(kit));
			ctrl.AddProperty(new VisibilityKitVerticesProperty(kit));
			ctrl.AddProperty(new VisibilityKitMarkersProperty(kit));
			ctrl.AddProperty(new VisibilityKitEdgeLightsProperty(kit));
			ctrl.AddProperty(new VisibilityKitMarkerLightsProperty(kit));
			ctrl.AddProperty(new VisibilityKitFaceLightsProperty(kit));
			ctrl.AddProperty(new VisibilityKitCuttingSectionsProperty(kit));
			ctrl.AddProperty(new VisibilityKitCutFacesProperty(kit));
			ctrl.AddProperty(new VisibilityKitCutEdgesProperty(kit));
			ctrl.AddProperty(new VisibilityKitWindowsProperty(kit));
			ctrl.AddProperty(new VisibilityKitShadowCastingProperty(kit));
			ctrl.AddProperty(new VisibilityKitShadowReceivingProperty(kit));
			ctrl.AddProperty(new VisibilityKitShadowEmittingProperty(kit));
		}

		void Apply() override
		{
			key.UnsetVisibility();
			key.SetVisibility(kit);
		}

	private:
		HPS::SegmentKey key;
		HPS::VisibilityKit kit;
	};

	class VisualEffectsKitSimpleReflectionVisibilityProperty : public SettableProperty
	{
	public:
		VisualEffectsKitSimpleReflectionVisibilityProperty(
			HPS::VisualEffectsKit & kit)
			: SettableProperty(_T("SimpleReflectionVisibility"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowSimpleReflectionVisibility(_reflected_types);
			if (!isSet)
			{
				_reflected_types = HPS::VisibilityKit();
			}
			AddSubItem(new VisibilityKitFacesProperty(_reflected_types));
			AddSubItem(new VisibilityKitLinesProperty(_reflected_types));
			AddSubItem(new VisibilityKitGenericEdgesProperty(_reflected_types));
			AddSubItem(new VisibilityKitInteriorSilhouetteEdgesProperty(_reflected_types));
			AddSubItem(new VisibilityKitAdjacentEdgesProperty(_reflected_types));
			AddSubItem(new VisibilityKitHardEdgesProperty(_reflected_types));
			AddSubItem(new VisibilityKitMeshQuadEdgesProperty(_reflected_types));
			AddSubItem(new VisibilityKitNonCulledEdgesProperty(_reflected_types));
			AddSubItem(new VisibilityKitPerimeterEdgesProperty(_reflected_types));
			AddSubItem(new VisibilityKitTextProperty(_reflected_types));
			AddSubItem(new VisibilityKitLeaderLinesProperty(_reflected_types));
			AddSubItem(new VisibilityKitVerticesProperty(_reflected_types));
			AddSubItem(new VisibilityKitMarkersProperty(_reflected_types));
			AddSubItem(new VisibilityKitEdgeLightsProperty(_reflected_types));
			AddSubItem(new VisibilityKitMarkerLightsProperty(_reflected_types));
			AddSubItem(new VisibilityKitFaceLightsProperty(_reflected_types));
			AddSubItem(new VisibilityKitCuttingSectionsProperty(_reflected_types));
			AddSubItem(new VisibilityKitCutFacesProperty(_reflected_types));
			AddSubItem(new VisibilityKitCutEdgesProperty(_reflected_types));
			AddSubItem(new VisibilityKitWindowsProperty(_reflected_types));
			AddSubItem(new VisibilityKitShadowCastingProperty(_reflected_types));
			AddSubItem(new VisibilityKitShadowReceivingProperty(_reflected_types));
			AddSubItem(new VisibilityKitShadowEmittingProperty(_reflected_types));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetSimpleReflectionVisibility(_reflected_types);
		}

		void Unset() override
		{
			kit.UnsetSimpleReflectionVisibility();
		}

	private:
		HPS::VisualEffectsKit & kit;
		HPS::VisibilityKit _reflected_types;
	};

	class VisualEffectsKitEyeDomeLightingBackColorProperty : public SettableProperty
	{
	public:
		VisualEffectsKitEyeDomeLightingBackColorProperty(
			HPS::VisualEffectsKit & kit)
			: SettableProperty(_T("EyeDomeLightingBackColor"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowEyeDomeLightingBackColor(_state, _color);
			if (!isSet)
			{
				_state = true;
				_color = HPS::RGBColor::Black();
			}
			else if (!_state)
				_color = HPS::RGBColor::Black();
			auto boolProperty = new ConditionalBoolProperty(_T("State"), _state);
			AddSubItem(boolProperty);
			AddSubItem(new RGBColorProperty(_T("Color"), _color));
			boolProperty->EnableValidProperties();
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetEyeDomeLightingBackColor(_state, _color);
		}

		void Unset() override
		{
			kit.UnsetEyeDomeLightingBackColor();
		}

	private:
		HPS::VisualEffectsKit & kit;
		bool _state;
		HPS::RGBColor _color;
	};

	class MaterialPaletteProperty : public SettableProperty
	{
	public:
		MaterialPaletteProperty(
			HPS::SegmentKey const & key)
			: SettableProperty(_T("Material Palette"))
		{
			bool isSet = key.ShowMaterialPalette(palette);
			if (!isSet)
				palette = "name";
			AddSubItem(new UTF8Property(_T("Name"), palette));
			IsSet(isSet);
		}

		HPS::UTF8 GetPalette() const
		{
			return palette;
		}

	protected:
		void Set() override
		{
			// nothing to do
		}

		void Unset() override
		{
			// nothing to do
		}

	private:
		HPS::UTF8 palette;
	};

	class SegmentKeyMaterialPaletteProperty : public RootProperty
	{
	private:
		enum PropertyTypeIndex
		{
			PalettePropertyIndex = 0,
		};

	public:
		SegmentKeyMaterialPaletteProperty(
			CMFCPropertyGridCtrl & ctrl,
			HPS::SegmentKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			ctrl.AddProperty(new MaterialPaletteProperty(this->key));
		}

		void Apply() override
		{
			auto paletteChild = static_cast<MaterialPaletteProperty *>(ctrl.GetProperty(PalettePropertyIndex));
			if (paletteChild->IsSet())
				key.SetMaterialPalette(paletteChild->GetPalette());
			else
				key.UnsetMaterialPalette();
		}

	private:
		HPS::SegmentKey key;
	};

	template <
		typename Key,
		bool (Key::*ShowMatrix)(HPS::MatrixKit &) const
	>
	class KeyMatrixProperty : public SettableProperty
	{
	public:
		KeyMatrixProperty(
			CString const & name,
			Key const & key)
			: SettableProperty(name)
		{
			bool isSet = (key.*ShowMatrix)(matrix);
			matrix.ShowElements(elements);
			for (size_t i = 0; i < elements.size(); ++i)
			{
				auto ithName = std::to_wstring(i);
				AddSubItem(new FloatProperty(ithName.c_str(), elements[i]));
			}
			IsSet(isSet);
		}

		HPS::MatrixKit GetMatrix() const
		{
			return matrix;
		}

	protected:
		void Set() override
		{
			matrix.SetElements(elements);
		}

		void Unset() override
		{
			// nothing to do
		}

	private:
		HPS::MatrixKit matrix;
		HPS::FloatArray elements;
	};

	class ReferenceKeyModellingMatrixProperty : public RootProperty
	{
	private:
		typedef KeyMatrixProperty<HPS::ReferenceKey, &HPS::ReferenceKey::ShowModellingMatrix> ModellingMatrixProperty;

		enum PropertyTypeIndex
		{
			MatrixPropertyIndex = 0,
		};

	public:
		ReferenceKeyModellingMatrixProperty(
			CMFCPropertyGridCtrl & ctrl,
			HPS::ReferenceKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			ctrl.AddProperty(new ModellingMatrixProperty(_T("Modelling Matrix"), this->key));
		}

		void Apply() override
		{
			auto matrixChild = static_cast<ModellingMatrixProperty *>(ctrl.GetProperty(MatrixPropertyIndex));
			if (matrixChild->IsSet())
				key.SetModellingMatrix(matrixChild->GetMatrix());
			else
				key.UnsetModellingMatrix();
		}

	private:
		HPS::ReferenceKey key;
	};

	class SegmentKeyModellingMatrixProperty : public RootProperty
	{
	private:
		typedef KeyMatrixProperty<HPS::SegmentKey, &HPS::SegmentKey::ShowModellingMatrix> ModellingMatrixProperty;

		enum PropertyTypeIndex
		{
			MatrixPropertyIndex = 0,
		};

	public:
		SegmentKeyModellingMatrixProperty(
			CMFCPropertyGridCtrl & ctrl,
			HPS::SegmentKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			ctrl.AddProperty(new ModellingMatrixProperty(_T("Modelling Matrix"), this->key));
		}

		void Apply() override
		{
			auto matrixChild = static_cast<ModellingMatrixProperty *>(ctrl.GetProperty(MatrixPropertyIndex));
			if (matrixChild->IsSet())
				key.SetModellingMatrix(matrixChild->GetMatrix());
			else
				key.UnsetModellingMatrix();
		}

	private:
		HPS::SegmentKey key;
	};

	class SegmentKeyTextureMatrixProperty : public RootProperty
	{
	private:
		typedef KeyMatrixProperty<HPS::SegmentKey, &HPS::SegmentKey::ShowTextureMatrix> TextureMatrixProperty;

		enum PropertyTypeIndex
		{
			MatrixPropertyIndex = 0,
		};

	public:
		SegmentKeyTextureMatrixProperty(
			CMFCPropertyGridCtrl & ctrl,
			HPS::SegmentKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			ctrl.AddProperty(new TextureMatrixProperty(_T("Texture Matrix"), this->key));
		}

		void Apply() override
		{
			auto matrixChild = static_cast<TextureMatrixProperty *>(ctrl.GetProperty(MatrixPropertyIndex));
			if (matrixChild->IsSet())
				key.SetTextureMatrix(matrixChild->GetMatrix());
			else
				key.UnsetTextureMatrix();
		}

	private:
		HPS::SegmentKey key;
	};

	class ConditionProperty : public SettableArrayProperty
	{
	public:
		ConditionProperty(
			HPS::SegmentKey const & key)
			: SettableArrayProperty(_T("Conditions"))
		{
			bool isSet = key.ShowConditions(conditions);
			if (isSet)
				conditionCount = static_cast<unsigned int>(conditions.size());
			else
			{
				conditionCount = 1;
				ResizeArrays();
			}
			AddSubItem(new ArraySizeProperty(_T("Count"), conditionCount));
			AddItems();
			IsSet(isSet);
		}

		HPS::UTF8Array GetConditions() const
		{
			return conditions;
		}

	protected:
		void Set() override
		{
			if (conditionCount < 1)
				return;
			AddOrDeleteItems(conditionCount, static_cast<unsigned int>(conditions.size()));
		}

		void Unset() override
		{
			// nothing to do
		}

		void ResizeArrays() override
		{
			conditions.resize(conditionCount, "condition");
		}

		void AddItems() override
		{
			for (unsigned int i = 0; i < conditionCount; ++i)
			{
				std::wstring name = L"Condition " + std::to_wstring(i);
				auto newCondition = new UTF8Property(name.c_str(), conditions[i]);
				AddSubItem(newCondition);
			}
		}

	private:
		HPS::UTF8Array conditions;
		unsigned int conditionCount;
	};

	class SegmentKeyConditionProperty : public RootProperty
	{
	private:
		enum PropertyTypeIndex
		{
			ConditionPropertyIndex = 0,
		};

	public:
		SegmentKeyConditionProperty(
			CMFCPropertyGridCtrl & ctrl,
			HPS::SegmentKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			ctrl.AddProperty(new ConditionProperty(this->key));
		}

		void Apply() override
		{
			auto conditionChild = static_cast<ConditionProperty *>(ctrl.GetProperty(ConditionPropertyIndex));
			if (conditionChild->IsSet())
				key.SetConditions(conditionChild->GetConditions());
			else
				key.UnsetConditions();
		}

	private:
		HPS::SegmentKey key;
	};

	class KeyUserDataProperty : public SettableProperty
	{
	public:
		KeyUserDataProperty(
			HPS::SegmentKey const & key)
			: SettableProperty(_T("User Data"))
		{
			bool isSet = key.ShowUserData(indices, data);
			if (isSet)
			{
				AddSubItem(new ImmutableSizeTProperty(_T("Count"), indices.size()));
				for (size_t i = 0; i < indices.size(); ++i)
					AddSubItem(new SingleUserDataProperty(i, indices[i], data[i].size()));
			}
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			// nothing to do
		}

		void Unset() override
		{
			// nothing to do
		}

	private:
		HPS::IntPtrTArray indices;
		HPS::ByteArrayArray data;
	};

	class SegmentKeyUserDataProperty : public RootProperty
	{
	private:
		enum PropertyTypeIndex
		{
			UserDataPropertyIndex = 0,
		};

	public:
		SegmentKeyUserDataProperty(
			CMFCPropertyGridCtrl & ctrl,
			HPS::SegmentKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			ctrl.AddProperty(new KeyUserDataProperty(this->key));
		}

		void Apply() override
		{
			auto userDataChild = static_cast<KeyUserDataProperty *>(ctrl.GetProperty(UserDataPropertyIndex));
			if (!userDataChild->IsSet())
				key.UnsetAllUserData();
		}

	private:
		HPS::SegmentKey key;
	};

	class KeyPriorityProperty : public SettableProperty
	{
	public:
		KeyPriorityProperty(
			HPS::SegmentKey const & key)
			: SettableProperty(_T("Priority"))
		{
			bool isSet = key.ShowPriority(priority);
			if (!isSet)
				priority = 0;
			AddSubItem(new IntProperty(_T("Value"), priority));
			IsSet(isSet);
		}

		int GetPriority() const
		{
			return priority;
		}

	protected:
		void Set() override
		{
			// nothing to do
		}

		void Unset() override
		{
			// nothing to do
		}

	private:
		int priority;
	};

	class SegmentKeyPriorityProperty : public RootProperty
	{
	private:
		enum PropertyTypeIndex
		{
			PriorityPropertyIndex = 0,
		};

	public:
		SegmentKeyPriorityProperty(
			CMFCPropertyGridCtrl & ctrl,
			HPS::SegmentKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			ctrl.AddProperty(new KeyPriorityProperty(this->key));
		}

		void Apply() override
		{
			auto priorityChild = static_cast<KeyPriorityProperty *>(ctrl.GetProperty(PriorityPropertyIndex));
			if (priorityChild->IsSet())
				key.SetPriority(priorityChild->GetPriority());
			else
				key.UnsetPriority();
		}

	private:
		HPS::SegmentKey key;
	};

	class MaterialPaletteMaterialArrayProperty : public ArrayProperty
	{
	public:
		MaterialPaletteMaterialArrayProperty(
			HPS::MaterialPaletteDefinition const & definition)
			: ArrayProperty(_T("Materials"))
		{
			definition.Show(materials);
			materialCount = static_cast<unsigned int>(materials.size());
			AddSubItem(new ArraySizeProperty(_T("Count"), materialCount));
			AddItems();
		}

		void OnChildChanged() override
		{
			AddOrDeleteItems(materialCount, static_cast<unsigned int>(materials.size()));
			ArrayProperty::OnChildChanged();
		}

		HPS::MaterialKitArray GetMaterials() const
		{
			return materials;
		}

	protected:
		void ResizeArrays() override
		{
			materials.resize(materialCount);
		}

		void AddItems() override
		{
			for (unsigned int i = 0; i < materialCount; ++i)
			{
				std::wstring name = L"Material " + std::to_wstring(i);
				AddSubItem(new MaterialProperty(name.c_str(), materials[i]));
			}
		}

	private:
		unsigned int materialCount;
		HPS::MaterialKitArray materials;
	};

	class MaterialPaletteDefinitionProperty : public RootProperty
	{
	private:
		enum PropertyTypeIndex
		{
			MaterialArrayPropertyIndex = 0,
		};

	public:
		MaterialPaletteDefinitionProperty(
			CMFCPropertyGridCtrl & ctrl,
			HPS::MaterialPaletteDefinition const & definition)
			: RootProperty(ctrl)
			, definition(definition)
		{
			ctrl.AddProperty(new DefinitionNameProperty<HPS::MaterialPaletteDefinition>(this->definition));
			ctrl.AddProperty(new MaterialPaletteMaterialArrayProperty(this->definition));
		}

		void Apply() override
		{
			auto definitionChild = static_cast<MaterialPaletteMaterialArrayProperty *>(ctrl.GetProperty(MaterialArrayPropertyIndex));
			definition.Set(definitionChild->GetMaterials());
		}

	private:
		HPS::MaterialPaletteDefinition definition;
	};

	typedef ImmutableArraySizeProperty <
		HPS::LinePatternKit,
		HPS::LinePatternParallelKit,
		&HPS::LinePatternKit::ShowParallels
	> BaseLinePatternKitParallelsProperty;
	class LinePatternKitParallelsProperty : public BaseLinePatternKitParallelsProperty
	{
	public:
		LinePatternKitParallelsProperty(
			HPS::LinePatternKit const & kit)
			: BaseLinePatternKitParallelsProperty(_T("Parallels"), _T("Count"), kit)
		{}
	};

	class LinePatternKitJoinProperty : public SettableProperty
	{
	public:
		LinePatternKitJoinProperty(
			HPS::LinePatternKit & kit)
			: SettableProperty(_T("Join"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowJoin(join);
			if (!isSet)
				join = HPS::LinePattern::Join::Mitre;
			AddSubItem(new LinePatternJoinProperty(join));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetJoin(join);
		}

		void Unset() override
		{
			kit.UnsetJoin();
		}

	private:
		HPS::LinePatternKit & kit;
		HPS::LinePattern::Join join;
	};

	class LinePatternDefinitionProperty : public RootProperty
	{
	public:
		LinePatternDefinitionProperty(
			CMFCPropertyGridCtrl & ctrl,
			HPS::LinePatternDefinition const & definition)
			: RootProperty(ctrl)
			, definition(definition)
		{
			this->definition.Show(kit);
			ctrl.AddProperty(new DefinitionNameProperty<HPS::LinePatternDefinition>(this->definition));
			ctrl.AddProperty(new LinePatternKitParallelsProperty(kit));
			ctrl.AddProperty(new LinePatternKitJoinProperty(kit));
		}

		void Apply() override
		{
			definition.Set(kit);
		}

	private:
		HPS::LinePatternDefinition definition;
		HPS::LinePatternKit kit;
	};

	class GlyphKitRadiusProperty : public BaseProperty
	{
	public:
		GlyphKitRadiusProperty(
			HPS::GlyphKit & kit)
			: BaseProperty(_T("Radius"))
			, kit(kit)
		{
			this->kit.ShowRadius(radius);
			AddSubItem(new SByteProperty(_T("Value"), radius));
		}

		void OnChildChanged() override
		{
			kit.SetRadius(radius);
			BaseProperty::OnChildChanged();
		}

	private:
		HPS::GlyphKit & kit;
		HPS::sbyte radius;
	};

	class GlyphKitOffsetProperty : public BaseProperty
	{
	public:
		GlyphKitOffsetProperty(
			HPS::GlyphKit & kit)
			: BaseProperty(_T("Offset"))
			, kit(kit)
		{
			this->kit.ShowOffset(offset);
			AddSubItem(new GlyphPointProperty(_T("Value"), offset));
		}

		void OnChildChanged() override
		{
			kit.SetOffset(offset);
			BaseProperty::OnChildChanged();
		}

	private:
		HPS::GlyphKit & kit;
		HPS::GlyphPoint offset;
	};

	typedef ImmutableArraySizeProperty <
		HPS::GlyphKit,
		HPS::GlyphElement,
		&HPS::GlyphKit::ShowElements
	> BaseGlyphKitElementsProperty;
	class GlyphKitElementsProperty : public BaseGlyphKitElementsProperty
	{
	public:
		GlyphKitElementsProperty(
			HPS::GlyphKit const & kit)
			: BaseGlyphKitElementsProperty(_T("Elements"), _T("Count"), kit)
		{}
	};

	class GlyphDefinitionProperty : public RootProperty
	{
	public:
		GlyphDefinitionProperty(
			CMFCPropertyGridCtrl & ctrl,
			HPS::GlyphDefinition const & definition)
			: RootProperty(ctrl)
			, definition(definition)
		{
			this->definition.Show(kit);
			ctrl.AddProperty(new DefinitionNameProperty<HPS::GlyphDefinition>(this->definition));
			ctrl.AddProperty(new GlyphKitRadiusProperty(kit));
			ctrl.AddProperty(new GlyphKitOffsetProperty(kit));
			ctrl.AddProperty(new GlyphKitElementsProperty(kit));
		}

		void Apply() override
		{
			definition.Set(kit);
		}

	private:
		HPS::GlyphDefinition definition;
		HPS::GlyphKit kit;
	};

	typedef ImmutableArraySizeProperty <
		HPS::ShapeKit,
		HPS::ShapeElement,
		&HPS::ShapeKit::ShowElements
	> BaseShapeKitElementsProperty;
	class ShapeKitElementsProperty : public BaseShapeKitElementsProperty
	{
	public:
		ShapeKitElementsProperty(
			HPS::ShapeKit const & kit)
			: BaseShapeKitElementsProperty(_T("Elements"), _T("Count"), kit)
		{}
	};

	class ShapeDefinitionProperty : public RootProperty
	{
	public:
		ShapeDefinitionProperty(
			CMFCPropertyGridCtrl & ctrl,
			HPS::ShapeDefinition const & definition)
			: RootProperty(ctrl)
		{
			HPS::ShapeKit kit;
			definition.Show(kit);
			ctrl.AddProperty(new DefinitionNameProperty<HPS::ShaderDefinition>(definition));
			ctrl.AddProperty(new ShapeKitElementsProperty(kit));
		}
	};

	class AttributeFilterArrayProperty : public SettableArrayProperty
	{
	public:
		AttributeFilterArrayProperty(
			HPS::AttributeLockTypeArray const & types)
			: SettableArrayProperty(_T("Filter"))
			, types(types)
		{
			bool isSet = !this->types.empty();
			if (isSet)
				typeCount = static_cast<unsigned int>(this->types.size());
			else
			{
				typeCount = 1;
				ResizeArrays();
			}
			AddSubItem(new ArraySizeProperty(_T("Count"), typeCount));
			AddItems();
			IsSet(isSet);
		}

		HPS::AttributeLockTypeArray GetTypes() const
		{
			return types;
		}

	protected:
		void Set() override
		{
			if (typeCount < 1)
				return;
			AddOrDeleteItems(typeCount, static_cast<unsigned int>(types.size()));
		}

		void Unset() override
		{
			// nothing to do
		}

		void ResizeArrays() override
		{
			types.resize(typeCount, HPS::AttributeLock::Type::Everything);
		}

		void AddItems() override
		{
			for (unsigned int i = 0; i < typeCount; ++i)
			{
				std::wstring itemName = L"Type " + std::to_wstring(i);
				AddSubItem(new AttributeLockTypeProperty(types[i], itemName.c_str()));
			}
		}

	private:
		unsigned int typeCount;
		HPS::AttributeLockTypeArray types;
	};

	template <typename Key>
	class AttributeFilterProperty : public RootProperty
	{
	private:
		enum PropertyTypeIndex
		{
			FilterPropertyIndex = 0,
		};

	public:
		AttributeFilterProperty(
			CMFCPropertyGridCtrl & ctrl,
			Key const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.ShowFilter(originalTypes);
			ctrl.AddProperty(new AttributeFilterArrayProperty(originalTypes));
		}

		void Apply() override
		{
			auto filterChild = static_cast<AttributeFilterArrayProperty *>(ctrl.GetProperty(FilterPropertyIndex));
			if (filterChild->IsSet())
			{
				key.UnsetFilter(originalTypes);
				key.SetFilter(filterChild->GetTypes());
			}
			else
				key.UnsetFilter(originalTypes);
		}

	private:
		Key key;
		HPS::AttributeLockTypeArray originalTypes;
	};

	typedef AttributeFilterProperty<HPS::StyleKey> StyleKeyAttributeFilterProperty;
	typedef AttributeFilterProperty<HPS::IncludeKey> IncludeKeyAttributeFilterProperty;

	class DrawingAttributeKitOverrideInternalColorProperty : public SettableProperty
	{
	public:
		DrawingAttributeKitOverrideInternalColorProperty(
			HPS::DrawingAttributeKit & kit)
			: SettableProperty(_T("OverrideInternalColor"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowOverrideInternalColor(_kit);
			if (!isSet)
			{
				_kit = HPS::VisibilityKit();
				_kit.SetEverything(false);
			}

			AddSubItem(new VisibilityKitFacesProperty(_kit));
			AddSubItem(new VisibilityKitLinesProperty(_kit));
			AddSubItem(new VisibilityKitGenericEdgesProperty(_kit));
			AddSubItem(new VisibilityKitTextProperty(_kit));
			AddSubItem(new VisibilityKitVerticesProperty(_kit));
			AddSubItem(new VisibilityKitMarkersProperty(_kit));

			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetOverrideInternalColor(_kit);
		}

		void Unset() override
		{
			kit.UnsetOverrideInternalColor();
		}

	private:
		HPS::DrawingAttributeKit & kit;
		HPS::VisibilityKit _kit;
	};


	class TextKitPositionProperty : public BaseProperty
	{
	public:
		TextKitPositionProperty(
			HPS::TextKit & kit)
			: BaseProperty(_T("Position"))
			, kit(kit)
		{
			this->kit.ShowPosition(_position);
			AddSubItem(new PointProperty(_T("Position"), _position));
		}

		void OnChildChanged() override
		{
			kit.SetPosition(_position);
			BaseProperty::OnChildChanged();
		}

	private:
		HPS::TextKit & kit;
		HPS::Point _position;
	};

	class TextKitTextProperty : public BaseProperty
	{
	public:
		TextKitTextProperty(
			HPS::TextKit & kit)
			: BaseProperty(_T("Text"))
			, kit(kit)
		{
			this->kit.ShowText(_string);
			AddSubItem(new UTF8Property(_T("String"), _string));
		}

		void OnChildChanged() override
		{
			kit.SetText(_string);
			BaseProperty::OnChildChanged();
		}

	private:
		HPS::TextKit & kit;
		HPS::UTF8 _string;
	};

	class TextKitAlignmentProperty : public SettableProperty
	{
	public:
		TextKitAlignmentProperty(
			HPS::TextKit & kit)
			: SettableProperty(_T("Alignment"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowAlignment(_alignment, _reference_frame, _justification);
			if (!isSet)
			{
				_alignment = HPS::Text::Alignment::BottomLeft;
				_reference_frame = HPS::Text::ReferenceFrame::WorldAligned;
				_justification = HPS::Text::Justification::Left;
			}
			AddSubItem(new TextAlignmentProperty(_alignment));
			AddSubItem(new TextReferenceFrameProperty(_reference_frame));
			AddSubItem(new TextJustificationProperty(_justification));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetAlignment(_alignment, _reference_frame, _justification);
		}

		void Unset() override
		{
			kit.UnsetAlignment();
		}

	private:
		HPS::TextKit & kit;
		HPS::Text::Alignment _alignment;
		HPS::Text::ReferenceFrame _reference_frame;
		HPS::Text::Justification _justification;
	};

	class TextKitBoldProperty : public SettableProperty
	{
	public:
		TextKitBoldProperty(
			HPS::TextKit & kit)
			: SettableProperty(_T("Bold"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowBold(_state);
			if (!isSet)
			{
				_state = true;
			}
			AddSubItem(new BoolProperty(_T("State"), _state));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetBold(_state);
		}

		void Unset() override
		{
			kit.UnsetBold();
		}

	private:
		HPS::TextKit & kit;
		bool _state;
	};

	class TextKitItalicProperty : public SettableProperty
	{
	public:
		TextKitItalicProperty(
			HPS::TextKit & kit)
			: SettableProperty(_T("Italic"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowItalic(_state);
			if (!isSet)
			{
				_state = true;
			}
			AddSubItem(new BoolProperty(_T("State"), _state));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetItalic(_state);
		}

		void Unset() override
		{
			kit.UnsetItalic();
		}

	private:
		HPS::TextKit & kit;
		bool _state;
	};

	class TextKitOverlineProperty : public SettableProperty
	{
	public:
		TextKitOverlineProperty(
			HPS::TextKit & kit)
			: SettableProperty(_T("Overline"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowOverline(_state);
			if (!isSet)
			{
				_state = true;
			}
			AddSubItem(new BoolProperty(_T("State"), _state));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetOverline(_state);
		}

		void Unset() override
		{
			kit.UnsetOverline();
		}

	private:
		HPS::TextKit & kit;
		bool _state;
	};

	class TextKitStrikethroughProperty : public SettableProperty
	{
	public:
		TextKitStrikethroughProperty(
			HPS::TextKit & kit)
			: SettableProperty(_T("Strikethrough"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowStrikethrough(_state);
			if (!isSet)
			{
				_state = true;
			}
			AddSubItem(new BoolProperty(_T("State"), _state));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetStrikethrough(_state);
		}

		void Unset() override
		{
			kit.UnsetStrikethrough();
		}

	private:
		HPS::TextKit & kit;
		bool _state;
	};

	class TextKitUnderlineProperty : public SettableProperty
	{
	public:
		TextKitUnderlineProperty(
			HPS::TextKit & kit)
			: SettableProperty(_T("Underline"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowUnderline(_state);
			if (!isSet)
			{
				_state = true;
			}
			AddSubItem(new BoolProperty(_T("State"), _state));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetUnderline(_state);
		}

		void Unset() override
		{
			kit.UnsetUnderline();
		}

	private:
		HPS::TextKit & kit;
		bool _state;
	};

	class TextKitSlantProperty : public SettableProperty
	{
	public:
		TextKitSlantProperty(
			HPS::TextKit & kit)
			: SettableProperty(_T("Slant"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowSlant(_angle);
			if (!isSet)
			{
				_angle = 0.0f;
			}
			AddSubItem(new FloatProperty(_T("Angle"), _angle));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetSlant(_angle);
		}

		void Unset() override
		{
			kit.UnsetSlant();
		}

	private:
		HPS::TextKit & kit;
		float _angle;
	};

	class TextKitLineSpacingProperty : public SettableProperty
	{
	public:
		TextKitLineSpacingProperty(
			HPS::TextKit & kit)
			: SettableProperty(_T("LineSpacing"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowLineSpacing(_multiplier);
			if (!isSet)
			{
				_multiplier = 0.0f;
			}
			AddSubItem(new FloatProperty(_T("Multiplier"), _multiplier));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetLineSpacing(_multiplier);
		}

		void Unset() override
		{
			kit.UnsetLineSpacing();
		}

	private:
		HPS::TextKit & kit;
		float _multiplier;
	};

	class TextKitExtraSpaceProperty : public SettableProperty
	{
	public:
		TextKitExtraSpaceProperty(
			HPS::TextKit & kit)
			: SettableProperty(_T("ExtraSpace"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowExtraSpace(_state, _size, _units);
			if (!isSet)
			{
				_state = true;
				_size = 0.0f;
				_units = HPS::Text::SizeUnits::Points;
			}
			auto boolProperty = new ConditionalBoolProperty(_T("State"), _state);
			AddSubItem(boolProperty);
			AddSubItem(new FloatProperty(_T("Size"), _size));
			AddSubItem(new TextSizeUnitsProperty(_units));
			boolProperty->EnableValidProperties();
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetExtraSpace(_state, _size, _units);
		}

		void Unset() override
		{
			kit.UnsetExtraSpace();
		}

	private:
		HPS::TextKit & kit;
		bool _state;
		float _size;
		HPS::Text::SizeUnits _units;
	};

	class TextKitGreekingProperty : public SettableProperty
	{
	public:
		TextKitGreekingProperty(
			HPS::TextKit & kit)
			: SettableProperty(_T("Greeking"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowGreeking(_state, _size, _units, _mode);
			if (!isSet)
			{
				_state = true;
				_size = 0.0f;
				_units = HPS::Text::GreekingUnits::Pixels;
				_mode = HPS::Text::GreekingMode::Lines;
			}
			auto boolProperty = new ConditionalBoolProperty(_T("State"), _state);
			AddSubItem(boolProperty);
			AddSubItem(new FloatProperty(_T("Size"), _size));
			AddSubItem(new TextGreekingUnitsProperty(_units));
			AddSubItem(new TextGreekingModeProperty(_mode));
			boolProperty->EnableValidProperties();
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetGreeking(_state, _size, _units, _mode);
		}

		void Unset() override
		{
			kit.UnsetGreeking();
		}

	private:
		HPS::TextKit & kit;
		bool _state;
		float _size;
		HPS::Text::GreekingUnits _units;
		HPS::Text::GreekingMode _mode;
	};

	class TextKitSizeToleranceProperty : public SettableProperty
	{
	public:
		TextKitSizeToleranceProperty(
			HPS::TextKit & kit)
			: SettableProperty(_T("SizeTolerance"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowSizeTolerance(_state, _size, _units);
			if (!isSet)
			{
				_state = true;
				_size = 0.0f;
				_units = HPS::Text::SizeToleranceUnits::Percent;
			}
			auto boolProperty = new ConditionalBoolProperty(_T("State"), _state);
			AddSubItem(boolProperty);
			AddSubItem(new FloatProperty(_T("Size"), _size));
			AddSubItem(new TextSizeToleranceUnitsProperty(_units));
			boolProperty->EnableValidProperties();
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetSizeTolerance(_state, _size, _units);
		}

		void Unset() override
		{
			kit.UnsetSizeTolerance();
		}

	private:
		HPS::TextKit & kit;
		bool _state;
		float _size;
		HPS::Text::SizeToleranceUnits _units;
	};

	class TextKitSizeProperty : public SettableProperty
	{
	public:
		TextKitSizeProperty(
			HPS::TextKit & kit)
			: SettableProperty(_T("Size"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowSize(_size, _units);
			if (!isSet)
			{
				_size = 0.0f;
				_units = HPS::Text::SizeUnits::Points;
			}
			AddSubItem(new FloatProperty(_T("Size"), _size));
			AddSubItem(new TextSizeUnitsProperty(_units));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetSize(_size, _units);
		}

		void Unset() override
		{
			kit.UnsetSize();
		}

	private:
		HPS::TextKit & kit;
		float _size;
		HPS::Text::SizeUnits _units;
	};

	class TextKitFontProperty : public SettableProperty
	{
	public:
		TextKitFontProperty(
			HPS::TextKit & kit)
			: SettableProperty(_T("Font"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowFont(_name);
			if (!isSet)
			{
				_name = "name";
			}
			AddSubItem(new UTF8Property(_T("Name"), _name));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetFont(_name);
		}

		void Unset() override
		{
			kit.UnsetFont();
		}

	private:
		HPS::TextKit & kit;
		HPS::UTF8 _name;
	};

	class TextKitTransformProperty : public SettableProperty
	{
	public:
		TextKitTransformProperty(
			HPS::TextKit & kit)
			: SettableProperty(_T("Transform"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowTransform(_trans);
			if (!isSet)
			{
				_trans = HPS::Text::Transform::Transformable;
			}
			AddSubItem(new TextTransformProperty(_trans));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetTransform(_trans);
		}

		void Unset() override
		{
			kit.UnsetTransform();
		}

	private:
		HPS::TextKit & kit;
		HPS::Text::Transform _trans;
	};

	class TextKitRendererProperty : public SettableProperty
	{
	public:
		TextKitRendererProperty(
			HPS::TextKit & kit)
			: SettableProperty(_T("Renderer"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowRenderer(_renderer);
			if (!isSet)
			{
				_renderer = HPS::Text::Renderer::Default;
			}
			AddSubItem(new TextRendererProperty(_renderer));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetRenderer(_renderer);
		}

		void Unset() override
		{
			kit.UnsetRenderer();
		}

	private:
		HPS::TextKit & kit;
		HPS::Text::Renderer _renderer;
	};

	class TextKitPreferenceProperty : public SettableProperty
	{
	public:
		TextKitPreferenceProperty(
			HPS::TextKit & kit)
			: SettableProperty(_T("Preference"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowPreference(_cutoff, _units, _smaller, _larger);
			if (!isSet)
			{
				_cutoff = 0.0f;
				_units = HPS::Text::SizeUnits::Points;
				_smaller = HPS::Text::Preference::Default;
				_larger = HPS::Text::Preference::Default;
			}
			AddSubItem(new FloatProperty(_T("Cutoff"), _cutoff));
			AddSubItem(new TextSizeUnitsProperty(_units));
			AddSubItem(new TextPreferenceProperty(_smaller));
			AddSubItem(new TextPreferenceProperty(_larger));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetPreference(_cutoff, _units, _smaller, _larger);
		}

		void Unset() override
		{
			kit.UnsetPreference();
		}

	private:
		HPS::TextKit & kit;
		float _cutoff;
		HPS::Text::SizeUnits _units;
		HPS::Text::Preference _smaller;
		HPS::Text::Preference _larger;
	};

	class TextKitPathProperty : public SettableProperty
	{
	public:
		TextKitPathProperty(
			HPS::TextKit & kit)
			: SettableProperty(_T("Path"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowPath(_path);
			if (!isSet)
			{
				_path = HPS::Vector::Unit();
			}
			AddSubItem(new VectorProperty(_T("Path"), _path));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetPath(_path);
		}

		void Unset() override
		{
			kit.UnsetPath();
		}

	private:
		HPS::TextKit & kit;
		HPS::Vector _path;
	};

	class TextKitSpacingProperty : public SettableProperty
	{
	public:
		TextKitSpacingProperty(
			HPS::TextKit & kit)
			: SettableProperty(_T("Spacing"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowSpacing(_multiplier);
			if (!isSet)
			{
				_multiplier = 0.0f;
			}
			AddSubItem(new FloatProperty(_T("Multiplier"), _multiplier));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetSpacing(_multiplier);
		}

		void Unset() override
		{
			kit.UnsetSpacing();
		}

	private:
		HPS::TextKit & kit;
		float _multiplier;
	};

	class TextKitBackgroundProperty : public SettableProperty
	{
	public:
		TextKitBackgroundProperty(
			HPS::TextKit & kit)
			: SettableProperty(_T("Background"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowBackground(_state, _name);
			if (!isSet)
			{
				_state = true;
				_name = "name";
			}
			auto boolProperty = new ConditionalBoolProperty(_T("State"), _state);
			AddSubItem(boolProperty);
			AddSubItem(new UTF8Property(_T("Name"), _name));
			boolProperty->EnableValidProperties();
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetBackground(_state, _name);
		}

		void Unset() override
		{
			kit.UnsetBackground();
		}

	private:
		HPS::TextKit & kit;
		bool _state;
		HPS::UTF8 _name;
	};

	class TextKitBackgroundStyleProperty : public SettableProperty
	{
	public:
		TextKitBackgroundStyleProperty(
			HPS::TextKit & kit)
			: SettableProperty(_T("BackgroundStyle"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowBackgroundStyle(_name);
			if (!isSet)
			{
				_name = "name";
			}
			AddSubItem(new UTF8Property(_T("Name"), _name));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetBackgroundStyle(_name);
		}

		void Unset() override
		{
			kit.UnsetBackgroundStyle();
		}

	private:
		HPS::TextKit & kit;
		HPS::UTF8 _name;
	};

	class TextKitProperty : public RootProperty
	{
	public:
		TextKitProperty(
			CMFCPropertyGridCtrl & ctrl,
			HPS::TextKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.Show(kit);
			ctrl.AddProperty(new TextKitPositionProperty(kit));
			ctrl.AddProperty(new TextKitTextProperty(kit));
			ctrl.AddProperty(new TextKitColorProperty(kit));
			ctrl.AddProperty(new TextKitModellingMatrixProperty(kit));
			ctrl.AddProperty(new TextKitAlignmentProperty(kit));
			ctrl.AddProperty(new TextKitBoldProperty(kit));
			ctrl.AddProperty(new TextKitItalicProperty(kit));
			ctrl.AddProperty(new TextKitOverlineProperty(kit));
			ctrl.AddProperty(new TextKitStrikethroughProperty(kit));
			ctrl.AddProperty(new TextKitUnderlineProperty(kit));
			ctrl.AddProperty(new TextKitSlantProperty(kit));
			ctrl.AddProperty(new TextKitLineSpacingProperty(kit));
			ctrl.AddProperty(new TextKitRotationProperty(kit));
			ctrl.AddProperty(new TextKitExtraSpaceProperty(kit));
			ctrl.AddProperty(new TextKitGreekingProperty(kit));
			ctrl.AddProperty(new TextKitSizeToleranceProperty(kit));
			ctrl.AddProperty(new TextKitSizeProperty(kit));
			ctrl.AddProperty(new TextKitFontProperty(kit));
			ctrl.AddProperty(new TextKitTransformProperty(kit));
			ctrl.AddProperty(new TextKitRendererProperty(kit));
			ctrl.AddProperty(new TextKitPreferenceProperty(kit));
			ctrl.AddProperty(new TextKitPathProperty(kit));
			ctrl.AddProperty(new TextKitSpacingProperty(kit));
			ctrl.AddProperty(new TextKitBackgroundProperty(kit));
			ctrl.AddProperty(new TextKitBackgroundMarginsProperty(kit));
			ctrl.AddProperty(new TextKitBackgroundStyleProperty(kit));
			ctrl.AddProperty(new TextKitLeaderLinesProperty(kit));
			ctrl.AddProperty(new TextKitRegionProperty(kit));
			ctrl.AddProperty(new TextKitPriorityProperty(kit));
			ctrl.AddProperty(new TextKitUserDataProperty(kit));
		}

		void Apply() override
		{
			key.Consume(kit);
		}

	private:
		HPS::TextKey key;
		HPS::TextKit kit;
	};

	class TransparencyKitMethodProperty : public SettableProperty
	{
	public:
		TransparencyKitMethodProperty(
			HPS::TransparencyKit & kit)
			: SettableProperty(_T("Method"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowMethod(_style);
			if (!isSet)
			{
				_style = HPS::Transparency::Method::Blended;
			}
			AddSubItem(new TransparencyMethodProperty(_style));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetMethod(_style);
		}

		void Unset() override
		{
			kit.UnsetMethod();
		}

	private:
		HPS::TransparencyKit & kit;
		HPS::Transparency::Method _style;
	};

	class TransparencyKitAlgorithmProperty : public SettableProperty
	{
	public:
		TransparencyKitAlgorithmProperty(
			HPS::TransparencyKit & kit)
			: SettableProperty(_T("Algorithm"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowAlgorithm(_algorithm);
			if (!isSet)
			{
				_algorithm = HPS::Transparency::Algorithm::DepthPeeling;
			}
			AddSubItem(new TransparencyAlgorithmProperty(_algorithm));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetAlgorithm(_algorithm);
		}

		void Unset() override
		{
			kit.UnsetAlgorithm();
		}

	private:
		HPS::TransparencyKit & kit;
		HPS::Transparency::Algorithm _algorithm;
	};

	class TransparencyKitDepthPeelingLayersProperty : public SettableProperty
	{
	public:
		TransparencyKitDepthPeelingLayersProperty(
			HPS::TransparencyKit & kit)
			: SettableProperty(_T("DepthPeelingLayers"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowDepthPeelingLayers(_layers);
			if (!isSet)
			{
				_layers = 0;
			}
			AddSubItem(new UnsignedIntProperty(_T("Layers"), _layers));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetDepthPeelingLayers(_layers);
		}

		void Unset() override
		{
			kit.UnsetDepthPeelingLayers();
		}

	private:
		HPS::TransparencyKit & kit;
		unsigned int _layers;
	};

	class TransparencyKitDepthPeelingMinimumAreaProperty : public SettableProperty
	{
	public:
		TransparencyKitDepthPeelingMinimumAreaProperty(
			HPS::TransparencyKit & kit)
			: SettableProperty(_T("DepthPeelingMinimumArea"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowDepthPeelingMinimumArea(_area, _units);
			if (!isSet)
			{
				_area = 0.0f;
				_units = HPS::Transparency::AreaUnits::Pixels;
			}
			AddSubItem(new FloatProperty(_T("Area"), _area));
			AddSubItem(new TransparencyAreaUnitsProperty(_units));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetDepthPeelingMinimumArea(_area, _units);
		}

		void Unset() override
		{
			kit.UnsetDepthPeelingMinimumArea();
		}

	private:
		HPS::TransparencyKit & kit;
		float _area;
		HPS::Transparency::AreaUnits _units;
	};

	class TransparencyKitDepthWritingProperty : public SettableProperty
	{
	public:
		TransparencyKitDepthWritingProperty(
			HPS::TransparencyKit & kit)
			: SettableProperty(_T("DepthWriting"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowDepthWriting(_state);
			if (!isSet)
			{
				_state = true;
			}
			AddSubItem(new BoolProperty(_T("State"), _state));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetDepthWriting(_state);
		}

		void Unset() override
		{
			kit.UnsetDepthWriting();
		}

	private:
		HPS::TransparencyKit & kit;
		bool _state;
	};

	class TransparencyKitDepthPeelingPreferenceProperty : public SettableProperty
	{
	public:
		TransparencyKitDepthPeelingPreferenceProperty(
			HPS::TransparencyKit & kit)
			: SettableProperty(_T("DepthPeelingPreference"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowDepthPeelingPreference(_preference);
			if (!isSet)
			{
				_preference = HPS::Transparency::Preference::Fastest;
			}
			AddSubItem(new TransparencyPreferenceProperty(_preference));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetDepthPeelingPreference(_preference);
		}

		void Unset() override
		{
			kit.UnsetDepthPeelingPreference();
		}

	private:
		HPS::TransparencyKit & kit;
		HPS::Transparency::Preference _preference;
	};

	class TransparencyKitProperty : public RootProperty
	{
	public:
		TransparencyKitProperty(
			CMFCPropertyGridCtrl & ctrl,
			HPS::SegmentKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.ShowTransparency(kit);
			ctrl.AddProperty(new TransparencyKitMethodProperty(kit));
			ctrl.AddProperty(new TransparencyKitAlgorithmProperty(kit));
			ctrl.AddProperty(new TransparencyKitDepthPeelingLayersProperty(kit));
			ctrl.AddProperty(new TransparencyKitDepthPeelingMinimumAreaProperty(kit));
			ctrl.AddProperty(new TransparencyKitDepthWritingProperty(kit));
			ctrl.AddProperty(new TransparencyKitDepthPeelingPreferenceProperty(kit));
		}

		void Apply() override
		{
			key.UnsetTransparency();
			key.SetTransparency(kit);
		}

	private:
		HPS::SegmentKey key;
		HPS::TransparencyKit kit;
	};

	class CullingKitDeferralExtentProperty : public SettableProperty
	{
	public:
		CullingKitDeferralExtentProperty(
			HPS::CullingKit & kit)
			: SettableProperty(_T("DeferralExtent"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowDeferralExtent(_state, _pixels);
			if (!isSet)
			{
				_state = true;
				_pixels = 0;
			}
			auto boolProperty = new ConditionalBoolProperty(_T("State"), _state);
			AddSubItem(boolProperty);
			AddSubItem(new UnsignedIntProperty(_T("Pixels"), _pixels));
			boolProperty->EnableValidProperties();
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetDeferralExtent(_state, _pixels);
		}

		void Unset() override
		{
			kit.UnsetDeferralExtent();
		}

	private:
		HPS::CullingKit & kit;
		bool _state;
		unsigned int _pixels;
	};

	class CullingKitExtentProperty : public SettableProperty
	{
	public:
		CullingKitExtentProperty(
			HPS::CullingKit & kit)
			: SettableProperty(_T("Extent"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowExtent(_state, _pixels);
			if (!isSet)
			{
				_state = true;
				_pixels = 0;
			}
			auto boolProperty = new ConditionalBoolProperty(_T("State"), _state);
			AddSubItem(boolProperty);
			AddSubItem(new UnsignedIntProperty(_T("Pixels"), _pixels));
			boolProperty->EnableValidProperties();
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetExtent(_state, _pixels);
		}

		void Unset() override
		{
			kit.UnsetExtent();
		}

	private:
		HPS::CullingKit & kit;
		bool _state;
		unsigned int _pixels;
	};

	class CullingKitBackFaceProperty : public SettableProperty
	{
	public:
		CullingKitBackFaceProperty(
			HPS::CullingKit & kit)
			: SettableProperty(_T("BackFace"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowBackFace(_state);
			if (!isSet)
			{
				_state = true;
			}
			AddSubItem(new BoolProperty(_T("State"), _state));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetBackFace(_state);
		}

		void Unset() override
		{
			kit.UnsetBackFace();
		}

	private:
		HPS::CullingKit & kit;
		bool _state;
	};

	class CullingKitVectorProperty : public SettableProperty
	{
	public:
		CullingKitVectorProperty(
			HPS::CullingKit & kit)
			: SettableProperty(_T("Vector"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowVector(_state, _vector);
			if (!isSet)
			{
				_state = true;
				_vector = HPS::Vector::Unit();
			}
			auto boolProperty = new ConditionalBoolProperty(_T("State"), _state);
			AddSubItem(boolProperty);
			AddSubItem(new VectorProperty(_T("Vector"), _vector));
			boolProperty->EnableValidProperties();
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetVector(_state, _vector);
		}

		void Unset() override
		{
			kit.UnsetVector();
		}

	private:
		HPS::CullingKit & kit;
		bool _state;
		HPS::Vector _vector;
	};

	class CullingKitVectorToleranceProperty : public SettableProperty
	{
	public:
		CullingKitVectorToleranceProperty(
			HPS::CullingKit & kit)
			: SettableProperty(_T("VectorTolerance"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowVectorTolerance(_tolerance_degrees);
			if (!isSet)
			{
				_tolerance_degrees = 0.0f;
			}
			AddSubItem(new FloatProperty(_T("Tolerance Degrees"), _tolerance_degrees));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetVectorTolerance(_tolerance_degrees);
		}

		void Unset() override
		{
			kit.UnsetVectorTolerance();
		}

	private:
		HPS::CullingKit & kit;
		float _tolerance_degrees;
	};

	class CullingKitFrustumProperty : public SettableProperty
	{
	public:
		CullingKitFrustumProperty(
			HPS::CullingKit & kit)
			: SettableProperty(_T("Frustum"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowFrustum(_state);
			if (!isSet)
			{
				_state = true;
			}
			AddSubItem(new BoolProperty(_T("State"), _state));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetFrustum(_state);
		}

		void Unset() override
		{
			kit.UnsetFrustum();
		}

	private:
		HPS::CullingKit & kit;
		bool _state;
	};

	class CullingKitVolumeProperty : public SettableProperty
	{
	public:
		CullingKitVolumeProperty(
			HPS::CullingKit & kit)
			: SettableProperty(_T("Volume"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowVolume(_state, _volume);
			if (!isSet)
			{
				_state = true;
				_volume = HPS::SimpleCuboid(HPS::Point(-1, -1, -1), HPS::Point(1, 1, 1));
			}
			AddSubItem(new BoolProperty(_T("State"), _state));
			AddSubItem(new SimpleCuboidProperty(_T("Volume"), _volume));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetVolume(_state, _volume);
		}

		void Unset() override
		{
			kit.UnsetVolume();
		}

	private:
		HPS::CullingKit & kit;
		bool _state;
		HPS::SimpleCuboid _volume;
	};

	class CullingKitDistanceProperty : public SettableProperty
	{
	public:
		CullingKitDistanceProperty(
			HPS::CullingKit & kit)
			: SettableProperty(_T("Distance"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowDistance(_state, _max_distance);
			if (!isSet)
			{
				_state = true;
				_max_distance = 0.0f;
			}
			AddSubItem(new BoolProperty(_T("State"), _state));
			AddSubItem(new FloatProperty(_T("Max Distance"), _max_distance));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetDistance(_state, _max_distance);
		}

		void Unset() override
		{
			kit.UnsetDistance();
		}

	private:
		HPS::CullingKit & kit;
		bool _state;
		float _max_distance;
	};

	class CullingKitProperty : public RootProperty
	{
	public:
		CullingKitProperty(
			CMFCPropertyGridCtrl & ctrl,
			HPS::SegmentKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.ShowCulling(kit);
			ctrl.AddProperty(new CullingKitDeferralExtentProperty(kit));
			ctrl.AddProperty(new CullingKitExtentProperty(kit));
			ctrl.AddProperty(new CullingKitBackFaceProperty(kit));
			ctrl.AddProperty(new CullingKitVectorProperty(kit));
			ctrl.AddProperty(new CullingKitVectorToleranceProperty(kit));
			ctrl.AddProperty(new CullingKitFrustumProperty(kit));
			ctrl.AddProperty(new CullingKitVolumeProperty(kit));
			ctrl.AddProperty(new CullingKitDistanceProperty(kit));
		}

		void Apply() override
		{
			key.UnsetCulling();
			key.SetCulling(kit);
		}

	private:
		HPS::SegmentKey key;
		HPS::CullingKit kit;
	};

	class TextAttributeKitAlignmentProperty : public SettableProperty
	{
	public:
		TextAttributeKitAlignmentProperty(
			HPS::TextAttributeKit & kit)
			: SettableProperty(_T("Alignment"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowAlignment(_align, _ref, _justify);
			if (!isSet)
			{
				_align = HPS::Text::Alignment::BottomLeft;
				_ref = HPS::Text::ReferenceFrame::WorldAligned;
				_justify = HPS::Text::Justification::Left;
			}
			AddSubItem(new TextAlignmentProperty(_align));
			AddSubItem(new TextReferenceFrameProperty(_ref));
			AddSubItem(new TextJustificationProperty(_justify));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetAlignment(_align, _ref, _justify);
		}

		void Unset() override
		{
			kit.UnsetAlignment();
		}

	private:
		HPS::TextAttributeKit & kit;
		HPS::Text::Alignment _align;
		HPS::Text::ReferenceFrame _ref;
		HPS::Text::Justification _justify;
	};

	class TextAttributeKitBoldProperty : public SettableProperty
	{
	public:
		TextAttributeKitBoldProperty(
			HPS::TextAttributeKit & kit)
			: SettableProperty(_T("Bold"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowBold(_state);
			if (!isSet)
			{
				_state = true;
			}
			AddSubItem(new BoolProperty(_T("State"), _state));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetBold(_state);
		}

		void Unset() override
		{
			kit.UnsetBold();
		}

	private:
		HPS::TextAttributeKit & kit;
		bool _state;
	};

	class TextAttributeKitItalicProperty : public SettableProperty
	{
	public:
		TextAttributeKitItalicProperty(
			HPS::TextAttributeKit & kit)
			: SettableProperty(_T("Italic"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowItalic(_state);
			if (!isSet)
			{
				_state = true;
			}
			AddSubItem(new BoolProperty(_T("State"), _state));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetItalic(_state);
		}

		void Unset() override
		{
			kit.UnsetItalic();
		}

	private:
		HPS::TextAttributeKit & kit;
		bool _state;
	};

	class TextAttributeKitOverlineProperty : public SettableProperty
	{
	public:
		TextAttributeKitOverlineProperty(
			HPS::TextAttributeKit & kit)
			: SettableProperty(_T("Overline"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowOverline(_state);
			if (!isSet)
			{
				_state = true;
			}
			AddSubItem(new BoolProperty(_T("State"), _state));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetOverline(_state);
		}

		void Unset() override
		{
			kit.UnsetOverline();
		}

	private:
		HPS::TextAttributeKit & kit;
		bool _state;
	};

	class TextAttributeKitStrikethroughProperty : public SettableProperty
	{
	public:
		TextAttributeKitStrikethroughProperty(
			HPS::TextAttributeKit & kit)
			: SettableProperty(_T("Strikethrough"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowStrikethrough(_state);
			if (!isSet)
			{
				_state = true;
			}
			AddSubItem(new BoolProperty(_T("State"), _state));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetStrikethrough(_state);
		}

		void Unset() override
		{
			kit.UnsetStrikethrough();
		}

	private:
		HPS::TextAttributeKit & kit;
		bool _state;
	};

	class TextAttributeKitUnderlineProperty : public SettableProperty
	{
	public:
		TextAttributeKitUnderlineProperty(
			HPS::TextAttributeKit & kit)
			: SettableProperty(_T("Underline"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowUnderline(_state);
			if (!isSet)
			{
				_state = true;
			}
			AddSubItem(new BoolProperty(_T("State"), _state));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetUnderline(_state);
		}

		void Unset() override
		{
			kit.UnsetUnderline();
		}

	private:
		HPS::TextAttributeKit & kit;
		bool _state;
	};

	class TextAttributeKitSlantProperty : public SettableProperty
	{
	public:
		TextAttributeKitSlantProperty(
			HPS::TextAttributeKit & kit)
			: SettableProperty(_T("Slant"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowSlant(_angle);
			if (!isSet)
			{
				_angle = 0.0f;
			}
			AddSubItem(new FloatProperty(_T("Angle"), _angle));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetSlant(_angle);
		}

		void Unset() override
		{
			kit.UnsetSlant();
		}

	private:
		HPS::TextAttributeKit & kit;
		float _angle;
	};

	class TextAttributeKitLineSpacingProperty : public SettableProperty
	{
	public:
		TextAttributeKitLineSpacingProperty(
			HPS::TextAttributeKit & kit)
			: SettableProperty(_T("LineSpacing"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowLineSpacing(_multiplier);
			if (!isSet)
			{
				_multiplier = 0.0f;
			}
			AddSubItem(new FloatProperty(_T("Multiplier"), _multiplier));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetLineSpacing(_multiplier);
		}

		void Unset() override
		{
			kit.UnsetLineSpacing();
		}

	private:
		HPS::TextAttributeKit & kit;
		float _multiplier;
	};

	class TextAttributeKitExtraSpaceProperty : public SettableProperty
	{
	public:
		TextAttributeKitExtraSpaceProperty(
			HPS::TextAttributeKit & kit)
			: SettableProperty(_T("ExtraSpace"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowExtraSpace(_state, _size, _units);
			if (!isSet)
			{
				_state = true;
				_size = 0.0f;
				_units = HPS::Text::SizeUnits::Points;
			}
			auto boolProperty = new ConditionalBoolProperty(_T("State"), _state);
			AddSubItem(boolProperty);
			AddSubItem(new FloatProperty(_T("Size"), _size));
			AddSubItem(new TextSizeUnitsProperty(_units));
			boolProperty->EnableValidProperties();
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetExtraSpace(_state, _size, _units);
		}

		void Unset() override
		{
			kit.UnsetExtraSpace();
		}

	private:
		HPS::TextAttributeKit & kit;
		bool _state;
		float _size;
		HPS::Text::SizeUnits _units;
	};

	class TextAttributeKitGreekingProperty : public SettableProperty
	{
	public:
		TextAttributeKitGreekingProperty(
			HPS::TextAttributeKit & kit)
			: SettableProperty(_T("Greeking"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowGreeking(_state, _size, _units, _mode);
			if (!isSet)
			{
				_state = true;
				_size = 0.0f;
				_units = HPS::Text::GreekingUnits::Pixels;
				_mode = HPS::Text::GreekingMode::Lines;
			}
			auto boolProperty = new ConditionalBoolProperty(_T("State"), _state);
			AddSubItem(boolProperty);
			AddSubItem(new FloatProperty(_T("Size"), _size));
			AddSubItem(new TextGreekingUnitsProperty(_units));
			AddSubItem(new TextGreekingModeProperty(_mode));
			boolProperty->EnableValidProperties();
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetGreeking(_state, _size, _units, _mode);
		}

		void Unset() override
		{
			kit.UnsetGreeking();
		}

	private:
		HPS::TextAttributeKit & kit;
		bool _state;
		float _size;
		HPS::Text::GreekingUnits _units;
		HPS::Text::GreekingMode _mode;
	};

	class TextAttributeKitSizeToleranceProperty : public SettableProperty
	{
	public:
		TextAttributeKitSizeToleranceProperty(
			HPS::TextAttributeKit & kit)
			: SettableProperty(_T("SizeTolerance"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowSizeTolerance(_state, _size, _units);
			if (!isSet)
			{
				_state = true;
				_size = 0.0f;
				_units = HPS::Text::SizeToleranceUnits::Percent;
			}
			auto boolProperty = new ConditionalBoolProperty(_T("State"), _state);
			AddSubItem(boolProperty);
			AddSubItem(new FloatProperty(_T("Size"), _size));
			AddSubItem(new TextSizeToleranceUnitsProperty(_units));
			boolProperty->EnableValidProperties();
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetSizeTolerance(_state, _size, _units);
		}

		void Unset() override
		{
			kit.UnsetSizeTolerance();
		}

	private:
		HPS::TextAttributeKit & kit;
		bool _state;
		float _size;
		HPS::Text::SizeToleranceUnits _units;
	};

	class TextAttributeKitSizeProperty : public SettableProperty
	{
	public:
		TextAttributeKitSizeProperty(
			HPS::TextAttributeKit & kit)
			: SettableProperty(_T("Size"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowSize(_size, _units);
			if (!isSet)
			{
				_size = 0.0f;
				_units = HPS::Text::SizeUnits::Points;
			}
			AddSubItem(new FloatProperty(_T("Size"), _size));
			AddSubItem(new TextSizeUnitsProperty(_units));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetSize(_size, _units);
		}

		void Unset() override
		{
			kit.UnsetSize();
		}

	private:
		HPS::TextAttributeKit & kit;
		float _size;
		HPS::Text::SizeUnits _units;
	};

	class TextAttributeKitFontProperty : public SettableProperty
	{
	public:
		TextAttributeKitFontProperty(
			HPS::TextAttributeKit & kit)
			: SettableProperty(_T("Font"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowFont(_name);
			if (!isSet)
			{
				_name = "name";
			}
			AddSubItem(new UTF8Property(_T("Name"), _name));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetFont(_name);
		}

		void Unset() override
		{
			kit.UnsetFont();
		}

	private:
		HPS::TextAttributeKit & kit;
		HPS::UTF8 _name;
	};

	class TextAttributeKitTransformProperty : public SettableProperty
	{
	public:
		TextAttributeKitTransformProperty(
			HPS::TextAttributeKit & kit)
			: SettableProperty(_T("Transform"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowTransform(_trans);
			if (!isSet)
			{
				_trans = HPS::Text::Transform::Transformable;
			}
			AddSubItem(new TextTransformProperty(_trans));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetTransform(_trans);
		}

		void Unset() override
		{
			kit.UnsetTransform();
		}

	private:
		HPS::TextAttributeKit & kit;
		HPS::Text::Transform _trans;
	};

	class TextAttributeKitRendererProperty : public SettableProperty
	{
	public:
		TextAttributeKitRendererProperty(
			HPS::TextAttributeKit & kit)
			: SettableProperty(_T("Renderer"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowRenderer(_rend);
			if (!isSet)
			{
				_rend = HPS::Text::Renderer::Default;
			}
			AddSubItem(new TextRendererProperty(_rend));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetRenderer(_rend);
		}

		void Unset() override
		{
			kit.UnsetRenderer();
		}

	private:
		HPS::TextAttributeKit & kit;
		HPS::Text::Renderer _rend;
	};

	class TextAttributeKitPreferenceProperty : public SettableProperty
	{
	public:
		TextAttributeKitPreferenceProperty(
			HPS::TextAttributeKit & kit)
			: SettableProperty(_T("Preference"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowPreference(_cutoff, _units, _smaller, _larger);
			if (!isSet)
			{
				_cutoff = 0.0f;
				_units = HPS::Text::SizeUnits::Points;
				_smaller = HPS::Text::Preference::Default;
				_larger = HPS::Text::Preference::Default;
			}
			AddSubItem(new FloatProperty(_T("Cutoff"), _cutoff));
			AddSubItem(new TextSizeUnitsProperty(_units));
			AddSubItem(new TextPreferenceProperty(_smaller));
			AddSubItem(new TextPreferenceProperty(_larger));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetPreference(_cutoff, _units, _smaller, _larger);
		}

		void Unset() override
		{
			kit.UnsetPreference();
		}

	private:
		HPS::TextAttributeKit & kit;
		float _cutoff;
		HPS::Text::SizeUnits _units;
		HPS::Text::Preference _smaller;
		HPS::Text::Preference _larger;
	};

	class TextAttributeKitPathProperty : public SettableProperty
	{
	public:
		TextAttributeKitPathProperty(
			HPS::TextAttributeKit & kit)
			: SettableProperty(_T("Path"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowPath(_path);
			if (!isSet)
			{
				_path = HPS::Vector::Unit();
			}
			AddSubItem(new VectorProperty(_T("Path"), _path));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetPath(_path);
		}

		void Unset() override
		{
			kit.UnsetPath();
		}

	private:
		HPS::TextAttributeKit & kit;
		HPS::Vector _path;
	};

	class TextAttributeKitSpacingProperty : public SettableProperty
	{
	public:
		TextAttributeKitSpacingProperty(
			HPS::TextAttributeKit & kit)
			: SettableProperty(_T("Spacing"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowSpacing(_multiplier);
			if (!isSet)
			{
				_multiplier = 0.0f;
			}
			AddSubItem(new FloatProperty(_T("Multiplier"), _multiplier));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetSpacing(_multiplier);
		}

		void Unset() override
		{
			kit.UnsetSpacing();
		}

	private:
		HPS::TextAttributeKit & kit;
		float _multiplier;
	};

	class TextAttributeKitBackgroundProperty : public SettableProperty
	{
	public:
		TextAttributeKitBackgroundProperty(
			HPS::TextAttributeKit & kit)
			: SettableProperty(_T("Background"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowBackground(_state, _name);
			if (!isSet)
			{
				_state = true;
				_name = "name";
			}
			auto boolProperty = new ConditionalBoolProperty(_T("State"), _state);
			AddSubItem(boolProperty);
			AddSubItem(new UTF8Property(_T("Name"), _name));
			boolProperty->EnableValidProperties();
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetBackground(_state, _name);
		}

		void Unset() override
		{
			kit.UnsetBackground();
		}

	private:
		HPS::TextAttributeKit & kit;
		bool _state;
		HPS::UTF8 _name;
	};

	class TextAttributeKitBackgroundStyleProperty : public SettableProperty
	{
	public:
		TextAttributeKitBackgroundStyleProperty(
			HPS::TextAttributeKit & kit)
			: SettableProperty(_T("BackgroundStyle"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowBackgroundStyle(_name);
			if (!isSet)
			{
				_name = "name";
			}
			AddSubItem(new UTF8Property(_T("Name"), _name));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetBackgroundStyle(_name);
		}

		void Unset() override
		{
			kit.UnsetBackgroundStyle();
		}

	private:
		HPS::TextAttributeKit & kit;
		HPS::UTF8 _name;
	};

	class TextAttributeKitProperty : public RootProperty
	{
	public:
		TextAttributeKitProperty(
			CMFCPropertyGridCtrl & ctrl,
			HPS::SegmentKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.ShowTextAttribute(kit);
			ctrl.AddProperty(new TextAttributeKitAlignmentProperty(kit));
			ctrl.AddProperty(new TextAttributeKitBoldProperty(kit));
			ctrl.AddProperty(new TextAttributeKitItalicProperty(kit));
			ctrl.AddProperty(new TextAttributeKitOverlineProperty(kit));
			ctrl.AddProperty(new TextAttributeKitStrikethroughProperty(kit));
			ctrl.AddProperty(new TextAttributeKitUnderlineProperty(kit));
			ctrl.AddProperty(new TextAttributeKitSlantProperty(kit));
			ctrl.AddProperty(new TextAttributeKitLineSpacingProperty(kit));
			ctrl.AddProperty(new TextAttributeKitRotationProperty(kit));
			ctrl.AddProperty(new TextAttributeKitExtraSpaceProperty(kit));
			ctrl.AddProperty(new TextAttributeKitGreekingProperty(kit));
			ctrl.AddProperty(new TextAttributeKitSizeToleranceProperty(kit));
			ctrl.AddProperty(new TextAttributeKitSizeProperty(kit));
			ctrl.AddProperty(new TextAttributeKitFontProperty(kit));
			ctrl.AddProperty(new TextAttributeKitTransformProperty(kit));
			ctrl.AddProperty(new TextAttributeKitRendererProperty(kit));
			ctrl.AddProperty(new TextAttributeKitPreferenceProperty(kit));
			ctrl.AddProperty(new TextAttributeKitPathProperty(kit));
			ctrl.AddProperty(new TextAttributeKitSpacingProperty(kit));
			ctrl.AddProperty(new TextAttributeKitBackgroundProperty(kit));
			ctrl.AddProperty(new TextAttributeKitBackgroundMarginsProperty(kit));
			ctrl.AddProperty(new TextAttributeKitBackgroundStyleProperty(kit));
		}

		void Apply() override
		{
			key.UnsetTextAttribute();
			key.SetTextAttribute(kit);
		}

	private:
		HPS::SegmentKey key;
		HPS::TextAttributeKit kit;
	};

	class EdgeAttributeKitPatternProperty : public SettableProperty
	{
	public:
		EdgeAttributeKitPatternProperty(
			HPS::EdgeAttributeKit & kit)
			: SettableProperty(_T("Pattern"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowPattern(_pattern_name);
			if (!isSet)
			{
				_pattern_name = "pattern_name";
			}
			AddSubItem(new UTF8Property(_T("Pattern Name"), _pattern_name));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetPattern(_pattern_name);
		}

		void Unset() override
		{
			kit.UnsetPattern();
		}

	private:
		HPS::EdgeAttributeKit & kit;
		HPS::UTF8 _pattern_name;
	};

	class EdgeAttributeKitWeightProperty : public SettableProperty
	{
	public:
		EdgeAttributeKitWeightProperty(
			HPS::EdgeAttributeKit & kit)
			: SettableProperty(_T("Weight"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowWeight(_weight, _units);
			if (!isSet)
			{
				_weight = 0.0f;
				_units = HPS::Edge::SizeUnits::ScaleFactor;
			}
			AddSubItem(new FloatProperty(_T("Weight"), _weight));
			AddSubItem(new EdgeSizeUnitsProperty(_units));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetWeight(_weight, _units);
		}

		void Unset() override
		{
			kit.UnsetWeight();
		}

	private:
		HPS::EdgeAttributeKit & kit;
		float _weight;
		HPS::Edge::SizeUnits _units;
	};

	class EdgeAttributeKitHardAngleProperty : public SettableProperty
	{
	public:
		EdgeAttributeKitHardAngleProperty(
			HPS::EdgeAttributeKit & kit)
			: SettableProperty(_T("HardAngle"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowHardAngle(_angle);
			if (!isSet)
			{
				_angle = 0.0f;
			}
			AddSubItem(new FloatProperty(_T("Angle"), _angle));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetHardAngle(_angle);
		}

		void Unset() override
		{
			kit.UnsetHardAngle();
		}

	private:
		HPS::EdgeAttributeKit & kit;
		float _angle;
	};

	class EdgeAttributeKitProperty : public RootProperty
	{
	public:
		EdgeAttributeKitProperty(
			CMFCPropertyGridCtrl & ctrl,
			HPS::SegmentKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.ShowEdgeAttribute(kit);
			ctrl.AddProperty(new EdgeAttributeKitPatternProperty(kit));
			ctrl.AddProperty(new EdgeAttributeKitWeightProperty(kit));
			ctrl.AddProperty(new EdgeAttributeKitHardAngleProperty(kit));
		}

		void Apply() override
		{
			key.UnsetEdgeAttribute();
			key.SetEdgeAttribute(kit);
		}

	private:
		HPS::SegmentKey key;
		HPS::EdgeAttributeKit kit;
	};

	class CurveAttributeKitBudgetProperty : public SettableProperty
	{
	public:
		CurveAttributeKitBudgetProperty(
			HPS::CurveAttributeKit & kit)
			: SettableProperty(_T("Budget"))
			, kit(kit)
		{
			size_t _budget_st;
			bool isSet = this->kit.ShowBudget(_budget_st);
			_budget = static_cast<unsigned int>(_budget_st);
			if (!isSet)
			{
				_budget = 0;
			}
			AddSubItem(new UnsignedIntProperty(_T("Budget"), _budget));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetBudget(_budget);
		}

		void Unset() override
		{
			kit.UnsetBudget();
		}

	private:
		HPS::CurveAttributeKit & kit;
		unsigned int _budget;
	};

	class CurveAttributeKitContinuedBudgetProperty : public SettableProperty
	{
	public:
		CurveAttributeKitContinuedBudgetProperty(
			HPS::CurveAttributeKit & kit)
			: SettableProperty(_T("ContinuedBudget"))
			, kit(kit)
		{
			size_t _budget_st;
			bool isSet = this->kit.ShowContinuedBudget(_state, _budget_st);
			_budget = static_cast<unsigned int>(_budget_st);
			if (!isSet)
			{
				_state = true;
				_budget = 0;
			}
			auto boolProperty = new ConditionalBoolProperty(_T("State"), _state);
			AddSubItem(boolProperty);
			AddSubItem(new UnsignedIntProperty(_T("Budget"), _budget));
			boolProperty->EnableValidProperties();
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetContinuedBudget(_state, _budget);
		}

		void Unset() override
		{
			kit.UnsetContinuedBudget();
		}

	private:
		HPS::CurveAttributeKit & kit;
		bool _state;
		unsigned int _budget;
	};

	class CurveAttributeKitViewDependentProperty : public SettableProperty
	{
	public:
		CurveAttributeKitViewDependentProperty(
			HPS::CurveAttributeKit & kit)
			: SettableProperty(_T("ViewDependent"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowViewDependent(_state);
			if (!isSet)
			{
				_state = true;
			}
			AddSubItem(new BoolProperty(_T("State"), _state));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetViewDependent(_state);
		}

		void Unset() override
		{
			kit.UnsetViewDependent();
		}

	private:
		HPS::CurveAttributeKit & kit;
		bool _state;
	};

	class CurveAttributeKitMaximumDeviationProperty : public SettableProperty
	{
	public:
		CurveAttributeKitMaximumDeviationProperty(
			HPS::CurveAttributeKit & kit)
			: SettableProperty(_T("MaximumDeviation"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowMaximumDeviation(_deviation);
			if (!isSet)
			{
				_deviation = 0.0f;
			}
			AddSubItem(new FloatProperty(_T("Deviation"), _deviation));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetMaximumDeviation(_deviation);
		}

		void Unset() override
		{
			kit.UnsetMaximumDeviation();
		}

	private:
		HPS::CurveAttributeKit & kit;
		float _deviation;
	};

	class CurveAttributeKitMaximumAngleProperty : public SettableProperty
	{
	public:
		CurveAttributeKitMaximumAngleProperty(
			HPS::CurveAttributeKit & kit)
			: SettableProperty(_T("MaximumAngle"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowMaximumAngle(_degrees);
			if (!isSet)
			{
				_degrees = 0.0f;
			}
			AddSubItem(new FloatProperty(_T("Degrees"), _degrees));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetMaximumAngle(_degrees);
		}

		void Unset() override
		{
			kit.UnsetMaximumAngle();
		}

	private:
		HPS::CurveAttributeKit & kit;
		float _degrees;
	};

	class CurveAttributeKitMaximumLengthProperty : public SettableProperty
	{
	public:
		CurveAttributeKitMaximumLengthProperty(
			HPS::CurveAttributeKit & kit)
			: SettableProperty(_T("MaximumLength"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowMaximumLength(_length);
			if (!isSet)
			{
				_length = 0.0f;
			}
			AddSubItem(new FloatProperty(_T("Length"), _length));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetMaximumLength(_length);
		}

		void Unset() override
		{
			kit.UnsetMaximumLength();
		}

	private:
		HPS::CurveAttributeKit & kit;
		float _length;
	};

	class CurveAttributeKitProperty : public RootProperty
	{
	public:
		CurveAttributeKitProperty(
			CMFCPropertyGridCtrl & ctrl,
			HPS::SegmentKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.ShowCurveAttribute(kit);
			ctrl.AddProperty(new CurveAttributeKitBudgetProperty(kit));
			ctrl.AddProperty(new CurveAttributeKitContinuedBudgetProperty(kit));
			ctrl.AddProperty(new CurveAttributeKitViewDependentProperty(kit));
			ctrl.AddProperty(new CurveAttributeKitMaximumDeviationProperty(kit));
			ctrl.AddProperty(new CurveAttributeKitMaximumAngleProperty(kit));
			ctrl.AddProperty(new CurveAttributeKitMaximumLengthProperty(kit));
		}

		void Apply() override
		{
			key.UnsetCurveAttribute();
			key.SetCurveAttribute(kit);
		}

	private:
		HPS::SegmentKey key;
		HPS::CurveAttributeKit kit;
	};

	class PBRMaterialKitBaseColorMapProperty : public SettableProperty
	{
	public:
		PBRMaterialKitBaseColorMapProperty(
			HPS::PBRMaterialKit & kit)
			: SettableProperty(_T("BaseColorMap"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowBaseColorMap(_texture_name);
			if (!isSet)
			{
				_texture_name = "texture_name";
			}
			AddSubItem(new UTF8Property(_T("Texture Name"), _texture_name));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetBaseColorMap(_texture_name);
		}

		void Unset() override
		{
			kit.UnsetBaseColorMap();
		}

	private:
		HPS::PBRMaterialKit & kit;
		HPS::UTF8 _texture_name;
	};

	class PBRMaterialKitNormalMapProperty : public SettableProperty
	{
	public:
		PBRMaterialKitNormalMapProperty(
			HPS::PBRMaterialKit & kit)
			: SettableProperty(_T("NormalMap"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowNormalMap(_texture_name);
			if (!isSet)
			{
				_texture_name = "texture_name";
			}
			AddSubItem(new UTF8Property(_T("Texture Name"), _texture_name));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetNormalMap(_texture_name);
		}

		void Unset() override
		{
			kit.UnsetNormalMap();
		}

	private:
		HPS::PBRMaterialKit & kit;
		HPS::UTF8 _texture_name;
	};

	class PBRMaterialKitEmissiveMapProperty : public SettableProperty
	{
	public:
		PBRMaterialKitEmissiveMapProperty(
			HPS::PBRMaterialKit & kit)
			: SettableProperty(_T("EmissiveMap"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowEmissiveMap(_texture_name);
			if (!isSet)
			{
				_texture_name = "texture_name";
			}
			AddSubItem(new UTF8Property(_T("Texture Name"), _texture_name));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetEmissiveMap(_texture_name);
		}

		void Unset() override
		{
			kit.UnsetEmissiveMap();
		}

	private:
		HPS::PBRMaterialKit & kit;
		HPS::UTF8 _texture_name;
	};

	class PBRMaterialKitMetalnessMapProperty : public SettableProperty
	{
	public:
		PBRMaterialKitMetalnessMapProperty(
			HPS::PBRMaterialKit & kit)
			: SettableProperty(_T("MetalnessMap"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowMetalnessMap(_texture_name, _channel);
			if (!isSet)
			{
				_texture_name = "texture_name";
				_channel = HPS::Material::Texture::ChannelMapping::Red;
			}
			AddSubItem(new UTF8Property(_T("Texture Name"), _texture_name));
			AddSubItem(new MaterialTextureChannelMappingProperty(_channel));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetMetalnessMap(_texture_name, _channel);
		}

		void Unset() override
		{
			kit.UnsetMetalnessMap();
		}

	private:
		HPS::PBRMaterialKit & kit;
		HPS::UTF8 _texture_name;
		HPS::Material::Texture::ChannelMapping _channel;
	};

	class PBRMaterialKitRoughnessMapProperty : public SettableProperty
	{
	public:
		PBRMaterialKitRoughnessMapProperty(
			HPS::PBRMaterialKit & kit)
			: SettableProperty(_T("RoughnessMap"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowRoughnessMap(_texture_name, _channel);
			if (!isSet)
			{
				_texture_name = "texture_name";
				_channel = HPS::Material::Texture::ChannelMapping::Red;
			}
			AddSubItem(new UTF8Property(_T("Texture Name"), _texture_name));
			AddSubItem(new MaterialTextureChannelMappingProperty(_channel));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetRoughnessMap(_texture_name, _channel);
		}

		void Unset() override
		{
			kit.UnsetRoughnessMap();
		}

	private:
		HPS::PBRMaterialKit & kit;
		HPS::UTF8 _texture_name;
		HPS::Material::Texture::ChannelMapping _channel;
	};

	class PBRMaterialKitOcclusionMapProperty : public SettableProperty
	{
	public:
		PBRMaterialKitOcclusionMapProperty(
			HPS::PBRMaterialKit & kit)
			: SettableProperty(_T("OcclusionMap"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowOcclusionMap(_texture_name, _channel);
			if (!isSet)
			{
				_texture_name = "texture_name";
				_channel = HPS::Material::Texture::ChannelMapping::Red;
			}
			AddSubItem(new UTF8Property(_T("Texture Name"), _texture_name));
			AddSubItem(new MaterialTextureChannelMappingProperty(_channel));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetOcclusionMap(_texture_name, _channel);
		}

		void Unset() override
		{
			kit.UnsetOcclusionMap();
		}

	private:
		HPS::PBRMaterialKit & kit;
		HPS::UTF8 _texture_name;
		HPS::Material::Texture::ChannelMapping _channel;
	};

	class PBRMaterialKitBaseColorFactorProperty : public SettableProperty
	{
	public:
		PBRMaterialKitBaseColorFactorProperty(
			HPS::PBRMaterialKit & kit)
			: SettableProperty(_T("BaseColorFactor"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowBaseColorFactor(_color);
			if (!isSet)
			{
				_color = HPS::RGBAColor::Black();
			}
			AddSubItem(new RGBAColorProperty(_T("Color"), _color));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetBaseColorFactor(_color);
		}

		void Unset() override
		{
			kit.UnsetBaseColorFactor();
		}

	private:
		HPS::PBRMaterialKit & kit;
		HPS::RGBAColor _color;
	};

	class PBRMaterialKitNormalFactorProperty : public SettableProperty
	{
	public:
		PBRMaterialKitNormalFactorProperty(
			HPS::PBRMaterialKit & kit)
			: SettableProperty(_T("NormalFactor"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowNormalFactor(_factor);
			if (!isSet)
			{
				_factor = 0.0f;
			}
			AddSubItem(new FloatProperty(_T("Factor"), _factor));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetNormalFactor(_factor);
		}

		void Unset() override
		{
			kit.UnsetNormalFactor();
		}

	private:
		HPS::PBRMaterialKit & kit;
		float _factor;
	};

	class PBRMaterialKitMetalnessFactorProperty : public SettableProperty
	{
	public:
		PBRMaterialKitMetalnessFactorProperty(
			HPS::PBRMaterialKit & kit)
			: SettableProperty(_T("MetalnessFactor"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowMetalnessFactor(_factor);
			if (!isSet)
			{
				_factor = 0.0f;
			}
			AddSubItem(new FloatProperty(_T("Factor"), _factor));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetMetalnessFactor(_factor);
		}

		void Unset() override
		{
			kit.UnsetMetalnessFactor();
		}

	private:
		HPS::PBRMaterialKit & kit;
		float _factor;
	};

	class PBRMaterialKitRoughnessFactorProperty : public SettableProperty
	{
	public:
		PBRMaterialKitRoughnessFactorProperty(
			HPS::PBRMaterialKit & kit)
			: SettableProperty(_T("RoughnessFactor"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowRoughnessFactor(_factor);
			if (!isSet)
			{
				_factor = 0.0f;
			}
			AddSubItem(new FloatProperty(_T("Factor"), _factor));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetRoughnessFactor(_factor);
		}

		void Unset() override
		{
			kit.UnsetRoughnessFactor();
		}

	private:
		HPS::PBRMaterialKit & kit;
		float _factor;
	};

	class PBRMaterialKitOcclusionFactorProperty : public SettableProperty
	{
	public:
		PBRMaterialKitOcclusionFactorProperty(
			HPS::PBRMaterialKit & kit)
			: SettableProperty(_T("OcclusionFactor"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowOcclusionFactor(_factor);
			if (!isSet)
			{
				_factor = 0.0f;
			}
			AddSubItem(new FloatProperty(_T("Factor"), _factor));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetOcclusionFactor(_factor);
		}

		void Unset() override
		{
			kit.UnsetOcclusionFactor();
		}

	private:
		HPS::PBRMaterialKit & kit;
		float _factor;
	};

	class PBRMaterialKitAlphaFactorProperty : public SettableProperty
	{
	public:
		PBRMaterialKitAlphaFactorProperty(
			HPS::PBRMaterialKit & kit)
			: SettableProperty(_T("AlphaFactor"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowAlphaFactor(_factor, _mask);
			if (!isSet)
			{
				_factor = 0.0f;
				_mask = true;
			}
			AddSubItem(new FloatProperty(_T("Factor"), _factor));
			AddSubItem(new BoolProperty(_T("Mask"), _mask));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetAlphaFactor(_factor, _mask);
		}

		void Unset() override
		{
			kit.UnsetAlphaFactor();
		}

	private:
		HPS::PBRMaterialKit & kit;
		float _factor;
		bool _mask;
	};

	class PBRMaterialKitProperty : public RootProperty
	{
	public:
		PBRMaterialKitProperty(
			CMFCPropertyGridCtrl & ctrl,
			HPS::SegmentKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.ShowPBRMaterial(kit);
			ctrl.AddProperty(new PBRMaterialKitBaseColorMapProperty(kit));
			ctrl.AddProperty(new PBRMaterialKitNormalMapProperty(kit));
			ctrl.AddProperty(new PBRMaterialKitEmissiveMapProperty(kit));
			ctrl.AddProperty(new PBRMaterialKitMetalnessMapProperty(kit));
			ctrl.AddProperty(new PBRMaterialKitRoughnessMapProperty(kit));
			ctrl.AddProperty(new PBRMaterialKitOcclusionMapProperty(kit));
			ctrl.AddProperty(new PBRMaterialKitBaseColorFactorProperty(kit));
			ctrl.AddProperty(new PBRMaterialKitNormalFactorProperty(kit));
			ctrl.AddProperty(new PBRMaterialKitMetalnessFactorProperty(kit));
			ctrl.AddProperty(new PBRMaterialKitRoughnessFactorProperty(kit));
			ctrl.AddProperty(new PBRMaterialKitOcclusionFactorProperty(kit));
			ctrl.AddProperty(new PBRMaterialKitAlphaFactorProperty(kit));
		}

		void Apply() override
		{
			key.UnsetPBRMaterial();
			key.SetPBRMaterial(kit);
		}

	private:
		HPS::SegmentKey key;
		HPS::PBRMaterialKit kit;
	};

	class MarkerKitPointProperty : public BaseProperty
	{
	public:
		MarkerKitPointProperty(
			HPS::MarkerKit & kit)
			: BaseProperty(_T("Point"))
			, kit(kit)
		{
			this->kit.ShowPoint(_point);
			AddSubItem(new PointProperty(_T("Point"), _point));
		}

		void OnChildChanged() override
		{
			kit.SetPoint(_point);
			BaseProperty::OnChildChanged();
		}

	private:
		HPS::MarkerKit & kit;
		HPS::Point _point;
	};

	class MarkerKitProperty : public RootProperty
	{
	public:
		MarkerKitProperty(
			CMFCPropertyGridCtrl & ctrl,
			HPS::MarkerKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.Show(kit);
			ctrl.AddProperty(new MarkerKitPointProperty(kit));
			ctrl.AddProperty(new MarkerKitPriorityProperty(kit));
			ctrl.AddProperty(new MarkerKitUserDataProperty(kit));
		}

		void Apply() override
		{
			key.Consume(kit);
		}

	private:
		HPS::MarkerKey key;
		HPS::MarkerKit kit;
	};

	class DistantLightKitDirectionProperty : public BaseProperty
	{
	public:
		DistantLightKitDirectionProperty(
			HPS::DistantLightKit & kit)
			: BaseProperty(_T("Direction"))
			, kit(kit)
		{
			this->kit.ShowDirection(_vector);
			AddSubItem(new VectorProperty(_T("Vector"), _vector));
		}

		void OnChildChanged() override
		{
			kit.SetDirection(_vector);
			BaseProperty::OnChildChanged();
		}

	private:
		HPS::DistantLightKit & kit;
		HPS::Vector _vector;
	};

	class DistantLightKitCameraRelativeProperty : public BaseProperty
	{
	public:
		DistantLightKitCameraRelativeProperty(
			HPS::DistantLightKit & kit)
			: BaseProperty(_T("CameraRelative"))
			, kit(kit)
		{
			this->kit.ShowCameraRelative(_state);
			AddSubItem(new BoolProperty(_T("State"), _state));
		}

		void OnChildChanged() override
		{
			kit.SetCameraRelative(_state);
			BaseProperty::OnChildChanged();
		}

	private:
		HPS::DistantLightKit & kit;
		bool _state;
	};

	class DistantLightKitProperty : public RootProperty
	{
	public:
		DistantLightKitProperty(
			CMFCPropertyGridCtrl & ctrl,
			HPS::DistantLightKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.Show(kit);
			ctrl.AddProperty(new DistantLightKitDirectionProperty(kit));
			ctrl.AddProperty(new DistantLightKitColorProperty(kit));
			ctrl.AddProperty(new DistantLightKitCameraRelativeProperty(kit));
			ctrl.AddProperty(new DistantLightKitPriorityProperty(kit));
			ctrl.AddProperty(new DistantLightKitUserDataProperty(kit));
		}

		void Apply() override
		{
			key.Consume(kit);
		}

	private:
		HPS::DistantLightKey key;
		HPS::DistantLightKit kit;
	};

	class CuttingSectionAttributeKitCuttingLevelProperty : public SettableProperty
	{
	public:
		CuttingSectionAttributeKitCuttingLevelProperty(
			HPS::CuttingSectionAttributeKit & kit)
			: SettableProperty(_T("CuttingLevel"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowCuttingLevel(_level);
			if (!isSet)
			{
				_level = HPS::CuttingSection::CuttingLevel::Global;
			}
			AddSubItem(new CuttingSectionCuttingLevelProperty(_level));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetCuttingLevel(_level);
		}

		void Unset() override
		{
			kit.UnsetCuttingLevel();
		}

	private:
		HPS::CuttingSectionAttributeKit & kit;
		HPS::CuttingSection::CuttingLevel _level;
	};

	class CuttingSectionAttributeKitCappingLevelProperty : public SettableProperty
	{
	public:
		CuttingSectionAttributeKitCappingLevelProperty(
			HPS::CuttingSectionAttributeKit & kit)
			: SettableProperty(_T("CappingLevel"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowCappingLevel(_level);
			if (!isSet)
			{
				_level = HPS::CuttingSection::CappingLevel::Segment;
			}
			AddSubItem(new CuttingSectionCappingLevelProperty(_level));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetCappingLevel(_level);
		}

		void Unset() override
		{
			kit.UnsetCappingLevel();
		}

	private:
		HPS::CuttingSectionAttributeKit & kit;
		HPS::CuttingSection::CappingLevel _level;
	};

	class CuttingSectionAttributeKitCappingUsageProperty : public SettableProperty
	{
	public:
		CuttingSectionAttributeKitCappingUsageProperty(
			HPS::CuttingSectionAttributeKit & kit)
			: SettableProperty(_T("CappingUsage"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowCappingUsage(_usage);
			if (!isSet)
			{
				_usage = HPS::CuttingSection::CappingUsage::Visibility;
			}
			AddSubItem(new CuttingSectionCappingUsageProperty(_usage));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetCappingUsage(_usage);
		}

		void Unset() override
		{
			kit.UnsetCappingUsage();
		}

	private:
		HPS::CuttingSectionAttributeKit & kit;
		HPS::CuttingSection::CappingUsage _usage;
	};

	class CuttingSectionAttributeKitMaterialPreferenceProperty : public SettableProperty
	{
	public:
		CuttingSectionAttributeKitMaterialPreferenceProperty(
			HPS::CuttingSectionAttributeKit & kit)
			: SettableProperty(_T("MaterialPreference"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowMaterialPreference(_preference);
			if (!isSet)
			{
				_preference = HPS::CuttingSection::MaterialPreference::Explicit;
			}
			AddSubItem(new CuttingSectionMaterialPreferenceProperty(_preference));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetMaterialPreference(_preference);
		}

		void Unset() override
		{
			kit.UnsetMaterialPreference();
		}

	private:
		HPS::CuttingSectionAttributeKit & kit;
		HPS::CuttingSection::MaterialPreference _preference;
	};

	class CuttingSectionAttributeKitEdgeWeightProperty : public SettableProperty
	{
	public:
		CuttingSectionAttributeKitEdgeWeightProperty(
			HPS::CuttingSectionAttributeKit & kit)
			: SettableProperty(_T("EdgeWeight"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowEdgeWeight(_weight, _units);
			if (!isSet)
			{
				_weight = 0.0f;
				_units = HPS::Line::SizeUnits::ScaleFactor;
			}
			AddSubItem(new FloatProperty(_T("Weight"), _weight));
			AddSubItem(new LineSizeUnitsProperty(_units));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetEdgeWeight(_weight, _units);
		}

		void Unset() override
		{
			kit.UnsetEdgeWeight();
		}

	private:
		HPS::CuttingSectionAttributeKit & kit;
		float _weight;
		HPS::Line::SizeUnits _units;
	};

	class CuttingSectionAttributeKitToleranceProperty : public SettableProperty
	{
	public:
		CuttingSectionAttributeKitToleranceProperty(
			HPS::CuttingSectionAttributeKit & kit)
			: SettableProperty(_T("Tolerance"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowTolerance(_tolerance, _units);
			if (!isSet)
			{
				_tolerance = 0.0f;
				_units = HPS::CuttingSection::ToleranceUnits::WorldSpace;
			}
			AddSubItem(new FloatProperty(_T("Tolerance"), _tolerance));
			AddSubItem(new CuttingSectionToleranceUnitsProperty(_units));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetTolerance(_tolerance, _units);
		}

		void Unset() override
		{
			kit.UnsetTolerance();
		}

	private:
		HPS::CuttingSectionAttributeKit & kit;
		float _tolerance;
		HPS::CuttingSection::ToleranceUnits _units;
	};

	class CuttingSectionAttributeKitProperty : public RootProperty
	{
	public:
		CuttingSectionAttributeKitProperty(
			CMFCPropertyGridCtrl & ctrl,
			HPS::SegmentKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.ShowCuttingSectionAttribute(kit);
			ctrl.AddProperty(new CuttingSectionAttributeKitCuttingLevelProperty(kit));
			ctrl.AddProperty(new CuttingSectionAttributeKitCappingLevelProperty(kit));
			ctrl.AddProperty(new CuttingSectionAttributeKitCappingUsageProperty(kit));
			ctrl.AddProperty(new CuttingSectionAttributeKitMaterialPreferenceProperty(kit));
			ctrl.AddProperty(new CuttingSectionAttributeKitEdgeWeightProperty(kit));
			ctrl.AddProperty(new CuttingSectionAttributeKitToleranceProperty(kit));
		}

		void Apply() override
		{
			key.UnsetCuttingSectionAttribute();
			key.SetCuttingSectionAttribute(kit);
		}

	private:
		HPS::SegmentKey key;
		HPS::CuttingSectionAttributeKit kit;
	};

	class CylinderAttributeKitTessellationProperty : public SettableProperty
	{
	public:
		CylinderAttributeKitTessellationProperty(
			HPS::CylinderAttributeKit & kit)
			: SettableProperty(_T("Tessellation"))
			, kit(kit)
		{
			size_t _facets_st;
			bool isSet = this->kit.ShowTessellation(_facets_st);
			_facets = static_cast<unsigned int>(_facets_st);
			if (!isSet)
			{
				_facets = 0;
			}
			AddSubItem(new UnsignedIntProperty(_T("Facets"), _facets));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetTessellation(_facets);
		}

		void Unset() override
		{
			kit.UnsetTessellation();
		}

	private:
		HPS::CylinderAttributeKit & kit;
		unsigned int _facets;
	};

	class CylinderAttributeKitOrientationProperty : public SettableProperty
	{
	public:
		CylinderAttributeKitOrientationProperty(
			HPS::CylinderAttributeKit & kit)
			: SettableProperty(_T("Orientation"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowOrientation(_orientation);
			if (!isSet)
			{
				_orientation = HPS::Cylinder::Orientation::Default;
			}
			AddSubItem(new CylinderOrientationProperty(_orientation));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetOrientation(_orientation);
		}

		void Unset() override
		{
			kit.UnsetOrientation();
		}

	private:
		HPS::CylinderAttributeKit & kit;
		HPS::Cylinder::Orientation _orientation;
	};

	class CylinderAttributeKitProperty : public RootProperty
	{
	public:
		CylinderAttributeKitProperty(
			CMFCPropertyGridCtrl & ctrl,
			HPS::SegmentKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.ShowCylinderAttribute(kit);
			ctrl.AddProperty(new CylinderAttributeKitTessellationProperty(kit));
			ctrl.AddProperty(new CylinderAttributeKitOrientationProperty(kit));
		}

		void Apply() override
		{
			key.UnsetCylinderAttribute();
			key.SetCylinderAttribute(kit);
		}

	private:
		HPS::SegmentKey key;
		HPS::CylinderAttributeKit kit;
	};

	class SphereKitCenterProperty : public BaseProperty
	{
	public:
		SphereKitCenterProperty(
			HPS::SphereKit & kit)
			: BaseProperty(_T("Center"))
			, kit(kit)
		{
			this->kit.ShowCenter(_center);
			AddSubItem(new PointProperty(_T("Center"), _center));
		}

		void OnChildChanged() override
		{
			kit.SetCenter(_center);
			BaseProperty::OnChildChanged();
		}

	private:
		HPS::SphereKit & kit;
		HPS::Point _center;
	};

	class SphereKitRadiusProperty : public BaseProperty
	{
	public:
		SphereKitRadiusProperty(
			HPS::SphereKit & kit)
			: BaseProperty(_T("Radius"))
			, kit(kit)
		{
			this->kit.ShowRadius(_radius);
			AddSubItem(new FloatProperty(_T("Radius"), _radius));
		}

		void OnChildChanged() override
		{
			kit.SetRadius(_radius);
			BaseProperty::OnChildChanged();
		}

	private:
		HPS::SphereKit & kit;
		float _radius;
	};

	class SphereKitBasisProperty : public BaseProperty
	{
	public:
		SphereKitBasisProperty(
			HPS::SphereKit & kit)
			: BaseProperty(_T("Basis"))
			, kit(kit)
		{
			this->kit.ShowBasis(_vertical, _horizontal);
			AddSubItem(new VectorProperty(_T("Vertical"), _vertical));
			AddSubItem(new VectorProperty(_T("Horizontal"), _horizontal));
		}

		void OnChildChanged() override
		{
			kit.SetBasis(_vertical, _horizontal);
			BaseProperty::OnChildChanged();
		}

	private:
		HPS::SphereKit & kit;
		HPS::Vector _vertical;
		HPS::Vector _horizontal;
	};

	class SphereKitProperty : public RootProperty
	{
	public:
		SphereKitProperty(
			CMFCPropertyGridCtrl & ctrl,
			HPS::SphereKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.Show(kit);
			ctrl.AddProperty(new SphereKitCenterProperty(kit));
			ctrl.AddProperty(new SphereKitRadiusProperty(kit));
			ctrl.AddProperty(new SphereKitBasisProperty(kit));
			ctrl.AddProperty(new SphereKitPriorityProperty(kit));
			ctrl.AddProperty(new SphereKitUserDataProperty(kit));
		}

		void Apply() override
		{
			key.Consume(kit);
		}

	private:
		HPS::SphereKey key;
		HPS::SphereKit kit;
	};

	class SphereAttributeKitTessellationProperty : public SettableProperty
	{
	public:
		SphereAttributeKitTessellationProperty(
			HPS::SphereAttributeKit & kit)
			: SettableProperty(_T("Tessellation"))
			, kit(kit)
		{
			size_t _facets_st;
			bool isSet = this->kit.ShowTessellation(_facets_st);
			_facets = static_cast<unsigned int>(_facets_st);
			if (!isSet)
			{
				_facets = 0;
			}
			AddSubItem(new UnsignedIntProperty(_T("Facets"), _facets));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetTessellation(_facets);
		}

		void Unset() override
		{
			kit.UnsetTessellation();
		}

	private:
		HPS::SphereAttributeKit & kit;
		unsigned int _facets;
	};

	class SphereAttributeKitProperty : public RootProperty
	{
	public:
		SphereAttributeKitProperty(
			CMFCPropertyGridCtrl & ctrl,
			HPS::SegmentKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.ShowSphereAttribute(kit);
			ctrl.AddProperty(new SphereAttributeKitTessellationProperty(kit));
		}

		void Apply() override
		{
			key.UnsetSphereAttribute();
			key.SetSphereAttribute(kit);
		}

	private:
		HPS::SegmentKey key;
		HPS::SphereAttributeKit kit;
	};

	class CircleKitCenterProperty : public BaseProperty
	{
	public:
		CircleKitCenterProperty(
			HPS::CircleKit & kit)
			: BaseProperty(_T("Center"))
			, kit(kit)
		{
			this->kit.ShowCenter(_center);
			AddSubItem(new PointProperty(_T("Center"), _center));
		}

		void OnChildChanged() override
		{
			kit.SetCenter(_center);
			BaseProperty::OnChildChanged();
		}

	private:
		HPS::CircleKit & kit;
		HPS::Point _center;
	};

	class CircleKitRadiusProperty : public BaseProperty
	{
	public:
		CircleKitRadiusProperty(
			HPS::CircleKit & kit)
			: BaseProperty(_T("Radius"))
			, kit(kit)
		{
			this->kit.ShowRadius(_radius);
			AddSubItem(new FloatProperty(_T("Radius"), _radius));
		}

		void OnChildChanged() override
		{
			kit.SetRadius(_radius);
			BaseProperty::OnChildChanged();
		}

	private:
		HPS::CircleKit & kit;
		float _radius;
	};

	class CircleKitNormalProperty : public BaseProperty
	{
	public:
		CircleKitNormalProperty(
			HPS::CircleKit & kit)
			: BaseProperty(_T("Normal"))
			, kit(kit)
		{
			this->kit.ShowNormal(_normal);
			AddSubItem(new VectorProperty(_T("Normal"), _normal));
		}

		void OnChildChanged() override
		{
			kit.SetNormal(_normal);
			BaseProperty::OnChildChanged();
		}

	private:
		HPS::CircleKit & kit;
		HPS::Vector _normal;
	};

	class CircleKitProperty : public RootProperty
	{
	public:
		CircleKitProperty(
			CMFCPropertyGridCtrl & ctrl,
			HPS::CircleKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.Show(kit);
			ctrl.AddProperty(new CircleKitCenterProperty(kit));
			ctrl.AddProperty(new CircleKitRadiusProperty(kit));
			ctrl.AddProperty(new CircleKitNormalProperty(kit));
			ctrl.AddProperty(new CircleKitPriorityProperty(kit));
			ctrl.AddProperty(new CircleKitUserDataProperty(kit));
		}

		void Apply() override
		{
			key.Consume(kit);
		}

	private:
		HPS::CircleKey key;
		HPS::CircleKit kit;
	};

	class CircularArcKitStartProperty : public BaseProperty
	{
	public:
		CircularArcKitStartProperty(
			HPS::CircularArcKit & kit)
			: BaseProperty(_T("Start"))
			, kit(kit)
		{
			this->kit.ShowStart(_start);
			AddSubItem(new PointProperty(_T("Start"), _start));
		}

		void OnChildChanged() override
		{
			kit.SetStart(_start);
			BaseProperty::OnChildChanged();
		}

	private:
		HPS::CircularArcKit & kit;
		HPS::Point _start;
	};

	class CircularArcKitMiddleProperty : public BaseProperty
	{
	public:
		CircularArcKitMiddleProperty(
			HPS::CircularArcKit & kit)
			: BaseProperty(_T("Middle"))
			, kit(kit)
		{
			this->kit.ShowMiddle(_middle);
			AddSubItem(new PointProperty(_T("Middle"), _middle));
		}

		void OnChildChanged() override
		{
			kit.SetMiddle(_middle);
			BaseProperty::OnChildChanged();
		}

	private:
		HPS::CircularArcKit & kit;
		HPS::Point _middle;
	};

	class CircularArcKitEndProperty : public BaseProperty
	{
	public:
		CircularArcKitEndProperty(
			HPS::CircularArcKit & kit)
			: BaseProperty(_T("End"))
			, kit(kit)
		{
			this->kit.ShowEnd(_end);
			AddSubItem(new PointProperty(_T("End"), _end));
		}

		void OnChildChanged() override
		{
			kit.SetEnd(_end);
			BaseProperty::OnChildChanged();
		}

	private:
		HPS::CircularArcKit & kit;
		HPS::Point _end;
	};

	class CircularArcKitProperty : public RootProperty
	{
	public:
		CircularArcKitProperty(
			CMFCPropertyGridCtrl & ctrl,
			HPS::CircularArcKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.Show(kit);
			ctrl.AddProperty(new CircularArcKitStartProperty(kit));
			ctrl.AddProperty(new CircularArcKitMiddleProperty(kit));
			ctrl.AddProperty(new CircularArcKitEndProperty(kit));
			ctrl.AddProperty(new CircularArcKitPriorityProperty(kit));
			ctrl.AddProperty(new CircularArcKitUserDataProperty(kit));
		}

		void Apply() override
		{
			key.Consume(kit);
		}

	private:
		HPS::CircularArcKey key;
		HPS::CircularArcKit kit;
	};

	class CircularWedgeKitStartProperty : public BaseProperty
	{
	public:
		CircularWedgeKitStartProperty(
			HPS::CircularWedgeKit & kit)
			: BaseProperty(_T("Start"))
			, kit(kit)
		{
			this->kit.ShowStart(_start);
			AddSubItem(new PointProperty(_T("Start"), _start));
		}

		void OnChildChanged() override
		{
			kit.SetStart(_start);
			BaseProperty::OnChildChanged();
		}

	private:
		HPS::CircularWedgeKit & kit;
		HPS::Point _start;
	};

	class CircularWedgeKitMiddleProperty : public BaseProperty
	{
	public:
		CircularWedgeKitMiddleProperty(
			HPS::CircularWedgeKit & kit)
			: BaseProperty(_T("Middle"))
			, kit(kit)
		{
			this->kit.ShowMiddle(_middle);
			AddSubItem(new PointProperty(_T("Middle"), _middle));
		}

		void OnChildChanged() override
		{
			kit.SetMiddle(_middle);
			BaseProperty::OnChildChanged();
		}

	private:
		HPS::CircularWedgeKit & kit;
		HPS::Point _middle;
	};

	class CircularWedgeKitEndProperty : public BaseProperty
	{
	public:
		CircularWedgeKitEndProperty(
			HPS::CircularWedgeKit & kit)
			: BaseProperty(_T("End"))
			, kit(kit)
		{
			this->kit.ShowEnd(_end);
			AddSubItem(new PointProperty(_T("End"), _end));
		}

		void OnChildChanged() override
		{
			kit.SetEnd(_end);
			BaseProperty::OnChildChanged();
		}

	private:
		HPS::CircularWedgeKit & kit;
		HPS::Point _end;
	};

	class CircularWedgeKitProperty : public RootProperty
	{
	public:
		CircularWedgeKitProperty(
			CMFCPropertyGridCtrl & ctrl,
			HPS::CircularWedgeKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.Show(kit);
			ctrl.AddProperty(new CircularWedgeKitStartProperty(kit));
			ctrl.AddProperty(new CircularWedgeKitMiddleProperty(kit));
			ctrl.AddProperty(new CircularWedgeKitEndProperty(kit));
			ctrl.AddProperty(new CircularWedgeKitPriorityProperty(kit));
			ctrl.AddProperty(new CircularWedgeKitUserDataProperty(kit));
		}

		void Apply() override
		{
			key.Consume(kit);
		}

	private:
		HPS::CircularWedgeKey key;
		HPS::CircularWedgeKit kit;
	};

	class InfiniteLineKitFirstProperty : public BaseProperty
	{
	public:
		InfiniteLineKitFirstProperty(
			HPS::InfiniteLineKit & kit)
			: BaseProperty(_T("First"))
			, kit(kit)
		{
			this->kit.ShowFirst(_first);
			AddSubItem(new PointProperty(_T("First"), _first));
		}

		void OnChildChanged() override
		{
			kit.SetFirst(_first);
			BaseProperty::OnChildChanged();
		}

	private:
		HPS::InfiniteLineKit & kit;
		HPS::Point _first;
	};

	class InfiniteLineKitSecondProperty : public BaseProperty
	{
	public:
		InfiniteLineKitSecondProperty(
			HPS::InfiniteLineKit & kit)
			: BaseProperty(_T("Second"))
			, kit(kit)
		{
			this->kit.ShowSecond(_second);
			AddSubItem(new PointProperty(_T("Second"), _second));
		}

		void OnChildChanged() override
		{
			kit.SetSecond(_second);
			BaseProperty::OnChildChanged();
		}

	private:
		HPS::InfiniteLineKit & kit;
		HPS::Point _second;
	};

	class InfiniteLineKitTypeProperty : public BaseProperty
	{
	public:
		InfiniteLineKitTypeProperty(
			HPS::InfiniteLineKit & kit)
			: BaseProperty(_T("Type"))
			, kit(kit)
		{
			this->kit.ShowType(_type);
			AddSubItem(new InfiniteLineTypeProperty(_type));
		}

		void OnChildChanged() override
		{
			kit.SetType(_type);
			BaseProperty::OnChildChanged();
		}

	private:
		HPS::InfiniteLineKit & kit;
		HPS::InfiniteLine::Type _type;
	};

	class InfiniteLineKitProperty : public RootProperty
	{
	public:
		InfiniteLineKitProperty(
			CMFCPropertyGridCtrl & ctrl,
			HPS::InfiniteLineKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.Show(kit);
			ctrl.AddProperty(new InfiniteLineKitFirstProperty(kit));
			ctrl.AddProperty(new InfiniteLineKitSecondProperty(kit));
			ctrl.AddProperty(new InfiniteLineKitTypeProperty(kit));
			ctrl.AddProperty(new InfiniteLineKitPriorityProperty(kit));
			ctrl.AddProperty(new InfiniteLineKitUserDataProperty(kit));
		}

		void Apply() override
		{
			key.Consume(kit);
		}

	private:
		HPS::InfiniteLineKey key;
		HPS::InfiniteLineKit kit;
	};

	class SpotlightKitPositionProperty : public BaseProperty
	{
	public:
		SpotlightKitPositionProperty(
			HPS::SpotlightKit & kit)
			: BaseProperty(_T("Position"))
			, kit(kit)
		{
			this->kit.ShowPosition(_position);
			AddSubItem(new PointProperty(_T("Position"), _position));
		}

		void OnChildChanged() override
		{
			kit.SetPosition(_position);
			BaseProperty::OnChildChanged();
		}

	private:
		HPS::SpotlightKit & kit;
		HPS::Point _position;
	};

	class SpotlightKitTargetProperty : public BaseProperty
	{
	public:
		SpotlightKitTargetProperty(
			HPS::SpotlightKit & kit)
			: BaseProperty(_T("Target"))
			, kit(kit)
		{
			this->kit.ShowTarget(_target);
			AddSubItem(new PointProperty(_T("Target"), _target));
		}

		void OnChildChanged() override
		{
			kit.SetTarget(_target);
			BaseProperty::OnChildChanged();
		}

	private:
		HPS::SpotlightKit & kit;
		HPS::Point _target;
	};

	class SpotlightKitOuterConeProperty : public BaseProperty
	{
	public:
		SpotlightKitOuterConeProperty(
			HPS::SpotlightKit & kit)
			: BaseProperty(_T("OuterCone"))
			, kit(kit)
		{
			this->kit.ShowOuterCone(_size, _units);
			AddSubItem(new FloatProperty(_T("Size"), _size));
			AddSubItem(new SpotlightOuterConeUnitsProperty(_units));
		}

		void OnChildChanged() override
		{
			kit.SetOuterCone(_size, _units);
			BaseProperty::OnChildChanged();
		}

	private:
		HPS::SpotlightKit & kit;
		float _size;
		HPS::Spotlight::OuterConeUnits _units;
	};

	class SpotlightKitInnerConeProperty : public BaseProperty
	{
	public:
		SpotlightKitInnerConeProperty(
			HPS::SpotlightKit & kit)
			: BaseProperty(_T("InnerCone"))
			, kit(kit)
		{
			this->kit.ShowInnerCone(_size, _units);
			AddSubItem(new FloatProperty(_T("Size"), _size));
			AddSubItem(new SpotlightInnerConeUnitsProperty(_units));
		}

		void OnChildChanged() override
		{
			kit.SetInnerCone(_size, _units);
			BaseProperty::OnChildChanged();
		}

	private:
		HPS::SpotlightKit & kit;
		float _size;
		HPS::Spotlight::InnerConeUnits _units;
	};

	class SpotlightKitConcentrationProperty : public BaseProperty
	{
	public:
		SpotlightKitConcentrationProperty(
			HPS::SpotlightKit & kit)
			: BaseProperty(_T("Concentration"))
			, kit(kit)
		{
			this->kit.ShowConcentration(_concentration);
			AddSubItem(new FloatProperty(_T("Concentration"), _concentration));
		}

		void OnChildChanged() override
		{
			kit.SetConcentration(_concentration);
			BaseProperty::OnChildChanged();
		}

	private:
		HPS::SpotlightKit & kit;
		float _concentration;
	};

	class SpotlightKitCameraRelativeProperty : public BaseProperty
	{
	public:
		SpotlightKitCameraRelativeProperty(
			HPS::SpotlightKit & kit)
			: BaseProperty(_T("CameraRelative"))
			, kit(kit)
		{
			this->kit.ShowCameraRelative(_state);
			AddSubItem(new BoolProperty(_T("State"), _state));
		}

		void OnChildChanged() override
		{
			kit.SetCameraRelative(_state);
			BaseProperty::OnChildChanged();
		}

	private:
		HPS::SpotlightKit & kit;
		bool _state;
	};

	class SpotlightKitProperty : public RootProperty
	{
	public:
		SpotlightKitProperty(
			CMFCPropertyGridCtrl & ctrl,
			HPS::SpotlightKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.Show(kit);
			ctrl.AddProperty(new SpotlightKitPositionProperty(kit));
			ctrl.AddProperty(new SpotlightKitTargetProperty(kit));
			ctrl.AddProperty(new SpotlightKitColorProperty(kit));
			ctrl.AddProperty(new SpotlightKitOuterConeProperty(kit));
			ctrl.AddProperty(new SpotlightKitInnerConeProperty(kit));
			ctrl.AddProperty(new SpotlightKitConcentrationProperty(kit));
			ctrl.AddProperty(new SpotlightKitCameraRelativeProperty(kit));
			ctrl.AddProperty(new SpotlightKitPriorityProperty(kit));
			ctrl.AddProperty(new SpotlightKitUserDataProperty(kit));
		}

		void Apply() override
		{
			key.Consume(kit);
		}

	private:
		HPS::SpotlightKey key;
		HPS::SpotlightKit kit;
	};

	class EllipseKitCenterProperty : public BaseProperty
	{
	public:
		EllipseKitCenterProperty(
			HPS::EllipseKit & kit)
			: BaseProperty(_T("Center"))
			, kit(kit)
		{
			this->kit.ShowCenter(_center);
			AddSubItem(new PointProperty(_T("Center"), _center));
		}

		void OnChildChanged() override
		{
			kit.SetCenter(_center);
			BaseProperty::OnChildChanged();
		}

	private:
		HPS::EllipseKit & kit;
		HPS::Point _center;
	};

	class EllipseKitMajorProperty : public BaseProperty
	{
	public:
		EllipseKitMajorProperty(
			HPS::EllipseKit & kit)
			: BaseProperty(_T("Major"))
			, kit(kit)
		{
			this->kit.ShowMajor(_major);
			AddSubItem(new PointProperty(_T("Major"), _major));
		}

		void OnChildChanged() override
		{
			kit.SetMajor(_major);
			BaseProperty::OnChildChanged();
		}

	private:
		HPS::EllipseKit & kit;
		HPS::Point _major;
	};

	class EllipseKitMinorProperty : public BaseProperty
	{
	public:
		EllipseKitMinorProperty(
			HPS::EllipseKit & kit)
			: BaseProperty(_T("Minor"))
			, kit(kit)
		{
			this->kit.ShowMinor(_minor);
			AddSubItem(new PointProperty(_T("Minor"), _minor));
		}

		void OnChildChanged() override
		{
			kit.SetMinor(_minor);
			BaseProperty::OnChildChanged();
		}

	private:
		HPS::EllipseKit & kit;
		HPS::Point _minor;
	};

	class EllipseKitProperty : public RootProperty
	{
	public:
		EllipseKitProperty(
			CMFCPropertyGridCtrl & ctrl,
			HPS::EllipseKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.Show(kit);
			ctrl.AddProperty(new EllipseKitCenterProperty(kit));
			ctrl.AddProperty(new EllipseKitMajorProperty(kit));
			ctrl.AddProperty(new EllipseKitMinorProperty(kit));
			ctrl.AddProperty(new EllipseKitPriorityProperty(kit));
			ctrl.AddProperty(new EllipseKitUserDataProperty(kit));
		}

		void Apply() override
		{
			key.Consume(kit);
		}

	private:
		HPS::EllipseKey key;
		HPS::EllipseKit kit;
	};

	class EllipticalArcKitCenterProperty : public BaseProperty
	{
	public:
		EllipticalArcKitCenterProperty(
			HPS::EllipticalArcKit & kit)
			: BaseProperty(_T("Center"))
			, kit(kit)
		{
			this->kit.ShowCenter(_center);
			AddSubItem(new PointProperty(_T("Center"), _center));
		}

		void OnChildChanged() override
		{
			kit.SetCenter(_center);
			BaseProperty::OnChildChanged();
		}

	private:
		HPS::EllipticalArcKit & kit;
		HPS::Point _center;
	};

	class EllipticalArcKitMajorProperty : public BaseProperty
	{
	public:
		EllipticalArcKitMajorProperty(
			HPS::EllipticalArcKit & kit)
			: BaseProperty(_T("Major"))
			, kit(kit)
		{
			this->kit.ShowMajor(_major);
			AddSubItem(new PointProperty(_T("Major"), _major));
		}

		void OnChildChanged() override
		{
			kit.SetMajor(_major);
			BaseProperty::OnChildChanged();
		}

	private:
		HPS::EllipticalArcKit & kit;
		HPS::Point _major;
	};

	class EllipticalArcKitMinorProperty : public BaseProperty
	{
	public:
		EllipticalArcKitMinorProperty(
			HPS::EllipticalArcKit & kit)
			: BaseProperty(_T("Minor"))
			, kit(kit)
		{
			this->kit.ShowMinor(_minor);
			AddSubItem(new PointProperty(_T("Minor"), _minor));
		}

		void OnChildChanged() override
		{
			kit.SetMinor(_minor);
			BaseProperty::OnChildChanged();
		}

	private:
		HPS::EllipticalArcKit & kit;
		HPS::Point _minor;
	};

	class EllipticalArcKitStartProperty : public BaseProperty
	{
	public:
		EllipticalArcKitStartProperty(
			HPS::EllipticalArcKit & kit)
			: BaseProperty(_T("Start"))
			, kit(kit)
		{
			this->kit.ShowStart(_start);
			AddSubItem(new FloatProperty(_T("Start"), _start));
		}

		void OnChildChanged() override
		{
			kit.SetStart(_start);
			BaseProperty::OnChildChanged();
		}

	private:
		HPS::EllipticalArcKit & kit;
		float _start;
	};

	class EllipticalArcKitEndProperty : public BaseProperty
	{
	public:
		EllipticalArcKitEndProperty(
			HPS::EllipticalArcKit & kit)
			: BaseProperty(_T("End"))
			, kit(kit)
		{
			this->kit.ShowEnd(_end);
			AddSubItem(new FloatProperty(_T("End"), _end));
		}

		void OnChildChanged() override
		{
			kit.SetEnd(_end);
			BaseProperty::OnChildChanged();
		}

	private:
		HPS::EllipticalArcKit & kit;
		float _end;
	};

	class EllipticalArcKitProperty : public RootProperty
	{
	public:
		EllipticalArcKitProperty(
			CMFCPropertyGridCtrl & ctrl,
			HPS::EllipticalArcKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.Show(kit);
			ctrl.AddProperty(new EllipticalArcKitCenterProperty(kit));
			ctrl.AddProperty(new EllipticalArcKitMajorProperty(kit));
			ctrl.AddProperty(new EllipticalArcKitMinorProperty(kit));
			ctrl.AddProperty(new EllipticalArcKitStartProperty(kit));
			ctrl.AddProperty(new EllipticalArcKitEndProperty(kit));
			ctrl.AddProperty(new EllipticalArcKitPriorityProperty(kit));
			ctrl.AddProperty(new EllipticalArcKitUserDataProperty(kit));
		}

		void Apply() override
		{
			key.Consume(kit);
		}

	private:
		HPS::EllipticalArcKey key;
		HPS::EllipticalArcKit kit;
	};

	class NURBSSurfaceAttributeKitBudgetProperty : public SettableProperty
	{
	public:
		NURBSSurfaceAttributeKitBudgetProperty(
			HPS::NURBSSurfaceAttributeKit & kit)
			: SettableProperty(_T("Budget"))
			, kit(kit)
		{
			size_t _budget_st;
			bool isSet = this->kit.ShowBudget(_budget_st);
			_budget = static_cast<unsigned int>(_budget_st);
			if (!isSet)
			{
				_budget = 0;
			}
			AddSubItem(new UnsignedIntProperty(_T("Budget"), _budget));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetBudget(_budget);
		}

		void Unset() override
		{
			kit.UnsetBudget();
		}

	private:
		HPS::NURBSSurfaceAttributeKit & kit;
		unsigned int _budget;
	};

	class NURBSSurfaceAttributeKitMaximumDeviationProperty : public SettableProperty
	{
	public:
		NURBSSurfaceAttributeKitMaximumDeviationProperty(
			HPS::NURBSSurfaceAttributeKit & kit)
			: SettableProperty(_T("MaximumDeviation"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowMaximumDeviation(_deviation);
			if (!isSet)
			{
				_deviation = 0.0f;
			}
			AddSubItem(new FloatProperty(_T("Deviation"), _deviation));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetMaximumDeviation(_deviation);
		}

		void Unset() override
		{
			kit.UnsetMaximumDeviation();
		}

	private:
		HPS::NURBSSurfaceAttributeKit & kit;
		float _deviation;
	};

	class NURBSSurfaceAttributeKitMaximumAngleProperty : public SettableProperty
	{
	public:
		NURBSSurfaceAttributeKitMaximumAngleProperty(
			HPS::NURBSSurfaceAttributeKit & kit)
			: SettableProperty(_T("MaximumAngle"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowMaximumAngle(_degrees);
			if (!isSet)
			{
				_degrees = 0.0f;
			}
			AddSubItem(new FloatProperty(_T("Degrees"), _degrees));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetMaximumAngle(_degrees);
		}

		void Unset() override
		{
			kit.UnsetMaximumAngle();
		}

	private:
		HPS::NURBSSurfaceAttributeKit & kit;
		float _degrees;
	};

	class NURBSSurfaceAttributeKitMaximumWidthProperty : public SettableProperty
	{
	public:
		NURBSSurfaceAttributeKitMaximumWidthProperty(
			HPS::NURBSSurfaceAttributeKit & kit)
			: SettableProperty(_T("MaximumWidth"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowMaximumWidth(_width);
			if (!isSet)
			{
				_width = 0.0f;
			}
			AddSubItem(new FloatProperty(_T("Width"), _width));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetMaximumWidth(_width);
		}

		void Unset() override
		{
			kit.UnsetMaximumWidth();
		}

	private:
		HPS::NURBSSurfaceAttributeKit & kit;
		float _width;
	};

	class NURBSSurfaceAttributeKitTrimBudgetProperty : public SettableProperty
	{
	public:
		NURBSSurfaceAttributeKitTrimBudgetProperty(
			HPS::NURBSSurfaceAttributeKit & kit)
			: SettableProperty(_T("TrimBudget"))
			, kit(kit)
		{
			size_t _budget_st;
			bool isSet = this->kit.ShowTrimBudget(_budget_st);
			_budget = static_cast<unsigned int>(_budget_st);
			if (!isSet)
			{
				_budget = 0;
			}
			AddSubItem(new UnsignedIntProperty(_T("Budget"), _budget));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetTrimBudget(_budget);
		}

		void Unset() override
		{
			kit.UnsetTrimBudget();
		}

	private:
		HPS::NURBSSurfaceAttributeKit & kit;
		unsigned int _budget;
	};

	class NURBSSurfaceAttributeKitMaximumTrimDeviationProperty : public SettableProperty
	{
	public:
		NURBSSurfaceAttributeKitMaximumTrimDeviationProperty(
			HPS::NURBSSurfaceAttributeKit & kit)
			: SettableProperty(_T("MaximumTrimDeviation"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowMaximumTrimDeviation(_deviation);
			if (!isSet)
			{
				_deviation = 0.0f;
			}
			AddSubItem(new FloatProperty(_T("Deviation"), _deviation));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetMaximumTrimDeviation(_deviation);
		}

		void Unset() override
		{
			kit.UnsetMaximumTrimDeviation();
		}

	private:
		HPS::NURBSSurfaceAttributeKit & kit;
		float _deviation;
	};

	class NURBSSurfaceAttributeKitProperty : public RootProperty
	{
	public:
		NURBSSurfaceAttributeKitProperty(
			CMFCPropertyGridCtrl & ctrl,
			HPS::SegmentKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.ShowNURBSSurfaceAttribute(kit);
			ctrl.AddProperty(new NURBSSurfaceAttributeKitBudgetProperty(kit));
			ctrl.AddProperty(new NURBSSurfaceAttributeKitMaximumDeviationProperty(kit));
			ctrl.AddProperty(new NURBSSurfaceAttributeKitMaximumAngleProperty(kit));
			ctrl.AddProperty(new NURBSSurfaceAttributeKitMaximumWidthProperty(kit));
			ctrl.AddProperty(new NURBSSurfaceAttributeKitTrimBudgetProperty(kit));
			ctrl.AddProperty(new NURBSSurfaceAttributeKitMaximumTrimDeviationProperty(kit));
		}

		void Apply() override
		{
			key.UnsetNURBSSurfaceAttribute();
			key.SetNURBSSurfaceAttribute(kit);
		}

	private:
		HPS::SegmentKey key;
		HPS::NURBSSurfaceAttributeKit kit;
	};

	class PerformanceKitDisplayListsProperty : public SettableProperty
	{
	public:
		PerformanceKitDisplayListsProperty(
			HPS::PerformanceKit & kit)
			: SettableProperty(_T("DisplayLists"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowDisplayLists(_display_list);
			if (!isSet)
			{
				_display_list = HPS::Performance::DisplayLists::Segment;
			}
			AddSubItem(new PerformanceDisplayListsProperty(_display_list));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetDisplayLists(_display_list);
		}

		void Unset() override
		{
			kit.UnsetDisplayLists();
		}

	private:
		HPS::PerformanceKit & kit;
		HPS::Performance::DisplayLists _display_list;
	};

	class PerformanceKitStaticModelProperty : public SettableProperty
	{
	public:
		PerformanceKitStaticModelProperty(
			HPS::PerformanceKit & kit)
			: SettableProperty(_T("StaticModel"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowStaticModel(_model_type);
			if (!isSet)
			{
				_model_type = HPS::Performance::StaticModel::Attribute;
			}
			AddSubItem(new PerformanceStaticModelProperty(_model_type));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetStaticModel(_model_type);
		}

		void Unset() override
		{
			kit.UnsetStaticModel();
		}

	private:
		HPS::PerformanceKit & kit;
		HPS::Performance::StaticModel _model_type;
	};

	class PerformanceKitStaticConditionsProperty : public SettableProperty
	{
	public:
		PerformanceKitStaticConditionsProperty(
			HPS::PerformanceKit & kit)
			: SettableProperty(_T("StaticConditions"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowStaticConditions(_conditions);
			if (!isSet)
			{
				_conditions = HPS::Performance::StaticConditions::Independent;
			}
			AddSubItem(new PerformanceStaticConditionsProperty(_conditions));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetStaticConditions(_conditions);
		}

		void Unset() override
		{
			kit.UnsetStaticConditions();
		}

	private:
		HPS::PerformanceKit & kit;
		HPS::Performance::StaticConditions _conditions;
	};

	class PerformanceKitProperty : public RootProperty
	{
	public:
		PerformanceKitProperty(
			CMFCPropertyGridCtrl & ctrl,
			HPS::SegmentKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.ShowPerformance(kit);
			ctrl.AddProperty(new PerformanceKitDisplayListsProperty(kit));
			ctrl.AddProperty(new PerformanceKitStaticModelProperty(kit));
			ctrl.AddProperty(new PerformanceKitStaticConditionsProperty(kit));
		}

		void Apply() override
		{
			key.UnsetPerformance();
			key.SetPerformance(kit);
		}

	private:
		HPS::SegmentKey key;
		HPS::PerformanceKit kit;
	};

	class HiddenLineAttributeKitAlgorithmProperty : public SettableProperty
	{
	public:
		HiddenLineAttributeKitAlgorithmProperty(
			HPS::HiddenLineAttributeKit & kit)
			: SettableProperty(_T("Algorithm"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowAlgorithm(_algorithm);
			if (!isSet)
			{
				_algorithm = HPS::HiddenLine::Algorithm::ZBuffer;
			}
			AddSubItem(new HiddenLineAlgorithmProperty(_algorithm));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetAlgorithm(_algorithm);
		}

		void Unset() override
		{
			kit.UnsetAlgorithm();
		}

	private:
		HPS::HiddenLineAttributeKit & kit;
		HPS::HiddenLine::Algorithm _algorithm;
	};

	class HiddenLineAttributeKitColorProperty : public SettableProperty
	{
	public:
		HiddenLineAttributeKitColorProperty(
			HPS::HiddenLineAttributeKit & kit)
			: SettableProperty(_T("Color"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowColor(_color);
			if (!isSet)
			{
				_color = HPS::RGBAColor::Black();
			}
			AddSubItem(new RGBAColorProperty(_T("Color"), _color));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetColor(_color);
		}

		void Unset() override
		{
			kit.UnsetColor();
		}

	private:
		HPS::HiddenLineAttributeKit & kit;
		HPS::RGBAColor _color;
	};

	class HiddenLineAttributeKitDimFactorProperty : public SettableProperty
	{
	public:
		HiddenLineAttributeKitDimFactorProperty(
			HPS::HiddenLineAttributeKit & kit)
			: SettableProperty(_T("DimFactor"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowDimFactor(_zero_to_one);
			if (!isSet)
			{
				_zero_to_one = 0.0f;
			}
			AddSubItem(new FloatProperty(_T("Zero To One"), _zero_to_one));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetDimFactor(_zero_to_one);
		}

		void Unset() override
		{
			kit.UnsetDimFactor();
		}

	private:
		HPS::HiddenLineAttributeKit & kit;
		float _zero_to_one;
	};

	class HiddenLineAttributeKitFaceDisplacementProperty : public SettableProperty
	{
	public:
		HiddenLineAttributeKitFaceDisplacementProperty(
			HPS::HiddenLineAttributeKit & kit)
			: SettableProperty(_T("FaceDisplacement"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowFaceDisplacement(_buckets);
			if (!isSet)
			{
				_buckets = 0.0f;
			}
			AddSubItem(new FloatProperty(_T("Buckets"), _buckets));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetFaceDisplacement(_buckets);
		}

		void Unset() override
		{
			kit.UnsetFaceDisplacement();
		}

	private:
		HPS::HiddenLineAttributeKit & kit;
		float _buckets;
	};

	class HiddenLineAttributeKitLinePatternProperty : public SettableProperty
	{
	public:
		HiddenLineAttributeKitLinePatternProperty(
			HPS::HiddenLineAttributeKit & kit)
			: SettableProperty(_T("LinePattern"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowLinePattern(_pattern);
			if (!isSet)
			{
				_pattern = "pattern";
			}
			AddSubItem(new UTF8Property(_T("Pattern"), _pattern));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetLinePattern(_pattern);
		}

		void Unset() override
		{
			kit.UnsetLinePattern();
		}

	private:
		HPS::HiddenLineAttributeKit & kit;
		HPS::UTF8 _pattern;
	};

	class HiddenLineAttributeKitLineSortingProperty : public SettableProperty
	{
	public:
		HiddenLineAttributeKitLineSortingProperty(
			HPS::HiddenLineAttributeKit & kit)
			: SettableProperty(_T("LineSorting"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowLineSorting(_state, _threshold, _units);
			if (!isSet)
			{
				_state = true;
				_threshold = 0.0f;
				_units = HPS::Line::SizeUnits::ScaleFactor;
			}
			AddSubItem(new BoolProperty(_T("State"), _state));
			AddSubItem(new FloatProperty(_T("Threshold"), _threshold));
			AddSubItem(new LineSizeUnitsProperty(_units));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetLineSorting(_state, _threshold, _units);
		}

		void Unset() override
		{
			kit.UnsetLineSorting();
		}

	private:
		HPS::HiddenLineAttributeKit & kit;
		bool _state;
		float _threshold;
		HPS::Line::SizeUnits _units;
	};

	class HiddenLineAttributeKitRenderFacesProperty : public SettableProperty
	{
	public:
		HiddenLineAttributeKitRenderFacesProperty(
			HPS::HiddenLineAttributeKit & kit)
			: SettableProperty(_T("RenderFaces"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowRenderFaces(_state);
			if (!isSet)
			{
				_state = true;
			}
			AddSubItem(new BoolProperty(_T("State"), _state));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetRenderFaces(_state);
		}

		void Unset() override
		{
			kit.UnsetRenderFaces();
		}

	private:
		HPS::HiddenLineAttributeKit & kit;
		bool _state;
	};

	class HiddenLineAttributeKitRenderTextProperty : public SettableProperty
	{
	public:
		HiddenLineAttributeKitRenderTextProperty(
			HPS::HiddenLineAttributeKit & kit)
			: SettableProperty(_T("RenderText"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowRenderText(_state);
			if (!isSet)
			{
				_state = true;
			}
			AddSubItem(new BoolProperty(_T("State"), _state));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetRenderText(_state);
		}

		void Unset() override
		{
			kit.UnsetRenderText();
		}

	private:
		HPS::HiddenLineAttributeKit & kit;
		bool _state;
	};

	class HiddenLineAttributeKitSilhouetteCleanupProperty : public SettableProperty
	{
	public:
		HiddenLineAttributeKitSilhouetteCleanupProperty(
			HPS::HiddenLineAttributeKit & kit)
			: SettableProperty(_T("SilhouetteCleanup"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowSilhouetteCleanup(_state);
			if (!isSet)
			{
				_state = true;
			}
			AddSubItem(new BoolProperty(_T("State"), _state));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetSilhouetteCleanup(_state);
		}

		void Unset() override
		{
			kit.UnsetSilhouetteCleanup();
		}

	private:
		HPS::HiddenLineAttributeKit & kit;
		bool _state;
	};

	class HiddenLineAttributeKitTransparencyCutoffProperty : public SettableProperty
	{
	public:
		HiddenLineAttributeKitTransparencyCutoffProperty(
			HPS::HiddenLineAttributeKit & kit)
			: SettableProperty(_T("TransparencyCutoff"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowTransparencyCutoff(_zero_to_one);
			if (!isSet)
			{
				_zero_to_one = 0.0f;
			}
			AddSubItem(new FloatProperty(_T("Zero To One"), _zero_to_one));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetTransparencyCutoff(_zero_to_one);
		}

		void Unset() override
		{
			kit.UnsetTransparencyCutoff();
		}

	private:
		HPS::HiddenLineAttributeKit & kit;
		float _zero_to_one;
	};

	class HiddenLineAttributeKitVisibilityProperty : public SettableProperty
	{
	public:
		HiddenLineAttributeKitVisibilityProperty(
			HPS::HiddenLineAttributeKit & kit)
			: SettableProperty(_T("Visibility"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowVisibility(_state);
			if (!isSet)
			{
				_state = true;
			}
			AddSubItem(new BoolProperty(_T("State"), _state));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetVisibility(_state);
		}

		void Unset() override
		{
			kit.UnsetVisibility();
		}

	private:
		HPS::HiddenLineAttributeKit & kit;
		bool _state;
	};

	class HiddenLineAttributeKitWeightProperty : public SettableProperty
	{
	public:
		HiddenLineAttributeKitWeightProperty(
			HPS::HiddenLineAttributeKit & kit)
			: SettableProperty(_T("Weight"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowWeight(_weight, _units);
			if (!isSet)
			{
				_weight = 0.0f;
				_units = HPS::Line::SizeUnits::ScaleFactor;
			}
			AddSubItem(new FloatProperty(_T("Weight"), _weight));
			AddSubItem(new LineSizeUnitsProperty(_units));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetWeight(_weight, _units);
		}

		void Unset() override
		{
			kit.UnsetWeight();
		}

	private:
		HPS::HiddenLineAttributeKit & kit;
		float _weight;
		HPS::Line::SizeUnits _units;
	};

	class HiddenLineAttributeKitProperty : public RootProperty
	{
	public:
		HiddenLineAttributeKitProperty(
			CMFCPropertyGridCtrl & ctrl,
			HPS::SegmentKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.ShowHiddenLineAttribute(kit);
			ctrl.AddProperty(new HiddenLineAttributeKitAlgorithmProperty(kit));
			ctrl.AddProperty(new HiddenLineAttributeKitColorProperty(kit));
			ctrl.AddProperty(new HiddenLineAttributeKitDimFactorProperty(kit));
			ctrl.AddProperty(new HiddenLineAttributeKitFaceDisplacementProperty(kit));
			ctrl.AddProperty(new HiddenLineAttributeKitLinePatternProperty(kit));
			ctrl.AddProperty(new HiddenLineAttributeKitLineSortingProperty(kit));
			ctrl.AddProperty(new HiddenLineAttributeKitRenderFacesProperty(kit));
			ctrl.AddProperty(new HiddenLineAttributeKitRenderTextProperty(kit));
			ctrl.AddProperty(new HiddenLineAttributeKitSilhouetteCleanupProperty(kit));
			ctrl.AddProperty(new HiddenLineAttributeKitTransparencyCutoffProperty(kit));
			ctrl.AddProperty(new HiddenLineAttributeKitVisibilityProperty(kit));
			ctrl.AddProperty(new HiddenLineAttributeKitWeightProperty(kit));
		}

		void Apply() override
		{
			key.UnsetHiddenLineAttribute();
			key.SetHiddenLineAttribute(kit);
		}

	private:
		HPS::SegmentKey key;
		HPS::HiddenLineAttributeKit kit;
	};

	class DrawingAttributeKitPolygonHandednessProperty : public SettableProperty
	{
	public:
		DrawingAttributeKitPolygonHandednessProperty(
			HPS::DrawingAttributeKit & kit)
			: SettableProperty(_T("PolygonHandedness"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowPolygonHandedness(_handedness);
			if (!isSet)
			{
				_handedness = HPS::Drawing::Handedness::Right;
			}
			AddSubItem(new DrawingHandednessProperty(_handedness));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetPolygonHandedness(_handedness);
		}

		void Unset() override
		{
			kit.UnsetPolygonHandedness();
		}

	private:
		HPS::DrawingAttributeKit & kit;
		HPS::Drawing::Handedness _handedness;
	};

	class DrawingAttributeKitWorldHandednessProperty : public SettableProperty
	{
	public:
		DrawingAttributeKitWorldHandednessProperty(
			HPS::DrawingAttributeKit & kit)
			: SettableProperty(_T("WorldHandedness"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowWorldHandedness(_handedness);
			if (!isSet)
			{
				_handedness = HPS::Drawing::Handedness::Right;
			}
			AddSubItem(new DrawingHandednessProperty(_handedness));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetWorldHandedness(_handedness);
		}

		void Unset() override
		{
			kit.UnsetWorldHandedness();
		}

	private:
		HPS::DrawingAttributeKit & kit;
		HPS::Drawing::Handedness _handedness;
	};

	class DrawingAttributeKitDepthRangeProperty : public SettableProperty
	{
	public:
		DrawingAttributeKitDepthRangeProperty(
			HPS::DrawingAttributeKit & kit)
			: SettableProperty(_T("DepthRange"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowDepthRange(_near, _far);
			if (!isSet)
			{
				_near = 0.0f;
				_far = 0.0f;
			}
			AddSubItem(new FloatProperty(_T("Near"), _near));
			AddSubItem(new FloatProperty(_T("Far"), _far));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetDepthRange(_near, _far);
		}

		void Unset() override
		{
			kit.UnsetDepthRange();
		}

	private:
		HPS::DrawingAttributeKit & kit;
		float _near;
		float _far;
	};

	class DrawingAttributeKitFaceDisplacementProperty : public SettableProperty
	{
	public:
		DrawingAttributeKitFaceDisplacementProperty(
			HPS::DrawingAttributeKit & kit)
			: SettableProperty(_T("FaceDisplacement"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowFaceDisplacement(_state, _buckets);
			if (!isSet)
			{
				_state = true;
				_buckets = 0;
			}
			auto boolProperty = new ConditionalBoolProperty(_T("State"), _state);
			AddSubItem(boolProperty);
			AddSubItem(new IntProperty(_T("Buckets"), _buckets));
			boolProperty->EnableValidProperties();
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetFaceDisplacement(_state, _buckets);
		}

		void Unset() override
		{
			kit.UnsetFaceDisplacement();
		}

	private:
		HPS::DrawingAttributeKit & kit;
		bool _state;
		int _buckets;
	};

	class DrawingAttributeKitGeneralDisplacementProperty : public SettableProperty
	{
	public:
		DrawingAttributeKitGeneralDisplacementProperty(
			HPS::DrawingAttributeKit & kit)
			: SettableProperty(_T("GeneralDisplacement"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowGeneralDisplacement(_state, _buckets);
			if (!isSet)
			{
				_state = true;
				_buckets = 0;
			}
			auto boolProperty = new ConditionalBoolProperty(_T("State"), _state);
			AddSubItem(boolProperty);
			AddSubItem(new IntProperty(_T("Buckets"), _buckets));
			boolProperty->EnableValidProperties();
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetGeneralDisplacement(_state, _buckets);
		}

		void Unset() override
		{
			kit.UnsetGeneralDisplacement();
		}

	private:
		HPS::DrawingAttributeKit & kit;
		bool _state;
		int _buckets;
	};

	class DrawingAttributeKitVertexDisplacementProperty : public SettableProperty
	{
	public:
		DrawingAttributeKitVertexDisplacementProperty(
			HPS::DrawingAttributeKit & kit)
			: SettableProperty(_T("VertexDisplacement"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowVertexDisplacement(_state, _buckets);
			if (!isSet)
			{
				_state = true;
				_buckets = 0;
			}
			auto boolProperty = new ConditionalBoolProperty(_T("State"), _state);
			AddSubItem(boolProperty);
			AddSubItem(new IntProperty(_T("Buckets"), _buckets));
			boolProperty->EnableValidProperties();
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetVertexDisplacement(_state, _buckets);
		}

		void Unset() override
		{
			kit.UnsetVertexDisplacement();
		}

	private:
		HPS::DrawingAttributeKit & kit;
		bool _state;
		int _buckets;
	};

	class DrawingAttributeKitVertexDecimationProperty : public SettableProperty
	{
	public:
		DrawingAttributeKitVertexDecimationProperty(
			HPS::DrawingAttributeKit & kit)
			: SettableProperty(_T("VertexDecimation"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowVertexDecimation(_zero_to_one);
			if (!isSet)
			{
				_zero_to_one = 0.0f;
			}
			AddSubItem(new FloatProperty(_T("Zero To One"), _zero_to_one));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetVertexDecimation(_zero_to_one);
		}

		void Unset() override
		{
			kit.UnsetVertexDecimation();
		}

	private:
		HPS::DrawingAttributeKit & kit;
		float _zero_to_one;
	};

	class DrawingAttributeKitVertexRandomizationProperty : public SettableProperty
	{
	public:
		DrawingAttributeKitVertexRandomizationProperty(
			HPS::DrawingAttributeKit & kit)
			: SettableProperty(_T("VertexRandomization"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowVertexRandomization(_state);
			if (!isSet)
			{
				_state = true;
			}
			AddSubItem(new BoolProperty(_T("State"), _state));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetVertexRandomization(_state);
		}

		void Unset() override
		{
			kit.UnsetVertexRandomization();
		}

	private:
		HPS::DrawingAttributeKit & kit;
		bool _state;
	};

	class DrawingAttributeKitOverlayProperty : public SettableProperty
	{
	public:
		DrawingAttributeKitOverlayProperty(
			HPS::DrawingAttributeKit & kit)
			: SettableProperty(_T("Overlay"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowOverlay(_overlay);
			if (!isSet)
			{
				_overlay = HPS::Drawing::Overlay::Default;
			}
			AddSubItem(new DrawingOverlayProperty(_overlay));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetOverlay(_overlay);
		}

		void Unset() override
		{
			kit.UnsetOverlay();
		}

	private:
		HPS::DrawingAttributeKit & kit;
		HPS::Drawing::Overlay _overlay;
	};

	class DrawingAttributeKitDeferralProperty : public SettableProperty
	{
	public:
		DrawingAttributeKitDeferralProperty(
			HPS::DrawingAttributeKit & kit)
			: SettableProperty(_T("Deferral"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowDeferral(_defer_batch);
			if (!isSet)
			{
				_defer_batch = 0;
			}
			AddSubItem(new IntProperty(_T("Defer Batch"), _defer_batch));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetDeferral(_defer_batch);
		}

		void Unset() override
		{
			kit.UnsetDeferral();
		}

	private:
		HPS::DrawingAttributeKit & kit;
		int _defer_batch;
	};

	class DrawingAttributeKitProperty : public RootProperty
	{
	public:
		DrawingAttributeKitProperty(
			CMFCPropertyGridCtrl & ctrl,
			HPS::SegmentKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.ShowDrawingAttribute(kit);
			ctrl.AddProperty(new DrawingAttributeKitPolygonHandednessProperty(kit));
			ctrl.AddProperty(new DrawingAttributeKitWorldHandednessProperty(kit));
			ctrl.AddProperty(new DrawingAttributeKitDepthRangeProperty(kit));
			ctrl.AddProperty(new DrawingAttributeKitFaceDisplacementProperty(kit));
			ctrl.AddProperty(new DrawingAttributeKitGeneralDisplacementProperty(kit));
			ctrl.AddProperty(new DrawingAttributeKitVertexDisplacementProperty(kit));
			ctrl.AddProperty(new DrawingAttributeKitVertexDecimationProperty(kit));
			ctrl.AddProperty(new DrawingAttributeKitVertexRandomizationProperty(kit));
			ctrl.AddProperty(new DrawingAttributeKitOverrideInternalColorProperty(kit));
			ctrl.AddProperty(new DrawingAttributeKitOverlayProperty(kit));
			ctrl.AddProperty(new DrawingAttributeKitDeferralProperty(kit));
			ctrl.AddProperty(new DrawingAttributeKitClipRegionProperty(kit));
		}

		void Apply() override
		{
			key.UnsetDrawingAttribute();
			key.SetDrawingAttribute(kit);
		}

	private:
		HPS::SegmentKey key;
		HPS::DrawingAttributeKit kit;
	};

	class SelectabilityKitWindowsProperty : public SettableProperty
	{
	public:
		SelectabilityKitWindowsProperty(
			HPS::SelectabilityKit & kit)
			: SettableProperty(_T("Windows"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowWindows(_val);
			if (!isSet)
			{
				_val = HPS::Selectability::Value::On;
			}
			AddSubItem(new SelectabilityValueProperty(_val));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetWindows(_val);
		}

		void Unset() override
		{
			kit.UnsetWindows();
		}

	private:
		HPS::SelectabilityKit & kit;
		HPS::Selectability::Value _val;
	};

	class SelectabilityKitEdgesProperty : public SettableProperty
	{
	public:
		SelectabilityKitEdgesProperty(
			HPS::SelectabilityKit & kit)
			: SettableProperty(_T("Edges"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowEdges(_val);
			if (!isSet)
			{
				_val = HPS::Selectability::Value::On;
			}
			AddSubItem(new SelectabilityValueProperty(_val));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetEdges(_val);
		}

		void Unset() override
		{
			kit.UnsetEdges();
		}

	private:
		HPS::SelectabilityKit & kit;
		HPS::Selectability::Value _val;
	};

	class SelectabilityKitFacesProperty : public SettableProperty
	{
	public:
		SelectabilityKitFacesProperty(
			HPS::SelectabilityKit & kit)
			: SettableProperty(_T("Faces"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowFaces(_val);
			if (!isSet)
			{
				_val = HPS::Selectability::Value::On;
			}
			AddSubItem(new SelectabilityValueProperty(_val));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetFaces(_val);
		}

		void Unset() override
		{
			kit.UnsetFaces();
		}

	private:
		HPS::SelectabilityKit & kit;
		HPS::Selectability::Value _val;
	};

	class SelectabilityKitLightsProperty : public SettableProperty
	{
	public:
		SelectabilityKitLightsProperty(
			HPS::SelectabilityKit & kit)
			: SettableProperty(_T("Lights"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowLights(_val);
			if (!isSet)
			{
				_val = HPS::Selectability::Value::On;
			}
			AddSubItem(new SelectabilityValueProperty(_val));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetLights(_val);
		}

		void Unset() override
		{
			kit.UnsetLights();
		}

	private:
		HPS::SelectabilityKit & kit;
		HPS::Selectability::Value _val;
	};

	class SelectabilityKitLinesProperty : public SettableProperty
	{
	public:
		SelectabilityKitLinesProperty(
			HPS::SelectabilityKit & kit)
			: SettableProperty(_T("Lines"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowLines(_val);
			if (!isSet)
			{
				_val = HPS::Selectability::Value::On;
			}
			AddSubItem(new SelectabilityValueProperty(_val));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetLines(_val);
		}

		void Unset() override
		{
			kit.UnsetLines();
		}

	private:
		HPS::SelectabilityKit & kit;
		HPS::Selectability::Value _val;
	};

	class SelectabilityKitMarkersProperty : public SettableProperty
	{
	public:
		SelectabilityKitMarkersProperty(
			HPS::SelectabilityKit & kit)
			: SettableProperty(_T("Markers"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowMarkers(_val);
			if (!isSet)
			{
				_val = HPS::Selectability::Value::On;
			}
			AddSubItem(new SelectabilityValueProperty(_val));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetMarkers(_val);
		}

		void Unset() override
		{
			kit.UnsetMarkers();
		}

	private:
		HPS::SelectabilityKit & kit;
		HPS::Selectability::Value _val;
	};

	class SelectabilityKitVerticesProperty : public SettableProperty
	{
	public:
		SelectabilityKitVerticesProperty(
			HPS::SelectabilityKit & kit)
			: SettableProperty(_T("Vertices"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowVertices(_val);
			if (!isSet)
			{
				_val = HPS::Selectability::Value::On;
			}
			AddSubItem(new SelectabilityValueProperty(_val));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetVertices(_val);
		}

		void Unset() override
		{
			kit.UnsetVertices();
		}

	private:
		HPS::SelectabilityKit & kit;
		HPS::Selectability::Value _val;
	};

	class SelectabilityKitTextProperty : public SettableProperty
	{
	public:
		SelectabilityKitTextProperty(
			HPS::SelectabilityKit & kit)
			: SettableProperty(_T("Text"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowText(_val);
			if (!isSet)
			{
				_val = HPS::Selectability::Value::On;
			}
			AddSubItem(new SelectabilityValueProperty(_val));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetText(_val);
		}

		void Unset() override
		{
			kit.UnsetText();
		}

	private:
		HPS::SelectabilityKit & kit;
		HPS::Selectability::Value _val;
	};

	class SelectabilityKitProperty : public RootProperty
	{
	public:
		SelectabilityKitProperty(
			CMFCPropertyGridCtrl & ctrl,
			HPS::SegmentKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.ShowSelectability(kit);
			ctrl.AddProperty(new SelectabilityKitWindowsProperty(kit));
			ctrl.AddProperty(new SelectabilityKitEdgesProperty(kit));
			ctrl.AddProperty(new SelectabilityKitFacesProperty(kit));
			ctrl.AddProperty(new SelectabilityKitLightsProperty(kit));
			ctrl.AddProperty(new SelectabilityKitLinesProperty(kit));
			ctrl.AddProperty(new SelectabilityKitMarkersProperty(kit));
			ctrl.AddProperty(new SelectabilityKitVerticesProperty(kit));
			ctrl.AddProperty(new SelectabilityKitTextProperty(kit));
		}

		void Apply() override
		{
			key.UnsetSelectability();
			key.SetSelectability(kit);
		}

	private:
		HPS::SegmentKey key;
		HPS::SelectabilityKit kit;
	};

	class MarkerAttributeKitSymbolProperty : public SettableProperty
	{
	public:
		MarkerAttributeKitSymbolProperty(
			HPS::MarkerAttributeKit & kit)
			: SettableProperty(_T("Symbol"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowSymbol(_glyph_name);
			if (!isSet)
			{
				_glyph_name = "glyph_name";
			}
			AddSubItem(new UTF8Property(_T("Glyph Name"), _glyph_name));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetSymbol(_glyph_name);
		}

		void Unset() override
		{
			kit.UnsetSymbol();
		}

	private:
		HPS::MarkerAttributeKit & kit;
		HPS::UTF8 _glyph_name;
	};

	class MarkerAttributeKitSizeProperty : public SettableProperty
	{
	public:
		MarkerAttributeKitSizeProperty(
			HPS::MarkerAttributeKit & kit)
			: SettableProperty(_T("Size"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowSize(_size, _units);
			if (!isSet)
			{
				_size = 0.0f;
				_units = HPS::Marker::SizeUnits::ScaleFactor;
			}
			AddSubItem(new FloatProperty(_T("Size"), _size));
			AddSubItem(new MarkerSizeUnitsProperty(_units));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetSize(_size, _units);
		}

		void Unset() override
		{
			kit.UnsetSize();
		}

	private:
		HPS::MarkerAttributeKit & kit;
		float _size;
		HPS::Marker::SizeUnits _units;
	};

	class MarkerAttributeKitDrawingPreferenceProperty : public SettableProperty
	{
	public:
		MarkerAttributeKitDrawingPreferenceProperty(
			HPS::MarkerAttributeKit & kit)
			: SettableProperty(_T("DrawingPreference"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowDrawingPreference(_preference);
			if (!isSet)
			{
				_preference = HPS::Marker::DrawingPreference::Fastest;
			}
			AddSubItem(new MarkerDrawingPreferenceProperty(_preference));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetDrawingPreference(_preference);
		}

		void Unset() override
		{
			kit.UnsetDrawingPreference();
		}

	private:
		HPS::MarkerAttributeKit & kit;
		HPS::Marker::DrawingPreference _preference;
	};

	class MarkerAttributeKitGlyphRotationProperty : public SettableProperty
	{
	public:
		MarkerAttributeKitGlyphRotationProperty(
			HPS::MarkerAttributeKit & kit)
			: SettableProperty(_T("GlyphRotation"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowGlyphRotation(_rotation);
			if (!isSet)
			{
				_rotation = 0.0f;
			}
			AddSubItem(new FloatProperty(_T("Rotation"), _rotation));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetGlyphRotation(_rotation);
		}

		void Unset() override
		{
			kit.UnsetGlyphRotation();
		}

	private:
		HPS::MarkerAttributeKit & kit;
		float _rotation;
	};

	class MarkerAttributeKitProperty : public RootProperty
	{
	public:
		MarkerAttributeKitProperty(
			CMFCPropertyGridCtrl & ctrl,
			HPS::SegmentKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.ShowMarkerAttribute(kit);
			ctrl.AddProperty(new MarkerAttributeKitSymbolProperty(kit));
			ctrl.AddProperty(new MarkerAttributeKitSizeProperty(kit));
			ctrl.AddProperty(new MarkerAttributeKitDrawingPreferenceProperty(kit));
			ctrl.AddProperty(new MarkerAttributeKitGlyphRotationProperty(kit));
		}

		void Apply() override
		{
			key.UnsetMarkerAttribute();
			key.SetMarkerAttribute(kit);
		}

	private:
		HPS::SegmentKey key;
		HPS::MarkerAttributeKit kit;
	};

	class LightingAttributeKitInterpolationAlgorithmProperty : public SettableProperty
	{
	public:
		LightingAttributeKitInterpolationAlgorithmProperty(
			HPS::LightingAttributeKit & kit)
			: SettableProperty(_T("InterpolationAlgorithm"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowInterpolationAlgorithm(_interpolation);
			if (!isSet)
			{
				_interpolation = HPS::Lighting::InterpolationAlgorithm::Phong;
			}
			AddSubItem(new LightingInterpolationAlgorithmProperty(_interpolation));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetInterpolationAlgorithm(_interpolation);
		}

		void Unset() override
		{
			kit.UnsetInterpolationAlgorithm();
		}

	private:
		HPS::LightingAttributeKit & kit;
		HPS::Lighting::InterpolationAlgorithm _interpolation;
	};

	class LightingAttributeKitProperty : public RootProperty
	{
	public:
		LightingAttributeKitProperty(
			CMFCPropertyGridCtrl & ctrl,
			HPS::SegmentKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.ShowLightingAttribute(kit);
			ctrl.AddProperty(new LightingAttributeKitInterpolationAlgorithmProperty(kit));
		}

		void Apply() override
		{
			key.UnsetLightingAttribute();
			key.SetLightingAttribute(kit);
		}

	private:
		HPS::SegmentKey key;
		HPS::LightingAttributeKit kit;
	};

	class VisualEffectsKitPostProcessEffectsEnabledProperty : public SettableProperty
	{
	public:
		VisualEffectsKitPostProcessEffectsEnabledProperty(
			HPS::VisualEffectsKit & kit)
			: SettableProperty(_T("PostProcessEffectsEnabled"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowPostProcessEffectsEnabled(_state);
			if (!isSet)
			{
				_state = true;
			}
			AddSubItem(new BoolProperty(_T("State"), _state));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetPostProcessEffectsEnabled(_state);
		}

		void Unset() override
		{
			kit.UnsetPostProcessEffectsEnabled();
		}

	private:
		HPS::VisualEffectsKit & kit;
		bool _state;
	};

	class VisualEffectsKitAmbientOcclusionEnabledProperty : public SettableProperty
	{
	public:
		VisualEffectsKitAmbientOcclusionEnabledProperty(
			HPS::VisualEffectsKit & kit)
			: SettableProperty(_T("AmbientOcclusionEnabled"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowAmbientOcclusionEnabled(_state);
			if (!isSet)
			{
				_state = true;
			}
			AddSubItem(new BoolProperty(_T("State"), _state));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetAmbientOcclusionEnabled(_state);
		}

		void Unset() override
		{
			kit.UnsetAmbientOcclusionEnabled();
		}

	private:
		HPS::VisualEffectsKit & kit;
		bool _state;
	};

	class VisualEffectsKitSilhouetteEdgesEnabledProperty : public SettableProperty
	{
	public:
		VisualEffectsKitSilhouetteEdgesEnabledProperty(
			HPS::VisualEffectsKit & kit)
			: SettableProperty(_T("SilhouetteEdgesEnabled"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowSilhouetteEdgesEnabled(_state);
			if (!isSet)
			{
				_state = true;
			}
			AddSubItem(new BoolProperty(_T("State"), _state));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetSilhouetteEdgesEnabled(_state);
		}

		void Unset() override
		{
			kit.UnsetSilhouetteEdgesEnabled();
		}

	private:
		HPS::VisualEffectsKit & kit;
		bool _state;
	};

	class VisualEffectsKitDepthOfFieldEnabledProperty : public SettableProperty
	{
	public:
		VisualEffectsKitDepthOfFieldEnabledProperty(
			HPS::VisualEffectsKit & kit)
			: SettableProperty(_T("DepthOfFieldEnabled"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowDepthOfFieldEnabled(_state);
			if (!isSet)
			{
				_state = true;
			}
			AddSubItem(new BoolProperty(_T("State"), _state));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetDepthOfFieldEnabled(_state);
		}

		void Unset() override
		{
			kit.UnsetDepthOfFieldEnabled();
		}

	private:
		HPS::VisualEffectsKit & kit;
		bool _state;
	};

	class VisualEffectsKitBloomEnabledProperty : public SettableProperty
	{
	public:
		VisualEffectsKitBloomEnabledProperty(
			HPS::VisualEffectsKit & kit)
			: SettableProperty(_T("BloomEnabled"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowBloomEnabled(_state);
			if (!isSet)
			{
				_state = true;
			}
			AddSubItem(new BoolProperty(_T("State"), _state));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetBloomEnabled(_state);
		}

		void Unset() override
		{
			kit.UnsetBloomEnabled();
		}

	private:
		HPS::VisualEffectsKit & kit;
		bool _state;
	};

	class VisualEffectsKitEyeDomeLightingEnabledProperty : public SettableProperty
	{
	public:
		VisualEffectsKitEyeDomeLightingEnabledProperty(
			HPS::VisualEffectsKit & kit)
			: SettableProperty(_T("EyeDomeLightingEnabled"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowEyeDomeLightingEnabled(_state);
			if (!isSet)
			{
				_state = true;
			}
			AddSubItem(new BoolProperty(_T("State"), _state));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetEyeDomeLightingEnabled(_state);
		}

		void Unset() override
		{
			kit.UnsetEyeDomeLightingEnabled();
		}

	private:
		HPS::VisualEffectsKit & kit;
		bool _state;
	};

	class VisualEffectsKitAntiAliasingProperty : public SettableProperty
	{
	public:
		VisualEffectsKitAntiAliasingProperty(
			HPS::VisualEffectsKit & kit)
			: SettableProperty(_T("AntiAliasing"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowAntiAliasing(_state);
			if (!isSet)
			{
				_state = true;
			}
			AddSubItem(new BoolProperty(_T("State"), _state));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetAntiAliasing(_state);
		}

		void Unset() override
		{
			kit.UnsetAntiAliasing();
		}

	private:
		HPS::VisualEffectsKit & kit;
		bool _state;
	};

	class VisualEffectsKitTextAntiAliasingProperty : public SettableProperty
	{
	public:
		VisualEffectsKitTextAntiAliasingProperty(
			HPS::VisualEffectsKit & kit)
			: SettableProperty(_T("TextAntiAliasing"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowTextAntiAliasing(_state);
			if (!isSet)
			{
				_state = true;
			}
			AddSubItem(new BoolProperty(_T("State"), _state));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetTextAntiAliasing(_state);
		}

		void Unset() override
		{
			kit.UnsetTextAntiAliasing();
		}

	private:
		HPS::VisualEffectsKit & kit;
		bool _state;
	};

	class VisualEffectsKitShadowMapsProperty : public SettableProperty
	{
	public:
		VisualEffectsKitShadowMapsProperty(
			HPS::VisualEffectsKit & kit)
			: SettableProperty(_T("ShadowMaps"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowShadowMaps(_state, _samples, _resolution, _view_dependent, _jitter);
			if (!isSet)
			{
				_state = true;
				_samples = 0;
				_resolution = 0;
				_view_dependent = true;
				_jitter = true;
			}
			auto boolProperty = new ConditionalBoolProperty(_T("State"), _state);
			AddSubItem(boolProperty);
			AddSubItem(new UnsignedIntProperty(_T("Samples"), _samples));
			AddSubItem(new UnsignedIntProperty(_T("Resolution"), _resolution));
			AddSubItem(new BoolProperty(_T("View Dependent"), _view_dependent));
			AddSubItem(new BoolProperty(_T("Jitter"), _jitter));
			boolProperty->EnableValidProperties();
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetShadowMaps(_state, _samples, _resolution, _view_dependent, _jitter);
		}

		void Unset() override
		{
			kit.UnsetShadowMaps();
		}

	private:
		HPS::VisualEffectsKit & kit;
		bool _state;
		unsigned int _samples;
		unsigned int _resolution;
		bool _view_dependent;
		bool _jitter;
	};

	class VisualEffectsKitSimpleShadowProperty : public SettableProperty
	{
	public:
		VisualEffectsKitSimpleShadowProperty(
			HPS::VisualEffectsKit & kit)
			: SettableProperty(_T("SimpleShadow"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowSimpleShadow(_state, _resolution, _blurring, _ignore_transparency);
			if (!isSet)
			{
				_state = true;
				_resolution = 0;
				_blurring = 0;
				_ignore_transparency = true;
			}
			auto boolProperty = new ConditionalBoolProperty(_T("State"), _state);
			AddSubItem(boolProperty);
			AddSubItem(new UnsignedIntProperty(_T("Resolution"), _resolution));
			AddSubItem(new UnsignedIntProperty(_T("Blurring"), _blurring));
			AddSubItem(new BoolProperty(_T("Ignore Transparency"), _ignore_transparency));
			boolProperty->EnableValidProperties();
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetSimpleShadow(_state, _resolution, _blurring, _ignore_transparency);
		}

		void Unset() override
		{
			kit.UnsetSimpleShadow();
		}

	private:
		HPS::VisualEffectsKit & kit;
		bool _state;
		unsigned int _resolution;
		unsigned int _blurring;
		bool _ignore_transparency;
	};

	class VisualEffectsKitSimpleShadowPlaneProperty : public SettableProperty
	{
	public:
		VisualEffectsKitSimpleShadowPlaneProperty(
			HPS::VisualEffectsKit & kit)
			: SettableProperty(_T("SimpleShadowPlane"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowSimpleShadowPlane(_projected_onto);
			if (!isSet)
			{
				_projected_onto = HPS::Plane(0, 0, 1, 0);
			}
			AddSubItem(new PlaneProperty(_T("Projected Onto"), _projected_onto));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetSimpleShadowPlane(_projected_onto);
		}

		void Unset() override
		{
			kit.UnsetSimpleShadowPlane();
		}

	private:
		HPS::VisualEffectsKit & kit;
		HPS::Plane _projected_onto;
	};

	class VisualEffectsKitSimpleShadowLightDirectionProperty : public SettableProperty
	{
	public:
		VisualEffectsKitSimpleShadowLightDirectionProperty(
			HPS::VisualEffectsKit & kit)
			: SettableProperty(_T("SimpleShadowLightDirection"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowSimpleShadowLightDirection(_direction);
			if (!isSet)
			{
				_direction = HPS::Vector::Unit();
			}
			AddSubItem(new VectorProperty(_T("Direction"), _direction));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetSimpleShadowLightDirection(_direction);
		}

		void Unset() override
		{
			kit.UnsetSimpleShadowLightDirection();
		}

	private:
		HPS::VisualEffectsKit & kit;
		HPS::Vector _direction;
	};

	class VisualEffectsKitSimpleShadowColorProperty : public SettableProperty
	{
	public:
		VisualEffectsKitSimpleShadowColorProperty(
			HPS::VisualEffectsKit & kit)
			: SettableProperty(_T("SimpleShadowColor"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowSimpleShadowColor(_color);
			if (!isSet)
			{
				_color = HPS::RGBAColor::Black();
			}
			AddSubItem(new RGBAColorProperty(_T("Color"), _color));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetSimpleShadowColor(_color);
		}

		void Unset() override
		{
			kit.UnsetSimpleShadowColor();
		}

	private:
		HPS::VisualEffectsKit & kit;
		HPS::RGBAColor _color;
	};

	class VisualEffectsKitSimpleReflectionProperty : public SettableProperty
	{
	public:
		VisualEffectsKitSimpleReflectionProperty(
			HPS::VisualEffectsKit & kit)
			: SettableProperty(_T("SimpleReflection"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowSimpleReflection(_state, _opacity, _blurring, _fading, _attenuation_near_distance, _attenuation_far_distance);
			if (!isSet)
			{
				_state = true;
				_opacity = 0.0f;
				_blurring = 0;
				_fading = true;
				_attenuation_near_distance = 0.0f;
				_attenuation_far_distance = 0.0f;
			}
			auto boolProperty = new ConditionalBoolProperty(_T("State"), _state);
			AddSubItem(boolProperty);
			AddSubItem(new FloatProperty(_T("Opacity"), _opacity));
			AddSubItem(new UnsignedIntProperty(_T("Blurring"), _blurring));
			AddSubItem(new BoolProperty(_T("Fading"), _fading));
			AddSubItem(new FloatProperty(_T("Attenuation Near Distance"), _attenuation_near_distance));
			AddSubItem(new FloatProperty(_T("Attenuation Far Distance"), _attenuation_far_distance));
			boolProperty->EnableValidProperties();
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetSimpleReflection(_state, _opacity, _blurring, _fading, _attenuation_near_distance, _attenuation_far_distance);
		}

		void Unset() override
		{
			kit.UnsetSimpleReflection();
		}

	private:
		HPS::VisualEffectsKit & kit;
		bool _state;
		float _opacity;
		unsigned int _blurring;
		bool _fading;
		float _attenuation_near_distance;
		float _attenuation_far_distance;
	};

	class VisualEffectsKitSimpleReflectionPlaneProperty : public SettableProperty
	{
	public:
		VisualEffectsKitSimpleReflectionPlaneProperty(
			HPS::VisualEffectsKit & kit)
			: SettableProperty(_T("SimpleReflectionPlane"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowSimpleReflectionPlane(_projected_onto);
			if (!isSet)
			{
				_projected_onto = HPS::Plane(0, 0, 1, 0);
			}
			AddSubItem(new PlaneProperty(_T("Projected Onto"), _projected_onto));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetSimpleReflectionPlane(_projected_onto);
		}

		void Unset() override
		{
			kit.UnsetSimpleReflectionPlane();
		}

	private:
		HPS::VisualEffectsKit & kit;
		HPS::Plane _projected_onto;
	};

	class VisualEffectsKitProperty : public RootProperty
	{
	public:
		VisualEffectsKitProperty(
			CMFCPropertyGridCtrl & ctrl,
			HPS::SegmentKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.ShowVisualEffects(kit);
			ctrl.AddProperty(new VisualEffectsKitPostProcessEffectsEnabledProperty(kit));
			ctrl.AddProperty(new VisualEffectsKitAmbientOcclusionEnabledProperty(kit));
			ctrl.AddProperty(new VisualEffectsKitSilhouetteEdgesEnabledProperty(kit));
			ctrl.AddProperty(new VisualEffectsKitDepthOfFieldEnabledProperty(kit));
			ctrl.AddProperty(new VisualEffectsKitBloomEnabledProperty(kit));
			ctrl.AddProperty(new VisualEffectsKitEyeDomeLightingEnabledProperty(kit));
			ctrl.AddProperty(new VisualEffectsKitEyeDomeLightingBackColorProperty(kit));
			ctrl.AddProperty(new VisualEffectsKitAntiAliasingProperty(kit));
			ctrl.AddProperty(new VisualEffectsKitTextAntiAliasingProperty(kit));
			ctrl.AddProperty(new VisualEffectsKitShadowMapsProperty(kit));
			ctrl.AddProperty(new VisualEffectsKitSimpleShadowProperty(kit));
			ctrl.AddProperty(new VisualEffectsKitSimpleShadowPlaneProperty(kit));
			ctrl.AddProperty(new VisualEffectsKitSimpleShadowLightDirectionProperty(kit));
			ctrl.AddProperty(new VisualEffectsKitSimpleShadowColorProperty(kit));
			ctrl.AddProperty(new VisualEffectsKitSimpleReflectionProperty(kit));
			ctrl.AddProperty(new VisualEffectsKitSimpleReflectionPlaneProperty(kit));
			ctrl.AddProperty(new VisualEffectsKitSimpleReflectionVisibilityProperty(kit));
		}

		void Apply() override
		{
			key.UnsetVisualEffects();
			key.SetVisualEffects(kit);
		}

	private:
		HPS::SegmentKey key;
		HPS::VisualEffectsKit kit;
	};

	class PostProcessEffectsKitAmbientOcclusionProperty : public BaseProperty
	{
	public:
		PostProcessEffectsKitAmbientOcclusionProperty(
			HPS::PostProcessEffectsKit & kit)
			: BaseProperty(_T("AmbientOcclusion"))
			, kit(kit)
		{
			this->kit.ShowAmbientOcclusion(_state, _strength, _quality, _radius, _sharpness);
			auto boolProperty = new ConditionalBoolProperty(_T("State"), _state);
			AddSubItem(boolProperty);
			AddSubItem(new FloatProperty(_T("Strength"), _strength));
			AddSubItem(new PostProcessEffectsAmbientOcclusionQualityProperty(_quality));
			AddSubItem(new FloatProperty(_T("Radius"), _radius));
			AddSubItem(new FloatProperty(_T("Sharpness"), _sharpness));
			boolProperty->EnableValidProperties();
			SmartShow();
		}

		void OnChildChanged() override
		{
			kit.SetAmbientOcclusion(_state, _strength, _quality, _radius, _sharpness);
			BaseProperty::OnChildChanged();
		}

	private:
		HPS::PostProcessEffectsKit & kit;
		bool _state;
		float _strength;
		HPS::PostProcessEffects::AmbientOcclusion::Quality _quality;
		float _radius;
		float _sharpness;
	};

	class PostProcessEffectsKitBloomProperty : public BaseProperty
	{
	public:
		PostProcessEffectsKitBloomProperty(
			HPS::PostProcessEffectsKit & kit)
			: BaseProperty(_T("Bloom"))
			, kit(kit)
		{
			this->kit.ShowBloom(_state, _strength, _blur, _shape);
			auto boolProperty = new ConditionalBoolProperty(_T("State"), _state);
			AddSubItem(boolProperty);
			AddSubItem(new FloatProperty(_T("Strength"), _strength));
			AddSubItem(new UnsignedIntProperty(_T("Blur"), _blur));
			AddSubItem(new PostProcessEffectsBloomShapeProperty(_shape));
			boolProperty->EnableValidProperties();
			SmartShow();
		}

		void OnChildChanged() override
		{
			kit.SetBloom(_state, _strength, _blur, _shape);
			BaseProperty::OnChildChanged();
		}

	private:
		HPS::PostProcessEffectsKit & kit;
		bool _state;
		float _strength;
		unsigned int _blur;
		HPS::PostProcessEffects::Bloom::Shape _shape;
	};

	class PostProcessEffectsKitDepthOfFieldProperty : public BaseProperty
	{
	public:
		PostProcessEffectsKitDepthOfFieldProperty(
			HPS::PostProcessEffectsKit & kit)
			: BaseProperty(_T("DepthOfField"))
			, kit(kit)
		{
			this->kit.ShowDepthOfField(_state, _strength, _near_distance, _far_distance);
			auto boolProperty = new ConditionalBoolProperty(_T("State"), _state);
			AddSubItem(boolProperty);
			AddSubItem(new FloatProperty(_T("Strength"), _strength));
			AddSubItem(new FloatProperty(_T("Near Distance"), _near_distance));
			AddSubItem(new FloatProperty(_T("Far Distance"), _far_distance));
			boolProperty->EnableValidProperties();
			SmartShow();
		}

		void OnChildChanged() override
		{
			kit.SetDepthOfField(_state, _strength, _near_distance, _far_distance);
			BaseProperty::OnChildChanged();
		}

	private:
		HPS::PostProcessEffectsKit & kit;
		bool _state;
		float _strength;
		float _near_distance;
		float _far_distance;
	};

	class PostProcessEffectsKitSilhouetteEdgesProperty : public BaseProperty
	{
	public:
		PostProcessEffectsKitSilhouetteEdgesProperty(
			HPS::PostProcessEffectsKit & kit)
			: BaseProperty(_T("SilhouetteEdges"))
			, kit(kit)
		{
			this->kit.ShowSilhouetteEdges(_state, _tolerance, _heavy_exterior);
			auto boolProperty = new ConditionalBoolProperty(_T("State"), _state);
			AddSubItem(boolProperty);
			AddSubItem(new FloatProperty(_T("Tolerance"), _tolerance));
			AddSubItem(new BoolProperty(_T("Heavy Exterior"), _heavy_exterior));
			boolProperty->EnableValidProperties();
			SmartShow();
		}

		void OnChildChanged() override
		{
			kit.SetSilhouetteEdges(_state, _tolerance, _heavy_exterior);
			BaseProperty::OnChildChanged();
		}

	private:
		HPS::PostProcessEffectsKit & kit;
		bool _state;
		float _tolerance;
		bool _heavy_exterior;
	};

	class PostProcessEffectsKitEyeDomeLightingProperty : public BaseProperty
	{
	public:
		PostProcessEffectsKitEyeDomeLightingProperty(
			HPS::PostProcessEffectsKit & kit)
			: BaseProperty(_T("EyeDomeLighting"))
			, kit(kit)
		{
			this->kit.ShowEyeDomeLighting(_state, _exponent, _tolerance, _strength);
			auto boolProperty = new ConditionalBoolProperty(_T("State"), _state);
			AddSubItem(boolProperty);
			AddSubItem(new FloatProperty(_T("Exponent"), _exponent));
			AddSubItem(new FloatProperty(_T("Tolerance"), _tolerance));
			AddSubItem(new FloatProperty(_T("Strength"), _strength));
			boolProperty->EnableValidProperties();
			SmartShow();
		}

		void OnChildChanged() override
		{
			kit.SetEyeDomeLighting(_state, _exponent, _tolerance, _strength);
			BaseProperty::OnChildChanged();
		}

	private:
		HPS::PostProcessEffectsKit & kit;
		bool _state;
		float _exponent;
		float _tolerance;
		float _strength;
	};

	class PostProcessEffectsKitWorldScaleProperty : public BaseProperty
	{
	public:
		PostProcessEffectsKitWorldScaleProperty(
			HPS::PostProcessEffectsKit & kit)
			: BaseProperty(_T("WorldScale"))
			, kit(kit)
		{
			this->kit.ShowWorldScale(_scale);
			AddSubItem(new FloatProperty(_T("Scale"), _scale));
		}

		void OnChildChanged() override
		{
			kit.SetWorldScale(_scale);
			BaseProperty::OnChildChanged();
		}

	private:
		HPS::PostProcessEffectsKit & kit;
		float _scale;
	};

	class PostProcessEffectsKitProperty : public RootProperty
	{
	public:
		PostProcessEffectsKitProperty(
			CMFCPropertyGridCtrl & ctrl,
			HPS::WindowKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.ShowPostProcessEffects(kit);
			ctrl.AddProperty(new PostProcessEffectsKitAmbientOcclusionProperty(kit));
			ctrl.AddProperty(new PostProcessEffectsKitBloomProperty(kit));
			ctrl.AddProperty(new PostProcessEffectsKitDepthOfFieldProperty(kit));
			ctrl.AddProperty(new PostProcessEffectsKitSilhouetteEdgesProperty(kit));
			ctrl.AddProperty(new PostProcessEffectsKitEyeDomeLightingProperty(kit));
			ctrl.AddProperty(new PostProcessEffectsKitWorldScaleProperty(kit));
		}

		void Apply() override
		{
			key.SetPostProcessEffects(kit);
		}

	private:
		HPS::WindowKey key;
		HPS::PostProcessEffectsKit kit;
	};

	class DebuggingKitResourceMonitorProperty : public BaseProperty
	{
	public:
		DebuggingKitResourceMonitorProperty(
			HPS::DebuggingKit & kit)
			: BaseProperty(_T("ResourceMonitor"))
			, kit(kit)
		{
			this->kit.ShowResourceMonitor(_display);
			AddSubItem(new BoolProperty(_T("Display"), _display));
		}

		void OnChildChanged() override
		{
			kit.SetResourceMonitor(_display);
			BaseProperty::OnChildChanged();
		}

	private:
		HPS::DebuggingKit & kit;
		bool _display;
	};

	class DebuggingKitProperty : public RootProperty
	{
	public:
		DebuggingKitProperty(
			CMFCPropertyGridCtrl & ctrl,
			HPS::WindowKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.ShowDebugging(kit);
			ctrl.AddProperty(new DebuggingKitResourceMonitorProperty(kit));
		}

		void Apply() override
		{
			key.SetDebugging(kit);
		}

	private:
		HPS::WindowKey key;
		HPS::DebuggingKit kit;
	};

	class ContourLineKitVisibilityProperty : public SettableProperty
	{
	public:
		ContourLineKitVisibilityProperty(
			HPS::ContourLineKit & kit)
			: SettableProperty(_T("Visibility"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowVisibility(_state);
			if (!isSet)
			{
				_state = true;
			}
			AddSubItem(new BoolProperty(_T("State"), _state));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetVisibility(_state);
		}

		void Unset() override
		{
			kit.UnsetVisibility();
		}

	private:
		HPS::ContourLineKit & kit;
		bool _state;
	};

	class ContourLineKitLightingProperty : public SettableProperty
	{
	public:
		ContourLineKitLightingProperty(
			HPS::ContourLineKit & kit)
			: SettableProperty(_T("Lighting"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowLighting(_state);
			if (!isSet)
			{
				_state = true;
			}
			AddSubItem(new BoolProperty(_T("State"), _state));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetLighting(_state);
		}

		void Unset() override
		{
			kit.UnsetLighting();
		}

	private:
		HPS::ContourLineKit & kit;
		bool _state;
	};

	class ContourLineKitProperty : public RootProperty
	{
	public:
		ContourLineKitProperty(
			CMFCPropertyGridCtrl & ctrl,
			HPS::SegmentKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.ShowContourLine(kit);
			ctrl.AddProperty(new ContourLineKitVisibilityProperty(kit));
			ctrl.AddProperty(new ContourLineKitPositionsProperty(kit));
			ctrl.AddProperty(new ContourLineKitColorsProperty(kit));
			ctrl.AddProperty(new ContourLineKitPatternsProperty(kit));
			ctrl.AddProperty(new ContourLineKitWeightsProperty(kit));
			ctrl.AddProperty(new ContourLineKitLightingProperty(kit));
		}

		void Apply() override
		{
			key.UnsetContourLine();
			key.SetContourLine(kit);
		}

	private:
		HPS::SegmentKey key;
		HPS::ContourLineKit kit;
	};

	class BoundingKitExclusionProperty : public SettableProperty
	{
	public:
		BoundingKitExclusionProperty(
			HPS::BoundingKit & kit)
			: SettableProperty(_T("Exclusion"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowExclusion(_exclusion);
			if (!isSet)
			{
				_exclusion = true;
			}
			AddSubItem(new BoolProperty(_T("Exclusion"), _exclusion));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetExclusion(_exclusion);
		}

		void Unset() override
		{
			kit.UnsetExclusion();
		}

	private:
		HPS::BoundingKit & kit;
		bool _exclusion;
	};

	class BoundingKitProperty : public RootProperty
	{
	public:
		BoundingKitProperty(
			CMFCPropertyGridCtrl & ctrl,
			HPS::SegmentKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.ShowBounding(kit);
			ctrl.AddProperty(new BoundingKitVolumeProperty(kit));
			ctrl.AddProperty(new BoundingKitExclusionProperty(kit));
		}

		void Apply() override
		{
			key.UnsetBounding();
			key.SetBounding(kit);
		}

	private:
		HPS::SegmentKey key;
		HPS::BoundingKit kit;
	};

	class TransformMaskKitCameraRotationProperty : public SettableProperty
	{
	public:
		TransformMaskKitCameraRotationProperty(
			HPS::TransformMaskKit & kit)
			: SettableProperty(_T("CameraRotation"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowCameraRotation(_state);
			if (!isSet)
			{
				_state = true;
			}
			AddSubItem(new BoolProperty(_T("State"), _state));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetCameraRotation(_state);
		}

		void Unset() override
		{
			kit.UnsetCameraRotation();
		}

	private:
		HPS::TransformMaskKit & kit;
		bool _state;
	};

	class TransformMaskKitCameraScaleProperty : public SettableProperty
	{
	public:
		TransformMaskKitCameraScaleProperty(
			HPS::TransformMaskKit & kit)
			: SettableProperty(_T("CameraScale"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowCameraScale(_state);
			if (!isSet)
			{
				_state = true;
			}
			AddSubItem(new BoolProperty(_T("State"), _state));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetCameraScale(_state);
		}

		void Unset() override
		{
			kit.UnsetCameraScale();
		}

	private:
		HPS::TransformMaskKit & kit;
		bool _state;
	};

	class TransformMaskKitCameraTranslationProperty : public SettableProperty
	{
	public:
		TransformMaskKitCameraTranslationProperty(
			HPS::TransformMaskKit & kit)
			: SettableProperty(_T("CameraTranslation"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowCameraTranslation(_state);
			if (!isSet)
			{
				_state = true;
			}
			AddSubItem(new BoolProperty(_T("State"), _state));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetCameraTranslation(_state);
		}

		void Unset() override
		{
			kit.UnsetCameraTranslation();
		}

	private:
		HPS::TransformMaskKit & kit;
		bool _state;
	};

	class TransformMaskKitCameraPerspectiveScaleProperty : public SettableProperty
	{
	public:
		TransformMaskKitCameraPerspectiveScaleProperty(
			HPS::TransformMaskKit & kit)
			: SettableProperty(_T("CameraPerspectiveScale"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowCameraPerspectiveScale(_state);
			if (!isSet)
			{
				_state = true;
			}
			AddSubItem(new BoolProperty(_T("State"), _state));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetCameraPerspectiveScale(_state);
		}

		void Unset() override
		{
			kit.UnsetCameraPerspectiveScale();
		}

	private:
		HPS::TransformMaskKit & kit;
		bool _state;
	};

	class TransformMaskKitCameraProjectionProperty : public SettableProperty
	{
	public:
		TransformMaskKitCameraProjectionProperty(
			HPS::TransformMaskKit & kit)
			: SettableProperty(_T("CameraProjection"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowCameraProjection(_state);
			if (!isSet)
			{
				_state = true;
			}
			AddSubItem(new BoolProperty(_T("State"), _state));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetCameraProjection(_state);
		}

		void Unset() override
		{
			kit.UnsetCameraProjection();
		}

	private:
		HPS::TransformMaskKit & kit;
		bool _state;
	};

	class TransformMaskKitCameraOffsetProperty : public SettableProperty
	{
	public:
		TransformMaskKitCameraOffsetProperty(
			HPS::TransformMaskKit & kit)
			: SettableProperty(_T("CameraOffset"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowCameraOffset(_state);
			if (!isSet)
			{
				_state = true;
			}
			AddSubItem(new BoolProperty(_T("State"), _state));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetCameraOffset(_state);
		}

		void Unset() override
		{
			kit.UnsetCameraOffset();
		}

	private:
		HPS::TransformMaskKit & kit;
		bool _state;
	};

	class TransformMaskKitCameraNearLimitProperty : public SettableProperty
	{
	public:
		TransformMaskKitCameraNearLimitProperty(
			HPS::TransformMaskKit & kit)
			: SettableProperty(_T("CameraNearLimit"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowCameraNearLimit(_state);
			if (!isSet)
			{
				_state = true;
			}
			AddSubItem(new BoolProperty(_T("State"), _state));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetCameraNearLimit(_state);
		}

		void Unset() override
		{
			kit.UnsetCameraNearLimit();
		}

	private:
		HPS::TransformMaskKit & kit;
		bool _state;
	};

	class TransformMaskKitModellingMatrixRotationProperty : public SettableProperty
	{
	public:
		TransformMaskKitModellingMatrixRotationProperty(
			HPS::TransformMaskKit & kit)
			: SettableProperty(_T("ModellingMatrixRotation"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowModellingMatrixRotation(_state);
			if (!isSet)
			{
				_state = true;
			}
			AddSubItem(new BoolProperty(_T("State"), _state));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetModellingMatrixRotation(_state);
		}

		void Unset() override
		{
			kit.UnsetModellingMatrixRotation();
		}

	private:
		HPS::TransformMaskKit & kit;
		bool _state;
	};

	class TransformMaskKitModellingMatrixScaleProperty : public SettableProperty
	{
	public:
		TransformMaskKitModellingMatrixScaleProperty(
			HPS::TransformMaskKit & kit)
			: SettableProperty(_T("ModellingMatrixScale"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowModellingMatrixScale(_state);
			if (!isSet)
			{
				_state = true;
			}
			AddSubItem(new BoolProperty(_T("State"), _state));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetModellingMatrixScale(_state);
		}

		void Unset() override
		{
			kit.UnsetModellingMatrixScale();
		}

	private:
		HPS::TransformMaskKit & kit;
		bool _state;
	};

	class TransformMaskKitModellingMatrixTranslationProperty : public SettableProperty
	{
	public:
		TransformMaskKitModellingMatrixTranslationProperty(
			HPS::TransformMaskKit & kit)
			: SettableProperty(_T("ModellingMatrixTranslation"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowModellingMatrixTranslation(_state);
			if (!isSet)
			{
				_state = true;
			}
			AddSubItem(new BoolProperty(_T("State"), _state));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetModellingMatrixTranslation(_state);
		}

		void Unset() override
		{
			kit.UnsetModellingMatrixTranslation();
		}

	private:
		HPS::TransformMaskKit & kit;
		bool _state;
	};

	class TransformMaskKitModellingMatrixOffsetProperty : public SettableProperty
	{
	public:
		TransformMaskKitModellingMatrixOffsetProperty(
			HPS::TransformMaskKit & kit)
			: SettableProperty(_T("ModellingMatrixOffset"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowModellingMatrixOffset(_state);
			if (!isSet)
			{
				_state = true;
			}
			AddSubItem(new BoolProperty(_T("State"), _state));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetModellingMatrixOffset(_state);
		}

		void Unset() override
		{
			kit.UnsetModellingMatrixOffset();
		}

	private:
		HPS::TransformMaskKit & kit;
		bool _state;
	};

	class TransformMaskKitProperty : public RootProperty
	{
	public:
		TransformMaskKitProperty(
			CMFCPropertyGridCtrl & ctrl,
			HPS::SegmentKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.ShowTransformMask(kit);
			ctrl.AddProperty(new TransformMaskKitCameraRotationProperty(kit));
			ctrl.AddProperty(new TransformMaskKitCameraScaleProperty(kit));
			ctrl.AddProperty(new TransformMaskKitCameraTranslationProperty(kit));
			ctrl.AddProperty(new TransformMaskKitCameraPerspectiveScaleProperty(kit));
			ctrl.AddProperty(new TransformMaskKitCameraProjectionProperty(kit));
			ctrl.AddProperty(new TransformMaskKitCameraOffsetProperty(kit));
			ctrl.AddProperty(new TransformMaskKitCameraNearLimitProperty(kit));
			ctrl.AddProperty(new TransformMaskKitModellingMatrixRotationProperty(kit));
			ctrl.AddProperty(new TransformMaskKitModellingMatrixScaleProperty(kit));
			ctrl.AddProperty(new TransformMaskKitModellingMatrixTranslationProperty(kit));
			ctrl.AddProperty(new TransformMaskKitModellingMatrixOffsetProperty(kit));
		}

		void Apply() override
		{
			key.UnsetTransformMask();
			key.SetTransformMask(kit);
		}

	private:
		HPS::SegmentKey key;
		HPS::TransformMaskKit kit;
	};

	class ColorInterpolationKitFaceColorProperty : public SettableProperty
	{
	public:
		ColorInterpolationKitFaceColorProperty(
			HPS::ColorInterpolationKit & kit)
			: SettableProperty(_T("FaceColor"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowFaceColor(_state);
			if (!isSet)
			{
				_state = true;
			}
			AddSubItem(new BoolProperty(_T("State"), _state));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetFaceColor(_state);
		}

		void Unset() override
		{
			kit.UnsetFaceColor();
		}

	private:
		HPS::ColorInterpolationKit & kit;
		bool _state;
	};

	class ColorInterpolationKitEdgeColorProperty : public SettableProperty
	{
	public:
		ColorInterpolationKitEdgeColorProperty(
			HPS::ColorInterpolationKit & kit)
			: SettableProperty(_T("EdgeColor"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowEdgeColor(_state);
			if (!isSet)
			{
				_state = true;
			}
			AddSubItem(new BoolProperty(_T("State"), _state));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetEdgeColor(_state);
		}

		void Unset() override
		{
			kit.UnsetEdgeColor();
		}

	private:
		HPS::ColorInterpolationKit & kit;
		bool _state;
	};

	class ColorInterpolationKitVertexColorProperty : public SettableProperty
	{
	public:
		ColorInterpolationKitVertexColorProperty(
			HPS::ColorInterpolationKit & kit)
			: SettableProperty(_T("VertexColor"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowVertexColor(_state);
			if (!isSet)
			{
				_state = true;
			}
			AddSubItem(new BoolProperty(_T("State"), _state));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetVertexColor(_state);
		}

		void Unset() override
		{
			kit.UnsetVertexColor();
		}

	private:
		HPS::ColorInterpolationKit & kit;
		bool _state;
	};

	class ColorInterpolationKitFaceIndexProperty : public SettableProperty
	{
	public:
		ColorInterpolationKitFaceIndexProperty(
			HPS::ColorInterpolationKit & kit)
			: SettableProperty(_T("FaceIndex"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowFaceIndex(_state);
			if (!isSet)
			{
				_state = true;
			}
			AddSubItem(new BoolProperty(_T("State"), _state));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetFaceIndex(_state);
		}

		void Unset() override
		{
			kit.UnsetFaceIndex();
		}

	private:
		HPS::ColorInterpolationKit & kit;
		bool _state;
	};

	class ColorInterpolationKitEdgeIndexProperty : public SettableProperty
	{
	public:
		ColorInterpolationKitEdgeIndexProperty(
			HPS::ColorInterpolationKit & kit)
			: SettableProperty(_T("EdgeIndex"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowEdgeIndex(_state);
			if (!isSet)
			{
				_state = true;
			}
			AddSubItem(new BoolProperty(_T("State"), _state));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetEdgeIndex(_state);
		}

		void Unset() override
		{
			kit.UnsetEdgeIndex();
		}

	private:
		HPS::ColorInterpolationKit & kit;
		bool _state;
	};

	class ColorInterpolationKitVertexIndexProperty : public SettableProperty
	{
	public:
		ColorInterpolationKitVertexIndexProperty(
			HPS::ColorInterpolationKit & kit)
			: SettableProperty(_T("VertexIndex"))
			, kit(kit)
		{
			bool isSet = this->kit.ShowVertexIndex(_state);
			if (!isSet)
			{
				_state = true;
			}
			AddSubItem(new BoolProperty(_T("State"), _state));
			IsSet(isSet);
		}

	protected:
		void Set() override
		{
			kit.SetVertexIndex(_state);
		}

		void Unset() override
		{
			kit.UnsetVertexIndex();
		}

	private:
		HPS::ColorInterpolationKit & kit;
		bool _state;
	};

	class ColorInterpolationKitProperty : public RootProperty
	{
	public:
		ColorInterpolationKitProperty(
			CMFCPropertyGridCtrl & ctrl,
			HPS::SegmentKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.ShowColorInterpolation(kit);
			ctrl.AddProperty(new ColorInterpolationKitFaceColorProperty(kit));
			ctrl.AddProperty(new ColorInterpolationKitEdgeColorProperty(kit));
			ctrl.AddProperty(new ColorInterpolationKitVertexColorProperty(kit));
			ctrl.AddProperty(new ColorInterpolationKitFaceIndexProperty(kit));
			ctrl.AddProperty(new ColorInterpolationKitEdgeIndexProperty(kit));
			ctrl.AddProperty(new ColorInterpolationKitVertexIndexProperty(kit));
		}

		void Apply() override
		{
			key.UnsetColorInterpolation();
			key.SetColorInterpolation(kit);
		}

	private:
		HPS::SegmentKey key;
		HPS::ColorInterpolationKit kit;
	};

	class UpdateOptionsKitUpdateTypeProperty : public BaseProperty
	{
	public:
		UpdateOptionsKitUpdateTypeProperty(
			HPS::UpdateOptionsKit & kit)
			: BaseProperty(_T("UpdateType"))
			, kit(kit)
		{
			this->kit.ShowUpdateType(_type);
			AddSubItem(new WindowUpdateTypeProperty(_type));
		}

		void OnChildChanged() override
		{
			kit.SetUpdateType(_type);
			BaseProperty::OnChildChanged();
		}

	private:
		HPS::UpdateOptionsKit & kit;
		HPS::Window::UpdateType _type;
	};

	class UpdateOptionsKitTimeLimitProperty : public BaseProperty
	{
	public:
		UpdateOptionsKitTimeLimitProperty(
			HPS::UpdateOptionsKit & kit)
			: BaseProperty(_T("TimeLimit"))
			, kit(kit)
		{
			this->kit.ShowTimeLimit(_time_limit);
			AddSubItem(new DoubleProperty(_T("Time Limit"), _time_limit));
		}

		void OnChildChanged() override
		{
			kit.SetTimeLimit(_time_limit);
			BaseProperty::OnChildChanged();
		}

	private:
		HPS::UpdateOptionsKit & kit;
		HPS::Time _time_limit;
	};

	class UpdateOptionsKitProperty : public RootProperty
	{
	public:
		UpdateOptionsKitProperty(
			CMFCPropertyGridCtrl & ctrl,
			HPS::WindowKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.ShowUpdateOptions(kit);
			ctrl.AddProperty(new UpdateOptionsKitUpdateTypeProperty(kit));
			ctrl.AddProperty(new UpdateOptionsKitTimeLimitProperty(kit));
		}

		void Apply() override
		{
			key.SetUpdateOptions(kit);
		}

	private:
		HPS::WindowKey key;
		HPS::UpdateOptionsKit kit;
	};

	class GridKitTypeProperty : public BaseProperty
	{
	public:
		GridKitTypeProperty(
			HPS::GridKit & kit)
			: BaseProperty(_T("Type"))
			, kit(kit)
		{
			this->kit.ShowType(_type);
			AddSubItem(new GridTypeProperty(_type));
		}

		void OnChildChanged() override
		{
			kit.SetType(_type);
			BaseProperty::OnChildChanged();
		}

	private:
		HPS::GridKit & kit;
		HPS::Grid::Type _type;
	};

	class GridKitOriginProperty : public BaseProperty
	{
	public:
		GridKitOriginProperty(
			HPS::GridKit & kit)
			: BaseProperty(_T("Origin"))
			, kit(kit)
		{
			this->kit.ShowOrigin(_origin);
			AddSubItem(new PointProperty(_T("Origin"), _origin));
		}

		void OnChildChanged() override
		{
			kit.SetOrigin(_origin);
			BaseProperty::OnChildChanged();
		}

	private:
		HPS::GridKit & kit;
		HPS::Point _origin;
	};

	class GridKitFirstPointProperty : public BaseProperty
	{
	public:
		GridKitFirstPointProperty(
			HPS::GridKit & kit)
			: BaseProperty(_T("FirstPoint"))
			, kit(kit)
		{
			this->kit.ShowFirstPoint(_first_point);
			AddSubItem(new PointProperty(_T("First Point"), _first_point));
		}

		void OnChildChanged() override
		{
			kit.SetFirstPoint(_first_point);
			BaseProperty::OnChildChanged();
		}

	private:
		HPS::GridKit & kit;
		HPS::Point _first_point;
	};

	class GridKitSecondPointProperty : public BaseProperty
	{
	public:
		GridKitSecondPointProperty(
			HPS::GridKit & kit)
			: BaseProperty(_T("SecondPoint"))
			, kit(kit)
		{
			this->kit.ShowSecondPoint(_second_point);
			AddSubItem(new PointProperty(_T("Second Point"), _second_point));
		}

		void OnChildChanged() override
		{
			kit.SetSecondPoint(_second_point);
			BaseProperty::OnChildChanged();
		}

	private:
		HPS::GridKit & kit;
		HPS::Point _second_point;
	};

	class GridKitFirstCountProperty : public BaseProperty
	{
	public:
		GridKitFirstCountProperty(
			HPS::GridKit & kit)
			: BaseProperty(_T("FirstCount"))
			, kit(kit)
		{
			this->kit.ShowFirstCount(_first_count);
			AddSubItem(new IntProperty(_T("First Count"), _first_count));
		}

		void OnChildChanged() override
		{
			kit.SetFirstCount(_first_count);
			BaseProperty::OnChildChanged();
		}

	private:
		HPS::GridKit & kit;
		int _first_count;
	};

	class GridKitSecondCountProperty : public BaseProperty
	{
	public:
		GridKitSecondCountProperty(
			HPS::GridKit & kit)
			: BaseProperty(_T("SecondCount"))
			, kit(kit)
		{
			this->kit.ShowSecondCount(_second_count);
			AddSubItem(new IntProperty(_T("Second Count"), _second_count));
		}

		void OnChildChanged() override
		{
			kit.SetSecondCount(_second_count);
			BaseProperty::OnChildChanged();
		}

	private:
		HPS::GridKit & kit;
		int _second_count;
	};

	class GridKitProperty : public RootProperty
	{
	public:
		GridKitProperty(
			CMFCPropertyGridCtrl & ctrl,
			HPS::GridKey const & key)
			: RootProperty(ctrl)
			, key(key)
		{
			this->key.Show(kit);
			ctrl.AddProperty(new GridKitTypeProperty(kit));
			ctrl.AddProperty(new GridKitOriginProperty(kit));
			ctrl.AddProperty(new GridKitFirstPointProperty(kit));
			ctrl.AddProperty(new GridKitSecondPointProperty(kit));
			ctrl.AddProperty(new GridKitFirstCountProperty(kit));
			ctrl.AddProperty(new GridKitSecondCountProperty(kit));
			ctrl.AddProperty(new GridKitPriorityProperty(kit));
			ctrl.AddProperty(new GridKitUserDataProperty(kit));
		}

		void Apply() override
		{
			key.Consume(kit);
		}

	private:
		HPS::GridKey key;
		HPS::GridKit kit;
	};

}
