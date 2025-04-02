# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/home/user/tntcxx/build/_deps/msgpuck-src")
  file(MAKE_DIRECTORY "/home/user/tntcxx/build/_deps/msgpuck-src")
endif()
file(MAKE_DIRECTORY
  "/home/user/tntcxx/build/_deps/msgpuck-build"
  "/home/user/tntcxx/build/_deps/msgpuck-subbuild/msgpuck-populate-prefix"
  "/home/user/tntcxx/build/_deps/msgpuck-subbuild/msgpuck-populate-prefix/tmp"
  "/home/user/tntcxx/build/_deps/msgpuck-subbuild/msgpuck-populate-prefix/src/msgpuck-populate-stamp"
  "/home/user/tntcxx/build/_deps/msgpuck-subbuild/msgpuck-populate-prefix/src"
  "/home/user/tntcxx/build/_deps/msgpuck-subbuild/msgpuck-populate-prefix/src/msgpuck-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/user/tntcxx/build/_deps/msgpuck-subbuild/msgpuck-populate-prefix/src/msgpuck-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/user/tntcxx/build/_deps/msgpuck-subbuild/msgpuck-populate-prefix/src/msgpuck-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
