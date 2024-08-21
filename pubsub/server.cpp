//  Server test Ryan

#include <chrono>
#include <csignal>
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

    std::unordered_map<std::string, std::string> kvmap;

    catchSignals();
    while (true) {
        try {
            static char letter = 'A';
            pub_socket.send(zmq::buffer(std::string(1, letter)), zmq::send_flags::sndmore);
            int value = rand() % 1000000;
            pub_socket.send(zmq::buffer(std::to_string(value)), zmq::send_flags::none);
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
    context.shutdown();
    context.close();
    return 0;
}