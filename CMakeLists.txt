cmake_minimum_required(VERSION 3.16)
project(AndroidStreamManager)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Dependências do Dashboard (Qt6)
if(BUILD_DASHBOARD)
    find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network)
    
    add_executable(android-stream-dashboard
        dashboard/main.cpp
        dashboard/main_window.cpp
        dashboard/apk_config_widget.cpp
        # ... outros arquivos
    )
    
    target_link_libraries(android-stream-dashboard
        Qt6::Core
        Qt6::Widgets
        Qt6::Network
        core-library
    )
endif()

# Biblioteca Core
add_library(core-library STATIC
    core/apk_builder.cpp
    core/device_manager.cpp
    shared/protocol.cpp
)

# Builder (APK Generator)
if(BUILD_APK_BUILDER)
    add_executable(apk-builder
        builder/main.cpp
        builder/apk_generator.cpp
    )
    
    target_link_libraries(apk-builder
        core-library
        OpenSSL::Crypto
        ${ANDROID_NDK_LIBRARIES}
    )
endif()