cmake_minimum_required(VERSION 3.16)

project(BitTestQt VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core Quick Network WebSockets Concurrent Multimedia SerialPort)

qt_standard_project_setup(REQUIRES 6.10)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/config.ini")
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/config.ini" "${CMAKE_CURRENT_BINARY_DIR}/config.ini" COPYONLY)
endif()

# Content of downloaded design from Figma to Qt or similar
if (EXISTS "${CMAKE_CURRENT_LIST_DIR}/importedcontent/CMakeLists.txt")
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/importedcontent)
endif()



qt_add_executable(appBitTestQt
    main.cpp
)

qt_add_qml_module(appBitTestQt
    URI BitTestQt
    QML_FILES
        Main.qml
        SOURCES BusApiController.h BusApiController.cpp
        QML_FILES MenuWindow.qml
        QML_FILES EPDVerticalScreen.qml
        QML_FILES EPDHorizontalScreen.qml
        QML_FILES LEDScreen.qml
        SOURCES BusAudioEngine.h BusAudioEngine.cpp
        SOURCES BitConvertUtils.h BitConvertUtils.cpp
        SOURCES SSEnvProtocol.h
        SOURCES SSEnvModules.h SSEnvModules.cpp
        SOURCES SSEnvManager.h SSEnvManager.cpp
)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
set_target_properties(appBitTestQt PROPERTIES
#    MACOSX_BUNDLE_GUI_IDENTIFIER com.example.appBitTestQt
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(appBitTestQt
    PRIVATE
    Qt6::Core
    Qt6::Quick
    Qt6::Network
    Qt6::WebSockets
    Qt6::Concurrent
    Qt6::Multimedia
    Qt6::SerialPort
)

if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/sounds")
    add_custom_command(TARGET appBitTestQt POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/sounds"
            "$<TARGET_FILE_DIR:appBitTestQt>/sounds"
    )
endif()

install(TARGETS appBitTestQt
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
