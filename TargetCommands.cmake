###############################################################################
## Target generation functions
##
## For a given target name "Target", the following variables can be set
##  VS_FOLDER			Set to the visual studio folder the target should be in
##	Target_SOURCE		Source files to build for the target
##	Target_TEST_SOURCE	Source files to build unit tests
##	Target_DEPS			Target dependencies
##  Target_INCLUDES     External include directories for this target
##	Target_LIBS			External libraries for this target
###############################################################################

function(set_gbits_target_properties NAME)
	if(DEFINED VS_FOLDER)
		set_target_properties(${NAME} PROPERTIES FOLDER ${VS_FOLDER})
	endif()
	if(DEFINED ${NAME}_DEPS)
		add_dependencies(${NAME} ${${NAME}_DEPS})
		target_link_libraries(${NAME} ${${NAME}_DEPS})
	endif()
	target_include_directories(${NAME} PUBLIC ${GBITS_DIR}/src)
	if(DEFINED ${NAME}_INCLUDES)
		target_include_directories(${NAME} PUBLIC ${${NAME}_INCLUDES})
	endif()
	if(DEFINED ${NAME}_LIBS)
		target_link_libraries(${NAME} ${${NAME}_LIBS})
	endif()
endfunction()

function(add_gbits_test NAME)
	add_executable(${NAME}_test ${${NAME}_TEST_SOURCE})

	target_include_directories(${NAME}_test PRIVATE 
		${GBITS_DIR}/src
		${GBITS_GTEST_INCLUDE}
	)

	add_dependencies(${NAME}_test ${NAME})
	target_link_libraries(${NAME}_test
		${NAME}
		${GBITS_GTEST_LIB_MAIN}
	)

	if(DEFINED VS_FOLDER)
		set_target_properties(${NAME}_test PROPERTIES FOLDER ${VS_FOLDER}/Tests)
	else()
		set_target_properties(${NAME}_test PROPERTIES FOLDER Tests)
	endif()
	set_target_properties(${NAME}_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${GBITS_DIR}/build/tests)

	add_test(${NAME}_Test ${NAME}_test)
	set_tests_properties(${NAME}_Test PROPERTIES WORKING_DIRECTORY ${GBITS_DIR}/build/tests)
endfunction()

function(add_gbits_library NAME)
	add_library(${NAME} ${${NAME}_SOURCE})
	set_gbits_target_properties(${NAME})
	if(DEFINED ${NAME}_TEST_SOURCE)
		add_gbits_test(${NAME})
	endif()
endfunction()

function(add_gbits_executable NAME)
	add_executable(${NAME} ${${NAME}_SOURCE})
	set_gbits_target_properties(${NAME})
	if(DEFINED ${NAME}_TEST_SOURCE)
		add_gbits_test(${NAME})
	endif()
endfunction()

function(add_gbits_win_executable NAME)
	add_executable(${NAME} WIN32 ${${NAME}_SOURCE})
	set_gbits_target_properties(${NAME})
	if(DEFINED ${NAME}_TEST_SOURCE)
		add_gbits_test(${NAME})
	endif()
endfunction()
