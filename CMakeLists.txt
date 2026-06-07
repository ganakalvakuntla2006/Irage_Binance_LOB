cmake_minimum_required(VERSION 3.22)
project(BinanceLobCapture CXX)

# Suppressing modern CMake policy warnings regarding legacy FindBoost scripts
if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()

# Enforcing C++17 standard conformity
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enforcing strict compiler warnings and optimizations required for production trading engines
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O2 -pthread")

# Locate System Dependencies 
find_package(Boost REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(GTest REQUIRED)
include(GoogleTest)

include_directories(include ${Boost_INCLUDE_DIRS} ${OPENSSL_INCLUDE_DIR})

add_executable(binance_capture 
    src/main.cpp 
    src/websocket_client.cpp
    src/order_book.cpp
)

target_link_libraries(binance_capture 
    OpenSSL::Crypto 
    OpenSSL::SSL 
    pthread
)
# Test Suite Configuration 
enable_testing()

add_executable(unit_tests 
    tests/unit_tests.cpp
    src/order_book.cpp # Linking engine logic to the tests
)

target_link_libraries(unit_tests GTest::gtest_main)
gtest_discover_tests(unit_tests)