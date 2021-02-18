#include "stdafx.h"
#include "CHPSPropertiesPane.h"
#include "CHPSApp.h"
#include "CHPSDoc.h"
#include "CHPSView.h"
#include "CHPSSegmentBrowserPane.h"

#include "properties.h"

using namespace Property;


CHPSPropertiesPane::CHPSPropertiesPane()
	: sceneTreeItem(nullptr)
{
}


CHPSPropertiesPane::~CHPSPropertiesPane()
{
}


BEGIN_MESSAGE_MAP(CHPSPropertiesPane, CDockablePane)
	ON_WM_CREATE()
	ON_WM_SIZE()
	ON_WM_PAINT()
	ON_BN_CLICKED(IDB_APPLY_PROPERTIES_EDIT, OnApplyButton)
	ON_UPDATE_COMMAND_UI(IDB_APPLY_PROPERTIES_EDIT, OnUpdateApplyButton)
END_MESSAGE_MAP()



int CHPSPropertiesPane::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CDockablePane::OnCreate(lpCreateStruct) == -1)
		return -1;

	CRect rectDummy;
	rectDummy.SetRectEmpty();

	if (!propertyCtrl.Create(WS_VISIBLE | WS_CHILD, rectDummy, this, 321))
	{
		TRACE0("Failed to create properties window \n");
		return -1;      // fail to create
	}

	propertyCtrl.EnableHeaderCtrl(FALSE);
	propertyCtrl.SetVSDotNetLook();

	if (!applyButton.Create(_T("Apply"), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, rectDummy, this, IDB_APPLY_PROPERTIES_EDIT))
	{
		TRACE0("Failed to create push button\n");
		return -1;
	}

	// maintain a constant text size
	int const textSizePoints = 10;
	int dpiY = GetDeviceCaps(GetDC()->GetSafeHdc(), LOGPIXELSY);
	int fontHeightPixels = MulDiv(textSizePoints, dpiY, 72);	// there are 72 points per inch

	VERIFY(font.CreateFont(
		-fontHeightPixels,         // nHeight
		0,                         // nWidth
		0,                         // nEscapement
		0,                         // nOrientation
		FW_NORMAL,                 // nWeight
		FALSE,                     // bItalic
		FALSE,                     // bUnderline
		0,                         // cStrikeOut
		ANSI_CHARSET,              // nCharSet
		OUT_DEFAULT_PRECIS,        // nOutPrecision
		CLIP_DEFAULT_PRECIS,       // nClipPrecision
		DEFAULT_QUALITY,           // nQuality
		DEFAULT_PITCH | FF_SWISS,  // nPitchAndFamily
		_T("Arial")));                 // lpszFacename
	applyButton.SetFont(&font);

	AdjustLayout();

	return 0;
}

void CHPSPropertiesPane::OnSize(UINT nType, int cx, int cy)
{
	CDockablePane::OnSize(nType, cx, cy);
	AdjustLayout();
}

void CHPSPropertiesPane::AdjustLayout()
{
	if (GetSafeHwnd() == NULL || (AfxGetMainWnd() != NULL && AfxGetMainWnd()->IsIconic()))
		return;

	CRect rectClient;
	GetClientRect(rectClient);

	SIZE applyButtonSize;
	applyButton.GetIdealSize(&applyButtonSize);

	propertyCtrl.SetWindowPos(NULL, rectClient.left + 3, rectClient.top + 2, rectClient.Width() - 4, rectClient.Height() - applyButtonSize.cy - 7, SWP_NOACTIVATE | SWP_NOZORDER);
	applyButton.SetWindowPos(NULL, rectClient.left + 3, rectClient.Height() - applyButtonSize.cy - 2, applyButtonSize.cx, applyButtonSize.cy, SWP_NOACTIVATE | SWP_NOZORDER);
}


void CHPSPropertiesPane::OnPaint()
{
	CPaintDC dc(this); // device context for painting

	CRect paneRect;
	GetWindowRect(paneRect);
	ScreenToClient(paneRect);

	CBrush bg;
	bg.CreateStockObject(WHITE_BRUSH);
	dc.FillRect(&paneRect, &bg);

	CRect rectPropertyCtrl;
	propertyCtrl.GetWindowRect(rectPropertyCtrl);
	ScreenToClient(rectPropertyCtrl);
	rectPropertyCtrl.InflateRect(1, 1);
	dc.Draw3dRect(rectPropertyCtrl, ::GetSysColor(COLOR_3DSHADOW), ::GetSysColor(COLOR_3DSHADOW));
}

BOOL CHPSPropertiesPane::OnShowControlBarMenu(CPoint pt)
{
	CRect rc;
	GetClientRect(&rc);
	ClientToScreen(&rc);
	if (rc.PtInRect(pt))
		return TRUE;
	return CDockablePane::OnShowControlBarMenu(pt);
}

void CHPSPropertiesPane::Flush()
{
	SetWindowText(_T("Properties"));
	propertyCtrl.RemoveAll();
	sceneTreeItem = nullptr;
	rootProperty.reset();
	propertyCtrl.AdjustLayout();
}

void CHPSPropertiesPane::AddProperty(
	MFCSceneTreeItem * treeItem)
{
	AddProperty(treeItem, treeItem->GetItemType());
}

void CHPSPropertiesPane::AddProperty(
	MFCSceneTreeItem * treeItem,
	HPS::SceneTree::ItemType itemType)
{
	Flush();

	HPS::Key key = treeItem->GetKey();
	switch (itemType)
	{
		// Segment or WindowKey
		case HPS::SceneTree::ItemType::Segment:
		{
			if (key.Type() == HPS::Type::ApplicationWindowKey)
			{
				HPS::ApplicationWindowKey window(key);
				rootProperty.reset(new ApplicationWindowKeyProperty(propertyCtrl, window));
			}
			else
			{
				HPS::SegmentKey segment(key);
				rootProperty.reset(new SegmentKeyProperty(propertyCtrl, segment, treeItem->GetKeyPath()));
			}
		}	break;

		case HPS::SceneTree::ItemType::AttributeFilter:
		{
			HPS::Type keyType = key.Type();
			if (keyType == HPS::Type::StyleKey)
			{
				HPS::StyleKey style(key);
				rootProperty.reset(new StyleKeyAttributeFilterProperty(propertyCtrl, style));
			}
			else if (keyType == HPS::Type::IncludeKey)
			{
				HPS::IncludeKey include(key);
				rootProperty.reset(new IncludeKeyAttributeFilterProperty(propertyCtrl, include));
			}
		}	break;

		// Geometry
		case HPS::SceneTree::ItemType::CuttingSection:
		{
			SetWindowText(_T("Cutting Section"));
			HPS::CuttingSectionKey geometry(key);
			rootProperty.reset(new CuttingSectionKitProperty(propertyCtrl, geometry));
		}	break;

		case HPS::SceneTree::ItemType::Shell:
		{
			SetWindowText(_T("Shell"));
			HPS::ShellKey geometry(key);
			rootProperty.reset(new ShellKitProperty(propertyCtrl, geometry));
		}	break;

		case HPS::SceneTree::ItemType::Mesh:
		{
			SetWindowText(_T("Mesh"));
			HPS::MeshKey geometry(key);
			rootProperty.reset(new MeshKitProperty(propertyCtrl, geometry));
		}	break;

		case HPS::SceneTree::ItemType::Grid:
		{
			SetWindowText(_T("Grid"));
			HPS::GridKey geometry(key);
			rootProperty.reset(new GridKitProperty(propertyCtrl, geometry));
		}	break;

		case HPS::SceneTree::ItemType::NURBSSurface:
		{
			SetWindowText(_T("NURBS Surface"));
			HPS::NURBSSurfaceKey geometry(key);
			rootProperty.reset(new NURBSSurfaceKitProperty(propertyCtrl, geometry));
		}	break;

		case HPS::SceneTree::ItemType::Cylinder:
		{
			SetWindowText(_T("Cylinder"));
			HPS::CylinderKey geometry(key);
			rootProperty.reset(new CylinderKitProperty(propertyCtrl, geometry));
		}	break;

		case HPS::SceneTree::ItemType::Sphere:
		{
			SetWindowText(_T("Sphere"));
			HPS::SphereKey geometry(key);
			rootProperty.reset(new SphereKitProperty(propertyCtrl, geometry));
		}	break;

		case HPS::SceneTree::ItemType::Polygon:
		{
			SetWindowText(_T("Polygon"));
			HPS::PolygonKey geometry(key);
			rootProperty.reset(new PolygonKitProperty(propertyCtrl, geometry));
		}	break;

		case HPS::SceneTree::ItemType::Circle:
		{
			SetWindowText(_T("Circle"));
			HPS::CircleKey geometry(key);
			rootProperty.reset(new CircleKitProperty(propertyCtrl, geometry));
		}	break;

		case HPS::SceneTree::ItemType::CircularWedge:
		{
			SetWindowText(_T("CircularWedge"));
			HPS::CircularWedgeKey geometry(key);
			rootProperty.reset(new CircularWedgeKitProperty(propertyCtrl, geometry));
		}	break;

		case HPS::SceneTree::ItemType::Ellipse:
		{
			SetWindowText(_T("Ellipse"));
			HPS::EllipseKey geometry(key);
			rootProperty.reset(new EllipseKitProperty(propertyCtrl, geometry));
		}	break;

		case HPS::SceneTree::ItemType::Line:
		{
			SetWindowText(_T("Line"));
			HPS::LineKey geometry(key);
			rootProperty.reset(new LineKitProperty(propertyCtrl, geometry));
		}	break;

		case HPS::SceneTree::ItemType::NURBSCurve:
		{
			SetWindowText(_T("NURBS Curve"));
			HPS::NURBSCurveKey geometry(key);
			rootProperty.reset(new NURBSCurveKitProperty(propertyCtrl, geometry));
		}	break;

		case HPS::SceneTree::ItemType::CircularArc:
		{
			SetWindowText(_T("Circular Arc"));
			HPS::CircularArcKey geometry(key);
			rootProperty.reset(new CircularArcKitProperty(propertyCtrl, geometry));
		}	break;

		case HPS::SceneTree::ItemType::EllipticalArc:
		{
			SetWindowText(_T("Elliptical Arc"));
			HPS::EllipticalArcKey geometry(key);
			rootProperty.reset(new EllipticalArcKitProperty(propertyCtrl, geometry));
		}	break;

		case HPS::SceneTree::ItemType::InfiniteLine:
		case HPS::SceneTree::ItemType::InfiniteRay:
		{
			SetWindowText(_T("Infinite Line"));
			HPS::InfiniteLineKey geometry(key);
			rootProperty.reset(new InfiniteLineKitProperty(propertyCtrl, geometry));
		}	break;

		case HPS::SceneTree::ItemType::Marker:
		{
			SetWindowText(_T("Marker"));
			HPS::MarkerKey geometry(key);
			rootProperty.reset(new MarkerKitProperty(propertyCtrl, geometry));
		}	break;

		case HPS::SceneTree::ItemType::Text:
		{
			SetWindowText(_T("Text"));
			HPS::TextKey geometry(key);
			rootProperty.reset(new TextKitProperty(propertyCtrl, geometry));
		}	break;

		case HPS::SceneTree::ItemType::Reference:
		{
			SetWindowText(_T("Reference"));
			// should we do anything in this case?
		}	break;

		case HPS::SceneTree::ItemType::DistantLight:
		{
			SetWindowText(_T("Distant Light"));
			HPS::DistantLightKey geometry(key);
			rootProperty.reset(new DistantLightKitProperty(propertyCtrl, geometry));
		}	break;

		case HPS::SceneTree::ItemType::Spotlight:
		{
			SetWindowText(_T("Spotlight"));
			HPS::SpotlightKey geometry(key);
			rootProperty.reset(new SpotlightKitProperty(propertyCtrl, geometry));
		}	break;

		// Attributes
		case HPS::SceneTree::ItemType::Material:
		{
			SetWindowText(_T("Material Mapping"));
			HPS::SegmentKey segment(key);
			rootProperty.reset(new MaterialMappingKitProperty(propertyCtrl, segment));
		}	break;

		case HPS::SceneTree::ItemType::Camera:
		{
			SetWindowText(_T("Camera"));
			HPS::SegmentKey segment(key);
			rootProperty.reset(new CameraKitProperty(propertyCtrl, segment));
		}	break;

		case HPS::SceneTree::ItemType::ModellingMatrix:
		{
			SetWindowText(_T("Modelling Matrix"));
			if (key.HasType(HPS::Type::SegmentKey))
			{
				HPS::SegmentKey segment(key);
				rootProperty.reset(new SegmentKeyModellingMatrixProperty(propertyCtrl, segment));
			}
			else if (key.Type() == HPS::Type::ReferenceKey)
			{
				HPS::ReferenceKey reference(key);
				rootProperty.reset(new ReferenceKeyModellingMatrixProperty(propertyCtrl, reference));
			}
		}	break;

		case HPS::SceneTree::ItemType::UserData:
		{
			SetWindowText(_T("User Data"));
			HPS::SegmentKey segment(key);
			rootProperty.reset(new SegmentKeyUserDataProperty(propertyCtrl, segment));
		}	break;

		case HPS::SceneTree::ItemType::TextureMatrix:
		{
			SetWindowText(_T("Texture Matrix"));
			HPS::SegmentKey segment(key);
			rootProperty.reset(new SegmentKeyTextureMatrixProperty(propertyCtrl, segment));
		}	break;

		case HPS::SceneTree::ItemType::Culling:
		{
			SetWindowText(_T("Culling"));
			HPS::SegmentKey segment(key);
			rootProperty.reset(new CullingKitProperty(propertyCtrl, segment));
		}	break;

		case HPS::SceneTree::ItemType::CurveAttribute:
		{
			SetWindowText(_T("Curve Attribute"));
			HPS::SegmentKey segment(key);
			rootProperty.reset(new CurveAttributeKitProperty(propertyCtrl, segment));
		}	break;

		case HPS::SceneTree::ItemType::CylinderAttribute:
		{
			SetWindowText(_T("Cylinder Attribute"));
			HPS::SegmentKey segment(key);
			rootProperty.reset(new CylinderAttributeKitProperty(propertyCtrl, segment));
		}	break;

		case HPS::SceneTree::ItemType::EdgeAttribute:
		{
			SetWindowText(_T("Edge Attribute"));
			HPS::SegmentKey segment(key);
			rootProperty.reset(new EdgeAttributeKitProperty(propertyCtrl, segment));
		}	break;

		case HPS::SceneTree::ItemType::LightingAttribute:
		{
			SetWindowText(_T("Lighting Attribute"));
			HPS::SegmentKey segment(key);
			rootProperty.reset(new LightingAttributeKitProperty(propertyCtrl, segment));
		}	break;

		case HPS::SceneTree::ItemType::LineAttribute:
		{
			SetWindowText(_T("Line Attribute"));
			HPS::SegmentKey segment(key);
			rootProperty.reset(new LineAttributeKitProperty(propertyCtrl, segment));
		}	break;

		case HPS::SceneTree::ItemType::MarkerAttribute:
		{
			SetWindowText(_T("Marker Attribute"));
			HPS::SegmentKey segment(key);
			rootProperty.reset(new MarkerAttributeKitProperty(propertyCtrl, segment));
		}	break;

		case HPS::SceneTree::ItemType::SurfaceAttribute:
		{
			SetWindowText(_T("Surface Attribute"));
			HPS::SegmentKey segment(key);
			rootProperty.reset(new NURBSSurfaceAttributeKitProperty(propertyCtrl, segment));
		}	break;

		case HPS::SceneTree::ItemType::Selectability:
		{
			SetWindowText(_T("Selectability"));
			HPS::SegmentKey segment(key);
			rootProperty.reset(new SelectabilityKitProperty(propertyCtrl, segment));
		}	break;

		case HPS::SceneTree::ItemType::SphereAttribute:
		{
			SetWindowText(_T("Sphere Attribute"));
			HPS::SegmentKey segment(key);
			rootProperty.reset(new SphereAttributeKitProperty(propertyCtrl, segment));
		}	break;

		case HPS::SceneTree::ItemType::Subwindow:
		{
			SetWindowText(_T("Subwindow"));
			HPS::SegmentKey segment(key);
			rootProperty.reset(new SubwindowKitProperty(propertyCtrl, segment));
		}	break;

		case HPS::SceneTree::ItemType::TextAttribute:
		{
			SetWindowText(_T("Text Attribute"));
			HPS::SegmentKey segment(key);
			rootProperty.reset(new TextAttributeKitProperty(propertyCtrl, segment));
		}	break;

		case HPS::SceneTree::ItemType::Transparency:
		{
			SetWindowText(_T("Transparency"));
			HPS::SegmentKey segment(key);
			rootProperty.reset(new TransparencyKitProperty(propertyCtrl, segment));
		}	break;

		case HPS::SceneTree::ItemType::Visibility:
		{
			SetWindowText(_T("Visiblity"));
			HPS::SegmentKey segment(key);
			rootProperty.reset(new VisibilityKitProperty(propertyCtrl, segment));
		}	break;

		case HPS::SceneTree::ItemType::VisualEffects:
		{
			SetWindowText(_T("Visual Effects"));
			HPS::SegmentKey segment(key);
			rootProperty.reset(new VisualEffectsKitProperty(propertyCtrl, segment));
		}	break;

		case HPS::SceneTree::ItemType::Performance:
		{
			SetWindowText(_T("Performance"));
			HPS::SegmentKey segment(key);
			rootProperty.reset(new PerformanceKitProperty(propertyCtrl, segment));
		}	break;

		case HPS::SceneTree::ItemType::DrawingAttribute:
		{
			SetWindowText(_T("Drawing Attribute"));
			HPS::SegmentKey segment(key);
			rootProperty.reset(new DrawingAttributeKitProperty(propertyCtrl, segment));
		}	break;

		case HPS::SceneTree::ItemType::HiddenLineAttribute:
		{
			SetWindowText(_T("Hidden Line Attribute"));
			HPS::SegmentKey segment(key);
			rootProperty.reset(new HiddenLineAttributeKitProperty(propertyCtrl, segment));
		}	break;

		case HPS::SceneTree::ItemType::MaterialPalette:
		{
			SetWindowText(_T("Material Palette"));
			HPS::SegmentKey segment(key);
			rootProperty.reset(new SegmentKeyMaterialPaletteProperty(propertyCtrl, segment));
		}	break;

		case HPS::SceneTree::ItemType::ContourLine:
		{
			SetWindowText(_T("Contour Line"));
			HPS::SegmentKey segment(key);
			rootProperty.reset(new ContourLineKitProperty(propertyCtrl, segment));
		}	break;

		case HPS::SceneTree::ItemType::Condition:
		{
			SetWindowText(_T("Condition"));
			HPS::SegmentKey segment(key);
			rootProperty.reset(new SegmentKeyConditionProperty(propertyCtrl, segment));
		}	break;

		case HPS::SceneTree::ItemType::Bounding:
		{
			SetWindowText(_T("Bounding"));
			HPS::SegmentKey segment(key);
			rootProperty.reset(new BoundingKitProperty(propertyCtrl, segment));
		}	break;

		case HPS::SceneTree::ItemType::AttributeLock:
		{
			SetWindowText(_T("Attribute Lock"));
			HPS::SegmentKey segment(key);
			rootProperty.reset(new AttributeLockKitProperty(propertyCtrl, segment));
		}	break;

		case HPS::SceneTree::ItemType::TransformMask:
		{
			SetWindowText(_T("Transform Mask"));
			HPS::SegmentKey segment(key);
			rootProperty.reset(new TransformMaskKitProperty(propertyCtrl, segment));
		}	break;

		case HPS::SceneTree::ItemType::ColorInterpolation:
		{
			SetWindowText(_T("Color Interpolation"));
			HPS::SegmentKey segment(key);
			rootProperty.reset(new ColorInterpolationKitProperty(propertyCtrl, segment));
		}	break;

		case HPS::SceneTree::ItemType::CuttingSectionAttribute:
		{
			SetWindowText(_T("Cutting Section Attribute"));
			HPS::SegmentKey segment(key);
			rootProperty.reset(new CuttingSectionAttributeKitProperty(propertyCtrl, segment));
		}	break;

		case HPS::SceneTree::ItemType::Priority:
		{
			SetWindowText(_T("Priority"));
			HPS::SegmentKey segment(key);
			rootProperty.reset(new SegmentKeyPriorityProperty(propertyCtrl, segment));
		}	break;

		// window attributes
		case HPS::SceneTree::ItemType::Debugging:
		{
			SetWindowText(_T("Debugging"));
			HPS::WindowKey window(key);
			rootProperty.reset(new DebuggingKitProperty(propertyCtrl, window));
		}	break;

		case HPS::SceneTree::ItemType::PostProcessEffects:
		{
			SetWindowText(_T("Post Process Effects"));
			HPS::WindowKey window(key);
			rootProperty.reset(new PostProcessEffectsKitProperty(propertyCtrl, window));
		}	break;

		case HPS::SceneTree::ItemType::SelectionOptions:
		{
			SetWindowText(_T("Selection Options"));
			HPS::WindowKey window(key);
			rootProperty.reset(new SelectionOptionsKitProperty(propertyCtrl, window));
		}	break;

		case HPS::SceneTree::ItemType::UpdateOptions:
		{
			SetWindowText(_T("Update Options"));
			HPS::WindowKey window(key);
			rootProperty.reset(new UpdateOptionsKitProperty(propertyCtrl, window));
		}	break;

		// definitions
		case HPS::SceneTree::ItemType::MaterialPaletteDefinition:
		{
			SetWindowText(_T("Material Palette Definition"));
			HPS::MaterialPaletteDefinition mpdef;
			HPS::PortfolioKey(key).ShowMaterialPaletteDefinition(treeItem->GetTitle(), mpdef);
			rootProperty.reset(new MaterialPaletteDefinitionProperty(propertyCtrl, mpdef));
		}	break;

		case HPS::SceneTree::ItemType::TextureDefinition:
		{
			SetWindowText(_T("Texture Definition"));
			HPS::TextureDefinition tdef;
			HPS::PortfolioKey(key).ShowTextureDefinition(treeItem->GetTitle(), tdef);
			rootProperty.reset(new TextureDefinitionProperty(propertyCtrl, tdef));
		}	break;

		case HPS::SceneTree::ItemType::CubeMapDefinition:
		{
			SetWindowText(_T("Cube Map Definition"));
			HPS::CubeMapDefinition cmdef;
			HPS::PortfolioKey(key).ShowCubeMapDefinition(treeItem->GetTitle(), cmdef);
			rootProperty.reset(new CubeMapDefinitionProperty(propertyCtrl, cmdef));
		}	break;

		case HPS::SceneTree::ItemType::ImageDefinition:
		{
			SetWindowText(_T("Image Definition"));
			HPS::ImageDefinition idef;
			if (HPS::PortfolioKey(key).ShowImageDefinition(treeItem->GetTitle(), idef) || treeItem->GetKeyPath().ShowEffectiveImageDefinition(treeItem->GetTitle(), idef))
				rootProperty.reset(new ImageDefinitionProperty(propertyCtrl, idef));
		}	break;

		case HPS::SceneTree::ItemType::ShaderDefinition:
		{
			SetWindowText(_T("Shader Definition"));
			HPS::ShaderDefinition sdef;
			HPS::PortfolioKey(key).ShowShaderDefinition(treeItem->GetTitle(), sdef);
			rootProperty.reset(new ShaderDefinitionProperty(propertyCtrl, sdef));
		}	break;

		case HPS::SceneTree::ItemType::LinePatternDefinition:
		{
			SetWindowText(_T("Line Pattern Definition"));
			HPS::LinePatternDefinition lpdef;
			HPS::PortfolioKey(key).ShowLinePatternDefinition(treeItem->GetTitle(), lpdef);
			rootProperty.reset(new LinePatternDefinitionProperty(propertyCtrl, lpdef));
		}	break;

		case HPS::SceneTree::ItemType::GlyphDefinition:
		{
			SetWindowText(_T("Glyph Definition"));
			HPS::GlyphDefinition gdef;
			HPS::PortfolioKey(key).ShowGlyphDefinition(treeItem->GetTitle(), gdef);
			rootProperty.reset(new GlyphDefinitionProperty(propertyCtrl, gdef));
		}	break;

		case HPS::SceneTree::ItemType::ShapeDefinition:
		{
			SetWindowText(_T("Shape Definition"));
			HPS::ShapeDefinition sdef;
			HPS::PortfolioKey(key).ShowShapeDefinition(treeItem->GetTitle(), sdef);
			rootProperty.reset(new ShapeDefinitionProperty(propertyCtrl, sdef));
		}	break;
	}

	if (rootProperty)
		sceneTreeItem = treeItem;
}

void CHPSPropertiesPane::UnsetAttribute(
	MFCSceneTreeItem * treeItem)
{
	if (!treeItem->HasItemType(HPS::SceneTree::ItemType::Attribute))
		return;

	bool unsetAttribute = true;
	HPS::SceneTree::ItemType itemType = treeItem->GetItemType();
	HPS::SegmentKey segment(treeItem->GetKey());
	switch (itemType)
	{
		case HPS::SceneTree::ItemType::Material:
		{
			segment.UnsetMaterialMapping();
		}	break;

		case HPS::SceneTree::ItemType::Camera:
		{
			segment.UnsetCamera();
		}	break;

		case HPS::SceneTree::ItemType::ModellingMatrix:
		{
			segment.UnsetModellingMatrix();
		}	break;

		case HPS::SceneTree::ItemType::UserData:
		{
			segment.UnsetAllUserData();
		}	break;

		case HPS::SceneTree::ItemType::TextureMatrix:
		{
			segment.UnsetTextureMatrix();
		}	break;

		case HPS::SceneTree::ItemType::Culling:
		{
			segment.UnsetCulling();
		}	break;

		case HPS::SceneTree::ItemType::CurveAttribute:
		{
			segment.UnsetCurveAttribute();
		}	break;

		case HPS::SceneTree::ItemType::CylinderAttribute:
		{
			segment.UnsetCylinderAttribute();
		}	break;

		case HPS::SceneTree::ItemType::EdgeAttribute:
		{
			segment.UnsetEdgeAttribute();
		}	break;

		case HPS::SceneTree::ItemType::LightingAttribute:
		{
			segment.UnsetLightingAttribute();
		}	break;

		case HPS::SceneTree::ItemType::LineAttribute:
		{
			segment.UnsetLineAttribute();
		}	break;

		case HPS::SceneTree::ItemType::MarkerAttribute:
		{
			segment.UnsetMarkerAttribute();
		}	break;

		case HPS::SceneTree::ItemType::SurfaceAttribute:
		{
			segment.UnsetNURBSSurfaceAttribute();
		}	break;

		case HPS::SceneTree::ItemType::Selectability:
		{
			segment.UnsetSelectability();
		}	break;

		case HPS::SceneTree::ItemType::SphereAttribute:
		{
			segment.UnsetSphereAttribute();
		}	break;

		case HPS::SceneTree::ItemType::Subwindow:
		{
			segment.UnsetSubwindow();
		}	break;

		case HPS::SceneTree::ItemType::TextAttribute:
		{
			segment.UnsetTextAttribute();
		}	break;

		case HPS::SceneTree::ItemType::Transparency:
		{
			segment.UnsetTransparency();
		}	break;

		case HPS::SceneTree::ItemType::Visibility:
		{
			segment.UnsetVisibility();
		}	break;

		case HPS::SceneTree::ItemType::VisualEffects:
		{
			segment.UnsetVisualEffects();
		}	break;

		case HPS::SceneTree::ItemType::Performance:
		{
			segment.UnsetPerformance();
		}	break;

		case HPS::SceneTree::ItemType::DrawingAttribute:
		{
			segment.UnsetDrawingAttribute();
		}	break;

		case HPS::SceneTree::ItemType::HiddenLineAttribute:
		{
			segment.UnsetHiddenLineAttribute();
		}	break;

		case HPS::SceneTree::ItemType::MaterialPalette:
		{
			segment.UnsetMaterialPalette();
		}	break;

		case HPS::SceneTree::ItemType::ContourLine:
		{
			segment.UnsetContourLine();
		}	break;

		case HPS::SceneTree::ItemType::Condition:
		{
			segment.UnsetConditions();
		}	break;

		case HPS::SceneTree::ItemType::Bounding:
		{
			segment.UnsetBounding();
		}	break;

		case HPS::SceneTree::ItemType::AttributeLock:
		{
			segment.UnsetAttributeLock();
		}	break;

		case HPS::SceneTree::ItemType::TransformMask:
		{
			segment.UnsetTransformMask();
		}	break;

		case HPS::SceneTree::ItemType::ColorInterpolation:
		{
			segment.UnsetColorInterpolation();
		}	break;

		case HPS::SceneTree::ItemType::CuttingSectionAttribute:
		{
			segment.UnsetCuttingSectionAttribute();
		}	break;

		case HPS::SceneTree::ItemType::Priority:
		{
			segment.UnsetPriority();
		}	break;

		default:
		{
			unsetAttribute = false;
		}	break;
	}

	if (unsetAttribute)
	{
		sceneTreeItem = treeItem;
		ReExpandTree();
		UpdateCanvas();

		Flush();
	}
}

void CHPSPropertiesPane::OnApplyButton()
{
	if (rootProperty)
	{
		rootProperty->Apply();
		ReExpandTree();
		UpdateCanvas();

		Flush();
	}
}

void CHPSPropertiesPane::OnUpdateApplyButton(CCmdUI * pCmdUI)
{
	BOOL enable = FALSE;
	if (rootProperty)
		enable = TRUE;
	pCmdUI->Enable(enable);
}

void CHPSPropertiesPane::ReExpandTree()
{
	if (sceneTreeItem == nullptr)
		return;

	if (sceneTreeItem->HasItemType(HPS::SceneTree::ItemType::Attribute))
	{
		CTreeCtrl * treeCtrl = sceneTreeItem->GetTreeCtrl();
		HTREEITEM attributeGroupItem = treeCtrl->GetNextItem(sceneTreeItem->GetTreeItem(), TVGN_PARENT);
		HTREEITEM segmentItem = treeCtrl->GetNextItem(attributeGroupItem, TVGN_PARENT);
		auto segmentSceneTreeItem = reinterpret_cast<HPS::SceneTreeItem *>(treeCtrl->GetItemData(segmentItem));
		segmentSceneTreeItem->ReExpand();
	}
	else if (sceneTreeItem->GetItemType() == HPS::SceneTree::ItemType::Segment)
		sceneTreeItem->ReExpand();
}

void CHPSPropertiesPane::UpdateCanvas()
{
	CHPSDoc * doc = GetCHPSDoc();
	HPS::Canvas canvas = doc->GetCHPSView()->GetCanvas();
	canvas.Update();
}
