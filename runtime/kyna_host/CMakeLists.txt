kyna_add_module(kyna_host SOURCES
  src/types/database_failure_phase_name.cpp
  src/types/network_failure_phase_name.cpp
  src/capabilities/local_file_system.cpp
  src/capabilities/local_process.cpp
  src/capabilities/local_host_info.cpp
  src/capabilities/system_clock.cpp
  src/capabilities/beast_http_server.cpp
  src/capabilities/curl_network.cpp
  src/capabilities/runtime_capabilities.cpp
  src/capabilities/file_stream.cpp
  src/capabilities/context.cpp
  src/database/unavailable_database.cpp
  src/database/postgres_database.cpp
  src/database/database_port.cpp
  PRIVATE_DEPENDENCIES CURL::libcurl)
target_compile_definitions(kyna_host PRIVATE BOOST_ERROR_CODE_HEADER_ONLY=1)
if(TARGET Boost::asio AND TARGET Boost::beast)
  target_link_libraries(kyna_host PRIVATE Boost::asio Boost::beast)
elseif(TARGET Boost::headers)
  target_link_libraries(kyna_host PRIVATE Boost::headers)
elseif(Boost_INCLUDE_DIRS)
  target_include_directories(kyna_host PRIVATE ${Boost_INCLUDE_DIRS})
endif()

if(PostgreSQL_FOUND)
  target_compile_definitions(kyna_host PRIVATE KYNA_HAS_POSTGRESQL=1)
  target_link_libraries(kyna_host PRIVATE PostgreSQL::PostgreSQL)
endif()
