#include "stdafx.h"
#include "PropertyHelper.h"
#include "ExchangeFaceHelper.h"
#include "PMIHelper.h"
#include "MainFrm.h"

#if defined(PARASOLID_DEMO_VIEWER)
#include "ParasolidFaceHelper.h"
#include "ParasolidCurveHelper.h"
#endif

PropertyHelper::PropertyHelper(
	size_t number_of_selected_items,
	HPS::CADModel const & cad_model,
	HPS::Component const & selected_component,
	HPS::Component const & ifc_component)
	: m_number_of_selected_items(number_of_selected_items)
	, m_cad_model(cad_model)
	, m_selected_component(selected_component)
	, m_ifc_component(ifc_component)
{
	CMainFrame * main_frame = (CMainFrame *)AfxGetApp()->m_pMainWnd;
	m_main_frame_handle = main_frame->GetSafeHwnd();
}

void PropertyHelper::SetComponentPath(HPS::ComponentArray const & in_path)
{
	m_key_path.Reset();

	m_component_path = in_path;
	if (!m_component_path.empty())
		m_key_path = HPS::ComponentPath(m_component_path).GetKeyPaths()[0];
}

void PropertyHelper::GetProperties(bool recalculate_triangle_count)
{
	try 
	{
		//flush information from the property pane
		PostMessage(m_main_frame_handle, WM_HOOPS_DEMO_VIEWER_REMOVE_ALL_PROPERTIES, 0, 0);

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
			if (m_selected_component.HasComponentType(HPS::Exchange::Component::ComponentType::ExchangePMIMask))
			{
				auto only_references_pmi = [](HPS::ComponentArray const & references)
				{
					bool only_references_pmi = true;
					for (auto const & one_reference : references)
					{
						if (!one_reference.HasComponentType(HPS::Component::ComponentType::ExchangePMIMask))
						{
							only_references_pmi = false;
							break;
						}
					}

					return only_references_pmi;
				};

				HPS::ComponentArray references = m_selected_component.GetReferences();
				while (!references.empty() && only_references_pmi(references))
					references = references[0].GetReferences();

				for (auto const & reference : references)
				{
					if (reference.HasComponentType(HPS::Component::ComponentType::ExchangePMIMask))
						continue;

					if (!reference.GetKeys().empty())
					{
						primary_component = reference;
						break;
					}
				}
			}

#		if defined(PARASOLID_DEMO_VIEWER)
			// Get properties from the Parasolid data underlying Exchange BRep representation items, not the Exchange BRep data.
			if (primary_component.GetComponentType() == HPS::Component::ComponentType::ExchangeRIBRepModel)
			{
				HPS::ComponentArray subcomponents = primary_component.GetSubcomponents();
				if (!subcomponents.empty() && subcomponents[0].HasComponentType(HPS::Component::ComponentType::ParasolidComponentMask))
					primary_component = subcomponents[0];
			}
#		endif

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

				GetMaterialProperties(exchange_component);

				if (m_selected_component.HasComponentType(HPS::Exchange::Component::ComponentType::ExchangePMIMask))
				{
					GetMetadata(m_selected_component, CommonProperty::PropertyType::PMI);
					GetPMIProperties(HPS::Exchange::Component(m_selected_component));
				}
				else
				{
					GetTransformationProperties(exchange_component);
					GetInheritanceProperties(exchange_component);
				}
			}
			else if (primary_component.HasComponentType(HPS::Component::ComponentType::ParasolidComponentMask))
			{
#			if defined(PARASOLID_DEMO_VIEWER)
				GetSelectionTypeProperties(primary_component);
				GetMetadata(primary_component, CommonProperty::PropertyType::Metadata);

				HPS::Parasolid::Component parasolid_component(primary_component);
				GetParasolidPhysicalProperties(parasolid_component);
				GetMaterialProperties(parasolid_component);

				if (m_selected_component.HasComponentType(HPS::Exchange::Component::ComponentType::ExchangePMIMask))
				{
					GetMetadata(m_selected_component, CommonProperty::PropertyType::PMI);

					int parasolid_entity = parasolid_component.GetParasolidEntity();
					CommonProperty::Property * tag_id_properties = new CommonProperty::Property("Referenced Parasolid Tag ID", parasolid_entity, CommonProperty::PropertyType::PMI);
					PostMessage(m_main_frame_handle, WM_HOOPS_DEMO_VIEWER_ADD_PROPERTY, reinterpret_cast<WPARAM>(tag_id_properties), 0);

					GetPMIProperties(HPS::Exchange::Component(m_selected_component));
				}
				else
					GetTransformationProperties(parasolid_component);
#			endif
			}
		}
	}
	catch (HPS::InvalidObjectException const &) {}
}

void PropertyHelper::GetSelectionTypeProperties(HPS::Component const & component)
{
	CommonProperty::Property * property = new CommonProperty::Property();
	property->property_type = CommonProperty::PropertyType::General;
	property->property_name = "Type";

	HPS::UTF8 p_type;
	if (component.GetComponentType() == HPS::Component::ComponentType::ParasolidInstance)
		p_type = ToString(component.GetSubcomponents()[0].GetComponentType());
	else
		p_type = ToString(component.GetComponentType());
	HPS::WCharArray characters;
	p_type.ToWStr(characters);

	property->property_value = characters.data();

	//The main thread takes ownership of the CommonProperty::Property structure
	PostMessage(m_main_frame_handle, WM_HOOPS_DEMO_VIEWER_ADD_PROPERTY, reinterpret_cast<WPARAM>(property), 0);
}

void PropertyHelper::GetMetadata(HPS::Component const & component, CommonProperty::PropertyType property_type)
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
			property->property_value = CommonProperty::GetMetadataValue(m);
			//The main thread takes ownership of the CommonProperty::Property structure
			PostMessage(m_main_frame_handle, WM_HOOPS_DEMO_VIEWER_ADD_PROPERTY, reinterpret_cast<WPARAM>(property), 0);
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
		PostMessage(m_main_frame_handle, WM_HOOPS_DEMO_VIEWER_ADD_PROPERTY, reinterpret_cast<WPARAM>(p), 0);
	}

	m_ifc_properties.clear();
	m_ifc_properties_keys.clear();
}

void PropertyHelper::GetExchangeEdgeLength(HPS::Exchange::Component const & edge_component, std::wstring const & units)
{
	A3DTopoEdge * edge = edge_component.GetExchangeEntity();
	A3DTopoEdgeData edge_data;
	A3D_INITIALIZE_DATA(A3DTopoEdgeData, edge_data);
	if (A3DTopoEdgeGet(edge, &edge_data) == A3D_SUCCESS)
	{
		double pocc_scale;
		double ri_scale;
		double context_scale;
		GetExchangeScale(edge_component, &pocc_scale, &ri_scale, &context_scale);

		//if the edge has a curve associated with it, this is easy
		A3DCrvBase * base_curve = edge_data.m_p3dCurve;
		double edge_length;
		if (base_curve != nullptr && A3DCurveLength(base_curve, NULL, &edge_length) == A3D_SUCCESS)
		{
			std::wstringstream length_string;
			length_string.precision(2);
			length_string << std::fixed << edge_length * context_scale * ri_scale * pocc_scale << L" " << units.c_str();
			CommonProperty::Property * edge_length_property = new CommonProperty::Property("Length", length_string.str().data(), CommonProperty::PropertyType::General);
			PostMessage(m_main_frame_handle, WM_HOOPS_DEMO_VIEWER_ADD_PROPERTY, reinterpret_cast<WPARAM>(edge_length_property), 0);
		}
		//otherwise we need to do some more work
		else if (base_curve == nullptr)
		{
			A3DTopoCoEdgeData co_edge_data;
			A3D_INITIALIZE_DATA(A3DTopoCoEdgeData, co_edge_data);
			A3DTopoFaceData face_data;
			A3D_INITIALIZE_DATA(A3DTopoFaceData, face_data);

			HPS::ComponentArray co_edges = edge_component.GetOwners();
			HPS::ComponentArray loops = co_edges[0].GetOwners();
			HPS::ComponentArray faces = loops[0].GetOwners();

			if (co_edges[0].GetComponentType() == HPS::Component::ComponentType::ExchangeTopoCoEdge)
			{
				HPS::Exchange::Component co_edge_component = co_edges[0];
				if (A3DTopoCoEdgeGet(co_edge_component.GetExchangeEntity(), &co_edge_data) == A3D_SUCCESS)
				{
					HPS::Exchange::Component face_component = faces[0];
					if (A3DTopoFaceGet(face_component.GetExchangeEntity(), &face_data) == A3D_SUCCESS)
					{
						A3DMiscCartesianTransformationData transformation_data;
						A3D_INITIALIZE_DATA(A3DMiscCartesianTransformationData, transformation_data);

						transformation_data.m_sOrigin.m_dX = 0 ;
						transformation_data.m_sOrigin.m_dY = 0 ;
						transformation_data.m_sOrigin.m_dZ = 0 ;
						transformation_data.m_sXVector.m_dX = 1 ;
						transformation_data.m_sXVector.m_dY = 0 ;
						transformation_data.m_sXVector.m_dZ = 0 ;
						transformation_data.m_sYVector.m_dX = 0 ;
						transformation_data.m_sYVector.m_dY = 1 ;
						transformation_data.m_sYVector.m_dZ = 0 ;
						transformation_data.m_sScale.m_dX = 1 ;
						transformation_data.m_sScale.m_dY = 1 ;
						transformation_data.m_sScale.m_dZ = 1 ;
						transformation_data.m_ucBehaviour = kA3DTransformationIdentity;

						A3DIntervalData interval_data;
						A3D_INITIALIZE_DATA(A3DIntervalData, interval_data);
						A3DCrvGetInterval(co_edge_data.m_pUVCurve, &interval_data);

						A3DParameterizationData parameterization_data;
						A3D_INITIALIZE_DATA(A3DParameterizationData, parameterization_data);
						parameterization_data.m_sInterval = interval_data;
						parameterization_data.m_dCoeffA = 1.0;

						A3DCrvOnSurf * curve_on_surface;
						A3DCrvOnSurfData curve_on_surface_data;
						A3D_INITIALIZE_DATA(A3DCrvOnSurfData, curve_on_surface_data);
						curve_on_surface_data.m_pUVCurve = co_edge_data.m_pUVCurve;
						curve_on_surface_data.m_bIs2D = false;
						curve_on_surface_data.m_sTrsf = transformation_data;
						curve_on_surface_data.m_sParam = parameterization_data;
						curve_on_surface_data.m_pSurface = face_data.m_pSurface;
						A3DCrvOnSurfCreate(&curve_on_surface_data, &curve_on_surface);

						double length;
						if (A3DCurveLength(curve_on_surface, 0, &length) == A3D_SUCCESS)
						{
							std::wstringstream length_string;
							length_string.precision(2);
							length_string << std::fixed << length * context_scale * ri_scale * pocc_scale << L" " << units.c_str();
							CommonProperty::Property * edge_length_property = new CommonProperty::Property("Length", length_string.str().data(), CommonProperty::PropertyType::General);
							PostMessage(m_main_frame_handle, WM_HOOPS_DEMO_VIEWER_ADD_PROPERTY, reinterpret_cast<WPARAM>(edge_length_property), 0);
						}
						A3DEntityDelete(curve_on_surface);
						A3DCrvGetInterval(nullptr, &interval_data);
						A3DTopoFaceGet(nullptr, &face_data);
					}
					A3DTopoCoEdgeGet(nullptr, &co_edge_data);
				}
			}
		}
	}

	A3DTopoEdgeGet(nullptr, &edge_data);
}

static A3DAsmProductOccurrence * get_pocc_external_data(
	A3DAsmProductOccurrenceData const & data)
{
	A3DAsmProductOccurrence * external_data = data.m_pExternalData;
	if (external_data == nullptr && data.m_pPrototype != nullptr)
	{
		A3DAsmProductOccurrenceData prototype_data;
		A3D_INITIALIZE_DATA(A3DAsmProductOccurrenceData, prototype_data);
		A3DStatus status = A3DAsmProductOccurrenceGet(data.m_pPrototype, &prototype_data);
		if (status == A3D_SUCCESS)
		{
			external_data = get_pocc_external_data(prototype_data);
			A3DAsmProductOccurrenceGet(nullptr, &prototype_data);
		}
	}
	return external_data;
}

static A3DMiscCartesianTransformation * get_pocc_location(
	A3DAsmProductOccurrence * pocc)
{
	if (pocc == nullptr)
		return nullptr;

	A3DMiscCartesianTransformation * location = nullptr;
	A3DAsmProductOccurrenceData data;
	A3D_INITIALIZE_DATA(A3DAsmProductOccurrenceData, data);
	A3DStatus status = A3DAsmProductOccurrenceGet(pocc, &data);
	if (status == A3D_SUCCESS)
	{
		location = data.m_pLocation;
		if (location == nullptr)
		{
			location = get_pocc_location(data.m_pPrototype);
			if (location == nullptr)
			{
				A3DAsmProductOccurrence * external_data = get_pocc_external_data(data);
				if (external_data != nullptr)
				{
					A3DAsmProductOccurrenceData external_data_data;
					A3D_INITIALIZE_DATA(A3DAsmProductOccurrenceData, external_data_data);
					status = A3DAsmProductOccurrenceGet(external_data, &external_data_data);
					if (status == A3D_SUCCESS)
					{
						location = external_data_data.m_pLocation;
						A3DAsmProductOccurrenceGet(nullptr, &external_data_data);
					}
				}
			}
		}

		A3DAsmProductOccurrenceGet(nullptr, &data);
	}

	return location;
}

void PropertyHelper::GetExchangeScale(HPS::Exchange::Component const & in_component, double * out_pocc_scale, double * out_ri_scale, double * out_context_scale)
{
	if (out_pocc_scale)
		*out_pocc_scale = 1;
	if (out_ri_scale)
		*out_ri_scale = 1;
	if (out_context_scale)
	{
#	if defined(EXCHANGE_DEMO_VIEWER)
		*out_context_scale = 1;
#	elif defined(PARASOLID_DEMO_VIEWER)
		*out_context_scale = 1000;
#	endif
	}

	HPS::Exchange::Component model;
	A3DTopoBody * a3d_body = nullptr;
	HPS::Component::ComponentType in_component_type = in_component.GetComponentType();

	if (in_component_type == HPS::Component::ComponentType::ExchangeRIBRepModel
		|| in_component_type == HPS::Component::ComponentType::ExchangeRIPolyBRepModel)
	{
		model = in_component;

#	if defined(EXCHANGE_DEMO_VIEWER)
		HPS::ComponentArray bodies = model.GetSubcomponents();
		if (!bodies.empty())
		{
			HPS::Exchange::Component body = bodies[0];
			a3d_body = body.GetExchangeEntity();
		}
#	elif defined(PARASOLID_DEMO_VIEWER)
		// In this case, the subcomponent of the RIBRepModel will be a Parasolid body.
		// So we need to use the Exchange APIs to get the A3DTopoBody so we can get the
		// context scale if the body has one.
		A3DRiBrepModel * a3d_model = model.GetExchangeEntity();
		A3DRiBrepModelData data;
		A3D_INITIALIZE_DATA(A3DRiBrepModelData, data);
		if (A3DRiBrepModelGet(a3d_model, &data) == A3D_SUCCESS)
		{
			a3d_body = data.m_pBrepData;
			A3DRiBrepModelGet(nullptr, &data);
		}
#	endif
	}
	else
	{
		HPS::Component face_component;
		if (in_component_type == HPS::Component::ComponentType::ExchangeTopoEdge)
		{
			HPS::ComponentArray co_edges = in_component.GetOwners();
			HPS::ComponentArray loops = co_edges[0].GetOwners();
			face_component = loops[0].GetOwners()[0];
		}
		else if (in_component_type == HPS::Component::ComponentType::ExchangeTopoFace)
			face_component = in_component;
		else
			return;

		HPS::ComponentArray shells = face_component.GetOwners();
		HPS::ComponentArray connexes = shells[0].GetOwners();
		HPS::ComponentArray bodies = connexes[0].GetOwners();
		HPS::Exchange::Component body = bodies[0];
		a3d_body = body.GetExchangeEntity();
		HPS::ComponentArray models = bodies[0].GetOwners();
		model = models[0];
	}

	if (out_pocc_scale)
	{
		HPS::Exchange::Component product_occurrence;
		bool use_component_path;
		auto it = std::find(m_component_path.begin(), m_component_path.end(), model);
		if (it != m_component_path.end())
		{
			use_component_path = true;
			it += 1;
			while (it->GetComponentType() == HPS::Component::ComponentType::ExchangePartDefinition)
				++it;
			product_occurrence = *it;
		}
		else
		{
			use_component_path = false;
			HPS::Component cursor = model.GetOwners()[0];
			while (cursor.GetComponentType() != HPS::Component::ComponentType::ExchangeProductOccurrence)
				cursor = cursor.GetOwners()[0];
			product_occurrence = cursor;
		}

		while (!product_occurrence.Empty() && product_occurrence.GetComponentType() == HPS::Component::ComponentType::ExchangeProductOccurrence)
		{
			A3DAsmProductOccurrence * a3d_pocc = product_occurrence.GetExchangeEntity();
			A3DMiscCartesianTransformation * location = get_pocc_location(a3d_pocc);
			if (location != nullptr)
			{
				A3DMiscCartesianTransformationData transformation_data;
				A3D_INITIALIZE_DATA(A3DMiscCartesianTransformationData, transformation_data);
				if (A3DMiscCartesianTransformationGet(location, &transformation_data) == A3D_SUCCESS)
				{
					*out_pocc_scale *= transformation_data.m_sScale.m_dX;
					A3DMiscCartesianTransformationGet(nullptr, &transformation_data);
				}
			}

			if (use_component_path)
				product_occurrence = *(++it);
			else
				product_occurrence = product_occurrence.GetOwners()[0];
		}
	}

	if (out_ri_scale)
	{
		if (model.HasComponentType(HPS::Component::ComponentType::ExchangeRepresentationItemMask))
		{
			A3DRiRepresentationItem * rep_item = model.GetExchangeEntity();
			A3DRiRepresentationItemData rep_item_data;
			A3D_INITIALIZE_DATA(A3DRiRepresentationItemData, rep_item_data);
			if (A3DRiRepresentationItemGet(rep_item, &rep_item_data) == A3D_SUCCESS)
			{
				if (rep_item_data.m_pCoordinateSystem != nullptr)
				{
					A3DRiCoordinateSystemData coordinate_data;
					A3D_INITIALIZE_DATA(A3DRiCoordinateSystemData, coordinate_data);
					if (A3DRiCoordinateSystemGet(rep_item_data.m_pCoordinateSystem, &coordinate_data) == A3D_SUCCESS)
					{
						if (coordinate_data.m_pTransformation != nullptr)
						{
							A3DMiscCartesianTransformationData transformation_data;
							A3D_INITIALIZE_DATA(A3DMiscCartesianTransformationData, transformation_data);
							if (A3DMiscCartesianTransformationGet(coordinate_data.m_pTransformation, &transformation_data) == A3D_SUCCESS)
							{
								*out_ri_scale = transformation_data.m_sScale.m_dX;
								A3DMiscCartesianTransformationGet(nullptr, &transformation_data);
							}
							A3DRiCoordinateSystemGet(nullptr, &coordinate_data);
						}
					}
				}
				A3DRiRepresentationItemGet(nullptr, &rep_item_data);
			}
		}
	}

	if (out_context_scale && a3d_body != nullptr)
	{
		A3DTopoBodyData topo_body_data;
		A3D_INITIALIZE_DATA(A3DTopoBodyData, topo_body_data);

		if (A3DTopoBodyGet(a3d_body, &topo_body_data) == A3D_SUCCESS)
		{
			A3DTopoContextData context_data;
			A3D_INITIALIZE_DATA(A3DTopoContextData, context_data);

			if (A3DTopoContextGet(topo_body_data.m_pContext, &context_data) == A3D_SUCCESS &&
				context_data.m_bHaveScale != 0)
				*out_context_scale = context_data.m_dScale;

			A3DTopoContextGet(nullptr, &context_data);
		}
		A3DTopoBodyGet(nullptr, &topo_body_data);
	}
}

std::wstring PropertyHelper::GetUnitsString()
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

void PropertyHelper::GetExchangePhysicalProperties(HPS::Exchange::Component const & exchange_component)
{
	std::wstring units = GetUnitsString();

	HPS::Component::ComponentType exchangeComponentType = exchange_component.GetComponentType();

	//If we selected a face get information on the surface and curve making up the face
	if (exchangeComponentType == HPS::Component::ComponentType::ExchangeTopoFace)
	{
		CommonProperty::Property * surface_properties = new CommonProperty::Property("Surface", 0, CommonProperty::PropertyType::General, true);

		HPS::ShellKey shell_key(exchange_component.GetKeys()[0]);
		HPS::IntArray face_list;
		shell_key.ShowFacelist(face_list);
		int number_of_triangles = ((int)face_list.size() / 4);
		CommonProperty::Property * triangle_number_properties = new CommonProperty::Property("No. of Triangles", number_of_triangles, CommonProperty::PropertyType::General);
		PostMessage(m_main_frame_handle, WM_HOOPS_DEMO_VIEWER_ADD_PROPERTY, reinterpret_cast<WPARAM>(triangle_number_properties), 0);

		double pocc_scale;
		double ri_scale;
		double context_scale;
		GetExchangeScale(exchange_component, &pocc_scale, &ri_scale, &context_scale);
		ExchangeFaceHelper helper(exchange_component, units, pocc_scale * ri_scale, context_scale);
		CommonProperty::Property * coedge_number_properties = new CommonProperty::Property("No. of Co-Edges", helper.GetEdgeNumber(), CommonProperty::PropertyType::General);
		PostMessage(m_main_frame_handle, WM_HOOPS_DEMO_VIEWER_ADD_PROPERTY, reinterpret_cast<WPARAM>(coedge_number_properties), 0);

		std::vector<CommonProperty::Property *> properties = helper.GetFaceProperties();
		for (auto p : properties)
			surface_properties->sub_properties.push_back(p);

		PostMessage(m_main_frame_handle, WM_HOOPS_DEMO_VIEWER_ADD_PROPERTY, reinterpret_cast<WPARAM>(surface_properties), 0);
	}

	//If we select an edge, we show the physical properties of its two co-edges
	else if (exchangeComponentType == HPS::Component::ComponentType::ExchangeTopoEdge)
	{
		GetExchangeEdgeLength(exchange_component, units);

		HPS::ComponentArray owners = exchange_component.GetOwners();
		CommonProperty::Property * co_edge_properties = new CommonProperty::Property("Co-Edge Properties", 0, CommonProperty::PropertyType::General, true);

		for (size_t i = 0; i < owners.size(); ++i)
		{
			if (owners[i].GetComponentType() == HPS::Exchange::Component::ComponentType::ExchangeTopoCoEdge)
			{
				std::stringstream text;
				text << "Co-Edge " << i + 1;
				CommonProperty::Property * coedge = new CommonProperty::Property(text.str().data(), 0, CommonProperty::PropertyType::General, true);

				A3DTopoCoEdgeData data;
				A3D_INITIALIZE_DATA(A3DTopoCoEdgeData, data);
				if (A3DTopoCoEdgeGet(HPS::Exchange::Component(owners[i]).GetExchangeEntity(), &data) == A3D_SUCCESS)
				{
					switch (data.m_ucOrientationWithLoop)
					{
					case 0:
						coedge->sub_properties.push_back(new CommonProperty::Property("Orientation", "Opposite", CommonProperty::PropertyType::General));
						break;
					case 1:
						coedge->sub_properties.push_back(new CommonProperty::Property("Orientation", "Same", CommonProperty::PropertyType::General));
						break;
					case 2:
						coedge->sub_properties.push_back(new CommonProperty::Property("Orientation", "Unknown", CommonProperty::PropertyType::General));
						break;
					}

					if (data.m_pUVCurve != nullptr)
					{
						//what do we do if there are no curve properties?
						ExchangeCurveHelper curve_helper;
						curve_helper.GetCurveProperties(data.m_pUVCurve, &coedge->sub_properties);
					}
					co_edge_properties->sub_properties.push_back(coedge);
					A3DTopoCoEdgeGet(nullptr, &data);
				}
			}
		}
		PostMessage(m_main_frame_handle, WM_HOOPS_DEMO_VIEWER_ADD_PROPERTY, reinterpret_cast<WPARAM>(co_edge_properties), 0);
	}

	//if we selected a model, get its physical data
	else
	{
		//first get the number of triangles, faces and edges
		int number_of_triangles = 0;
		_variant_t number_of_faces = 0;
		_variant_t number_of_edges = 0;
		GetComponentCount(exchange_component, number_of_triangles, number_of_faces.intVal, number_of_edges.intVal);

		if (exchangeComponentType == HPS::Component::ComponentType::ExchangeRIPolyBRepModel)
		{
			number_of_edges = "N/A";
			number_of_faces = "N/A";
		}

		CommonProperty::Property * no_of_edges_property = new CommonProperty::Property("No. of Edges", number_of_edges, CommonProperty::PropertyType::General);
		CommonProperty::Property * no_of_faces_property = new CommonProperty::Property("No. of Faces", number_of_faces, CommonProperty::PropertyType::General);
		CommonProperty::Property * no_of_triangles_property = new CommonProperty::Property("No. of Triangles", number_of_triangles, CommonProperty::PropertyType::General);

		PostMessage(m_main_frame_handle, WM_HOOPS_DEMO_VIEWER_ADD_PROPERTY, reinterpret_cast<WPARAM>(no_of_edges_property), 0);
		PostMessage(m_main_frame_handle, WM_HOOPS_DEMO_VIEWER_ADD_PROPERTY, reinterpret_cast<WPARAM>(no_of_faces_property), 0);
		PostMessage(m_main_frame_handle, WM_HOOPS_DEMO_VIEWER_ADD_PROPERTY, reinterpret_cast<WPARAM>(no_of_triangles_property), 0);

		if (exchangeComponentType == HPS::Exchange::Component::ComponentType::ExchangeRIBRepModel)
//			|| exchangeComponentType == HPS::Exchange::Component::ComponentType::ExchangeRIPolyBRepModel)
		{
			double volume = 0;
			double area = 0;
			A3DVector3dData center_of_gravity;
			GetExchangeComponentProperties(exchange_component, volume, area, center_of_gravity);

			//The main thread takes ownership of the CommonProperty::Property structures
			CommonProperty::Property * physical_properties = new CommonProperty::Property("Properties", 0, CommonProperty::PropertyType::General, true);

			hpsMFCView * view = ((hpsMFCApp *)AfxGetApp())->GetActiveView();
			if (view && !view->HasBRep())
			{
				physical_properties->sub_properties.push_back(new CommonProperty::Property("Import with BRep", "for Physical Properties", CommonProperty::PropertyType::General));
				PostMessage(m_main_frame_handle, WM_HOOPS_DEMO_VIEWER_ADD_PROPERTY, reinterpret_cast<WPARAM>(physical_properties), 0);
				return;
			}

			HPS::Vector cog((float)center_of_gravity.m_dX, (float)center_of_gravity.m_dY, (float)center_of_gravity.m_dZ);
			physical_properties->sub_properties.push_back(CommonProperty::Property::AddVector(cog, "Center of Gravity", CommonProperty::PropertyType::General));

			if (area > 0)
			{
				std::wstringstream surface_area_value;
				surface_area_value.precision(2);
				surface_area_value << std::fixed << area << L" " << units.c_str() << L"\xB2";
				physical_properties->sub_properties.push_back(new CommonProperty::Property("Surface Area", surface_area_value.str().data(), CommonProperty::PropertyType::General));
			}

			if (volume > 0)
			{
				std::wstringstream volume_value;
				volume_value.precision(2);
				volume_value << std::fixed << volume << L" " << units.c_str() << L"\xB3";
				physical_properties->sub_properties.push_back(new CommonProperty::Property("Volume", volume_value.str().data(), CommonProperty::PropertyType::General));
			}

			PostMessage(m_main_frame_handle, WM_HOOPS_DEMO_VIEWER_ADD_PROPERTY, reinterpret_cast<WPARAM>(physical_properties), 0);
		}

		else if (exchangeComponentType == HPS::Exchange::Component::ComponentType::ExchangeProductOccurrence)
		{
			// TODO: Refactor this into HPS::Component::GetAllSubcomponentCount after able to parse/wrap inner array typedef
			typedef std::vector<HPS::Component::ComponentType> ComponentTypeArray;
			std::function<size_t(HPS::Component const &, ComponentTypeArray)> countSubcomponents = [&](HPS::Component const & comp, ComponentTypeArray const & comp_types)
			{
				size_t count = 0;
				for (auto t : comp_types)
					if (comp.HasComponentType(t))
						count++;

				for (auto s : comp.GetSubcomponents())
					count += countSubcomponents(s, comp_types);

				return count;
			};

			ComponentTypeArray bodies_comp_types;
			bodies_comp_types.push_back(HPS::Component::ComponentType::ExchangeRIBRepModel);
			bodies_comp_types.push_back(HPS::Component::ComponentType::ExchangeRIPolyBRepModel);
			bodies_comp_types.push_back(HPS::Component::ComponentType::ExchangeRICurve);
			bodies_comp_types.push_back(HPS::Component::ComponentType::ExchangeRIPolyWire);
			size_t num_bodies = countSubcomponents(exchange_component, bodies_comp_types);

			ComponentTypeArray parts_comp_types;
			parts_comp_types.push_back(HPS::Component::ComponentType::ExchangePartDefinition);
			size_t num_parts = countSubcomponents(exchange_component, parts_comp_types);

			CommonProperty::Property * bodies_property = new CommonProperty::Property("No. of Bodies", (int)num_bodies, CommonProperty::PropertyType::General);
			CommonProperty::Property * parts_property = new CommonProperty::Property("No. of Parts", (int)num_parts, CommonProperty::PropertyType::General);

			PostMessage(m_main_frame_handle, WM_HOOPS_DEMO_VIEWER_ADD_PROPERTY, reinterpret_cast<WPARAM>(bodies_property), 0);
			PostMessage(m_main_frame_handle, WM_HOOPS_DEMO_VIEWER_ADD_PROPERTY, reinterpret_cast<WPARAM>(parts_property), 0);

			if (theApp.IsTracking())
			{
				if (exchange_component.GetOwners()[0].GetComponentType() == HPS::Component::ComponentType::ExchangeModelFile)
				{
					tbEventTrackNum(L"Bodies", L"Bodies", (double)num_bodies, NULL);
					tbEventTrackNum(L"Triangles", L"Triangles", (double)number_of_triangles, NULL);
				}
			}
		}
		
	}

	if (exchange_component.HasComponentType(HPS::Component::ComponentType::ExchangeRepresentationItemMask))
	{
		hpsMFCView * view = ((hpsMFCApp *)AfxGetApp())->GetActiveView();
		TessellationInfo tessellation_info = view->GetComponentTessellation(exchange_component);

		bool custom_tessellation = (tessellation_info.tessellation_level == Common::TessellationLevel::Custom);

		CommonProperty::Property * tessellation_property = nullptr;
		if (custom_tessellation)
		{
			tessellation_property = new CommonProperty::Property("Tessellation", 0, CommonProperty::PropertyType::General, true);

			CommonProperty::Property * tessellation_level = new CommonProperty::Property("Level", "Custom", CommonProperty::PropertyType::General);
			CommonProperty::Property * chord_type = new CommonProperty::Property("Chord Type", Common::ChordLimitToString(tessellation_info.chord_limit_type).data(), CommonProperty::PropertyType::General);
			CommonProperty::Property * chord_limit = new CommonProperty::Property("Chord Limit", (int)tessellation_info.chord_limit, CommonProperty::PropertyType::General);
			CommonProperty::Property * angle_tolerance = new CommonProperty::Property("Angle Tolerance", (int)tessellation_info.angle_tolerance, CommonProperty::PropertyType::General);

			tessellation_property->sub_properties.push_back(tessellation_level);
			tessellation_property->sub_properties.push_back(chord_type);
			tessellation_property->sub_properties.push_back(chord_limit);
			tessellation_property->sub_properties.push_back(angle_tolerance);
		}
		else
			tessellation_property = new CommonProperty::Property("Tessellation", Common::TessellationLevelToString(tessellation_info.tessellation_level).data(), CommonProperty::PropertyType::General);

		PostMessage(m_main_frame_handle, WM_HOOPS_DEMO_VIEWER_ADD_PROPERTY, reinterpret_cast<WPARAM>(tessellation_property), 0);
	}

#if defined SHOW_RELATED_PROPERTIES
	//if the selected component is a face or an edge, get the physical properties of its brepmodel
	if (exchangeComponentType == HPS::Component::ComponentType::ExchangeTopoEdge ||
		exchangeComponentType == HPS::Component::ComponentType::ExchangeTopoFace)
	{
		HPS::ComponentArray owners = exchange_component.GetOwners();
		while (owners.size() > 0 &&
			owners[0].GetComponentType() != HPS::Component::ComponentType::ExchangeRIBRepModel &&
			owners[0].GetComponentType() != HPS::Component::ComponentType::ExchangeRIPolyBRepModel)
			owners = owners[0].GetOwners();

		GetPhysicalProperties(owners[0]);
	}
#endif
}

void PropertyHelper::GetExchangeComponentProperties(HPS::Exchange::Component const & exchange_component, double & volume, double & area, A3DVector3dData & center_of_gravity)
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

void PropertyHelper::GetComponentCount(HPS::Component const & component, int & number_of_triangles, int & number_of_faces, int & number_of_edges)
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

void PropertyHelper::GetPolyBRepCount(HPS::Component const & component, int & out_tris, int & out_faces, int & out_edges)
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

void PropertyHelper::RecursiveComponentCount(HPS::Component const & component, int & number_of_triangles, int & number_of_faces, int & number_of_edges)
{
	HPS::Component::ComponentType componentType = component.GetComponentType();

	if (componentType == HPS::Component::ComponentType::ParasolidTopoRegion)
	{
		HPS::BooleanMetadata is_void = component.GetMetadata("IsRegionVoid");
		if (is_void.GetValue() == true)
			return;
	}

	HPS::ComponentArray subcomponents = component.GetSubcomponents();
	if (componentType == HPS::Component::ComponentType::ExchangeTopoShell || componentType == HPS::Component::ComponentType::ParasolidTopoShell)
		number_of_faces += (int)subcomponents.size();
	else if (componentType == HPS::Component::ComponentType::ExchangeRIPolyBRepModel || componentType == HPS::Component::ComponentType::ExchangeRIPolyWire
		|| (componentType == HPS::Component::ComponentType::ExchangeRIBRepModel && (subcomponents.empty() || subcomponents[0].GetSubcomponents().empty()))
		|| (componentType == HPS::Component::ComponentType::ExchangeRICurve && subcomponents.empty()))
		GetPolyBRepCount(component, number_of_triangles, number_of_faces, number_of_edges);
	else if (componentType == HPS::Component::ComponentType::ExchangeTopoFace || componentType == HPS::Component::ComponentType::ParasolidTopoFace)
	{
		HPS::KeyArray component_keys = component.GetKeys();
		if (component_keys.size() > 0)
		{
			HPS::ShellKey shell_key(component_keys[0]);
			HPS::IntArray face_list;
			shell_key.ShowFacelist(face_list);
			number_of_triangles += ((int)face_list.size() / 4);
		}
	}
	else if (componentType == HPS::Component::ComponentType::ExchangeTopoEdge)
	{
#	if defined(EXCHANGE_DEMO_VIEWER)
		HPS::Exchange::Component exchange_component(component);
		m_unique_edges.insert(exchange_component.GetExchangeEntity());
#	endif
		return;
	}
	else if (componentType == HPS::Component::ComponentType::ParasolidTopoEdge)
	{
#	if defined(PARASOLID_DEMO_VIEWER)
		HPS::Parasolid::Component parasolid_component(component);
		m_unique_edges.insert(parasolid_component.GetParasolidEntity());
#	endif
		return;
	}

	for (auto const & c : subcomponents)
		RecursiveComponentCount(c, number_of_triangles, number_of_faces, number_of_edges);
}

void PropertyHelper::GetMaterialProperties(HPS::Component const & component)
{
	//do not show material information for edges
	HPS::Component::ComponentType componentType = component.GetComponentType();
	if (componentType == HPS::Component::ComponentType::ExchangeTopoEdge ||
		componentType == HPS::Component::ComponentType::ParasolidTopoEdge)
		return;

	CommonProperty::GetMaterialProperties(m_main_frame_handle, m_key_path);
}

void PropertyHelper::GetTransformationProperties(HPS::Component const & component)
{
	//do not show transformation properties for pmi, edges and faces
	HPS::Component::ComponentType componentType = component.GetComponentType();
	if (componentType == HPS::Exchange::Component::ComponentType::ParasolidTopoEdge ||
		componentType == HPS::Exchange::Component::ComponentType::ParasolidTopoFace ||
		componentType == HPS::Exchange::Component::ComponentType::ExchangeTopoEdge ||
		componentType == HPS::Exchange::Component::ComponentType::ExchangeTopoFace ||
		component.HasComponentType(HPS::Exchange::Component::ComponentType::ExchangePMIMask))
		return;

	CommonProperty::GetTransformationProperties(m_main_frame_handle, m_key_path);
}

void PropertyHelper::GetPMIProperties(HPS::Exchange::Component const & pmi_component)
{
	HPS::Metadata units = m_cad_model.GetMetadata("Units");
	HPS::StringMetadata string_units(units);
	std::vector<CommonProperty::Property *> properties = PMIHelper(pmi_component).GetPMIProperties(string_units.GetValue());
	for (auto p : properties)
		PostMessage(m_main_frame_handle, WM_HOOPS_DEMO_VIEWER_ADD_PROPERTY, reinterpret_cast<WPARAM>(p), 0);
}

void PropertyHelper::GetInheritanceProperties(HPS::Exchange::Component const & exchange_component)
{
	A3DEntity * exchange_entity = exchange_component.GetExchangeEntity();
	if (A3DEntityIsBaseWithGraphicsType(exchange_entity))
	{
		A3DRootBaseWithGraphicsData data;
		A3D_INITIALIZE_DATA(A3DRootBaseWithGraphicsData, data);
		if (A3DRootBaseWithGraphicsGet(exchange_entity, &data) == A3D_SUCCESS)
		{
			A3DGraphicsData graphics_data;
			A3D_INITIALIZE_DATA(A3DGraphicsData, graphics_data);
			if (A3DGraphicsGet(data.m_pGraphics, &graphics_data) == A3D_SUCCESS)
			{
				CommonProperty::Property * inheritance_property = new CommonProperty::Property("Inheritance", 0, CommonProperty::PropertyType::General, true);
				if (graphics_data.m_usBehaviour & kA3DGraphicsSonHeritShow)
					inheritance_property->sub_properties.push_back(new CommonProperty::Property("Visibility", "Inherited by Child", CommonProperty::PropertyType::General));
				else if (graphics_data.m_usBehaviour & kA3DGraphicsFatherHeritShow)
					inheritance_property->sub_properties.push_back(new CommonProperty::Property("Visibility", "From Parent", CommonProperty::PropertyType::General));
				else if (graphics_data.m_usBehaviour & kA3DGraphicsShow)
					inheritance_property->sub_properties.push_back(new CommonProperty::Property("Visibility", "Entity is Visible", CommonProperty::PropertyType::General));

				if (graphics_data.m_usBehaviour & kA3DGraphicsFatherHeritColor)
					inheritance_property->sub_properties.push_back(new CommonProperty::Property("Color", "From Parent", CommonProperty::PropertyType::General));
				else if (graphics_data.m_usBehaviour & kA3DGraphicsSonHeritColor)
					inheritance_property->sub_properties.push_back(new CommonProperty::Property("Color", "Inherited by Child", CommonProperty::PropertyType::General));

				if (graphics_data.m_usBehaviour & kA3DGraphicsFatherHeritLayer)
					inheritance_property->sub_properties.push_back(new CommonProperty::Property("Layer", "From Parent", CommonProperty::PropertyType::General));
				else if (graphics_data.m_usBehaviour & kA3DGraphicsSonHeritLayer)
					inheritance_property->sub_properties.push_back(new CommonProperty::Property("Layer", "Inherited by Child", CommonProperty::PropertyType::General));

				if (graphics_data.m_usBehaviour & kA3DGraphicsFatherHeritTransparency)
					inheritance_property->sub_properties.push_back(new CommonProperty::Property("Transparency", "From Parent", CommonProperty::PropertyType::General));
				else if (graphics_data.m_usBehaviour & kA3DGraphicsSonHeritTransparency)
					inheritance_property->sub_properties.push_back(new CommonProperty::Property("Transparency", "Inherited by Child", CommonProperty::PropertyType::General));

				if (graphics_data.m_usBehaviour & kA3DGraphicsFatherHeritLinePattern)
					inheritance_property->sub_properties.push_back(new CommonProperty::Property("Line Pattern", "From Parent", CommonProperty::PropertyType::General));
				else if (graphics_data.m_usBehaviour & kA3DGraphicsSonHeritLinePattern)
					inheritance_property->sub_properties.push_back(new CommonProperty::Property("Line Pattern", "Inherited by Child", CommonProperty::PropertyType::General));

				if (graphics_data.m_usBehaviour & kA3DGraphicsFatherHeritLineWidth)
					inheritance_property->sub_properties.push_back(new CommonProperty::Property("Line Width", "From Parent", CommonProperty::PropertyType::General));
				else if (graphics_data.m_usBehaviour & kA3DGraphicsSonHeritLineWidth)
					inheritance_property->sub_properties.push_back(new CommonProperty::Property("Line Width", "Inherited by Child", CommonProperty::PropertyType::General));

				PostMessage(m_main_frame_handle, WM_HOOPS_DEMO_VIEWER_ADD_PROPERTY, reinterpret_cast<WPARAM>(inheritance_property), 0);
				A3DGraphicsGet(nullptr, &graphics_data);
			}

			A3DRootBaseWithGraphicsGet(nullptr, &data);
		}
	}
}

HPS::UTF8 PropertyHelper::ToString(HPS::Exchange::Component::ComponentType component_type)
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

#if defined(PARASOLID_DEMO_VIEWER)
void PropertyHelper::GetParasolidPhysicalProperties(HPS::Parasolid::Component const & parasolid_component)
{
	double scale = 1;
	std::wstring units = GetUnitsString();
	if (!units.empty())
	{
		HPS::Exchange::Component brep_model;
		for (auto const & component : m_component_path)
		{
			if (component.GetComponentType() == HPS::Component::ComponentType::ExchangeRIBRepModel)
			{
				brep_model = component;
				break;
			}
		}

		if (!brep_model.Empty())
		{
			double pocc_scale;
			double ri_scale;
			double context_scale;
			GetExchangeScale(brep_model, &pocc_scale, &ri_scale, &context_scale);
			scale = pocc_scale * ri_scale * context_scale;
		}
		else
			scale = 1000.0;
	}

	PK_ENTITY_t parasolid_entity = parasolid_component.GetParasolidEntity();

	CommonProperty::Property * tag_id_properties = new CommonProperty::Property("Parasolid Tag ID", parasolid_entity, CommonProperty::PropertyType::General);
	PostMessage(m_main_frame_handle, WM_HOOPS_DEMO_VIEWER_ADD_PROPERTY, reinterpret_cast<WPARAM>(tag_id_properties), 0);

	HPS::Component::ComponentType parasolidComponentType = parasolid_component.GetComponentType();

	//If we selected a face get information on the surface and curve making up the face
	if (parasolidComponentType == HPS::Component::ComponentType::ParasolidTopoFace)
	{
		CommonProperty::Property * surface_properties = new CommonProperty::Property("Parasolid Surface Properties", 0, CommonProperty::PropertyType::General, true);

		HPS::ShellKey shell_key(parasolid_component.GetKeys()[0]);
		HPS::IntArray face_list;
		shell_key.ShowFacelist(face_list);
		int number_of_triangles = ((int)face_list.size() / 4);
		CommonProperty::Property * triangle_number_properties = new CommonProperty::Property("No. of Triangles", number_of_triangles, CommonProperty::PropertyType::General);
		PostMessage(m_main_frame_handle, WM_HOOPS_DEMO_VIEWER_ADD_PROPERTY, reinterpret_cast<WPARAM>(triangle_number_properties), 0);

		ParasolidFaceHelper helper(parasolid_component, units, scale);
		CommonProperty::Property * coedge_number_properties = new CommonProperty::Property("No. of Co-Edges", helper.GetEdgeNumber(), CommonProperty::PropertyType::General);
		PostMessage(m_main_frame_handle, WM_HOOPS_DEMO_VIEWER_ADD_PROPERTY, reinterpret_cast<WPARAM>(coedge_number_properties), 0);

		std::vector<CommonProperty::Property *> properties = helper.GetFaceProperties();
		for (auto p : properties)
			surface_properties->sub_properties.push_back(p);

		PostMessage(m_main_frame_handle, WM_HOOPS_DEMO_VIEWER_ADD_PROPERTY, reinterpret_cast<WPARAM>(surface_properties), 0);
	}

	//If we select an edge, we show the physical properties of its two fins
	else if (parasolidComponentType == HPS::Component::ComponentType::ParasolidTopoEdge)
	{
		GetParasolidEdgeLength(parasolid_component, units, scale);

		HPS::ComponentArray owners = parasolid_component.GetOwners();
		CommonProperty::Property * fin_properties = new CommonProperty::Property("Parasolid Fin Properties", 0, CommonProperty::PropertyType::General, true);

		for (size_t i = 0; i < owners.size(); ++i)
		{
			if (owners[i].GetComponentType() == HPS::Parasolid::Component::ComponentType::ParasolidTopoFin)
			{
				std::stringstream text;
				text << "Fin " << i + 1;
				CommonProperty::Property * fin = new CommonProperty::Property(text.str().data(), 0, CommonProperty::PropertyType::General, true);

				ParasolidCurveHelper helper(scale);
				PK_CURVE_t curve;
				HPS::Parasolid::Component fin_component(owners[i]);
				PK_LOGICAL_t orientation;
				if (PK_FIN_ask_oriented_curve(fin_component.GetParasolidEntity(), &curve, &orientation) == PK_ERROR_no_errors)
				{
					if (curve != PK_ENTITY_null)
					{
						if (orientation == PK_LOGICAL_true)
							fin->sub_properties.push_back(new CommonProperty::Property("Orientation", "Parallel", CommonProperty::PropertyType::General));
						else
							fin->sub_properties.push_back(new CommonProperty::Property("Orientation", "Anti-Parallel", CommonProperty::PropertyType::General));

						helper.GetCurveProperties(curve, &fin->sub_properties);
					}
					else if (PK_FIN_ask_curve(fin_component.GetParasolidEntity(), &curve) == PK_ERROR_no_errors)
					{
						if (curve != PK_ENTITY_null)
							helper.GetCurveProperties(curve, &fin->sub_properties);
						else
						{
							if (PK_EDGE_ask_curve(parasolid_component.GetParasolidEntity(), &curve) == PK_ERROR_no_errors &&
								curve != PK_ENTITY_null)
								helper.GetCurveProperties(curve, &fin->sub_properties);
						}
					}
				}

				fin_properties->sub_properties.push_back(fin);
			}
		}
		PostMessage(m_main_frame_handle, WM_HOOPS_DEMO_VIEWER_ADD_PROPERTY, reinterpret_cast<WPARAM>(fin_properties), 0);
	}

	//if we selected a model, get its physical data
	else
	{
		//first get the number of triangles, faces and edges
		int number_of_triangles = 0;
		_variant_t number_of_faces = 0;
		_variant_t number_of_edges = 0;
		GetComponentCount(parasolid_component, number_of_triangles, number_of_faces.intVal, number_of_edges.intVal);

		CommonProperty::Property * no_of_edges_property = new CommonProperty::Property("No. of Edges", number_of_edges, CommonProperty::PropertyType::General);
		CommonProperty::Property * no_of_faces_property = new CommonProperty::Property("No. of Faces", number_of_faces, CommonProperty::PropertyType::General);
		CommonProperty::Property * no_of_triangles_property = new CommonProperty::Property("No. of Triangles", number_of_triangles, CommonProperty::PropertyType::General);

		PostMessage(m_main_frame_handle, WM_HOOPS_DEMO_VIEWER_ADD_PROPERTY, reinterpret_cast<WPARAM>(no_of_edges_property), 0);
		PostMessage(m_main_frame_handle, WM_HOOPS_DEMO_VIEWER_ADD_PROPERTY, reinterpret_cast<WPARAM>(no_of_faces_property), 0);
		PostMessage(m_main_frame_handle, WM_HOOPS_DEMO_VIEWER_ADD_PROPERTY, reinterpret_cast<WPARAM>(no_of_triangles_property), 0);

		if (parasolidComponentType == HPS::Parasolid::Component::ComponentType::ParasolidModelFile ||
			parasolidComponentType == HPS::Parasolid::Component::ComponentType::ParasolidAssembly ||
			parasolidComponentType == HPS::Parasolid::Component::ComponentType::ParasolidInstance ||
			parasolidComponentType == HPS::Parasolid::Component::ComponentType::ParasolidTopoBody)
		{
			HPS::Parasolid::Component property_component = parasolid_component;
			if (parasolidComponentType == HPS::Parasolid::Component::ComponentType::ParasolidInstance)
				property_component = parasolid_component.GetSubcomponents()[0];

			double volume = 0;
			double area = 0;
			double length = 0;
			HPS::Vector center_of_gravity;
			GetParasolidComponentProperties(property_component, volume, area, length, center_of_gravity);

			volume *= scale * scale * scale;
			area *= scale * scale;
			length *= scale;
			center_of_gravity *= static_cast<float>(scale);

			//The main thread takes ownership of the CommonProperty::Property structures
			CommonProperty::Property * physical_properties = new CommonProperty::Property("Properties", 0, CommonProperty::PropertyType::General, true);

			if (center_of_gravity != HPS::Vector())
				physical_properties->sub_properties.push_back(CommonProperty::Property::AddVector(center_of_gravity, "Center of Gravity", CommonProperty::PropertyType::General));

			if (area > 0)
			{
				std::wstringstream surface_area_value;
				surface_area_value << area;
				if (!units.empty())
					surface_area_value << L" " << units.c_str() << L"\xB2";
				physical_properties->sub_properties.push_back(new CommonProperty::Property("Surface Area", surface_area_value.str().c_str(), CommonProperty::PropertyType::General));
			}

			if (volume > 0)
			{
				std::wstringstream volume_value;
				volume_value << volume;
				if (!units.empty())
					volume_value << L" " << units.c_str() << L"\xB3";
				physical_properties->sub_properties.push_back(new CommonProperty::Property("Volume", volume_value.str().c_str(), CommonProperty::PropertyType::General));
			}

			if (length > 0)
			{
				std::wstringstream length_value;
				length_value << length;
				if (!units.empty())
					length_value << L" " << units.c_str();
				physical_properties->sub_properties.push_back(new CommonProperty::Property("Length", length_value.str().c_str(), CommonProperty::PropertyType::General));
			}

			PostMessage(m_main_frame_handle, WM_HOOPS_DEMO_VIEWER_ADD_PROPERTY, reinterpret_cast<WPARAM>(physical_properties), 0);
		}
	}

	HPS::Component tessellation_component = parasolid_component;
	if (parasolidComponentType == HPS::Component::ComponentType::ParasolidInstance)
	{
		tessellation_component = parasolid_component.GetSubcomponents()[0];
		parasolidComponentType = tessellation_component.GetComponentType();
	}

	if (parasolidComponentType == HPS::Component::ComponentType::ParasolidTopoBody)
	{
		hpsMFCView * view = ((hpsMFCApp *)AfxGetApp())->GetActiveView();
		TessellationInfo tessellation_info = view->GetComponentTessellation(tessellation_component);

		bool custom_tessellation = (tessellation_info.tessellation_level == Common::TessellationLevel::Custom);

		CommonProperty::Property * tessellation_property = nullptr;
		if (custom_tessellation)
		{
			tessellation_property = new CommonProperty::Property("Tessellation", 0, CommonProperty::PropertyType::General, true);

			CommonProperty::Property * tessellation_level = new CommonProperty::Property("Level", "Custom", CommonProperty::PropertyType::General);
			CommonProperty::Property * chord_limit = new CommonProperty::Property("Chord Limit", (int)tessellation_info.chord_limit, CommonProperty::PropertyType::General);
			CommonProperty::Property * angle_tolerance = new CommonProperty::Property("Angle Tolerance", (int)tessellation_info.angle_tolerance, CommonProperty::PropertyType::General);

			tessellation_property->sub_properties.push_back(tessellation_level);
			tessellation_property->sub_properties.push_back(chord_limit);
			tessellation_property->sub_properties.push_back(angle_tolerance);
		}
		else
			tessellation_property = new CommonProperty::Property("Tessellation", Common::TessellationLevelToString(tessellation_info.tessellation_level).data(), CommonProperty::PropertyType::General);

		PostMessage(m_main_frame_handle, WM_HOOPS_DEMO_VIEWER_ADD_PROPERTY, reinterpret_cast<WPARAM>(tessellation_property), 0);
	}
}

void PropertyHelper::GetParasolidEdgeLength(HPS::Parasolid::Component const & parasolid_component, std::wstring const & units, double scale)
{
	PK_TOPOL_t topologies = parasolid_component.GetParasolidEntity();
	PK_TOPOL_eval_mass_props_o_t mass_calculation_options;
	PK_TOPOL_eval_mass_props_o_m(mass_calculation_options);
	double length, mass, periphery;
	double center_of_gravity[3];
	double moment_of_inertia[9];
	PK_ERROR_code_t ret = PK_TOPOL_eval_mass_props(1, &topologies, 0.99, &mass_calculation_options, &length, &mass, center_of_gravity, moment_of_inertia, &periphery);

	if (ret == PK_ERROR_no_errors)
	{
		std::wstringstream length_value;
		length_value << length * scale;
		if (!units.empty())
			length_value << L" " << units.c_str();

		CommonProperty::Property * edge_length_property = new CommonProperty::Property("Length", length_value.str().c_str(), CommonProperty::PropertyType::General);
		PostMessage(m_main_frame_handle, WM_HOOPS_DEMO_VIEWER_ADD_PROPERTY, reinterpret_cast<WPARAM>(edge_length_property), 0);
	}
}

PK_ERROR_code_t PropertyHelper::GetMassProperties(const PK_TOPOL_t topol, double accuracy, const PK_TOPOL_eval_mass_props_o_t * options, double & volume, double & surface, double & mass, double c_of_g[3])
{
	PK_ERROR_code_t err = PK_ERROR_no_errors;
	volume = 0;
	surface = 0;
	mass = 0;
	c_of_g[0] = 0;
	c_of_g[1] = 0;
	c_of_g[2] = 0;

	PK_CLASS_t  class_type;
	PK_ENTITY_ask_class(topol, &class_type);
	if (class_type == PK_CLASS_assembly)
	{
		int n_parts = 0;
		PK_PART_t * parts = NULL;
		PK_ERROR_code_t err = PK_ASSEMBLY_ask_parts(topol, &n_parts, &parts);
		
		for (int ipart = 0;  ipart < n_parts; ipart++)
		{
			double tmp_volume = 0;
			double tmp_surface= 0;
			double tmp_mass = 0;
			double tmp_c_of_g[3] = { 0, 0, 0 };
			
			err = GetMassProperties(parts[ipart], accuracy, options, tmp_volume, tmp_surface, tmp_mass, tmp_c_of_g);
			if (err != PK_ERROR_no_errors)
				return err;

			volume += tmp_volume;
			surface += tmp_surface;
			mass += tmp_mass;
			c_of_g[0] += tmp_c_of_g[0] * tmp_mass;
			c_of_g[1] += tmp_c_of_g[1] * tmp_mass;
			c_of_g[2] += tmp_c_of_g[2] * tmp_mass;
		}
		if (mass != 0)
		{
			c_of_g[0] /= mass;
			c_of_g[1] /= mass;
			c_of_g[2] /= mass;
		}
	}
	else if (class_type == PK_CLASS_body)
	{
		PK_BODY_type_t body_type = 0;
		PK_ERROR_code_t err2 = PK_BODY_ask_type(topol, &body_type);

		if(err2 == PK_ERROR_no_errors)
		{
			double amount = 0;
			double periphery = 0;
			double m_of_i[9];

			if(body_type == PK_BODY_type_sheet_c || body_type == PK_BODY_type_solid_c)
			// case PK_BODY_type_general_c  => need to find the meaning of parasolid documentation about the dimensions
			{
				err = PK_TOPOL_eval_mass_props(1, &topol, 0.99, options, &amount, &mass, c_of_g, m_of_i, &periphery);
				if(err != PK_ERROR_no_errors)
					return err;

				if(body_type == PK_BODY_type_sheet_c)
					surface = amount;
				else
				{
					volume = amount;
					surface = periphery;
				}//what about PK_BODY_type_general_c
			}
		}
	}
	return err;
}

void PropertyHelper::GetParasolidComponentProperties(HPS::Parasolid::Component const & parasolid_component, double & volume, double & area, double & length, HPS::Vector & center_of_gravity)
{
	HPS::Component::ComponentType parasolidComponentType = parasolid_component.GetComponentType();
	if (parasolidComponentType == HPS::Component::ComponentType::ParasolidTopoBody)
		GetParasolidBodyProperties(parasolid_component.GetParasolidEntity(), volume, area, length, center_of_gravity);
	else if (parasolidComponentType == HPS::Component::ComponentType::ParasolidAssembly)
	{
		PK_TOPOL_t topologies = parasolid_component.GetParasolidEntity();
		PK_TOPOL_eval_mass_props_o_t mass_calculation_options;
		PK_TOPOL_eval_mass_props_o_m(mass_calculation_options);
		mass_calculation_options.mass = PK_mass_c_of_g_c;
		double amount = 0, mass = 0, periphery = 0;
		double cog[3] = {0, 0, 0};

		PK_ERROR_code_t ret = GetMassProperties(topologies, 0.99, &mass_calculation_options, volume, area, mass, cog);
		
		if (ret == PK_ERROR_no_errors)
		{
			center_of_gravity.x = (float)cog[0];
			center_of_gravity.y = (float)cog[1];
			center_of_gravity.z = (float)cog[2];
		}
		length = 0;
	}
	else if (parasolidComponentType == HPS::Component::ComponentType::ParasolidModelFile)
	{
		HPS::ComponentArray sub_components = parasolid_component.GetSubcomponents();
		for (auto const & one_component : sub_components)
		{
			double component_volume = 0;
			double component_area = 0;
			double component_length = 0;
			GetParasolidComponentProperties(one_component, component_volume, component_area, component_length, center_of_gravity);
			volume += component_volume;
			area += component_area;
			length += component_length;
		}
		center_of_gravity = HPS::Vector();
	}
	else
		ASSERT(0);
}

void PropertyHelper::GetParasolidBodyProperties(PK_BODY_t body, double & volume, double & area, double & length, HPS::Vector & center_of_gravity)
{
	PK_TOPOL_t topologies = body;
	PK_TOPOL_eval_mass_props_o_t mass_calculation_options;
	PK_TOPOL_eval_mass_props_o_m(mass_calculation_options);
	mass_calculation_options.mass = PK_mass_c_of_g_c;
	double amount, mass, periphery;
	double cog[3];
	double moment_of_inertia[9];

	PK_BODY_type_t body_type;
	PK_BODY_ask_type(body, &body_type);

	if(body_type == PK_BODY_type_wire_c
		|| body_type == PK_BODY_type_sheet_c 
		|| body_type == PK_BODY_type_solid_c 
		|| body_type == PK_BODY_type_general_c)
	{
		PK_ERROR_code_t ret = PK_TOPOL_eval_mass_props(1, &topologies, 0.99, &mass_calculation_options, &amount, &mass, cog, moment_of_inertia, &periphery);

		if (ret == PK_ERROR_no_errors)
		{

			if (body_type == PK_BODY_type_wire_c)
			{
				//amount == length
				//periphery not used
				volume = 0;
				area = 0;
				length = amount;
			}
			else if (body_type == PK_BODY_type_sheet_c)
			{
				//amount == surface area
				volume = 0;
				area = amount;
				length = 0;
			}
			else if (body_type == PK_BODY_type_solid_c || body_type == PK_BODY_type_general_c)
			{
				//amount == volume
				//periphery == surface area
				volume = amount;
				area = periphery;
				length = 0;
			}

			center_of_gravity.x = (float)cog[0];
			center_of_gravity.y = (float)cog[1];
			center_of_gravity.z = (float)cog[2];
		}
	}
}
#endif
