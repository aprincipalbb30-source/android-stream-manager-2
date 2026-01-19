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
include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CMAKE_SOURCE_DIR}/shared)
include_directories(${CMAKE_SOURCE_DIR}/core)
include_directories(${CMAKE_SOURCE_DIR}/security)
include_directories(${CMAKE_SOURCE_DIR}/optimization)
include_directories(${CMAKE_SOURCE_DIR}/compliance)

# Encontrar dependências
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)

# SQLite3 (obrigatório)
find_package(SQLite3 REQUIRED)
if(NOT SQLite3_FOUND)
    message(FATAL_ERROR "SQLite3 not found - database features are required")
endif()

# LZ4 (tentar encontrar)
find_library(LZ4_LIBRARY NAMES lz4 liblz4)
if(NOT LZ4_LIBRARY)
    message(WARNING "LZ4 library not found - compression features will be limited")
endif()

# libzip (tentar encontrar)
find_library(LIBZIP_LIBRARY NAMES zip libzip)
if(NOT LIBZIP_LIBRARY)
    message(WARNING "libzip library not found - APK operations may be limited")
endif()

# JWT (tentar encontrar)
find_library(JWT_LIBRARY NAMES jwt libjwt)
if(NOT JWT_LIBRARY)
    message(WARNING "JWT library not found - authentication features will be limited")
endif()

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

target_link_libraries(optimization_lib
    ${LZ4_LIBRARY}
    OpenSSL::Crypto
    ${PLATFORM_LIBS}
)

# Biblioteca de Segurança
add_library(security_lib STATIC
    security/tls_manager.cpp
    security/jwt_manager.cpp
    security/apk_signer.cpp
)

target_link_libraries(security_lib
    OpenSSL::SSL
    OpenSSL::Crypto
    ${JWT_LIBRARY}
    ${PLATFORM_LIBS}
)

# Biblioteca de Compliance
add_library(compliance_lib STATIC
    compliance/compliance_manager.cpp
)

target_link_libraries(compliance_lib
    ${PLATFORM_LIBS}
)

# Biblioteca Core
add_library(core_lib STATIC
    core/system_manager.cpp
    core/apk_builder.cpp
    core/device_manager.cpp
    shared/protocol.cpp
)

# Biblioteca Database
add_library(database_lib STATIC
    database/database_manager.cpp
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

# Biblioteca Monitoring
add_library(monitoring_lib STATIC
    monitoring/metrics_collector.cpp
    monitoring/alerts_manager.cpp
    monitoring/health_checker.cpp
    monitoring/prometheus_exporter.cpp
)

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
    find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network Multimedia)

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

        target_link_libraries(android_stream_dashboard
            core_lib
            Qt6::Core
            Qt6::Widgets
            Qt6::Network
            Qt6::Multimedia
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
        builder/apk_generator.cpp
    )

    target_link_libraries(apk_builder
        core_lib
        ${LIBZIP_LIBRARY}
        ${ZLIB_LIBRARIES}
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

# Servidor (se implementado)
if(BUILD_SERVER)
    # TODO: Implementar servidor principal
    message(STATUS "Server component - TODO")
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
        message(WARNING "Google Test not found - tests will not be built")
    endif()
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