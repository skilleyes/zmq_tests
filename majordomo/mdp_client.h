#pragma once

#include <zmq.hpp>
#include <string>

class MDPClient {
 public:
    MDPClient(std::string broker, int verbose);
    virtual ~MDPClient();

    //  Connect or reconnect to broker
    void connectToBroker();

    //  Set request timeout

    void setTimeout(int timeout);
    //  Set request retries

    void setRetries(int retries);

    //  Send request to broker and get reply by hook or crook
    //  Takes ownership of request message and destroys it when sent.
    //  Returns the reply message or NULL if there was no reply.

    zmq::message_t send(std::string service, zmq::message_t request);

 private:
    const std::string broker_;
    zmq::context_t *context_;
    zmq::socket_t *client_{nullptr};  //  Socket to broker
    const int verbose_;               //  Print activity to stdout
    int timeout_{2500};               //  Request timeout
    int retries_{3};                  //  Request retries
};