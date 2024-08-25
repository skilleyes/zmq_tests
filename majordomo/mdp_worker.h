#pragma once

#include <string>
#include <zmq.hpp>
#include <zmq_addon.hpp>

class MDPWorker {
 public:
    MDPWorker(std::string broker, std::string service, int verbose);
    virtual ~MDPWorker();

    //  Connect or reconnect to broker
    void connectToBroker();

    void setHeartbeatDelayMs(int delay_ms);

    void setReconnectDelayMs(int delay_ms);

    void send(const char *command, std::string option, zmq::multipart_t reply);
    zmq::message_t recv(zmq::message_t reply);

 private:
    const std::string broker_;
    const std::string service_;
    zmq::context_t *context_;
    zmq::socket_t *worker_{nullptr};  //  Socket to broker
    const int verbose_;               //  Print activity to stdout

    // Heartbeat management
    int liveness_;
    int heartbeat_delay_ms_;
    int reconnect_delay_ms_;
    std::chrono::steady_clock::time_point heartbeat_tp_;

    //  Internal state
    bool expect_reply_;  //  Zero only at start

    //  Return address, if any
    zmq::multipart_t reply_to_;
};