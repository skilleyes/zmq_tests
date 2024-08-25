#include <iostream>
#include <string>

#include "mdp_worker.h"

int main(int argc, char *argv[]) {
    int verbose = (argc > 1 && std::string(argv[1]) == "-v");
    MDPWorker mdp_worker("tcp://localhost:5555", "echo", verbose);

    zmq::message_t reply(0);
    while (true) {
        zmq::message_t request = mdp_worker.recv(std::move(reply));
        if (request.empty()) {
            break;
        }
        reply.copy(request);
    }
    return 0;
}
