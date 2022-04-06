cmake_minimum_required(VERSION 3.13)
project(project-template VERSION 0.1)

message(STATUS "building ${CMAKE_PROJECT_NAME}")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

if (NOT CMAKE_BUILD_TYPE)
	message(WARNING "CMAKE_BUILD_TYPE not set, defaulting to Release")
	set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS "-Wall -Wnon-virtual-dtor -Wl,--export-dynamic")
if (EMSCRIPTEN)
	# emcc doesn't recognize -Og
	set(CMAKE_CXX_FLAGS_DEBUG   "-DGAME_BUILD_DEBUG=1 -Os")
	set(CMAKE_CXX_FLAGS_RELEASE "-DGAME_BUILD_RELEASE=1 -Os")
else()
	set(CMAKE_CXX_FLAGS_DEBUG   "-DGAME_BUILD_DEBUG=1 -Og -g")
	set(CMAKE_CXX_FLAGS_RELEASE "-DGAME_BUILD_RELEASE=1 -O2")
endif()
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# TODO: remove this, I think portable build will be the only build
#       except maybe on android?
# TODO: reimplement precompiled headers
#       (is there a way to get )
option(PORTABLE_BUILD      "Portable build, include all dependancies in the install" ON)
option(PRECOMPILED_HEADERS "Build with precompiled headers (experimental)"           OFF)

include(GNUInstallDirs)
include(sources.cmake)

if (EXISTS ${PROJECT_SOURCE_DIR}/grend)
	message(STATUS "Found grend subdirectory, using that as library")
	set(GREND_PATH ./grend)
endif()

if (EXISTS ${PROJECT_SOURCE_DIR}/bullet)
	message(STATUS "Found bullet subdirectory, using that as library")
	set(BULLET_PHYSICS_SOURCE_DIR "${PROJECT_SOURCE_DIR}/bullet")
endif()

if (GREND_PATH)
	message(STATUS "GREND_PATH: ${GREND_PATH}")
	add_subdirectory(${GREND_PATH})
endif()

if (BULLET_PHYSICS_SOURCE_DIR)
	message(STATUS "BULLET_PHYSICS_SOURCE_DIR: ${BULLET_PHYSICS_SOURCE_DIR}")
	add_subdirectory(${BULLET_PHYSICS_SOURCE_DIR})
endif()

set (DEMO_LINK_LIBS)
set (DEMO_LINK_OPTIONS)
set (DEMO_PREFIX)

if (GREND_PATH)
	# everything needed should be pulled in here
	find_library(Grend Grend)
	list(APPEND DEMO_LINK_LIBS Grend)

else()
	if (ANDROID OR EMSCRIPTEN)
		message(ERROR "must have grend subdirectory")
	endif()

	# shared library
	# (TODO: would it be better to just include the lib with cmake?
	#        probably)
	find_package(PkgConfig REQUIRED)
	pkg_check_modules(Grend REQUIRED IMPORTED_TARGET grend)
	list(APPEND DEMO_LINK_LIBS PkgConfig::Grend)
endif()

message(STATUS "CMake host: ${CMAKE_HOST_SYSTEM_NAME}")

if (ANDROID)
	include(cmake-lib/android.cmake)

elseif (EMSCRIPTEN)
	include(cmake-lib/emscripten.cmake)

elseif (MINGW OR WIN32 OR CYGWIN)
	include(cmake-lib/windows.cmake)

else()
	# otherwise assume unixen
	include(cmake-lib/unixen.cmake)
endif()

### TODO: configurable data path, seperate stages of data
install(DIRECTORY assets DESTINATION ${CMAKE_INSTALL_DATADIR}/proj)

target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC "${PROJECT_BINARY_DIR}")
target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC src)
target_include_directories(${CMAKE_PROJECT_NAME} PUBLIC Grend)
target_link_libraries(${CMAKE_PROJECT_NAME} ${DEMO_LINK_LIBS})
target_link_options(${CMAKE_PROJECT_NAME} PUBLIC ${DEMO_LINK_OPTIONS})

# TODO: redo, ideally autogenerate
#if (PRECOMPILED_HEADERS)
#target_precompile_headers(${CMAKE_PROJECT_NAME} PRIVATE
	# this is the way described in the cmake documentation to limit
	# a header to one language... jeeeeeez this is terrible man
	#"$<$<COMPILE_LANGUAGE:CXX>:<components/area.hpp$<ANGLE-R>>"
	#...
#)
#endif()

include(cmake-lib/portable-install.cmake)
target_compile_options(${CMAKE_PROJECT_NAME} PUBLIC -DDEMO_PREFIX="./share/proj/")
