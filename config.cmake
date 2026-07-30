set(PROJECT_NAME "p101-sync-check")
set(PROJECT_VERSION "1.0.0")
set(PROJECT_DESCRIPTION "Analyze p101 synchronization event streams")
set(PROJECT_LANGUAGE "C")

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Common compiler flags
set(STANDARD_FLAGS
        -D_POSIX_C_SOURCE=200809L
        -D_XOPEN_SOURCE=700
        -Werror
)

set(DARWIN_STANDARD_FLAGS
        -D_DARWIN_C_SOURCE
)

set(LINUX_STANDARD_FLAGS
)

set(BSD_STANDARD_FLAGS
)

# Define targets
set(EXECUTABLE_TARGETS main)
set(LIBRARY_TARGETS "")
set(main_OUTPUT_NAME "p101-sync-check")

set(main_SOURCES
        src/concurrency.c
        src/concurrency_finding.c
        src/concurrency_graph.c
        src/concurrency_identity.c
        src/concurrency_runner.c
        src/main.c
)

set(main_HEADERS
        include/concurrency.h
        include/concurrency_finding.h
        include/concurrency_types.h
)

set(main_LINK_LIBRARIES
        p101_error
        p101_env
        p101_tool_event
        p101_c
        p101_posix
        p101_unix
        m
)
