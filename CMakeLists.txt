project(CCBOOST_SEGMENTATION_PLUGIN)

include (GenerateExportHeader)

# Version Number
set (PLUGIN_VERSION_MAJOR 0)
set (PLUGIN_VERSION_MINOR 2)
set (PLUGIN_VERSION_PATCH 0)

cmake_minimum_required(VERSION 2.6)

set (BUILD_TESTING OFF  CACHE BOOL "Build EspINA Unitary Tests")

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/CMake/Modules/")

if (NOT COMPILING_ESPINA)

    find_package(ESPINA REQUIRED)
    include(${ESPINA_USE_FILE})

  find_package(Espina REQUIRED)
  include(${ESPINA_USE_FILE})
    
    find_package(Qt4 REQUIRED)
    include(${QT_USE_FILE})
    
    find_package(VTK REQUIRED)
    include(${VTK_USE_FILE})
    
    find_package(ITK REQUIRED)
    include(${ITK_USE_FILE})
    
endif ()

find_package(OpenMP REQUIRED)

#TODO fix FIndXlslib or remove dependency
find_package(XLSLIB)
#set(XLSLIB_INCLUDE_DIR "/home/monso/code/espina-project2/xlslib/src/")
#set(XLSLIB_LIBRARY "/home/monso/code/espina-project2/xlslib/src/.libs/libxls.so")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fopenmp")


include_directories (
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR} # UI files
  ${XLSLIB_INCLUDE_DIRS}
)


# ccboost
include_directories( Core/auxItk )

set(auxItkFilters Core/auxItk/GradientMagnitudeImageFilter2.cxx
                  Core/auxItk/AllEigenVectorsOfHessian2.cxx
                  Core/auxItk/RepolarizeYVersorWithGradient2.cxx
                  Core/auxItk/EigenOfStructureTensorImageFilter2.cxx
                  Core/auxItk/ImageSplitter.h)


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


qt4_add_resources (RCC_BUILT_SOURCES 
  GUI/rsc/CcboostSegmentation.qrc
  )

# Create Interface files
qt4_wrap_ui (UI_BUILT_SOURCES
  ${ESPINA_INCLUDE}/Support/Widgets/TabularReportEntry.ui
  GUI/Settings/CcboostSegmentationSettings.ui
#  GUI/FilterInspector/CcboostSegmentationFilterInspector.ui
)

QT4_WRAP_CPP(MOC_SOURCES
  CcboostSegmentationPlugin.h
#  Filter/CcboostSegmentationFilter.h
  GUI/Settings/CcboostSegmentationSettings.h
  GUI/CcboostSegmentationToolGroup.h
#  GUI/FilterInspector/CcboostSegmentationFilterInspector.h

  OPTIONS -DBOOST_TT_HAS_OPERATOR_HPP_INCLUDED
)

set (PLUGIN_SOURCES
  ${MOC_SOURCES}
  ${RCC_BUILT_SOURCES}
  ${UI_BUILT_SOURCES}
  CcboostSegmentationPlugin.cpp
#  Core/Extensions/CcboostSegmentationExtension.cpp
#  Core/Extensions/ExtensionFactory.cpp
  Tasks/CcboostTask.cpp
  Filter/CcboostAdapter.cpp
#  Filter/CcboostSegmentationFilter.cpp
#  Filter/SASFetchBehaviour.cpp
#  GUI/Analysis/SASAnalysisDialog.cpp
#  GUI/Analysis/SASTabularReport.cpp
#  GUI/Analysis/SASInformationProxy.cpp
  GUI/CcboostSegmentationToolGroup.cpp
  GUI/Settings/CcboostSegmentationSettings.cpp
#  GUI/FilterInspector/CcboostSegmentationFilterInspector.cpp
  ${auxItkFilters}
)

set (PLUGIN_LIBRARIES
  EspinaCore
  EspinaGUI
  EspinaUndo
  EspinaSupport
  EspinaExtensions
  vtkInfovisCore
  ${XLSLIB_LIBRARIES}
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
target_link_libraries(CcboostSegmentationPlugin ${PLUGIN_LIBRARIES} ${QT_LIBRARIES} ${ITK_LIBRARIES} ${QUAZIP_LIBRARIES})

if( CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64" )
  set_target_properties(CcboostSegmentationPlugin PROPERTIES COMPILE_FLAGS "-fPIC")
endif( CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64" )

install(
  TARGETS CcboostSegmentationPlugin
  DESTINATION "${INSTALL_BIN_DIR}/plugins"
  )

add_executable(postprocess Filter/CcboostAdapter.cpp ${auxItkFilters})
target_link_libraries(postprocess ${ITK_LIBRARIES} ${RANDOMLIB_LIB})
