#ifndef AFINA_NETWORK_MT_NONBLOCKING_CONNECTION_H
#define AFINA_NETWORK_MT_NONBLOCKING_CONNECTION_H

#include <cstring>

#include <sys/epoll.h>

#include <spdlog/logger.h>
#include <protocol/Parser.h>

#include <afina/Storage.h>
#include <afina/execute/Command.h>


namespace Afina {
namespace Network {
namespace MTnonblock {

class Connection {
public:
    Connection(int s, std::shared_ptr<Afina::Storage> ps, std::shared_ptr<spdlog::logger> lg) : _socket(s),
                pStorage(ps), _logger(lg) {
        std::memset(&_event, 0, sizeof(struct epoll_event));
        _event.data.ptr = this;
        _is_alive.store(true);
    }

    inline bool isAlive() const { return _is_alive.load(std::memory_order_acquire); }

    void Start();

protected:
    void OnError();
    void OnClose();
    void DoRead();
    void DoWrite();

private:
    friend class Worker;
    friend class ServerImpl;

    int _socket;
    struct epoll_event _event;

    std::atomic<bool> _is_alive;
    std::mutex _mutex;

    std::shared_ptr<spdlog::logger> _logger;
    std::shared_ptr<Afina::Storage> pStorage;

    Protocol::Parser parser;
    std::unique_ptr<Execute::Command> command_to_execute;
    std::size_t arg_remains;
    std::string argument_for_command;

    int alrdy_prsed_bytes;
    ssize_t written_position;
    std::vector<std::string> responses;
    char client_buffer[4096] = { 0 };


    std::atomic<bool> _reading_finished;
};

} // namespace MTnonblock
} // namespace Network
} // namespace Afina

#endif // AFINA_NETWORK_MT_NONBLOCKING_CONNECTION_H
