#add_executable(mdp_broker mdp_broker.cpp)
#target_link_libraries(mdp_broker cppzmq)

add_executable(mdp_client mdp_client.cpp mdp_client_example.cpp)
target_link_libraries(mdp_client cppzmq)

add_executable(mdp_worker mdp_worker.cpp mdp_worker_example.cpp)
target_link_libraries(mdp_worker cppzmq)

add_executable(mdp_broker mdp_broker.cpp mdp_broker_example.cpp)
target_link_libraries(mdp_broker cppzmq)

add_executable(mmi_service mdp_client.cpp mmi_service.cpp)
target_link_libraries(mmi_service cppzmq)
