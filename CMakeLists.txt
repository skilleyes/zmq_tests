cmake_minimum_required(VERSION 3.0.0.)

project(zmq_test)

set(CMAKE_CXX_STANDARD 17)

# This allows for our packaged libs to have priority over system libraries (ie. protobuf, zeromq etc..)
list(INSERT CMAKE_FIND_ROOT_PATH 0 /opt/sqdr)
# In case path already exists (in parent CMakeLists.txt), keep only the first occurence using REMOVE_DUPLICATES
list(REMOVE_DUPLICATES CMAKE_FIND_ROOT_PATH)


find_package(cppzmq CONFIG REQUIRED)
message(STATUS "cppzmq version ${cppzmq_VERSION}")
find_package(Threads REQUIRED)

add_executable(client client.cpp)
target_link_libraries(client cppzmq)

add_executable(worker worker.cpp)
target_link_libraries(worker cppzmq)

add_executable(broker broker.cpp)
target_link_libraries(broker cppzmq)

add_executable(standalone_broker standalone_broker.cpp)
target_link_libraries(standalone_broker cppzmq Threads::Threads)

add_executable(pubsub pubsub/server.cpp)
target_link_libraries(pubsub cppzmq)

add_executable(pubsubclient pubsub/client.cpp)
target_link_libraries(pubsubclient cppzmq)