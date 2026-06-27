set(PROJECT_LIBS hal bsp utility picolibc)
set(PLATFORM "watch_v1_0" CACHE STRING "Target hardware variant")
set(PLATFORM_DIR "${CMAKE_SOURCE_DIR}/platforms/${PLATFORM}")
