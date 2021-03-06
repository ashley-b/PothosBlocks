// Copyright (c) 2014-2017 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Framework.hpp>
#include <Poco/Format.h>
#include <Poco/Types.h>
#include <cstring> //memcpy
#include <vector>
#include <algorithm> //min/max
#include <json.hpp>

using json = nlohmann::json;

class CollectorSink : Pothos::Block
{
public:
    CollectorSink(const Pothos::DType &dtype)
    {
        this->setupInput(0, dtype);
        this->registerCall(this, POTHOS_FCN_TUPLE(CollectorSink, getBuffer));
        this->registerCall(this, POTHOS_FCN_TUPLE(CollectorSink, getLabels));
        this->registerCall(this, POTHOS_FCN_TUPLE(CollectorSink, getMessages));
        this->registerCall(this, POTHOS_FCN_TUPLE(CollectorSink, getPackets));
        this->registerCall(this, POTHOS_FCN_TUPLE(CollectorSink, verifyTestPlan));
        this->registerCall(this, POTHOS_FCN_TUPLE(CollectorSink, clear));
    }

    static Block *make(const Pothos::DType &dtype)
    {
        return new CollectorSink(dtype);
    }

    Pothos::BufferChunk getBuffer(void) const
    {
        return _buffer;
    }

    std::vector<Pothos::Label> getLabels(void) const
    {
        return _labels;
    }

    std::vector<Pothos::Object> getMessages(void) const
    {
        return _messages;
    }

    std::vector<Pothos::Packet> getPackets(void) const
    {
        return _packets;
    }

    void verifyTestPlan(const std::string &expected);
    static void verifyTestPlanExpectedValues(const json &expected, const Pothos::BufferChunk &buffer, const Pothos::DType &expectedDType);
    static void verifyTestPlanExpectedLabels(const json &expected, const std::vector<Pothos::Label> &labels);
    static void verifyTestPlanExpectedMessages(const json &expected, const std::vector<Pothos::Object> &messages);
    static void verifyTestPlanExpectedPackets(const json &expected, const std::vector<Pothos::Packet> &packets, const Pothos::DType &expectedDType);

    void work(void)
    {
        auto inputPort = this->input(0);

        //accumulate the buffer into a bigger buffer
        const auto &buffer = inputPort->buffer();
        if (buffer.length != 0) _buffer.append(buffer);

        //consume buffer
        inputPort->consume(inputPort->elements());

        //store labels -- take all labels since we consume all the buffer
        while (inputPort->labels().begin() != inputPort->labels().end())
        {
            auto label = *inputPort->labels().begin();
            inputPort->removeLabel(label);
            label.index += inputPort->totalElements(); //rel -> abs
            _labels.push_back(std::move(label));
        }

        //store messages
        while (inputPort->hasMessage())
        {
            auto msg = inputPort->popMessage();
            if (msg.type() == typeid(Pothos::Packet))
            {
                auto pkt = msg.extract<Pothos::Packet>();
                const auto oldBuff = pkt.payload;
                //copy the payload so we don't hold upstream resources
                pkt.payload = Pothos::BufferChunk(oldBuff.dtype, oldBuff.elements());
                std::memcpy(pkt.payload.as<void *>(), oldBuff.as<const void *>(), oldBuff.length);
                _packets.push_back(std::move(pkt));
            }
            else _messages.push_back(std::move(msg));
        }
    }

    void clear(void)
    {
        _buffer = Pothos::BufferChunk();
        _labels.clear();
        _messages.clear();
        _packets.clear();
    }

private:
    Pothos::BufferChunk _buffer;
    std::vector<Pothos::Label> _labels;
    std::vector<Pothos::Object> _messages;
    std::vector<Pothos::Packet> _packets;
};

static Pothos::BlockRegistry registerCollectorSink(
    "/blocks/collector_sink", &CollectorSink::make);

/***********************************************************************
 * Verify test plan -- top level call
 **********************************************************************/
void CollectorSink::verifyTestPlan(const std::string &expectedStr)
{
    const auto expected = json::parse(expectedStr);
    bool checked = false;

    if (expected.count("expectedValues"))
    {
        verifyTestPlanExpectedValues(expected, _buffer, this->input(0)->dtype());
        checked = true;
    }

    if (expected.count("expectedLabels"))
    {
        verifyTestPlanExpectedLabels(expected, _labels);
        checked = true;
    }

    if (expected.count("expectedMessages"))
    {
        verifyTestPlanExpectedMessages(expected, _messages);
        checked = true;
    }

    if (expected.count("expectedPackets"))
    {
        verifyTestPlanExpectedPackets(expected, _packets, this->input(0)->dtype());
        checked = true;
    }

    if (not checked) throw Pothos::AssertionViolationException("CollectorSink::verifyTestPlan()", "nothing checked!");

    //clear for re-use
    this->clear();
}

/***********************************************************************
 * Verify test plan helper -- check buffer
 **********************************************************************/
void CollectorSink::verifyTestPlanExpectedValues(const json &expected, const Pothos::BufferChunk &buffer, const Pothos::DType &expectedDType)
{
    if (buffer.length == 0) throw Pothos::AssertionViolationException("CollectorSink::verifyTestPlan()", "buffer is empty");

    if (not (buffer.dtype == expectedDType)) throw Pothos::AssertionViolationException("CollectorSink::verifyTestPlan()",
        Poco::format("Buffer type mismatch: expected %s -> actual %s", expectedDType.toString(), buffer.dtype.toString()));

    const auto expectedValues = expected.value("expectedValues", json::array());
    auto intBuffer = buffer.convert(typeid(int));
    auto numActualElems = intBuffer.elements();

    for (size_t i = 0; i < std::min(numActualElems, expectedValues.size()); i++)
    {
        const int value = expectedValues[i];
        const auto actual = intBuffer.as<const int *>()[i];
        if (value != actual) throw Pothos::AssertionViolationException("CollectorSink::verifyTestPlan()",
            Poco::format("Value check for element %z: expected %d -> actual %d", i, value, actual));
    }

    if (numActualElems != expectedValues.size()) throw Pothos::AssertionViolationException("CollectorSink::verifyTestPlan()",
        Poco::format("Check expected %z elements, actual %z elements", expectedValues.size(), numActualElems));
}

/***********************************************************************
 * Verify test plan helper -- check labels
 **********************************************************************/
void CollectorSink::verifyTestPlanExpectedLabels(const json &expected, const std::vector<Pothos::Label> &labels)
{
    const auto expectedLabels = expected.value("expectedLabels", json::array());
    for (size_t i = 0; i < std::min(labels.size(), expectedLabels.size()); i++)
    {
        const auto &lbl = labels[i];
        const auto &expectedLabel = expectedLabels[i];
        const std::string value = expectedLabel["data"];
        const size_t index = expectedLabel["index"];
        const std::string id = expectedLabel["id"];
        if (lbl.data.type() != typeid(std::string)) throw Pothos::AssertionViolationException("CollectorSink::verifyTestPlan()",
            "cant handle this label type: " + lbl.data.getTypeString());
        auto actual = lbl.data.extract<std::string>();
        if (lbl.width != 1) throw Pothos::AssertionViolationException("CollectorSink::verifyTestPlan()",
            Poco::format("Value check for label width %z: expected %d -> actual %z", i, int(1), lbl.width));
        if (lbl.index != index) throw Pothos::AssertionViolationException("CollectorSink::verifyTestPlan()",
            Poco::format("Value check for label index %z: expected %z -> actual %z", i, index, size_t(lbl.index)));
        if (lbl.id != id) throw Pothos::AssertionViolationException("CollectorSink::verifyTestPlan()",
            Poco::format("Value check for label id %z: expected '%s' -> actual '%s'", i, id, lbl.id));
        if (actual != value) throw Pothos::AssertionViolationException("CollectorSink::verifyTestPlan()",
            Poco::format("Value check for label data %z: expected '%s' -> actual '%s'", i, value, actual));
    }

    if (labels.size() != expectedLabels.size()) throw Pothos::AssertionViolationException("CollectorSink::verifyTestPlan()",
        Poco::format("Check expected %z labels, actual %z labels", expectedLabels.size(), labels.size()));
}

/***********************************************************************
 * Verify test plan helper -- check messages
 **********************************************************************/
void CollectorSink::verifyTestPlanExpectedMessages(const json &expected, const std::vector<Pothos::Object> &messages)
{
    const auto expectedMessages = expected.value("expectedMessages", json::array());
    for (size_t i = 0; i < std::min(messages.size(), expectedMessages.size()); i++)
    {
        const auto &msg = messages[i];
        const std::string value = expectedMessages[i];
        if (msg.type() != typeid(std::string)) throw Pothos::AssertionViolationException("CollectorSink::verifyTestPlan()",
            "cant handle this message type: " + msg.getTypeString());
        auto actual = msg.extract<std::string>();
        if (actual != value) throw Pothos::AssertionViolationException("CollectorSink::verifyTestPlan()",
            Poco::format("Value check for message %z: expected %s -> actual %s", i, value, actual));
    }

    if (messages.size() != expectedMessages.size()) throw Pothos::AssertionViolationException("CollectorSink::verifyTestPlan()",
        Poco::format("Check expected %z messages, actual %z messages", expectedMessages.size(), messages.size()));
}


/***********************************************************************
 * Verify test plan helper -- check packets
 **********************************************************************/
void CollectorSink::verifyTestPlanExpectedPackets(const json &expected, const std::vector<Pothos::Packet> &packets, const Pothos::DType &expectedDType)
{
    const auto expectedPackets = expected.value("expectedPackets", json::array());
    for (size_t i = 0; i < std::min(packets.size(), expectedPackets.size()); i++)
    {
        const auto &pkt = packets[i];
        const auto &expectedPacket = expectedPackets[i];
        try
        {
            verifyTestPlanExpectedValues(expectedPacket, pkt.payload, expectedDType);
            if (expectedPacket.count("expectedLabels"))
                verifyTestPlanExpectedLabels(expectedPacket, pkt.labels);
        }
        catch (const Pothos::Exception &ex)
        {
            throw Pothos::AssertionViolationException("CollectorSink::verifyTestPlan()",
                Poco::format("packet%z -- %s", i, ex.message()));
        }
    }

    if (packets.size() != expectedPackets.size()) throw Pothos::AssertionViolationException("CollectorSink::verifyTestPlan()",
        Poco::format("Check expected %z packets, actual %z packets", expectedPackets.size(), packets.size()));
}
