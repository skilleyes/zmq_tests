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
        zmq::multipart_t recv_msgs;
        bool ret = recv_msgs.recv(dealer_socket);
        if (!ret) {
            break;  //  Interrupted
        }
        std::string key, value;
        key = recv_msgs.front().to_string();
        std::memcpy(&sequence, recv_msgs[1].data(), sizeof(uint64_t));
        value = recv_msgs.back().to_string();
        if (key == "KTHXBAI") {
            std::cout << "Received snapshot = " << sequence << std::endl;
            break;  //  Done
        }
        std::cout << "Received " << key << " : " << value << std::endl;
        kvmap[key] = value;
    }
    while (true) {
        try {
            zmq::multipart_t recv_msgs;

            bool ret = recv_msgs.recv(sub_socket);
            if (!ret) {
                std::cout << "Error receiving multipart message" << std::endl;
                break;
            }
            std::string key, value;
            uint64_t received_sequence;
            key = recv_msgs.front().to_string();
            std::memcpy(&received_sequence, recv_msgs[1].data(), sizeof(uint64_t));
            value = recv_msgs.back().to_string();
            std::cout << "Received " << key << " : " << value << " (" << received_sequence << ")" << std::endl;
            if (received_sequence > sequence) {
                kvmap[key] = value;
                sequence = received_sequence;
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