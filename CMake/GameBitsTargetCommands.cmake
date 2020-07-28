## Copyright (c) 2020 John Pursey
##
## Use of this source code is governed by an MIT-style License that can be found
## in the LICENSE file or at https://opensource.org/licenses/MIT.

###############################################################################
## Target generation functions
##
## For a given target name "Target", the following variables can be set
##  VS_FOLDER			      Set to the visual studio folder the target should be in
##	Target_SOURCE		    Source files to build for the target
##	Target_TEST_SOURCE	Source files to build unit tests
##	Target_DEPS			    Target dependencies
##	Target_LIBS			    External libraries for this target
###############################################################################

function(gb_set_target_properties NAME)
	if(DEFINED VS_FOLDER)
		set_target_properties(${NAME} PROPERTIES FOLDER ${VS_FOLDER})
	endif()
	target_include_directories(${NAME} PUBLIC "${GB_DIR}/src" ${GB_INCLUDE_DIRS})
	if(DEFINED ${NAME}_INCLUDES)
		target_include_directories(${NAME} PUBLIC ${${NAME}_INCLUDES})
	endif()
	if(DEFINED ${NAME}_DEFINES)
		target_compile_definitions(${NAME} PUBLIC NOMINMAX ${${NAME}_DEFINES})
	else()
		target_compile_definitions(${NAME} PUBLIC NOMINMAX)
	endif()
	if(DEFINED ${NAME}_DEPS)
		add_dependencies(${NAME} ${${NAME}_DEPS})
		target_link_libraries(${NAME} ${${NAME}_DEPS})
	endif()
	if(DEFINED ${NAME}_LIBS)
		target_link_libraries(${NAME} ${${NAME}_LIBS})
	endif()
	
	# The version of ABSL synced generates some warnings for C++17 in VS2019.
	# This disables those warnings. Since these often come up in headers,
	# visibility must be PUBLIC 
	target_compile_definitions(${NAME} PUBLIC 
		_SILENCE_CXX17_OLD_ALLOCATOR_MEMBERS_DEPRECATION_WARNING
	) 
endfunction()

function(gb_add_test NAME)
	add_executable(${NAME}_test ${${NAME}_TEST_SOURCE})

	target_include_directories(${NAME}_test PRIVATE
		"${GB_THIRD_PARTY_DIR}/googletest/googlemock/include"
		"${GB_THIRD_PARTY_DIR}/googletest/googletest/include"
	)

	add_dependencies(${NAME}_test ${NAME})
	target_link_libraries(${NAME}_test
		${NAME}
		"${GB_BUILD_DIR}/third_party/googletest/lib/$(Configuration)/gmock_main$<$<CONFIG:Debug>:d>.lib"
	)
	if(DEFINED ${NAME}_TEST_DEPS)
		add_dependencies(${NAME}_test ${${NAME}_TEST_DEPS})
		target_link_libraries(${NAME}_test
			${${NAME}_TEST_DEPS}
		)
	endif()
	if(DEFINED ${NAME}_TEST_LIBS)
		target_link_libraries(${NAME}_test ${${NAME}_TEST_LIBS})
	endif()

	if(DEFINED VS_FOLDER)
		set_target_properties(${NAME}_test PROPERTIES FOLDER ${VS_FOLDER}/Tests)
	else()
		set_target_properties(${NAME}_test PROPERTIES FOLDER Tests)
	endif()
	set_target_properties(${NAME}_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${GB_BUILD_DIR}/tests")

	add_test(${NAME}_test ${NAME}_test)
	set_tests_properties(${NAME}_test PROPERTIES WORKING_DIRECTORY "${GB_BUILD_DIR}/tests")
endfunction()

function(gb_add_library NAME)
	add_library(${NAME} ${${NAME}_SOURCE})
	gb_set_target_properties(${NAME})
	if(DEFINED ${NAME}_TEST_SOURCE)
		gb_add_test(${NAME})
	endif()
endfunction()

function(gb_add_executable NAME)
	add_executable(${NAME} ${${NAME}_SOURCE})
	gb_set_target_properties(${NAME})
	if(DEFINED ${NAME}_TEST_SOURCE)
		gb_add_test(${NAME})
	endif()
	set_property(TARGET ${NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${GB_BIN_DIR}")
endfunction()

function(gb_add_win_executable NAME)
	add_executable(${NAME} WIN32 ${${NAME}_SOURCE})
	gb_set_target_properties(${NAME})
	if(DEFINED ${NAME}_TEST_SOURCE)
		gb_add_test(${NAME})
	endif()
	set_property(TARGET ${NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${GB_BIN_DIR}")
endfunction()
