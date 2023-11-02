# Usd interface
add_library(UsdInterface INTERFACE)
add_library(Usd::Usd ALIAS UsdInterface)

if(DEFINED USD_ROOT)
    message(STATUS "Using Pixar USD: ${USD_ROOT}")
    include(cmake/FindPixarUsd.cmake)
elseif(DEFINED HOUDINI_ROOT)
    message(STATUS "Using Houdini USD: ${HOUDINI_ROOT}")
    include(cmake/FindHoudiniUsd.cmake)
else()
    message(FATAL_ERROR "No Houdini USD or Pixar USD ROOT was set, can not continue!")
endif()

message(STATUS "Using Usd Schema Generator: ${USD_SCHEMA_GENERATOR}")
