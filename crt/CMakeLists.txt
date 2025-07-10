cmake_minimum_required(VERSION 3.16)
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -ffast-math")

project(crt LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(${PROJECT_NAME} main.cpp)
# CImg
target_include_directories(${PROJECT_NAME} PRIVATE "include")

# Required for CImg
# Uncomment if you plan to save images
find_package(X11 REQUIRED)
find_package(OpenMP REQUIRED)
include_directories(${X11_INCLUDE_DIR})
# Same here
target_link_libraries(${PROJECT_NAME} PRIVATE ${X11_LIBRARIES} OpenMP::OpenMP_CXX)

# GLM
find_package(glm CONFIG REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE glm::glm)
