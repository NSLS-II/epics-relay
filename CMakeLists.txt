#
#  epics-relay
#
#  Stuart B. Wilkins, Brookhaven National Laboratory
#
#
#  BSD 3-Clause License
#
#  Copyright (c) 2021, Brookhaven Science Associates
#  All rights reserved.
#
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions are met:
#
#  1. Redistributions of source code must retain the above copyright notice,
#     this list of conditions and the following disclaimer.
#
#  2. Redistributions in binary form must reproduce the above copyright notice,
#     this list of conditions and the following disclaimer in the documentation
#     and/or other materials provided with the distribution.
#
#  3. Neither the name of the copyright holder nor the names of its
#     contributors may be used to endorse or promote products derived from
#     this software without specific prior written permission.
#
#  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
#  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
#  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
#  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
#  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
#  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
#  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
#  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
#  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
#  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
#  THE POSSIBILITY OF SUCH DAMAGE.
#

cmake_minimum_required(VERSION 3.10)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# set the project name
project(epics-helper C)

# Set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED TRUE)
set(CMAKE_C_EXTENSIONS ON)

# Options
option(CPPLINT_CHECK        "Check CXX Source for Formatting" ON)
option(PEDANTIC             "Compile with pedantic warnings" ON)
option(DEBUG                "Show debug comments" OFF)
option(SYSTEMD              "Compile as systemd daemon" ON)
option(NO_IN_SOURCE_BUILDS  "Prevent in source builds" ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

if(NO_IN_SOURCE_BUILDS)
  include(PreventInSourceBuilds)
endif()

# Add options

set(CMAKE_C_FLAGS_DEBUG "-g")
set(CMAKE_C_FLAGS_RELEASE "-O3")

if(PEDANTIC)
  add_compile_options(-pedantic -Wall -Wextra -Werror)
  add_compile_options(-Wno-stringop-truncation)
endif()

if(DEBUG)
  add_compile_options(-DDEBUG)
endif()

if(SYSTEMD)
  add_compile_options(-DSYSTEMD)
  include(systemdservice)
endif()

if(CPPLINT_CHECK)
  include(cpplint)
  cpplint_add_subdirectory(src)
  message(STATUS "Checking CXX Code via cpplint")
endif()

# Add __FILENAME__ with short path
set(CMAKE_C_FLAGS "${CMAKE_CXX_FLAGS} -D__FILENAME__='\"$(subst ${CMAKE_SOURCE_DIR}/,,$(abspath $<))\"'")

# Add a custom command that produces version.c, plus
# a dummy output that's not actually produced, in order
# to force version.cmake to always be re-run before the build
add_custom_command(
  OUTPUT ${CMAKE_BINARY_DIR}/version.c
  ${CMAKE_BINARY_DIR}/_version.c
  COMMAND ${CMAKE_COMMAND} -P
  ${CMAKE_SOURCE_DIR}/cmake/version.cmake
)

# add the executable
add_executable(collector src/collector.c
                         src/ethernet.c
                         src/epics.c
                         version.c)

add_executable(emitter   src/emitter.c
                         src/ethernet.c
                         src/epics.c
                         version.c)

add_executable(${PROJECT_NAME} ${SRC_LIST} epics-relay.conf)

add_custom_target(version_info DEPENDS ${CMAKE_BINARY_DIR}/version.c)

find_library(NET_LIBRARY net REQUIRED)
find_library(NET_LIBRARY pcre2-8 REQUIRED)

target_link_libraries(collector PRIVATE pcre2-8 pcap net pthread)
target_link_libraries(emitter PRIVATE pcre2-8 pcap net pthread)

# Install

install(TARGETS collector RUNTIME DESTINATION bin/ COMPONENT binaries)
install(TARGETS emitter RUNTIME DESTINATION bin/ COMPONENT binaries)
install(TARGETS epics-relay.conf RUNTIME DESTINATION etc/ COMPONENT config)
if (SYSTEMD_FOUND)
  configure_file(systemd/emitter.service.in ${CMAKE_CURRENT_BINARY_DIR}/emitter.service @ONLY)
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/emitter.service
    DESTINATION ${SYSTEMD_SERVICES_INSTALL_DIR}
    COMPONENT data
  )
  configure_file(systemd/collector.service.in ${CMAKE_CURRENT_BINARY_DIR}/collector.service @ONLY)
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/collector.service
    DESTINATION ${SYSTEMD_SERVICES_INSTALL_DIR}
    COMPONENT data
  )
endif (SYSTEMD_FOUND)
