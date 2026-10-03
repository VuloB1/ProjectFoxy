# Startup smoke test: starts the real application on a photo with no visible window
# (Qt's "offscreen" platform, software rendering), lets it run for a few seconds and
# fails if it dies or writes ANY message to its error output.
#
# That is exactly the class of problem the unit tests cannot see: a QML file that
# fails to load, a binding loop, a bad resource path, a property assigned to the
# wrong type. Before this test existed, three such warnings sat in the log for weeks.
#
# Usage:  cmake -DEXE=<ImageViewer.exe> -DIMAGE=<picture> -DQT_PLUGINS=<qt plugins dir>
#               -P smoke_startup.cmake

foreach(var EXE IMAGE QT_PLUGINS)
    if(NOT DEFINED ${var})
        message(FATAL_ERROR "smoke_startup.cmake: -D${var}=... is required")
    endif()
endforeach()

set(ENV{QT_QPA_PLATFORM} "offscreen")
set(ENV{QT_PLUGIN_PATH} "${QT_PLUGINS}")      # the offscreen platform plugin is not deployed with the app
set(ENV{QSG_RHI_BACKEND} "software")          # no GPU needed (works on a build server too)
set(ENV{QT_FORCE_STDERR_LOGGING} "1")         # a GUI (WIN32) app otherwise hides Qt's messages

execute_process(
    COMMAND "${EXE}" "${IMAGE}"
    TIMEOUT 6
    RESULT_VARIABLE result
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err)

# Healthy = still running when the timeout killed it.
if(NOT result MATCHES "timeout")
    message(FATAL_ERROR "The application did not stay alive (result: ${result}).\n${err}")
endif()

# The offscreen platform has no fonts directory of its own and says so; that is the
# platform, not the application. Everything else counts.
string(REGEX REPLACE "QFontDatabase: Cannot find font directory[^\n]*\n?" "" err "${err}")
string(REGEX REPLACE "Note that Qt no longer ships fonts[^\n]*\n?" "" err "${err}")
string(STRIP "${err}" err)
if(NOT err STREQUAL "")
    message(FATAL_ERROR "Unexpected messages while starting up:\n${err}")
endif()

message(STATUS "Started, stayed alive for 6 s and wrote nothing to its error output.")
