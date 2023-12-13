
set(USD_LIBRARY_MONOLITHIC FALSE)

include_directories( ${BOOST_ROOT} )

get_filename_component(USD_INCLUDE_DIR ${USD_ROOT}/include ABSOLUTE)
get_filename_component(USD_LIBRARY_DIR ${USD_ROOT}/lib ABSOLUTE)
get_filename_component(USD_BIN_DIR ${USD_ROOT}/bin ABSOLUTE)

include_directories( ${USD_INCLUDE_DIR} )

set(PXR_LIB_PREFIX "lib")

#find_package(pxr CONFIG REQUIRED PATHS ${USD_ROOT} )

target_compile_definitions(UsdInterface
    INTERFACE
    BOOST_NS=boost
    $<$<CXX_COMPILER_ID:MSVC>:HAVE_SNPRINTF>
    $<$<CXX_COMPILER_ID:GNU>:TBB_USE_DEBUG=0>
    )

# Usd
set(_pxr_libs usd_ar;usd_arch;usd_cameraUtil;usd_garch;usd_gf;usd_glf;usd_hd;usd_hdSt;usd_hdx;usd_hf;usd_hgi;usd_hgiGL;usd_hio;usd_js;usd_kind;usd_ndr;usd_pcp;usd_plug;usd_pxOsd;usd_sdf;usd_sdr;usd_tf;usd_trace;usd_usd;usd_usdHydra;usd_usdImaging;usd_usdImagingGL;usd_usdLux;usd_usdRender;usd_usdShade;usd_usdSkel;usd_usdUtils;usd_usdVol;usd_vt;usd_work;usd_usdGeom)

foreach(_pxr_lib ${_pxr_libs})
    find_library(${_pxr_lib}_path
            NAMES
            ${_pxr_lib}
            PATHS
            ${USD_LIBRARY_DIR}
            REQUIRED
            )

    target_link_libraries(UsdInterface
            INTERFACE
            ${${_pxr_lib}_path}
            )
endforeach()

# Find Usd Schema Generator
if(USD_GEN_SCHEMA)
if (NOT USD_SCHEMA_GENERATOR)
    find_program(USD_SCHEMA_GENERATOR
            NAMES
            usdGenSchema
            usdGenSchema.py
            PATHS
            ${USD_BIN_DIR}
            REQUIRED
            NO_DEFAULT_PATH
            )

    get_filename_component(USD_SCHEMA_GENERATOR_EXT
            ${USD_SCHEMA_GENERATOR}
            EXT
            )

    if("${USD_SCHEMA_GENERATOR_EXT}" STREQUAL ".py")
        find_program(HYTHON_EXECUTABLE
                NAMES
                hython
                PATHS
                ${USD_BIN_DIR}
                REQUIRED
                NO_DEFAULT_PATH
                )
        list(PREPEND USD_SCHEMA_GENERATOR ${HYTHON_EXECUTABLE})
        set(USD_SCHEMA_GENERATOR ${USD_SCHEMA_GENERATOR} CACHE STRING "" FORCE)
    endif()
endif()
endif()