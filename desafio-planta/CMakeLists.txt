cmake_minimum_required(VERSION 3.16)
project(desafio_planta CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

# raylib 5.5 é baixado automaticamente na primeira compilação (precisa de internet).
include(FetchContent)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
  raylib
  GIT_REPOSITORY https://github.com/raysan5/raylib.git
  GIT_TAG 5.5
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(raylib)

add_executable(desafio_planta src/main.cpp src/serial.cpp src/ble.cpp)
target_link_libraries(desafio_planta PRIVATE raylib)
if(WIN32)
  # Bluetooth LE nativo do Windows (C++/WinRT, vem com o Windows SDK)
  target_link_libraries(desafio_planta PRIVATE WindowsApp)
  if(MSVC)
    set_source_files_properties(src/ble.cpp PROPERTIES COMPILE_OPTIONS "/permissive-;/EHsc")
  endif()
endif()

# copia a pasta assets para ao lado do executável
add_custom_command(TARGET desafio_planta POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:desafio_planta>/assets)

# testes da lógica (não precisam de raylib): ctest
enable_testing()
add_executable(test_game tests/test_game.cpp)
add_test(NAME logica_do_jogo COMMAND test_game)
