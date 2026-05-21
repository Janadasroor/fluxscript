/* Copyright 2026 Janada Sroor
 SPDX-License-Identifier: Apache-2.0
 http://www.apache.org/licenses/LICENSE-2.0
 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at
     http://www.apache.org/licenses/LICENSE-2.0
 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License. */

// Instrument Control Implementation (SCPI via TCP/IP)
#include "flux/instruments/instrument.h"
#include <cstring>
#include <iostream>
#include <sstream>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#ifndef __MINGW32__
typedef int ssize_t;
#endif
#define CLOSE_SOCKET(s) closesocket(s)
#define GET_ERRNO() WSAGetLastError()
// WSAStartup/WSACleanup handled via static init guard
struct WsaInit
{
    WsaInit()
    {
        WSADATA d;
        WSAStartup(MAKEWORD(2, 2), &d);
    }
    ~WsaInit() { WSACleanup(); }
};
static WsaInit g_wsa;
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#define CLOSE_SOCKET(s) close(s)
#define GET_ERRNO() errno
#endif

namespace Flux {
namespace Instruments {

//
//  Base Instrument Implementation
//

Instrument::Instrument() : m_port(5025), m_socketFd(-1) {}

Instrument::~Instrument()
{
    disconnect();
}

bool Instrument::connect(const std::string& address, int port)
{
    m_address = address;
    m_port = port;

    m_socketFd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_socketFd < 0) {
        std::cerr << "[Instrument] Failed to create socket\n";
        return false;
    }

    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(m_port);

    if (inet_pton(AF_INET, m_address.c_str(), &serverAddr.sin_addr) <= 0) {
        std::cerr << "[Instrument] Invalid address: " << m_address << "\n";
        CLOSE_SOCKET(m_socketFd);
        m_socketFd = -1;
        return false;
    }

    if (::connect(m_socketFd, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "[Instrument] Connection failed to " << m_address << ":" << m_port << "\n";
        CLOSE_SOCKET(m_socketFd);
        m_socketFd = -1;
        return false;
    }

    std::cout << "[Instrument] Connected to " << m_address << ":" << m_port << "\n";
    return true;
}

void Instrument::disconnect()
{
    if (m_socketFd >= 0) {
        CLOSE_SOCKET(m_socketFd);
        m_socketFd = -1;
    }
}

bool Instrument::isConnected() const
{
    return m_socketFd >= 0;
}

std::string Instrument::getID()
{
    return query("*IDN?");
}

std::string Instrument::sendCommand(const std::string& cmd)
{
    if (!isConnected()) {
        std::cerr << "[Instrument] Not connected!\n";
        return "";
    }
    return query(cmd);
}

void Instrument::sendRawCommand(const std::string& cmd)
{
    writeSocket(cmd + "\n");
}

std::string Instrument::query(const std::string& cmd)
{
    if (!isConnected())
        return "";

    writeSocket(cmd + "\n");
    return readSocket();
}

int Instrument::createSocket()
{
    return socket(AF_INET, SOCK_STREAM, 0);
}

bool Instrument::writeSocket(const std::string& data)
{
    if (m_socketFd < 0)
        return false;
    ssize_t sent = send(m_socketFd, data.c_str(), data.size(), 0);
    return (sent > 0);
}

std::string Instrument::readSocket()
{
    if (m_socketFd < 0)
        return "";

    char buffer[4096];
    memset(buffer, 0, sizeof(buffer));

    // Set a small timeout for reading (optional improvement, skipping for simplicity here)
    ssize_t received = recv(m_socketFd, buffer, sizeof(buffer) - 1, 0);

    if (received > 0) {
        buffer[received] = '\0';
        // Remove trailing newlines
        std::string result(buffer);
        while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) {
            result.pop_back();
        }
        return result;
    }
    return "";
}

//
//  DC Power Supply Implementation
//

PowerSupply::PowerSupply() {}

void PowerSupply::setVoltage(double volts)
{
    std::ostringstream oss;
    oss << ":SOUR:VOLT " << volts;
    sendRawCommand(oss.str());
}

void PowerSupply::setCurrentLimit(double amps)
{
    std::ostringstream oss;
    oss << ":SOUR:CURR " << amps;
    sendRawCommand(oss.str());
}

void PowerSupply::setOutput(bool state)
{
    sendRawCommand(":OUTP " + std::to_string(state ? 1 : 0));
}

double PowerSupply::measureVoltage()
{
    std::string resp = sendCommand(":MEAS:VOLT?");
    try {
        return std::stod(resp);
    } catch (...) {
        return 0.0;
    }
}

double PowerSupply::measureCurrent()
{
    std::string resp = sendCommand(":MEAS:CURR?");
    try {
        return std::stod(resp);
    } catch (...) {
        return 0.0;
    }
}

//
//  Digital Multimeter Implementation
//

Multimeter::Multimeter() {}

double Multimeter::measureVoltageDC()
{
    std::string resp = sendCommand(":MEAS:VOLT:DC?");
    try {
        return std::stod(resp);
    } catch (...) {
        return 0.0;
    }
}

double Multimeter::measureCurrentDC()
{
    std::string resp = sendCommand(":MEAS:CURR:DC?");
    try {
        return std::stod(resp);
    } catch (...) {
        return 0.0;
    }
}

double Multimeter::measureResistance()
{
    std::string resp = sendCommand(":MEAS:RES?");
    try {
        return std::stod(resp);
    } catch (...) {
        return 0.0;
    }
}

double Multimeter::measureFrequency()
{
    std::string resp = sendCommand(":MEAS:FREQ?");
    try {
        return std::stod(resp);
    } catch (...) {
        return 0.0;
    }
}

//
//  Oscilloscope Implementation
//

Oscilloscope::Oscilloscope() {}

void Oscilloscope::setTimebase(double secondsPerDiv)
{
    std::ostringstream oss;
    oss << ":TIM:SCAL " << secondsPerDiv;
    sendRawCommand(oss.str());
}

void Oscilloscope::setVerticalScale(int channel, double voltsPerDiv)
{
    std::ostringstream oss;
    oss << ":CHAN" << channel << ":SCAL " << voltsPerDiv;
    sendRawCommand(oss.str());
}

void Oscilloscope::setTriggerLevel(double volts)
{
    std::ostringstream oss;
    oss << ":TRIG:LEV " << volts;
    sendRawCommand(oss.str());
}

void Oscilloscope::run()
{
    sendRawCommand(":RUN");
}
void Oscilloscope::stop()
{
    sendRawCommand(":STOP");
}
void Oscilloscope::single()
{
    sendRawCommand(":SING");
}

Oscilloscope::WaveformData Oscilloscope::captureWaveform(int channel)
{
    WaveformData data;

    // 1. Get preamble
    std::string preamble = sendCommand(":WAV:SOUR CHAN" + std::to_string(channel) + ";:WAV:PRE?");
    // Parsing preamble usually involves splitting by comma
    // Format: <format>,<type>,<points>,<count>,<xincrement>,<xorigin>,<xreference>,<yincrement>,<yorigin>,<yreference>

    // Simplified for demo - in real app we would parse preamble
    data.xIncrement = 1e-6;  // 1us
    data.yIncrement = 0.001; // 1mV

    // 2. Get data (usually binary block, here we assume ASCII for simplicity of this stub)
    // :WAV:DATA? returns comma separated values
    std::string rawData = sendCommand(":WAV:DATA?");

    // Parse CSV data
    std::istringstream ss(rawData);
    std::string segment;
    double x = 0.0;

    while (std::getline(ss, segment, ',')) {
        try {
            double y = std::stod(segment);
            data.time.push_back(x);
            data.voltage.push_back(y * data.yIncrement);
            x += data.xIncrement;
        } catch (...) {
        }
    }

    return data;
}

//
//  C Interface Implementation
//

extern "C" {
void* flux_ps_create()
{
    return new PowerSupply();
}
void flux_ps_destroy(void* ps)
{
    delete static_cast<PowerSupply*>(ps);
}

bool flux_ps_connect(void* ps, const char* ip, int port)
{
    return static_cast<PowerSupply*>(ps)->connect(ip ? ip : "127.0.0.1", port);
}

void flux_ps_set_voltage(void* ps, double v)
{
    static_cast<PowerSupply*>(ps)->setVoltage(v);
}

double flux_ps_measure_voltage(void* ps)
{
    return static_cast<PowerSupply*>(ps)->measureVoltage();
}

void flux_ps_disconnect(void* ps)
{
    static_cast<PowerSupply*>(ps)->disconnect();
}
}

} // namespace Instruments
} // namespace Flux
