//  Server test Ryan

#include <csignal>
#include <iostream>
#include <string>
#include <zmq.hpp>

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

    zmq::socket_t sub_socket(context, zmq::socket_type::sub);
    sub_socket.connect("tcp://localhost:5555");
    sub_socket.set(zmq::sockopt::subscribe, "A");

    catchSignals();
    while (true) {
        try {
            zmq::message_t topic, message;
            sub_socket.recv(topic, zmq::recv_flags::none);
            sub_socket.recv(message, zmq::recv_flags::none);
            std::cout << "Received " << message.to_string() << " on topic " << topic.to_string() << std::endl;
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