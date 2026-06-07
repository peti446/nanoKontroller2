cmake_minimum_required(VERSION 4.2)

if(NOT DEFINED PROJECT_VERSION_STRING)
    set(PROJECT_VERSION_STRING "1.0.0")
endif()

project(nanoKontroller2 VERSION ${PROJECT_VERSION_STRING})

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
find_package(glaze REQUIRED)
find_package(cxxopts REQUIRED)
find_package(concurrentqueue REQUIRED)

pkg_check_modules(WIREPLUMBER REQUIRED wireplumber-0.5)
pkg_check_modules(GLIB        REQUIRED glib-2.0)
pkg_check_modules(GOBJECT     REQUIRED gobject-2.0)
pkg_check_modules(ALSA        REQUIRED alsa)
pkg_check_modules(RTMIDI      REQUIRED rtmidi)

file(GLOB_RECURSE SOURCES "src/*.cpp")

add_executable(nanoKontroller2 ${SOURCES})

target_include_directories(nanoKontroller2 PRIVATE
        includes
        ${WIREPLUMBER_INCLUDE_DIRS}
        ${GLIB_INCLUDE_DIRS}
        ${GOBJECT_INCLUDE_DIRS}
        ${ALSA_INCLUDE_DIRS}
        ${RTMIDI_INCLUDE_DIRS})

target_link_libraries(nanoKontroller2 PRIVATE
        glaze::glaze
        cxxopts::cxxopts
        concurrentqueue::concurrentqueue
        ${WIREPLUMBER_LIBRARIES}
        ${GLIB_LIBRARIES}
        ${GOBJECT_LIBRARIES}
        ${ALSA_LIBRARIES}
        ${RTMIDI_LIBRARIES}
)

target_compile_options(nanoKontroller2 PRIVATE
        ${WIREPLUMBER_CFLAGS_OTHER}
        ${GLIB_CFLAGS_OTHER}
        ${GOBJECT_CFLAGS_OTHER}
        ${ALSA_CFLAGS_OTHER}
        ${RTMIDI_CFLAGS_OTHER}
)

include(GNUInstallDirs)

install(TARGETS nanoKontroller2
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(FILES packaging/nanokontroller2.service
        DESTINATION lib/systemd/user)

install(FILES config-example.json
        DESTINATION ${CMAKE_INSTALL_DATADIR}/nanokontroller2
        RENAME config-example.json)

set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_NAME "nanokontroller2")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")   # add project(... VERSION x.y.z) above
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
        "Korg nanoKONTROL2 MIDI to PipeWire/WirePlumber bridge")
set(CPACK_DEBIAN_PACKAGE_SECTION "sound")
set(CPACK_PACKAGE_CONTACT "peti446 - Github")
#set(CPACK_DEBIAN_PACKAGE_DEPENDS "libwireplumber-0.5-0, libasound2, librtmidi7, libglib2.0-0")
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
        "${CMAKE_SOURCE_DIR}/packaging/postinst;${CMAKE_SOURCE_DIR}/packaging/prerm")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
include(CPack)