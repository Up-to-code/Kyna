find_package(Qt6 REQUIRED COMPONENTS Quick)
add_library(kyna_gui MODULE src/bindings/quick_gui.cpp)
set_target_properties(kyna_gui PROPERTIES AUTOMOC ON LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/native" RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/native")
target_include_directories(kyna_gui PRIVATE ../native/include ../../sdk/kyna_embedding/include)
target_link_libraries(kyna_gui PRIVATE Qt6::Quick)
target_compile_features(kyna_gui PRIVATE cxx_std_20)

install(TARGETS kyna_gui LIBRARY DESTINATION lib/kyna/native RUNTIME DESTINATION lib/kyna/native)
install(FILES ${CMAKE_SOURCE_DIR}/examples/native/packages/gui.ky DESTINATION share/kyna/packages)
