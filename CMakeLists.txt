cmake_minimum_required(VERSION 3.16)
project(SystemInfoPet VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# 这两个宏必须全局定义：
#   _WIN32_WINNT / NTDDI_VERSION 决定 GetIfTable2、MIB_IF_ROW2 是否可见
add_compile_definitions(_WIN32_WINNT=0x0A00 NTDDI_VERSION=0x0A000006)

# ---------------------------------------------------------------------------
# 中文路径兼容（很重要，别删）
#
# 本工程位于 E:\上位机\系统信息，路径含中文。
# 实测：g++ 读写中文路径没问题，CMake/Ninja 也没问题，
#       但 Qt 的 moc.exe【无法把生成文件写到含中文的路径】，
#       会报 "Cannot create ... Error: Permission denied"。
#
# 对策：把 AUTOGEN 的产物（moc_*.cpp、mocs_compilation.cpp）挪到纯 ASCII 目录。
#       moc 读中文路径的源文件是正常的，只有输出路径有限制。
# ---------------------------------------------------------------------------
if (WIN32 AND NOT CMAKE_AUTOGEN_BUILD_DIR)
    if (DEFINED ENV{TEMP})
        file(TO_CMAKE_PATH "$ENV{TEMP}/SystemInfoPet_autogen" _sip_autogen_dir)
    else()
        file(TO_CMAKE_PATH "$ENV{SystemDrive}/SystemInfoPet_autogen" _sip_autogen_dir)
    endif()
    set(CMAKE_AUTOGEN_BUILD_DIR "${_sip_autogen_dir}")
    message(STATUS "AUTOGEN 输出目录（规避 moc 中文路径限制）: ${CMAKE_AUTOGEN_BUILD_DIR}")
endif()

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets)

set(SOURCES
    src/main.cpp

    src/core/WinSource.cpp
    src/core/GpuSource.cpp
    src/core/NetSource.cpp
    src/core/DiskSource.cpp
    src/core/FreqSource.cpp
    src/core/DwmSource.cpp
    src/core/HwinfoSource.cpp
    src/core/MetricsHub.cpp
    src/core/SerialSink.cpp

    src/pet/PetState.cpp
    src/pet/PetRenderer.cpp
    src/pet/PetWindow.cpp

    src/ui/RingPanel.cpp
    src/ui/TrayIcon.cpp
)

set(HEADERS
    src/core/Metrics.h
    src/core/IDataSource.h
    src/core/WinSource.h
    src/core/GpuSource.h
    src/core/NetSource.h
    src/core/DiskSource.h
    src/core/FreqSource.h
    src/core/DwmSource.h
    src/core/HwinfoSource.h
    src/core/MetricsHub.h
    src/core/SerialSink.h

    src/pet/PetState.h
    src/pet/PetRenderer.h
    src/pet/PetWindow.h

    src/ui/RingPanel.h
    src/ui/TrayIcon.h
)

add_executable(SystemInfoPet WIN32 ${SOURCES} ${HEADERS})

# 上面那个变量在某些 CMake 版本下不会传导到目标，这里再用目标属性兜一层
if (_sip_autogen_dir)
    set_target_properties(SystemInfoPet PROPERTIES AUTOGEN_BUILD_DIR "${_sip_autogen_dir}")
endif()

target_include_directories(SystemInfoPet PRIVATE src)

target_link_libraries(SystemInfoPet PRIVATE
    Qt6::Widgets
    pdh          # GPU Engine / CPU 频率 性能计数器
    iphlpapi     # GetIfTable2
    psapi        # GetProcessMemoryInfo
    dwmapi       # DwmGetCompositionTimingInfo（FPS）
)

# 输出到 bin 目录，方便 windeployqt 打包
set_target_properties(SystemInfoPet PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
