
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include_directories( ./ ../  ../../include  ../../common
/opt/ropod/ropod_common/ropodcpp/zyre_communicator/include)

# Qt related stuff
set(CMAKE_AUTOMOC ON)
SET(CMAKE_AUTOUIC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)

QT5_WRAP_UI ( COMMON_UI_SRC
    dialog_select_bb_variables.ui)

# Copied from the ropod_com_mediator:
# -----------------------------------------------------
find_package(jsoncpp REQUIRED)
IF (JSONCPP_FOUND)
    include_directories(${JSONCPP_INCLUDE_DIRS})
    list(APPEND LIBS ${JSONCPP_LIBRARIES})
ELSE (JSONCPP_FOUND)
    message( FATAL_ERROR "JSONCPP not found." )
ENDIF (JSONCPP_FOUND)

find_package(libzmq REQUIRED)
IF (LIBZMQ_FOUND)
    include_directories(${LIBZMQ_INCLUDE_DIRS})
    list(APPEND LIBS ${LIBZMQ_LIBRARIES})
ELSE (LIBZMQ_FOUND)
    message( FATAL_ERROR "libzmq not found." )
ENDIF (LIBZMQ_FOUND)

find_package(czmq REQUIRED)
IF (CZMQ_FOUND)
    include_directories(${CZMQ_INCLUDE_DIRS})
    list(APPEND LIBS ${CZMQ_LIBRARIES})
ELSE (CZMQ_FOUND)
    message( FATAL_ERROR "czmq not found." )
ENDIF (CZMQ_FOUND)

find_package(zyre REQUIRED)
IF (ZYRE_FOUND)
    include_directories(${ZYRE_INCLUDE_DIRS})
    list(APPEND LIBS ${ZYRE_LIBRARIES})
ELSE (ZYRE_FOUND)
    message( FATAL_ERROR "zyre not found." )
ENDIF (ZYRE_FOUND)
# -----------------------------------------------------

SET( SRC
    /opt/ropod/ropod_common/ropodcpp/zyre_communicator/src/ZyreBaseCommunicator.cpp
    datastream_bb.cpp
    ../../include/PlotJuggler/datastreamer_base.h
    dialog_select_bb_variables.cpp
    )

add_library(DataStreamBB SHARED ${SRC} ${COMMON_UI_SRC})
target_link_libraries(DataStreamBB  ${Qt5Widgets_LIBRARIES} ${Qt5Xml_LIBRARIES} ${LIBS} 
)

add_dependencies(DataStreamBB
    ${${PROJECT_NAME}_EXPORTED_TARGETS}
    ${catkin_EXPORTED_TARGETS}
    )

if(COMPILING_WITH_CATKIN)
    install(TARGETS DataStreamBB
        ARCHIVE DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION}
        LIBRARY DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION}
        RUNTIME DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION} )
else()
    install(TARGETS DataStreamBB DESTINATION bin  )
endif()
