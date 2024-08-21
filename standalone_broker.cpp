//
//  Simple request-reply broker in C++
//

#include <iostream>
#include <thread>

#include <unistd.h>

#include <zmq.hpp>

void client_thread(zmq::context_t *context) {
    zmq::socket_t requester(*context, ZMQ_REQ);
    requester.connect("inproc://broker");

    for (int request = 0; request < 10; request++) {
        zmq::message_t msg;
        requester.send(zmq::str_buffer("Hello"));
        requester.recv(msg);
        std::string string = msg.to_string();

        std::cout << "Received reply " << request << " [" << string << "]" << std::endl;
    }
}

void worker_thread(zmq::context_t *context) {
    zmq::socket_t responder(*context, ZMQ_REP);
    responder.connect("tcp://localhost:5560");
    // responder.bind("tcp://*:5560");

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

int main(int argc, char *argv[]) {
    //  Prepare our context and sockets
    zmq::context_t context(1);
    zmq::socket_t frontend(context, ZMQ_ROUTER);
    zmq::socket_t backend(context, ZMQ_DEALER);

    frontend.bind("inproc://broker");
    backend.bind("tcp://*:5560");
    // backend.connect("tcp://localhost:5560");

    std::thread client1(client_thread, &context);
    std::thread client2(client_thread, &context);
    std::thread client3(client_thread, &context);
    std::thread worker1(worker_thread, &context);
    std::thread worker2(worker_thread, &context);
    std::thread worker3(worker_thread, &context);

    zmq::proxy(frontend, backend);

    client1.join();
    client2.join();
    client3.join();
    worker1.join();
    worker2.join();
    worker3.join();
    return 0;
}