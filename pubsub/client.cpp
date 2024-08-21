//  Server test Ryan

#include <csignal>
#include <iostream>
#include <string>
#include <zmq.hpp>
#include <zmq_addon.hpp>

#include "kvmsg.h"

bool interrupted = false;

void signalHandler(int signal_value) {
    interrupted = true;
}

void catchSignals() {
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    std::signal(SIGSEGV, signalHandler);
    std::signal(SIGABRT, signalHandler);
}

int main(int argc, char *argv[]) {
    zmq::context_t context(1);

    std::string topic = "";
    if (argc > 1) {
        topic = argv[1];
        std::cout << topic << std::endl;
    }

    zmq::socket_t dealer_socket(context, zmq::socket_type::dealer);
    dealer_socket.connect("tcp://localhost:5556");
    zmq::socket_t sub_socket(context, zmq::socket_type::sub);
    sub_socket.connect("tcp://localhost:5555");
    sub_socket.set(zmq::sockopt::subscribe, topic);

    std::unordered_map<std::string, std::string> kvmap;

    catchSignals();

    //  Get state snapshot
    uint64_t sequence = 0;
    dealer_socket.send(zmq::str_buffer("ICANHAZ?"));
    std::cout << "Requesting snapshot" << std::endl;
    while (true) {
        KVMsg kvmsg;
        bool ret = kvmsg.recv(dealer_socket);
        if (!ret) {
            break;  //  Interrupted
        }
        if (kvmsg.key() == "KTHXBAI") {
            std::cout << "Received snapshot = " << kvmsg.sequence() << std::endl;
            sequence = kvmsg.sequence();
            break;  //  Done
        }
        std::cout << "Received " << kvmsg.dump() << std::endl;
        kvmsg.store(kvmap);
    }
    while (true) {
        try {
            KVMsg kvmsg;
            int ret = kvmsg.recv(sub_socket);
            if (!ret) {
                break;  //  Interrupted
            }
            if (kvmsg.sequence() > sequence) {
                sequence = kvmsg.sequence();
                kvmsg.store(kvmap);
                std::cout << "Received update " << kvmsg.dump() << std::endl;
            }
        } catch (zmq::error_t &e) {
            std::cout << "interrupt received, proceeding..." << std::endl;
            std::cout << e.what() << std::endl;
        }

        if (interrupted) {
            std::cout << "interrupt received, killing program..." << std::endl;
            break;
        }
    }
    sub_socket.close();
    dealer_socket.close();
    context.shutdown();
    context.close();
    return 0;
}