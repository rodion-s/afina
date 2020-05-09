#include "Connection.h"

#include <iostream>
#include <sys/uio.h>
#include <sys/socket.h>
//#include <sys/unistd.h>

namespace Afina {
namespace Network {
namespace MTnonblock {

// See Connection.h
	
void Connection::Start() {
	std::lock_guard<std::mutex> lock(_mutex);
	_logger->debug("Connection on {} socket started", _socket);
	_event.events = EPOLLIN | EPOLLRDHUP | EPOLLERR;
    _event.data.fd = _socket;
    _event.data.ptr = this;

    argument_for_command.resize(0);
    command_to_execute.reset();
    arg_remains = 0;

    alrdy_prsed_bytes = 0;
    written_position = 0;
    responses.clear();
    parser.Reset();
}

// See Connection.h
void Connection::OnError() {
	_logger->error("Connection on {} socket has error", _socket);
	_is_alive.store(false, std::memory_order_release);
}

// See Connection.h
void Connection::OnClose() {
	_logger->debug("Close {} socket", _socket);
    _is_alive.store(false, std::memory_order_release);
}

// See Connection.h
void Connection::DoRead() {
	std::lock_guard<std::mutex> lock(_mutex);
	_logger->debug("Read from {} socket", _socket);
	try {
        int readed_bytes = 0;
        while ((readed_bytes = read(_socket, client_buffer + alrdy_prsed_bytes, sizeof(client_buffer) - alrdy_prsed_bytes)) > 0) {
            _logger->debug("Got {} bytes from socket", readed_bytes);
            alrdy_prsed_bytes += readed_bytes;
            // Single block of data readed from the socket could trigger inside actions a multiple times,
            // for example:
            // - read#0: [<command1 start>]
            // - read#1: [<command1 end> <argument> <command2> <argument for command 2> <command3> ... ]
            while (alrdy_prsed_bytes > 0) {
                _logger->debug("Process {} bytes", alrdy_prsed_bytes);
                // There is no command yet
                if (!command_to_execute) {
                    std::size_t parsed = 0;
                    if (parser.Parse(client_buffer, alrdy_prsed_bytes, parsed)) {
                        // There is no command to be launched, continue to parse input stream
                        // Here we are, current chunk finished some command, process it
                        _logger->debug("Found new command: {} in {} bytes", parser.Name(), parsed);
                        command_to_execute = parser.Build(arg_remains);
                        if (arg_remains > 0) {
                            arg_remains += 2;
                        }
                    }

                    // Parsed might fails to consume any bytes from input stream. In real life that could happens,
                    // for example, because we are working with UTF-16 chars and only 1 byte left in stream
                    if (parsed == 0) {
                        break;
                    } else {
                        std::memmove(client_buffer, client_buffer + parsed, readed_bytes - parsed);
                        alrdy_prsed_bytes -= parsed;
                    }
                }

                // There is command, but we still wait for argument to arrive...
                if (command_to_execute && arg_remains > 0) {
                    _logger->debug("Fill argument: {} bytes of {}", readed_bytes, arg_remains);
                    // There is some parsed command, and now we are reading argument
                    std::size_t to_read = std::min(arg_remains, std::size_t(readed_bytes));

                    if (to_read == arg_remains) {
                    	argument_for_command.append(client_buffer, to_read - 2);	
                    } else {
                    	argument_for_command.append(client_buffer, to_read);
                    }
                    
                    std::memmove(client_buffer, client_buffer + to_read, readed_bytes - to_read);
                    arg_remains -= to_read;
                    alrdy_prsed_bytes -= to_read;
                }

                // Thre is command & argument - RUN!
                if (command_to_execute && arg_remains == 0) {
                    _logger->debug("Start command execution");

                    std::string result;
                    command_to_execute->Execute(*pStorage, argument_for_command, result);

                    // Send response
                    result += "\r\n";
                    _event.events = EPOLLIN | EPOLLRDHUP | EPOLLERR | EPOLLOUT;
                    responses.push_back(result);
                    
                    
                    // Prepare for the next command
                    command_to_execute.reset();
                    argument_for_command.resize(0);
                    parser.Reset();
                }
            } // while (readed_bytes)
        }

        if (readed_bytes == 0 || errno == EAGAIN || errno == EWOULDBLOCK) {
            _logger->debug("Connection closed");
        } else {
            throw std::runtime_error(std::string(strerror(errno)));
        }
    } catch (std::runtime_error &ex) {
        _event.events = EPOLLIN | EPOLLRDHUP | EPOLLERR | EPOLLOUT;
        std::string result("ERROR: Failed to process connection\r\n");

        if (send(_socket, result.data(), result.size(), 0) <= 0) {
        	_logger->error("Failed to send error response to socket\r\n");
        }
        OnError();
    }
}

// See Connection.h
void Connection::DoWrite() {
	std::lock_guard<std::mutex> lock(_mutex);
	_logger->debug("DoWrite on {}", _socket);

	if (responses.empty()) {
        return;
	}

    struct iovec msgs[responses.size()];
    msgs[0].iov_len = responses[0].size() - written_position;
    msgs[0].iov_base = &(responses[0][0]) + written_position;

    for (int i = 1; i < responses.size(); ++i) {
        msgs[i].iov_len = responses[i].size();
        msgs[i].iov_base = &(responses[i][0]);
    }
    ssize_t written = writev(_socket, msgs, responses.size());
    if (written <= 0) {
        OnError();
    }
    written_position += written;

    int i = 0;
    for (i = 0; i < responses.size() && written_position - msgs[i].iov_len >= 0; ++i) {
        written_position -= msgs[i].iov_len;
    }

    responses.erase(responses.begin(), responses.begin() + i);
    if (responses.empty()) {
        _event.events = EPOLLIN | EPOLLRDHUP | EPOLLERR;
    }
}

} // namespace MTnonblock
} // namespace Network
} // namespace Afina
