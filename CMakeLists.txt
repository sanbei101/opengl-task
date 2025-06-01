cmake_minimum_required(VERSION 3.10)
project(OpenGLTask)

set(CMAKE_CXX_STANDARD 11)

add_library(glad STATIC ${CMAKE_SOURCE_DIR}/glad.c)

target_include_directories(glad PUBLIC ${CMAKE_SOURCE_DIR})

find_package(glfw3 3.3 REQUIRED)
find_package(glm REQUIRED)
find_package(OpenGL REQUIRED)
include_directories(${OPENGL_INCLUDE_DIRS})
add_executable(task1 task1/task1.cpp)
add_executable(task2 task2/task2.cpp)
add_custom_command(TARGET task2
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/task2/pyramid_texture.jpg
        $<TARGET_FILE_DIR:task2>
)
add_executable(task3 task3/task3.cpp)
add_executable(task4 task4/task4.cpp)
target_link_libraries(task1 PRIVATE glad glfw ${OPENGL_LIBRARIES})
target_link_libraries(task2 PRIVATE glad glfw ${OPENGL_LIBRARIES})
target_link_libraries(task3 PRIVATE glad glfw ${OPENGL_LIBRARIES})
target_link_libraries(task4 PRIVATE glad glfw ${OPENGL_LIBRARIES})