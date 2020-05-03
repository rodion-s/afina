#ifndef AFINA_NETWORK_ST_NONBLOCKING_CONNECTION_H
#define AFINA_NETWORK_ST_NONBLOCKING_CONNECTION_H

#include <cstring>

#include <sys/epoll.h>

#include <spdlog/logger.h>
#include <protocol/Parser.h>

#include <afina/Storage.h>
#include <afina/execute/Command.h>

namespace Afina {
namespace Network {
namespace STnonblock {

class Connection {
public:
    Connection(int s, std::shared_ptr<Afina::Storage> ps, std::shared_ptr<spdlog::logger> lg) : _socket(s),
                pStorage(ps), _logger(lg) {
        std::memset(&_event, 0, sizeof(struct epoll_event));
        _event.data.ptr = this;
    }

    inline bool isAlive() const { return _is_alive; }

    void Start();

protected:
    void OnError();
    void OnClose();
    void DoRead();
    void DoWrite();

private:
    friend class ServerImpl;

    int _socket;
    struct epoll_event _event;

    bool _is_alive = true;

    std::shared_ptr<spdlog::logger> _logger;
    std::shared_ptr<Afina::Storage> pStorage;

    Protocol::Parser parser;
    std::unique_ptr<Execute::Command> command_to_execute;
    std::size_t arg_remains = 0;
    std::string argument_for_command;

    int alrdy_prsed_bytes = 0;
    ssize_t written_position = 0;
    std::vector<std::string> responses;

    char client_buffer[4096] = { 0 };
};


} // namespace STnonblock
} // namespace Network
} // namespace Afina

#endif // AFINA_NETWORK_ST_NONBLOCKING_CONNECTION_H
