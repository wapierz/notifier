#pragma once

#include <cstring>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <string>



auto open_ofstream(std::filesystem::path file)
    -> std::expected<std::basic_ofstream<char>, std::string> {
    std::basic_ofstream<char> f(
        file, std::ios::out | std::ios::binary /*| std::ios::noreplace*/);
    if (!f.is_open()) {
        return std::unexpected{std::format("Cannot open output file {}. {}",
                                           file.string(),
                                           std::strerror(errno))};
    }
    return f;
}


auto save_to( std::ranges::range  auto input, std::filesystem::path file)
    -> std::expected<size_t, std::string> {

    std::filesystem::create_directories(file.parent_path());
    auto stream{open_ofstream(file)};
    if (!stream) {return std::unexpected{stream.error()};}
    std::string data{};
    try {
        if constexpr (std::ranges::sized_range<decltype(input)>) {
            data.reserve(input.size());
        }
        std::ranges::copy(input, std::back_inserter(data));
    } catch (const std::exception &e) {
        return std::unexpected{
            std::format("Provided string {} is too big to be copied and "
                        "thus too big to be saved to file {}",
                        file.string(),
                        e.what())};
    }
    try {
        stream->write(data.data(), data.size());
    } catch (const std::exception &e) {
        return std::unexpected{std::format(
            "Cannot write to file {}. {}", file.string(), e.what())};
    }
    return data.size();
}