kyna_add_module(kyna_host SOURCES
  src/production_runtime_capabilities.cpp
  src/postgres_database.cpp
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
