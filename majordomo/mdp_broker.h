#pragma once

#include <map>
#include <set>
#include <string>
#include <zmq.hpp>
#include <zmq_addon.hpp>

struct Service;
struct Worker;
class MDPBroker {
 public:
    MDPBroker(int verbose);
    virtual ~MDPBroker();

    void bind(std::string endpoint);

    void startBrokering();

 private:
    void purgeWorkers();
    Service *serviceRequire(std::string name);
    void serviceDispatch(Service *service, zmq::multipart_t *msg);
    void serviceInternal(std::string service_name, zmq::multipart_t *msg);
    Worker *workerRequire(std::string identity);
    void workerDelete(Worker *worker, int disconnect);
    void workerProcess(std::string sender, zmq::multipart_t *msg);
    void workerSend(Worker *worker, const char *command, std::string option, zmq::multipart_t *msg);
    void workerWaiting(Worker *worker);
    void clientProcess(std::string sender, zmq::multipart_t *msg);

    static constexpr int kHeartbeatLivenes = 3;
    static constexpr int kHeartbeatIntervalMs = 2500;
    static constexpr int kHeartbeatExpiryMs = kHeartbeatIntervalMs * kHeartbeatLivenes;
    zmq::context_t context_;
    zmq::socket_t broker_;
    bool verbose_;

    std::map<std::string, Service *> services_;
    std::map<std::string, Worker *> workers_;
    std::set<Worker *> waiting_;
    std::chrono::steady_clock::time_point heartbeat_tp_;
};