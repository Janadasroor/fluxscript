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

#ifndef FLUX_UNITS_H
#define FLUX_UNITS_H

#include <cmath>
#include <cstdint>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

namespace Flux {
namespace Units {

// Unit dimensions: [M L T I  N J]
// M: Mass, L: Length, T: Time, I: Electric Current
// : Temperature, N: Amount of Substance, J: Luminous Intensity
struct UnitDimensions
{
    int8_t mass = 0;        // kg
    int8_t length = 0;      // m
    int8_t time = 0;        // s
    int8_t current = 0;     // A
    int8_t temperature = 0; // K
    int8_t amount = 0;      // mol
    int8_t luminous = 0;    // cd

    bool isDimensionless() const
    {
        return mass == 0 && length == 0 && time == 0 && current == 0 && temperature == 0 && amount == 0 &&
               luminous == 0;
    }

    bool operator==(const UnitDimensions& other) const
    {
        return mass == other.mass && length == other.length && time == other.time && current == other.current &&
               temperature == other.temperature && amount == other.amount && luminous == other.luminous;
    }

    bool operator!=(const UnitDimensions& other) const { return !(*this == other); }

    UnitDimensions operator*(const UnitDimensions& other) const
    {
        return {static_cast<int8_t>(mass + other.mass),
                static_cast<int8_t>(length + other.length),
                static_cast<int8_t>(time + other.time),
                static_cast<int8_t>(current + other.current),
                static_cast<int8_t>(temperature + other.temperature),
                static_cast<int8_t>(amount + other.amount),
                static_cast<int8_t>(luminous + other.luminous)};
    }

    UnitDimensions operator/(const UnitDimensions& other) const
    {
        return {static_cast<int8_t>(mass - other.mass),
                static_cast<int8_t>(length - other.length),
                static_cast<int8_t>(time - other.time),
                static_cast<int8_t>(current - other.current),
                static_cast<int8_t>(temperature - other.temperature),
                static_cast<int8_t>(amount - other.amount),
                static_cast<int8_t>(luminous - other.luminous)};
    }

    UnitDimensions pow(int exponent) const
    {
        return {static_cast<int8_t>(mass * exponent),        static_cast<int8_t>(length * exponent),
                static_cast<int8_t>(time * exponent),        static_cast<int8_t>(current * exponent),
                static_cast<int8_t>(temperature * exponent), static_cast<int8_t>(amount * exponent),
                static_cast<int8_t>(luminous * exponent)};
    }

    std::string toString() const;
};

// Predefined unit dimensions
namespace Dimension {
const UnitDimensions Dimensionless = {};
const UnitDimensions Length = {0, 1, 0, 0, 0, 0, 0};      // m
const UnitDimensions Mass = {1, 0, 0, 0, 0, 0, 0};        // kg
const UnitDimensions Time = {0, 0, 1, 0, 0, 0, 0};        // s
const UnitDimensions Current = {0, 0, 0, 1, 0, 0, 0};     // A
const UnitDimensions Temperature = {0, 0, 0, 0, 1, 0, 0}; // K
const UnitDimensions Amount = {0, 0, 0, 0, 0, 1, 0};      // mol
const UnitDimensions Luminous = {0, 0, 0, 0, 0, 0, 1};    // cd

// Derived units
const UnitDimensions Velocity = {0, 1, -1, 0, 0, 0, 0};     // m/s
const UnitDimensions Acceleration = {0, 1, -2, 0, 0, 0, 0}; // m/s
const UnitDimensions Force = {1, 1, -2, 0, 0, 0, 0};        // N = kgm/s
const UnitDimensions Energy = {1, 2, -2, 0, 0, 0, 0};       // J = Nm
const UnitDimensions Power = {1, 2, -3, 0, 0, 0, 0};        // W = J/s
const UnitDimensions Voltage = {1, 2, -3, -1, 0, 0, 0};     // V = W/A
const UnitDimensions Resistance = {1, 2, -3, -2, 0, 0, 0};  //  = V/A
const UnitDimensions Capacitance = {-1, -2, 4, 2, 0, 0, 0}; // F = C/V
const UnitDimensions Inductance = {1, 2, -2, -2, 0, 0, 0};  // H = Vs/A
const UnitDimensions Charge = {0, 0, 1, 1, 0, 0, 0};        // C = As
const UnitDimensions Frequency = {0, 0, -1, 0, 0, 0, 0};    // Hz = 1/s
const UnitDimensions Pressure = {1, -1, -2, 0, 0, 0, 0};    // Pa = N/m
const UnitDimensions Area = {0, 2, 0, 0, 0, 0, 0};          // m
const UnitDimensions Volume = {0, 3, 0, 0, 0, 0, 0};        // m
} // namespace Dimension

// Unit with scale factor (for prefixes)
struct ScaledUnit
{
    UnitDimensions dimensions;
    double scale; // e.g., 1000 for kilo, 0.001 for milli
    std::string name;
    std::string symbol;

    ScaledUnit() : scale(1.0) {}
    ScaledUnit(const UnitDimensions& d, double s, const std::string& n, const std::string& sym)
        : dimensions(d), scale(s), name(n), symbol(sym)
    {
    }
};

// Unit registry
class UnitRegistry
{
public:
    static UnitRegistry& instance();

    // Register a unit
    void registerUnit(const std::string& symbol, const ScaledUnit& unit);

    // Lookup a unit by symbol
    const ScaledUnit* lookupUnit(const std::string& symbol) const;

    // Parse unit string (e.g., "k", "mA", "F")
    ScaledUnit parseUnitString(const std::string& str) const;

    // Get canonical unit for dimensions
    std::string getCanonicalUnit(const UnitDimensions& dims) const;

    // Check if two units are compatible (same dimensions)
    bool areCompatible(const ScaledUnit& a, const ScaledUnit& b) const;

    // Convert value between units
    double convert(double value, const ScaledUnit& from, const ScaledUnit& to) const;

private:
    UnitRegistry();
    void initializeSIUnits();

    std::map<std::string, ScaledUnit> m_units;
};

// Value with units
class QuantifiedValue
{
public:
    QuantifiedValue() : m_value(0.0), m_unit() {}
    QuantifiedValue(double value, const UnitDimensions& dims) : m_value(value), m_unit(dims, 1.0, "", "") {}
    QuantifiedValue(double value, const ScaledUnit& unit) : m_value(value), m_unit(unit) {}

    double value() const { return m_value; }
    double valueInBase() const { return m_value * m_unit.scale; }
    const ScaledUnit& unit() const { return m_unit; }
    const UnitDimensions& dimensions() const { return m_unit.dimensions; }

    // Arithmetic operations with unit checking
    QuantifiedValue operator+(const QuantifiedValue& other) const;
    QuantifiedValue operator-(const QuantifiedValue& other) const;
    QuantifiedValue operator*(const QuantifiedValue& other) const;
    QuantifiedValue operator/(const QuantifiedValue& other) const;
    QuantifiedValue operator-() const { return QuantifiedValue(-m_value, m_unit); }

    // Comparison
    bool operator==(const QuantifiedValue& other) const;
    bool operator!=(const QuantifiedValue& other) const;
    bool operator<(const QuantifiedValue& other) const;
    bool operator<=(const QuantifiedValue& other) const;
    bool operator>(const QuantifiedValue& other) const;
    bool operator>=(const QuantifiedValue& other) const;

    // Convert to different unit
    QuantifiedValue to(const ScaledUnit& newUnit) const;

    // String representation
    std::string toString(int precision = 6) const;

    // Check if dimensionless
    bool isDimensionless() const { return m_unit.dimensions.isDimensionless(); }

private:
    double m_value;
    ScaledUnit m_unit;
};

// Unit conversion helpers
namespace Convert {
double toBase(double value, const std::string& unitSymbol);
double fromBase(double value, const std::string& unitSymbol);

// Common conversions
inline double volts(double v)
{
    return v;
}
inline double millivolts(double mv)
{
    return mv * 0.001;
}
inline double microvolts(double uv)
{
    return uv * 1e-6;
}
inline double kilovolts(double kv)
{
    return kv * 1000.0;
}

inline double ohms(double r)
{
    return r;
}
inline double milliohms(double mr)
{
    return mr * 0.001;
}
inline double kiloohms(double kr)
{
    return kr * 1000.0;
}
inline double megaohms(double mr)
{
    return mr * 1e6;
}

inline double farads(double f)
{
    return f;
}
inline double millifarads(double mf)
{
    return mf * 0.001;
}
inline double microfarads(double uf)
{
    return uf * 1e-6;
}
inline double nanofarads(double nf)
{
    return nf * 1e-9;
}
inline double picofarads(double pf)
{
    return pf * 1e-12;
}

inline double henrys(double h)
{
    return h;
}
inline double millihenrys(double mh)
{
    return mh * 0.001;
}
inline double microhenrys(double uh)
{
    return uh * 1e-6;
}

inline double amperes(double a)
{
    return a;
}
inline double milliamperes(double ma)
{
    return ma * 0.001;
}
inline double microamperes(double ua)
{
    return ua * 1e-6;
}

inline double hertz(double hz)
{
    return hz;
}
inline double kilohertz(double khz)
{
    return khz * 1000.0;
}
inline double megahertz(double mhz)
{
    return mhz * 1e6;
}
inline double gigahertz(double ghz)
{
    return ghz * 1e9;
}

inline double seconds(double s)
{
    return s;
}
inline double milliseconds(double ms)
{
    return ms * 0.001;
}
inline double microseconds(double us)
{
    return us * 1e-6;
}
inline double nanoseconds(double ns)
{
    return ns * 1e-9;
}
} // namespace Convert

// Exception for unit errors
class UnitException : public std::runtime_error
{
public:
    explicit UnitException(const std::string& msg) : std::runtime_error(msg) {}
};

class UnitMismatchException : public UnitException
{
public:
    UnitMismatchException(const std::string& expected, const std::string& actual)
        : UnitException("Unit mismatch: expected " + expected + ", got " + actual)
    {
    }
};

class UnitParseException : public UnitException
{
public:
    explicit UnitParseException(const std::string& unit) : UnitException("Cannot parse unit: " + unit) {}
};

} // namespace Units
} // namespace Flux

#endif // FLUX_UNITS_H
