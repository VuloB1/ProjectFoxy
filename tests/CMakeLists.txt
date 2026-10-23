find_package(Qt6 REQUIRED COMPONENTS Test)

# Each QTEST_MAIN test file defines its own main() - one executable per file.
function(add_core_test name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE imageviewer_core Qt6::Test)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

add_core_test(test_decoder_registry)
target_compile_definitions(test_decoder_registry PRIVATE
    FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
add_core_test(test_edit_stack)
add_core_test(test_comic_book)
add_core_test(test_update_check)
add_core_test(test_festivity)
add_core_test(test_text_paragraphs)
add_core_test(test_cas)
add_core_test(test_effect_families)
add_core_test(test_frame)
add_core_test(test_collage)
add_core_test(test_anim)
target_link_libraries(test_anim PRIVATE WebP::webpdemux)
add_core_test(test_lens)
target_compile_definitions(test_lens PRIVATE
    LENSFUN_DIR="${CMAKE_SOURCE_DIR}/resources/lensfun")
add_core_test(test_color_management)
add_core_test(test_image_loader)
add_core_test(test_thumbnail_cache)
add_core_test(test_image_writer)
target_compile_definitions(test_image_writer PRIVATE
    FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
add_core_test(test_animated_decoder)
target_compile_definitions(test_animated_decoder PRIVATE
    FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")

# The folder list and the "rename chosen files" rules.
add_executable(test_folder_model test_folder_model.cpp)
target_link_libraries(test_folder_model PRIVATE imageviewer_app imageviewer_core Qt6::Test)
add_test(NAME test_folder_model COMMAND test_folder_model)

# The pictures of the "Varias imágenes" view.
add_executable(test_color_picker test_color_picker.cpp)
target_link_libraries(test_color_picker PRIVATE imageviewer_app imageviewer_core Qt6::Test)
add_test(NAME test_color_picker COMMAND test_color_picker)

add_executable(test_pane_images test_pane_images.cpp)
target_link_libraries(test_pane_images PRIVATE imageviewer_app imageviewer_core Qt6::Test)
target_compile_definitions(test_pane_images PRIVATE FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
add_test(NAME test_pane_images COMMAND test_pane_images)

# The animation studio (Crear > GIF animado).
add_executable(test_anim_studio test_anim_studio.cpp)
target_link_libraries(test_anim_studio PRIVATE imageviewer_app imageviewer_core Qt6::Test)
add_test(NAME test_anim_studio COMMAND test_anim_studio)

# The collage maker's controller (Crear > Collage).
add_executable(test_collage_studio test_collage_studio.cpp)
target_link_libraries(test_collage_studio PRIVATE imageviewer_app imageviewer_core Qt6::Test)
add_test(NAME test_collage_studio COMMAND test_collage_studio)

# Saving on its own thread, and the open/paste requests that wait for a save.
add_executable(test_save_coordinator test_save_coordinator.cpp)
target_link_libraries(test_save_coordinator PRIVATE imageviewer_app imageviewer_core Qt6::Test)
add_test(NAME test_save_coordinator COMMAND test_save_coordinator)

# The effect preview (Efectos): the picked effect, its sliders and the calculations behind them.
add_executable(test_effect_preview test_effect_preview.cpp)
target_link_libraries(test_effect_preview PRIVATE imageviewer_app imageviewer_core Qt6::Test)
add_test(NAME test_effect_preview COMMAND test_effect_preview)

# Playback of an animated picture in the viewer.
add_executable(test_animation_playback test_animation_playback.cpp)
target_link_libraries(test_animation_playback PRIVATE imageviewer_app imageviewer_core Qt6::Test)
add_test(NAME test_animation_playback COMMAND test_animation_playback)

# The picture provider and its display proxy (a reduced copy for the canvas).
add_executable(test_image_provider test_image_provider.cpp)
target_link_libraries(test_image_provider PRIVATE imageviewer_app imageviewer_core Qt6::Test)
add_test(NAME test_image_provider COMMAND test_image_provider)

# The secondary tools' workers: destroying their owner mid-work.
add_executable(test_worker_lifetime test_worker_lifetime.cpp)
target_link_libraries(test_worker_lifetime PRIVATE imageviewer_app imageviewer_core Qt6::Test)
add_test(NAME test_worker_lifetime COMMAND test_worker_lifetime)

# The settings file: Modo pixel's switch and extras, and the migration from the old single switch.
add_executable(test_app_settings test_app_settings.cpp)
target_link_libraries(test_app_settings PRIVATE imageviewer_app imageviewer_core Qt6::Test)
add_test(NAME test_app_settings COMMAND test_app_settings)

# The real AppController (load / dirty / save rules), with a stand-in decoder.
add_executable(test_app_controller test_app_controller.cpp)
target_link_libraries(test_app_controller PRIVATE imageviewer_app imageviewer_core Qt6::Test)
target_compile_definitions(test_app_controller PRIVATE FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
add_test(NAME test_app_controller COMMAND test_app_controller)

# One window for the program: a second launch hands its file to the first.
add_executable(test_single_instance test_single_instance.cpp)
target_link_libraries(test_single_instance PRIVATE imageviewer_app imageviewer_core Qt6::Test)
add_test(NAME test_single_instance COMMAND test_single_instance)

# What is derived from the picture and cached: structural result, histograms, small copy.
add_executable(test_picture_cache test_picture_cache.cpp)
target_link_libraries(test_picture_cache PRIVATE imageviewer_app imageviewer_core Qt6::Test)
add_test(NAME test_picture_cache COMMAND test_picture_cache)

# The Ajustes and Filtros vocabulary: sliders, levels, curves and looks, as plain functions.
add_executable(test_adjust_model test_adjust_model.cpp)
target_link_libraries(test_adjust_model PRIVATE imageviewer_app imageviewer_core Qt6::Test)
add_test(NAME test_adjust_model COMMAND test_adjust_model)

# The live edits (Ajustes, Filtros), the cached derived pictures and the order of the signals QML listens to.
add_executable(test_app_controller_edits test_app_controller_edits.cpp)
target_link_libraries(test_app_controller_edits PRIVATE imageviewer_app imageviewer_core Qt6::Test)
add_test(NAME test_app_controller_edits COMMAND test_app_controller_edits)

# CPU/GPU parity: the preview shaders (Direct3D 11) against what gets written to
# disk. Skips itself when there is no Direct3D 11 device.
add_executable(test_gpu_parity test_gpu_parity.cpp)
target_link_libraries(test_gpu_parity PRIVATE imageviewer_app imageviewer_core Qt6::Test Qt6::Quick)
target_compile_definitions(test_gpu_parity PRIVATE
    PARITY_SCENE="${CMAKE_CURRENT_SOURCE_DIR}/gpu_parity/ParityScene.qml")
# The same shader sources the application compiles (see the root CMakeLists.txt).
qt_add_shaders(test_gpu_parity "parity_shaders"
    PREFIX "/shaders"
    GLSL "150,300es"
    FILES
        ${CMAKE_SOURCE_DIR}/qml/shaders/Grade.frag
        ${CMAKE_SOURCE_DIR}/qml/shaders/Detail.frag
    OUTPUTS
        Grade.frag.qsb
        Detail.frag.qsb
)
add_test(NAME test_gpu_parity COMMAND test_gpu_parity)

# Startup smoke test of the real application (see smoke_startup.cmake).
add_test(NAME smoke_startup
    COMMAND ${CMAKE_COMMAND}
        -DEXE=$<TARGET_FILE:ImageViewer>
        -DIMAGE=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/exif_orient_6.jpg
        -DQT_PLUGINS=${QT6_INSTALL_PREFIX}/${QT6_INSTALL_PLUGINS}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/smoke_startup.cmake)

# Developer tool, not a test: wall and CPU time of every effect (bench_effects [longSide] [id...]).
add_executable(bench_effects bench_effects.cpp)
target_link_libraries(bench_effects PRIVATE imageviewer_core Qt6::Core)
target_include_directories(bench_effects PRIVATE ${CMAKE_SOURCE_DIR}/src/core)

# Developer tool: time of each stage of opening a picture (bench_load file...).
add_executable(bench_load bench_load.cpp)
target_link_libraries(bench_load PRIVATE imageviewer_core Qt6::Core)
target_include_directories(bench_load PRIVATE ${CMAKE_SOURCE_DIR}/src/core)

# The language of the interface and its catalogues (resources/i18n).
add_executable(test_translations test_translations.cpp)
target_link_libraries(test_translations PRIVATE imageviewer_app imageviewer_core Qt6::Test)
target_compile_definitions(test_translations PRIVATE I18N_DIR="${CMAKE_SOURCE_DIR}/resources/i18n")
add_test(NAME test_translations COMMAND test_translations)

# Qt Multimedia (video to GIF): its FFmpeg backend must load and know the usual video formats. Needs the plugin folder.
add_executable(test_multimedia test_multimedia.cpp)
target_include_directories(test_multimedia PRIVATE ${CMAKE_SOURCE_DIR}/src/app)
target_link_libraries(test_multimedia PRIVATE imageviewer_app Qt6::Multimedia Qt6::Test)
add_test(NAME test_multimedia COMMAND test_multimedia)
set_tests_properties(test_multimedia PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_PLUGIN_PATH=${QT6_INSTALL_PREFIX}/${QT6_INSTALL_PLUGINS};QT_FORCE_STDERR_LOGGING=1")

# Frame-exact reading of a video by time (the base of "video to GIF").
add_executable(test_video_frames test_video_frames.cpp)
target_include_directories(test_video_frames PRIVATE ${CMAKE_SOURCE_DIR}/src/app)
target_link_libraries(test_video_frames PRIVATE imageviewer_app Qt6::Multimedia Qt6::Test)
target_compile_definitions(test_video_frames PRIVATE FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
add_test(NAME test_video_frames COMMAND test_video_frames)
set_tests_properties(test_video_frames PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_PLUGIN_PATH=${QT6_INSTALL_PREFIX}/${QT6_INSTALL_PLUGINS};QT_FORCE_STDERR_LOGGING=1"
    TIMEOUT 120)

# "Desde un vídeo" (VideoToGif): the frames of the pieces that are kept, cropped and scaled.
add_executable(test_video_to_gif test_video_to_gif.cpp)
target_include_directories(test_video_to_gif PRIVATE ${CMAKE_SOURCE_DIR}/src/app)
target_link_libraries(test_video_to_gif PRIVATE imageviewer_app Qt6::Multimedia Qt6::Test)
target_compile_definitions(test_video_to_gif PRIVATE FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
add_test(NAME test_video_to_gif COMMAND test_video_to_gif)
set_tests_properties(test_video_to_gif PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_PLUGIN_PATH=${QT6_INSTALL_PREFIX}/${QT6_INSTALL_PLUGINS};QT_FORCE_STDERR_LOGGING=1"
    TIMEOUT 120)
