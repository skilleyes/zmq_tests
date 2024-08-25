#include <iostream>
#include <map>
#include <string>
#include <thread>

#include "mdp_broker.h"

int main(int argc, char *argv[]) {
    int verbose = (argc > 1 && std::string(argv[1]) == "-v");
    MDPBroker broker(verbose);
    broker.bind("tcp://*:5555");

    broker.startBrokering();
}
