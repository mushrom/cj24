message(STATUS "Setting standalone executable")
add_executable(${CMAKE_PROJECT_NAME} ${PROJECT_SOURCE})
install(TARGETS ${CMAKE_PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR})

message(STATUS "Building for/on windows")
#add_compile_options(-lopengl32)
# XXX: should be part of grend target compile options...
add_compile_options(-D_WIN32)
find_library(opengl32 opengl32)
list(APPEND DEMO_LINK_LIBS opengl32)
