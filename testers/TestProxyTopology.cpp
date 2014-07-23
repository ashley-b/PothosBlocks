// Copyright (c) 2014-2014 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Testing.hpp>
#include <Pothos/Framework.hpp>
#include <Pothos/Proxy.hpp>
#include <Pothos/Remote.hpp>
#include <Poco/JSON/Object.h>
#include <iostream>

POTHOS_TEST_BLOCK("/blocks/tests", test_proxy_topology)
{
    auto env = Pothos::ProxyEnvironment::make("managed");
    auto registry = env->findProxy("Pothos/BlockRegistry");
    auto feeder = registry.callProxy("/blocks/feeder_source", "int");
    auto collector = registry.callProxy("/blocks/collector_sink", "int");

    //create a test plan
    Poco::JSON::Object::Ptr testPlan(new Poco::JSON::Object());
    testPlan->set("enableBuffers", true);
    testPlan->set("enableLabels", true);
    testPlan->set("enableMessages", true);
    auto expected = feeder.callProxy("feedTestPlan", testPlan);

    //run the topology
    std::cout << "run the topology\n";
    {
        auto topology = env->findProxy("Pothos/Topology").callProxy("new");
        topology.callVoid("connect", feeder, "0", collector, "0");
        topology.callVoid("commit");
        POTHOS_TEST_TRUE(topology.call<bool>("waitInactive"));
    }

    std::cout << "verifyTestPlan!\n";
    collector.callVoid("verifyTestPlan", expected);

    std::cout << "done!\n";
}

//create subtopology as per https://github.com/pothosware/pothos-library/issues/44
static Pothos::Topology* makeForwardingTopology(void)
{
    auto env = Pothos::ProxyEnvironment::make("managed");
    auto registry = env->findProxy("Pothos/BlockRegistry");
    auto forwarder = registry.callProxy("/blocks/forwarder");
    auto t = new Pothos::Topology();
    t->connect(t, "0", forwarder, "0");
    t->connect(forwarder, "0", t, "0");
    return t;
}

static Pothos::BlockRegistry registerAdd(
    "/blocks/tests/forwarder_topology", &makeForwardingTopology);

POTHOS_TEST_BLOCK("/blocks/tests", test_proxy_subtopology)
{
    //spawn a server and client
    std::cout << "create proxy server\n";
    Pothos::RemoteServer server("tcp://0.0.0.0");
    Pothos::RemoteClient client("tcp://localhost:"+server.getActualPort());
    auto env = Pothos::ProxyEnvironment::make("managed");
    auto envRemote = client.makeEnvironment("managed");

    auto registry = env->findProxy("Pothos/BlockRegistry");
    auto registryRemote = envRemote->findProxy("Pothos/BlockRegistry");

    auto feeder = registry.callProxy("/blocks/feeder_source", "int");
    auto collector = registry.callProxy("/blocks/collector_sink", "int");
    std::cout << "make the remote subtopology\n";
    auto forwarder = registryRemote.callProxy("/blocks/tests/forwarder_topology");

    //create a test plan
    Poco::JSON::Object::Ptr testPlan(new Poco::JSON::Object());
    testPlan->set("enableBuffers", true);
    testPlan->set("enableLabels", true);
    testPlan->set("enableMessages", true);
    auto expected = feeder.callProxy("feedTestPlan", testPlan);

    //run the topology
    std::cout << "run the topology\n";
    {
        auto topology = Pothos::Topology();
        topology.connect(feeder, "0", forwarder, "0");
        topology.connect(forwarder, "0", collector, "0");
        topology.commit();
        POTHOS_TEST_TRUE(topology.waitInactive());
    }

    std::cout << "verifyTestPlan!\n";
    collector.callVoid("verifyTestPlan", expected);

    std::cout << "done!\n";
}
