//
//   Request-reply client in C++
//   Connects REQ socket to tcp://localhost:5559
//   Sends "Hello" to server, expects "World" back
//

#include <iostream>

#include <zmq.hpp>

int main(int argc, char *argv[]) {
    zmq::context_t context(1);

    zmq::socket_t requester(context, ZMQ_REQ);
    requester.connect("tcp://localhost:5559");
    // requester.bind("tcp://*:5560");

    for (int request = 0; request < 10; request++) {
        zmq::message_t msg;
        requester.send(zmq::str_buffer("Hello"));
        requester.recv(msg);
        std::string string = msg.to_string();

        std::cout << "Received reply " << request << " [" << string << "]" << std::endl;
    }
}
