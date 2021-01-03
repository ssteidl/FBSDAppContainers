set(APP_DIR ${CMAKE_SOURCE_DIR}/src/app)
include_directories(${APP_DIR})

add_executable(vessel_mount
  vessel_mount.cpp
  ${APP_DIR}/app_functions.cpp
  ${APP_DIR}/cmdline.cpp
  ${APP_DIR}/environment.cpp
)
target_link_libraries(vessel_mount vessel-priv archive jail)

add_executable(vessel_save
  vessel_save.cpp
  ${APP_DIR}/app_functions.cpp
  ${APP_DIR}/cmdline.cpp
  ${APP_DIR}/environment.cpp
)
target_link_libraries(vessel_save vessel-priv archive jail)

add_executable(vessel_compress
  vessel_compress.cpp
)
target_link_libraries(vessel_compress archive)
