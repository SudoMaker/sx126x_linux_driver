cmake_minimum_required(VERSION 3.4)
project(sx126x_linux_driver)

set(CMAKE_CXX_STANDARD 17)

set(CPM_DOWNLOAD_VERSION 0.27.2)
set(CPM_DOWNLOAD_LOCATION "${CMAKE_BINARY_DIR}/cmake/CPM_${CPM_DOWNLOAD_VERSION}.cmake")

if(NOT (EXISTS ${CPM_DOWNLOAD_LOCATION}))
    message(STATUS "Downloading CPM.cmake")
    file(DOWNLOAD https://github.com/TheLartians/CPM.cmake/releases/download/v${CPM_DOWNLOAD_VERSION}/CPM.cmake ${CPM_DOWNLOAD_LOCATION})
endif()

include(${CPM_DOWNLOAD_LOCATION})

CPMAddPackage(
        NAME sx126x_driver
        GITHUB_REPOSITORY YukiWorkshop/sx126x_driver
        VERSION 0.0.1
)

CPMAddPackage(
        NAME GPIOPlusPlus
        GITHUB_REPOSITORY YukiWorkshop/GPIOPlusPlus
        VERSION 0.0.2
)

CPMAddPackage(
        NAME SPPI
        GITHUB_REPOSITORY YukiWorkshop/SPPI
        VERSION 0.0.4
)

add_library(sx126x_linux_driver SX126x_Linux.cpp SX126x_Linux.hpp)
target_include_directories(sx126x_linux_driver PUBLIC ${sx126x_driver_SOURCE_DIR} ${GPIOPlusPlus_SOURCE_DIR} ${SPPI_SOURCE_DIR})
target_link_libraries(sx126x_linux_driver pthread GPIOPlusPlus SPPI sx126x_driver)

add_executable(SX126x_Test Test.cpp)
target_link_libraries(SX126x_Test sx126x_linux_driver)