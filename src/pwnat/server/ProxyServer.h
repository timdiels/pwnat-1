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

#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <pwnat/udtservice/UDTService.h>

/**
 * Listens for new ProxyClients using pwnat ICMP trickery
 */
class ProxyServer {
public:
    ProxyServer();
    void run(); // TODO rm

private:
    void send_icmp_echo(); // TODO timed every 5 sec
    void handle_send(const boost::system::error_code& error);
    void start_receive();
    void handle_receive(boost::system::error_code error, size_t bytes_transferred);

private:
    boost::asio::io_service m_io_service;
    boost::asio::ip::icmp::socket m_socket;
    boost::array<char, 64 * 1024> m_receive_buffer;

    UDTService m_udt_service;
};

