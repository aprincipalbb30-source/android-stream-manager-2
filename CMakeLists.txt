cmake_minimum_required(VERSION 3.16)
project(AndroidStreamManager VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Política para silenciar o aviso do FetchContent sobre DOWNLOAD_EXTRACT_TIMESTAMP
set(CMAKE_POLICY_DEFAULT_CMP0135 NEW)

# Opções de build
option(BUILD_DASHBOARD "Build Qt Dashboard" ON)
option(BUILD_APK_BUILDER "Build APK Builder" ON)
option(BUILD_SERVER "Build Stream Server" ON)
option(BUILD_TESTS "Build unit tests" OFF)

# Incluir GNUInstallDirs para caminhos de instalação padrão (e.g., /usr/local/bin)
include(GNUInstallDirs)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_BINDIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_LIBDIR})

# Encontrar dependências
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)

# SQLite3 (obrigatório)
find_package(SQLite3 REQUIRED)
if(NOT SQLite3_FOUND)
    message(FATAL_ERROR "SQLite3 not found - database features are required")
endif()

# --- Dependências via FetchContent e find_package ---
include(FetchContent)

# Biblioteca JSON nlohmann (header-only)
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.2
)
FetchContent_MakeAvailable(nlohmann_json)

# JWT (jwt-cpp)
FetchContent_Declare(
    jwt_cpp
    GIT_REPOSITORY https://github.com/Thalhammer/jwt-cpp.git
    GIT_TAG        v0.7.0
)
# Desabilitar a construção de exemplos e testes da dependência para evitar erros de linkagem.
set(JWT_BUILD_EXAMPLES OFF)
set(JWT_BUILD_TESTS OFF)
FetchContent_MakeAvailable(jwt_cpp)

# uWebSockets (essencial para o servidor de streaming)
FetchContent_Declare(
    usockets
    GIT_REPOSITORY https://github.com/uNetworking/uSockets.git
    GIT_TAG        v0.8.6
)
FetchContent_MakeAvailable(usockets)

FetchContent_Declare(
    uWebSockets
    GIT_REPOSITORY https://github.com/uNetworking/uWebSockets.git
    GIT_TAG        v20.45.0
)
FetchContent_MakeAvailable(uWebSockets)

# LZ4 (Forçar FetchContent para evitar problemas com pacotes de sistema)
# Força a construção da biblioteca estática dentro do projeto lz4.
# Esta variável deve ser definida ANTES de FetchContent_Declare para ser eficaz.
set(BUILD_STATIC_LIBS ON)
message(STATUS "Fetching lz4 from source for consistency.")
FetchContent_Declare(
    lz4_fetch
    GIT_REPOSITORY https://github.com/lz4/lz4.git
    GIT_TAG        v1.9.4
    SOURCE_SUBDIR  build/cmake # Aponta para o diretório correto do CMakeLists.txt do lz4
)
FetchContent_MakeAvailable(lz4_fetch)
# O alvo criado pelo build do lz4 é 'lz4_static', criamos um alias para ele
add_library(lz4::lz4 ALIAS lz4_static)

# libzip (tentar encontrar e usar FetchContent como fallback)
# A busca por libzip pode falhar em sistemas com pacotes quebrados.
# A lógica a seguir tenta usar o find_package e, se falhar, recorre ao FetchContent.
# Forçamos o uso de FetchContent para libzip para evitar problemas com pacotes de sistema quebrados.
message(STATUS "Fetching libzip from source to avoid system package issues.")
FetchContent_Declare(
    libzip_fetch
    URL https://libzip.org/download/libzip-1.10.1.tar.gz # Usando uma versão estável
    URL_HASH SHA256=9669ae5dfe3ac5b3897536dc8466a874c8cf2c0e3b1fdd08d75b273884299363
)
# Desativamos opções que podem buscar por dependências externas desnecessárias (GnuTLS, mbedTLS)
# e garantimos que ele use a dependência OpenSSL que já encontramos.
set(ENABLE_GNUTLS OFF CACHE BOOL "" FORCE)
set(ENABLE_MBEDTLS OFF CACHE BOOL "" FORCE)
set(ENABLE_OPENSSL ON CACHE BOOL "" FORCE) # Garantir que ele use OpenSSL
FetchContent_MakeAvailable(libzip_fetch)
add_library(libzip::zip ALIAS zip)

# --- Configurações Específicas da Plataforma ---
if(WIN32)
    add_definitions(-DWINDOWS_BUILD -D_CRT_SECURE_NO_WARNINGS)
    set(PLATFORM_LIBS ws2_32 crypt32)
elseif(UNIX)
    add_definitions(-DLINUX_BUILD -D_GLIBCXX_USE_CXX11_ABI=1)
    set(PLATFORM_LIBS pthread dl)
endif()

# --- Biblioteca Principal Unificada ---
# Unificamos todas as fontes em uma única biblioteca para simplificar
# o gerenciamento de dependências e includes, resolvendo os erros de compilação.
add_library(android_stream_lib STATIC
    # Base64
    base64.cpp

    # Optimization
    optimization/stream_optimizer.cpp
    optimization/build_cache.cpp
    optimization/thread_pool.cpp

    # Security
    security/tls_manager.cpp
    security/jwt_manager.cpp
    security/apk_signer.cpp

    # Compliance
    compliance/compliance_manager.cpp

    # Database
    database/database_manager.cpp

    # Server
    server/stream_server.cpp
    server/websocket_session.cpp
    server/http_server.cpp

    # Monitoring
    monitoring/metrics_collector.cpp
    monitoring/alerts_manager.cpp
    monitoring/health_checker.cpp
    monitoring/prometheus_exporter.cpp

    # Core & Shared
    core/system_manager.cpp
    core/apk_builder.cpp
    core/device_manager.cpp
    shared/protocol.cpp
)

# Diretórios de inclusão para a biblioteca unificada
target_include_directories(android_stream_lib PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
    ${nlohmann_json_SOURCE_DIR}/include
    ${lz4_fetch_SOURCE_DIR}/lib
)

# Dependências da biblioteca unificada
target_link_libraries(android_stream_lib PUBLIC
    # Externas
    OpenSSL::SSL
    OpenSSL::Crypto
    ZLIB::ZLIB
    SQLite::SQLite3
    jwt-cpp
    nlohmann_json::nlohmann_json
    lz4::lz4
    libzip::zip
    usockets      # Dependência do uWebSockets
    uWebSockets
    # Plataforma
    ${PLATFORM_LIBS}
)

# Dependências opcionais
target_link_libraries(android_stream_lib PUBLIC lz4::lz4)
if(TARGET libzip::zip)
find_package(libprocps QUIET)
if(LIBPROCPS_FOUND)
    target_link_libraries(android_stream_lib PUBLIC ${LIBPROCPS_LIBRARIES})
    target_include_directories(android_stream_lib PUBLIC ${LIBPROCPS_INCLUDE_DIRS})
endif()
endif()

# --- Executáveis ---

# Dashboard Qt
if(BUILD_DASHBOARD)
    find_package(Qt6 COMPONENTS Core Widgets Network Multimedia WebSockets)
    find_package(PkgConfig)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(AVCODEC REQUIRED libavcodec)
        pkg_check_modules(AVUTIL REQUIRED libavutil)
        pkg_check_modules(SWSCALE REQUIRED libswscale)
    endif()

    if(Qt6_FOUND)
        set(CMAKE_AUTOMOC ON)
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
        target_include_directories(android_stream_dashboard PRIVATE
            ${AVCODEC_INCLUDE_DIRS}
            ${AVUTIL_INCLUDE_DIRS}
            ${SWSCALE_INCLUDE_DIRS}
        )
        target_link_libraries(android_stream_dashboard PRIVATE
            android_stream_lib
            Qt6::Core
            Qt6::Widgets
            Qt6::Network
            Qt6::Multimedia
            Qt6::WebSockets
            ${AVCODEC_LIBRARIES}
            ${AVUTIL_LIBRARIES}
            ${SWSCALE_LIBRARIES}
        )
        if(UNIX AND NOT APPLE)
            target_compile_definitions(android_stream_dashboard PRIVATE QT_NO_FOREACH)
        endif()
    else()
        message(WARNING "Qt6 not found - dashboard will not be built")
        set(BUILD_DASHBOARD OFF)
    endif()
endif()

# APK Builder
if(BUILD_APK_BUILDER)
    add_executable(apk_builder builder/main.cpp)
    target_link_libraries(apk_builder PRIVATE
        android_stream_lib
        ZLIB::ZLIB
        ${PLATFORM_LIBS}
    )
    if(TARGET libzip::zip)
        target_link_libraries(apk_builder PRIVATE libzip::zip) # libzip é usado aqui
    endif()
endif()

# Stream Server
if(BUILD_SERVER)
    add_executable(stream_server server/main.cpp)
    target_link_libraries(stream_server PRIVATE
        android_stream_lib
        ${PLATFORM_LIBS}
    )
endif()

# --- Testes ---
if(BUILD_TESTS)
    enable_testing()

    # Encontrar Google Test (com fallback para FetchContent)
    find_package(GTest)
    if(NOT GTest_FOUND)
        message(STATUS "GTest not found. Fetching from source.")
        FetchContent_Declare(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG release-1.12.1
        )
        FetchContent_MakeAvailable(googletest)
    endif()

    add_executable(unit_tests
        tests/test_system_manager.cpp
        tests/test_apk_builder.cpp
        tests/test_stream_optimizer.cpp
    )
    target_link_libraries(unit_tests PRIVATE
        android_stream_lib
        GTest::GTest
        GTest::Main
    )
    add_test(NAME unit_tests COMMAND unit_tests)

    # Teste de streaming

    add_executable(test_streaming tests/test_streaming.cpp) # Corrigido o caminho do arquivo de teste
    target_link_libraries(test_streaming PRIVATE android_stream_lib)
endif()

# --- Instalação ---
install(TARGETS android_stream_lib DESTINATION ${CMAKE_INSTALL_LIBDIR})

if(BUILD_DASHBOARD AND Qt6_FOUND)
    install(TARGETS android_stream_dashboard DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

if(BUILD_APK_BUILDER)
    install(TARGETS apk_builder DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

if(BUILD_SERVER)
    install(TARGETS stream_server DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

install(DIRECTORY shared/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/android-stream-manager
    FILES_MATCHING PATTERN "*.h")

install(FILES config/system_config.json
    DESTINATION etc/android-stream-manager/)

# --- Configuração de CPack para pacotes ---
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