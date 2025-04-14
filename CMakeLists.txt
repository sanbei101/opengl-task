cmake_minimum_required(VERSION 3.10)
project(OpenGLSpheresSingleFile)

set(CMAKE_CXX_STANDARD 11)

# --- 依赖 ---
add_library(glad STATIC glad.c glad/glad.h glad/KHR/khrplatform.h)
target_include_directories(glad PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/glad)

find_package(glfw3 3.3 REQUIRED)
find_package(glm REQUIRED)

# --- 可执行文件 ---
add_executable(task1 task1/task1.cpp)

# --- 链接 ---
target_link_libraries(task1 PRIVATE glad glfw)

if (TARGET glm::glm)
    target_link_libraries(task1 PRIVATE glm::glm)
elseif(GLM_FOUND AND NOT TARGET glm::glm)
    target_include_directories(task1 PRIVATE ${GLM_INCLUDE_DIRS})
endif()

if(APPLE)
    target_link_libraries(task1 PRIVATE "-framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo")
elseif(UNIX AND NOT APPLE)
    find_package(OpenGL REQUIRED)
    target_link_libraries(task1 PRIVATE OpenGL::GL)
endif()

# --- 确保 glad 的头文件路径传递给 task1 ---
target_include_directories(task1 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/glad)