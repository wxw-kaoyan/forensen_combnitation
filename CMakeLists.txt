cmake_minimum_required(VERSION 3.19)
project(TASoftware LANGUAGES CXX)

set(Qt6_DIR "B:/qt/6.8.3/mingw_64/lib/cmake/Qt6")
set(Qt6Charts_DIR "B:/qt/6.8.3/mingw_64/lib/cmake/Qt6Charts")
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Network SerialPort Concurrent Charts)

qt_standard_project_setup()

set(CMAKE_AUTOUIC_SEARCH_PATHS ${CMAKE_CURRENT_SOURCE_DIR})

qt_add_executable(TASoftware
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    common/toolprocess.cpp
    common/toolprocess.h
    common/toolprocessmanager.cpp
    common/toolprocessmanager.h
    common/uiinitializer.cpp
    common/uiinitializer.h
    common/tcpconnection.cpp
    common/tcpconnection.h
    common/serialconnection.cpp
    common/serialconnection.h
    modules/ntrip/ntripdialog.cpp
    modules/ntrip/ntripdialog.h
    modules/ntrip/NTRIP.ui
    modules/ntrip/ntripworker.cpp
    modules/ntrip/ntripworker.h
    modules/ntrip/forwardtarget.cpp
    modules/ntrip/forwardtarget.h
    modules/ntrip/forwardertablemanager.cpp
    modules/ntrip/forwardertablemanager.h
    modules/ntrip/ntrippagemanager.cpp
    modules/ntrip/ntrippagemanager.h
    modules/ntrip/datapool/datapool.cpp
    modules/ntrip/datapool/datapool.h
    modules/ntrip/datapool/dataviewerdialog.cpp
    modules/ntrip/datapool/dataviewerdialog.h
    modules/commtool/commtool.cpp
    modules/commtool/commtool.h
    modules/commtool/realtimeparsepanel.cpp
    modules/commtool/realtimeparsepanel.h
    modules/commtool/streamframer.cpp
    modules/commtool/streamframer.h
    modules/commtool/atcmd/atcmdpanel.cpp
    modules/commtool/atcmd/atcmdpanel.h
    modules/commtool/atcmd/atcmdpanel.ui
    modules/commtool/atcmd/atcmdstore.cpp
    modules/commtool/atcmd/atcmdstore.h
    modules/commtool/atcmd/atparamdialog.cpp
    modules/commtool/atcmd/atparamdialog.h
    modules/simulator/simulatorpanel.cpp
    modules/simulator/simulatorpanel.h
    modules/simulator/datasimulator.cpp
    modules/simulator/datasimulator.h
    modules/simulator/ppsgenerator.cpp
    modules/simulator/ppsgenerator.h
    modules/simulator/simprotocolregistry.cpp
    modules/simulator/simprotocolregistry.h
    modules/simulator/simclock.h
    modules/simulator/simprotocol.h
    modules/simulator/encoders/aa55encoder.cpp
    modules/simulator/encoders/aa55encoder.h
    modules/simulator/encoders/nmeaencoder.cpp
    modules/simulator/encoders/nmeaencoder.h
    modules/simulator/encoders/novatelencoder.cpp
    modules/simulator/encoders/novatelencoder.h
    modules/simulator/encoders/bytewriter.h
    modules/simulator/protocols/bestposb.h
    modules/simulator/protocols/gpgga.h
    modules/simulator/protocols/gprmc.h
    modules/simulator/protocols/psrvelb.h
    modules/parse/parser/parserbase.cpp
    modules/parse/parser/parserbase.h
    modules/parse/parser/parserfactory.h
    modules/parse/parser/single/frameprotocolparser.h
    modules/parse/parser/single/protocolregistry.cpp
    modules/parse/parser/single/protocolregistry.h
    modules/parse/parser/debugstream/debugframeparser.cpp
    modules/parse/parser/debugstream/debugframeparser.h
    modules/parse/parser/debugstream/eb614debugparser.cpp
    modules/parse/parser/debugstream/eb614debugparser.h
    modules/parse/parser/external/kaixinframeparser.cpp
    modules/parse/parser/external/kaixinframeparser.h
    modules/parse/parser/can/canrtkparser.h
    modules/parse/parser/other/rtcmframeparser.cpp
    modules/parse/parser/other/rtcmframeparser.h
    modules/parse/parser/core/frameparser.cpp
    modules/parse/parser/core/frameparser.h
    modules/parse/parser/core/protocoldefs.h
    modules/parse/parser/core/canblfparser.cpp
    modules/parse/parser/core/canblfparser.h
    modules/parse/parser/core/rtcmparser.cpp
    modules/parse/parser/core/rtcmparser.h
    modules/parse/parser/core/rtcm_deep_decoder.cpp
    modules/parse/parser/core/rtcm_deep_decoder.h
    common/rtklib/rtkcmn.c
    common/rtklib/rtcm.c
    common/rtklib/rtcm3.c
    common/rtklib/rtcm_stubs.c
    common/rtklib/rinex.c
    common/rtklib/rtklib.h
    modules/parse/parsepanel.cpp
    modules/parse/parsepanel.h
    modules/parse/parsepanel.ui
    modules/parse/columnextract.cpp
    modules/parse/columnextract.h
    modules/parse/downsampledialog.cpp
    modules/parse/downsampledialog.h
    modules/analysis/compare/analysispanel.cpp
    modules/analysis/compare/analysispanel.h
    modules/analysis/compare/comparechartwindow.cpp
    modules/analysis/compare/comparechartwindow.h
    modules/analysis/raw/rawanalysispanel.cpp
    modules/analysis/raw/rawanalysispanel.h
    modules/analysis/raw/rinexreader.cpp
    modules/analysis/raw/rinexreader.h
    modules/analysis/raw/imureader.cpp
    modules/analysis/raw/imureader.h
    modules/analysis/raw/truthreader.cpp
    modules/analysis/raw/truthreader.h
)

# RTKLIB .c files compiled as C++ (no C compiler available in this environment)
# Suppress C++ warnings from legacy C code
set_source_files_properties(
    common/rtklib/rtkcmn.c
    common/rtklib/rtcm.c
    common/rtklib/rtcm3.c
    common/rtklib/rtcm_stubs.c
    common/rtklib/rinex.c
    PROPERTIES
        LANGUAGE CXX
        COMPILE_FLAGS "-Wno-write-strings"
)

target_include_directories(TASoftware
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/common/rtklib
)

# RTKLIB expects WIN32 to be defined on Windows
if (WIN32)
    add_compile_definitions(WIN32)
endif()

target_link_libraries(TASoftware
    PRIVATE
        Qt::Core
        Qt::Widgets
        Qt::Network
        Qt::SerialPort
        Qt::Concurrent
        Qt::Charts
        winmm
)

install(TARGETS TASoftware
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# 构建后将 scripts 目录拷贝到可执行文件目录（Python 解析脚本）
add_custom_command(TARGET TASoftware POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/scripts
        $<TARGET_FILE_DIR:TASoftware>/scripts
    COMMENT "Copying scripts/ to output directory"
)

# 构建后将 at_commands.json 拷贝到可执行文件目录（AT 指令库）
add_custom_command(TARGET TASoftware POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/at_commands.json
        $<TARGET_FILE_DIR:TASoftware>/at_commands.json
    COMMENT "Copying at_commands.json to output directory"
)

# 构建后将 sim_protocols.json 拷贝到可执行文件目录（模拟协议库）
add_custom_command(TARGET TASoftware POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/sim_protocols.json
        $<TARGET_FILE_DIR:TASoftware>/sim_protocols.json
    COMMENT "Copying sim_protocols.json to output directory"
)

qt_generate_deploy_app_script(
    TARGET TASoftware
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
