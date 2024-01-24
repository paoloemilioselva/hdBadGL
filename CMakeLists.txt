cmake_minimum_required(VERSION 3.23)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(TARGET "hdBadGL")
add_definitions(-DNOMINMAX)
add_definitions(-DTBB_USE_DEBUG)

project( 
    ${TARGET}
    LANGUAGES CXX C
)
set( DELEGATE_NAME ${TARGET} )

find_package( OpenGL REQUIRED )

# check target config
if(NOT TARGET_CONFIG)
    set( TARGET_CONFIG "usd-23.11")
endif()

string(REPLACE "-" ";" _t_config ${TARGET_CONFIG})
list(LENGTH _t_config _t_config_length)
if(_t_config_length LESS 2)
    MESSAGE(FATAL_ERROR "Wrong TARGET_CONFIG value " ${TARGET_CONFIG} )
endif()

list(GET _t_config 0 _target_name )

set( USD_LIBS "" )
set( USD_EMBEDDED FALSE )

if( _target_name MATCHES "houdini" )
    list(GET _t_config 1 HOUDINI_MAJ_MIN )
    list(GET _t_config 2 HOUDINI_PATCH )
    list(GET _t_config 3 HOUDINI_PYTHON_VERSION )
    set( HOUDINI_VERSION "Houdini ${HOUDINI_MAJ_MIN}.${HOUDINI_PATCH}" )
    set( HOUDINI_ROOT "C:\\Program Files\\Side Effects Software\\${HOUDINI_VERSION}" )

elseif( _target_name MATCHES "usd" )
    list(GET _t_config 1 USD_VERSION )
    set(USD_ROOT "C:\\dev\\usd-${USD_VERSION}" )
    set(USD_ROOT_FOR_BATCH ${USD_ROOT})
    if( USD_EMBEDDED )
        set(USD_ROOT_FOR_BATCH .\\usd-${USD_VERSION})
    endif()
    if(_t_config_length EQUAL 3)
        set( USD_EMBEDDED TRUE )
    endif()

else()
    MESSAGE(FATAL_ERROR "Invalid target " ${_target_name})
endif()

include(cmake/FindUsd.cmake)

include(cmake/FindTBB.cmake)

# Add GLFW
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory( "../glfw" "../glfw" )

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

target_compile_options( 
    ${DELEGATE_NAME}
    PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W3 /O2 /wd4273 /Zi /experimental:external /external:W0>
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:RelWithDebInfo>>:/Ob0 /Od> 
)

target_link_options( 
    ${DELEGATE_NAME}
    PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/ignore:4217 /ignore:4049> 
)

target_link_directories( 
    ${DELEGATE_NAME}
    PRIVATE 
    ${USD_LIBRARY_DIR}
)

#set_property(TARGET ${DELEGATE_NAME} PROPERTY IMPORTED_LOCATION ${Python_LIBRARIES})

target_link_libraries(
    ${DELEGATE_NAME}
    PUBLIC
    OpenGL::GL
    glfw
    ${USD_LIBS}
)


set(_installation_folder "")
if( _target_name MATCHES "houdini" )
    set(_installation_folder ${HOUDINI_ROOT_USER}/${DELEGATE_NAME})
    configure_file(houdini-extra/plugInfo.json plugInfo.json @ONLY)
    configure_file(houdini-extra/UsdRenderers.json UsdRenderers.json @ONLY)
    configure_file(houdini-extra/${DELEGATE_NAME}.json ${DELEGATE_NAME}.json @ONLY)
    install(TARGETS ${DELEGATE_NAME} DESTINATION ${_installation_folder}/dso/usd_plugins)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${DELEGATE_NAME}.json DESTINATION ${HOUDINI_ROOT_USER}/packages)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/UsdRenderers.json DESTINATION ${_installation_folder})
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/plugInfo.json DESTINATION ${_installation_folder}/dso/usd_plugins)

elseif( _target_name MATCHES "usd" )
    set(_installation_folder bin)
    install(TARGETS ${DELEGATE_NAME} RUNTIME DESTINATION ${_installation_folder}/usd-extra/plugin/usd)
    install(FILES ${CMAKE_SOURCE_DIR}/usd-extra/plugInfo.json DESTINATION ${_installation_folder}/usd-extra/plugin/usd/${DELEGATE_NAME}/resources )

endif()