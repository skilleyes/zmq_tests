//  Server test Ryan

#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>
#include <zmq.hpp>
#include <zmq_addon.hpp>

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
    zmq::socket_t pair_socket(context, zmq::socket_type::pair);
    pair_socket.connect("inproc://updates");
    pair_socket.send(zmq::str_buffer("READY"));

    zmq::socket_t router_socket(context, zmq::socket_type::router);
    router_socket.set(zmq::sockopt::router_mandatory, 1);
    router_socket.bind("tcp://localhost:5556");

    std::unordered_map<std::string, std::string> kvmap;
    int64_t sequence = 0;

    zmq::pollitem_t items[] = {
        {pair_socket, 0, ZMQ_POLLIN, 0},   // 0
        {router_socket, 0, ZMQ_POLLIN, 0}  // 1
    };
    //  Process messages from both sockets
    while (1) {
        zmq::message_t message;
        zmq::poll(&items[0], 2, std::chrono::milliseconds(100));

        if (items[0].revents & ZMQ_POLLIN) {
            zmq::multipart_t recv_msgs;

            bool ret = recv_msgs.recv(pair_socket);
            if (!ret) {
                std::cout << "Error receiving multipart message" << std::endl;
                break;
            }
            std::string key, value;
            key = recv_msgs.front().to_string();
            value = recv_msgs.back().to_string();
            std::cout << "Updating state " << key << " : " << value << std::endl;
            kvmap[key] = value;
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
            // Send all entries
            std::cout << "Sending all entries" << std::endl;
            for (const auto &entry : kvmap) {
                std::cout << "Sending entry " << entry.first << " to " << identity << std::endl;
                router_socket.send(zmq::buffer(identity), zmq::send_flags::sndmore);
                router_socket.send(zmq::buffer(entry.first), zmq::send_flags::sndmore);
                router_socket.send(zmq::buffer(entry.second), zmq::send_flags::none);
            }
            std::cout << "Sending KTHXBAI" << std::endl;
            router_socket.send(zmq::buffer(identity), zmq::send_flags::sndmore);
            router_socket.send(zmq::str_buffer("KTHXBAI"), zmq::send_flags::sndmore);
            router_socket.send(zmq::str_buffer(""), zmq::send_flags::none);
        }
        if (interrupted) {
            std::cout << "Exiting state manager thread" << std::endl;
            break;
        }
    }

    router_socket.set(zmq::sockopt::linger, 0);
    pair_socket.set(zmq::sockopt::linger, 0);
    router_socket.close();
    pair_socket.close();
}

int main(int argc, char *argv[]) {
    catchSignals();
    zmq::context_t context(1);

    zmq::socket_t pub_socket(context, zmq::socket_type::pub);
    pub_socket.bind("tcp://localhost:5555");
    zmq::socket_t pair_socket(context, zmq::socket_type::pair);
    pair_socket.bind("inproc://updates");

    std::thread state_manager_thread(stateManager, std::ref(context));
    zmq::message_t ready_msg;
    if (pair_socket.recv(ready_msg).has_value()) {
        std::cout << "Got " << ready_msg.to_string() << std::endl;
    }

    while (true) {
        try {
            static char letter = 'A';
            pub_socket.send(zmq::buffer(std::string(1, letter)), zmq::send_flags::sndmore);
            pair_socket.send(zmq::buffer(std::string(1, letter)), zmq::send_flags::sndmore);
            int value = rand() % 1000000;
            pub_socket.send(zmq::buffer(std::to_string(value)), zmq::send_flags::none);
            pair_socket.send(zmq::buffer(std::to_string(value)), zmq::send_flags::none);
            letter++;
            if (letter > 'Z') {
                letter = 'A';
            }
        } catch (zmq::error_t &e) {
            std::cout << "interrupt received, proceeding..." << std::endl;
        }

        if (interrupted) {
            std::cout << "interrupt received, killing program..." << std::endl;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    pub_socket.close();
    pair_socket.close();
    state_manager_thread.join();
    context.shutdown();
    context.close();
    return 0;
}