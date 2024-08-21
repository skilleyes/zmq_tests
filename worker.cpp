//
//   Request-reply service in C++
//   Connects REP socket to tcp://localhost:5560
//   Expects "Hello" from client, replies with "World"
//

#include <iostream>

#include <unistd.h>

#include <zmq.hpp>

int main(int argc, char *argv[]) {
    zmq::context_t context(1);

    zmq::socket_t responder(context, ZMQ_REP);
    responder.connect("tcp://localhost:5560");

    while (1) {
        //  Wait for next request from client
        zmq::message_t msg;
        responder.recv(msg);
        std::string string = msg.to_string();

        std::cout << "Received request: " << string << std::endl;

        // Do some 'work'
        sleep(1);

        //  Send reply back to client
        responder.send(zmq::str_buffer("World"));
    }
}