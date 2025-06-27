cmake_minimum_required(VERSION 3.10)
project(PersysAgent)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find packages
find_package(PkgConfig REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(CURL REQUIRED)
pkg_check_modules(JSONCPP REQUIRED jsoncpp)
pkg_check_modules(LIBUUID REQUIRED uuid)

# Include directories
include_directories(
    ${JSONCPP_INCLUDE_DIRS}
    ${LIBUUID_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
    ${CURL_INCLUDE_DIRS}
    /usr/local/include
    ${CMAKE_SOURCE_DIR}/src/controllers
    ${CMAKE_SOURCE_DIR}/src/routes
)

# Source files
set(SOURCE_FILES
    src/main.cpp
    src/controllers/ComposeController.cpp
    src/controllers/CronController.cpp
    src/controllers/DockerController.cpp
    src/controllers/NodeController.cpp
    src/controllers/SwarmController.cpp
    src/controllers/SystemController.cpp
    src/routes/HandshakeRoutes.cpp
    src/routes/DockerRoutes.cpp
    src/routes/ComposeRoutes.cpp
    src/routes/CronRoutes.cpp
    src/routes/SwarmRoutes.cpp
)

# Define executable
add_executable(PersysAgent ${SOURCE_FILES})

# Link libraries
target_link_libraries(PersysAgent
    PRIVATE
    ${JSONCPP_LIBRARIES}
    ${LIBUUID_LIBRARIES}
    OpenSSL::SSL
    OpenSSL::Crypto
    ${CURL_LIBRARIES}
)

# Print library paths for debugging
message(STATUS "JSONCPP Libraries: ${JSONCPP_LIBRARIES}")
message(STATUS "UUID Libraries: ${LIBUUID_LIBRARIES}")
message(STATUS "OpenSSL Libraries: ${OPENSSL_LIBRARIES}")
message(STATUS "CURL Libraries: ${CURL_LIBRARIES}")