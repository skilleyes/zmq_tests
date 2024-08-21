//  Server test Ryan

#include <csignal>
#include <iostream>
#include <string>
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

int main(int argc, char *argv[]) {
    zmq::context_t context(1);

    std::string topic = "";
    if (argc > 1) {
        topic = argv[1];
        std::cout << topic << std::endl;
    }

    zmq::socket_t sub_socket(context, zmq::socket_type::sub);
    sub_socket.connect("tcp://localhost:5555");
    sub_socket.set(zmq::sockopt::subscribe, topic);

    std::unordered_map<std::string, std::string> kvmap;

    catchSignals();
    while (true) {
        try {
            std::vector<zmq::message_t> recv_msgs;

            const auto ret = zmq::recv_multipart(sub_socket, std::back_inserter(recv_msgs));
            if (!ret) {
                std::cout << "Error receiving multipart message" << std::endl;
                break;
            }
            std::string key, value;
            key = recv_msgs.front().to_string();
            value = recv_msgs.back().to_string();
            std::cout << "Received " << key << " : " << value << std::endl;
            kvmap[key] = value;
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
    context.shutdown();
    context.close();
    return 0;
}