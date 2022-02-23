cmake_minimum_required(VERSION 3.15)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 14)

project(cutil)

add_executable(lsdir lsdir.cpp)

string(TOLOWER ${CMAKE_HOST_SYSTEM_PROCESSOR} PLOC)
string(TOLOWER ${CMAKE_HOST_SYSTEM_NAME}-${PLOC} PLATNAME)

if (WIN32)

add_custom_command(
  TARGET lsdir POST_BUILD
  COMMAND powershell compress-archive 
    -Path ${CMAKE_BINARY_DIR}/lsdir.exe
    -DestinationPath ${CMAKE_BINARY_DIR}/lsdir_win${PLOC}.zip
  VERBATIM)

else (WIN32)
add_custom_command(
  TARGET lsdir POST_BUILD
  COMMAND mv ${CMAKE_BINARY_DIR}/lsdir ${CMAKE_BINARY_DIR}/lsdir_${PLATNAME} && gzip ${CMAKE_BINARY_DIR}/lsdir_${PLATNAME}
  VERBATIM)

endif (WIN32)
