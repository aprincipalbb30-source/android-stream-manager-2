cmake_minimum_required(VERSION 3.16)
project(AndroidStreamManager VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Opções de build
option(BUILD_DASHBOARD "Build Qt Dashboard" ON)
option(BUILD_APK_BUILDER "Build APK Builder" ON)
option(BUILD_SERVER "Build Stream Server" ON)
option(BUILD_TESTS "Build unit tests" OFF)

# Diretórios
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Incluir diretórios
# REMOVIDO: include_directories globais. Usaremos target_include_directories por alvo.

# Encontrar dependências
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)

# SQLite3 (obrigatório)
find_package(SQLite3 REQUIRED)
if(NOT SQLite3_FOUND)
    message(FATAL_ERROR "SQLite3 not found - database features are required")
endif()

# Biblioteca JSON nlohmann (header-only)
include(FetchContent)
FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.2/json.hpp
    DOWNLOAD_NO_EXTRACT TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

# LZ4 (tentar encontrar)
find_library(LZ4_LIBRARY NAMES lz4 liblz4)
find_path(LZ4_INCLUDE_DIR NAMES lz4.h)
if(NOT LZ4_LIBRARY)
    message(WARNING "LZ4 library not found - compression features will be limited")
endif()

# libzip (tentar encontrar)
find_library(LIBZIP_LIBRARY NAMES zip libzip)
if(NOT LIBZIP_LIBRARY)
    message(WARNING "libzip library not found - APK operations may be limited")
endif()

# JWT (tentar encontrar)
FetchContent_Declare(
    jwt_cpp
    GIT_REPOSITORY git@github.com:Thalhammer/jwt-cpp.git
    GIT_TAG        v0.4.0 # Ou a versão mais recente/estável
)
FetchContent_MakeAvailable(jwt_cpp)

# Biblioteca Base64 (vendida localmente)
add_library(base64 STATIC
    base64.cpp
)
target_include_directories(base64 PUBLIC ${CMAKE_SOURCE_DIR})

# Configurações específicas da plataforma
if(WIN32)
    add_definitions(-DWINDOWS_BUILD -D_CRT_SECURE_NO_WARNINGS)
    set(PLATFORM_LIBS ws2_32 crypt32)
elseif(UNIX)
    add_definitions(-DLINUX_BUILD -D_GLIBCXX_USE_CXX11_ABI=1)
    set(PLATFORM_LIBS pthread dl)
endif()

# Biblioteca de Otimização
add_library(optimization_lib STATIC
    optimization/stream_optimizer.cpp
    optimization/build_cache.cpp
    optimization/thread_pool.cpp
)
target_include_directories(optimization_lib
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(optimization_lib PRIVATE
    ${LZ4_LIBRARY}
    OpenSSL::Crypto
    ${PLATFORM_LIBS}
)
# Add include directories for dependencies
target_include_directories(optimization_lib PRIVATE ${OpenSSL_INCLUDE_DIR} ${LZ4_INCLUDE_DIR})

# Biblioteca de Segurança
add_library(security_lib STATIC
    security/tls_manager.cpp
    security/jwt_manager.cpp
    security/apk_signer.cpp
)
target_include_directories(security_lib PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(security_lib PUBLIC
    OpenSSL::SSL
    OpenSSL::Crypto
    jwt-cpp
    ${PLATFORM_LIBS}
)
target_link_libraries(security_lib PRIVATE base64)

# Biblioteca de Compliance
add_library(compliance_lib STATIC
    compliance/compliance_manager.cpp
)
target_include_directories(compliance_lib PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(compliance_lib
    PRIVATE
    optimization_lib
    ${PLATFORM_LIBS}
)

# Biblioteca Core
add_library(core_lib STATIC
    core/system_manager.cpp
    core/apk_builder.cpp
    core/device_manager.cpp
    shared/protocol.cpp
)
target_include_directories(core_lib PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

# Biblioteca Database
add_library(database_lib STATIC
    database/database_manager.cpp
)
target_include_directories(database_lib PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(database_lib
    SQLite::SQLite3
    ${PLATFORM_LIBS}
)

# Biblioteca Server
add_library(server_lib STATIC
    server/stream_server.cpp
    server/websocket_session.cpp
    server/http_server.cpp
)
target_include_directories(server_lib PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

# Encontrar libprocps para monitoramento
find_library(LIBPROCPS_LIBRARY NAMES procps)
find_path(LIBPROCPS_INCLUDE_DIR NAMES proc/readproc.h)
add_library(monitoring_lib STATIC
    monitoring/metrics_collector.cpp
    monitoring/alerts_manager.cpp
    monitoring/health_checker.cpp
    monitoring/prometheus_exporter.cpp
)
target_include_directories(monitoring_lib PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)
if(LIBPROCPS_LIBRARY)
    target_link_libraries(monitoring_lib PRIVATE ${LIBPROCPS_LIBRARY})
    if(LIBPROCPS_INCLUDE_DIR)
        target_include_directories(monitoring_lib PRIVATE ${LIBPROCPS_INCLUDE_DIR})
    endif()
endif()

target_link_libraries(core_lib
    database_lib
    optimization_lib
    security_lib
    compliance_lib
    monitoring_lib
    OpenSSL::Crypto
    ${LIBZIP_LIBRARY}
    ${ZLIB_LIBRARIES}
    ${PLATFORM_LIBS}
)

# Dashboard Qt
if(BUILD_DASHBOARD)
    find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network Multimedia WebSockets)

    # --- Find FFmpeg libraries for real-time decoding ---
    find_package(PkgConfig)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(AVCODEC REQUIRED libavcodec)
        pkg_check_modules(AVUTIL REQUIRED libavutil)
        pkg_check_modules(SWSCALE REQUIRED libswscale)
    endif()

    if(Qt6_FOUND)
        # Configurar Qt6
        set(CMAKE_AUTOMOC ON)
        set(CMAKE_AUTORCC ON)
        set(CMAKE_AUTOUIC ON)

        add_executable(android_stream_dashboard
            dashboard/main.cpp
            dashboard/main_window.cpp
            dashboard/apk_config_widget.cpp
            dashboard/monitoring_widget.cpp
            dashboard/streaming_viewer.cpp
            dashboard/app_monitoring_widget.cpp
            dashboard/resources.qrc
        )

        if(AVCODEC_FOUND)
            target_include_directories(android_stream_dashboard PRIVATE ${AVCODEC_INCLUDE_DIRS})
            target_include_directories(android_stream_dashboard PRIVATE ${AVUTIL_INCLUDE_DIRS})
            target_include_directories(android_stream_dashboard PRIVATE ${SWSCALE_INCLUDE_DIRS})
        endif()

        target_link_libraries(android_stream_dashboard
            core_lib
            Qt6::Core
            Qt6::Widgets
            Qt6::Network
            Qt6::Multimedia
            Qt6::WebSockets
            ${AVCODEC_LIBRARIES}
            ${AVUTIL_LIBRARIES}
            ${SWSCALE_LIBRARIES}
        )

        # Configurações específicas do Qt
        if(WIN32)
            # Windows - nada especial necessário
        elseif(UNIX)
            # Linux - configurações específicas
            target_compile_definitions(android_stream_dashboard PRIVATE QT_NO_FOREACH)
        endif()

    else()
        message(WARNING "Qt6 not found - dashboard will not be built")
        set(BUILD_DASHBOARD OFF)
    endif()
endif()

# APK Builder
if(BUILD_APK_BUILDER)
    add_executable(apk_builder
        builder/main.cpp
    )

    target_link_libraries(apk_builder
        core_lib
        ${LIBZIP_LIBRARY}
        ${ZLIB_LIBRARIES}
        ${PLATFORM_LIBS}
    )
endif()

# Stream Server
if(BUILD_SERVER)
    add_executable(stream_server
        server/main.cpp
    )

    target_link_libraries(stream_server
        server_lib
        core_lib
        ${PLATFORM_LIBS}
    )
endif()

# Testes
if(BUILD_TESTS)
    enable_testing()

    # Encontrar Google Test (opcional)
    find_package(GTest)

    if(GTest_FOUND)
            add_executable(unit_tests
            tests/test_system_manager.cpp
            tests/test_apk_builder.cpp
            tests/test_stream_optimizer.cpp
        )

        target_link_libraries(unit_tests
            core_lib
            GTest::GTest
            GTest::Main
        )
        add_test(NAME unit_tests COMMAND unit_tests)
    else()
        message(WARNING "Google Test not found - unit tests will not be built")
    endif()

    # Teste de streaming
    add_executable(test_streaming
        test_streaming.cpp
    )

    target_link_libraries(test_streaming
        core_lib
    )
endif()

# Instalação
install(TARGETS core_lib optimization_lib security_lib compliance_lib
    DESTINATION lib)

if(BUILD_DASHBOARD AND Qt6_FOUND)
    install(TARGETS android_stream_dashboard DESTINATION bin)
endif()

if(BUILD_APK_BUILDER)
    install(TARGETS apk_builder DESTINATION bin)
endif()

if(BUILD_SERVER)
    install(TARGETS stream_server DESTINATION bin)
endif()

install(DIRECTORY shared/ DESTINATION include/android-stream-manager
    FILES_MATCHING PATTERN "*.h")

install(FILES config/system_config.json
    DESTINATION etc/android-stream-manager/)

# Configuração de CPack para pacotes
set(CPACK_PACKAGE_NAME "android-stream-manager")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_CONTACT "your-email@example.com")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Android Stream Manager - Corporate Android Device Management")
set(CPACK_PACKAGE_VENDOR "Your Company")

# Pacotes DEB/RPM
if(UNIX)
    set(CPACK_GENERATOR "DEB")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "openssl, zlib1g, liblz4-1, libzip4")
    set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
endif()

include(CPack)
