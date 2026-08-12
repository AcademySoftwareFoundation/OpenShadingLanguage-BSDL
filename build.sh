#!/usr/bin/env sh
# Configure and build BSDL. Pass --configure to only configure, or --test to run CTest.

set -eu

run_tests=false
configure_only=false
build_testing=false
test_pattern=

while [ "$#" -gt 0 ]; do
    case "$1" in
        --configure)
            configure_only=true
            build_testing=true
            shift
            ;;
        --test)
            run_tests=true
            build_testing=true
            shift
            if [ "$#" -gt 0 ]; then
                test_pattern=$1
                shift
            fi
            ;;
        --help|-h)
            printf '%s\n' "Usage: $0 [--configure] [--test [PATTERN]]"
            exit 0
            ;;
        *)
            printf '%s\n' "Unknown option: $1" >&2
            printf '%s\n' "Usage: $0 [--configure] [--test [PATTERN]]" >&2
            exit 2
            ;;
    esac
done

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
build_dir=${BSDL_BUILD_DIR:-"$project_dir/build"}
build_type=${CMAKE_BUILD_TYPE:-Release}

cmake -S "$project_dir" -B "$build_dir" \
    -DCMAKE_BUILD_TYPE="$build_type" \
    -DBUILD_TESTING="$build_testing"

if "$configure_only" && ! "$run_tests"; then
    exit 0
fi

cmake --build "$build_dir" --parallel

if "$run_tests"; then
    if [ -n "$test_pattern" ]; then
        ctest --test-dir "$build_dir" -R "$test_pattern" --output-on-failure
    else
        ctest --test-dir "$build_dir" --output-on-failure
    fi
fi
