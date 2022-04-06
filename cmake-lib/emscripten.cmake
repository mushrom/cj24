message(STATUS "Setting standalone executable")
add_executable(${CMAKE_PROJECT_NAME} ${PROJECT_SOURCE})
install(TARGETS ${CMAKE_PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR})

message(STATUS "Building for emscripten")
# TODO: point to asset dir
set(EMSCRIPTEN_MORE_STUFF
	--preload-file=/tmp/webbuild/@/
	--shell-file=${PROJECT_SOURCE_DIR}/shell.html
)

set(EMSCRIPTEN_USE_STUFF
	-sUSE_SDL=2
	-sMIN_WEBGL_VERSION=2
	-sMAX_WEBGL_VERSION=2
	-sUSE_WEBGL2=1
	-sERROR_ON_UNDEFINED_SYMBOLS=0
	-sTOTAL_MEMORY=1048576000
	-sOFFSCREEN_FRAMEBUFFER=1
	#-sFULL_ES3=1
)

set(CMAKE_EXECUTABLE_SUFFIX ".html")

target_link_directories(${CMAKE_PROJECT_NAME} PUBLIC "${CMAKE_INSTALL_PREFIX}/lib")

#find_library(Grend Grend)
#find_library(bullet bullet)
#list(APPEND DEMO_LINK_LIBS Grend bullet)

#target_link_libraries(${CMAKE_PROJECT_NAME} PUBLIC ${DEMO_LINK_LIBS})
target_compile_options(${CMAKE_PROJECT_NAME} PUBLIC ${EMSCRIPTEN_USE_STUFF} ${EMSCRIPTEN_MORE_STUFF})
target_link_options(${CMAKE_PROJECT_NAME}  PUBLIC ${EMSCRIPTEN_USE_STUFF} ${EMSCRIPTEN_MORE_STUFF})
