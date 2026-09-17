cmake_minimum_required(VERSION 3.20)
project(CTR-Native C)
include(CheckCCompilerFlag)

if(IOS OR APPLE)
    enable_language(OBJC)
endif()

set(CTR_NATIVE_VERSION "0.1.1")
option(CTR_NATIVE_MACOS_BUNDLE "Build CTR Native as a macOS application bundle" OFF)
set(CTR_NATIVE_SOURCE_COMMIT "" CACHE STRING
    "Source commit for a renamed corresponding-source tree without .git"
)
set(CTR_NATIVE_IOS_BUNDLE_IDENTIFIER "io.github.chrissotraidis.ctrpad" CACHE STRING
    "Reverse-DNS bundle identifier for iOS and iPadOS builds"
)

set(CTR_NATIVE_RENDERER_GLES_DEFAULT OFF)
if(ANDROID OR IOS)
    set(CTR_NATIVE_RENDERER_GLES_DEFAULT ON)
endif()
option(CTR_NATIVE_RENDERER_GLES
    "Build the native renderer for an OpenGL ES 3.0 context"
    ${CTR_NATIVE_RENDERER_GLES_DEFAULT}
)

if(CTR_NATIVE_MACOS_BUNDLE AND NOT APPLE)
    message(FATAL_ERROR "CTR_NATIVE_MACOS_BUNDLE is supported only on Apple platforms.")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4 AND NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR
        "CTR Native requires a target with 32-bit or 64-bit pointers; "
        "CMAKE_SIZEOF_VOID_P=${CMAKE_SIZEOF_VOID_P} is unsupported."
    )
endif()
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(STATUS "CTR Native: enabling the checked 64-bit memory model")
endif()

set(CTR_NATIVE_MSVC_FRONTEND OFF)
if(CMAKE_C_COMPILER_ID STREQUAL "MSVC" OR CMAKE_C_SIMULATE_ID STREQUAL "MSVC")
    set(CTR_NATIVE_MSVC_FRONTEND ON)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" CTR_NATIVE_SYSTEM_PROCESSOR_LOWER)
set(CTR_NATIVE_X86_TARGET OFF)
if(CTR_NATIVE_SYSTEM_PROCESSOR_LOWER MATCHES "^(x86|i[3-6]86|amd64|x86_64)$"
   OR CMAKE_GENERATOR_PLATFORM MATCHES "^(Win32|x64)$")
    set(CTR_NATIVE_X86_TARGET ON)
endif()

set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE NEVER)

set(CTR_NATIVE_SOURCE_COMMIT_SHORT "")
if(CTR_NATIVE_SOURCE_COMMIT)
    string(TOLOWER "${CTR_NATIVE_SOURCE_COMMIT}" CTR_NATIVE_SOURCE_COMMIT_NORMALIZED)
    string(LENGTH "${CTR_NATIVE_SOURCE_COMMIT_NORMALIZED}" CTR_NATIVE_SOURCE_COMMIT_LENGTH)
    if(NOT CTR_NATIVE_SOURCE_COMMIT_NORMALIZED MATCHES "^[0-9a-f]+$"
       OR (NOT CTR_NATIVE_SOURCE_COMMIT_LENGTH EQUAL 12
           AND NOT CTR_NATIVE_SOURCE_COMMIT_LENGTH EQUAL 40))
        message(FATAL_ERROR
            "CTR_NATIVE_SOURCE_COMMIT must contain exactly 12 or 40 hexadecimal characters."
        )
    endif()
    string(SUBSTRING "${CTR_NATIVE_SOURCE_COMMIT_NORMALIZED}" 0 12
        CTR_NATIVE_SOURCE_COMMIT_SHORT)
endif()

execute_process(
    COMMAND git -C "${CMAKE_SOURCE_DIR}" rev-parse --show-toplevel
    OUTPUT_VARIABLE CTR_NATIVE_GIT_TOPLEVEL
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE CTR_NATIVE_GIT_TOPLEVEL_RESULT
    ERROR_QUIET
)
get_filename_component(CTR_NATIVE_SOURCE_REALPATH "${CMAKE_SOURCE_DIR}" REALPATH)
if(CTR_NATIVE_GIT_TOPLEVEL_RESULT EQUAL 0)
    get_filename_component(CTR_NATIVE_GIT_TOPLEVEL_REALPATH
        "${CTR_NATIVE_GIT_TOPLEVEL}" REALPATH)
endif()

set(CTR_NATIVE_GIT_HASH "unknown")
set(CTR_NATIVE_SOURCE_IDENTITY "unknown")
set(CTR_NATIVE_GIT_DIRTY 1)
if(CTR_NATIVE_GIT_TOPLEVEL_RESULT EQUAL 0
   AND CTR_NATIVE_GIT_TOPLEVEL_REALPATH STREQUAL CTR_NATIVE_SOURCE_REALPATH)
    execute_process(
        COMMAND git -C "${CMAKE_SOURCE_DIR}" rev-parse HEAD
        OUTPUT_VARIABLE CTR_NATIVE_SOURCE_IDENTITY
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE CTR_NATIVE_GIT_COMMIT_RESULT
        ERROR_QUIET
    )
    string(LENGTH "${CTR_NATIVE_SOURCE_IDENTITY}"
        CTR_NATIVE_SOURCE_IDENTITY_LENGTH)
    if(NOT CTR_NATIVE_GIT_COMMIT_RESULT EQUAL 0
       OR NOT CTR_NATIVE_SOURCE_IDENTITY MATCHES "^[0-9a-f]+$"
       OR NOT CTR_NATIVE_SOURCE_IDENTITY_LENGTH EQUAL 40)
        message(FATAL_ERROR "Could not determine the source checkout commit.")
    endif()
    string(SUBSTRING "${CTR_NATIVE_SOURCE_IDENTITY}" 0 12 CTR_NATIVE_GIT_HASH)
    if(CTR_NATIVE_SOURCE_COMMIT_NORMALIZED)
        if(CTR_NATIVE_SOURCE_COMMIT_LENGTH EQUAL 40)
            set(CTR_NATIVE_SOURCE_COMMIT_EXPECTED "${CTR_NATIVE_SOURCE_IDENTITY}")
        else()
            set(CTR_NATIVE_SOURCE_COMMIT_EXPECTED "${CTR_NATIVE_GIT_HASH}")
        endif()
        if(NOT CTR_NATIVE_SOURCE_COMMIT_NORMALIZED STREQUAL
           CTR_NATIVE_SOURCE_COMMIT_EXPECTED)
            message(FATAL_ERROR
                "CTR_NATIVE_SOURCE_COMMIT does not match the source checkout commit."
            )
        endif()
    endif()
    execute_process(
        COMMAND git -C "${CMAKE_SOURCE_DIR}" status --porcelain=v1
            --untracked-files=normal
        OUTPUT_VARIABLE CTR_NATIVE_GIT_STATUS
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE CTR_NATIVE_GIT_STATUS_RESULT
        ERROR_QUIET
    )
    if(NOT CTR_NATIVE_GIT_STATUS_RESULT EQUAL 0)
        message(FATAL_ERROR "Could not determine whether the source checkout is dirty.")
    endif()
    if(CTR_NATIVE_GIT_STATUS)
        set(CTR_NATIVE_GIT_DIRTY 1)
    else()
        set(CTR_NATIVE_GIT_DIRTY 0)
    endif()
elseif(CTR_NATIVE_SOURCE_COMMIT_SHORT)
    set(CTR_NATIVE_GIT_HASH "${CTR_NATIVE_SOURCE_COMMIT_SHORT}")
    set(CTR_NATIVE_SOURCE_IDENTITY "${CTR_NATIVE_SOURCE_COMMIT_NORMALIZED}")
    set(CTR_NATIVE_GIT_DIRTY 0)
    message(STATUS
        "CTR Native: source identity ${CTR_NATIVE_GIT_HASH} from CTR_NATIVE_SOURCE_COMMIT"
    )
else()
    get_filename_component(CTR_NATIVE_SOURCE_DIR_NAME "${CMAKE_SOURCE_DIR}" NAME)
    if(CTR_NATIVE_SOURCE_DIR_NAME MATCHES "^CTRPad-source-([0-9a-f]+)$")
        set(CTR_NATIVE_SOURCE_DIR_COMMIT "${CMAKE_MATCH_1}")
        string(LENGTH "${CTR_NATIVE_SOURCE_DIR_COMMIT}"
            CTR_NATIVE_SOURCE_DIR_COMMIT_LENGTH)
        if(CTR_NATIVE_SOURCE_DIR_COMMIT_LENGTH EQUAL 12)
            set(CTR_NATIVE_GIT_HASH "${CTR_NATIVE_SOURCE_DIR_COMMIT}")
            set(CTR_NATIVE_SOURCE_IDENTITY "${CTR_NATIVE_SOURCE_DIR_COMMIT}")
            set(CTR_NATIVE_GIT_DIRTY 0)
            message(STATUS
                "CTR Native: source identity ${CTR_NATIVE_GIT_HASH} from corresponding-source root"
            )
        endif()
    endif()
endif()

if(CTR_NATIVE_GIT_DIRTY EQUAL 0)
    set(CTR_NATIVE_BUILD_ID "${CTR_NATIVE_GIT_HASH}")
else()
    set(CTR_NATIVE_BUILD_ID "${CTR_NATIVE_GIT_HASH}-dirty")
endif()

function(ctr_native_add_label_warning_if_supported target warning flag_check)
    set(saved_required_flags "${CMAKE_REQUIRED_FLAGS}")
    if(CMAKE_C_COMPILER_ID MATCHES "Clang")
        set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} -Werror=unknown-warning-option")
    endif()

    check_c_compiler_flag("${warning}" "${flag_check}")
    set(CMAKE_REQUIRED_FLAGS "${saved_required_flags}")

    if(${flag_check})
        target_compile_options(${target} PRIVATE "${warning}")
    endif()
endfunction()

# SDL3
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
set(SDL_GPU OFF CACHE BOOL "" FORCE)
set(SDL_RENDER OFF CACHE BOOL "" FORCE)
set(SDL_CAMERA OFF CACHE BOOL "" FORCE)
set(SDL_SENSOR OFF CACHE BOOL "" FORCE)
set(SDL_DIALOG OFF CACHE BOOL "" FORCE)
set(SDL_TRAY OFF CACHE BOOL "" FORCE)
set(SDL_POWER OFF CACHE BOOL "" FORCE)
set(SDL_VULKAN OFF CACHE BOOL "" FORCE)
set(SDL_OPENGLES ${CTR_NATIVE_RENDERER_GLES} CACHE BOOL "" FORCE)
set(SDL_OFFSCREEN OFF CACHE BOOL "" FORCE)
set(SDL_SNDIO OFF CACHE BOOL "" FORCE)
set(SDL_SNDIO_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_X11_XSCRNSAVER OFF CACHE BOOL "" FORCE)
set(SDL_X11_XTEST OFF CACHE BOOL "" FORCE)
add_subdirectory(externals/SDL)

# CTR Native
set(CTR_NATIVE_EXECUTABLE_OPTIONS)
if(CTR_NATIVE_MACOS_BUNDLE OR IOS)
    list(APPEND CTR_NATIVE_EXECUTABLE_OPTIONS MACOSX_BUNDLE)
endif()

add_executable(ctr_native ${CTR_NATIVE_EXECUTABLE_OPTIONS} main.c)
if(CTR_NATIVE_MACOS_BUNDLE OR IOS)
    set(CTR_NATIVE_DISTRIBUTION_RESOURCES
        "${CMAKE_SOURCE_DIR}/LICENSE"
        "${CMAKE_SOURCE_DIR}/THIRD_PARTY_NOTICES.md"
    )
    if(IOS)
        list(APPEND CTR_NATIVE_DISTRIBUTION_RESOURCES
            "${CMAKE_SOURCE_DIR}/docs/INSTALL-IOS.md")
    else()
        list(APPEND CTR_NATIVE_DISTRIBUTION_RESOURCES
            "${CMAKE_SOURCE_DIR}/docs/INSTALL-MACOS.md")
    endif()
    set_source_files_properties(${CTR_NATIVE_DISTRIBUTION_RESOURCES} PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources"
    )
    target_sources(ctr_native PRIVATE ${CTR_NATIVE_DISTRIBUTION_RESOURCES})
endif()
if(IOS)
    file(GLOB_RECURSE CTR_NATIVE_IOS_ASSET_CATALOG_FILES CONFIGURE_DEPENDS
        "${CMAKE_SOURCE_DIR}/platform/apple/Assets.xcassets/*"
    )
    set_source_files_properties(${CTR_NATIVE_IOS_ASSET_CATALOG_FILES} PROPERTIES HEADER_FILE_ONLY TRUE)
    # Asset-only edits must rerun the post-link actool packaging step too.
    set_property(TARGET ctr_native APPEND PROPERTY LINK_DEPENDS ${CTR_NATIVE_IOS_ASSET_CATALOG_FILES})
    target_sources(ctr_native PRIVATE
        platform/apple/native_ios_import.m
        platform/apple/native_ios_telemetry.m
        platform/apple/native_ios_touch.m
        platform/apple/native_diagnostics.m
        ${CTR_NATIVE_IOS_ASSET_CATALOG_FILES}
    )
    target_compile_options(ctr_native PRIVATE "$<$<COMPILE_LANGUAGE:OBJC>:-fobjc-arc>")

    if(CMAKE_OSX_SYSROOT MATCHES "[Ss]imulator")
        set(CTR_NATIVE_IOS_ASSET_PLATFORM iphonesimulator)
    else()
        set(CTR_NATIVE_IOS_ASSET_PLATFORM iphoneos)
    endif()
    add_custom_command(TARGET ctr_native POST_BUILD
        COMMAND /usr/bin/xcrun actool
            "${CMAKE_SOURCE_DIR}/platform/apple/Assets.xcassets"
            --compile "$<TARGET_BUNDLE_DIR:ctr_native>"
            --platform "${CTR_NATIVE_IOS_ASSET_PLATFORM}"
            --minimum-deployment-target "${CMAKE_OSX_DEPLOYMENT_TARGET}"
            --target-device iphone
            --target-device ipad
            --app-icon AppIcon
            --output-partial-info-plist "${CMAKE_BINARY_DIR}/CTRPad-asset-info.plist"
            --compress-pngs
            --warnings
            --errors
            --notices
        COMMENT "Compiling CTRPad iOS app icon asset catalog"
        VERBATIM
    )
endif()
set_target_properties(ctr_native PROPERTIES
    C_STANDARD 17
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)
if(CTR_NATIVE_MACOS_BUNDLE)
    find_program(CTR_NATIVE_CODESIGN_EXECUTABLE codesign REQUIRED)
    set(CTR_NATIVE_MACOS_ICON "${CMAKE_SOURCE_DIR}/platform/apple/CTRPad.icns")
    set_source_files_properties("${CTR_NATIVE_MACOS_ICON}" PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources"
    )
    target_sources(ctr_native PRIVATE
        platform/apple/native_macos_controls.m
        platform/apple/native_macos_import.m
        "${CTR_NATIVE_MACOS_ICON}"
    )
    target_compile_options(ctr_native PRIVATE "$<$<COMPILE_LANGUAGE:OBJC>:-fobjc-arc>")
    target_compile_definitions(ctr_native PRIVATE CTR_NATIVE_MACOS_BUNDLE=1)
    target_link_libraries(ctr_native "-framework AppKit" "-framework UniformTypeIdentifiers")
    set_target_properties(ctr_native PROPERTIES
        OUTPUT_NAME "CTRPad"
        MACOSX_BUNDLE_BUNDLE_NAME "CTRPad"
        MACOSX_BUNDLE_ICON_FILE "CTRPad.icns"
        MACOSX_BUNDLE_GUI_IDENTIFIER "io.github.chrissotraidis.ctrpad"
        MACOSX_BUNDLE_INFO_STRING "Touch-first native Crash Team Racing runtime"
        MACOSX_BUNDLE_LONG_VERSION_STRING "${CTR_NATIVE_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${CTR_NATIVE_VERSION}"
        MACOSX_BUNDLE_BUNDLE_VERSION "2"
        MACOSX_BUNDLE_COPYRIGHT "GPL-3.0; retail game data is user-supplied"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/platform/apple/Info.plist.in"
    )
    add_custom_command(TARGET ctr_native POST_BUILD
        COMMAND "${CTR_NATIVE_CODESIGN_EXECUTABLE}"
            --force
            --sign -
            --identifier "io.github.chrissotraidis.ctrpad"
            "$<TARGET_BUNDLE_DIR:ctr_native>"
        COMMENT "Ad-hoc signing CTRPad.app for local macOS execution"
        VERBATIM
    )
endif()
if(IOS)
    set_target_properties(ctr_native PROPERTIES
        OUTPUT_NAME "CTRPad"
        MACOSX_BUNDLE_BUNDLE_NAME "CTRPad"
        MACOSX_BUNDLE_GUI_IDENTIFIER "${CTR_NATIVE_IOS_BUNDLE_IDENTIFIER}"
        MACOSX_BUNDLE_INFO_STRING "Touch-first native Crash Team Racing runtime"
        MACOSX_BUNDLE_LONG_VERSION_STRING "${CTR_NATIVE_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${CTR_NATIVE_VERSION}"
        MACOSX_BUNDLE_BUNDLE_VERSION "4"
        MACOSX_BUNDLE_COPYRIGHT "GPL-3.0; retail game data is user-supplied"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/platform/apple/Info-iOS.plist.in"
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${CTR_NATIVE_IOS_BUNDLE_IDENTIFIER}"
        XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"
    )
endif()
target_include_directories(ctr_native PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_compile_definitions(ctr_native PRIVATE
    CTR_NATIVE
    BUILD=926
    CTR_INTERNAL
    CTR_NATIVE_VERSION=\"${CTR_NATIVE_VERSION}\"
    CTR_NATIVE_SOURCE_IDENTITY=\"${CTR_NATIVE_SOURCE_IDENTITY}\"
    CTR_NATIVE_BUILD_ID=\"${CTR_NATIVE_BUILD_ID}\"
)
if(CTR_NATIVE_RENDERER_GLES)
    target_compile_definitions(ctr_native PRIVATE CTR_NATIVE_RENDERER_GLES=1)
endif()
if(CTR_NATIVE_MSVC_FRONTEND)
    target_compile_options(ctr_native PRIVATE
        /W4 #TODO(aalhendi): fix warnings
        /bigobj
        /wd4152
        /wd4244
        /wd4245
        /wd4310
        /wd5287
    )
    if(CTR_NATIVE_X86_TARGET)
        target_compile_options(ctr_native PRIVATE /arch:SSE2)
    endif()
else()
    target_compile_options(ctr_native PRIVATE
        -Wenum-conversion
        -Wenum-compare
        -Wswitch-enum
        -Wformat
        -Wparentheses
        $<$<CONFIG:Release>:-O2>
        $<$<CONFIG:Release>:-g>
    )

    if(CTR_NATIVE_X86_TARGET)
        target_compile_options(ctr_native PRIVATE -msse)
    endif()

    check_c_compiler_flag("-fno-strict-aliasing" CTR_HAS_FNO_STRICT_ALIASING)
    if(CTR_HAS_FNO_STRICT_ALIASING)
        target_compile_options(ctr_native PRIVATE -fno-strict-aliasing)
    endif()

    check_c_compiler_flag("-fwrapv" CTR_HAS_FWRAPV)
    if(CTR_HAS_FWRAPV)
        target_compile_options(ctr_native PRIVATE -fwrapv)
    endif()

    if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
        target_compile_options(ctr_native PRIVATE -Wall -Wextra)
        ctr_native_add_label_warning_if_supported(ctr_native -Wfree-labels CTR_HAS_WFREE_LABELS)
    elseif(CMAKE_C_COMPILER_ID MATCHES "Clang")
        ctr_native_add_label_warning_if_supported(ctr_native -Wc23-extensions CTR_HAS_WC23_EXTENSIONS)
    endif()
endif()
target_link_libraries(ctr_native SDL3::SDL3)
if(IOS)
    target_link_libraries(ctr_native "-framework UIKit" "-framework QuartzCore" "-framework UniformTypeIdentifiers")
endif()
if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_link_options(ctr_native PRIVATE -static-libgcc)
endif()
if(MINGW)
    target_link_options(ctr_native PRIVATE -static)
endif()

include(CTest)
if(BUILD_TESTING)
    add_test(NAME ctr_native_particle_pool COMMAND $<TARGET_FILE:ctr_native> --self-test-particle-pool)
    add_test(NAME ctr_native_version COMMAND $<TARGET_FILE:ctr_native> --version)
    set_tests_properties(ctr_native_version PROPERTIES PASS_REGULAR_EXPRESSION "^CTR Native ")
    add_test(NAME ctr_native_state_digest COMMAND $<TARGET_FILE:ctr_native> --self-test-state-digest)
    set_tests_properties(ctr_native_state_digest PROPERTIES PASS_REGULAR_EXPRESSION
        "^\\[CTR StateDigest\\] self-test passed: .* mutation component=drivers"
    )
    add_test(NAME ctr_native_replay_gate COMMAND $<TARGET_FILE:ctr_native> --self-test-replay-gate)
    set_tests_properties(ctr_native_replay_gate PROPERTIES PASS_REGULAR_EXPRESSION
        "\\[CTR Replay\\] self-test passed: runtime-error=1 divergence=2 mutation-frame=17 component=drivers binary-identity=checked checkpoint-start=checked complete-vsync=checked"
    )
    add_test(NAME ctr_native_guest_ref COMMAND $<TARGET_FILE:ctr_native> --self-test-guest-ref)
    set_tests_properties(ctr_native_guest_ref PROPERTIES PASS_REGULAR_EXPRESSION
        "^\\[CTR GuestRef\\] self-test passed: reference=0x03000014 overflow=out-of-range stale=unregistered-region"
    )
    add_test(NAME ctr_native_asset_relocation COMMAND $<TARGET_FILE:ctr_native> --self-test-asset-relocation)
    set_tests_properties(ctr_native_asset_relocation PROPERTIES PASS_REGULAR_EXPRESSION
        "^\\[CTR AssetRelocation\\] self-test passed: first=0x07000014 duplicate=duplicate-patch target=target-out-of-range atomic=yes model=checked override=checked level=checked cache-recycle=targeted\\+range\\+all"
    )
    add_test(NAME ctr_native_input COMMAND $<TARGET_FILE:ctr_native> --self-test-input)
    set_tests_properties(ctr_native_input PROPERTIES PASS_REGULAR_EXPRESSION
        "^\\[CTR Input\\] self-test passed: metadata-key=4 legacy-enter=40 migration-enter=40 live-start=retail tap-latch=shift\\+right until-retail-poll aliases=17 remap\\+duplicate\\+reset=passed"
    )
    add_test(NAME ctr_native_collision_scratch COMMAND $<TARGET_FILE:ctr_native> --self-test-collision-scratch)
    set_tests_properties(ctr_native_collision_scratch PROPERTIES PASS_REGULAR_EXPRESSION
        "^\\[CTR CollScratch\\] self-test passed: guest-size=0x20c extended=0x2f8 pointer-word=(raw32|zero64) owners=isolated"
    )
    add_test(NAME ctr_native_vehicle_constants COMMAND $<TARGET_FILE:ctr_native> --self-test-vehicle-constants)
    set_tests_properties(ctr_native_vehicle_constants PROPERTIES PASS_REGULAR_EXPRESSION
        "^\\[CTR VehBirth\\] self-test passed: constants=65 classes=4 retail-offsets=translated"
    )
    add_test(NAME ctr_native_vs_quip_offsets COMMAND $<TARGET_FILE:ctr_native> --self-test-vs-quip-offsets)
    set_tests_properties(ctr_native_vs_quip_offsets PROPERTIES PASS_REGULAR_EXPRESSION
        "^\\[CTR VsQuip\\] self-test passed: metas=51 readers=46 retail-ranges=translated"
    )
    add_test(NAME ctr_native_render_lists COMMAND $<TARGET_FILE:ctr_native> --self-test-render-lists)
    set_tests_properties(ctr_native_render_lists PROPERTIES PASS_REGULAR_EXPRESSION
        "^\\[CTR RenderLists\\] self-test passed: pointer-size=(4|8) slot-size=(8|16) full-dynamic=0x(28|50) named-heads=6 dispatch=canonical-index host-cursors=4 clip-ot=rebased"
    )
    add_test(NAME ctr_native_red_beaker_layout COMMAND $<TARGET_FILE:ctr_native> --self-test-red-beaker-layout)
    set_tests_properties(ctr_native_red_beaker_layout PROPERTIES PASS_REGULAR_EXPRESSION
        "^\\[CTR RedBeaker\\] self-test passed: pointer-size=(4|8) instance=0x(74|a0) idpp=0x(88|b0) players=4 retail-alias=named"
    )
    add_test(NAME ctr_native_audio_state_alignment COMMAND $<TARGET_FILE:ctr_native> --self-test-audio-state-alignment)
    set_tests_properties(ctr_native_audio_state_alignment PROPERTIES PASS_REGULAR_EXPRESSION
        "^\\[CTR AudioState\\] self-test passed: snapshot-align=(4|8) blob-mod8=4 boundary=memcpy"
    )
    add_test(NAME ctr_native_audio_mixer COMMAND $<TARGET_FILE:ctr_native> --self-test-audio-mixer)
    set_tests_properties(ctr_native_audio_mixer PROPERTIES PASS_REGULAR_EXPRESSION
        "^\\[CTR AudioMixer\\] self-test passed: adpcm=streamed pan=left reverb=tail dry=0x132e19d77167fb3d wet=0x4bdedc91d1293ad8 tail-frames=6905"
    )
    add_test(NAME ctr_native_renderer_dialect COMMAND $<TARGET_FILE:ctr_native> --self-test-renderer-dialect)
    if(CTR_NATIVE_RENDERER_GLES)
        set_tests_properties(ctr_native_renderer_dialect PROPERTIES PASS_REGULAR_EXPRESSION
            "^\\[CTR Renderer\\] dialect self-test passed: api=gles context=3.0 profile=es shader=300es loader=sdl-proc wireframe=disabled debug-labels=disabled gpu-timer=disabled shutdown=preinit-safe"
        )
    else()
        set_tests_properties(ctr_native_renderer_dialect PROPERTIES PASS_REGULAR_EXPRESSION
            "^\\[CTR Renderer\\] dialect self-test passed: api=gl context=3.3 profile=core shader=140 loader=native-gl wireframe=enabled debug-labels=enabled gpu-timer=enabled shutdown=preinit-safe"
        )
    endif()
    if(APPLE AND NOT IOS AND NOT CTR_NATIVE_RENDERER_GLES)
        add_test(NAME ctr_native_renderer_pixels COMMAND $<TARGET_FILE:ctr_native> --self-test-renderer-pixels)
        set_tests_properties(ctr_native_renderer_pixels PROPERTIES PASS_REGULAR_EXPRESSION
            "\\[CTR Renderer\\] pixel self-test passed: api=gl size=32x16 formats=4,8,16 clut=4,8 transparency=zero,stp blend=average,add,subtract,quarter bilinear=mixed-stp ordered-overlap=match fallback-draws=12 active-draws=12 mask=output-bit framebuffer=feedback vram=rgb5551 hash=[0-9a-f]+ blend-hash=[0-9a-f]+ blend-oracle=two-pass oracle-hash=[0-9a-f]+ framebuffer-fetch=two-pass present=resolve\\+blit@[0-9]+x[0-9]+ present-hash=[0-9a-f]+"
        )
    endif()
    add_test(NAME ctr_native_lifecycle COMMAND $<TARGET_FILE:ctr_native> --self-test-lifecycle)
    set_tests_properties(ctr_native_lifecycle PROPERTIES PASS_REGULAR_EXPRESSION
        "^\\[CTR Lifecycle\\] self-test passed: background=idempotent foreground=rebase quit=cooperative audio=paired low-memory=flush"
    )
    add_test(NAME ctr_native_frame_stats COMMAND $<TARGET_FILE:ctr_native> --self-test-frame-stats)
    set_tests_properties(ctr_native_frame_stats PROPERTIES PASS_REGULAR_EXPRESSION
        "^\\[CTR FrameStats\\] self-test passed: mean=checked median=odd\\+even p95=nearest-rank p99=nearest-rank input=immutable"
    )
    add_test(NAME ctr_native_storage COMMAND $<TARGET_FILE:ctr_native> --self-test-storage)
    set_tests_properties(ctr_native_storage PROPERTIES PASS_REGULAR_EXPRESSION
        "^\\[CTR Storage\\] self-test passed: bundle=read-only preferences=private documents=user-visible imports=preferred desktop=portable"
    )
    add_test(NAME ctr_native_memcard_atomic_write COMMAND $<TARGET_FILE:ctr_native> --self-test-memcard-atomic-write)
    set_tests_properties(ctr_native_memcard_atomic_write PROPERTIES PASS_REGULAR_EXPRESSION
        "^\\[CTR Memcard\\] atomic-write self-test passed: write=flushed replace=atomic failure=preserves-existing temp=clean retry=checked"
    )
    add_test(NAME ctr_native_checkpoint_pointer_validation COMMAND $<TARGET_FILE:ctr_native> --self-test-checkpoint-pointer-validation)
    set_tests_properties(ctr_native_checkpoint_pointer_validation PROPERTIES PASS_REGULAR_EXPRESSION
        "^\\[CTR State\\] pointer-validation self-test passed: scalar=ignored relocated=reversion-checked overlap-idempotence=checked pool-allocation=free-list-complement camera-quad=checked"
    )
    add_test(NAME ctr_native_vehicle_lap_checkpoint_bounds COMMAND $<TARGET_FILE:ctr_native> --self-test-vehicle-lap-checkpoint-bounds)
    set_tests_properties(ctr_native_vehicle_lap_checkpoint_bounds PROPERTIES PASS_REGULAR_EXPRESSION
        "^\\[CTR VehLap\\] self-test passed: count=4 first=valid last=valid one-past=rejected sentinel=0xff-rejected empty=rejected"
    )
    add_test(NAME ctr_native_potion_emitter_layout COMMAND $<TARGET_FILE:ctr_native> --self-test-potion-emitter-layout)
    set_tests_properties(ctr_native_potion_emitter_layout PROPERTIES PASS_REGULAR_EXPRESSION
        "^\\[CTR PotionEmitter\\] self-test passed: pointer-size=(4|8) entries=8 lifespan=20 random-velocity=400 typed-layout=yes"
    )
    add_test(NAME ctr_native_cutscene_particle_emitter_layout COMMAND $<TARGET_FILE:ctr_native> --self-test-cutscene-particle-emitter-layout)
    set_tests_properties(ctr_native_cutscene_particle_emitter_layout PROPERTIES PASS_REGULAR_EXPRESSION
        "^\\[CTR CutsceneEmitter\\] self-test passed: pointer-size=(4|8) groups=7 observed-group=46 lifespan=15 count=1 typed-function-init=yes"
    )
    if(APPLE AND NOT IOS)
        add_test(NAME ctr_ios_entitlement_binding
            COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tools/test-ios-entitlement-binding.sh"
        )
        set_tests_properties(ctr_ios_entitlement_binding PROPERTIES PASS_REGULAR_EXPRESSION
            "^IOS_ENTITLEMENT_BINDING_SELF_TEST=passed positives=2 negatives=7"
        )
        add_test(NAME ctr_ios_devicectl_json
            COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tools/test-devicectl-json.sh"
        )
        set_tests_properties(ctr_ios_devicectl_json PROPERTIES PASS_REGULAR_EXPRESSION
            "^DEVICECTL_JSON_SELF_TEST=passed positives=4 negatives=10"
        )
        add_test(NAME ctr_ios_build_identity
            COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/tools/test-ios-build-identity.sh"
        )
        set_tests_properties(ctr_ios_build_identity PROPERTIES PASS_REGULAR_EXPRESSION
            "^IOS_BUILD_IDENTITY_SELF_TEST=passed positives=2 negatives=6"
        )
    endif()
endif()
