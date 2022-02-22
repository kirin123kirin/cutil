cmake_minimum_required(VERSION 3.15)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 14)

project(cutil)

add_executable(lsdir lsdir.cpp)

if (WIN32)
add_custom_command(
  TARGET lsdir POST_BUILD
  COMMAND powershell compress-archive 
    -Path ${CMAKE_BINARY_DIR}/lsdir.exe
    -DestinationPath ${CMAKE_BINARY_DIR}/lsdir.zip
  VERBATIM)

else (WIN32)
add_custom_command(
  TARGET lsdir POST_BUILD
  COMMAND gzip ${CMAKE_BINARY_DIR}/lsdir
  VERBATIM)

endif (WIN32)
