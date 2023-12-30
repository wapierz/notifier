#pragma once

#include <curl/curl.h>

#include <cassert>
#include <exception>
#include <expected>
#include <format>
#include <iostream>
#include <ranges>
#include <span>
#include <stack>
#include <string_view>


#define FORWARD_ERROR(status)             \
    {                                     \
        auto _status{status};             \
        if (!_status) { return _status; } \
    }

#define FORWARD_UNEXPECTED(exp)             \
    {                                       \
        auto _exp{exp};                     \
        if (!_exp) { return _exp.error(); } \
    }


#define UNEXP_FORWARD_ERROR(status)                        \
    {                                                      \
        auto _status{status};                              \
        if (!_status) { return std::unexpected{_status}; } \
    }

#define UNEXP_FORWARD_UNEXPECTED(exp)                        \
    {                                                        \
        auto _exp{exp};                                      \
        if (!_exp) { return std::unexpected{_exp.error()}; } \
    }


namespace cppurl {


    enum class ffor { single, multi, url };

    template <ffor m>
    struct status {
        using code_type = std::conditional_t<
            m == ffor::single,
            CURLcode,
            std::conditional_t<m == ffor::multi, CURLMcode, CURLUcode>>;

      public:
        code_type code{};

      public:
        constexpr operator bool() const {
            if constexpr (m == ffor::single) {
                return (code == CURLE_OK);
            } else if constexpr (m == ffor::multi) {
                return (code == CURLM_OK) || (code == CURLM_CALL_MULTI_PERFORM);
            } else {
                return (code == CURLUE_OK);
            }
        }

      public:
        auto what() const {
            if constexpr ((m == ffor::single)) {
                return std::string_view{curl_easy_strerror(code)};
            } else if constexpr (m == ffor::multi) {
                return std::string_view{curl_multi_strerror(code)};
            } else {
                return std::string_view{curl_url_strerror(code)};
            }
        }
    };


    using b_status = status<ffor::single>;
    using nb_status = status<ffor::multi>;


    template <ffor m>
    class handle;

    template <>
    class handle<ffor::url> {
      private:
        CURLU *_handle{nullptr};
        std::string _uri{};

      public:
        handle() {}
        handle(CURLU *u) : _handle{u} {
            if (!u) {
                throw std::runtime_error{"url handle could not be initialized"};
            }
        }
        ~handle() noexcept {
            if (_handle) { curl_url_cleanup(_handle); }
        }

      public:
        operator bool() const { return _handle != nullptr; }

        auto to_underlying() { return _handle; }

      public:
        auto operator=(std::string_view uri) {
            assert(_handle != nullptr);
            _uri = uri;
            auto s = status<ffor::url>{
                curl_url_set(_handle, CURLUPART_URL, _uri.c_str(), 0)};
            return s;
        }

        auto get() const -> std::string_view { return _uri; }
    };


    template <>
    class handle<ffor::single> {

      private:
        using handle_type = CURL *;
        using code_type = CURLcode;
        using info_type = CURLINFO;
        using error = status<ffor::single>;
        using url_type = handle<ffor::url>;


      private:
        static auto _write(char *data, size_t n, size_t l, void *userp) {
            // std::string_view{data, n * l};
            return n * l;
        };


      private:
        handle_type _handle{curl_easy_init()};
        url_type _url{curl_url()};


      public:
        handle() {
            if (!_handle) {
                throw std::runtime_error{
                    "single handle could not be initialized"};
            }
            if (!error{curl_easy_setopt(
                    _handle, CURLOPT_CURLU, _url.to_underlying())}) {
                throw std::runtime_error{
                    "could not set url handle for single handle"};
            }
            if (!error{curl_easy_setopt(_handle, CURLOPT_PRIVATE, this)}) {
                throw std::runtime_error{
                    "could not set private struct for single handle"};
            };
        }


        auto clean() -> void { curl_easy_cleanup(_handle); }


        ~handle() {
            if (_handle) { curl_easy_cleanup(_handle); }
        }


      public:
        auto url(std::string_view path) -> status<ffor::url> {
            return _url = path;
        }


        auto url() const -> std::string_view { return _url.get(); }


        template <bool should_copy_post_fields>
        auto post(std::string_view data) -> error {
            FORWARD_ERROR(error{
                curl_easy_setopt(_handle, CURLOPT_POSTFIELDSIZE, data.size())});
            if constexpr (should_copy_post_fields) {
                return error{curl_easy_setopt(
                    _handle, CURLOPT_COPYPOSTFIELDS, data.data())};
            } else {
                return error{
                    curl_easy_setopt(_handle, CURLOPT_POSTFIELDS, data.data())};
                ;
            }
        }


        auto perform() -> error { return error{curl_easy_perform(_handle)}; }


        auto to_underlying() { return _handle; }


        auto write(auto &&w) -> error {
            return error{curl_easy_setopt(_handle, CURLOPT_WRITEFUNCTION, w)};
        }
    };


    class handle_info {
      private:
        CURLMsg *_message{nullptr};

      public:
        handle_info(CURLMsg *msg) : _message{msg} {}

      public:
        auto handle()
            -> std::expected<handle<ffor::single> *, status<ffor::single>> {
            cppurl::handle<ffor::single> *h{};
            cppurl::status<ffor::single> s{
                curl_easy_getinfo(_message->easy_handle, CURLINFO_PRIVATE, &h)};
            if (!s) {
                return std::unexpected{s};
            } else {
                return h;
            }
        }


        bool completed() const { return (_message->msg == CURLMSG_DONE); }


        auto status() -> b_status {
            assert(completed());
            return b_status{_message->data.result};
        }


        constexpr operator bool() const { return _message != nullptr; }
    };


    template <>
    class handle<ffor::multi> {
      private:
        using handle_type = CURLM *;
        using code_type = CURLcode;
        using info_type = CURLINFO;
        using handle_info_type = handle_info;
        using error = status<ffor::multi>;


      private:
        handle_type _multi_handle{};


      public:
        handle() : _multi_handle{curl_multi_init()} {
            if (!_multi_handle) {
                throw std::runtime_error("could not initialize multi handle");
            }
        }


        ~handle() {
            if (_multi_handle) { curl_multi_cleanup(_multi_handle); }
        }

      public:
        auto add(handle<ffor::single> &h) -> error {
            return error{
                curl_multi_add_handle(_multi_handle, h.to_underlying())};
        }

        auto remove(handle<ffor::single> &h) -> error {
            return error{
                curl_multi_remove_handle(_multi_handle, h.to_underlying())};
        }


        auto perform() -> std::expected<int, error> {
            int still_running_handles{-1};
            error s{curl_multi_perform(_multi_handle, &still_running_handles)};
            if (s) {
                return still_running_handles;
            } else {
                return std::unexpected{s};
            }
        }


        auto info() -> std::pair<handle_info_type, int> {
            int messages_left{-1};
            auto msg{curl_multi_info_read(_multi_handle, &messages_left)};
            return std::pair(handle_info_type{msg}, messages_left);
        }


        auto maximal_number_of_connections(int64_t n) -> error {
            return error{
                curl_multi_setopt(_multi_handle, CURLMOPT_MAXCONNECTS, n)};
        }

        auto wait(int timeout_ms) -> std::expected<int, error> {
            int number_of_file_descriptor_events{};
            error e{curl_multi_wait(_multi_handle,
                                    nullptr,
                                    0,
                                    timeout_ms,
                                    &number_of_file_descriptor_events)};
            if (!e) {
                return std::unexpected{e};
            } else {
                return number_of_file_descriptor_events;
            }
        }


        auto to_underlying() { return _multi_handle; }


        auto clean(handle<ffor::single> &h) -> void { h.clean(); }
    };


    using nb_handle = class handle<ffor::multi>;

    using b_handle = class handle<ffor::single>;


    template <size_t N>
    class handle_pool {
      private:
        std::array<b_handle, N> _handles{};
        std::stack<b_handle *> _available_handles{};

      public:
        handle_pool() {
            for (auto &h : _handles) { _available_handles.push(&h); }
        }

      public:
        auto get() -> b_handle & {
            auto h{_available_handles.top()};
            _available_handles.pop();
            return *h;
        }


        auto add(b_handle &h) { _available_handles.push(std::addressof(h)); }


        auto size() const { return _available_handles.size(); }
    };


    template <typename CRTP>
    class app {
      public:
        app() {
            if (!status<ffor::single>{curl_global_init(CURL_GLOBAL_ALL)}) {
                throw std::runtime_error("app could not initialize curl");
            };
        }


        ~app() noexcept { curl_global_cleanup(); }

      public:
        auto run(auto &&...args) {
            return static_cast<CRTP &>(*this).run(
                std::forward<decltype(args)>(args)...);
        }
    };


}  // namespace cppurl