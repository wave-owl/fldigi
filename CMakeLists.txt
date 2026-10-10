cmake_minimum_required(VERSION 3.16)

file(STRINGS "${CMAKE_SOURCE_DIR}/VERSION" FLDIGI_VERSION
    LIMIT_COUNT 1
)

string(STRIP "${FLDIGI_VERSION}" FLDIGI_VERSION)

project(fldigi
    VERSION ${FLDIGI_VERSION}
    LANGUAGES C CXX
)

# fldigi requires a C++11-capable compiler.
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

include(cmake/CheckFeatures.cmake)

include(GNUInstallDirs)

#
# External libraries used by the current Linux Autotools build
#

find_package(PkgConfig REQUIRED)

find_package(Threads REQUIRED)

pkg_check_modules(PORTAUDIO REQUIRED IMPORTED_TARGET portaudio-2.0)
pkg_check_modules(SNDFILE REQUIRED IMPORTED_TARGET sndfile)
pkg_check_modules(SAMPLERATE REQUIRED IMPORTED_TARGET samplerate)

if(NOT WIN32)
    pkg_check_modules(PULSEAUDIO REQUIRED IMPORTED_TARGET libpulse-simple)
    pkg_check_modules(UDEV REQUIRED IMPORTED_TARGET libudev)
    find_package(X11 REQUIRED)
    pkg_check_modules(X11_VERSION_INFO REQUIRED x11)
endif()

pkg_check_modules(HAMLIB REQUIRED IMPORTED_TARGET hamlib)
pkg_check_modules(PNG REQUIRED IMPORTED_TARGET libpng)

#
# FLTK
#
# Use fltk-config so that CMake uses the same FLTK installation and
# link options as the existing Autotools build.
#

if(WIN32)
    find_program(FLTK_CONFIG
        NAMES fltk-config
        PATHS
            "$ENV{HOME}/mxe/usr/x86_64-w64-mingw32.static/bin"
        NO_DEFAULT_PATH
        REQUIRED
    )
else()
    find_program(FLTK_CONFIG
        NAMES fltk-config
        HINTS /usr/local/bin
        REQUIRED
    )
endif()

execute_process(
    COMMAND ${FLTK_CONFIG} --version
    OUTPUT_VARIABLE FLTK_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

if(FLTK_VERSION VERSION_LESS "1.4.0")
    message(FATAL_ERROR
        "FLTK 1.4 or later is required; found ${FLTK_VERSION}"
    )
endif()

execute_process(
    COMMAND ${FLTK_CONFIG} --cxxflags
    OUTPUT_VARIABLE FLTK_CXXFLAGS_STRING
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process(
    COMMAND ${FLTK_CONFIG} --ldflags --use-images
    OUTPUT_VARIABLE FLTK_LDFLAGS_STRING
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

separate_arguments(FLTK_CXXFLAGS UNIX_COMMAND "${FLTK_CXXFLAGS_STRING}")
separate_arguments(FLTK_LDFLAGS UNIX_COMMAND "${FLTK_LDFLAGS_STRING}")

message(STATUS "FLTK version: ${FLTK_VERSION}")
message(STATUS "FLTK cxxflags: ${FLTK_CXXFLAGS_STRING}")
message(STATUS "FLTK ldflags: ${FLTK_LDFLAGS_STRING}")

#
# Configuration corresponding initially to the known-good Linux
# Autotools configuration.
#

set(GCC_VER_OK 1)

set(ENABLE_NLS 1)

if(WIN32)
    set(_WINDOWS 1)
    set(USE_FLXMLRPC 0)
    set(USE_HAMLIB 1)
    set(USE_LIBGPIOD 0)
    set(USE_LIBMBEDTLS 0)
    set(USE_OSS 0)
    set(USE_PNG 1)
    set(USE_PORTAUDIO 1)
    set(USE_PULSEAUDIO 0)
    set(USE_SAMPLERATE 1)
    set(USE_SNDFILE 1)
    set(USE_TLS 0)
    set(USE_X 0)
else()
    set(USE_FLXMLRPC 0)
    set(USE_HAMLIB 1)
    set(USE_LIBGPIOD 0)
    set(USE_LIBMBEDTLS 0)
    set(USE_OSS 1)
    set(USE_PNG 1)
    set(USE_PORTAUDIO 1)
    set(USE_PULSEAUDIO 1)
    set(USE_SAMPLERATE 1)
    set(USE_SNDFILE 1)
    set(USE_TLS 1)
    set(USE_X 1)
endif()

set(FLDIGI_AUTHORS "Dave Freese, Stelios Bounanos, and others")

set(FLDIGI_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(FLDIGI_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(FLDIGI_VERSION_PATCH ".${PROJECT_VERSION_PATCH}")

execute_process(
    COMMAND ${CMAKE_CXX_COMPILER} -dumpmachine
    OUTPUT_VARIABLE BUILD_TARGET_PLATFORM
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process(
    COMMAND ${CMAKE_CXX_COMPILER} --version
    OUTPUT_VARIABLE BUILD_COMPILER_FULL
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

string(REGEX MATCH "^[^\n]*" BUILD_COMPILER "${BUILD_COMPILER_FULL}")

execute_process(
    COMMAND date
    OUTPUT_VARIABLE BUILD_DATE
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process(
    COMMAND hostname
    OUTPUT_VARIABLE BUILD_HOST
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process(
    COMMAND whoami
    OUTPUT_VARIABLE BUILD_USER
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

set(BUILD_BUILD_PLATFORM "${BUILD_TARGET_PLATFORM}")
set(BUILD_HOST_PLATFORM "${BUILD_TARGET_PLATFORM}")
set(BUILD_CONFIGURE_ARGS "")

set(FLTK_BUILD_VERSION "${FLTK_VERSION}")
set(FLXMLRPC_BUILD_VERSION "")
set(HAMLIB_BUILD_VERSION "${HAMLIB_VERSION}")
set(LIBGPIOD_BUILD_VERSION "")
set(LIBMBEDTLS_BUILD_VERSION "")
set(PNG_BUILD_VERSION "${PNG_VERSION}")
set(PORTAUDIO_BUILD_VERSION "${PORTAUDIO_VERSION}")
set(SAMPLERATE_BUILD_VERSION "${SAMPLERATE_VERSION}")
set(SNDFILE_BUILD_VERSION "${SNDFILE_VERSION}")

if(NOT WIN32)
    set(PULSEAUDIO_BUILD_VERSION "${PULSEAUDIO_VERSION}")
    set(X_BUILD_VERSION "${X11_VERSION_INFO_VERSION}")
else()
    set(PULSEAUDIO_BUILD_VERSION "")
    set(X_BUILD_VERSION "")
endif()

set(FLDIGI_BUILD_CXXFLAGS
    "${FLTK_CXXFLAGS_STRING} -pipe -Wall -fexceptions"
)

set(FLDIGI_BUILD_LDFLAGS
    "${FLTK_LDFLAGS_STRING}"
)

configure_file(
    ${CMAKE_SOURCE_DIR}/cmake/config.h.in
    ${CMAKE_BINARY_DIR}/src/config.h
)

#
# Core fldigi sources
#

set(FLDIGI_SOURCES
    src/cmedia/cmedia.cxx
    src/cmedia/hid.cxx
    src/combo/combo.cxx

    src/config_script/create_default_script.cxx
    src/config_script/run_scripts.cxx
    src/config_script/script_parsing.cxx

    src/contestia/contestia.cxx

    src/cw/cw.cxx
    src/cw/ICOMkeying.cxx
    src/cw/KYkeying.cxx
    src/cw/morse.cxx
    src/cw/nanoIO.cxx
    src/cw/Nav.cxx
    src/cw/view_cw.cxx
    src/cw/winkeyer.cxx
    src/cw/YAESUkeying.cxx

    src/rtty/fsk.cxx
    src/rtty/rtty.cxx
    src/rtty/view_rtty.cxx

    src/scamp/scamp.cxx
    src/scamp/scamp_protocol.cxx
    src/scamp/view_scamp.cxx

    src/debug/debug.cxx
    src/debug/debug_dialog.cxx

    src/dialogs/confdialog.cxx
    src/dialogs/gpio_conf.cxx
    src/dialogs/fl_digi.cxx
    src/dialogs/font_browser.cxx
    src/dialogs/htmlstrings.cxx
    src/dialogs/notifydialog.cxx
    src/dialogs/record_browse.cxx
    src/dialogs/rxmon.cxx
    src/dialogs/testsigs.cxx
    src/dialogs/tod_clock.cxx
    src/dialogs/Viewer.cxx

    src/dominoex/dominoex.cxx
    src/dominoex/dominovar.cxx
    src/dtmf/dtmf.cxx

    src/dxcluster/dxcluster.cxx
    src/dxcluster/dx_dialog.cxx

    src/feld/feld.cxx
    src/feld/feldfonts.cxx

    src/fft-monitor/fft-monitor.cxx
    src/fft-monitor/spectrum.cxx
    src/fft-monitor/spectrum_viewer.cxx

    src/fileselector/fileselect.cxx

    src/filters/fftfilt.cxx
    src/filters/filters.cxx
    src/filters/viterbi.cxx

    src/fmt/fmt.cxx
    src/fmt/fmt_dialog.cxx

    src/fsq/fsq.cxx
    src/fsq/fsq_monitor.cxx

    src/globals/globals.cxx
    src/gpio_common/gpio_common.cxx

    src/ifkp/ifkp.cxx
    src/ifkp/tux.cxx

    src/irrxml/irrXML.cpp

    src/kml/kmlserver.cxx

    src/libtiniconv/tiniconv.c
    src/libtiniconv/tiniconv_desc.c

    src/logbook/adif_io.cxx
    src/logbook/calendar.cxx
    src/logbook/contest.cxx
    src/logbook/counties.cxx
    src/logbook/county_lists.cxx
    src/logbook/cty-dat.cxx
    src/logbook/date.cxx
    src/logbook/export_prefs.cxx
    src/logbook/fd_logger.cxx
    src/logbook/fd_view.cxx
    src/logbook/lgbook.cxx
    src/logbook/logbook.cxx
    src/logbook/logsupport.cxx
    src/logbook/lookupcall.cxx
    src/logbook/maclogger.cxx
    src/logbook/n3fjp_logger.cxx
    src/logbook/qrzlib.cxx
    src/logbook/qso_db.cxx
    src/logbook/table.cxx
    src/logbook/textio.cxx
    src/logbook/xmlrpc_log.cxx

    src/logger/logger.cxx
    src/logger/rx_extract.cxx
    src/logger/speak.cxx

    src/main.cxx

    src/mfsk/interleave.cxx
    src/mfsk/mfsk.cxx
    src/mfsk/mfskvaricode.cxx

    src/misc/arq_io.cxx
    src/misc/ascii.cxx
    src/misc/ax25_decode.cxx
    src/misc/charsetdistiller.cxx
    src/misc/charsetlist.cxx
    src/misc/configuration.cxx
    src/misc/coordinate.cxx
    src/misc/dxcc.cxx
    src/misc/estrings.cxx
    src/misc/icons.cxx
    src/misc/kiss_io.cxx
    src/misc/locator.cxx
    src/misc/log.cxx
    src/misc/macroedit.cxx
    src/misc/macros.cxx
    src/misc/misc.cxx
    src/misc/newinstall.cxx
    src/misc/outputencoder.cxx
    src/misc/pixmaps.cxx
    src/misc/pixmaps_tango.cxx
    src/misc/record_loader.cxx
    src/misc/re.cxx
    src/misc/stacktrace.cxx
    src/misc/strutil.cxx
    src/misc/threads.cxx
    src/misc/timeops.cxx
    src/misc/utf8file_io.cxx
    src/misc/util.cxx

    src/mt63/dsp.cxx
    src/mt63/mt63base.cxx
    src/mt63/mt63.cxx

    src/navtex/navtex.cxx

    src/network/ca_cert.cxx
    src/network/metar.cxx
    src/network/network.cxx
    src/network/socket.cxx
    src/network/weather.cxx

    src/olivia/olivia.cxx

    src/psk/pskcoeff.cxx
    src/psk/psk.cxx
    src/psk/pskeval.cxx
    src/psk/pskvaricode.cxx
    src/psk/viewpsk.cxx

    src/psm/psm.cxx

    src/qrunner/qrunner.cxx

    src/rigcontrol/FreqControl.cxx
    src/rigcontrol/norig.cxx
    src/rigcontrol/ptt.cxx
    src/rigcontrol/rigio.cxx
    src/rigcontrol/rigsupport.cxx
    src/rigcontrol/rigxml.cxx
    src/rigcontrol/serial.cxx
    src/rigcontrol/xmlrpc_rig.cxx

    src/rsid/rsid.cxx

    src/soundcard/audio_alert.cxx
    src/soundcard/bark.cxx
    src/soundcard/checkout.cxx
    src/soundcard/diesel.cxx
    src/soundcard/dinnerbell.cxx
    src/soundcard/doesnot.cxx
    src/soundcard/play.pa.cxx
    src/soundcard/soundconf.cxx
    src/soundcard/sound.cxx
    src/soundcard/steam_train.cxx
    src/soundcard/ttybell.cxx

    src/spot/notify.cxx
    src/spot/pskrep.cxx
    src/spot/spot.cxx

    src/ssb/ssb.cxx

    src/status/squelch_status.cxx
    src/status/status.cxx
    src/status/ui_colors.cxx

    src/synop-src/synop.cxx

    src/thor/thor.cxx
    src/thor/thorvaricode.cxx

    src/throb/throb.cxx

    src/timesig/timesig.cxx
    src/timesig/timesig_decoder.cxx
    src/timesig/timesig_formats.cxx

    src/trx/modem.cxx
    src/trx/nullmodem.cxx
    src/trx/test_signal.cxx
    src/trx/trx.cxx

    src/waterfall/colorbox.cxx
    src/waterfall/digiscope.cxx
    src/waterfall/raster.cxx
    src/waterfall/waterfall.cxx

    src/wefax/wefax.cxx
    src/wefax/wefax_map.cxx
    src/wefax/wefax-pic.cxx

    src/widgets/flinput2.cxx
    src/widgets/flmisc.cxx
    src/widgets/flslider2.cxx
    src/widgets/Fl_Text_Buffer_mod.cxx
    src/widgets/Fl_Text_Display_mod.cxx
    src/widgets/Fl_Text_Editor_mod.cxx
    src/widgets/FTextRXTX.cxx
    src/widgets/FTextView.cxx
    src/widgets/Panel.cxx
    src/widgets/picture.cxx
    src/widgets/plot_xy.cxx
    src/widgets/progress.cxx
    src/widgets/psk_browser.cxx
    src/widgets/pwrmeter.cxx
    src/widgets/smeter.cxx
    src/widgets/vumeter.cxx

    src/wwv/analysis.cxx
    src/wwv/wwv.cxx

    src/compat/regex.c
)

set(FLDIGI_XMLRPC_SOURCES
    src/network/xmlrpc.cxx
)

file(GLOB FLDIGI_XMLRPCPP_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/xmlrpcpp/*.cpp"
)

file(GLOB FLDIGI_MBEDTLS_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/mbedtls/*.c"
)

set(FLDIGI_HAMLIB_SOURCES
    src/rigcontrol/hamlib.cxx
    src/rigcontrol/rigclass.cxx
)

set(FLDIGI_NLS_SOURCES
    src/misc/nls.cxx
)

add_executable(fldigi
    ${FLDIGI_SOURCES}
    ${FLDIGI_XMLRPC_SOURCES}
    ${FLDIGI_XMLRPCPP_SOURCES}
    ${FLDIGI_MBEDTLS_SOURCES}
    ${FLDIGI_HAMLIB_SOURCES}
    ${FLDIGI_NLS_SOURCES}
)

if(WIN32)
    string(TIMESTAMP RC_BUILD_TIME "%s" UTC)

    set(FLDIGI_RC_OBJECT
        "${CMAKE_CURRENT_BINARY_DIR}/fldigirc.rc.o"
    )

    add_custom_command(
        OUTPUT "${FLDIGI_RC_OBJECT}"
        COMMAND "${CMAKE_RC_COMPILER}"
            "-DRC_BUILD_TIME=\\\"${RC_BUILD_TIME}\\\""
            "-I${CMAKE_BINARY_DIR}/src"
            "-I${CMAKE_SOURCE_DIR}/src"
            "-I${CMAKE_SOURCE_DIR}/src/include"
            "-I${CMAKE_SOURCE_DIR}/data/win32"
            "${CMAKE_SOURCE_DIR}/src/fldigirc.rc"
            -O coff
            "${FLDIGI_RC_OBJECT}"
        DEPENDS
            "${CMAKE_SOURCE_DIR}/src/fldigirc.rc"
            "${CMAKE_SOURCE_DIR}/src/common.rc"
            "${CMAKE_BINARY_DIR}/src/config.h"
        VERBATIM
        COMMENT "Building Windows resource fldigirc.rc"
    )

    target_sources(fldigi PRIVATE
        src/compat/getsysinfo.c
        src/compat/mingw.c
        "${FLDIGI_RC_OBJECT}"
    )
endif()

target_link_libraries(fldigi PRIVATE
    PkgConfig::PORTAUDIO
    PkgConfig::SNDFILE
    PkgConfig::SAMPLERATE
    PkgConfig::HAMLIB
    PkgConfig::PNG

    ${FLTK_LDFLAGS}

    Threads::Threads
)

if(NOT WIN32)
    target_link_libraries(fldigi PRIVATE
        PkgConfig::PULSEAUDIO
        PkgConfig::UDEV
        ${X11_LIBRARIES}
        m
        dl
    )
endif()

if(WIN32)
    target_link_libraries(fldigi PRIVATE
        intl
        iconv
        wsock32
        ws2_32
        setupapi
        hid
    )

#
# Windows NSIS installer
#
    find_program(MAKENSIS makensis)
    find_program(MSGFMT msgfmt)

    if(MAKENSIS)
        set(INSTALLER_FILE
            "${CMAKE_CURRENT_BINARY_DIR}/fldigi-${FLDIGI_VERSION}_setup.exe"
        )

        set(FLDIGI_LOCALES
            ca
            el
            es
            fr
            it
            pl
            de
            nl
            ru
        )

        set(FLDIGI_MO_FILES)

        if(MSGFMT)
            foreach(lang IN LISTS FLDIGI_LOCALES)
                set(mo_file
                    "${CMAKE_CURRENT_BINARY_DIR}/share/locale/${lang}/LC_MESSAGES/fldigi.mo"
                )

                add_custom_command(
                    OUTPUT "${mo_file}"
                    COMMAND "${CMAKE_COMMAND}" -E make_directory
                        "${CMAKE_CURRENT_BINARY_DIR}/share/locale/${lang}/LC_MESSAGES"
                    COMMAND "${MSGFMT}"
                        -c
                        -o "${mo_file}"
                        "${CMAKE_SOURCE_DIR}/po/${lang}.po"
                    DEPENDS "${CMAKE_SOURCE_DIR}/po/${lang}.po"
                    VERBATIM
                    COMMENT "Building ${lang} message catalog"
                )

                list(APPEND FLDIGI_MO_FILES "${mo_file}")
            endforeach()

            add_custom_target(fldigi-locales
                DEPENDS ${FLDIGI_MO_FILES}
            )
        endif()

        set(NSIS_COMMAND
            "${MAKENSIS}"
            -V2
            -NOCD
            "-DINSTALLER_FILE=${INSTALLER_FILE}"
            "-DLICENSE_FILE=${CMAKE_SOURCE_DIR}/COPYING"
            "-DSUPPORT_URL=https://www.w1hkj.org/Fldigi.html"
            "-DUPDATES_URL=https://www.w1hkj.org/download.html"
            "-DFLDIGI_DOCS_URL=https://www.w1hkj.org/FldigiHelp/index.html"
            "-DGUIDE_URL=https://www.w1hkj.org/beginners.html"
            -DHAVE_FLDIGI
            -DFLDIGI_NAME=Fldigi
            -DFLDIGI_BINARY=fldigi.exe
            "-DFLDIGI_VERSION=${FLDIGI_VERSION}"
        )

        if(MSGFMT)
            list(APPEND NSIS_COMMAND
                "-DFLDIGI_LOCALE_PATH=${CMAKE_CURRENT_BINARY_DIR}/share"
                -DFLDIGI_LOCALE_DIR=locale
            )
        endif()

        list(APPEND NSIS_COMMAND
            "${CMAKE_SOURCE_DIR}/data/win32/fldigi.nsi"
        )

        add_custom_target(nsisinst
            COMMAND ${NSIS_COMMAND}
            DEPENDS fldigi ${FLDIGI_MO_FILES}
            WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
            VERBATIM
            COMMENT "Building ${INSTALLER_FILE}"
        )
    endif()

endif()

target_compile_definitions(fldigi PRIVATE
    HAVE_CONFIG_H
    BUILD_FLDIGI
    LOCALEDIR="${CMAKE_INSTALL_FULL_LOCALEDIR}"
    PKGDATADIR="${CMAKE_INSTALL_FULL_DATADIR}/fldigi"
    NDEBUG
)

target_include_directories(fldigi PRIVATE
    ${CMAKE_BINARY_DIR}/src

    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/include
    ${CMAKE_SOURCE_DIR}/src/irrxml
    ${CMAKE_SOURCE_DIR}/src/libtiniconv
    ${CMAKE_SOURCE_DIR}/src/fileselector
    ${CMAKE_SOURCE_DIR}/src/xmlrpcpp
    ${CMAKE_SOURCE_DIR}/src/mbedtls
)


