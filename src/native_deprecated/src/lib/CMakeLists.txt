add_library(vessel-priv STATIC
  vessel_tcl.cpp
  container.cpp
  fs.cpp
  image_archive.cpp
  mountpoint.cpp)
target_link_libraries(vessel-priv ${TCL_LIBRARY})
