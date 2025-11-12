#pragma once

#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace gm::core {

class Error : public std::runtime_error {
public:
    explicit Error(std::string message);
    virtual ~Error() = default;
};

inline Error::Error(std::string message)
    : std::runtime_error(std::move(message)) {}

class ResourceError : public Error {
public:
    ResourceError(std::string_view resourceType,
                  std::string_view identifier,
                  std::string details);

    std::string_view resourceType() const noexcept { return m_resourceType; }
    std::string_view identifier() const noexcept { return m_identifier; }
    std::string_view details() const noexcept { return m_details; }

private:
    static std::string BuildMessage(std::string_view resourceType,
                                    std::string_view identifier,
                                    const std::string& details);

    std::string m_resourceType;
    std::string m_identifier;
    std::string m_details;
};

inline std::string ResourceError::BuildMessage(std::string_view resourceType,
                                               std::string_view identifier,
                                               const std::string& details) {
    std::string message;
    message.reserve(resourceType.size() + identifier.size() + details.size() + 16);
    message.append("Resource error [");
    message.append(resourceType);
    message.append("] ");
    message.append(identifier);
    if (!details.empty()) {
        message.append(": ");
        message.append(details);
    }
    return message;
}

inline ResourceError::ResourceError(std::string_view resourceType,
                                    std::string_view identifier,
                                    std::string details)
    : Error(BuildMessage(resourceType, identifier, details)),
      m_resourceType(resourceType),
      m_identifier(identifier),
      m_details(std::move(details)) {}

class GraphicsError : public Error {
public:
    GraphicsError(std::string_view operation, std::string details);

    std::string_view operation() const noexcept { return m_operation; }
    std::string_view details() const noexcept { return m_details; }

private:
    static std::string BuildMessage(std::string_view operation,
                                    const std::string& details);

    std::string m_operation;
    std::string m_details;
};

inline std::string GraphicsError::BuildMessage(std::string_view operation,
                                               const std::string& details) {
    std::string message;
    message.reserve(operation.size() + details.size() + 16);
    message.append("Graphics error during ");
    message.append(operation);
    if (!details.empty()) {
        message.append(": ");
        message.append(details);
    }
    return message;
}

inline GraphicsError::GraphicsError(std::string_view operation, std::string details)
    : Error(BuildMessage(operation, details)),
      m_operation(operation),
      m_details(std::move(details)) {}

} // namespace gm::core
