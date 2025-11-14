#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

namespace gm::content {

enum class FieldKind {
    String,
    Integer,
    Number,
    Boolean,
    Object,
    Array
};

struct ValidationIssue {
    bool isError = true;
    std::string path;
    std::string message;
};

struct SchemaField {
    std::string name;
    FieldKind kind = FieldKind::String;
    bool required = false;
    std::vector<std::string> enumValues;
    std::optional<double> minimum;
    std::optional<double> maximum;
    std::optional<std::size_t> minLength;
    std::optional<std::size_t> maxLength;
    std::vector<SchemaField> properties;
    std::unique_ptr<SchemaField> element;

    SchemaField() = default;
    SchemaField(const SchemaField& other) {
        *this = other;
    }
    SchemaField& operator=(const SchemaField& other) {
        if (this == &other) {
            return *this;
        }
        name = other.name;
        kind = other.kind;
        required = other.required;
        enumValues = other.enumValues;
        minimum = other.minimum;
        maximum = other.maximum;
        minLength = other.minLength;
        maxLength = other.maxLength;
        properties = other.properties;
        if (other.element) {
            element = std::make_unique<SchemaField>(*other.element);
        } else {
            element.reset();
        }
        return *this;
    }
    SchemaField(SchemaField&&) noexcept = default;
    SchemaField& operator=(SchemaField&&) noexcept = default;
};

struct ContentSchema {
    std::string type;
    std::string displayName;
    std::string sourceFile;
    int version = 1;
    std::vector<std::string> dataDirectories;
    std::vector<SchemaField> fields;
};

struct ContentRecord {
    std::string type;
    std::string identifier;
    std::string displayName;
    std::string guid;
    std::string relativePath;
    std::filesystem::path sourcePath;
    std::filesystem::file_time_type lastWriteTime{};
    bool valid = false;
    std::vector<ValidationIssue> issues;
    nlohmann::json document;
    std::uint64_t version = 0;
};

} // namespace gm::content


