/*
 * Copyright (C) 2013 by Tim Diels
 *
 * This file is part of pwnat.
 *
 * pwnat is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * pwnat is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with pwnat.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "ProxyServer.h"
#include <boost/bind.hpp>
#include <cassert>
#include <pwnat/UDTSocket.h>
#include <pwnat/constants.h>
#include <pwnat/packet.h>
#include "ProxyClient.h"

using namespace std;

ProxyServer::ProxyServer() :
    m_socket(m_io_service, boost::asio::ip::icmp::endpoint(boost::asio::ip::icmp::v4(), 0)),
    m_udt_service(m_io_service)
{
    m_socket.connect(boost::asio::ip::icmp::endpoint(boost::asio::ip::address::from_string(g_icmp_echo_destination), 0));
    send_icmp_echo();
    start_receive();
}

void ProxyServer::run() {
    cout << "running server" << endl;
    m_io_service.run();
}

void ProxyServer::send_icmp_echo() {
    auto buffer = boost::asio::buffer(&g_icmp_echo, sizeof(g_icmp_echo));
    auto callback = boost::bind(&ProxyServer::handle_send, this, boost::asio::placeholders::error);
    m_socket.async_send(buffer, callback);
}

void ProxyServer::handle_send(const boost::system::error_code& error) {
    if (error) {
        cerr << "Warning: send icmp echo failed: " << error.message() << endl;
    }
}

void ProxyServer::start_receive() {
    auto callback = boost::bind(&ProxyServer::handle_receive, this, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred);
    m_socket.async_receive(boost::asio::buffer(m_receive_buffer), callback);
}

void ProxyServer::handle_receive(boost::system::error_code error, size_t bytes_transferred) {
    if (error) {
        cerr << "Warning: icmp receive error: " << error.message() << endl;
    }
    else {
        cout << "received icmp" << endl;
        auto ip_header = reinterpret_cast<ip*>(m_receive_buffer.data());
        const int ip_header_size = ip_header->ip_hl * 4;
        auto header = reinterpret_cast<icmp_ttl_exceeded*>(m_receive_buffer.data() + ip_header_size);

        if (bytes_transferred == ip_header_size + sizeof(icmp_ttl_exceeded) &&
            header->ip_header.ip_hl == 5 &&
            header->icmp.type == ICMP_TIME_EXCEEDED &&
            memcmp(&header->original_icmp, &g_icmp_echo, sizeof(g_icmp_echo)) == 0) 
        {
            // somebody wants to connect, so let them
            cout << "Accepting new proxy client" << endl;
            boost::asio::ip::address_v4 destination(ntohl(ip_header->ip_src.s_addr));
            new ProxyClient(m_io_service, m_udt_service, destination); // TODO shouldn't do this more than once! (keep a map of ip -> client)
        }
    }

    start_receive();
}
