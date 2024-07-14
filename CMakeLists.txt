cmake_minimum_required(VERSION 3.10)
project(OpenGLdiff VERSION 1.0)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

find_package(OpenGL REQUIRED)
find_package(glfw3 3.3 REQUIRED)
find_package(GLEW REQUIRED)


include_directories(${OPENGL_INCLUDE_DIRS} ${GLEW_INCLUDE_DIRS} ./include)

add_executable(OpenGLdiff main.cpp)
target_link_libraries(OpenGLdiff ${OPENGL_LIBRARIES} glfw GL ${GLEW_LIBRARIES})

