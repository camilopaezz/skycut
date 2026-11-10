cmake_minimum_required(VERSION 3.16)
project(CameraCut C CXX)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(CTest)
if(BUILD_TESTING)
  add_executable(job_send_test src/Job.cpp src/PltGuides.cpp src/CorelGuides.cpp tests/job_send.cpp)
  target_include_directories(job_send_test BEFORE PRIVATE tests/ipc_stub include)
  add_test(NAME job_send COMMAND job_send_test "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
  add_executable(ipc_send_test src/Ipc.cpp tests/ipc_send.cpp)
  target_include_directories(ipc_send_test BEFORE PRIVATE tests/ipc_stub)
  add_test(NAME ipc_send COMMAND ipc_send_test)
  add_executable(plt_guides_test src/PltGuides.cpp src/CorelGuides.cpp tests/plt_guides_native.cpp)
  target_include_directories(plt_guides_test PRIVATE include)
  add_test(NAME plt_guides COMMAND plt_guides_test "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
endif()
if(NOT WIN32)
  return()
endif()

# MinGW i686 is a secondary toolchain; MSVC is primary.

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")

add_compile_definitions(UNICODE _UNICODE WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/res)

add_executable(CameraCut WIN32
  src/WinMain.cpp
  src/Dialog.cpp
  src/Log.cpp
  src/Paths.cpp
  src/Ipc.cpp
  src/Job.cpp
  src/PltGuides.cpp src/CorelGuides.cpp
  src/Cfg.cpp
  src/Mch.cpp
  src/Core.cpp
  res/CameraCut.rc)

target_link_libraries(CameraCut PRIVATE
  user32
  kernel32
  shell32
  advapi32
  comctl32
  shlwapi
  ole32)

if(MSVC)
  target_compile_options(CameraCut PRIVATE /W3)
  # Embed the manifest from CameraCut.rc (RT_MANIFEST); do not let the linker invent one.
  target_link_options(CameraCut PRIVATE /MANIFEST:NO)
endif()

add_executable(sendmsg tools/sendmsg.c)
target_link_libraries(sendmsg PRIVATE user32)

add_executable(fake_mch WIN32 tools/fake_mch.c)
target_link_libraries(fake_mch PRIVATE user32)
