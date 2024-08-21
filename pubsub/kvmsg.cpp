#include "kvmsg.h"

KVMsg::KVMsg() : key_(""), value_(""), sequence_(0) {
}

KVMsg::KVMsg(std::string key, std::string value, uint64_t sequence)
        : key_(key), value_(value), sequence_(sequence) {
}

bool KVMsg::recv(zmq::socket_t &socket) {
    zmq::multipart_t recv_msgs;
    bool ret = recv_msgs.recv(socket);
    if (!ret) {
        return false;  //  Interrupted
    }
    key_ = recv_msgs.front().to_string();
    std::memcpy(&sequence_, recv_msgs[1].data(), sizeof(uint64_t));
    value_ = recv_msgs.back().to_string();
    return true;
}

bool KVMsg::send(zmq::socket_t &socket) {
    if (!socket.send(zmq::buffer(key_), zmq::send_flags::sndmore)) {
        return false;
    }
    if (!socket.send(zmq::buffer(&sequence_, sizeof(sequence_)), zmq::send_flags::sndmore)) {
        return false;
    }
    if (!socket.send(zmq::buffer(value_), zmq::send_flags::none)) {
        return false;
    }
    return true;
}

std::string KVMsg::key() {
    return key_;
}

std::string KVMsg::value() {
    return value_;
}

uint64_t KVMsg::sequence() {
    return sequence_;
}

size_t KVMsg::size() {
    return value_.size();
}

void KVMsg::set_key(const std::string &key) {
    key_ = key;
}

void KVMsg::set_value(const std::string &value) {
    value_ = value;
}

void KVMsg::set_sequence(const uint64_t &sequence) {
    sequence_ = sequence;
}

bool KVMsg::store(std::unordered_map<std::string, std::string> &hash) {
    if (key_.empty()) {
        return false;
    }

    if (value_.empty()) {
        hash.erase(key_);
    } else {
        hash[key_] = value_;
    }
    return true;
}

std::string KVMsg::dump() {
    std::stringstream ss;
    ss << key_ << " : " << value_ << " (" << sequence_ << ")";
    return ss.str();
}
