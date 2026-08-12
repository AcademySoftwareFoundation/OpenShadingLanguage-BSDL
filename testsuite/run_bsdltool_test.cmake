if(NOT DEFINED BSDLTOOL OR NOT DEFINED OUTPUT OR NOT DEFINED REFERENCE OR NOT DEFINED RESOLUTION OR
    NOT DEFINED THRESHOLD OR NOT DEFINED TEST_MANIFEST OR NOT DEFINED TESTS_MD)
    message(FATAL_ERROR "BSDL test harness is missing required arguments")
endif()

string(TOUPPER "$ENV{BSDL_UPDATE_REFERENCES}" environment_update)
get_filename_component(output_directory "${OUTPUT}" DIRECTORY)
file(MAKE_DIRECTORY "${output_directory}")

function(write_tests_report)
    file(STRINGS "${TEST_MANIFEST}" test_entries)
    set(tests_md "# BSDL Image Tests\n\n| Status | Test | Rendered | Reference | Difference |\n| --- | --- | --- | --- | --- |\n")
    foreach(entry IN LISTS test_entries)
        string(STRIP "${entry}" entry)
        if(entry STREQUAL "" OR entry MATCHES "^#")
            continue()
        endif()
        string(FIND "${entry}" "|" separator)
        if(separator LESS 1)
            message(FATAL_ERROR "Invalid bsdltool test entry '${entry}'")
        endif()
        string(SUBSTRING "${entry}" 0 ${separator} test_name)
        string(STRIP "${test_name}" test_name)
        set(entry_status "<span style=\"color: gray\">NOT RUN</span>")
        set(entry_diff "")
        set(entry_status_file "${output_directory}/${test_name}.status")
        if(EXISTS "${entry_status_file}")
            file(READ "${entry_status_file}" recorded_status)
            if(recorded_status MATCHES "^PASS")
                set(entry_status "<span style=\"color: green\">PASS</span>")
            elseif(recorded_status MATCHES "^FAIL")
                set(entry_status "<span style=\"color: red\">FAIL</span>")
                if(EXISTS "${output_directory}/${test_name}_diff.png")
                    set(entry_diff "![difference](results/${test_name}_diff.png)")
                endif()
            endif()
        endif()
        string(APPEND tests_md "| ${entry_status} | ${test_name} | ![rendered](results/${test_name}.png) | ![reference](results/${test_name}_ref.png) | ${entry_diff} |\n")
    endforeach()
    file(WRITE "${TESTS_MD}" "${tests_md}")
endfunction()

function(write_source_tests_report)
    file(STRINGS "${TEST_MANIFEST}" test_entries)
    set(tests_md "# BSDL Image Tests\n\n| Test | Reference |\n| --- | --- |\n")
    foreach(entry IN LISTS test_entries)
        string(STRIP "${entry}" entry)
        if(entry STREQUAL "" OR entry MATCHES "^#")
            continue()
        endif()
        string(FIND "${entry}" "|" separator)
        if(separator LESS 1)
            message(FATAL_ERROR "Invalid bsdltool test entry '${entry}'")
        endif()
        string(SUBSTRING "${entry}" 0 ${separator} test_name)
        string(STRIP "${test_name}" test_name)
        string(APPEND tests_md "| ${test_name} | ![${test_name}](references/${test_name}.png) |\n")
    endforeach()
    file(WRITE "${SOURCE_TESTS_MD}" "${tests_md}")
endfunction()

execute_process(
    COMMAND "${BSDLTOOL}" render ${ARGS} --resolution "${RESOLUTION}" -o "${OUTPUT}"
    RESULT_VARIABLE result
)
if(NOT result EQUAL 0)
    get_filename_component(output_name "${OUTPUT}" NAME_WE)
    file(REMOVE "${output_directory}/${output_name}_diff.png")
    file(WRITE "${output_directory}/${output_name}.status" "FAIL\n")
    write_tests_report()
    message(FATAL_ERROR "bsdltool exited with status ${result}")
endif()

if(UPDATE_REFERENCES OR environment_update STREQUAL "1" OR environment_update STREQUAL "ON" OR
   environment_update STREQUAL "TRUE")
    get_filename_component(reference_directory "${REFERENCE}" DIRECTORY)
    file(MAKE_DIRECTORY "${reference_directory}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${OUTPUT}" "${REFERENCE}"
        RESULT_VARIABLE result
    )
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Could not update reference ${REFERENCE}")
    endif()
    write_source_tests_report()
elseif(NOT EXISTS "${REFERENCE}")
    message(FATAL_ERROR
        "Reference image does not exist: ${REFERENCE}. Re-run this selected test with "
        "BSDL_UPDATE_REFERENCES=1 or configure with -DBSDL_UPDATE_REFERENCES=ON.")
endif()

get_filename_component(output_name "${OUTPUT}" NAME_WE)
get_filename_component(output_extension "${OUTPUT}" LAST_EXT)
set(status_file "${output_directory}/${output_name}.status")
set(diff_output "${output_directory}/${output_name}_diff${output_extension}")
set(reference_output "${output_directory}/${output_name}_ref${output_extension}")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${REFERENCE}" "${reference_output}"
    RESULT_VARIABLE result
)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "Could not copy reference to ${reference_output}")
endif()

set(test_status PASS)
if(NOT (UPDATE_REFERENCES OR environment_update STREQUAL "1" OR environment_update STREQUAL "ON" OR
        environment_update STREQUAL "TRUE"))
    execute_process(
        COMMAND "${BSDLTOOL}" diff "${REFERENCE}" "${OUTPUT}" --threshold "${THRESHOLD}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE diff_score
    )
    if(NOT result EQUAL 0)
        if(result EQUAL 1)
            set(test_status FAIL)
            execute_process(
                COMMAND "${BSDLTOOL}" diff "${REFERENCE}" "${OUTPUT}" --threshold "${THRESHOLD}"
                        -o "${diff_output}"
                RESULT_VARIABLE diff_image_result
            )
            if(diff_image_result GREATER 1)
                message(FATAL_ERROR "Could not write difference image: ${diff_output}")
            endif()
        else()
            file(WRITE "${status_file}" "FAIL\n")
            write_tests_report()
            message(FATAL_ERROR "bsdltool diff failed with status ${result}: ${diff_score}")
        endif()
    else()
        file(REMOVE "${diff_output}")
    endif()
endif()

file(WRITE "${status_file}" "${test_status}\n")

write_tests_report()

if(test_status STREQUAL "FAIL")
    message(FATAL_ERROR "Rendered image exceeds the ${THRESHOLD} RMSE threshold: ${REFERENCE}${diff_score}")
endif()