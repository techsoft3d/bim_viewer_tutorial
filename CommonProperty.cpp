#include "stdafx.h"
#include "CommonProperty.h"

CommonProperty::Property::Property()
	: property_name("")
	, property_value(-1)
	, property_type(CommonProperty::PropertyType::Metadata)
	, add_sub_grid(false)
	, has_extra_data(false)
	, entry_type(CommonProperty::Property::EntryType::GridEntry)
	, property_subtype(CommonProperty::PropertyType::None)
{}

CommonProperty::Property::Property(HPS::UTF8 name, _variant_t value, PropertyType type, bool sub_grid, bool extra_data)
	: property_name(name)
	, property_value(value)
	, property_type(type)
	, add_sub_grid(sub_grid)
	, has_extra_data(extra_data)
	, entry_type(CommonProperty::Property::EntryType::GridEntry)
	, property_subtype(CommonProperty::PropertyType::None)
{}

CommonProperty::Property::~Property()
{
	if (has_extra_data)
		delete extra_data;
}

CommonProperty::Property * CommonProperty::Property::AddVector(HPS::Vector & vector, HPS::UTF8 const & property_name, PropertyType property_type, VectorLabel label)
{
	Property * property = new Property(property_name, 0, property_type, true);

	HPS::UTF8Array labels;
	if (label == VectorLabel::Label_XYZ)
	{
		labels.push_back("x");
		labels.push_back("y");
		labels.push_back("z");
	}
	else if (label == VectorLabel::Label_012)
	{
		labels.push_back("0");
		labels.push_back("1");
		labels.push_back("2");
	}

	property->sub_properties.push_back(new Property(labels[0], vector.x, property_type));
	property->sub_properties.push_back(new Property(labels[1], vector.y, property_type));
	property->sub_properties.push_back(new Property(labels[2], vector.z, property_type));

	return property;
}

bool CommonProperty::Property::ToBool(signed char in_bool)
{
	if (in_bool == TRUE)
		return true;
	else
		return false;
}

CommonProperty::Property::ExtraData::ExtraData(void * in_component, Action in_action, bool in_expanded)
	: component(in_component)
	, action(in_action)
	, expanded(in_expanded)
{}

_variant_t CommonProperty::GetMetadataValue(HPS::Metadata const & metadata)
{
	switch (metadata.Type())
	{
	case HPS::Type::StringMetadata:
		{
			HPS::WCharArray characters(1, L'\0');
			HPS::UTF8 value = HPS::StringMetadata(metadata).GetValue();
			if (value.IsValid())
				value.ToWStr(characters);

			return (_variant_t)characters.data();
		}	break;
	case HPS::Type::UnsignedIntegerMetadata:
		return (_variant_t)HPS::UnsignedIntegerMetadata(metadata).GetValue();
		break;
	case HPS::Type::IntegerMetadata:
		return (_variant_t)HPS::IntegerMetadata(metadata).GetValue();
		break;
	case HPS::Type::DoubleMetadata:
		return (_variant_t)HPS::DoubleMetadata(metadata).GetValue();
		break;
	case HPS::Type::TimeMetadata:
		return (_variant_t)HPS::TimeMetadata(metadata).GetValue();
		break;
	case HPS::Type::BooleanMetadata:
		return (_variant_t)HPS::BooleanMetadata(metadata).GetValue();
		break;
	default:
		{
			ASSERT(0);	//Are we missing a metadata type?	
			return (_variant_t)FALSE;
		}
	}
}

void CommonProperty::GetMaterialProperties(HWND main_frame_handle, HPS::KeyPath const & key_path)
{
	if (key_path.Empty())
		return;

	HPS::MaterialMappingKit material_mapping;
	key_path.ShowNetMaterialMapping(material_mapping);
	HPS::Material::Type material_type;
	HPS::MaterialKit material;
	float dummy;
	material_mapping.ShowFaceMaterial(material_type, material, dummy);

	if (material_type != HPS::Material::Type::None)
	{
		CommonProperty::Property * material_property = new CommonProperty::Property("Net Material", 0, CommonProperty::PropertyType::General, true);

		ReallyGetMaterialProperties(material_property, material, material_type);
		PostMessage(main_frame_handle, WM_HOOPS_DEMO_VIEWER_ADD_PROPERTY, reinterpret_cast<WPARAM>(material_property), 0);
	}

	HPS::SegmentKey segment_key(key_path.Front());
	if (segment_key.Type() != HPS::Type::SegmentKey)
	{
		if (key_path.Size() > 1)
			segment_key = (HPS::SegmentKey)key_path.At(1);
		else
			return;
	}

	HPS::MaterialKit local_material;
	if (segment_key.GetMaterialMappingControl().ShowFaceMaterial(material_type, local_material, dummy))
	{

		if (material_type != HPS::Material::Type::None &&
			local_material != material)
		{
			CommonProperty::Property * local_material_property = new CommonProperty::Property("Local Material", 0, CommonProperty::PropertyType::General, true);

			ReallyGetMaterialProperties(local_material_property, local_material, material_type);
			PostMessage(main_frame_handle, WM_HOOPS_DEMO_VIEWER_ADD_PROPERTY, reinterpret_cast<WPARAM>(local_material_property), 0);
		}
	}
}

void CommonProperty::ReallyGetMaterialProperties(CommonProperty::Property * material_property, HPS::MaterialKit const & material, HPS::Material::Type material_type)
{
	switch (material_type)
	{
	case HPS::Material::Type::FullMaterial:
		{
			CommonProperty::Property * color = new CommonProperty::Property("Material Type", "Full Material", CommonProperty::PropertyType::General);
			material_property->sub_properties.push_back(color);
		}
		break;
	case HPS::Material::Type::RGBColor:
		{
			CommonProperty::Property * color = new CommonProperty::Property("Material Type", "RGB Color", CommonProperty::PropertyType::General);
			material_property->sub_properties.push_back(color);
		}
		break;
	case HPS::Material::Type::RGBAColor:
		{
			CommonProperty::Property * color = new CommonProperty::Property("Material Type", "RGBA Color", CommonProperty::PropertyType::General);
			material_property->sub_properties.push_back(color);
		}
		break;
	case HPS::Material::Type::TextureName:
		{
			CommonProperty::Property * color = new CommonProperty::Property("Material Type", "Texture", CommonProperty::PropertyType::General);
			material_property->sub_properties.push_back(color);
		}
		break;
	case HPS::Material::Type::ModulatedTexture:
		{
			CommonProperty::Property * color = new CommonProperty::Property("Material Type", "Modulated Texture", CommonProperty::PropertyType::General);
			material_property->sub_properties.push_back(color);
		}
		break;
	}

	HPS::RGBAColor rgba_color;
	if (material.ShowDiffuseColor(rgba_color))
	{
		CommonProperty::Property * color_value = new CommonProperty::Property("Color", 0, CommonProperty::PropertyType::General);
		color_value->entry_type = CommonProperty::Property::EntryType::ColorEntry;
		color_value->color = RGB(rgba_color.red * 255, rgba_color.green * 255, rgba_color.blue * 255);
		material_property->sub_properties.push_back(color_value);

		std::stringstream transparency;
		transparency << 100 - rgba_color.alpha * 100 << "%";
		CommonProperty::Property * alpha = new CommonProperty::Property("Transparency", transparency.str().data(), CommonProperty::PropertyType::General);
		material_property->sub_properties.push_back(alpha);
	}

	HPS::UTF8 texture_name;
	if (material.ShowDiffuseTexture(0, material_type, rgba_color, texture_name))
	{
		//no data to display -- texture names cannot be obtained by exchange
		//if deemed necessary we can display the size and format of the image used for the texture
	}

	float gloss_value;
	if (material.ShowGloss(gloss_value))
	{
		CommonProperty::Property * gloss = new CommonProperty::Property("Gloss", gloss_value, CommonProperty::PropertyType::General);
		material_property->sub_properties.push_back(gloss);
	}

	if (material.ShowMirror(material_type, rgba_color, texture_name))
	{
		if (material_type == HPS::Material::Type::RGBAColor || 
			material_type == HPS::Material::Type::ModulatedTexture)
		{
			CommonProperty::Property * mirror = new CommonProperty::Property("Mirror Color", 0, CommonProperty::PropertyType::General);
			mirror->entry_type = CommonProperty::Property::EntryType::ColorEntry;
			mirror->color = RGB(rgba_color.red * 255, rgba_color.green * 255, rgba_color.blue * 255);
			material_property->sub_properties.push_back(mirror);
		}
	}

	if (material.ShowEnvironment(material_type, rgba_color, texture_name))
	{
		if (material_type == HPS::Material::Type::RGBAColor || 
			material_type == HPS::Material::Type::ModulatedTexture)
		{
			CommonProperty::Property * environment = new CommonProperty::Property("Environment Color", 0, CommonProperty::PropertyType::General);
			environment->entry_type = CommonProperty::Property::EntryType::ColorEntry;
			environment->color = RGB(rgba_color.red * 255, rgba_color.green * 255, rgba_color.blue * 255);
			material_property->sub_properties.push_back(environment);
		}
	}

	if (material.ShowSpecular(material_type, rgba_color, texture_name))
	{
		if (material_type == HPS::Material::Type::RGBAColor || 
			material_type == HPS::Material::Type::ModulatedTexture)
		{
			CommonProperty::Property * specular = new CommonProperty::Property("Specular Color", 0, CommonProperty::PropertyType::General);
			specular->entry_type = CommonProperty::Property::EntryType::ColorEntry;
			specular->color = RGB(rgba_color.red * 255, rgba_color.green * 255, rgba_color.blue * 255);
			material_property->sub_properties.push_back(specular);
		}
	}
}

void CommonProperty::GetTransformationProperties(HWND main_frame_handle, HPS::KeyPath const & in_key_path)
{
	if (in_key_path.Empty())
		return;

	HPS::Key const & front = in_key_path.Front();
	HPS::SegmentKey segment_key;
	if (front.Type() == HPS::Type::SegmentKey)
		segment_key = (HPS::SegmentKey)front;
	else
		segment_key = front.Owner();

	HPS::MatrixKit matrix;

	if (!segment_key.ShowModellingMatrix(matrix))
		matrix = matrix.GetDefault();

	CommonProperty::Property * local_transformation = new CommonProperty::Property("Local Transformation", 0, CommonProperty::PropertyType::General, true);
	if (ReallyGetTransformationProperties(local_transformation, matrix))
		PostMessage(main_frame_handle, WM_HOOPS_DEMO_VIEWER_ADD_PROPERTY, reinterpret_cast<WPARAM>(local_transformation), 0);

	in_key_path.ShowNetModellingMatrix(matrix);
	CommonProperty::Property * net_transformation = new CommonProperty::Property("Net Transformation", 0, CommonProperty::PropertyType::General, true);
	if (ReallyGetTransformationProperties(net_transformation, matrix))
		PostMessage(main_frame_handle, WM_HOOPS_DEMO_VIEWER_ADD_PROPERTY, reinterpret_cast<WPARAM>(net_transformation), 0);
}

bool CommonProperty::ReallyGetTransformationProperties(CommonProperty::Property * transformation_property, HPS::MatrixKit const & matrix)
{
	HPS::FloatArray elements;
	matrix.ShowElements(elements);

	if (elements.size() == 16)
	{
		CommonProperty::Property * matrix_property;

		if (matrix == HPS::MatrixKit::GetDefault())
			matrix_property = new CommonProperty::Property("Matrix", "Identity", CommonProperty::PropertyType::General);
		else
		{
			HPS::Vector rx_old(elements[0], elements[1], elements[2]);
			HPS::Vector ry_old(elements[4], elements[5], elements[6]);
			HPS::Vector rz_old(elements[8], elements[9], elements[10]);

			HPS::Vector rx_normalized, ry_normalized, rz_normalized;
			rx_normalized.x = rx_old.x, rx_normalized.y = rx_old.y, rx_normalized.z = rx_old.z;
			ry_normalized.x = ry_old.x, ry_normalized.y = ry_old.y, ry_normalized.z = ry_old.z;
			rz_normalized.x = rz_old.x, rz_normalized.y = rz_old.y, rz_normalized.z = rz_old.z;

			rx_normalized.Normalize();
			ry_normalized.Normalize();
			rz_normalized.Normalize();

			HPS::Vector scale(rx_old.Magnitude() / rx_normalized.Magnitude(), ry_old.Magnitude() / ry_normalized.Magnitude(), rz_old.Magnitude() / rz_normalized.Magnitude());

			std::vector<HPS::Vector> vectors;
			vectors.push_back(rx_normalized);
			vectors.push_back(ry_normalized);
			vectors.push_back(rz_normalized);
			vectors.push_back(HPS::Vector(elements[12], elements[13], elements[14]));
			vectors.push_back(scale);

			matrix_property = new CommonProperty::Property("Matrix", "Click to Explore", CommonProperty::PropertyType::General, false, true);
			matrix_property->entry_type = CommonProperty::Property::EntryType::ButtonEntry;

			CommonProperty::Property::ExtraData * extra_data = new CommonProperty::Property::ExtraData(nullptr, CommonProperty::Property::Action::PopulateMatrixTable);
			extra_data->m_matrices = vectors;
			matrix_property->extra_data = extra_data;
		}

		transformation_property->sub_properties.push_back(matrix_property);
		return true;
	}

	return false;
}