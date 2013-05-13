/*
Copyright (c) 2003, 2011, Oracle and/or its affiliates. All rights
reserved.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of
the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
02110-1301  USA
*/

#ifndef _TCP_DRIVER_H
#define	_TCP_DRIVER_H

#include <asio.hpp>
#include <pthread.h>
#include <functional>

#include "binlog_driver.h"
#include "bounded_buffer.h"
#include "protocol.h"

#define MAX_PACKAGE_SIZE 0xffffff

using asio::ip::tcp;

namespace mysql { namespace system {

class Binlog_tcp_driver;
struct Thread_data {
    Binlog_tcp_driver *tcp_driver;
};

class Binlog_tcp_driver : public Binary_log_driver
{
public:

    Binlog_tcp_driver(const std::string& user, const std::string& passwd,
                      const std::string& host, unsigned long port)
      : Binary_log_driver("", 4), m_host(host), m_user(user), m_passwd(passwd),
        m_port(port), m_socket(NULL), m_waiting_event(0), m_event_loop(0),
        m_total_bytes_transferred(0), m_shutdown(false),
        m_event_queue(new bounded_buffer<Binary_log_event *>(50))
    {
    }

    ~Binlog_tcp_driver()
    {
        delete m_event_queue;
        delete m_socket;
        free(this->thread_data);
    }

    /**
     * Connect using previously declared connection parameters.
     */
    int connect();

    /**
     * Blocking wait for the next binary log event to reach the client
     */
    int wait_for_next_event(mysql::Binary_log_event **event);

    /**
     * Reconnects to the master with a new binlog dump request.
     */
    int set_position(const std::string &str, unsigned long position);

    int get_position(std::string *str, unsigned long *position);

    const std::string& user() const { return m_user; }
    const std::string& password() const { return m_passwd; }
    const std::string& host() const { return m_host; }
    unsigned long port() const { return m_port; }

    static void *start(void *data);

protected:
    /**
     * Connects to a mysql server, authenticates and initiates the event
     * request loop.
     *
     * @param user The user account on the server side
     * @param passwd The password used to authenticate the user
     * @param host The DNS host name or IP of the server
     * @param port The service port number to connect to
     *
     *
     * @return Success or failure code
     *   @retval 0 Successfully established a connection
     *   @retval >1 An error occurred.
     */
    int connect(const std::string& user, const std::string& passwd,
                const std::string& host, long port,
                const std::string& binlog_filename="", size_t offset=4);

private:

    /**
     * Request a binlog dump and starts the event loop in a new thread
     * @param binlog_file_name The base name of the binlog files to query
     *
     */
    void start_binlog_dump(const std::string &binlog_file_name, size_t offset);

    /**
     * Handles a completed mysql server package header and put a
     * request for the body in the job queue.
     */
    void handle_net_packet_header(const asio::error_code& err, std::size_t bytes_transferred);

    /**
     * Handles a completed network package with the assumption that it contains
     * a binlog event.
     *
     * TODO rename to handle_event_log_packet?
     */
    void handle_net_packet(const asio::error_code& err, std::size_t bytes_transferred);

    /**
     * Called from handle_net_packet(). The function handle a stream of bytes
     * representing event packets which may or may not be complete.
     * It uses m_waiting_event and the size of the stream as parameters
     * in a state machine. If there is no m_waiting_event then the event
     * header must be parsed for the event packet length. This can only
     * be done if the accumulated stream of bytes are more than 19.
     * Next, if there is a m_waiting_event, it can only be completed if
     * event_length bytes are waiting on the stream.
     *
     * If none of these conditions are fullfilled, the function exits without
     * any action.
     *
     * @param err Not used
     * @param bytes_transferred The number of bytes waiting in the event stream
     *
     */
    void handle_event_packet(const asio::error_code& err, std::size_t bytes_transferred);

    /**
     * Executes io_service in a loop.
     * TODO Checks for connection errors and reconnects to the server
     * if necessary.
     */
    void start_event_loop(void);

    /**
     * Reconnect to the server by first calling disconnect and then connect.
     */
    void reconnect(void);

    /**
     * Disconnet from the server. The io service must have been stopped before
     * this function is called.
     * The event queue is emptied.
     */
    void disconnect(void);

    /**
     * Terminates the io service and sets the shudown flag.
     * this causes the event loop to terminate.
     */
    void shutdown(void);

    pthread_t *m_event_loop;
    asio::io_service m_io_service;
    tcp::socket *m_socket;
    bool m_shutdown;

    /**
     * Temporary storage for a handshake package
     */
    st_handshake_package m_handshake_package;

    /**
     * Temporary storage for an OK package
     */
    st_ok_package m_ok_package;

    /**
     * Temporary storage for an error package
     */
    st_error_package m_error_package;

    /**
     * each bin log event starts with a 19 byte long header
     * We use this sturcture every time we initiate an async
     * read.
     */
    uint8_t m_event_header[19];

    /**
     *
     */
    uint8_t m_net_header[4];

    /**
     *
     */
    uint8_t m_net_packet[MAX_PACKAGE_SIZE];
    asio::streambuf m_event_stream_buffer;
    char * m_event_packet;

    /**
     * This pointer points to an object constructed from event
     * stream during async communication with
     * server. If it is 0 it means that no event has been
     * constructed yet.
     */
    Log_event_header *m_waiting_event;
    Log_event_header m_log_event_header;
    /**
     * A ring buffer used to dispatch aggregated events to the user application
     */
    bounded_buffer<Binary_log_event *> *m_event_queue;

    std::string m_user;
    std::string m_host;
    std::string m_passwd;
    long m_port;

    uint64_t m_total_bytes_transferred;
    struct mysql::system::Thread_data *thread_data;
};

class Read_handler {
public:
    void (Binlog_tcp_driver:: *method)(const asio::error_code& err, std::size_t bytes_transferred);
    Binlog_tcp_driver *tcp_driver;

    void operator()(const asio::error_code& err, std::size_t bytes_transferred)
    {
        (tcp_driver->*method)(err, bytes_transferred);
    }
};

class Shutdown_handler {
public:
    void (Binlog_tcp_driver:: *method)();
    Binlog_tcp_driver *tcp_driver;

    void operator()()
    {
        (tcp_driver->*method)();
    }
};

/**
 * Sends a SHOW MASTER STATUS command to the server and retrieve the
 * current binlog position.
 *
 * @return False if the operation succeeded, true if it failed.
 */
bool fetch_master_status(tcp::socket *socket, std::string *filename, unsigned long *position);
/**
 * Sends a SHOW BINARY LOGS command to the server and stores the file
 * names and sizes in a map.
 */
bool fetch_binlogs_name_and_size(tcp::socket *socket, std::map<std::string, unsigned long> &binlog_map);

int authenticate(tcp::socket *socket, const std::string& user,
                 const std::string& passwd,
                 const st_handshake_package &handshake_package);

tcp::socket *
sync_connect_and_authenticate(asio::io_service &io_service, const std::string &user,
                              const std::string &passwd, const std::string &host, long port);


} }

#endif	/* _TCP_DRIVER_H */
