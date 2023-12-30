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

/*
 * WARNING! THIS HEADER IS A SIMPLE CPP WRAPPER AROUND CURL BASIC STRUCTURES
 * (E.G. SIMPLE/MULTI/URL HANDLES/STATUSES). IN PARTICULAR, WE DO NOT TAKE MUCH
 * CARE OF MAKING THIS DOCUMENTATION SELF-SUFFICIENT. IF YOU HAVE ANY DOUBTS
 * REFER TO THE DOCUMENTATION OF CURL LIBRARY;
 */


/**
 * @brief      forwards error if any occured
 *
 * @param      status  status to be checked for an error
 *
 * @return     void
 */
#define FORWARD_ERROR(status)             \
    {                                     \
        auto _status{status};             \
        if (!_status) { return _status; } \
    }

/**
 * @brief      if std::expected holds error forward this error
 *
 * @param      exp   std::expected
 *
 * @return     void
 */
#define FORWARD_UNEXPECTED(exp)             \
    {                                       \
        auto _exp{exp};                     \
        if (!_exp) { return _exp.error(); } \
    }


/**
 * @brief      if error occured forward this error wrapped into std::unexpected
 *
 * @param      status  The status
 *
 * @return     void
 */
#define UNEXP_FORWARD_ERROR(status)                        \
    {                                                      \
        auto _status{status};                              \
        if (!_status) { return std::unexpected{_status}; } \
    }

/**
 * @brief      if std::expected holds error forward this error wrapped into
 * std::unexpected
 *
 * @param      exp   std::expected
 *
 * @return     void
 */
#define UNEXP_FORWARD_UNEXPECTED(exp)                        \
    {                                                        \
        auto _exp{exp};                                      \
        if (!_exp) { return std::unexpected{_exp.error()}; } \
    }


namespace cppurl {


    /**
     * @brief      Used to distinguish between curl simple, multi and url types
     */
    enum class ffor { single, multi, url };

    /**
     * @brief      Wrapper for curl status
     *
     * @tparam     m
     */
    template <ffor m>
    struct status {
        using code_type = std::conditional_t<
            m == ffor::single,
            CURLcode,
            std::conditional_t<m == ffor::multi, CURLMcode, CURLUcode>>;

      public:
        code_type code{};

      public:
        /**
         * @brief      Returns true iff no error occured
         */
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
        /**
         * @brief      Explains error reason
         *
         * @return     Human readable error description.
         */
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


    /**
     * @brief      This classes are wrappers for corresponding curl handles.
     *
     * @tparam     m
     */
    template <ffor m>
    class handle;

    /**
     * @brief      This is a wrapper for curl url handle
     */
    template <>
    class handle<ffor::url> {
      private:
        CURLU *_handle{curl_url()};
        std::string _uri{};

      public:
        /**
         * @brief      Constructs a new instance.
         */
        handle() {
            if (!_handle) {
                throw std::runtime_error{"url handle could not be initialized"};
            }
        }


        /**
         * @brief      Destroys the object.
         */
        ~handle() noexcept {
            if (_handle) { curl_url_cleanup(_handle); }
        }

      public:
        /**
         * @brief      Checks if this handle is in valid state;
         */
        operator bool() const { return _handle != nullptr; }

        /**
         * @brief      Returns an underlying curl representation of the object.
         *
         * @return     Underlying representation of the object.
         */
        auto to_underlying() { return _handle; }

      public:
        /**
         * @brief      Assigns new url adress
         *
         * @param[in]  uri   The new url
         *
         * @return     status<ffor::url>
         */
        auto operator=(std::string_view uri) {
            assert(_handle != nullptr);
            _uri = uri;
            auto s = status<ffor::url>{
                curl_url_set(_handle, CURLUPART_URL, _uri.c_str(), 0)};
            return s;
        }

        /**
         * @brief      View onto current url.
         *
         * @return     std::string_view onto current url;
         */
        auto get() const -> std::string_view { return _uri; }
    };


    /**
     * @brief      A wrapper for curl simple handle.
     */
    template <>
    class handle<ffor::single> {

      private:
        using handle_type = CURL *;
        using code_type = CURLcode;
        using info_type = CURLINFO;
        using error = status<ffor::single>;
        using url_type = handle<ffor::url>;


      private:
        /**
         * @brief      Template for curl writing function. Does nothing.
         *
         * @param      data   The response data
         * @param[in]  n      number of bytes
         * @param[in]  l      always 1
         * @param      userp  The userp
         *
         * @return     { description_of_the_return_value }
         */
        static auto _write(char *data, size_t n, size_t l, void *userp) {
            return n * l;
        };


      private:
        handle_type _handle{curl_easy_init()};
        url_type _url{};


      public:
        /**
         * @brief      Constructs a new instance.
         */
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


        /**
         * @brief      Destroys the object.
         */
        ~handle() {
            if (_handle) { curl_easy_cleanup(_handle); }
        }


      public:
        /**
         * @brief      Url setter.
         *
         * @param[in]  path  new url
         *
         * @return     status of setting new url.
         */
        auto url(std::string_view path) -> status<ffor::url> {
            return _url = path;
        }


        /**
         * @brief      Url getter
         *
         * @return     the current url of this handle
         */
        auto url() const -> std::string_view { return _url.get(); }


        /**
         * @brief      Sets this handle to post mode and sets post fields.
         *
         * @param[in]  data Post fields
         *
         * @tparam     should_copy_post_fields  If true curl copies data,
         * otherwise user is responsible for lifetime of data.
         *
         * @return     status
         */
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


        /**
         * @brief      Perform wrapper
         *
         * @return     status
         */
        auto perform() -> error { return error{curl_easy_perform(_handle)}; }


        /**
         * @brief      Returns an underlying representation of the object.
         *
         * @return     curl simple handle
         */
        auto to_underlying() { return _handle; }


        /**
         * @brief      Write function setter
         *
         * @param      w     new write function
         *
         * @return     status
         */
        auto write(auto &&w) -> error {
            return error{curl_easy_setopt(_handle, CURLOPT_WRITEFUNCTION, w)};
        }
    };


    /**
     * @brief      This class continas information about handle. It is needed in
     * handle<ffor::multi>.
     */
    class handle_info {
      private:
        CURLMsg *_message{nullptr};

      public:
        /**
         * @brief      Constructs a new instance.
         *
         * @param      msg   The curl message
         */
        handle_info(CURLMsg *msg) : _message{msg} {}

      public:
        /**
         * @brief      single handle getter
         *
         * @return     single handle to which this info corresponds
         */
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


        /**
         * @brief      Checks if handle completed it job (still you need to
         * check if it succeeded or failed)
         *
         * @return     True iff handle is done.
         */
        bool completed() const { return (_message->msg == CURLMSG_DONE); }


        /**
         * @brief      Current status of the handle.
         *
         * @return     status of the handle
         */
        auto status() -> b_status {
            assert(completed());
            return b_status{_message->data.result};
        }


        /**
         * @brief      True iff there is some message
         */
        constexpr operator bool() const { return _message != nullptr; }
    };


    /**
     * @brief      Wrapper for curl multi handle.
     */
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
        /**
         * @brief      Constructs a new instance.
         */
        handle() : _multi_handle{curl_multi_init()} {
            if (!_multi_handle) {
                throw std::runtime_error("could not initialize multi handle");
            }
        }


        /**
         * @brief      Destroys the object.
         */
        ~handle() {
            if (_multi_handle) { curl_multi_cleanup(_multi_handle); }
        }

      public:
        /**
         * @brief     Adds simple handle
         *
         * @param      h     simple handle
         *
         * @return     status
         */
        auto add(handle<ffor::single> &h) -> error {
            return error{
                curl_multi_add_handle(_multi_handle, h.to_underlying())};
        }

        /**
         * @brief      Removes the specified handle.
         *
         * @param      h     simple handle
         *
         * @return     status
         */
        auto remove(handle<ffor::single> &h) -> error {
            return error{
                curl_multi_remove_handle(_multi_handle, h.to_underlying())};
        }


        /**
         * @brief      Wrapper for multi perform
         *
         * @return     Still running handles if succeded. Otherwise, error.
         */
        auto perform() -> std::expected<int, error> {
            int still_running_handles{-1};
            error s{curl_multi_perform(_multi_handle, &still_running_handles)};
            if (s) {
                return still_running_handles;
            } else {
                return std::unexpected{s};
            }
        }


        /**
         * @brief      Wrapper fo info_read
         *
         * @return     Information about handle and number of ready handles.
         */
        auto info() -> std::pair<handle_info_type, int> {
            int messages_left{-1};
            auto msg{curl_multi_info_read(_multi_handle, &messages_left)};
            return std::pair(handle_info_type{msg}, messages_left);
        }


        /**
         * @brief      Setter of maximal number of similtanous connections
         *
         * @param[in]  n     max number of connections
         *
         * @return     status
         */
        auto maximal_number_of_connections(int64_t n) -> error {
            return error{
                curl_multi_setopt(_multi_handle, CURLMOPT_MAXCONNECTS, n)};
        }

        /**
         * @brief      Wrapper for curl wait
         *
         * @param[in]  timeout_ms  The timeout milliseconds
         *
         * @return     number of file descriptor events or error
         */
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


        /**
         * @brief      Returns an underlying  curl multi handle.
         *
         * @return     Returns an underlying  curl multi handle.
         */
        auto to_underlying() { return _multi_handle; }
    };


    using nb_handle = class handle<ffor::multi>;

    using b_handle = class handle<ffor::single>;


    /**
     * @brief      Static set of simple handles
     *
     * @tparam     N     Number of available handles.
     */
    template <size_t N>
    class handle_pool {
      private:
        std::array<b_handle, N> _handles{};
        std::stack<b_handle *> _available_handles{};

      public:
        /**
         * @brief      Constructs a new instance.
         */
        handle_pool() {
            for (auto &h : _handles) { _available_handles.push(&h); }
        }

      public:
        /**
         * @brief      Gets a free simple handle
         *
         * @return     Simple handle
         */
        auto get() -> b_handle & {
            auto h{_available_handles.top()};
            _available_handles.pop();
            return *h;
        }


        /**
         * @brief      Adds new simple handle to pool.
         *
         * @param      h     simple handle
         *
         * @return     void
         */
        auto add(b_handle &h) { _available_handles.push(std::addressof(h)); }


        /**
         * @brief      Size of currently available handles.
         *
         * @return     Size of currently available handles.
         */
        auto size() const { return _available_handles.size(); }
    };


    /**
     * @brief      This is a simple app which initializes curl. IMPORTANT!
     * THERE CAN BE ONLY ONE INSTANCE OF THIS CLASS IN A PROGRAM.
     *
     * @tparam     CRTP  The underlying application.
     */
    template <typename CRTP>
    class app {
      public:
        /**
         * @brief      Constructs a new instance.
         */
        app() {
            if (!status<ffor::single>{curl_global_init(CURL_GLOBAL_ALL)}) {
                throw std::runtime_error("app could not initialize curl");
            };
        }


        /**
         * @brief      Destroys the object.
         */
        ~app() noexcept { curl_global_cleanup(); }

      public:
        /**
         * @brief      Runs CRTP::run. The derived class must implement this method.
         *
         * @param      args  The arguments.
         *
         * @return     any
         */
        auto run(auto &&...args) {
            return static_cast<CRTP &>(*this).run(
                std::forward<decltype(args)>(args)...);
        }
    };


}  // namespace cppurl