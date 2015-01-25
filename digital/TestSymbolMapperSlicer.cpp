// Copyright (c) 2014-2014 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Testing.hpp>
#include <Pothos/Framework.hpp>
#include <Pothos/Proxy.hpp>
#include <iostream>

POTHOS_TEST_BLOCK("/blocks/tests", test_symbol_mapper_slicer_float)
{
    auto registry = Pothos::ProxyEnvironment::make("managed")->findProxy("Pothos/BlockRegistry");

    auto feeder0 = registry.callProxy("/blocks/feeder_source", "unsigned char");
    auto mapper = registry.callProxy("/blocks/symbol_mapper", "float");
    auto slicer = registry.callProxy("/blocks/symbol_slicer", "float");
    auto collector = registry.callProxy("/blocks/collector_sink", "unsigned char");

    std::vector<float> map{-3, -1, 1, 3};
    mapper.callProxy("setMap", map);
    slicer.callProxy("setMap", map);

    //load feeder blocks
    auto b0 = Pothos::BufferChunk(10*sizeof(unsigned char));
    auto p0 = b0.as<unsigned char *>();
    for (size_t i = 0; i < 10; i++) p0[i] = i&3;
    feeder0.callProxy("feedBuffer", b0);

    //run the topology
    {
        Pothos::Topology topology;
        topology.connect(feeder0, 0, mapper, 0);
        topology.connect(mapper, 0, slicer, 0);
        topology.connect(slicer, 0, collector, 0);
        topology.commit();
        POTHOS_TEST_TRUE(topology.waitInactive());
    }

    //check the collector
    auto buff = collector.call<Pothos::BufferChunk>("getBuffer");
    POTHOS_TEST_EQUAL(buff.length, 10*sizeof(unsigned char));
    auto pb = buff.as<const unsigned char *>();
//    for (int i = 0; i < 10; i++) std::cout << i << " " << int(pb[i]) << std::endl;
    for (int i = 0; i < 10; i++) POTHOS_TEST_EQUAL(pb[i], p0[i]);
}
