add_executable(bsdl_include_test include_test.cpp)
target_link_libraries(bsdl_include_test PRIVATE BSDL::BSDL)

add_test(NAME bsdl.include COMMAND bsdl_include_test)

add_executable(bsdl_png_test png_test.cpp ../bsdltool/png.cpp)
target_link_libraries(bsdl_png_test PRIVATE BSDL::BSDL ZLIB::ZLIB)
add_test(NAME bsdltool.png
	COMMAND bsdl_png_test "${CMAKE_CURRENT_BINARY_DIR}/png-test.png")

option(BSDL_UPDATE_REFERENCES "Update bsdltool PNG reference images while running CTest" OFF)
set(BSDLTOOL_TEST_RESOLUTION 256)
set(BSDLTOOL_TEST_DIFF_THRESHOLD 0.001)

include("${CMAKE_CURRENT_SOURCE_DIR}/test_manifest.cmake")
set(BSDLTOOL_TEST_MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/bsdltool_tests.ini")
bsdl_read_test_manifest("${BSDLTOOL_TEST_MANIFEST}" bsdltool_tests)
list(LENGTH bsdltool_tests_NAMES bsdltool_test_count)
math(EXPR bsdltool_test_last "${bsdltool_test_count} - 1")
foreach(index RANGE ${bsdltool_test_last})
	list(GET bsdltool_tests_NAMES ${index} test_name)
	list(GET bsdltool_tests_ARGUMENTS ${index} command_line)
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
			"-DTEST_MANIFEST=${BSDLTOOL_TEST_MANIFEST}"
			"-DTESTS_MD=${CMAKE_CURRENT_BINARY_DIR}/TESTS.md"
			"-DSOURCE_TESTS_MD=${CMAKE_CURRENT_SOURCE_DIR}/TESTS.md"
			-P "${CMAKE_CURRENT_SOURCE_DIR}/run_bsdltool_test.cmake"
	)
endforeach()
