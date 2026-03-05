cmake_minimum_required(VERSION 3.16)
project(StrategyGame VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find SFML 3.0
find_package(SFML 3.0 COMPONENTS Graphics Window System REQUIRED)

# Source files
set(SOURCES
    src/main.cpp
    src/Game.cpp
    src/Map.cpp
    src/Entity.cpp
    src/EntityData.cpp
    src/Unit.cpp
    src/Worker.cpp
    src/Soldier.cpp
    src/Brute.cpp
    src/Building.cpp
    src/Player.cpp
    src/InputHandler.cpp
    src/Renderer.cpp
    src/ResourceManager.cpp
    src/AIController.cpp
    src/ActionBar.cpp
)

# Header files
set(HEADERS
    include/Game.h
    include/Map.h
    include/Entity.h
    include/Unit.h
    include/Worker.h
    include/Soldier.h
    include/Brute.h
    include/Building.h
    include/Player.h
    include/InputHandler.h
    include/Renderer.h
    include/ResourceManager.h
    include/AIController.h
    include/Constants.h
    include/Types.h
    include/EntityData.h
    include/ActionBar.h
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/include)

# Link SFML 3.0
target_link_libraries(${PROJECT_NAME} PRIVATE SFML::Graphics SFML::Window SFML::System)

# Copy SFML DLLs on Windows
if(WIN32 AND BUILD_SHARED_LIBS)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SFML::Graphics>
        $<TARGET_FILE:SFML::Window>
        $<TARGET_FILE:SFML::System>
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
    )
endif()
