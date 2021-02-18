#include "stdafx.h"
#include "SimplePropertyHelper.h"
#include "CommonProperty.h"
#include "A3DSDKIncludes.h"

SimplePropertyHelper::SimplePropertyHelper(
	size_t number_of_selected_items,
	HPS::CADModel const & cad_model,
	HPS::Component const & selected_component,
	HPS::Component const & ifc_component)
	: m_number_of_selected_items(number_of_selected_items)
	, m_cad_model(cad_model)
	, m_selected_component(selected_component)
	, m_ifc_component(ifc_component)
{
	
}

void SimplePropertyHelper::SetComponentPath(HPS::ComponentArray const & in_path)
{
	m_key_path.Reset();

	m_component_path = in_path;
	if (!m_component_path.empty())
		m_key_path = HPS::ComponentPath(m_component_path).GetKeyPaths()[0];
}

void SimplePropertyHelper::GetProperties(bool recalculate_triangle_count)
{
	try 
	{
	
		if (recalculate_triangle_count)
		{
			HPS::SearchResults searchResults;
			m_cad_model.GetModel().GetSegmentKey().Find(HPS::Search::Type::UserData, HPS::Search::Space::SubsegmentsAndIncludes, searchResults);
			HPS::SearchResultsIterator it = searchResults.GetIterator();
			while (it.IsValid())
			{
				HPS::SegmentKey(it.GetItem()).UnsetUserData(Common::NumberOfTrianglesIndex);
				++it;
			}
		}

		if (m_number_of_selected_items > 0 && !m_selected_component.Empty())
		{
			HPS::Component primary_component = m_selected_component;

			if (primary_component.HasComponentType(HPS::Component::ComponentType::ExchangeComponentMask))
			{
				GetSelectionTypeProperties(primary_component); 
				GetMetadata(primary_component, CommonProperty::PropertyType::Metadata);

				HPS::Exchange::Component exchange_component(primary_component);

				//for IFCs we want the metadata to come from product occurrences except for
				//physical properties, which should come from a separate component,
				//which was saved into m_ifc_property_component during selection
				if (!m_ifc_component.Empty())
					GetExchangePhysicalProperties(m_ifc_component);
				else
					GetExchangePhysicalProperties(exchange_component);


		
			}
		}
	}
	catch (HPS::InvalidObjectException const &) {}
}

void SimplePropertyHelper::GetSelectionTypeProperties(HPS::Component const & component)
{
	CommonProperty::Property * property = new CommonProperty::Property();
	property->property_type = CommonProperty::PropertyType::General;
	property->property_name = "Type";

	HPS::UTF8 p_type;
	
	p_type = ToString(component.GetComponentType());
	HPS::WCharArray characters;
	p_type.ToWStr(characters);

	property->property_value = characters.data();

}

void SimplePropertyHelper::GetMetadata(HPS::Component const & component, CommonProperty::PropertyType property_type)
{
	HPS::MetadataArray metadata = component.GetAllMetadata();

	for (auto m : metadata)
	{
		std::wstring name;
		HPS::UTF8 metadata_name = m.GetName();
		if (metadata_name == "Units")
		{
			metadata_name = "Local Units";
			name = L"Local Units";
		}
		else
		{
			HPS::WCharArray characters;
			metadata_name.ToWStr(characters);
			name = characters.data();
		}

		size_t found = name.find_first_of(L"/");
		if (found != std::wstring::npos)
		{
			//we found a compounded property
			m_ifc_properties_keys.insert(name.substr(0, found));
			m_ifc_properties.insert(std::make_pair(name.substr(0, found), std::pair<std::wstring, _variant_t>(name.substr(found + 1), CommonProperty::GetMetadataValue(m))));
		}
		else
		{
			CommonProperty::Property * property = new CommonProperty::Property();
			property->property_type = property_type;
			property->property_name = metadata_name;
			property->property_value = GetMetadataValue(m);
		}
	}

	//add IFC properties
	for (auto it = m_ifc_properties_keys.begin(), e = m_ifc_properties_keys.end(); it != e; ++it)
	{
		std::set<std::wstring> property_names;
		CommonProperty::Property * p = new CommonProperty::Property(HPS::UTF8(it->data()), 0, CommonProperty::PropertyType::Metadata, true); 
		auto range = m_ifc_properties.equal_range(*it);
		for (auto range_it = range.first; range_it != range.second; ++range_it)
		{
			auto set_it = property_names.find(range_it->second.first);
			if (set_it == property_names.end())
			{
				p->sub_properties.push_back(new CommonProperty::Property(range_it->second.first.data(), range_it->second.second, CommonProperty::PropertyType::Metadata));
				property_names.insert(range_it->second.first);
			}
		}

		property_names.clear();
	}

	m_ifc_properties.clear();
	m_ifc_properties_keys.clear();
}



std::wstring SimplePropertyHelper::GetUnitsString()
{
	if (m_cad_model.GetComponentType() != HPS::Component::ComponentType::ExchangeModelFile)
		return L"";

	HPS::StringMetadata units_metadata = m_cad_model.GetMetadata("Units");
	if (units_metadata.Empty())
		return L"mm";

	HPS::UTF8 full = units_metadata.GetValue();
	if (full == "Millimeter")
		return L"mm";
	else if (full == "Centimeter")
		return L"cm";
	else if (full == "Meter")
		return L"m";
	else if (full == "Kilometer")
		return L"km";
	else if (full == "Inch")
		return L"in";
	else if (full == "Foot")
		return L"ft";
	else if (full == "Yard")
		return L"yd";
	else if (full == "Mile")
		return L"mi";
	else
	{
		// Just use the full name
		HPS::WCharArray wchars;
		full.ToWStr(wchars);
		return wchars.data();
	}
}


void SimplePropertyHelper::GetExchangeComponentProperties(HPS::Exchange::Component const & exchange_component, double & volume, double & area, A3DVector3dData & center_of_gravity)
{
	//check if the data is already available for this component. if so, we are done
	if (exchange_component.GetKeys()[0].Type() == HPS::Type::SegmentKey)
	{
		HPS::SegmentKey key(exchange_component.GetKeys()[0]);
		HPS::ByteArray byte_volume, byte_area, byte_center_of_gravity;
		if (key.ShowUserData(Common::VolumeIndex, byte_volume)
			&& key.ShowUserData(Common::AreaIndex, byte_area)
			&& key.ShowUserData(Common::CenterOfGravityIndex, byte_center_of_gravity))
		{
			volume = *reinterpret_cast<double *>(byte_volume.data());
			area = *reinterpret_cast<double *>(byte_area.data());
			center_of_gravity = *reinterpret_cast<A3DVector3dData *>(byte_center_of_gravity.data());
			return;
		}
	}

	HPS::Component::ComponentType exchangeComponentType = exchange_component.GetComponentType();
	A3DPhysicalPropertiesData data;
	A3D_INITIALIZE_DATA(A3DPhysicalPropertiesData, data);
	if (exchangeComponentType == HPS::Component::ComponentType::ExchangeRIBRepModel)
	{
		double pocc_scale;
		double ri_scale;
		GetExchangeScale(exchange_component, &pocc_scale, &ri_scale, nullptr);

		A3DVector3dData scale;
		A3D_INITIALIZE_DATA(A3DVector3dData, scale);
		scale.m_dX = scale.m_dY = scale.m_dZ = pocc_scale * ri_scale;

		A3DStatus status = A3DComputePhysicalProperties(exchange_component.GetExchangeEntity(), &scale, &data);
		if (status == A3D_SUCCESS)
		{
			area += data.m_dSurface;
			center_of_gravity = data.m_sGravityCenter;
			if (data.m_bVolumeComputed)
				volume += data.m_dVolume;
		}
	}
	else if (exchangeComponentType == HPS::Component::ComponentType::ExchangeRIPolyBRepModel)
	{
		double pocc_scale;
		double ri_scale;
		GetExchangeScale(exchange_component, &pocc_scale, &ri_scale, nullptr);

		A3DVector3dData scale;
		A3D_INITIALIZE_DATA(A3DVector3dData, scale);
		scale.m_dX = scale.m_dY = scale.m_dZ = pocc_scale * ri_scale;

		A3DStatus status = A3DComputePolyBrepPhysicalProperties(exchange_component.GetExchangeEntity(), &scale, &data);
		if (status == A3D_SUCCESS)
		{
			area += data.m_dSurface;
			center_of_gravity = data.m_sGravityCenter;
			if (data.m_bVolumeComputed)
				volume += data.m_dVolume;
		}
	}
	else if (exchange_component.HasComponentType(HPS::Component::ComponentType::ExchangeRepresentationItemMask) &&
		exchangeComponentType != HPS::Component::ComponentType::ExchangeRISet)
		return;

	//for now do not do it recursively
// 	HPS::ComponentArray subcomponents = exchange_component.GetSubcomponents();
// 	for (auto const & c : subcomponents)
// 		GetComponentProperties(c, volume, area, center_of_gravity);

	//cache the data for future use
	if (exchange_component.GetKeys()[0].Type() == HPS::Type::SegmentKey)
	{
		HPS::SegmentKey key(exchange_component.GetKeys()[0]);
		unsigned char buffer[256];

		memcpy(buffer, (unsigned char *)&volume, sizeof(double));
		key.SetUserData(Common::VolumeIndex, 256, buffer);
		memcpy(buffer, (unsigned char *)&area, sizeof(double));
		key.SetUserData(Common::AreaIndex, 256, buffer);
		memcpy(buffer, (unsigned char *)&center_of_gravity, sizeof(A3DVector3dData));
		key.SetUserData(Common::CenterOfGravityIndex, 256, buffer);
	}
}

void SimplePropertyHelper::GetComponentCount(HPS::Component const & component, int & number_of_triangles, int & number_of_faces, int & number_of_edges)
{
	//check if the data is already available for this component. if so, we are done
	if (component.GetKeys()[0].Type() == HPS::Type::SegmentKey)
	{
		HPS::SegmentKey key(component.GetKeys()[0]);
		HPS::ByteArray triangles, faces, edges;
		if (key.ShowUserData(Common::NumberOfTrianglesIndex, triangles)
			&& key.ShowUserData(Common::NumberOfFacesIndex, faces)
			&& key.ShowUserData(Common::NumberOfEdgesIndex, edges))
		{
			number_of_triangles = *reinterpret_cast<int *>(triangles.data());
			number_of_faces = *reinterpret_cast<int *>(faces.data());
			number_of_edges = *reinterpret_cast<int *>(edges.data());
			return;
		}
	}

	bool brep_available = false;
	hpsMFCView * view = ((hpsMFCApp *)AfxGetApp())->GetActiveView();
	if (view)
	{
		HPS::Component::ComponentType comp_type = component.GetComponentType();
		if (view->HasBRep() && comp_type != HPS::Component::ComponentType::ExchangeRIPolyBRepModel && comp_type != HPS::Component::ComponentType::ExchangeRIPolyWire)
		{
			HPS::ComponentArray subcomponents = component.GetSubcomponents();
			if ((comp_type == HPS::Component::ComponentType::ExchangeRIBRepModel && (subcomponents.empty() || subcomponents[0].GetSubcomponents().empty()))
				|| (comp_type == HPS::Component::ComponentType::ExchangeRICurve && subcomponents.empty()))
				brep_available = false;
			else
				brep_available = true;
		}
	}

	if (brep_available)
	{
		HPS::ComponentArray subcomponents = component.GetSubcomponents();
		for (auto const & c : subcomponents)
			RecursiveComponentCount(c, number_of_triangles, number_of_faces, number_of_edges);

		number_of_edges += (int)m_unique_edges.size();
		m_unique_edges.clear();
	}
	else
	{
		GetPolyBRepCount(component, number_of_triangles, number_of_faces, number_of_edges);
	}

	//cache the data for future use
	if (component.GetKeys()[0].Type() == HPS::Type::SegmentKey)
	{
		HPS::SegmentKey key(component.GetKeys()[0]);
		unsigned char buffer[256];

		memcpy(buffer, (unsigned char *)&number_of_triangles, sizeof(int));
		key.SetUserData(Common::NumberOfTrianglesIndex, 256, buffer);
		memcpy(buffer, (unsigned char *)&number_of_faces, sizeof(int));
		key.SetUserData(Common::NumberOfFacesIndex, 256, buffer);
		memcpy(buffer, (unsigned char *)&number_of_edges, sizeof(int));
		key.SetUserData(Common::NumberOfTrianglesIndex, 256, buffer);
	}
}

void SimplePropertyHelper::GetPolyBRepCount(HPS::Component const & component, int & out_tris, int & out_faces, int & out_edges)
{
	HPS::KeyArray component_keys = component.GetKeys();
	ASSERT(component_keys.size() > 0);
	HPS::SegmentKey search_here = HPS::SegmentKey(component_keys[0]);
	if (component_keys.size() > 1)
	{
		if (component_keys[0].Type() != HPS::Type::SegmentKey)
			search_here = HPS::SegmentKey(component_keys[1]);
	}

	HPS::SearchResults results;
	out_faces += (int)search_here.Find(HPS::Search::Type::Shell, HPS::Search::Space::SubsegmentsAndIncludes, results);

	HPS::SearchResultsIterator it = results.GetIterator();
	while (it.IsValid())
	{
		HPS::ShellKey shell_key(it.GetItem());
		HPS::IntArray face_list;
		shell_key.ShowFacelist(face_list);
		out_tris += ((int)face_list.size() / 4);
		it.Next();
	}

	out_edges += (int)search_here.Find(HPS::Search::Type::Line, HPS::Search::Space::SubsegmentsAndIncludes, results);
}




HPS::UTF8 SimplePropertyHelper::ToString(HPS::Exchange::Component::ComponentType component_type)
{
	switch (component_type)
	{
		case HPS::Component::ComponentType::ExchangeModelFile:
			return "Model File";
			break;
		case HPS::Component::ComponentType::ExchangeProductOccurrence:
			return "Product Occurrence";
			break;
		case HPS::Component::ComponentType::ExchangePartDefinition:
			return "Part Definition";
			break;
		case HPS::Component::ComponentType::ExchangeView:
			return "View";
			break;
		case HPS::Component::ComponentType::ExchangeFilter:
			return "Filter";
			break;
		case HPS::Component::ComponentType::ExchangeRIBRepModel:
			return "RI: Body";
			break;
		case HPS::Component::ComponentType::ExchangeRICurve:
			return "RI: Curve";
			break;
		case HPS::Component::ComponentType::ExchangeRIDirection:
			return "RI: Direction";
			break;
		case HPS::Component::ComponentType::ExchangeRIPlane:
			return "RI: Plane";
			break;
		case HPS::Component::ComponentType::ExchangeRIPointSet:
			return "RI: Point Set";
			break;
		case HPS::Component::ComponentType::ExchangeRIPolyBRepModel:
			return "RI: Faceted Model";
			break;
		case HPS::Component::ComponentType::ExchangeRIPolyWire:
			return "RI: Wireframe Model";
			break;
		case HPS::Component::ComponentType::ExchangeRISet:
			return "RI: Set";
			break;
		case HPS::Component::ComponentType::ExchangeRICoordinateSystem:
			return "RI: Coordinate System";
			break;
		case HPS::Component::ComponentType::ExchangeTopoBody:
			return "Body";
			break;
		case HPS::Component::ComponentType::ExchangeTopoConnex:
			return "Connex";
			break;
		case HPS::Component::ComponentType::ExchangeTopoShell:
			return "Shell";
			break;
		case HPS::Component::ComponentType::ExchangeTopoFace:
			return "Face";
			break;
		case HPS::Component::ComponentType::ExchangeTopoLoop:
			return "Loop";
			break;
		case HPS::Component::ComponentType::ExchangeTopoCoEdge:
			return "Co-Edge";
			break;
		case HPS::Component::ComponentType::ExchangeTopoEdge:
			return "Edge";
			break;
		case HPS::Component::ComponentType::ExchangeTopoVertex:
			return "Vertex";
			break;
		case HPS::Component::ComponentType::ExchangeTopoSingleWireBody:
			return "Wire Body";
			break;
		case HPS::Component::ComponentType::ExchangeTopoWireEdge:
			return "Wire Edge";
			break;
		case HPS::Component::ComponentType::ExchangePMI:
			return "PMI";
			break;
		case HPS::Component::ComponentType::ExchangePMIText:
			return "PMI: Text";
			break;
		case HPS::Component::ComponentType::ExchangePMIRichText:
			return "PMI: Rich Text";
			break;
		case HPS::Component::ComponentType::ExchangePMIRoughness:
			return "PMI: Roughness";
			break;
		case HPS::Component::ComponentType::ExchangePMIGDT:
			return "PMI: GDT";
			break;
		case HPS::Component::ComponentType::ExchangePMIDatum:
			return "PMI: Datum";
			break;
		case HPS::Component::ComponentType::ExchangePMILineWelding:
			return "PMI: Line Welding";
			break;
		case HPS::Component::ComponentType::ExchangePMISpotWelding:
			return "PMI: Spot Welding";
			break;
		case HPS::Component::ComponentType::ExchangePMIDimension:
			return "PMI: Dimension";
			break;
		case HPS::Component::ComponentType::ExchangePMIBalloon:
			return "PMI: Balloon";
			break;
		case HPS::Component::ComponentType::ExchangePMICoordinate:
			return "PMI: Coordinate";
			break;
		case HPS::Component::ComponentType::ExchangePMIFastener:
			return "PMI: Fastener";
			break;
		case HPS::Component::ComponentType::ExchangePMILocator:
			return "PMI: Locator";
			break;
		case HPS::Component::ComponentType::ExchangePMIMeasurementPoint:
			return "PMI: Measurement Point";
			break;

		case HPS::Component::ComponentType::ExchangeDrawingModel:
			return "Drawing Model";
			break;
		case HPS::Component::ComponentType::ExchangeDrawingView:
			return "Drawing View";
			break;
		case HPS::Component::ComponentType::ExchangeDrawingSheet:
			return "Drawing Sheet";
			break;
		case HPS::Component::ComponentType::ExchangeBasicDrawingBlock:
		case HPS::Component::ComponentType::ExchangeOperatorDrawingBlock:
			return "Drawing Block";
			break;

		case HPS::Component::ComponentType::ParasolidModelFile:
			return "Model File";
			break;
		case HPS::Component::ComponentType::ParasolidAssembly:
			return "Assembly";
			break;
		case HPS::Component::ComponentType::ParasolidTopoBody:
			return "Body";
			break;
		case HPS::Component::ComponentType::ParasolidTopoRegion:
			return "Region";
			break;
		case HPS::Component::ComponentType::ParasolidTopoShell:
			return "Shell";
			break;
		case HPS::Component::ComponentType::ParasolidTopoFace:
			return "Face";
			break;
		case HPS::Component::ComponentType::ParasolidTopoLoop:
			return "Loop";
			break;
		case HPS::Component::ComponentType::ParasolidTopoFin:
			return "Fin";
			break;
		case HPS::Component::ComponentType::ParasolidTopoEdge:
			return "Edge";
			break;
		case HPS::Component::ComponentType::ParasolidTopoVertex:
			return "Vertex";
			break;
		case HPS::Component::ComponentType::ParasolidInstance:
			return "Instance";
			break;
	}

	return "N/A";
}

_variant_t SimplePropertyHelper::GetMetadataValue(HPS::Metadata const & metadata)
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