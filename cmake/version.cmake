execute_process(COMMAND bash -c "git log --pretty=format:'%h' -n 1"
	OUTPUT_VARIABLE GIT_REV ERROR_QUIET)

# Check whether we got any revision (which isn't
# always the case, e.g. when someone downloaded a zip
# file from Github instead of a checkout)
if ("${GIT_REV}" STREQUAL "")
  set(GIT_DIFF "")
  set(GIT_VERSION "UNKNOWN")
  set(GIT_BRANCH "N/A")
else()
  execute_process(
    COMMAND bash -c "git diff --quiet --exit-code || echo +"
    OUTPUT_VARIABLE GIT_DIFF)
  execute_process(
    COMMAND bash -c "git describe --always --tags || echo UNKNOWN"
    OUTPUT_VARIABLE GIT_VERSION ERROR_QUIET)
  execute_process(
    COMMAND git rev-parse --abbrev-ref HEAD
    OUTPUT_VARIABLE GIT_BRANCH)

  string(STRIP "${GIT_VERSION}" GIT_VERSION)
  string(STRIP "${GIT_REV}" GIT_REV)
  string(STRIP "${GIT_DIFF}" GIT_DIFF)
  string(STRIP "${GIT_BRANCH}" GIT_BRANCH)

endif()

set(VERSION "const char* EPICS_RELAY_GIT_REV=\"${GIT_REV}\";
const char* EPICS_RELAY_GIT_BRANCH=\"${GIT_BRANCH}\";
const char* EPICS_RELAY_GIT_VERSION=\"${GIT_VERSION}${GIT_DIFF}\";")

if(EXISTS ${CMAKE_BINARY_DIR}/version.c)
  file(READ ${CMAKE_BINARY_DIR}/version.c VERSION_)
else()
  set(VERSION_ "")
endif()

if (NOT "${VERSION}" STREQUAL "${VERSION_}")
  file(WRITE ${CMAKE_BINARY_DIR}/version.c "${VERSION}")
endif()
