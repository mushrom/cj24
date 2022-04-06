# everything needed should be pulled in here
#find_library(Grend Grend)
#list(APPEND DEMO_LINK_LIBS Grend)

message(STATUS "Setting library for android")
add_library(${CMAKE_PROJECT_NAME} SHARED ${PROJECT_SOURCE})
install(TARGETS ${CMAKE_PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_LIBDIR})
list(APPEND DEMO_LINK_LIBS SDL2 SDL2main)
