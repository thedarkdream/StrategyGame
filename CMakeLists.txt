cmake_minimum_required(VERSION 3.16)
project(StrategyGame VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find SFML 3.0
find_package(SFML 3.0 COMPONENTS Graphics Window System Audio REQUIRED)

# 3D rendering: math (GLM), GL function loader (GLEW), system OpenGL
find_package(glm CONFIG REQUIRED)
find_package(GLEW REQUIRED)
find_package(OpenGL REQUIRED)

# glTF model loading
find_package(fastgltf CONFIG REQUIRED)

# Sources: every .cpp / .h under src/ (one folder per module, see README.md).
# CONFIGURE_DEPENDS makes the build re-glob when files are added or removed.
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/*.cpp)
file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/*.h)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Link SFML 3.0
target_link_libraries(${PROJECT_NAME} PRIVATE SFML::Graphics SFML::Window SFML::System SFML::Audio
    glm::glm GLEW::GLEW OpenGL::GL fastgltf::fastgltf)

# Debug flags
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(${PROJECT_NAME} PRIVATE -g3 -O0 -gdwarf-4)
endif()

# Copy SFML DLLs on Windows
if(WIN32 AND BUILD_SHARED_LIBS)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SFML::Graphics>
        $<TARGET_FILE:SFML::Window>
        $<TARGET_FILE:SFML::System>
        $<TARGET_FILE:SFML::Audio>
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
    )
endif()
