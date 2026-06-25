/* Copyright 2026 Janada Sroor
 SPDX-License-Identifier: Apache-2.0 */

#include "flux/tooling/lsp_server.h"
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace Flux {
namespace Tooling {

std::string LspServer::jsonGet(const std::string& json, const std::string& key)
{
    // Handle dotted keys (e.g., "textDocument.uri" -> jsonGetNested)
    size_t dotPos = key.find('.');
    if (dotPos != std::string::npos) {
        std::string key1 = key.substr(0, dotPos);
        std::string key2 = key.substr(dotPos + 1);
        return jsonGetNested(json, key1, key2);
    }

    std::string searchKey = "\"" + key + "\"";
    size_t keyPos = json.find(searchKey);
    if (keyPos == std::string::npos)
        return "";

    size_t colonPos = json.find(':', keyPos + searchKey.size());
    if (colonPos == std::string::npos)
        return "";

    size_t valueStart = colonPos + 1;
    while (valueStart < json.size() && json[valueStart] == ' ')
        valueStart++;

    if (valueStart >= json.size())
        return "";

    if (json[valueStart] == '"') {
        size_t strStart = valueStart + 1;
        std::string result;
        for (size_t i = strStart; i < json.size(); ++i) {
            if (json[i] == '"' && (i == 0 || json[i - 1] != '\\'))
                break;
            if (json[i] == '\\' && i + 1 < json.size()) {
                i++;
                switch (json[i]) {
                case '"':
                    result += '"';
                    break;
                case '\\':
                    result += '\\';
                    break;
                case '/':
                    result += '/';
                    break;
                case 'n':
                    result += '\n';
                    break;
                case 'r':
                    result += '\r';
                    break;
                case 't':
                    result += '\t';
                    break;
                default:
                    result += '\\';
                    result += json[i];
                    break;
                }
            } else {
                result += json[i];
            }
        }
        return result;
    } else if (json.substr(valueStart, 4) == "null") {
        return "";
    } else if (json.substr(valueStart, 4) == "true") {
        return "true";
    } else if (json.substr(valueStart, 5) == "false") {
        return "false";
    } else if (json[valueStart] == '-' || std::isdigit(json[valueStart])) {
        size_t numEnd = valueStart + 1;
        while (numEnd < json.size() && (std::isdigit(json[numEnd]) || json[numEnd] == '.' || json[numEnd] == 'e' ||
                                        json[numEnd] == 'E' || json[numEnd] == '-' || json[numEnd] == '+'))
            numEnd++;
        return json.substr(valueStart, numEnd - valueStart);
    }

    // Try to extract nested object
    if (json[valueStart] == '{') {
        int depth = 0;
        size_t objStart = valueStart;
        for (size_t i = valueStart; i < json.size(); ++i) {
            if (json[i] == '{')
                depth++;
            else if (json[i] == '}') {
                depth--;
                if (depth == 0)
                    return json.substr(objStart, i - objStart + 1);
            }
        }
    }

    return "";
}

std::string LspServer::jsonGetNested(const std::string& json, const std::string& key1, const std::string& key2)
{
    // Find key1, then within that object find key2
    std::string searchKey = "\"" + key1 + "\"";
    size_t keyPos = json.find(searchKey);
    if (keyPos == std::string::npos)
        return "";

    size_t colonPos = json.find(':', keyPos + searchKey.size());
    if (colonPos == std::string::npos)
        return "";

    size_t objStart = colonPos + 1;
    while (objStart < json.size() && json[objStart] == ' ')
        objStart++;

    if (objStart >= json.size() || json[objStart] != '{') {
        // Try direct key2 lookup (flat structure)
        return jsonGet(json, key2);
    }

    int depth = 0;
    size_t objEnd = objStart;
    for (size_t i = objStart; i < json.size(); ++i) {
        if (json[i] == '{')
            depth++;
        else if (json[i] == '}') {
            depth--;
            if (depth == 0) {
                objEnd = i;
                break;
            }
        }
    }

    std::string inner = json.substr(objStart, objEnd - objStart + 1);
    return jsonGet(inner, key2);
}

int LspServer::jsonGetInt(const std::string& json, const std::string& key, int defaultVal)
{
    std::string val = jsonGet(json, key);
    if (val.empty())
        return defaultVal;
    try {
        return std::stoi(val);
    } catch (...) {
        return defaultVal;
    }
}

std::string LspServer::jsonEscape(const std::string& s)
{
    std::string result;
    for (char c : s) {
        switch (c) {
        case '"':
            result += "\\\"";
            break;
        case '\\':
            result += "\\\\";
            break;
        case '\n':
            result += "\\n";
            break;
        case '\r':
            result += "\\r";
            break;
        case '\t':
            result += "\\t";
            break;
        default:
            result += c;
        }
    }
    return result;
}

std::string LspServer::makeResponse(int id, const std::string& result)
{
    return "{\"jsonrpc\":\"2.0\",\"id\":" + std::to_string(id) + ",\"result\":" + result + "}";
}

std::string LspServer::makeErrorResponse(int id, int code, const std::string& message)
{
    return "{\"jsonrpc\":\"2.0\",\"id\":" + std::to_string(id) + ",\"error\":{\"code\":" + std::to_string(code) +
           ",\"message\":\"" + jsonEscape(message) + "\"}}";
}

std::string LspServer::makeNotification(const std::string& method, const std::string& params)
{
    return "{\"jsonrpc\":\"2.0\",\"method\":\"" + method + "\",\"params\":" + params + "}";
}

void LspServer::publishDiagnostics(const std::string& uri, const std::vector<Diagnostic>& diagnostics)
{
    std::ostringstream oss;
    oss << R"({"uri":")" << jsonEscape(uri) << R"(","diagnostics":[)";
    for (size_t i = 0; i < diagnostics.size(); ++i) {
        if (i > 0)
            oss << ",";
        auto& d = diagnostics[i];
        oss << "{";
        oss << R"("range":{"start":{"line":)" << d.range.start.line << ",";
        oss << R"("character":)" << d.range.start.character << "},";
        oss << R"("end":{"line":)" << d.range.end.line << ",";
        oss << R"("character":)" << d.range.end.character << "}}";
        oss << R"(,"severity":)" << static_cast<int>(d.severity);
        oss << R"(,"source":")" << jsonEscape(d.source) << "\"";
        oss << R"(,"message":")" << jsonEscape(d.message) << "\"";
        oss << "}";
    }
    oss << "]}";
    std::string notification = makeNotification("textDocument/publishDiagnostics", oss.str());
    std::cout << "Content-Length: " << notification.size() << "\r\n\r\n" << notification;
    std::cout.flush();
}

// ============================================================================
// References Provider
// ============================================================================

} // namespace Tooling
} // namespace Flux
