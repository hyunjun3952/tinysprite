# cmake -DFILE=<path> -P check_file.cmake: fails unless the file exists and is not empty.
if(NOT EXISTS "${FILE}")
  message(FATAL_ERROR "${FILE} was not written")
endif()
file(SIZE "${FILE}" size)
if(size EQUAL 0)
  message(FATAL_ERROR "${FILE} is empty")
endif()
