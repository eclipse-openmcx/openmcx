// Copyright: 2024 AVL List GmbH

#include "Slave.hpp"

#include <cstdint>
#include <exception>
#include <iostream>
#include <string>


static void show_usage(const std::string & name) {
    std::cerr << "Usage: " << name << " <option(s)>"
              << "Options:\n"
              << "\t-p PORT       \tPort used to listen for control packets\n"
              << "\t-o DESTINATION\tSlave description file written by the slave\n"
              << "\n"
              << "If binding on PORT is not possible (due to it being used by another\n"
              << "application, the first free port (greater than PORT) will be used.\n"
              << std::endl;
}


enum class ArgMode {
    pos_args,
    port_def,
    output_def
};


int main(int argc, char *argv[]) {
    if (argc < 1) {
        show_usage(argv[0]);
        return 1;
    }

    std::string host = "127.0.0.1";
    uint16_t port = 40100;
    std::string output_file = "";

    ArgMode mode = ArgMode::pos_args;
    for (size_t i = 1; i < static_cast<size_t>(argc); i++) {
        std::string arg = argv[i];
        if (mode == ArgMode::pos_args && arg == "-p") {
            mode = ArgMode::port_def;
            continue;
        } else if (mode == ArgMode::pos_args && arg == "-o") {
            mode = ArgMode::output_def;
            continue;
        } else if (mode == ArgMode::pos_args) {
            show_usage(argv[0]);
            return 1;
        }

        if (mode == ArgMode::port_def) {
            port = std::atoi(arg.c_str());
            mode = ArgMode::pos_args;
            continue;
        } else if (mode == ArgMode::output_def) {
            output_file = arg;
            mode = ArgMode::pos_args;
            continue;
        } else {
            show_usage(argv[0]);
            return 1;
        }
    }

    while (true) {
        try {
            std::cout << "Control port: " << port << std::endl;
            Slave slave(host, port, output_file);
            slave.start();
            break;
        } catch (const std::system_error &) {
            std::cout << "Port in use" << std::endl;
            port++;
            continue;
        } catch (const std::exception& e) {
            std::cout << e.what() << std::endl;
            return -1;
        }
    }

    return 0;
}