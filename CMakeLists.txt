cmake_minimum_required(VERSION 3.21)
project(ImageViewer VERSION 0.1.0 LANGUAGES CXX)
if(WIN32)
    enable_language(RC)   # the executable carries its icon as a resource (resources/app.rc)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

if(POLICY CMP0177)
    cmake_policy(SET CMP0177 NEW)
endif()
# file(GET_RUNTIME_DEPENDENCIES) in the install rules below: match normalized paths.
if(POLICY CMP0207)
    cmake_policy(SET CMP0207 NEW)
endif()

# ---------------------------------------------------------------------------
# Dependencies
# ---------------------------------------------------------------------------
find_package(Qt6 6.6 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 QuickDialogs2 Svg ShaderTools)

# Qt is the one dependency vcpkg does not pin (it comes from the Qt online
# installer / aqtinstall), so the exact version the project is validated with is
# written down here. 6.6 stays the declared minimum; the windows-release preset
# turns the check into an error so a shipped build is always the validated Qt.
set(IMAGEVIEWER_VALIDATED_QT 6.7.3)
option(IMAGEVIEWER_REQUIRE_VALIDATED_QT
    "Fail (instead of warn) when Qt is not exactly IMAGEVIEWER_VALIDATED_QT" OFF)
if(NOT Qt6_VERSION VERSION_EQUAL IMAGEVIEWER_VALIDATED_QT)
    set(_iv_qt_msg
        "Qt ${Qt6_VERSION} found, but the project is validated with Qt ${IMAGEVIEWER_VALIDATED_QT}.")
    if(IMAGEVIEWER_REQUIRE_VALIDATED_QT)
        message(FATAL_ERROR "${_iv_qt_msg} Point CMAKE_PREFIX_PATH at Qt ${IMAGEVIEWER_VALIDATED_QT} "
            "(or configure with -DIMAGEVIEWER_REQUIRE_VALIDATED_QT=OFF).")
    else()
        message(WARNING "${_iv_qt_msg}")
    endif()
endif()

# Windows has no system pkg-config, so vcpkg.json pulls in "pkgconf" as a
# host tool. FindPkgConfig.cmake only looks for a program literally named
# "pkg-config" by default, so point it at the pkgconf vcpkg just built
# before asking it to find anything.
if(NOT PKG_CONFIG_EXECUTABLE AND DEFINED VCPKG_INSTALLED_DIR)
    find_program(PKG_CONFIG_EXECUTABLE
        NAMES pkgconf pkg-config
        PATHS "${VCPKG_INSTALLED_DIR}/${VCPKG_HOST_TRIPLET}/tools/pkgconf"
        NO_DEFAULT_PATH
    )
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(VIPS REQUIRED IMPORTED_TARGET vips-cpp)
# libraw_r (not plain libraw): the reentrant/thread-safe build, matching
# IImageDecoder's "must be safe to call from a worker thread" contract -
# ImageLoader's pool can run several decodes concurrently (current image +
# preloadNeighbors), and plain libraw shares global state across instances.
pkg_check_modules(LIBRAW REQUIRED IMPORTED_TARGET libraw_r)
pkg_check_modules(LIBHEIF REQUIRED IMPORTED_TARGET libheif)
pkg_check_modules(EXIV2 REQUIRED IMPORTED_TARGET exiv2)
# The animated WebP writer (libwebp's animation encoder lives in libwebpmux); both DLLs already ship with libvips.
find_package(WebP CONFIG REQUIRED)

# The shipped folder is flat on Windows (ProjectFoxy.exe next to its DLLs), so
# `cmake --install` puts the executable at the prefix root instead of in bin/.
# Must be set before qt_standard_project_setup() pulls in GNUInstallDirs.
set(CMAKE_INSTALL_BINDIR ".")

qt_standard_project_setup(REQUIRES 6.6)

# ---------------------------------------------------------------------------
# Core engine (static lib, no Qt-UI dependency beyond Core/Gui)
# ---------------------------------------------------------------------------
add_subdirectory(src/core)

# ---------------------------------------------------------------------------
# Application executable + QML module
# ---------------------------------------------------------------------------
# Everything the window is made of except main(): a library of its own so the tests
# can build the real AppController (load/save/dirty rules live there), not a copy.
add_library(imageviewer_app STATIC
    src/app/AppController.h
    src/app/AppController.cpp
    src/app/ThemeManager.h
    src/app/ThemeManager.cpp
    src/app/AppPaths.h
    src/app/AppSettings.h
    src/app/AppSettings.cpp
    src/app/ImageProvider.h
    src/app/ImageProvider.cpp
    src/app/FolderModel.h
    src/app/FolderModel.cpp
    src/app/ThumbnailImageProvider.h
    src/app/ThumbnailImageProvider.cpp
    src/app/BatchExporter.h
    src/app/BatchExporter.cpp
    src/app/LensController.h
    src/app/LensController.cpp
    src/app/PaneImageProvider.h
    src/app/PaneImageProvider.cpp
    src/app/AnimStudio.h
    src/app/AnimStudio.cpp
    src/app/CollageStudio.h
    src/app/CollageStudio.cpp
)

target_include_directories(imageviewer_app PUBLIC src/app)

target_link_libraries(imageviewer_app PUBLIC
    imageviewer_core
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Shell32
    User32
)

qt_add_executable(ImageViewer
    src/app/main.cpp
)

qt_add_qml_module(ImageViewer
    URI ImageViewerApp
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/ImageCanvas.qml
        qml/ThumbnailStrip.qml
        qml/Toolbar.qml
        qml/FloatingToolbar.qml
        qml/EditPanel.qml
        qml/CropTool.qml
        qml/ResizeTool.qml
        qml/AdjustTool.qml
        qml/FilterTool.qml
        qml/EffectsTool.qml
        qml/ScrubNumberField.qml
        qml/AdjustRow.qml
        qml/AdjustSection.qml
        qml/HistogramView.qml
        qml/ChannelSelector.qml
        qml/LevelsEditor.qml
        qml/CurvesEditor.qml
        qml/LookCell.qml
        qml/EffectParamRow.qml
        qml/EffectCaption.qml
        qml/EffectHandles.qml
        qml/LensTool.qml
        qml/FramePanel.qml
        qml/PaneView.qml
        qml/MultiView.qml
        qml/GifStudio.qml
        qml/CollageMaker.qml
        qml/FrameCornerPicker.qml
        qml/UnsavedChangesDialog.qml
        qml/OverwriteDialog.qml
        qml/SettingsDialog.qml
        qml/ConfirmDialog.qml
        qml/BatchFileList.qml
        qml/BatchExportDialog.qml
        qml/BatchRenameDialog.qml
        qml/controls/Win98Bevel.qml
        qml/controls/AppIcon.qml
        qml/controls/AppToolTip.qml
        qml/controls/AppHintBox.qml
        qml/controls/AppButton.qml
        qml/controls/AppToolButton.qml
        qml/controls/AppSlider.qml
        qml/controls/AppCheckBox.qml
        qml/controls/AppRadioButton.qml
        qml/controls/AppMenu.qml
        qml/controls/AppMenuItem.qml
        qml/controls/AppMenuSeparator.qml
        qml/controls/AppDialog.qml
        qml/controls/AppComboBox.qml
        qml/controls/AppColorSwatch.qml
        qml/controls/AppSpinBox.qml
        qml/controls/AppTextField.qml
        qml/controls/AppProgressBar.qml
        qml/controls/AppLoadingPill.qml
        qml/controls/AppSwitch.qml
        qml/controls/AppPanelBackground.qml
        qml/controls/AppTabBar.qml
        qml/controls/AppTabButton.qml
        qml/controls/AppScrollBar.qml
        qml/controls/AppWheelScroll.qml
    RESOURCES
        resources/icons/icon.png
)

# The Lensfun lens database (resources/lensfun, CC BY-SA 3.0 - see docs/LICENCIAS.md), compiled into the
# program as Qt resources under :/lensfun so the lens correction works wherever the .exe is copied.
file(GLOB _iv_lensfun_files CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/resources/lensfun/*.xml"
                                              "${CMAKE_SOURCE_DIR}/resources/lensfun/timestamp.txt")
qt_add_resources(ImageViewer "lensfun_db"
    PREFIX "/lensfun"
    BASE "${CMAKE_SOURCE_DIR}/resources/lensfun"
    FILES ${_iv_lensfun_files}
)

target_link_libraries(ImageViewer PRIVATE
    imageviewer_app
    imageviewer_core
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::QuickDialogs2
    Qt6::Svg
    Shell32
    User32
)

# Custom fragment shaders for the live GPU preview of the edits
# (Grade.frag = tone/color grading, used for both the Filtros look and the
# Ajustes sliders; Detail.frag = clarity + sharpen + vignette + grain) -
# compiled by qsb into .qsb bytecode bundled as Qt resources.
#
# GLSL "150,300es": the film grain hashes pixel coordinates with unsigned
# integer math (so CPU and GPU produce the very same noise), which the default
# legacy targets (GLSL ES 100 / GLSL 120) cannot express. The app renders with
# Direct3D 11 anyway; the OpenGL variants only matter if QSG_RHI_BACKEND is
# overridden.
qt_add_shaders(ImageViewer "imageviewer_shaders"
    PREFIX "/shaders"
    GLSL "150,300es"
    FILES
        qml/shaders/Grade.frag
        qml/shaders/Detail.frag
        qml/shaders/Checker.frag
        qml/shaders/PixelGrid.frag
    OUTPUTS
        Grade.frag.qsb
        Detail.frag.qsb
        Checker.frag.qsb
        PixelGrid.frag.qsb
)

# `import QtQuick.Effects` (MultiEffect, used for the floating toolbar's drop
# shadow) can't be statically linked like the other QML
# modules above: this Qt SDK installation ships effectsplugin.dll without
# its import library (.lib), which is a hard requirement for static linking
# - qt6_import_qml_plugins()/direct linking both fail at link time trying to
# use the bare .dll as if it were one. Load it dynamically instead, the way
# Qt loads any other runtime plugin (imageformats, platforms, ...): copy its
# files next to the exe, under qml/QtQuick/Effects/, which is already one of
# QQmlEngine's default import search paths relative to the app.
#
# This copy only serves the build tree (running ProjectFoxy.exe straight from
# the build folder). The shipped folder is produced by `cmake --install`, where
# windeployqt finds the plugin by scanning the QML imports (see "Installation"
# below). The SDK paths come from the QT6_INSTALL_* variables Qt itself defines
# instead of being hand-built with "../../..".
set(_iv_qt_qml_dir "${QT6_INSTALL_PREFIX}/${QT6_INSTALL_QML}")
set(_iv_qt_bin_dir "${QT6_INSTALL_PREFIX}/${QT6_INSTALL_BINS}")
add_custom_command(TARGET ImageViewer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:ImageViewer>/qml/QtQuick/Effects"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${_iv_qt_qml_dir}/QtQuick/Effects/qmldir"
        "${_iv_qt_qml_dir}/QtQuick/Effects/plugins.qmltypes"
        "${_iv_qt_qml_dir}/QtQuick/Effects/$<IF:$<CONFIG:Debug>,effectsplugind.dll,effectsplugin.dll>"
        "$<TARGET_FILE_DIR:ImageViewer>/qml/QtQuick/Effects/"
    # effectsplugin.dll itself depends on Qt6QuickEffects.dll, which - unlike
    # every other Qt6*.dll here - never gets copied automatically since
    # nothing links against it as a proper CMake target (see above).
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${_iv_qt_bin_dir}/$<IF:$<CONFIG:Debug>,Qt6QuickEffectsd.dll,Qt6QuickEffects.dll>"
        "$<TARGET_FILE_DIR:ImageViewer>/"
    COMMENT "Deploying QtQuick.Effects QML plugin into the build tree"
)

set_target_properties(ImageViewer PROPERTIES
    WIN32_EXECUTABLE TRUE
    OUTPUT_NAME "ProjectFoxy"
)
if(WIN32)
    target_sources(ImageViewer PRIVATE resources/app.rc)
endif()

# ---------------------------------------------------------------------------
# Installation
#
#   cmake --install <build dir> --prefix <folder>
#
# produces the complete folder to ship (and the one an installer should pack):
#   ProjectFoxy.exe, the Qt runtime + plugins + QML modules (windeployqt, driven
#   by Qt's deploy script, which also finds QtQuick.Effects by scanning the QML),
#   the vcpkg DLLs the program really needs (libvips, LibRaw, libheif, exiv2 and
#   their dependencies - resolved from the executable's import tables, not by
#   copying the whole vcpkg bin folder), and licenses/.
#
# Every native library is a DLL (vcpkg triplet x64-windows, pinned in
# CMakePresets.json): that is what lets the LGPL libraries be replaced by the
# user. Do not switch to a static triplet without redoing the licence analysis
# in docs/LICENCIAS.md.
# ---------------------------------------------------------------------------
if(WIN32)
    install(TARGETS ImageViewer RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

    # Qt derives its deploy destinations from the GNUInstallDirs values it saw
    # when it was found (bin/), before CMAKE_INSTALL_BINDIR was overridden above;
    # state the flat layout explicitly so the Qt DLLs land next to the exe.
    install(CODE "set(QT_DEPLOY_BIN_DIR \".\")")

    # The C/C++ runtime is installed by CMake itself (below) instead of by
    # windeployqt, which finds the Visual Studio redistributables through
    # vswhere.exe and silently skips them when it is not on PATH.
    qt_generate_deploy_qml_app_script(
        TARGET ImageViewer
        OUTPUT_SCRIPT imageviewer_qt_deploy_script
        NO_TRANSLATIONS
        NO_COMPILER_RUNTIME
        NO_UNSUPPORTED_PLATFORM_ERROR
        # The renderer is Direct3D 11 (see main.cpp); the 20 MB software OpenGL
        # fallback is only used if the user forces QT_OPENGL=software.
        DEPLOY_TOOL_OPTIONS --no-opengl-sw
    )
    install(SCRIPT ${imageviewer_qt_deploy_script})

    set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION ".")
    set(CMAKE_INSTALL_UCRT_LIBRARIES FALSE)   # the Universal CRT is part of Windows 10/11
    include(InstallRequiredSystemLibraries)

    if(DEFINED VCPKG_INSTALLED_DIR AND DEFINED VCPKG_TARGET_TRIPLET)
        if(CMAKE_BUILD_TYPE STREQUAL "Debug")
            set(_iv_vcpkg_bin "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/debug/bin")
        else()
            set(_iv_vcpkg_bin "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin")
        endif()
        install(CODE "set(IV_EXE \"$<TARGET_FILE:ImageViewer>\")
set(IV_VCPKG_BIN \"${_iv_vcpkg_bin}\")")
        install(CODE [[
            if(POLICY CMP0207)
                cmake_policy(SET CMP0207 NEW)
            endif()
            file(GET_RUNTIME_DEPENDENCIES
                EXECUTABLES "${IV_EXE}"
                RESOLVED_DEPENDENCIES_VAR iv_resolved
                UNRESOLVED_DEPENDENCIES_VAR iv_unresolved
                DIRECTORIES "${IV_VCPKG_BIN}"
                PRE_EXCLUDE_REGEXES "api-ms-.*" "ext-ms-.*"
                # Windows' own DLLs: neither shipped nor worth analysing.
                POST_EXCLUDE_REGEXES ".*[/\\\\][Ww][Ii][Nn][Dd][Oo][Ww][Ss][/\\\\].*")
            # The analysis finds each DLL first next to the executable (vcpkg's
            # applocal step copied them there), so match by file name and take the
            # copy from the vcpkg tree. Qt's DLLs are not in it: windeployqt owns them.
            foreach(dll IN LISTS iv_resolved)
                cmake_path(GET dll FILENAME iv_name)
                if(EXISTS "${IV_VCPKG_BIN}/${iv_name}")
                    file(INSTALL "${IV_VCPKG_BIN}/${iv_name}" DESTINATION "${CMAKE_INSTALL_PREFIX}")
                endif()
            endforeach()
            # Qt's own DLLs and the C/C++ runtime are deployed by windeployqt above;
            # anything else left unresolved is a real problem worth seeing.
            foreach(dll IN LISTS iv_unresolved)
                if(NOT dll MATCHES "^(Qt6|VCRUNTIME|MSVCP|vcruntime|msvcp|concrt)")
                    message(WARNING "Unresolved runtime dependency: ${dll}")
                endif()
            endforeach()
        ]])
    else()
        message(WARNING "Not using the vcpkg toolchain: `cmake --install` will not copy the native libraries")
    endif()

    install(FILES docs/THIRD_PARTY_NOTICES.txt DESTINATION licenses)
    install(FILES LICENSE DESTINATION licenses RENAME LICENSE-ProjectFoxy.txt)
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
enable_testing()
add_subdirectory(tests)
