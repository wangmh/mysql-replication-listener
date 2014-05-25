project (mysql-replication-listener)
cmake_minimum_required (VERSION 2.8)

set(MySQL_BINLOG_VERSION_MAJOR "0")
set(MySQL_BINLOG_VERSION_MINOR "2")
set(MySQL_BINLOG_VERSION_PATCH "0")
set(MySQL_BINLOG_VERSION "${MySQL_BINLOG_VERSION_MAJOR}.${MySQL_BINLOG_VERSION_MINOR}.${MySQL_BINLOG_VERSION_PATCH}")

set(CMAKE_VERSION_STRING "${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION}")

# General settings
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)

include_directories(include)
link_directories(${PROJECT_BINARY_DIR}/lib)

find_package(Boost)
if(Boost_FOUND)
    include_directories(Boost_INCLUDE_DIRS)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DHAVE_BOOST")
else(Boost_FOUND)
    message(INFO "No Boost found, will not use Boost")
endif(Boost_FOUND)

# --------- Find crypt ------------------------------------------
FIND_LIBRARY(LIB_CRYPTO crypto /opt/local/lib /opt/lib /usr/lib /usr/local/lib)

add_subdirectory(src)

include(InstallRequiredSystemLibraries)

# installation configuration
install(DIRECTORY include DESTINATION . FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp" PATTERN "*.ipp")

