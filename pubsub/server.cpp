//  Server test Ryan

#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>
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

void stateManager(zmq::context_t &context) {
}

int main(int argc, char *argv[]) {
    catchSignals();
    zmq::context_t context(1);

    zmq::socket_t pub_socket(context, zmq::socket_type::pub);
    pub_socket.bind("tcp://localhost:5555");

    zmq::socket_t router_socket(context, zmq::socket_type::router);
    router_socket.set(zmq::sockopt::router_mandatory, 1);
    router_socket.bind("tcp://localhost:5556");

    zmq::socket_t pull_socket(context, zmq::socket_type::pull);
    pull_socket.bind("tcp://localhost:5557");

    uint64_t sequence = 0;

    std::unordered_map<std::string, std::string> kvmap;
    zmq::pollitem_t items[] = {
        {pull_socket, 0, ZMQ_POLLIN, 0},   // 0
        {router_socket, 0, ZMQ_POLLIN, 0}  // 1
    };
    //  Process messages from both sockets
    while (1) {
        zmq::message_t message;
        zmq::poll(&items[0], 2, std::chrono::milliseconds(100));

        if (items[0].revents & ZMQ_POLLIN) {
            KVMsg kvmsg;
            bool ret = kvmsg.recv(pull_socket);
            if (!ret) {
                std::cout << "Error receiving KVMsg from pull socket" << std::endl;
                break;
            }
            kvmsg.set_sequence(++sequence);
            std::cout << "Updating state and publishing " << kvmsg.dump() << std::endl;
            kvmsg.store(kvmap);
            kvmsg.send(pub_socket);
        }
        if (items[1].revents & ZMQ_POLLIN) {
            // Get identity
            zmq::message_t identity_msg;
            if (!router_socket.recv(identity_msg).has_value()) {
                break;
            }
            std::string identity = identity_msg.to_string();

            std::cout << "Got identity : " << identity << std::endl;

            // Get message request
            if (!router_socket.recv(message).has_value()) {
                break;
            }
            std::cout << "Got request" << std::endl;
            if (message.to_string() != "ICANHAZ?") {
                std::cout << "Bad request, aborting : " << message.to_string();
                break;
            }
            // Get subtree
            if (!router_socket.recv(message).has_value()) {
                break;
            }
            std::string subtree = message.to_string();
            std::cout << "Subtree request " << subtree << std::endl;

            // Send entries
            std::cout << "Sending entries" << std::endl;
            for (const auto &entry : kvmap) {
                if (subtree.empty() || entry.first.find(subtree) == 0) {
                    std::cout << "Sending entry " << entry.first << " to " << identity << std::endl;
                    router_socket.send(zmq::buffer(identity), zmq::send_flags::sndmore);
                    KVMsg kvmsg(entry.first, entry.second, 0);
                    kvmsg.send(router_socket);
                }
            }
            std::cout << "Sending KTHXBAI with sequence " << sequence << std::endl;
            router_socket.send(zmq::buffer(identity), zmq::send_flags::sndmore);
            router_socket.send(zmq::str_buffer("KTHXBAI"), zmq::send_flags::sndmore);
            router_socket.send(zmq::buffer(&sequence, sizeof(sequence)), zmq::send_flags::sndmore);
            router_socket.send(zmq::str_buffer(""), zmq::send_flags::none);
        }
        if (interrupted) {
            std::cout << "Exiting state manager thread" << std::endl;
            break;
        }
    }

    router_socket.set(zmq::sockopt::linger, 0);
    router_socket.close();
    pull_socket.close();
    pub_socket.close();
    context.shutdown();
    context.close();
    return 0;
}