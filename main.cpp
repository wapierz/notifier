#include <cxxopts.hpp>
#include <notifier.hpp>

auto parse_options(int argc, char const *argv[]) {
    cxxopts::Options options("notifier",
                             "////////////////// Send post requests to a given "
                             "url //////////////////\n\n");
    options.add_options()(
        "u,url", "the post url", cxxopts::value<std::string>())(
        "i,interval",
        "notification interval in seconds",
        cxxopts::value<int>()->default_value("5")) /**/ ("h,help", "Usage");
    options.allow_unrecognised_options();
    return std::pair{options, options.parse(argc, argv)};
}


auto on_successful_transfer() {
    return [](cppurl::handle_info info) -> cppurl::notifier::status {
        auto h{info.handle()};
        FORWARD_UNEXPECTED(h);
        std::cout << std::format(
            "\n///\nHandle for {} has completed successfully\n///\n",
            (*h)->url());
        return cppurl::status_ok;
    };
}


auto on_unsuccessful_transfer() {
    return [](cppurl::handle_info info) -> cppurl::notifier::status {
        auto h{info.handle()};
        FORWARD_UNEXPECTED(h);
        std::cout << std::format(
            "\n///\nHandle for {} has failed. Reason: {}\n///\n",
            (*h)->url(),
            info.status().what());
        return cppurl::status_ok;
    };
}


auto on_success(auto &t, std::string_view url) {
    t.tock();
    std::cout << std::format(
        "Post requests for url = {} finished with success!\nTime elapsed "
        "{}\n\n",
        url,
        t.duration());
}


auto on_fail(auto status, std::string_view url) {
    std::cout << std::format(
        "Post requests for url = {} failed! Reason:{}\n\n", url, status.what());
}


int main(int argc, char const *argv[]) {

    timer t{};
    cppurl::notifier::status status{cppurl::status_ok};
    std::string url{};
    try {
        auto [options, result] = parse_options(argc, argv);
        if (result.count("help")) {
            std::cout << options.help() << std::endl;
            return 0;
        }
        url = result["url"].as<std::string>();
        auto interval{std::chrono::seconds{result["interval"].as<int>()}};
        cppurl::notifier ex1{url, interval};
        status = ex1.run(on_successful_transfer(), on_unsuccessful_transfer());
    } catch (const std::exception &e) {
        std::cout << std::format("Exception was thrown. Reason: {}\n\n",
                                 e.what());
        return 0;
    }
    if (!status) {
        on_fail(status, url);
    } else {
        on_success(t, url);
    }


    return 0;
}
