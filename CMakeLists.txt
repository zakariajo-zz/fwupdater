cmake_minimum_required(VERSION 2.8)

if( NOT DEFINED CMAKE_TOOLCHAIN_FILE )
  if( DEFINED ENV{VITASDK} )
  set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
  else()
  message(FATAL_ERROR "Please define VITASDK to point to your SDK path!")
  endif()
endif()

project(fwupdater)
include("${VITASDK}/share/vita.cmake" REQUIRED)
set(VITA_APP_NAME "Firmware Updater")
set(VITA_TITLEID  "FWUPDATER")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -O3")

include_directories(
)

link_directories(
  ${CMAKE_CURRENT_BINARY_DIR}
)

add_executable(fwupdater
  main.c
  scr_printf.c
  font.c
)

target_link_libraries(fwupdater
  SceCtrl_stub
  SceDisplay_stub
  SceSblUpdateMgr_stub
  SceVshBridge_stub
)

vita_create_self(eboot.bin fwupdater UNSAFE)
vita_create_vpk(fwupdater.vpk ${VITA_TITLEID} eboot.bin
  VERSION ${VITA_VERSION}
  NAME ${VITA_APP_NAME}
)
