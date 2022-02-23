cmake_minimum_required(VERSION 3.15)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 14)

project(cutil)

add_executable(lsdir lsdir.cpp)

if (WIN32)
  string(TOLOWER win-${CMAKE_HOST_SYSTEM_PROCESSOR} PLATNAME)
  set(OUTPUTFILE ${CMAKE_BINARY_DIR}/lsdir.exe)
else (WIN32)
  string(TOLOWER ${CMAKE_HOST_SYSTEM_NAME}-${CMAKE_HOST_SYSTEM_PROCESSOR} PLATNAME)
  set(OUTPUTFILE ${CMAKE_BINARY_DIR}/lsdir)
endif (WIN32)

add_custom_command(
  TARGET lsdir POST_BUILD
  COMMAND chmod a+x ${CMAKE_BINARY_DIR}/lsdir && tar czf ${CMAKE_BINARY_DIR}/lsdir_${PLATNAME}.tar.gz ${CMAKE_BINARY_DIR}/lsdir
  VERBATIM)

