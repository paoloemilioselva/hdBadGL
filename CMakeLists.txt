cmake_minimum_required(VERSION 3.23)

project( BadGL )
set( DELEGATE_NAME hdBadGL )

find_package( OpenGL REQUIRED )

set( HOUDINI_MAJ_MIN "19.5" )
set( HOUDINI_PATCH "773" )
set( HOUDINI_VERSION "Houdini ${HOUDINI_MAJ_MIN}.${HOUDINI_PATCH}" )
set( HOUDINI_ROOT "C:\\Program Files\\Side Effects Software\\${HOUDINI_VERSION}" )
set( HOUDINI_ROOT_USER "C:\\Users\\$ENV{USERNAME}\\Documents\\houdini${HOUDINI_MAJ_MIN}" )

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
