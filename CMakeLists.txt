cmake_minimum_required(VERSION 3.20)
project(W101Suite LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Force x64
if(NOT CMAKE_GENERATOR_PLATFORM)
    set(CMAKE_GENERATOR_PLATFORM x64)
endif()

# DLL output
add_library(w101suite SHARED
    src/dllmain.cpp
)

# Header-only modules — just need to be findable
target_include_directories(w101suite PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Windows SDK libs (no legacy DirectX SDK needed)
target_link_libraries(w101suite PRIVATE
    d3d9
    ws2_32
    gdi32
    user32
    kernel32
)

# MSVC-specific flags
if(MSVC)
    target_compile_options(w101suite PRIVATE
        /W3             # Warning level 3
        /EHa            # SEH + C++ exceptions (needed for __try/__except)
        /MP             # Multi-processor compilation
        /GS-            # No buffer security checks (smaller DLL)
        /Oi             # Enable intrinsics
        /fp:fast        # Fast floating point
    )

    target_compile_definitions(w101suite PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
        _WINSOCK_DEPRECATED_NO_WARNINGS
    )

    # Release optimizations
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2 /GL /Gy")
    set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /LTCG /OPT:REF /OPT:ICF")

    # Output name without lib prefix
    set_target_properties(w101suite PROPERTIES
        PREFIX ""
        OUTPUT_NAME "w101suite"
        SUFFIX ".dll"
    )
endif()

# No default manifests (injected DLL doesn't need them)
set_target_properties(w101suite PROPERTIES
    LINK_FLAGS "/MANIFEST:NO"
)
