#include <map>
#include <string>
#include <zmq.hpp>
#include <zmq_addon.hpp>

class KVMsg {
 public:
    KVMsg();
    KVMsg(std::string key, std::string value, uint64_t sequence);

    bool recv(zmq::socket_t &socket);
    bool send(zmq::socket_t &socket);
    std::string key();
    std::string value();
    uint64_t sequence();
    size_t size();

    void set_key(const std::string &key);
    void set_value(const std::string &value);
    void set_sequence(const uint64_t &sequence);

    bool store(std::unordered_map<std::string, std::string> &hash);

    std::string dump();

 private:
    std::string key_;
    std::string value_;
    uint64_t sequence_;
};
