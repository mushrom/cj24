message(STATUS "Setting standalone executable")
add_executable(${CMAKE_PROJECT_NAME} ${PROJECT_SOURCE})
install(TARGETS ${CMAKE_PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR})

# pthreads on unixen
message(STATUS "Building for/on unixen, adding -pthread")
list(APPEND DEMO_LINK_OPTIONS -pthread)
