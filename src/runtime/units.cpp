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

#include "flux/runtime/units.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <iostream>

namespace Flux {
namespace Units {

// ============================================================================
// UnitDimensions Implementation
// ============================================================================

std::string UnitDimensions::toString() const
{
    if (isDimensionless()) {
        return "dimensionless";
    }

    std::ostringstream oss;
    bool first = true;

    auto appendDim = [&](char name, int8_t power) {
        if (power != 0) {
            if (!first)
                oss << "*";
            oss << name;
            if (power != 1) {
                oss << "^" << static_cast<int>(power);
            }
            first = false;
        }
    };

    appendDim('m', mass);
    appendDim('l', length);
    appendDim('t', time);
    appendDim('i', current);
    appendDim('T', temperature);
    appendDim('n', amount);
    appendDim('J', luminous);

    return oss.str();
}

// ============================================================================
// UnitRegistry Implementation
// ============================================================================

UnitRegistry& UnitRegistry::instance()
{
    static UnitRegistry registry;
    return registry;
}

UnitRegistry::UnitRegistry()
{
    initializeSIUnits();
}

void UnitRegistry::initializeSIUnits()
{
    // SI Base Units
    registerUnit("m", ScaledUnit(Dimension::Length, 1.0, "meter", "m"));
    registerUnit("kg", ScaledUnit(Dimension::Mass, 1.0, "kilogram", "kg"));
    registerUnit("s", ScaledUnit(Dimension::Time, 1.0, "second", "s"));
    registerUnit("A", ScaledUnit(Dimension::Current, 1.0, "ampere", "A"));
    registerUnit("K", ScaledUnit(Dimension::Temperature, 1.0, "kelvin", "K"));
    registerUnit("mol", ScaledUnit(Dimension::Amount, 1.0, "mole", "mol"));
    registerUnit("cd", ScaledUnit(Dimension::Luminous, 1.0, "candela", "cd"));

    // SI Derived Units with special names
    registerUnit("Hz", ScaledUnit(Dimension::Frequency, 1.0, "hertz", "Hz"));
    registerUnit("N", ScaledUnit(Dimension::Force, 1.0, "newton", "N"));
    registerUnit("Pa", ScaledUnit(Dimension::Pressure, 1.0, "pascal", "Pa"));
    registerUnit("J", ScaledUnit(Dimension::Energy, 1.0, "joule", "J"));
    registerUnit("W", ScaledUnit(Dimension::Power, 1.0, "watt", "W"));
    registerUnit("C", ScaledUnit(Dimension::Charge, 1.0, "coulomb", "C"));
    registerUnit("V", ScaledUnit(Dimension::Voltage, 1.0, "volt", "V"));
    registerUnit("F", ScaledUnit(Dimension::Capacitance, 1.0, "farad", "F"));
    registerUnit("", ScaledUnit(Dimension::Resistance, 1.0, "ohm", ""));
    registerUnit("ohm", ScaledUnit(Dimension::Resistance, 1.0, "ohm", "ohm")); // Alternative
    registerUnit("S", ScaledUnit(Dimension::Resistance, 1.0, "siemens", "S")); // 1/
    registerUnit("H", ScaledUnit(Dimension::Inductance, 1.0, "henry", "H"));
    registerUnit("T", ScaledUnit({1, 0, -2, -1, 0, 0, 0}, 1.0, "tesla", "T"));   // Wb/m
    registerUnit("Wb", ScaledUnit({1, 2, -2, -1, 0, 0, 0}, 1.0, "weber", "Wb")); // Vs
    registerUnit("lm", ScaledUnit({0, 2, -3, 0, 0, 0, 1}, 1.0, "lumen", "lm"));
    registerUnit("lx", ScaledUnit({0, 0, -3, 0, 0, 0, 1}, 1.0, "lux", "lx"));

    // Common prefixes for electrical units
    // Voltage
    registerUnit("mV", ScaledUnit(Dimension::Voltage, 0.001, "millivolt", "mV"));
    registerUnit("V", ScaledUnit(Dimension::Voltage, 1.0, "volt", "V"));
    registerUnit("uV", ScaledUnit(Dimension::Voltage, 1e-6, "microvolt", "uV"));
    registerUnit("kV", ScaledUnit(Dimension::Voltage, 1000.0, "kilovolt", "kV"));
    registerUnit("MV", ScaledUnit(Dimension::Voltage, 1e6, "megavolt", "MV"));

    // Resistance
    registerUnit("mohm", ScaledUnit(Dimension::Resistance, 0.001, "milliohm", "mohm"));
    registerUnit("k", ScaledUnit(Dimension::Resistance, 1000.0, "kiloohm", "k"));
    registerUnit("M", ScaledUnit(Dimension::Resistance, 1e6, "megaohm", "M"));

    // Capacitance
    registerUnit("mF", ScaledUnit(Dimension::Capacitance, 0.001, "millifarad", "mF"));
    registerUnit("F", ScaledUnit(Dimension::Capacitance, 1.0, "farad", "F"));
    registerUnit("uF", ScaledUnit(Dimension::Capacitance, 1e-6, "microfarad", "uF"));
    registerUnit("nF", ScaledUnit(Dimension::Capacitance, 1e-9, "nanofarad", "nF"));
    registerUnit("pF", ScaledUnit(Dimension::Capacitance, 1e-12, "picofarad", "pF"));

    // Inductance
    registerUnit("mH", ScaledUnit(Dimension::Inductance, 0.001, "millihenry", "mH"));
    registerUnit("H", ScaledUnit(Dimension::Inductance, 1.0, "henry", "H"));
    registerUnit("uH", ScaledUnit(Dimension::Inductance, 1e-6, "microhenry", "uH"));
    registerUnit("nH", ScaledUnit(Dimension::Inductance, 1e-9, "nanohenry", "nH"));

    // Current
    registerUnit("mA", ScaledUnit(Dimension::Current, 0.001, "milliampere", "mA"));
    registerUnit("A", ScaledUnit(Dimension::Current, 1.0, "ampere", "A"));
    registerUnit("uA", ScaledUnit(Dimension::Current, 1e-6, "microampere", "uA"));
    registerUnit("nA", ScaledUnit(Dimension::Current, 1e-9, "nanoampere", "nA"));
    registerUnit("kA", ScaledUnit(Dimension::Current, 1000.0, "kiloampere", "kA"));

    // Frequency
    registerUnit("kHz", ScaledUnit(Dimension::Frequency, 1000.0, "kilohertz", "kHz"));
    registerUnit("MHz", ScaledUnit(Dimension::Frequency, 1e6, "megahertz", "MHz"));
    registerUnit("GHz", ScaledUnit(Dimension::Frequency, 1e9, "gigahertz", "GHz"));
    registerUnit("THz", ScaledUnit(Dimension::Frequency, 1e12, "terahertz", "THz"));

    // Time
    registerUnit("ms", ScaledUnit(Dimension::Time, 0.001, "millisecond", "ms"));
    registerUnit("s", ScaledUnit(Dimension::Time, 1.0, "second", "s"));
    registerUnit("us", ScaledUnit(Dimension::Time, 1e-6, "microsecond", "us"));
    registerUnit("ns", ScaledUnit(Dimension::Time, 1e-9, "nanosecond", "ns"));
    registerUnit("ps", ScaledUnit(Dimension::Time, 1e-12, "picosecond", "ps"));
    registerUnit("min", ScaledUnit(Dimension::Time, 60.0, "minute", "min"));
    registerUnit("h", ScaledUnit(Dimension::Time, 3600.0, "hour", "h"));

    // Power
    registerUnit("mW", ScaledUnit(Dimension::Power, 0.001, "milliwatt", "mW"));
    registerUnit("W", ScaledUnit(Dimension::Power, 1.0, "watt", "W"));
    registerUnit("uW", ScaledUnit(Dimension::Power, 1e-6, "microwatt", "uW"));
    registerUnit("kW", ScaledUnit(Dimension::Power, 1000.0, "kilowatt", "kW"));
    registerUnit("MW", ScaledUnit(Dimension::Power, 1e6, "megawatt", "MW"));

    // Energy
    registerUnit("mJ", ScaledUnit(Dimension::Energy, 0.001, "millijoule", "mJ"));
    registerUnit("kJ", ScaledUnit(Dimension::Energy, 1000.0, "kilojoule", "kJ"));
    registerUnit("Wh", ScaledUnit(Dimension::Energy, 3600.0, "watthour", "Wh"));
    registerUnit("kWh", ScaledUnit(Dimension::Energy, 3.6e6, "kilowatthour", "kWh"));

    // Charge
    registerUnit("mC", ScaledUnit(Dimension::Charge, 0.001, "millicoulomb", "mC"));
    registerUnit("C", ScaledUnit(Dimension::Charge, 1e-6, "microcoulomb", "C"));
    registerUnit("nC", ScaledUnit(Dimension::Charge, 1e-9, "nanocoulomb", "nC"));

    // Length
    registerUnit("mm", ScaledUnit(Dimension::Length, 0.001, "millimeter", "mm"));
    registerUnit("cm", ScaledUnit(Dimension::Length, 0.01, "centimeter", "cm"));
    registerUnit("m", ScaledUnit(Dimension::Length, 1e-6, "micrometer", "m"));
    registerUnit("nm", ScaledUnit(Dimension::Length, 1e-9, "nanometer", "nm"));
    registerUnit("km", ScaledUnit(Dimension::Length, 1000.0, "kilometer", "km"));

    // Mass
    registerUnit("g", ScaledUnit(Dimension::Mass, 0.001, "gram", "g"));
    registerUnit("mg", ScaledUnit(Dimension::Mass, 1e-6, "milligram", "mg"));
    registerUnit("g", ScaledUnit(Dimension::Mass, 1e-9, "microgram", "g"));
    registerUnit("tonne", ScaledUnit(Dimension::Mass, 1000.0, "tonne", "tonne"));

    // Pressure
    registerUnit("bar", ScaledUnit(Dimension::Pressure, 1e5, "bar", "bar"));
    registerUnit("mbar", ScaledUnit(Dimension::Pressure, 100.0, "millibar", "mbar"));
    registerUnit("atm", ScaledUnit(Dimension::Pressure, 101325.0, "atmosphere", "atm"));
    registerUnit("Torr", ScaledUnit(Dimension::Pressure, 133.322, "torr", "Torr"));
    registerUnit("psi", ScaledUnit(Dimension::Pressure, 6894.76, "psi", "psi"));

    // Area and Volume
    registerUnit("m", ScaledUnit(Dimension::Area, 1.0, "square meter", "m"));
    registerUnit("cm", ScaledUnit(Dimension::Area, 1e-4, "square centimeter", "cm"));
    registerUnit("mm", ScaledUnit(Dimension::Area, 1e-6, "square millimeter", "mm"));
    registerUnit("m", ScaledUnit(Dimension::Volume, 1.0, "cubic meter", "m"));
    registerUnit("L", ScaledUnit(Dimension::Volume, 0.001, "liter", "L"));
    registerUnit("mL", ScaledUnit(Dimension::Volume, 1e-6, "milliliter", "mL"));
    registerUnit("L", ScaledUnit(Dimension::Volume, 1e-9, "microliter", "L"));

    // Temperature (with offset for Celsius)
    registerUnit("C", ScaledUnit(Dimension::Temperature, 1.0, "degree Celsius", "C"));
    registerUnit("F", ScaledUnit(Dimension::Temperature, 5.0 / 9.0, "degree Fahrenheit", "F"));
}

void UnitRegistry::registerUnit(const std::string& symbol, const ScaledUnit& unit)
{
    m_units[symbol] = unit;
}

const ScaledUnit* UnitRegistry::lookupUnit(const std::string& symbol) const
{
    auto it = m_units.find(symbol);
    if (it != m_units.end()) {
        return &it->second;
    }
    return nullptr;
}

ScaledUnit UnitRegistry::parseUnitString(const std::string& str) const
{
    if (str.empty()) {
        return ScaledUnit(); // Dimensionless
    }

    // Try direct lookup first
    const ScaledUnit* unit = lookupUnit(str);
    if (unit) {
        return *unit;
    }

    // Try to parse compound units (e.g., "m/s", "Nm")
    size_t slashPos = str.find('/');
    if (slashPos != std::string::npos) {
        std::string numStr = str.substr(0, slashPos);
        std::string denStr = str.substr(slashPos + 1);

        ScaledUnit numUnit = parseUnitString(numStr);
        ScaledUnit denUnit = parseUnitString(denStr);

        return ScaledUnit(numUnit.dimensions / denUnit.dimensions, numUnit.scale / denUnit.scale, numStr + "/" + denStr,
                          str);
    }

    // Try to parse with powers (e.g., "m", "m^2")
    size_t powerPos = str.find_first_of("^");
    if (powerPos != std::string::npos) {
        std::string baseStr = str.substr(0, powerPos);
        int power = std::stoi(str.substr(powerPos + 1));

        ScaledUnit baseUnit = parseUnitString(baseStr);
        return ScaledUnit(baseUnit.dimensions.pow(power), std::pow(baseUnit.scale, power),
                          baseUnit.name + "^" + std::to_string(power), str);
    }

    // Check for unicode superscripts
    if (str.find("") != std::string::npos) {
        size_t pos = str.find("");
        std::string baseStr = str.substr(0, pos);
        ScaledUnit baseUnit = parseUnitString(baseStr);
        return ScaledUnit(baseUnit.dimensions.pow(2), std::pow(baseUnit.scale, 2), baseUnit.name + "^2", str);
    }
    if (str.find("") != std::string::npos) {
        size_t pos = str.find("");
        std::string baseStr = str.substr(0, pos);
        ScaledUnit baseUnit = parseUnitString(baseStr);
        return ScaledUnit(baseUnit.dimensions.pow(3), std::pow(baseUnit.scale, 3), baseUnit.name + "^3", str);
    }

    throw UnitParseException(str);
}

std::string UnitRegistry::getCanonicalUnit(const UnitDimensions& dims) const
{
    // Return the most common unit for these dimensions
    if (dims == Dimension::Voltage)
        return "V";
    if (dims == Dimension::Current)
        return "A";
    if (dims == Dimension::Resistance)
        return "";
    if (dims == Dimension::Capacitance)
        return "F";
    if (dims == Dimension::Inductance)
        return "H";
    if (dims == Dimension::Frequency)
        return "Hz";
    if (dims == Dimension::Power)
        return "W";
    if (dims == Dimension::Energy)
        return "J";
    if (dims == Dimension::Charge)
        return "C";
    if (dims == Dimension::Time)
        return "s";
    if (dims == Dimension::Length)
        return "m";
    if (dims == Dimension::Mass)
        return "kg";
    if (dims == Dimension::Temperature)
        return "K";
    if (dims == Dimension::Force)
        return "N";
    if (dims == Dimension::Pressure)
        return "Pa";

    return ""; // Unknown
}

bool UnitRegistry::areCompatible(const ScaledUnit& a, const ScaledUnit& b) const
{
    return a.dimensions == b.dimensions;
}

double UnitRegistry::convert(double value, const ScaledUnit& from, const ScaledUnit& to) const
{
    if (!areCompatible(from, to)) {
        throw UnitMismatchException(from.symbol, to.symbol);
    }

    // Convert to base, then to target
    double baseValue = value * from.scale;
    return baseValue / to.scale;
}

// ============================================================================
// QuantifiedValue Implementation
// ============================================================================

QuantifiedValue QuantifiedValue::operator+(const QuantifiedValue& other) const
{
    if (m_unit.dimensions != other.m_unit.dimensions) {
        throw UnitMismatchException(m_unit.symbol, other.m_unit.symbol);
    }

    // Convert to same units and add
    double otherInThisUnit = UnitRegistry::instance().convert(other.m_value, other.m_unit, m_unit);
    return QuantifiedValue(m_value + otherInThisUnit, m_unit);
}

QuantifiedValue QuantifiedValue::operator-(const QuantifiedValue& other) const
{
    if (m_unit.dimensions != other.m_unit.dimensions) {
        throw UnitMismatchException(m_unit.symbol, other.m_unit.symbol);
    }

    double otherInThisUnit = UnitRegistry::instance().convert(other.m_value, other.m_unit, m_unit);
    return QuantifiedValue(m_value - otherInThisUnit, m_unit);
}

QuantifiedValue QuantifiedValue::operator*(const QuantifiedValue& other) const
{
    ScaledUnit resultUnit(m_unit.dimensions * other.m_unit.dimensions, m_unit.scale * other.m_unit.scale,
                          m_unit.name + "" + other.m_unit.name, m_unit.symbol + "" + other.m_unit.symbol);
    return QuantifiedValue(m_value * other.m_value, resultUnit);
}

QuantifiedValue QuantifiedValue::operator/(const QuantifiedValue& other) const
{
    ScaledUnit resultUnit(m_unit.dimensions / other.m_unit.dimensions, m_unit.scale / other.m_unit.scale,
                          m_unit.name + "/" + other.m_unit.name, m_unit.symbol + "/" + other.m_unit.symbol);
    return QuantifiedValue(m_value / other.m_value, resultUnit);
}

bool QuantifiedValue::operator==(const QuantifiedValue& other) const
{
    if (m_unit.dimensions != other.m_unit.dimensions) {
        return false;
    }
    double otherInThisUnit = UnitRegistry::instance().convert(other.m_value, other.m_unit, m_unit);
    return std::abs(m_value - otherInThisUnit) < 1e-12;
}

bool QuantifiedValue::operator!=(const QuantifiedValue& other) const
{
    return !(*this == other);
}

bool QuantifiedValue::operator<(const QuantifiedValue& other) const
{
    if (m_unit.dimensions != other.m_unit.dimensions) {
        throw UnitMismatchException(m_unit.symbol, other.m_unit.symbol);
    }
    double otherInThisUnit = UnitRegistry::instance().convert(other.m_value, other.m_unit, m_unit);
    return m_value < otherInThisUnit;
}

bool QuantifiedValue::operator<=(const QuantifiedValue& other) const
{
    return !(other < *this);
}

bool QuantifiedValue::operator>(const QuantifiedValue& other) const
{
    return other < *this;
}

bool QuantifiedValue::operator>=(const QuantifiedValue& other) const
{
    return !(*this < other);
}

QuantifiedValue QuantifiedValue::to(const ScaledUnit& newUnit) const
{
    if (m_unit.dimensions != newUnit.dimensions) {
        throw UnitMismatchException(m_unit.symbol, newUnit.symbol);
    }
    double newValue = UnitRegistry::instance().convert(m_value, m_unit, newUnit);
    return QuantifiedValue(newValue, newUnit);
}

std::string QuantifiedValue::toString(int precision) const
{
    std::ostringstream oss;
    oss.precision(precision);
    oss << std::fixed << m_value;

    if (!m_unit.symbol.empty()) {
        oss << " " << m_unit.symbol;
    }

    return oss.str();
}

// ============================================================================
// Conversion Helpers Implementation
// ============================================================================

double Convert::toBase(double value, const std::string& unitSymbol)
{
    const ScaledUnit* unit = UnitRegistry::instance().lookupUnit(unitSymbol);
    if (!unit) {
        throw UnitParseException(unitSymbol);
    }
    return value * unit->scale;
}

double Convert::fromBase(double value, const std::string& unitSymbol)
{
    const ScaledUnit* unit = UnitRegistry::instance().lookupUnit(unitSymbol);
    if (!unit) {
        throw UnitParseException(unitSymbol);
    }
    return value / unit->scale;
}

} // namespace Units
} // namespace Flux
