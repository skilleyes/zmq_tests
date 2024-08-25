#include <iostream>
#include <string>

#include "mdp_client.h"

int main(int argc, char *argv[]) {
    int verbose = (argc > 1 && std::string(argv[1]) == "-v");
    MDPClient mdp_client("tcp://localhost:5555", verbose);

    mdp_client.setRetries(2);
    mdp_client.setTimeout(1000);

    int count;
    for (count = 0; count < 10; count++) {
        std::cout << "Sending Hello World (" << count << ")" << std::endl;
        zmq::message_t request(std::string("Hello world"));
        auto reply = mdp_client.send("echo", std::move(request));
        if (reply.empty()) {
            break;  //  Interrupt or failure
        }
    }
    std::cout << count << " requests/replies processed" << std::endl;
    return 0;
}
