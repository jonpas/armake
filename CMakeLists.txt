#Change this if you need to target a specific CMake version

cmake_minimum_required(VERSION 3.2)
cmake_policy(SET CMP0015 NEW)

#set(CMAKE_USER_MAKE_RULES_OVERRIDE
#   ${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/c_flag_overrides.cmake)
#set(CMAKE_USER_MAKE_RULES_OVERRIDE_CXX
#   ${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/cxx_flag_overrides.cmake)

project(armake)

set(CMAKE_SUPPRESS_REGENERATION 1)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING
      "Choose the type of build, options are: Debug Release
RelWithDebInfo MinSizeRel."
      FORCE)
endif(NOT CMAKE_BUILD_TYPE)
set(CMAKE_CONFIGURATION_TYPES Release CACHE TYPE INTERNAL FORCE)
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake_modules" ${CMAKE_MODULE_PATH})

file(GLOB_RECURSE SOURCES src/*.c src/*.cpp src/*.h lib/*.c lib/*.cpp lib/*.h)

SET(OPENSSL_USE_STATIC_LIBS  TRUE CACHE BOOL "Use OpenSSL Static Libraries")
if(${WIN32})
    SET(OPENSSL_MSVC_STATIC_RT TRUE CACHE BOOL "Use OpenSSL Static RT")
endif()

if (CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(EXECUTABLE_NAME "armake_w32")
else()
    set(EXECUTABLE_NAME "armake_w64")
endif()
add_executable(${EXECUTABLE_NAME} ${SOURCES})


# -----------------------
# START STATIC LIBRARIES
# -----------------------
if(${UNIX})
     set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
endif()

if(${WIN32})
    add_definitions(-D _WIN32_WINNT=0x0600)
endif()

find_package(OpenSSL)
if (OPENSSL_FOUND)
    include_directories(${OPENSSL_INCLUDE_DIR})
else()
    message(FATAL_ERROR "\nOPENSSL not found\n")
endif()

find_package(BISON)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/src/)
if (BISON_FOUND)
    set (BISON_${EXECUTABLE_NAME}_COMPILE_FLAGS "--defines=${CMAKE_CURRENT_SOURCE_DIR}/src/rapify.tab.h")
    BISON_TARGET(${EXECUTABLE_NAME} ${CMAKE_CURRENT_SOURCE_DIR}/src/rapify.y ${CMAKE_CURRENT_SOURCE_DIR}/src/rapify.tab.c)
else()
    message(FATAL_ERROR "\nBISON not found\n")
endif()
find_package(FLEX)
if (FLEX_FOUND)
    FLEX_TARGET(${EXECUTABLE_NAME} ${CMAKE_CURRENT_SOURCE_DIR}/src/rapify.l ${CMAKE_CURRENT_SOURCE_DIR}/src/rapify.yy.c)
else()
    message(FATAL_ERROR "\nFLEX not found\n")
endif()

target_sources(${EXECUTABLE_NAME} PUBLIC ${BISON_${EXECUTABLE_NAME}_OUTPUTS})
target_sources(${EXECUTABLE_NAME} PUBLIC ${FLEX_${EXECUTABLE_NAME}_OUTPUTS})


# -----------------------
# END STATIC LIBRARIES
# -----------------------


if(${WIN32})
    # Windows
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /wd4996") # _CRT_SECURE_NO_WARNINGS
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4996") # _CRT_SECURE_NO_WARNINGS
    target_link_libraries(${EXECUTABLE_NAME} ${OPENSSL_LIBRARIES} Iphlpapi crypt32 wsock32 ws2_32)
    add_definitions(-DUNICODE -D_UNICODE -DWIN32_LEAN_AND_MEAN)
    SET_TARGET_PROPERTIES(${EXECUTABLE_NAME} PROPERTIES LINK_FLAGS " /MANIFEST:NO /ERRORREPORT:NONE")
else()
    # Linux
    target_link_libraries(${EXECUTABLE_NAME} -Wl,-Bstatic ${OPENSSL_LIBRARIES} -lcrypto -lm -Wl,-Bdynamic -ldl -pthread -lz)
    set(CMAKE_CXX_FLAGS "-std=c++0x -static-libstdc++ -static-libgcc ${CMAKE_CXX_FLAGS}")
endif()

mark_as_advanced(CMAKE_INSTALL_PREFIX)
mark_as_advanced(CMAKE_CONFIGURATION_TYPES)


message(STATUS "C_FLAGS:  =${CMAKE_C_FLAGS}")
message(STATUS "CMAKE_C_FLAGS_DEBUG:=${CMAKE_C_FLAGS_DEBUG}")
message(STATUS "CMAKE_C_FLAGS_RELEASE:=${CMAKE_C_FLAGS_RELEASE}")
message(STATUS "CMAKE_C_FLAGS_MINSIZEREL:=${CMAKE_C_FLAGS_MINSIZEREL}")
message(STATUS "CMAKE_C_FLAGS_RELWITHDEBINFO:=${CMAKE_C_FLAGS_RELWITHDEBINFO}")
message(STATUS "")
message(STATUS "")
message(STATUS "CXX_FLAGS:=${CMAKE_CXX_FLAGS}")
message(STATUS "CMAKE_CXX_FLAGS_DEBUG:=${CMAKE_CXX_FLAGS_DEBUG}")
message(STATUS "CMAKE_CXX_FLAGS_RELEASE:=${CMAKE_CXX_FLAGS_RELEASE}")
message(STATUS "CMAKE_CXX_FLAGS_MINSIZEREL:=${CMAKE_CXX_FLAGS_MINSIZEREL}")
message(STATUS "CMAKE_CXX_FLAGS_RELWITHDEBINFO:=${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
