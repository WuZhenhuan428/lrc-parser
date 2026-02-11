cmake_minimum_required(VERSION 3.14)
project(lrc-parser VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Iconv REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(UCHARDET REQUIRED uchardet IMPORTED_TARGET)

add_library(lrc_parser lrc_parser.cpp lrc_parser.h)

target_include_directories(lrc_parser
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(lrc_parser PUBLIC
    Iconv::Iconv
    PkgConfig::UCHARDET
)
