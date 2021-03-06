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

#pragma once

#include <memory>
#include <boost/asio.hpp>
#include <pwnat/UDTSocket.h>
#include <pwnat/Socket.h>
#include <pwnat/packet.h>

class UDTService;

class TCPClient {
public:
    /**
     * flow_id: Identifies which flow on the UDT connection to pick (allows reusing the UDT ports)
     */
    TCPClient(UDTService& udt_service, boost::asio::ip::tcp::socket* tcp_socket, u_int16_t flow_id);
    ~TCPClient();

private:
    void die();
    void send_udt_flow_init(std::string remote_host, u_int16_t remote_port);
    void build_icmp_ttl_exceeded(u_int16_t flow_id, u_int16_t client_port);
    void send_icmp_ttl_exceeded();
    void handle_send(const boost::system::error_code& error);
    void handle_icmp_timer_expired(const boost::system::error_code& error);
    void handle_udt_connected();

private:
    std::shared_ptr<UDTSocket> m_udt_socket;
    std::shared_ptr<TCPSocket> m_tcp_socket;

    boost::asio::ip::icmp::socket m_icmp_socket;
    boost::asio::deadline_timer m_icmp_timer;
    std::vector<char> m_icmp_ttl_exceeded;
};
