#pragma once

#include <cppurl.hpp>
#include <csignal>
#include <future>
#include <queue>
#include <timer.hpp>

constinit inline bool should_stop{false};


extern "C" auto handle_interuption(int sig) -> void {
    std::cout << "\n///\nReceived interruption signal\n///\n";
    should_stop = true;
};


auto cin_to_string() -> std::string {
    static constexpr std::chrono::seconds timeout(5);
    std::future<std::string> future = std::async([]() -> std::string {
        return {std::istreambuf_iterator<char>{std::cin},
                std::istreambuf_iterator<char>{}};
    });
    if (future.wait_for(timeout) == std::future_status::ready) {
        return future.get();
    } else {
        return {};
    }
}


namespace cppurl {


    class notifier : public app<notifier> {


      private:
        static constexpr size_t max_num_of_connections{100};
        static constexpr int timeout_ms{1000};
        static constexpr int poll_wait_time{100};


      public:
        struct status : std::variant<cppurl::status<ffor::single>,
                                     cppurl::status<ffor::multi>,
                                     cppurl::status<ffor::url>> {

          public:
            using variant = std::variant<cppurl::status<ffor::single>,
                                         cppurl::status<ffor::multi>,
                                         cppurl::status<ffor::url>>;

          public:
            constexpr status(cppurl::status<ffor::single> s) : variant{s} {};

            constexpr status(cppurl::status<ffor::multi> s) : variant{s} {};

            constexpr status(cppurl::status<ffor::url> s) : variant{s} {};


            constexpr operator bool() const {
                return std::visit([](auto s) -> bool { return s; }, *this);
            }


            constexpr auto what() const {
                return std::visit([](auto s) { return s.what(); }, *this);
            }
        };


      private:
        handle_pool<max_num_of_connections> pool{};
        nb_handle mhandle{};
        std::string_view url{};
        std::queue<std::string> requests{};
        timer<std::chrono::steady_clock> _timer{};
        const std::chrono::seconds time_for_new_data{1};

      private:
        auto read_stdin_requests() const {
            return std::ranges::owning_view{cin_to_string()} |
                   std::views::split(std::string_view{"\n"}) |
                   std::views::transform([](auto &&req) -> std::string {
                       return std::string{
                           std::string_view{std::forward<decltype(req)>(req)}};
                   });
        }


        auto add_post_request() -> status {
            auto &handle{pool.get()};
            FORWARD_ERROR(handle.url(url));
            FORWARD_ERROR(handle.template post<true>(requests.front()));
            requests.pop();
            FORWARD_ERROR(mhandle.add(handle));
            return cppurl::status<ffor::multi>{CURLM_OK};
        };


        auto add_post_requests() -> status {

            for (auto &&req : read_stdin_requests()) {
                requests.push(std::move(req));
            }
            auto min{std::min(requests.size(), pool.size())};
            for (size_t i{0}; i < min; ++i) {
                FORWARD_ERROR(add_post_request());
            }
            return cppurl::status<ffor::multi>{CURLM_OK};
        }

      private:
        auto handle_completed_transfer(auto handle_info,
                                       auto &&on_successful_transfer,
                                       auto &&on_unsuccessful_transfer)
            -> status {

            if (handle_info.status()) {
                FORWARD_ERROR(on_successful_transfer(handle_info));
            } else {
                FORWARD_ERROR(on_unsuccessful_transfer(handle_info));
            }
            auto h{handle_info.handle()};
            FORWARD_UNEXPECTED(h);
            FORWARD_ERROR(mhandle.remove(**h));
            pool.add(**h);
            if (!::should_stop && !requests.empty()) {
                FORWARD_ERROR(add_post_request());
            }
            return cppurl::status<ffor::multi>{CURLM_OK};
        }


        auto handle_finished_transfers(auto &&on_successful_transfer,
                                       auto &&on_unsuccessful_transfer)
            -> std::expected<int, status> {
            auto info{mhandle.info()};
            while (info.first) {
                auto [handle_info, _] = info;
                if (handle_info.completed()) {
                    UNEXP_FORWARD_ERROR(
                        handle_completed_transfer(handle_info,
                                                  on_successful_transfer,
                                                  on_unsuccessful_transfer));
                } else {
                    UNEXP_FORWARD_ERROR(on_unsuccessful_transfer(handle_info));
                }
                UNEXP_FORWARD_UNEXPECTED(mhandle.wait(timeout_ms));
                info = mhandle.info();
            }
            return info.second;
        }

      public:
        notifier(std::string_view url,
                     std::chrono::seconds time_for_new_data)
            : url{url}, time_for_new_data{time_for_new_data} {

            if (!mhandle.maximal_number_of_connections(
                    max_num_of_connections)) {
                throw std::runtime_error(
                    "post example could not set maximal number of connections");
            }
            if (std::signal(SIGINT, handle_interuption) == SIG_ERR) {
                throw std::runtime_error(
                    "post example could not set custom handling for "
                    "interruption signal");
            }
        }


      public:
        auto run(auto &&on_successful_transfer, auto &&on_unsuccessful_transfer)
            -> status {
            FORWARD_ERROR(add_post_requests());
            std::expected<int, status> still_working_handles{0};
            _timer.tick();
            do {
                FORWARD_UNEXPECTED(mhandle.perform());
                still_working_handles = handle_finished_transfers(
                    on_successful_transfer, on_unsuccessful_transfer);
                FORWARD_UNEXPECTED(still_working_handles);
                _timer.tock();
                if (_timer.duration<std::chrono::milliseconds>() >=
                    time_for_new_data) {
                    FORWARD_ERROR(add_post_requests());
                    _timer.tick();
                }
                mhandle.wait(poll_wait_time);
            } while (!::should_stop || (still_working_handles &&
                                        still_working_handles.value() > 0));


            return cppurl::status<ffor::multi>{CURLM_OK};
        }
    };

    constexpr notifier::status status_ok{
        cppurl::status<cppurl::ffor::multi>{CURLM_OK}};
}  // namespace cppurl