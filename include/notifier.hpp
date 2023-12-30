#pragma once

#include <cppurl.hpp>
#include <csignal>
#include <future>
#include <queue>
#include <timer.hpp>

/*this global variable is used to handle interuption signal in the notifier
 * class*/
constinit inline bool should_stop{false};


/**
 * @brief      Handle for interruption signal. Sets should_stop to true.
 *
 * @param[in]  sig   The signal
 *
 * @return     void
 */
extern "C" auto handle_interuption(int sig) -> void {
    std::cout << "\n///\nReceived interruption signal\n///\n";
    should_stop = true;
};


/**
 * @brief      Reads all from std::cin and returns it as a std::string
 *
 * @return     std::string from std::cin
 */
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


    /**
     * @brief      Notifier application.
     */
    class notifier : public app<notifier> {


      private:
        static constexpr size_t max_num_of_connections{100};
        static constexpr int poll_wait_time{100};


      public:
        /**
         * @brief      General status (combines single/multi/url statuses into
         * one)
         */
        struct status : std::variant<cppurl::status<ffor::single>,
                                     cppurl::status<ffor::multi>,
                                     cppurl::status<ffor::url>> {

          public:
            using variant = std::variant<cppurl::status<ffor::single>,
                                         cppurl::status<ffor::multi>,
                                         cppurl::status<ffor::url>>;

          public:
            /**
             * @brief      Constructs a new instance.
             *
             * @param[in]  s     { parameter_description }
             */
            constexpr status(cppurl::status<ffor::single> s) : variant{s} {};

            /**
             * @brief      Constructs a new instance.
             *
             * @param[in]  s     { parameter_description }
             */
            constexpr status(cppurl::status<ffor::multi> s) : variant{s} {};

            /**
             * @brief      Constructs a new instance.
             *
             * @param[in]  s     { parameter_description }
             */
            constexpr status(cppurl::status<ffor::url> s) : variant{s} {};


            /**
             * @brief     True iff status is ok.
             */
            constexpr operator bool() const {
                return std::visit([](auto s) -> bool { return s; }, *this);
            }


            /**
             * @brief      Human readable description of status.
             *
             * @return     std::string_view description of status
             */
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
        /**
         * @brief      Reads stdin list of post requests.
         *
         * @return     A view consisiting of post field strings.
         */
        auto read_stdin_requests() const {
            return std::ranges::owning_view{cin_to_string()} |
                   std::views::split(std::string_view{"\n"}) |
                   std::views::transform([](auto &&req) -> std::string {
                       return std::string{
                           std::string_view{std::forward<decltype(req)>(req)}};
                   });
        }


        /**
         * @brief      Adds a post request using free handle from the pool and
         * assigning it a request from requests queue.
         *
         * @return     status
         */
        auto add_post_request() -> status {
            auto &handle{pool.get()};
            FORWARD_ERROR(handle.url(url));
            FORWARD_ERROR(handle.template post<true>(requests.front()));
            requests.pop();
            FORWARD_ERROR(mhandle.add(handle));
            return cppurl::status<ffor::multi>{CURLM_OK};
        };


        /**
         * @brief      Adds post requests. Reads stdin for new requests, adds
         * them the the queue and then launches as many post requests as
         * possible (this number is limited due to max_number_of_connections).
         *
         * @return     status
         */
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
        /**
         * @brief      Handles a completed transfer case.
         *
         * @param[in]  handle_info               The handle information
         * @param      on_successful_transfer    Function to be launched on
         * successful transfer
         * @param      on_unsuccessful_transfer  Function to be launched on
         * unsuccessful transfer
         *
         * @return     status
         */
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


        /**
         * @brief      Iterates over finished transfers and for each of them
         * fires handle_completed_transfer function (see above).
         *
         * @param      on_successful_transfer    Function to be launched on
         * successful transfer
         * @param      on_unsuccessful_transfer  Function to be launched on
         * unsuccessful transfer
         *
         * @return     Number of still available completed tasks which were not
         * handled (should be always 0).
         */
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
                info = mhandle.info();
            }
            return info.second;
        }

      public:
        /**
         * @brief      Constructs a new instance of notifier assigning it a
         * destination url and time interval.
         *
         * @param[in]  url                The destination url
         * @param[in]  time_for_new_data  After this time we repeatedly check
         * for new data
         */
        notifier(std::string_view url, std::chrono::seconds time_for_new_data)
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
        /**
         * @brief      Runs an application. Every iteration of the loop launches
         * transfers, handles completed transfers (if any) and if there is high
         * time for new data reads it from stdin. Runs until interuption signal
         * SIGINT is received.
         *
         * @param      on_successful_transfer    Function to be launched on
         * successful transfer. It must be of the form [](cppurl::handle_info
         * info) -> cppurl::notifier::status. If cppurl::status_ok is return
         * then this aplication does not abort. Otherwise it does and return the
         * error.
         * @param      on_unsuccessful_transfer  Function to be launched on
         * unsuccessful transfer. It must be of the form [](cppurl::handle_info
         * info) -> cppurl::notifier::status. If cppurl::status_ok is return
         * then this aplication does not abort. Otherwise it does and return the
         * error.
         *
         * @return     status
         */
        auto run(auto &&on_successful_transfer, auto &&on_unsuccessful_transfer)
            -> status {
            FORWARD_ERROR(add_post_requests());
            std::expected<int, status> ready_handles{0};
            _timer.tick();
            do {
                FORWARD_UNEXPECTED(mhandle.perform());
                ready_handles = handle_finished_transfers(
                    on_successful_transfer, on_unsuccessful_transfer);
                FORWARD_UNEXPECTED(ready_handles);
                _timer.tock();
                if (_timer.duration<std::chrono::milliseconds>() >=
                    time_for_new_data) {
                    FORWARD_ERROR(add_post_requests());
                    _timer.tick();
                }
                FORWARD_UNEXPECTED(mhandle.wait(poll_wait_time));
            } while (!::should_stop ||
                     (ready_handles && ready_handles.value() > 0));


            return cppurl::status<ffor::multi>{CURLM_OK};
        }
    };

    /*Non error cppurl::notifier::status, can be used in on_successful_transfer
     * and on_unsuccessful_transfer in cpp::notifier::run method*/
    constexpr notifier::status status_ok{
        cppurl::status<cppurl::ffor::multi>{CURLM_OK}};
}  // namespace cppurl