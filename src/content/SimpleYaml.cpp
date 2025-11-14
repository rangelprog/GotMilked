#include "gm/content/SimpleYaml.hpp"

#include "gm/core/Logger.hpp"

#include <charconv>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stack>
#include <string_view>
#include <fmt/format.h>

namespace gm::content::SimpleYaml {

namespace {

struct LineInfo {
    int indent = 0;
    std::string text;
    std::size_t number = 0;
};

enum class FrameKind {
    Object,
    Array
};

struct Frame {
    nlohmann::json* node = nullptr;
    FrameKind kind = FrameKind::Object;
    int indent = 0;
};

std::string_view TrimView(std::string_view value) {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) {
        value.remove_prefix(1);
    }
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
        value.remove_suffix(1);
    }
    return value;
}

std::string Trim(std::string_view value) {
    return std::string(TrimView(value));
}

bool IsNumber(std::string_view value) {
    if (value.empty()) {
        return false;
    }
    std::size_t i = 0;
    if (value[i] == '-' || value[i] == '+') {
        ++i;
    }
    bool hasDecimal = false;
    for (; i < value.size(); ++i) {
        if (value[i] == '.') {
            if (hasDecimal) {
                return false;
            }
            hasDecimal = true;
            continue;
        }
        if (!std::isdigit(static_cast<unsigned char>(value[i]))) {
            return false;
        }
    }
    return true;
}

nlohmann::json ParseScalar(std::string_view value) {
    value = TrimView(value);
    if (value.empty()) {
        return nlohmann::json();
    }
    if (value == "~" || value == "null" || value == "Null" || value == "NULL") {
        return nlohmann::json();
    }
    if (value == "true" || value == "True" || value == "TRUE") {
        return true;
    }
    if (value == "false" || value == "False" || value == "FALSE") {
        return false;
    }
    if (value.front() == '"' && value.back() == '"' && value.size() >= 2) {
        std::string unescaped(value.substr(1, value.size() - 2));
        return unescaped;
    }
    if (value.front() == '\'' && value.back() == '\'' && value.size() >= 2) {
        std::string unescaped(value.substr(1, value.size() - 2));
        return unescaped;
    }
    if (IsNumber(value)) {
        if (value.find('.') != std::string_view::npos) {
            double number = 0.0;
            std::from_chars(value.data(), value.data() + value.size(), number);
            return number;
        }
        std::int64_t integer = 0;
        std::from_chars(value.data(), value.data() + value.size(), integer);
        return integer;
    }
    if (value == "[]") {
        return nlohmann::json::array();
    }
    if (value == "{}") {
        return nlohmann::json::object();
    }
    return std::string(value);
}

bool NextLineIsListItem(const std::vector<LineInfo>& lines, std::size_t currentIndex, int currentIndent) {
    for (std::size_t i = currentIndex + 1; i < lines.size(); ++i) {
        if (lines[i].text.empty()) {
            continue;
        }
        if (lines[i].indent <= currentIndent) {
            return false;
        }
        return lines[i].text.rfind("- ", 0) == 0;
    }
    return false;
}

bool EnsureObject(Frame& frame, std::string& error, std::size_t lineNumber) {
    if (!frame.node) {
        error = "internal parser error (null frame)";
        return false;
    }
    if (!frame.node->is_object() && !frame.node->is_null()) {
        error = fmt::format("Line {}: expected mapping but found different type", lineNumber);
        return false;
    }
    if (frame.node->is_null()) {
        *frame.node = nlohmann::json::object();
    }
    return true;
}

bool EnsureArray(Frame& frame, std::string& error, std::size_t lineNumber) {
    if (!frame.node) {
        error = "internal parser error (null frame)";
        return false;
    }
    if (!frame.node->is_array() && !frame.node->is_null()) {
        error = fmt::format("Line {}: expected list but found different type", lineNumber);
        return false;
    }
    if (frame.node->is_null()) {
        *frame.node = nlohmann::json::array();
    }
    return true;
}

std::vector<LineInfo> Tokenize(const std::string& source) {
    std::vector<LineInfo> lines;
    std::istringstream stream(source);
    std::string raw;
    std::size_t lineNumber = 0;
    while (std::getline(stream, raw)) {
        ++lineNumber;
        if (lineNumber == 1 && !raw.empty() && raw[0] == '\xEF' && raw.size() >= 3 &&
            static_cast<unsigned char>(raw[1]) == 0xBB && static_cast<unsigned char>(raw[2]) == 0xBF) {
            raw.erase(0, 3);
        }
        auto commentPos = raw.find('#');
        if (commentPos != std::string::npos) {
            raw = raw.substr(0, commentPos);
        }
        std::string_view trimmed = raw;
        while (!trimmed.empty() && (trimmed.back() == '\r' || trimmed.back() == '\n')) {
            trimmed.remove_suffix(1);
        }
        int indent = 0;
        while (!trimmed.empty() && trimmed.front() == ' ') {
            ++indent;
            trimmed.remove_prefix(1);
        }
        std::string text = std::string(TrimView(trimmed));
        lines.push_back(LineInfo{indent, std::move(text), lineNumber});
    }
    return lines;
}

} // namespace

bool Parse(const std::string& source, nlohmann::json& out, std::string& error) {
    auto lines = Tokenize(source);
    out = nlohmann::json::object();

    std::vector<Frame> stack;
    stack.push_back(Frame{&out, FrameKind::Object, 0});

    for (std::size_t i = 0; i < lines.size(); ++i) {
        const auto& line = lines[i];
        if (line.text.empty()) {
            continue;
        }
        if (line.indent % 2 != 0) {
            error = fmt::format("Line {}: indentation must be multiples of two spaces", line.number);
            return false;
        }

        while (stack.size() > 1 && line.indent < stack.back().indent) {
            stack.pop_back();
        }

        Frame& frame = stack.back();
        if (line.text.rfind("- ", 0) == 0) {
            if (frame.kind != FrameKind::Array) {
                error = fmt::format("Line {}: list item without list context", line.number);
                return false;
            }
            if (!EnsureArray(frame, error, line.number)) {
                return false;
            }

            std::string valuePart = line.text.substr(2);
            valuePart = Trim(valuePart);
            if (valuePart.empty()) {
                nlohmann::json element = nlohmann::json::object();
                frame.node->push_back(element);
                nlohmann::json& back = frame.node->back();
                stack.push_back(Frame{&back, FrameKind::Object, line.indent + 2});
                continue;
            }

            auto colonPos = valuePart.find(':');
            if (colonPos != std::string::npos) {
                std::string key = Trim(valuePart.substr(0, colonPos));
                std::string remainder = Trim(valuePart.substr(colonPos + 1));

                nlohmann::json element = nlohmann::json::object();
                if (!remainder.empty()) {
                    element[key] = ParseScalar(remainder);
                } else {
                    element[key] = nlohmann::json();
                }
                frame.node->push_back(element);
                nlohmann::json& back = frame.node->back();
                stack.push_back(Frame{&back, FrameKind::Object, line.indent + 2});
            } else {
                frame.node->push_back(ParseScalar(valuePart));
            }
            continue;
        }

        auto colonPos = line.text.find(':');
        if (colonPos == std::string::npos) {
            error = fmt::format("Line {}: expected ':' in mapping entry", line.number);
            return false;
        }

        if (!EnsureObject(frame, error, line.number)) {
            return false;
        }

        std::string key = Trim(line.text.substr(0, colonPos));
        std::string value = Trim(line.text.substr(colonPos + 1));

        if (value.empty()) {
            if (NextLineIsListItem(lines, i, line.indent)) {
                nlohmann::json array = nlohmann::json::array();
                (*frame.node)[key] = array;
                nlohmann::json& child = (*frame.node)[key];
                stack.push_back(Frame{&child, FrameKind::Array, line.indent + 2});
            } else {
                nlohmann::json object = nlohmann::json::object();
                (*frame.node)[key] = object;
                nlohmann::json& child = (*frame.node)[key];
                stack.push_back(Frame{&child, FrameKind::Object, line.indent + 2});
            }
        } else {
            (*frame.node)[key] = ParseScalar(value);
        }
    }

    while (stack.size() > 1) {
        stack.pop_back();
    }

    return true;
}

bool LoadStructuredFile(const std::filesystem::path& path, nlohmann::json& out, std::string& error) {
    std::ifstream file(path);
    if (!file) {
        error = fmt::format("Failed to open '{}'", path.string());
        return false;
    }

    const auto ext = path.extension().string();
    if (ext == ".json" || ext == ".JSON") {
        try {
            file >> out;
            return true;
        } catch (const nlohmann::json::exception& e) {
            error = fmt::format("JSON parse error in '{}': {}", path.string(), e.what());
            return false;
        }
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    const std::string source = buffer.str();
    std::string parseError;
    if (!Parse(source, out, parseError)) {
        error = fmt::format("YAML parse error in '{}': {}", path.string(), parseError);
        return false;
    }
    error.clear();
    return true;
}

} // namespace gm::content::SimpleYaml


