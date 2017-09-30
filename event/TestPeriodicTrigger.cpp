// Copyright (c) 2014-2015 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Testing.hpp>
#include <Pothos/Framework.hpp>
#include <Pothos/Proxy.hpp>
#include <chrono>
#include <thread>
#include <iostream>

POTHOS_TEST_BLOCK("/blocks/tests", test_periodic_trigger)
{
    auto trigger = Pothos::BlockRegistry::make("/blocks/periodic_trigger");
    trigger.call("setRate", 4.0);
    auto collector = Pothos::BlockRegistry::make("/blocks/collector_sink", "int");

    //run the topology
    {
        Pothos::Topology topology;
        topology.connect(trigger, "triggered", collector, 0);
        topology.commit();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    //collect the messages
    std::vector<Pothos::Object> msgs = collector.call("getMessages");
    std::cout << msgs.size() << std::endl;

    //we expect 4 messages in one second,
    //but since this is rough timing, just check +/-1
    POTHOS_TEST_TRUE(msgs.size() >= 3);
    POTHOS_TEST_TRUE(msgs.size() <= 5);
}
