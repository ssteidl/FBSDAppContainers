enable_testing()

include_directories(${CMAKE_SOURCE_DIR}/src/lib)

add_executable(fs_path_test fs_path_test.cpp)
target_link_libraries(fs_path_test vessel-priv)
