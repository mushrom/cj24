if (PORTABLE_BUILD)
	configure_file(run/run-linux64.sh.in run-linux64.sh @ONLY)

	install(FILES ${PROJECT_BINARY_DIR}/run-linux64.sh
	        DESTINATION ${CMAKE_INSTALL_PREFIX}
	        PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ
	                    GROUP_EXECUTE GROUP_READ)
	# TODO: should also output a launcher script
	install(CODE "set(CMAKE_INSTALL_LIBDIR \"${CMAKE_INSTALL_LIBDIR}\")")
	install(CODE "set(CMAKE_PROJECT_NAME   \"${CMAKE_PROJECT_NAME}\")")
	install(CODE
	[[
		#include(BundleUtilities)
		#	fixup_bundle("landscape-test" "" "asdf")


		#file(GET_RUNTIME_DEPENDENCIES
		#	RESOLVED_DEPENDENCIES_VAR   runtime_libs
		#	UNRESOLVED_DEPENDENCIES_VAR unresolved_libs
		#	EXECUTABLES ${CMAKE_PROJECT_NAME}
		#	# have to bring your own libGL
		#	PRE_EXCLUDE_REGEXES ".*/lib32/.*"
		#	POST_EXCLUDE_REGEXES ".*libGL.so" ".*opengl32.dll"
		#)

		# https://discourse.cmake.org/t/file-get-runtime-dependencies-issues/2574
		# ugh
		execute_process(COMMAND ldd ${CMAKE_PROJECT_NAME} OUTPUT_VARIABLE ldd_out)
		string (REPLACE "\n" ";" lines "${ldd_out}")
		foreach (line ${lines})
			string (REGEX REPLACE "^.* => | \(.*\)" "" pruned "${line}")
			string (STRIP "${pruned}" dep)
			message("LIB: ${pruned} => ${dep}")
			if (${dep} MATCHES ".*libGL.*so.*|.*opengl32.dll")
				continue()
			endif()

			if (IS_ABSOLUTE ${dep})
				message("ADDED: ${pruned} => ${dep}")
				list (APPEND runtime_libs ${dep})
			endif()
		endforeach()

		message("RESOLVED:   ${runtime_libs}")
		message("UNRESOLVED: ${unresolved_libs}")

		foreach(_lib ${runtime_libs})
			file(INSTALL
				DESTINATION "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}"
				TYPE SHARED_LIBRARY
				FOLLOW_SYMLINK_CHAIN
				FILES "${_lib}")
		endforeach()
	]])
endif()

