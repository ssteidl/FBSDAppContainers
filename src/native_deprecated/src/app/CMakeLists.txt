add_executable(vessel
  app_containers.cpp
  app_functions.cpp
  cmdline.cpp
  environment.cpp
)
target_link_libraries(vessel vessel-priv archive)
