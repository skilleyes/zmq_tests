#include <iostream>
#include <string>
#include <thread>
#include <zmq.hpp>
#include <zmq_addon.hpp>

#include "mdp.h"
#include "mdp_worker.h"

MDPWorker::MDPWorker(std::string broker, std::string service, int verbose)
        : broker_(broker),
          service_(service),
          verbose_(verbose),
          liveness_(3),
          heartbeat_tp_(std::chrono::steady_clock::now()),
          heartbeat_delay_ms_(1000),
          reconnect_delay_ms_(2500),
          expect_reply_(false) {
    context_ = new zmq::context_t(1);
    connectToBroker();
}

MDPWorker::~MDPWorker() {
    delete worker_;
    delete context_;
}

void MDPWorker::connectToBroker() {
    if (worker_) {
        delete worker_;
    }
    worker_ = new zmq::socket_t(*context_, ZMQ_DEALER);
    worker_->set(zmq::sockopt::linger, 0);
    worker_->connect(broker_);
    if (verbose_) {
        std::cout << "I: connecting to broker at " << broker_ << "..." << std::endl;
    }

    //  Register service with broker
    send(k_mdpw_ready.data(), service_, std::move(zmq::message_t(0)));
    liveness_ = 3;
    heartbeat_tp_ = std::chrono::steady_clock::now();
}

void MDPWorker::setHeartbeatDelayMs(int delay_ms) {
    heartbeat_delay_ms_ = delay_ms;
}

void MDPWorker::setReconnectDelayMs(int delay_ms) {
    reconnect_delay_ms_ = delay_ms;
}

void MDPWorker::send(const char *command, std::string option, zmq::multipart_t request) {
    zmq::multipart_t frames;
    frames.addstr("");
    frames.addstr(std::string(k_mdpw_worker));
    frames.addstr(command);
    if (!option.empty()) {
        frames.addstr(option);
    }
    if (!request.empty()) {
        frames.append(std::move(request));
    }
    if (verbose_) {
        std::cout << "I: sending " << mdps_commands [(int) *command].data() << std::endl;
        std::cout << frames << std::endl;
    }
    frames.send(*worker_);
}

zmq::message_t MDPWorker::recv(zmq::message_t reply) {
    if (!reply.empty()) {
        zmq::multipart_t envelope;
        envelope.prepend(std::move(reply_to_));
        envelope.addstr("");
        envelope.add(std::move(reply));
        reply_to_ = zmq::multipart_t();
        send(k_mdpw_reply.data(), "", std::move(envelope));
    }
    expect_reply_ = true;
    while (true) {
        zmq::pollitem_t items[] = {{*worker_, 0, ZMQ_POLLIN, 0}};
        int rc = zmq::poll(&items[0], 1, std::chrono::milliseconds(heartbeat_delay_ms_));
        if (rc == -1) {
            break;
        }
        if (items[0].revents & ZMQ_POLLIN) {
            zmq::multipart_t recv_msg;
            recv_msg.recv(*worker_);
            if (verbose_) {
                std::cout << "I: Received message " << std::endl;
                std::cout << recv_msg << std::endl;
            }
            auto empty = recv_msg.pop();    // Empty frame
            auto header = recv_msg.pop();   // MDPW01
            auto command = recv_msg.pop();  // Command
            if (command.to_string() == std::string(k_mdpw_request)) {
                for (size_t i = 0; i < recv_msg.size(); ++i) {
                    auto envelope_part = recv_msg.pop();
                    if (envelope_part.size() == 0) {
                        break;
                    }
                    reply_to_.add(std::move(envelope_part));
                }
                return recv_msg.pop();  // Return last frame which is the message
            } else if (command.to_string() == std::string(k_mdpw_heartbeat)) {
                // Do nothing for heartbeats
            } else if (command.to_string() == std::string(k_mdpw_disconnect)) {
                connectToBroker();
            } else {
                std::cout << "E: Invalid input message" << std::endl;
            }

        } else if (liveness_ == 0) {
            if (verbose_) {
                std::cout << "Disconnected from broker - retrying..." << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(reconnect_delay_ms_));
                connectToBroker();
            }
        }

        // Send heartbeat if it's time
        if (std::chrono::steady_clock::now() > heartbeat_tp_) {
            send(k_mdpw_heartbeat.data(), "", zmq::message_t(0));
        }
    }
    return zmq::message_t(0);
}
