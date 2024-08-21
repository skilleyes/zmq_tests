//  Server test Ryan

#include <csignal>
#include <chrono>
#include <iostream>
#include <thread>
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

    zmq::socket_t pub_socket(context, zmq::socket_type::pub);
    pub_socket.bind("tcp://localhost:5555");

    catchSignals();
    while (true) {
        try {
            pub_socket.send(zmq::str_buffer("A"), zmq::send_flags::sndmore);
            pub_socket.send(zmq::buffer("value for A"), zmq::send_flags::none);
            pub_socket.send(zmq::str_buffer("B"), zmq::send_flags::sndmore);
            pub_socket.send(zmq::buffer("value for B"), zmq::send_flags::none);
        } catch (zmq::error_t &e) {
            std::cout << "interrupt received, proceeding..." << std::endl;
        }

        if (interrupted) {
            std::cout << "interrupt received, killing program..." << std::endl;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    pub_socket.close();
    context.shutdown();
    context.close();
    return 0;
}