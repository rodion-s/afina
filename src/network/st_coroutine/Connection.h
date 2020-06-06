
#ifndef AFINA_NETWORK_ST_COROUTINE_CONNECTION_H
#define AFINA_NETWORK_ST_COROUTINE_CONNECTION_H

#include <afina/execute/Command.h>
#include <cstring>

#include <afina/Storage.h>
#include <afina/coroutine/Engine.h>
#include <protocol/Parser.h>
#include <spdlog/logger.h>
#include <sys/epoll.h>

namespace Afina {
namespace Network {
    namespace STcoroutine {

        class Connection {
        public:
            Connection(int s, std::shared_ptr<Afina::Storage> ps, std::shared_ptr<spdlog::logger> lg)
                : _socket(s)
                , pStorage(ps)
                , _logger(lg)
            {
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
            void work(Afina::Coroutine::Engine& engine);

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
            int written_position = 0;
            std::vector<std::string> responses;

            Afina::Coroutine::Engine::context* _ctx;
            Afina::Coroutine::Engine::context* cour_worker;

            char client_buffer[4096] = { 0 };
        };
    } // namespace STcoroutine
} // namespace Network
} // namespace Afina

#endif // AFINA_NETWORK_ST_COROUTINE_CONNECTION_H
