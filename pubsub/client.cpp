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
    zmq::socket_t push_socket(context, zmq::socket_type::push);
    push_socket.connect("tcp://localhost:5557");

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
    zmq::pollitem_t items[] = {
        {sub_socket, 0, ZMQ_POLLIN, 0},  // 0
    };
    auto last_sent_tp = std::chrono::steady_clock::now();
    auto next = last_sent_tp + std::chrono::milliseconds(100);
    while (true) {
        try {
            zmq::poll(&items[0], 1, std::chrono::milliseconds(100));
            if (items[0].revents & ZMQ_POLLIN) {
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
            }
            if (std::chrono::steady_clock::now() >= next) {
                static char letter = 'A';
                int value = rand() % 1000000;
                KVMsg kvmsg(std::string(1, letter), std::to_string(value), 0);
                kvmsg.send(push_socket);
                std::cout << "Publishing update " << kvmsg.dump() << std::endl;
                letter++;
                if (letter > 'Z') {
                    letter = 'A';
                }
                next += std::chrono::milliseconds(100);
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
    push_socket.set(zmq::sockopt::linger, 0);
    push_socket.close();
    sub_socket.close();
    dealer_socket.close();
    context.shutdown();
    context.close();
    return 0;
}