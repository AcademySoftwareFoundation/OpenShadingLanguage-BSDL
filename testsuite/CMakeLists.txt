add_executable(bsdl_include_test include_test.cpp)
target_link_libraries(bsdl_include_test PRIVATE BSDL::BSDL)

add_test(NAME bsdl.include COMMAND bsdl_include_test)

option(BSDL_UPDATE_REFERENCES "Update bsdltool PNG reference images while running CTest" OFF)
set(BSDLTOOL_TEST_RESOLUTION 256)
set(BSDLTOOL_TEST_DIFF_THRESHOLD 0.001)

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/bsdltool_tests.txt" bsdltool_test_entries)
foreach(entry IN LISTS bsdltool_test_entries)
	string(STRIP "${entry}" entry)
	if(entry STREQUAL "" OR entry MATCHES "^#")
		continue()
	endif()

	string(FIND "${entry}" "|" separator)
	if(separator LESS 1)
		message(FATAL_ERROR
			"Invalid bsdltool test entry '${entry}'. Expected: name | bsdltool arguments")
	endif()
	string(SUBSTRING "${entry}" 0 ${separator} test_name)
	math(EXPR arguments_start "${separator} + 1")
	string(SUBSTRING "${entry}" ${arguments_start} -1 command_line)
	string(STRIP "${test_name}" test_name)
	string(STRIP "${command_line}" command_line)
	separate_arguments(command_arguments UNIX_COMMAND "${command_line}")

	set(output "${CMAKE_CURRENT_BINARY_DIR}/results/${test_name}.png")
	set(reference "${CMAKE_CURRENT_SOURCE_DIR}/references/${test_name}.png")
	add_test(
		NAME "${test_name}"
		COMMAND "${CMAKE_COMMAND}"
			"-DBSDLTOOL=$<TARGET_FILE:bsdltool>"
			"-DOUTPUT=${output}"
			"-DREFERENCE=${reference}"
			"-DRESOLUTION=${BSDLTOOL_TEST_RESOLUTION}"
			"-DTHRESHOLD=${BSDLTOOL_TEST_DIFF_THRESHOLD}"
			"-DARGS=${command_arguments}"
			"-DUPDATE_REFERENCES=${BSDL_UPDATE_REFERENCES}"
			"-DTEST_MANIFEST=${CMAKE_CURRENT_SOURCE_DIR}/bsdltool_tests.txt"
			"-DTESTS_MD=${CMAKE_CURRENT_BINARY_DIR}/TESTS.md"
			"-DSOURCE_TESTS_MD=${CMAKE_CURRENT_SOURCE_DIR}/TESTS.md"
			-P "${CMAKE_CURRENT_SOURCE_DIR}/run_bsdltool_test.cmake"
	)
endforeach()
