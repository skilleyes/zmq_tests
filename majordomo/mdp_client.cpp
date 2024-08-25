#include <iostream>
#include <string>
#include <zmq.hpp>
#include <zmq_addon.hpp>

#include "mdp.h"
#include "mdp_client.h"

MDPClient::MDPClient(std::string broker, int verbose) : broker_(broker), verbose_(verbose) {
    context_ = new zmq::context_t(1);
    connectToBroker();
}

MDPClient::~MDPClient() {
    delete client_;
    ;
    delete context_;
}

void MDPClient::connectToBroker() {
    if (client_) {
        delete client_;
    }
    client_ = new zmq::socket_t(*context_, ZMQ_REQ);
    client_->set(zmq::sockopt::linger, 0);
    client_->connect(broker_);
    if (verbose_) {
        std::cout << "I: connecting to broker at " << broker_ << "..." << std::endl;
    }
}

void MDPClient::setTimeout(int timeout) {
    timeout_ = timeout;
}

void MDPClient::setRetries(int retries) {
    retries_ = retries;
}

zmq::message_t MDPClient::send(std::string service, zmq::message_t request) {
    zmq::multipart_t frames;
    //  Prefix request with protocol frames
    //  Frame 1: "MDPCxy" (six bytes, MDP/Client x.y)
    //  Frame 2: Service name (printable string)
    frames.addstr(std::string(k_mdp_client));
    frames.addstr(service);
    frames.add(std::move(request));
    if (verbose_) {
        std::cout << "I: send request to " << service << std::endl;
        std::cout << request << std::endl;
    }

    int retries_left = retries_;
    while (retries_left) {

        frames.send(*client_);

        while (true) {
            //  Poll socket for a reply, with timeout
            zmq::pollitem_t items[] = {{*client_, 0, ZMQ_POLLIN, 0}};
            zmq::poll(items, 1, std::chrono::milliseconds(timeout_));

            //  If we got a reply, process it
            if (items[0].revents & ZMQ_POLLIN) {
                zmq::multipart_t recv_msg;
                recv_msg.recv(*client_);
                if (verbose_) {
                    std::cout << "I: received reply:" << std::endl;
                    std::cout << recv_msg << std::endl;
                }

                std::string header = recv_msg.pop().to_string();
                std::string reply_service = recv_msg.pop().to_string();

                return recv_msg.pop();  //  Success
            } else {
                if (--retries_left) {
                    if (verbose_) {
                        std::cout << "W: no reply, reconnecting..." << std::endl;
                    }
                    //  Reconnect, and resend message
                    connectToBroker();
                    frames.send(*client_);
                } else {
                    if (verbose_) {
                        std::cout << "W: permanent error, abandoning request" << std::endl;
                    }
                    break;  //  Give up
                }
            }
        }
    }
    return zmq::message_t(0);  // Return an empty message
}