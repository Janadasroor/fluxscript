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

#ifndef FLUX_INSTRUMENTS_H
#define FLUX_INSTRUMENTS_H

#include <string>
#include <vector>

namespace Flux {
namespace Instruments {

//
//  Base Instrument Class
//

class Instrument
{
public:
    Instrument();
    virtual ~Instrument();

    // Connection management
    bool connect(const std::string& address, int port = 5025);
    void disconnect();
    bool isConnected() const;

    // SCPI Communication
    std::string sendCommand(const std::string& cmd);
    void sendRawCommand(const std::string& cmd);
    std::string query(const std::string& cmd);

    // Identity
    std::string getID();

protected:
    std::string m_address;
    int m_port;
    int m_socketFd;

    // Internal socket helpers
    int createSocket();
    bool writeSocket(const std::string& data);
    std::string readSocket();
};

//
//  DC Power Supply
//

class PowerSupply : public Instrument
{
public:
    PowerSupply();

    // Basic Control
    void setOutput(bool state);
    void setVoltage(double volts);
    void setCurrentLimit(double amps);

    // Measurement
    double measureVoltage();
    double measureCurrent();
};

//
//  Digital Multimeter
//

class Multimeter : public Instrument
{
public:
    Multimeter();

    // Measurement
    double measureVoltageDC();
    double measureCurrentDC();
    double measureResistance();
    double measureFrequency();
};

//
//  Oscilloscope
//

class Oscilloscope : public Instrument
{
public:
    Oscilloscope();

    // Configuration
    void setTimebase(double secondsPerDiv);
    void setVerticalScale(int channel, double voltsPerDiv);
    void setTriggerLevel(double volts);

    // Acquisition
    void run();
    void stop();
    void single();

    // Data Retrieval (simplified waveform capture)
    struct WaveformData
    {
        std::vector<double> time;
        std::vector<double> voltage;
        double xIncrement;
        double xOrigin;
        double yIncrement;
        double yOrigin;
    };

    WaveformData captureWaveform(int channel = 1);
};

//
//  Convenience Functions
//

extern "C" {
void* flux_ps_create();
void flux_ps_destroy(void* ps);
bool flux_ps_connect(void* ps, const char* ip, int port);
void flux_ps_set_voltage(void* ps, double v);
double flux_ps_measure_voltage(void* ps);
void flux_ps_disconnect(void* ps);
}

} // namespace Instruments
} // namespace Flux

#endif // FLUX_INSTRUMENTS_H
