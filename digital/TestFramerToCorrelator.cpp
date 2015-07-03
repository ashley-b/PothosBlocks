// Copyright (c) 2015-2015 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Testing.hpp>
#include <Pothos/Framework.hpp>
#include <Pothos/Proxy.hpp>
#include <Pothos/Remote.hpp>
#include <Poco/JSON/Object.h>
#include <iostream>

POTHOS_TEST_BLOCK("/blocks/tests", test_framer_to_correlator)
{
    auto env = Pothos::ProxyEnvironment::make("managed");
    auto registry = env->findProxy("Pothos/BlockRegistry");

    auto feeder = registry.callProxy("/blocks/feeder_source", "uint8");
    auto generator = registry.callProxy("/blocks/packet_to_stream");
    auto framer = registry.callProxy("/blocks/preamble_framer");
    auto correlator = registry.callProxy("/blocks/preamble_correlator");
    auto deframer = registry.callProxy("/blocks/stream_to_packet");
    auto collector = registry.callProxy("/blocks/collector_sink", "uint8");

    //Copy block provides the loopback path:
    //Copy can cause buffer boundaries to change,
    //which helps to aid in robust testing.
    auto copier = registry.callProxy("/blocks/copier");

    //configuration constants
    const size_t mtu = 107;
    const std::string txFrameStartId = "txFrameStart";
    const std::string txFrameEndId = "txFrameEnd";
    const std::string rxFrameStartId = "rxFrameStart";
    const size_t maxValue = 1;
    std::vector<unsigned char> preamble;
    for (size_t i = 0; i < 32; i++) preamble.push_back(std::rand() % (maxValue+1));

    //configure
    generator.callVoid("setFrameStartId", txFrameStartId);
    generator.callVoid("setFrameEndId", txFrameEndId);
    generator.callVoid("setName", "frameGenerator");
    framer.callVoid("setPreamble", preamble);
    framer.callVoid("setFrameStartId", txFrameStartId);
    framer.callVoid("setFrameEndId", txFrameEndId);
    framer.callVoid("setPaddingSize", 10);
    correlator.callVoid("setPreamble", preamble);
    correlator.callVoid("setThreshold", 0); //expect perfect match
    correlator.callVoid("setFrameStartId", rxFrameStartId);
    deframer.callVoid("setFrameStartId", rxFrameStartId);
    deframer.callVoid("setMTU", mtu);

    //create a test plan
    Poco::JSON::Object::Ptr testPlan(new Poco::JSON::Object());
    testPlan->set("enablePackets", true);
    testPlan->set("minValue", 0);
    testPlan->set("maxValue", maxValue);
    testPlan->set("minBufferSize", mtu);
    testPlan->set("maxBufferSize", mtu);
    auto expected = feeder.callProxy("feedTestPlan", testPlan);

    //because of correlation window, pad feeder to flush through last message
    Pothos::Packet paddingPacket;
    paddingPacket.payload = Pothos::BufferChunk("uint8", preamble.size());
    feeder.callVoid("feedPacket", paddingPacket);

    //create tester topology
    {
        Pothos::Topology topology;
        topology.connect(feeder, 0, generator, 0);
        topology.connect(generator, 0, framer, 0);
        topology.connect(framer, 0, copier, 0);
        topology.connect(copier, 0, correlator, 0);
        topology.connect(correlator, 0, deframer, 0);
        topology.connect(deframer, 0, collector, 0);
        topology.commit();
        POTHOS_TEST_TRUE(topology.waitInactive());
        //std::cout << topology.queryJSONStats() << std::endl;
    }

    std::cout << "verifyTestPlan" << std::endl;
    collector.callVoid("verifyTestPlan", expected);
}
