// Copyright (c) 2014-2014 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Framework.hpp>
#include <chrono>
#include <complex>
#include <iostream>

/***********************************************************************
 * |PothosDoc Stream Snooper
 *
 * The stream snooper accepts streaming input and forwards
 * chunks of the stream buffer under configurable conditions.
 * Conditions include periodic updates and trigger events.
 *
 * |category /Utility
 *
 * |param numPorts[Num Ports] The number of IO ports.
 * |default 1
 * |widget SpinBox(minimum=1)
 * |preview disable
 *
 * |param chunkSize[Chunk Size] How many elements to yield when triggered?
 * |default 1024
 *
 * |param triggerRate[Trigger Rate] The rate of the time-based trigger.
 * |units events/sec
 * |default 1.0
 *
 * |param alignMode[Alignment] Synchronous or asynchronous multi-channel consumption pattern.
 * When in synchronous mode, work() consumes the same amount from all channels to preserve alignment.
 * When in asynchronous mode, work() consumes all available input from each channel independently.
 * |default "ASYNC"
 * |option [Asynchronous] "ASYNC"
 * |option [Synchronous] "SYNC"
 *
 * |factory /blocks/stream_snooper()
 * |initializer setNumPorts(numPorts)
 * |setter setChunkSize(chunkSize)
 * |setter setTriggerRate(triggerRate)
 * |setter setAlignMode(alignMode)
 **********************************************************************/
class StreamSnooper : public Pothos::Block
{
public:
    static Block *make(void)
    {
        return new StreamSnooper();
    }

    StreamSnooper(void):
        _chunkSize(0),
        _triggerRate(1.0)
    {
        this->setAlignMode("ASYNC");
        this->setupInput(0);
        this->setupOutput(0);
        this->registerCall(this, POTHOS_FCN_TUPLE(StreamSnooper, setNumPorts));
        this->registerCall(this, POTHOS_FCN_TUPLE(StreamSnooper, setChunkSize));
        this->registerCall(this, POTHOS_FCN_TUPLE(StreamSnooper, getChunkSize));
        this->registerCall(this, POTHOS_FCN_TUPLE(StreamSnooper, setTriggerRate));
        this->registerCall(this, POTHOS_FCN_TUPLE(StreamSnooper, getTriggerRate));
        this->registerCall(this, POTHOS_FCN_TUPLE(StreamSnooper, setAlignMode));
        this->registerCall(this, POTHOS_FCN_TUPLE(StreamSnooper, getAlignMode));
    }

    void setNumPorts(const size_t numPorts)
    {
        for (size_t i = this->inputs().size(); i < numPorts; i++) this->setupInput(i);
        for (size_t i = this->outputs().size(); i < numPorts; i++) this->setupOutput(i);
    }

    void setChunkSize(const size_t numElems)
    {
        _chunkSize = numElems;
    }

    size_t getChunkSize(void) const
    {
        return _chunkSize;
    }

    void setTriggerRate(const double rate)
    {
        _triggerRate = rate;
    }

    double getTriggerRate(void) const
    {
        return _triggerRate;
    }

    void setAlignMode(const std::string &mode)
    {
        if (mode == "SYNC"){}
        else if (mode == "ASYNC"){}
        else throw Pothos::InvalidArgumentException("StreamSnooper::setAlignMode("+mode+")", "unknown mode");

        _alignMode = mode;
    }

    std::string getAlignMode(void) const
    {
        return _alignMode;
    }

    void activate(void)
    {
        _lastTriggerTimes.resize(this->inputs().size());
        _accumulationBuffs.resize(this->inputs().size());
    }

    void work(void)
    {
        const bool sync = (_alignMode == "SYNC");

        for (auto inPort : this->inputs())
        {
            //forward messages
            if (inPort->hasMessage())
            {
                const auto msg = inPort->popMessage();
                this->output(inPort->index())->postMessage(msg);
            }

            //determine how many elements are available based on mode
            const size_t num = sync?this->workInfo().minInElements:inPort->elements();
            if (num == 0) continue;

            //always consume all available input
            inPort->consume(num);

            //forward all labels in case they have meaning
            for (const auto &label : inPort->labels())
            {
                if (label.index >= num) break;
                this->output(inPort->index())->postMessage(label);
            }

            //are we triggered by the periodic event?
            auto &lastTriggerTime = _lastTriggerTimes[inPort->index()];
            const auto timeBetweenUpdates = std::chrono::nanoseconds((long long)(1e9/_triggerRate));
            bool doUpdate = (std::chrono::high_resolution_clock::now() - lastTriggerTime) > timeBetweenUpdates;

            //perform the accumulation buffer update
            if (doUpdate and this->handleTrigger(inPort, num))
            {
                lastTriggerTime = std::chrono::high_resolution_clock::now();
            }
        }
    }

    bool handleTrigger(Pothos::InputPort *inPort, const size_t num)
    {
        auto &packet = _accumulationBuffs[inPort->index()];
        const auto initialOffset = packet.payload.elements();

        //append the new buffer
        auto inBuff = inPort->buffer();
        const auto bytesPerEvent = _chunkSize*inBuff.dtype.size();
        inBuff.length = std::min(num, bytesPerEvent - packet.payload.length);
        packet.payload.append(inBuff);

        //append new labels
        for (auto label : inPort->labels())
        {
            label.index /= packet.payload.dtype.size(); //bytes to elements
            label.index += initialOffset;
            if (label.index >= packet.payload.elements()) break;
            packet.labels.push_back(label);
        }

        //enough in the buffer?
        if (packet.payload.elements() < _chunkSize) return false;

        //send the message
        this->output(inPort->index())->postMessage(packet);

        //clear old packet buffer
        packet = Pothos::Packet();
        return true;
    }

private:
    size_t _chunkSize;
    double _triggerRate;
    std::string _alignMode;
    std::vector<std::chrono::high_resolution_clock::time_point> _lastTriggerTimes;
    std::vector<Pothos::Packet> _accumulationBuffs;
};

static Pothos::BlockRegistry registerStreamSnooper(
    "/blocks/stream_snooper", &StreamSnooper::make);
