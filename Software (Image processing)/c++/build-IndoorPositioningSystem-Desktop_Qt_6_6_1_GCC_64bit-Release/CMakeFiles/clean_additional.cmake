# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Release")
  file(REMOVE_RECURSE
  "CMakeFiles/IndoorPositioningSystem_autogen.dir/AutogenUsed.txt"
  "CMakeFiles/IndoorPositioningSystem_autogen.dir/ParseCache.txt"
  "IndoorPositioningSystem_autogen"
  )
endif()
