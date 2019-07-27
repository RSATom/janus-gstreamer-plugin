cmake_minimum_required(VERSION 3.0)

set(JANUS_PREFIX "/opt/janus" CACHE STRING "Janus Gateway install prefix")

project(janus-gstreamer-plugin)

SET(CMAKE_INSTALL_PREFIX ${JANUS_PREFIX})

find_package(PkgConfig REQUIRED)
pkg_search_module(GLIB REQUIRED glib-2.0)
pkg_search_module(GSTREAMER REQUIRED gstreamer-1.0)
pkg_search_module(GSTREAMER_APP REQUIRED gstreamer-app-1.0)
pkg_search_module(GSTREAMER_SDP REQUIRED gstreamer-sdp-1.0)

find_path(JANUS_INCLUDE_PATH janus/plugins/plugin.h
    PATHS ${JANUS_PREFIX}/include)

if(JANUS_INCLUDE_PATH STREQUAL "JANUS_INCLUDE_PATH-NOTFOUND")
   message(FATAL_ERROR "Unable to find Janus includes ")
endif()

file(GLOB SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    plugins/gstreamer/[^.]*.cpp
    plugins/gstreamer/[^.]*.h
    snap/snapcraft.yaml
    snap/hooks/install
    README.md)

add_library(${PROJECT_NAME} SHARED ${SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE
    ${JANUS_INCLUDE_PATH}
    ${JANUS_INCLUDE_PATH}/janus
    ${GLIB_INCLUDE_DIRS}
    ${GSTREAMER_INCLUDE_DIRS}
    ${GSTREAMER_APP_INCLUDE_DIRS}
    ${GSTREAMER_SDP_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME}
    ${GLIB_LIBRARIES}
    ${GSTREAMER_LDFLAGS}
    ${GSTREAMER_APP_LDFLAGS}
    ${GSTREAMER_SDP_LDFLAGS})

install(TARGETS ${PROJECT_NAME} DESTINATION lib/janus/plugins)
if(DEFINED ENV{SNAPCRAFT_BUILD_ENVIRONMENT})
    install(FILES conf/janus.plugin.gstreamer.jcfg DESTINATION etc/janus)
endif()
