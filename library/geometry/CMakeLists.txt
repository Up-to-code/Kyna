find_package(CGAL REQUIRED)
add_library(kyna_geometry MODULE src/bindings/geometry_2d.cpp src/bindings/geometry_3d.cpp)
target_include_directories(kyna_geometry PRIVATE ../native/include ../../sdk/kyna_embedding/include)
target_link_libraries(kyna_geometry PRIVATE CGAL::CGAL)
target_compile_features(kyna_geometry PRIVATE cxx_std_20)
set_target_properties(kyna_geometry PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/native" RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/native")

install(TARGETS kyna_geometry LIBRARY DESTINATION lib/kyna/native RUNTIME DESTINATION lib/kyna/native)
install(FILES ${CMAKE_SOURCE_DIR}/examples/native/packages/geometry.ky DESTINATION share/kyna/packages)
