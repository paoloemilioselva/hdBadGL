cmake_minimum_required(VERSION 3.23)

project( BadGL )
set( DELEGATE_NAME hdBadGL )

find_package( OpenGL REQUIRED )

if(NOT TARGET_CONFIG)
    set( TARGET_CONFIG "houdini-19.5-773-39")
    #set( TARGET_CONFIG "houdini-20.0-506-310")
    #set( TARGET_CONFIG "usd-23.08")
endif()
string(REPLACE "-" ";" _t_config ${TARGET_CONFIG})
list(LENGTH _t_config _t_config_length)
if(_t_config_length LESS 2)
    MESSAGE(FATAL_ERROR "Wrong TARGET_CONFIG value " ${TARGET_CONFIG} )
endif()
list(GET _t_config 0 _target_name )
if( _target_name MATCHES "houdini" )
    list(GET _t_config 1 HOUDINI_MAJ_MIN )
    list(GET _t_config 2 HOUDINI_PATCH )
    list(GET _t_config 3 HOUDINI_PYTHON_VERSION )
    set( HOUDINI_VERSION "Houdini ${HOUDINI_MAJ_MIN}.${HOUDINI_PATCH}" )
    set( HOUDINI_ROOT "C:\\Program Files\\Side Effects Software\\${HOUDINI_VERSION}" )
    set( HOUDINI_ROOT_USER "C:\\Users\\$ENV{USERNAME}\\Documents\\houdini${HOUDINI_MAJ_MIN}" )
    MESSAGE(STATUS "Building for Houdini" )
    MESSAGE(STATUS "HOUDINI_VERSION = " ${HOUDINI_VERSION} )
    MESSAGE(STATUS "HOUDINI_ROOT = " ${HOUDINI_ROOT} )
    MESSAGE(STATUS "HOUDINI_ROOT_USER = " ${HOUDINI_ROOT_USER} )
    MESSAGE(STATUS "HOUDINI_PYTHON_VERSION = " ${HOUDINI_PYTHON_VERSION} )
elseif( _target_name MATCHES "usd" )
    MESSAGE(STATUS "Building for OpenUSD" )
else()
    MESSAGE(FATAL_ERROR "Invalid target " ${_target_name})
endif()

include(cmake/FindUsd.cmake)

add_library( ${DELEGATE_NAME} SHARED
    rendererPlugin.cpp
    rendererPlugin.h
    renderDelegate.cpp
    renderDelegate.h
    renderPass.cpp
    renderPass.h
    renderBuffer.cpp
    renderBuffer.h
    mesh.cpp
    mesh.h
    camera.cpp
    camera.h
    instancer.cpp
    instancer.h
    glad.c
    glad.h
)

target_link_libraries(
        ${DELEGATE_NAME}
        PUBLIC
        Usd::Usd
        OpenGL::GL
        )

target_compile_options(
        ${DELEGATE_NAME}
        PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/W4 /wd4273 /Zi /experimental:external /external:W0 /D> 
        $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:RelWithDebInfo>>:/Ob0 /Od>
        )

target_link_options(
        ${DELEGATE_NAME}
        PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/ignore:4217 /ignore:4049> 
        )

configure_file(plugInfo.json plugInfo.json @ONLY)
configure_file(UsdRenderers.json UsdRenderers.json @ONLY)
configure_file(${DELEGATE_NAME}.json ${DELEGATE_NAME}.json @ONLY)

install(TARGETS ${DELEGATE_NAME} DESTINATION ${HOUDINI_ROOT_USER}/${DELEGATE_NAME}/dso/usd_plugins)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${DELEGATE_NAME}.json DESTINATION ${HOUDINI_ROOT_USER}/packages)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/UsdRenderers.json DESTINATION ${HOUDINI_ROOT_USER}/${DELEGATE_NAME})
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/plugInfo.json DESTINATION ${HOUDINI_ROOT_USER}/${DELEGATE_NAME}/dso/usd_plugins)
