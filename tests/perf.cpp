
/* Copyright (c) 2016, Human Brain Project
 *                     Stefan.Eilemann@epfl.ch
 */

// Performance test measuring pub-sub throughput

#define BOOST_TEST_MODULE zeroeq_perf

#include "common.h"
#include <servus/servus.h>
#include <servus/uri.h>

#include <chrono>
#include <cmath>
#include <thread>

using std::chrono::duration_cast;
using std::chrono::high_resolution_clock;
using std::chrono::microseconds;
using std::chrono::milliseconds;

namespace
{
class Message : public servus::Serializable
{
public:
    Message(const size_t size)
        : _bottle(size, '0')
    {
    }
    std::string getTypeName() const final { return "zeroeq::test::Message"; }
private:
    bool _fromBinary(const void*, const size_t) final { return true; }
    Data _toBinary() const final
    {
        Data data;
        data.ptr =
            std::shared_ptr<const void>(_bottle.data(), [](const void*) {});
        data.size = _bottle.length();
        return data;
    }

    std::string _bottle;
};

class Runner
{
public:
    Runner(const size_t size)
        : message(size)
        , sent(0)
        , running(false)
    {
    }

    void run(zeroeq::Publisher& publisher)
    {
        running = true;
        sent = 0;

        while (running)
        {
            publisher.publish(message);
            ++sent;
        }
    }

    const Message message;
    size_t sent;
    bool running;
};
}

BOOST_AUTO_TEST_CASE(throughput)
{
    zeroeq::Publisher publisher(zeroeq::URI("127.0.0.1"), zeroeq::NULL_SESSION);
    zeroeq::Subscriber subscriber(zeroeq::URI(publisher.getURI()));
    {
        // establish subscription
        Message message(4096);
        subscriber.subscribe(message);
        while (true)
        {
            publisher.publish(message);
            if (subscriber.receive(100 /*ms*/))
                break;
        }
        while (subscriber.receive(0)) /* flush pending messages */
            ;
        subscriber.unsubscribe(message);
    }

    std::cout << "msg size, MB/s, P/s, loss" << std::endl;
    for (size_t i = 1; i <= 256 * 1024 * 1024; i = i << 1)
    {
        Runner runner(i);
        Message message(i);
        size_t received = 0;
        auto endTime = high_resolution_clock::now();

        message.registerDeserializedCallback([&] {
            ++received;
            endTime = high_resolution_clock::now();
        });
        subscriber.subscribe(message);

        const auto startTime = high_resolution_clock::now();
        std::thread thread([&] { runner.run(publisher); });

        while (duration_cast<milliseconds>(high_resolution_clock::now() -
                                           startTime)
                   .count() < 500)
        {
            subscriber.receive();
        }
        const uint64_t wait = ((i / 2000000) + 1) * 100;
        runner.running = false;
        while (received < runner.sent && subscriber.receive(wait))
            /* nop */;

        const float seconds =
            float(duration_cast<microseconds>(endTime - startTime).count()) /
            1000000.f;
        const int loss = std::round(float(runner.sent - received) /
                                    float(runner.sent) * 100.f);
        const std::string size =
            (i >= 1024 * 1024 ? std::to_string(i >> 20) + "M"
                              : i >= 1024 ? std::to_string(i >> 10) + "K"
                                          : std::to_string(i));

        thread.join();
        subscriber.unsubscribe(message);

        std::cout << size << ", "
                  << float(received * i) / 1024.f / 1024.f / seconds << ", "
                  << float(received) / seconds << ", " << loss << "%"
                  << std::endl;
    }
}
