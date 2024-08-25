#include <deque>
#include <iostream>
#include <list>
#include <set>
#include <string>
#include <zmq.hpp>
#include <zmq_addon.hpp>

#include "mdp.h"
#include "mdp_broker.h"

struct Service {
    Service(std::string name) : name_(name) {
    }
    ~Service() {
        for (size_t i = 0; i < requests_.size(); i++) {
            delete requests_[i];
        }
    }

    const std::string name_;                   //  Service name
    std::deque<zmq::multipart_t *> requests_;  //  List of client requests
    std::list<Worker *> waiting_;              //  List of waiting workers
    size_t workers_;                           //  How many workers we have
};

struct Worker {
    Worker(std::string identity,
           Service *service = nullptr,
           std::chrono::steady_clock::time_point expiry = std::chrono::steady_clock::time_point())
            : identity_(identity), service_(service), expiry_(expiry) {
    }
    std::string identity_;                          //  Address of worker
    Service *service_;                              //  Owning service, if known
    std::chrono::steady_clock::time_point expiry_;  //  Expires at unless heartbeat
};

MDPBroker::MDPBroker(int verbose)
        : context_(zmq::context_t(1)),
          broker_(zmq::socket_t(context_, ZMQ_ROUTER)),
          verbose_(verbose),
          heartbeat_tp_(std::chrono::steady_clock::now()) {
}

MDPBroker::~MDPBroker() {
    while (!services_.empty()) {
        delete services_.begin()->second;
        services_.erase(services_.begin());
    }
    while (!workers_.empty()) {
        delete workers_.begin()->second;
        workers_.erase(workers_.begin());
    }
    broker_.set(zmq::sockopt::linger, 0);
    broker_.close();
    context_.shutdown();
    context_.close();
}

void MDPBroker::bind(std::string endpoint) {
    broker_.bind(endpoint);
    std::cout << "I: MDP broker/0.2.0 is active at " << endpoint << std::endl;
}

void MDPBroker::startBrokering() {
    while (true) {
        zmq::pollitem_t items[] = {{broker_, 0, ZMQ_POLLIN, 0}};
        int rc = zmq::poll(items, 1, std::chrono::milliseconds(kHeartbeatIntervalMs));
        if (rc == -1) {
            break;
        }

        //  Process next input message, if any
        if (items[0].revents & ZMQ_POLLIN) {
            zmq::multipart_t message;
            if (!message.recv(broker_)) {
                break;  //  Interrupted
            }
            if (verbose_) {
                std::cout << "I: received message: " << std::endl;
                std::cout << message << std::endl;
            }
            auto sender = message.pop().to_string();
            auto empty = message.pop();
            auto header = message.pop().to_string();

            if (header == k_mdp_client) {
                clientProcess(sender, &message);
            } else if (header == k_mdpw_worker) {
                workerProcess(sender, &message);
            } else {
                std::cout << "E: invalid message:" << std::endl;
                std::cout << message << std::endl;
            }
        }
        //  Disconnect and delete any expired workers
        //  Send heartbeats to idle workers if needed
        if (std::chrono::steady_clock::now() > heartbeat_tp_) {
            purgeWorkers();
            for (auto worker : waiting_) {
                workerSend(worker, k_mdpw_heartbeat.data(), "", nullptr);
            }
            heartbeat_tp_ += std::chrono::milliseconds(kHeartbeatIntervalMs);
        }
    }
}

void MDPBroker::purgeWorkers() {
    std::deque<Worker *> toDelete;
    auto now = std::chrono::steady_clock::now();
    for (auto worker = waiting_.begin(); worker != waiting_.end(); ++worker) {
        if ((*worker)->expiry_ <= now) toDelete.push_back(*worker);
    }
    for (auto worker = toDelete.begin(); worker != toDelete.end(); ++worker) {
        if (verbose_) {
            std::cout << "I: deleting expired worker: " << (*worker)->identity_ << std::endl;
        }
        workerDelete(*worker, 0);
    }
}

Service *MDPBroker::serviceRequire(std::string name) {
    assert(!name.empty());
    if (services_.count(name)) {
        return services_.at(name);
    }
    Service *service = new Service(name);
    services_.insert(std::pair{name, service});
    if (verbose_) {
        std::cout << "I: created service " << name << std::endl;
    }
    return service;
}

void MDPBroker::serviceDispatch(Service *service, zmq::multipart_t *msg) {
    assert(service);
    if (msg) {  //  Queue message if any
        service->requests_.push_back(msg);
    }

    purgeWorkers();
    while (!service->waiting_.empty() && !service->requests_.empty()) {
        // Choose the most recently seen idle worker; others might be about to expire
        auto worker = service->waiting_.begin();
        auto next = worker;
        for (++next; next != service->waiting_.end(); ++next) {
            if ((*next)->expiry_ > (*worker)->expiry_) worker = next;
        }

        zmq::multipart_t *msg = service->requests_.front();
        service->requests_.pop_front();
        workerSend(*worker, k_mdpw_request.data(), "", msg);
        waiting_.erase(*worker);
        service->waiting_.erase(worker);
        // delete msg;
    }
}

void MDPBroker::serviceInternal(std::string service_name, zmq::multipart_t *msg) {
    zmq::multipart_t response;

    // Remove & save client return envelope in response
    for (int i = 0; i < msg->size(); ++i) {
        auto part = msg->pop();
        response.add(std::move(part));
        if (response.back().empty()) {
            break;
        }
    }
    response.addstr(k_mdp_client.data());
    response.addstr(service_name);

    if (service_name.compare("mmi.service") == 0) {
        Service *service = services_[msg->pop().to_string()];
        if (service && service->workers_) {
            response.addstr("200");
        } else {
            response.addstr("404");
        }
    } else {
        response.addstr("501");
    }

    response.send(broker_);
}

Worker *MDPBroker::workerRequire(std::string identity) {
    assert(!identity.empty());

    //  self->workers is keyed off worker identity
    if (workers_.count(identity)) {
        return workers_.at(identity);
    } else {
        Worker *worker = new Worker(identity);
        workers_.insert(std::make_pair(identity, worker));
        if (verbose_) {
            std::cout << "I: registering new worker: " << identity << std::endl;
        }
        return worker;
    }
}

void MDPBroker::workerDelete(Worker *worker, int disconnect) {
    assert(worker);
    if (disconnect) {
        workerSend(worker, k_mdpw_disconnect.data(), "", NULL);
    }

    if (worker->service_) {
        for (auto it = worker->service_->waiting_.begin();
             it != worker->service_->waiting_.end();) {
            if (*it == worker) {
                it = worker->service_->waiting_.erase(it);
            } else {
                ++it;
            }
        }
        worker->service_->workers_--;
    }
    waiting_.erase(worker);
    //  This implicitly calls the worker destructor
    workers_.erase(worker->identity_);
    delete worker;
}

void MDPBroker::workerProcess(std::string sender, zmq::multipart_t *msg) {
    assert(msg && msg->size() >= 1);  //  At least, command

    std::string command = msg->pop().to_string();
    bool worker_ready = workers_.count(sender) > 0;
    Worker *worker = workerRequire(sender);

    if (command.compare(k_mdpw_ready.data()) == 0) {
        if (worker_ready) {  //  Not first command in session
            workerDelete(worker, 1);
        } else {
            if (sender.size() >= 4  //  Reserved service name
                && sender.find_first_of("mmi.") == 0) {
                workerDelete(worker, 1);
            } else {
                //  Attach worker to service and mark as idle
                std::string service_name = msg->pop().to_string();
                worker->service_ = serviceRequire(service_name);
                worker->service_->workers_++;
                workerWaiting(worker);
            }
        }
    } else {
        if (command == k_mdpw_reply.data()) {
            if (worker_ready) {
                //  Remove & save client return envelope and insert the
                //  protocol header and service name, then rewrap envelope.
                std::string client = msg->pop().to_string();
                msg->pop();  // Empty delimiter
                msg->pushstr(worker->service_->name_);
                msg->pushstr(k_mdp_client.data());
                msg->pushstr("");
                msg->pushstr(client);
                msg->send(broker_);
                workerWaiting(worker);
            } else {
                workerDelete(worker, 1);
            }
        } else {
            if (command.compare(k_mdpw_heartbeat.data()) == 0) {
                if (worker_ready) {
                    worker->expiry_ = std::chrono::steady_clock::now() +
                                      std::chrono::milliseconds(kHeartbeatExpiryMs);
                } else {
                    workerDelete(worker, 1);
                }
            } else {
                if (command == k_mdpw_disconnect.data()) {
                    workerDelete(worker, 0);
                } else {
                    std::cout << "E: invalid input message " << (int)*command.c_str() << std::endl;
                    std::cout << msg << std::endl;
                }
            }
        }
    }
    // delete msg;
}

void MDPBroker::workerSend(Worker *worker,
                           const char *command,
                           std::string option,
                           zmq::multipart_t *msg) {
    // std::cout << "workerSend" << std::endl;
    if (!msg) {
        msg = new zmq::multipart_t();
    }

    //  Stack protocol envelope to start of message
    if (option.size() > 0) {  //  Optional frame after command
        msg->pushstr(option);
    }
    msg->pushstr(command);
    msg->pushstr(k_mdpw_worker.data());
    //  Stack routing envelope to start of message
    msg->pushstr("");
    msg->pushstr(worker->identity_);

    if (verbose_) {
        std::cout << "I: sending " << mdps_commands[(int)*command].data() << " to worker"
                  << std::endl;
        std::cout << msg << std::endl;
    }
    msg->send(broker_);
    // delete msg;
}

void MDPBroker::workerWaiting(Worker *worker) {
    assert(worker);
    //  Queue to broker and service waiting lists
    waiting_.insert(worker);
    worker->service_->waiting_.push_back(worker);
    worker->expiry_ =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(kHeartbeatExpiryMs);
    // Attempt to process outstanding requests
    serviceDispatch(worker->service_, nullptr);
}

void MDPBroker::clientProcess(std::string sender, zmq::multipart_t *msg) {
    assert(msg && msg->size() >= 2);  //  Service name + body

    std::string service_name = msg->pop().to_string();
    Service *service = serviceRequire(service_name);
    //  Set reply return address to client sender
    msg->pushstr("");
    msg->pushstr(sender);
    if (service_name.length() >= 4 && service_name.find_first_of("mmi.") == 0) {
        serviceInternal(service_name, msg);
    } else {
        serviceDispatch(service, msg);
    }
}
