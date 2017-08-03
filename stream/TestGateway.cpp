// Copyright (c) 2014-2017 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Testing.hpp>
#include <Pothos/Framework.hpp>
#include <Pothos/Proxy.hpp>
#include <Pothos/Remote.hpp>
#include <iostream>
#include <json.hpp>

using json = nlohmann::json;

POTHOS_TEST_BLOCK("/blocks/tests", test_gateway)
{
    auto feeder = Pothos::BlockRegistry::make("/blocks/feeder_source", "int");
    auto collector = Pothos::BlockRegistry::make("/blocks/collector_sink", "int");
    auto gateway = Pothos::BlockRegistry::make("/blocks/gateway");

    //create a test plan
    json testPlan;
    testPlan["enableBuffers"] = true;
    testPlan["enableLabels"] = true;
    testPlan["enableMessages"] = true;

    //run the topology
    std::cout << "run the topology\n";
    {
        Pothos::Topology topology;
        topology.connect(feeder, 0, gateway, 0);
        topology.connect(gateway, 0, collector, 0);
        topology.commit();

        //test the drop mode
        gateway.callVoid("setMode", "DROP");
        feeder.callProxy("feedTestPlan", testPlan.dump());
        POTHOS_TEST_TRUE(topology.waitInactive());
        POTHOS_TEST_EQUAL(collector.call<Pothos::BufferChunk>("getBuffer").length, 0);
        POTHOS_TEST_TRUE(collector.call<std::vector<Pothos::Label>>("getLabels").empty());
        POTHOS_TEST_TRUE(collector.call<std::vector<Pothos::Object>>("getMessages").empty());

        //test the backup mode
        gateway.callVoid("setMode", "BACKUP");
        auto expected = feeder.callProxy("feedTestPlan", testPlan.dump());
        POTHOS_TEST_TRUE(topology.waitInactive());
        POTHOS_TEST_EQUAL(collector.call<Pothos::BufferChunk>("getBuffer").length, 0);
        POTHOS_TEST_TRUE(collector.call<std::vector<Pothos::Label>>("getLabels").empty());
        POTHOS_TEST_TRUE(collector.call<std::vector<Pothos::Object>>("getMessages").empty());

        //test the forward mode
        gateway.callVoid("setMode", "FORWARD");
        POTHOS_TEST_TRUE(topology.waitInactive());
        std::cout << "verifyTestPlan!\n";
        collector.callVoid("verifyTestPlan", expected);
    }

    std::cout << "done!\n";
}
