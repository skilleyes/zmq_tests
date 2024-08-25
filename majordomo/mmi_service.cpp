#include "mdp_client.h"

#include <iostream>
#include <string>

int main(int argc, char **argv) {
    int verbose = (argc > 1 && std::string(argv[1]) == "-v");

    MDPClient client("tcp://localhost:5555", verbose);

    zmq::message_t request(std::string("echo"));
    auto reply = client.send("mmi.service", std::move(request));
    std::cout << reply << std::endl;
    if (!reply.empty()) {
        std::string status = reply.to_string();
        std::cout << "Lookup echo service: " << status << std::endl;
    }
    return 0;
}