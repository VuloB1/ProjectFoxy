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

# The settings file: Modo pixel's switch and extras, and the migration from the old single switch.
add_executable(test_app_settings test_app_settings.cpp)
target_link_libraries(test_app_settings PRIVATE imageviewer_app imageviewer_core Qt6::Test)
add_test(NAME test_app_settings COMMAND test_app_settings)

# The real AppController (load / dirty / save rules), with a stand-in decoder.
add_executable(test_app_controller test_app_controller.cpp)
target_link_libraries(test_app_controller PRIVATE imageviewer_app imageviewer_core Qt6::Test)
target_compile_definitions(test_app_controller PRIVATE FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
add_test(NAME test_app_controller COMMAND test_app_controller)

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

# The language of the interface and its catalogues (resources/i18n).
add_executable(test_translations test_translations.cpp)
target_link_libraries(test_translations PRIVATE imageviewer_app imageviewer_core Qt6::Test)
target_compile_definitions(test_translations PRIVATE I18N_DIR="${CMAKE_SOURCE_DIR}/resources/i18n")
add_test(NAME test_translations COMMAND test_translations)
