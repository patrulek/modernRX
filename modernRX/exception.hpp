#pragma once

/*
* Generic exception class for modernRX library.
* Not a part of RandomX algorithm.
*/

#include <format>
#include <source_location>
#include <stdexcept>
#include <string>

namespace modernRX {
    class Exception : public std::runtime_error {
    public:
        [[nodiscard]] explicit Exception(const std::string& message, const std::source_location& location = std::source_location::current())
            : std::runtime_error(message) {
            std::string_view file_name{ location.file_name() };
            file_name = file_name.substr(file_name.find_last_of("\\/") + 1);
            formatted_message = std::format("{} ({}:{})", std::exception::what(), file_name, location.line());
        }

        [[nodiscard]] const char* what() const noexcept override {
            return formatted_message.c_str();
        }
    private:
        std::string formatted_message;
    };
}
