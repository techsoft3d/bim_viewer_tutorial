project(hps_mfc_sandbox)

string(TOUPPER ${PROJECT_NAME} PROJECT_UPPER)
option(${PROJECT_UPPER}_USING_EXCHANGE "Configure MFC Sandbox to use Exchange integration" OFF)
option(${PROJECT_UPPER}_USING_PUBLISH "Configure MFC Sandbox to use Publish integration" OFF)
option(${PROJECT_UPPER}_USING_PARASOLID "Configure MFC Sandbox to use Parasolid integration" OFF)
option(${PROJECT_UPPER}_USING_DWG "Configure MFC Sandbox to use DWG integration" OFF)

# MFC applications need this set; a value of 2 denotes use of the shared MFC lib
set(CMAKE_MFC_FLAG 2)

set(MFC_SANDBOX_SRC
	BaseEdit.cpp
	CHPSApp.cpp
	CHPSConfigurationPane.cpp
	CHPSDoc.cpp
	CHPSFrame.cpp
	CHPSModelBrowserPane.cpp
	CHPSPropertiesPane.cpp
	CHPSSegmentBrowserPane.cpp
	CHPSView.cpp
	CProgressDialog.cpp
	FloatEdit.cpp
	HPSExchangeProgressDialog.cpp
	SandboxHighlightOp.cpp
	stdafx.cpp
	UIntEdit.cpp
	UIntRangeEdit.cpp
	UnitFloatEdit.cpp
	hps_mfc_sandbox.rc
)

file(GLOB MFC_SANDBOX_HEADERS *.h)
file(GLOB MFC_SANDBOX_RESOURCE res/*)

add_executable(${PROJECT_NAME} WIN32
	${MFC_SANDBOX_SRC}
	${MFC_SANDBOX_HEADERS}
	${MFC_SANDBOX_RESOURCE}
)

target_compile_options(${PROJECT_NAME} PRIVATE
	/bigobj
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
	_AFXDLL
	UNICODE
	_UNICODE
)

target_include_directories(${PROJECT_NAME} PRIVATE ${HPS_DIR}/include)

target_link_libraries(${PROJECT_NAME} PRIVATE
	mfc${MSVC_MAJ_VERSION}0u$<$<CONFIG:Debug>:d>
    hps_core
    hps_sprk
    hps_sprk_ops
)

if(${PROJECT_UPPER}_USING_EXCHANGE)
	target_link_libraries(${PROJECT_NAME} PRIVATE hps_sprk_exchange)
	target_compile_definitions(${PROJECT_NAME} PRIVATE USING_EXCHANGE)
endif()

if(${PROJECT_UPPER}_USING_PUBLISH)
	target_link_libraries(${PROJECT_NAME} PRIVATE hps_sprk_publish)
	target_compile_definitions(${PROJECT_NAME} PRIVATE USING_PUBLISH)
endif()

if(${PROJECT_UPPER}_USING_PARASOLID)
	target_link_libraries(${PROJECT_NAME} PRIVATE hps_sprk_parasolid)
	target_compile_definitions(${PROJECT_NAME} PRIVATE USING_PARASOLID)
endif()

if(${PROJECT_UPPER}_USING_DWG)
	target_link_libraries(${PROJECT_NAME} PRIVATE hps_sprk_dwg)
	target_compile_definitions(${PROJECT_NAME} PRIVATE USING_DWG)
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
	FOLDER "Applications/Sandbox"
)

add_target_metadata(${PROJECT_NAME}
	"HPS MFC Sandbox"
	TEMPLATE hps_mfc_sandbox.rc2.in
	OUTPUT hps_mfc_sandbox.rc2
)

add_precompiled_header(${PROJECT_NAME} stdafx.h stdafx.cpp)

target_links_or_depends_on(${PROJECT_NAME} hps_directx11)

if (TARGET hps_vulkan)
	target_links_or_depends_on(${PROJECT_NAME} hps_vulkan)
endif()

if (TARGET hps_opengl2_mesa)
    add_dependencies(${PROJECT_NAME} hps_opengl2_mesa)
endif()

source_group("Resources" FILES ${MFC_SANDBOX_RESOURCE})
