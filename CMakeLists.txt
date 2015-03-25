project(CCBOOST_SEGMENTATION_PLUGIN)

include (GenerateExportHeader)

# Version Number
set (PLUGIN_VERSION_MAJOR 0)
set (PLUGIN_VERSION_MINOR 2)
set (PLUGIN_VERSION_PATCH 0)

cmake_minimum_required(VERSION 2.6)

set (BUILD_STAND_ALONE   OFF CACHE BOOL "Build CCBOOST-Segmentation stand alone executable")
set (BUILD_ESPINA_PLUGIN ON  CACHE BOOL "Build CCBOOST-Segmentation ESPINA plugin executable")

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/CMake/Modules/")

if (BUILD_ESPINA_PLUGIN)
  find_package(ESPINA REQUIRED)
  include(${ESPINA_USE_FILE})

#FIXME espina headers need wuazip headers and
#if they are not in system dir we need to request them
#(find fetches the libraries too, thought we don't need them
  find_package(QuaZip REQUIRED)
  include_directories(${QUAZIP_INCLUDE_DIRS})
endif ()

find_package(Qt4 REQUIRED)
include(${QT_USE_FILE})

find_package(VTK REQUIRED NO_MODULE)
include(${VTK_USE_FILE})

find_package(ITK REQUIRED)
include(${ITK_USE_FILE})
    
find_package(OpenMP REQUIRED)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fopenmp")


include_directories (
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR} # UI files
)


# ccboost
include_directories( Core/auxItk )

set(auxItkFilters Core/auxItk/GradientMagnitudeImageFilter2.cxx
                  Core/auxItk/AllEigenVectorsOfHessian2.cxx
                  Core/auxItk/RepolarizeYVersorWithGradient2.cxx
                  Core/auxItk/EigenOfStructureTensorImageFilter2.cxx
                  Core/auxItk/ImageSplitter.h)

add_definitions(
  -std=c++11
  -fpermissive
)

set( iiboost_SOURCE_DIR Core/iiboost )
add_subdirectory( ${iiboost_SOURCE_DIR} )

include_directories( ${iiboost_SOURCE_DIR}
                     ${iiboost_SOURCE_DIR}/extra
                     ${iiboost_SOURCE_DIR}/deps/eigen
                     ${iiboost_SOURCE_DIR}/deps/rapidjson/include )

# RANDOMLIB
set(RANDOMLIB_DESTPATH "${CMAKE_BINARY_DIR}/randomlib")
message(STATUS "randomlib: ${RANDOMLIB_DESTPATH}")
set(RANDOMLIB_LIB "${RANDOMLIB_DESTPATH}/lib/libRandom.a")
set(RANDOMLIB_INCPATH "${RANDOMLIB_DESTPATH}/include")
include_directories(${RANDOMLIB_INCPATH})
# RANDOMLIB

#ccboost done
if(BUILD_STAND_ALONE)
  add_executable(ccbooststandalone Filter/main.cpp Filter/CcboostAdapter.cpp ${auxItkFilters})
  target_link_libraries(ccbooststandalone ${ITK_LIBRARIES} ${RANDOMLIB_LIB} )
  add_dependencies(ccbooststandalone randomlibBuild) 
  add_dependencies(ccbooststandalone randomlib)
endif(BUILD_STAND_ALONE)


if(BUILD_ESPINA_PLUGIN)

  qt4_add_resources (RCC_BUILT_SOURCES 
    GUI/rsc/CcboostSegmentation.qrc
  )

  # Create Interface files
  qt4_wrap_ui (UI_BUILT_SOURCES
    GUI/Settings/CcboostSegmentationSettings.ui
    DockWidget/CvlabPanel.ui
  #  GUI/FilterInspector/CcboostSegmentationFilterInspector.ui
  )

  QT4_WRAP_CPP(MOC_SOURCES
    Filter/CcboostAdapter.h
    Tasks/CcboostTask.h
    Tasks/ImportTask.h
    CcboostSegmentationPlugin.h
    DockWidget/CvlabPanel.h
    Preview/PreviewSliceRepresentation.h
    Preview/PreviewWidget.h
  #  Filter/CcboostSegmentationFilter.h
    GUI/Settings/CcboostSegmentationSettings.h
  #  GUI/FilterInspector/CcboostSegmentationFilterInspector.h

    OPTIONS -DBOOST_TT_HAS_OPERATOR_HPP_INCLUDED
  )

set (PLUGIN_SOURCES
  ${MOC_SOURCES}
  ${RCC_BUILT_SOURCES}
  ${UI_BUILT_SOURCES}
  CcboostSegmentationPlugin.cpp
#  Core/Extensions/CcboostSegmentationExtension.cpp
  Tasks/CcboostTask.cpp
  Tasks/ImportTask.cpp
  Filter/CcboostAdapter.cpp
  DockWidget/CvlabPanel.cpp
  Preview/PreviewSliceRepresentation.cpp
  Preview/PreviewWidget.cpp
#  Filter/CcboostSegmentationFilter.cpp
#  GUI/CcboostSegmentationToolGroup.cpp
  GUI/Settings/CcboostSegmentationSettings.cpp
  Preview/PreviewSliceRepresentation.cpp
  Preview/PreviewWidget.cpp
  DockWidget/CvlabPanel.cpp
  Label.cpp
#  GUI/FilterInspector/CcboostSegmentationFilterInspector.cpp
  ${auxItkFilters}
)

set (PLUGIN_LIBRARIES
  EspinaCore
  EspinaFilters
  EspinaGUI
  EspinaSupport
  EspinaUndo
  EspinaExtensions
  vtkInfovisCore
  ${ITK_LIBRARIES}
  ${RANDOMLIB_LIB}
  )

if(MINGW)
  if(DEFINED ESPINA_VERSION_MAJOR)
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/windllprops.rc.in" "${CMAKE_CURRENT_BINARY_DIR}/windllprops.rc")
    SET(PLUGIN_SOURCES ${PLUGIN_SOURCES} ${CMAKE_CURRENT_BINARY_DIR}/windllprops.rc)
    SET(CMAKE_RC_COMPILE_OBJECT "<CMAKE_RC_COMPILER> -O coff -o <OBJECT> -i <SOURCE>")
    ENABLE_LANGUAGE(RC)
  endif(DEFINED ESPINA_VERSION_MAJOR)
endif(MINGW)

add_library(CcboostSegmentationPlugin SHARED ${PLUGIN_SOURCES})
GENERATE_EXPORT_HEADER(
             CcboostSegmentationPlugin
             BASE_NAME CcboostSegmentationPlugin
             EXPORT_MACRO_NAME CcboostSegmentationPlugin_EXPORT
             EXPORT_FILE_NAME CcboostSegmentationPlugin_Export.h
  )
ADD_COMPILER_EXPORT_FLAGS()

target_link_libraries(CcboostSegmentationPlugin ${PLUGIN_LIBRARIES} ${QT_LIBRARIES} ${ITK_LIBRARIES})

if( CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64" )
  set_target_properties(CcboostSegmentationPlugin PROPERTIES COMPILE_FLAGS "-fPIC")
  endif( CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64" )

endif(BUILD_ESPINA_PLUGIN)

# install(
#   TARGETS CcboostSegmentationPlugin
#   DESTINATION "${INSTALL_BIN_DIR}/plugins"
#   )
# add_executable(ccbooststandalone Filter/main.cpp Filter/CcboostAdapter.cpp ${auxItkFilters})
# target_link_libraries(ccbooststandalone ${ITK_LIBRARIES} ${RANDOMLIB_LIB})
# endif()
