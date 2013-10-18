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
#include <pwnat/packet.h>
#include <pwnat/util.h>

#include <pwnat/namespaces.h>

ProxyServer::ProxyServer(const ProgramArgs& args) :
    Application(args),
    m_socket(m_io_service, asio::ip::icmp::endpoint(args.icmp_version(), 0)),
    m_icmp_timer(m_io_service)
{
    args.get_icmp_echo(m_icmp_echo, 0u, 0u);
    m_socket.connect(asio::ip::icmp::endpoint(args.icmp_echo_destination(), 0));
    send_icmp_echo();
    start_receive();
}

ProxyServer::~ProxyServer() {
    for (auto entry : m_clients) {
        delete entry.second;
    }
}

void ProxyServer::send_icmp_echo() {
    // send
    {
        auto buffer = asio::buffer(m_icmp_echo);
        auto callback = bind(&ProxyServer::handle_send, this, asio::placeholders::error);
        m_socket.async_send(buffer, callback);
    }

    // set timer
    {
        m_icmp_timer.expires_from_now(boost::posix_time::seconds(5));
        auto callback = bind(&ProxyServer::handle_icmp_timer_expired, this, asio::placeholders::error);
        m_icmp_timer.async_wait(callback);
    }
}

void ProxyServer::handle_icmp_timer_expired(const boost::system::error_code& error) {
    if (error) {
        if (error.value() != asio::error::operation_aborted) {  // aborted = timer cancelled
            cerr << "Warning: Unexpected timer error: " << error.message() << endl;
        }
    }
    else {
        send_icmp_echo();
    } 
}

void ProxyServer::handle_send(const boost::system::error_code& error) {
    if (error) {
        cerr << "Warning: send icmp echo failed: " << error.message() << endl;
    }
}

void ProxyServer::start_receive() {
    auto callback = bind(&ProxyServer::handle_receive, this, asio::placeholders::error, asio::placeholders::bytes_transferred);
    m_socket.async_receive_from(asio::buffer(m_receive_buffer), m_endpoint, callback);
}

void ProxyServer::handle_receive(boost::system::error_code error, size_t bytes_transferred) {
    using asio::ip::address;
    using asio::ip::address_v4;
    using asio::ip::address_v6;

    if (error) {
        cerr << "Warning: icmp receive error: " << error.message() << endl;
    }
    else {
        auto& args = Application::instance().args();

        if (args.is_ipv6()) {
            // Note: Buffer starts with ICMPv6 header (there's no such thing as IP_HDRINCL for raw IPv6 sockets)
            auto header = reinterpret_cast<icmp6_ttl_exceeded*>(m_receive_buffer.data());
            if (bytes_transferred == sizeof(icmp6_ttl_exceeded) &&
                address(address_v6(*reinterpret_cast<address_v6::bytes_type*>(&header->ip_header.ip6_dst))) == args.icmp_echo_destination() &&
                header->icmp.icmp6_type == ICMP6_TIME_EXCEEDED)
            {
                ProxyClient::Id client_id;
                client_id.address = m_endpoint.address();
                client_id.flow_id = header->original_icmp.icmp6_id;  // we've abused the id field to store the flow id in
                //u_int16_t client_port = header->original_icmp.icmp6_seq; // TODO will also need to include client port in the client 'id' of clients map
                add_client(client_id);
            }
        }
        else {
            // Note: Buffer starts with IPv4 header
            const int ip_header_size = reinterpret_cast<ip*>(m_receive_buffer.data())->ip_hl * 4;
            auto header = reinterpret_cast<icmp_ttl_exceeded*>(m_receive_buffer.data() + ip_header_size);

            if (bytes_transferred == ip_header_size + sizeof(icmp_ttl_exceeded) &&
                address(address_v4(*reinterpret_cast<address_v4::bytes_type*>(&header->ip_header.ip_dst))) == args.icmp_echo_destination() &&
                header->icmp.type == ICMP_TIME_EXCEEDED)
            {
                ProxyClient::Id client_id; // TODO ctor
                client_id.address = m_endpoint.address();
                client_id.flow_id = header->original_icmp.un.echo.id;  // we've abused the id field to store the flow id in
                //u_int16_t client_port = header->original_icmp.un.echo.sequence;
                add_client(client_id);
            }
        }

        
    }

    start_receive();
}

void ProxyServer::add_client(ProxyClient::Id& id) {
    if (m_clients.find(id) == m_clients.end()) {
        cout << "Accepting new proxy client" << endl;
        try {
        m_clients[id] = new ProxyClient(*this, m_io_service, m_udt_service, id);
        }
        catch (const exception& e) {
            cerr << "Failed to create client: " << e.what() << endl;
        }
        catch (...) {
            cerr << "Failed to create client: unknown error" << endl;
        }
    }
}

void ProxyServer::kill_client(ProxyClient& client) {
    m_clients.erase(client.id());
    delete &client;
}
