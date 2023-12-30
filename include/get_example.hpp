#pragma once

#include <cppurl.hpp>

namespace cppurl {


    constexpr std::array urls{
        "https://www.microsoft.com",
        "https://opensource.org",
        "https://www.google.com",
        "https://www.yahoo.com",
        "https://www.ibm.com",
        "https://www.mysql.com",
        "https://www.oracle.com",
        "https://www.ripe.net",
        "https://www.iana.org",
        "https://www.amazon.com",
        "https://www.netcraft.com",
        "https://www.heise.de",
        "https://www.chip.de",
        "https://www.ca.com",
        "https://www.cnet.com",
        "https://www.mozilla.org",
        "https://www.cnn.com",
        "https://www.wikipedia.org",
        "https://www.dell.com",
        "https://www.hp.com",
        "https://www.cert.org",
        "https://www.mit.edu",
        "https://www.nist.gov",
        "https://www.ebay.com",
        "https://www.playstation.com",
        "https://www.uefa.com",
        "https://www.ieee.org",
        "https://www.apple.com",
        "https://www.symantec.com",
        "https://www.zdnet.com",
        "https://www.fujitsu.com/global/",
        "https://www.supermicro.com",
        "https://www.hotmail.com",
        "https://www.ietf.org",
        "https://www.bbc.co.uk",
        "https://news.google.com",
        "https://www.foxnews.com",
        "https://www.msn.com",
        "https://www.wired.com",
        "https://www.sky.com",
        "https://www.usatoday.com",
        "https://www.cbs.com",
        "https://www.nbc.com/",
        "https://slashdot.org",
        "https://www.informationweek.com",
        "https://apache.org",
        "https://www.un.org",
    };

    class get_example : public app<get_example> {

      public:
        static constexpr int64_t MAX_PARALLEL =
            10; /* number of simultaneous transfers */
        static constexpr auto NUM_URLS = urls.size();


      public:
        static size_t write_cb([[maybe_unused]] char *data,
                               size_t n,
                               size_t l,
                               [[maybe_unused]] void *userp) {
            /* take care of the data here, ignored in this example */
            // std::string_view d{data, n * l};
            // std::cout << std::format("{}", std::string_view{d});
            // (void)data;
            // (void)userp;
            return n * l;
        }


      private:
        handle_pool<MAX_PARALLEL> pool{};
        nb_handle mhandle{};

      private:
        struct status : std::variant<cppurl::status<ffor::single>,
                                     cppurl::status<ffor::multi>,
                                     cppurl::status<ffor::url>> {

            using variant = std::variant<cppurl::status<ffor::single>,
                                         cppurl::status<ffor::multi>,
                                         cppurl::status<ffor::url>>;
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
        auto add_transfer(auto url) -> status {
            // std::cout << std::format("pool size = {}\n\n", pool.size());
            auto &handle{pool.get()};
            FORWARD_ERROR(handle.write(write_cb));
            // std::cout << std::format("wrtie set\n", pool.size());

            FORWARD_ERROR(handle.url(url));
            // std::cout << std::format("url set\n", pool.size());

            FORWARD_ERROR(mhandle.add(handle));
            // std::cout << std::format("single handle added to multi\n",
            //                          pool.size());

            return cppurl::status<ffor::multi>{CURLM_OK};
        };


      public:
        auto on_completed_transfer(handle_info info) -> status {
            auto h{info.handle()};
            FORWARD_UNEXPECTED(h);
            auto url{(*h)->url()};
            std::cout << std::format(
                "handle for {} has completed with code = {} and "
                "what = {}\n\n",
                url,
                (int)info.status().code,
                info.status().what());
            return cppurl::status<ffor::multi>{CURLM_OK};
        }


        auto run() -> status {


            unsigned int transfers{0};
            int left{0};
            FORWARD_ERROR(mhandle.maximal_number_of_connections(MAX_PARALLEL));
            for (auto &&url : urls | std::views::take(pool.size())) {
                FORWARD_ERROR(add_transfer(url));
                ++left;
                ++transfers;
            }
            // std::cout << std::format("transfers added\n");
            do {
                FORWARD_UNEXPECTED(mhandle.perform());
                auto info{mhandle.info()};
                // FORWARD_UNEXPECTED(info);
                // std::cout << std::format("before while");
                std::cout << std::format(
                "info first bool = {}, info second = {}\n\n",
                bool{info.first},
                info.second);
                while (info.first) {
                    auto [h_info, _] = info;
                    if (h_info.completed()) {
                        FORWARD_ERROR(on_completed_transfer(h_info));
                        auto h{h_info.handle()};
                        FORWARD_UNEXPECTED(h);
                        FORWARD_ERROR(mhandle.remove(**h));
                        pool.add(**h);
                        if (transfers < NUM_URLS) {
                            FORWARD_ERROR(add_transfer(urls[transfers++]));
                        } else {
                            --left;
                        }
                    }
                    if (left) { mhandle.wait(1000); }
                    info = mhandle.info();
                }
                // std::cout << std::format("left = {}\n", left);
            } while (left);


            return cppurl::status<ffor::multi>{CURLM_OK};
        }
    };
}  // namespace cppurl
