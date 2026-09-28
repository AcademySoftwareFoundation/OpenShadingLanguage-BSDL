#!/usr/bin/env sh
# Configure and build BSDL. Pass --configure to only configure, or --test to run CTest.

set -eu

run_tests=false
configure_only=false
build_testing=false
update_references=false
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
            # The pattern is optional; do not swallow a following option.
            if [ "$#" -gt 0 ] && [ "${1#--}" = "$1" ]; then
                test_pattern=$1
                shift
            fi
            ;;
        --update)
            update_references=true
            shift
            ;;
        --help|-h)
            printf '%s\n' "Usage: $0 [--configure] [--test [PATTERN] [--update]]"
            exit 0
            ;;
        *)
            printf '%s\n' "Unknown option: $1" >&2
            printf '%s\n' "Usage: $0 [--configure] [--test [PATTERN] [--update]]" >&2
            exit 2
            ;;
    esac
done

if "$update_references" && ! "$run_tests"; then
    printf '%s\n' "--update only makes sense together with --test" >&2
    exit 2
fi

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
build_dir=${BSDL_BUILD_DIR:-"$project_dir/build"}
build_type=${CMAKE_BUILD_TYPE:-Release}

# Use ccache as a compiler launcher when available to speed up rebuilds.
if command -v ccache >/dev/null 2>&1; then
    ccache_arg=-DCMAKE_CXX_COMPILER_LAUNCHER=ccache
fi

cmake -S "$project_dir" -B "$build_dir" \
    -DCMAKE_BUILD_TYPE="$build_type" \
    -DBUILD_TESTING="$build_testing" \
    ${ccache_arg:-}

if "$configure_only" && ! "$run_tests"; then
    exit 0
fi

cmake --build "$build_dir" --parallel

if "$run_tests"; then
    if "$update_references"; then
        printf '%s\n' "Updating reference images for the selected tests"
        export BSDL_UPDATE_REFERENCES=1
    fi
    if [ -n "$test_pattern" ]; then
        ctest --test-dir "$build_dir" -R "$test_pattern" --output-on-failure
    else
        ctest --test-dir "$build_dir" --output-on-failure
    fi
fi
