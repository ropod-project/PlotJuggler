
find_package(PkgConfig)
include(FindPkgConfig)

pkg_check_modules(JSONCPP jsoncpp)
link_libraries(${JSONCPP_LIBRARIES})

pkg_check_modules(LIBMONGOCXX REQUIRED libmongocxx)
pkg_check_modules(LIBBSONCXX REQUIRED libbsoncxx)

# Qt related stuff
set(CMAKE_AUTOMOC ON)
SET(CMAKE_AUTOUIC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

#############
add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)
add_definitions(-DQT_SHARED)
#############

include_directories( 
    ./ 
    ../  
    ../../include  
    ../../common
    ${LIBMONGOCXX_INCLUDE_DIRS}
    ${LIBBSONCXX_INCLUDE_DIRS}
    ${JSONCPP_INCLUDE_DIRS}
)

add_definitions(${QT_DEFINITIONS})
add_definitions(-DQT_PLUGIN)

QT5_WRAP_UI ( UI_SRC  
    ../../include/PlotJuggler/selectlistdialog.ui 
    dialog_select_mongodb_collections.ui
    dialog_with_itemlist.ui
    )

SET( SRC
    dataload_mongodb.cpp
    ../../include/PlotJuggler/selectlistdialog.h
    ../../include/PlotJuggler/dataloader_base.h
    dialog_select_mongodb_collections.h
    dialog_select_mongodb_collections.cpp
    dialog_with_itemlist.h
    )

link_directories(
    ${LIBMONGOCXX_LIBRARY_DIRS}
    ${LIBBSONCXX_LIBRARY_DIRS}
)


add_library(DataLoadMongoDB SHARED ${SRC} ${UI_SRC}  )
target_link_libraries(DataLoadMongoDB  
    ${Qt5Widgets_LIBRARIES} 
    ${Qt5Xml_LIBRARIES}
    ${LIBMONGOCXX_LIBRARIES}
    ${LIBBSONCXX_LIBRARIES}
    jsoncpp
    )

if(COMPILING_WITH_CATKIN)
    install(TARGETS DataLoadMongoDB
        ARCHIVE DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION}
        LIBRARY DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION}
        RUNTIME DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION} )
else()
    install(TARGETS DataLoadMongoDB DESTINATION bin  )
endif()
