# JUCE 9.0.4 — dual licensed AGPLv3 / commercial JUCE 9 EULA. See LICENSES/THIRD_PARTY_NOTICES.md.
#
# Either point MEPROEQ_JUCE_DIR at a local checkout, or let CMake fetch the 9.0.4 tag. The fetched
# checkout is verified against the pinned commit so a moved/compromised tag fails the configure step.

set(MEPROEQ_JUCE_TAG "9.0.4")
set(MEPROEQ_JUCE_COMMIT "3fb5cbc0340758596dac39f7c8de47d562b4896e")

if(MEPROEQ_JUCE_DIR)
    set(_juce_src "${MEPROEQ_JUCE_DIR}")
    add_subdirectory("${_juce_src}" "${CMAKE_BINARY_DIR}/_deps/juce-build" EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(juce
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        ${MEPROEQ_JUCE_TAG}
        GIT_SHALLOW    TRUE
        GIT_PROGRESS   FALSE
    )
    FetchContent_MakeAvailable(juce)
    set(_juce_src "${juce_SOURCE_DIR}")
endif()

find_package(Git QUIET)
if(GIT_FOUND AND EXISTS "${_juce_src}/.git")
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" rev-parse HEAD
        WORKING_DIRECTORY "${_juce_src}"
        OUTPUT_VARIABLE _juce_head
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE _juce_rc)
    if(_juce_rc EQUAL 0 AND NOT _juce_head STREQUAL MEPROEQ_JUCE_COMMIT)
        message(FATAL_ERROR "JUCE checkout at ${_juce_src} is ${_juce_head}, expected ${MEPROEQ_JUCE_COMMIT} (JUCE ${MEPROEQ_JUCE_TAG})")
    endif()
    message(STATUS "Using JUCE ${MEPROEQ_JUCE_TAG} (${_juce_head}) from ${_juce_src}")
else()
    message(WARNING "Could not verify the JUCE commit (no git metadata at ${_juce_src})")
endif()
