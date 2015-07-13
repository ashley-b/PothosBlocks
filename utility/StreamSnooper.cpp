// Copyright (c) 2014-2015 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Framework.hpp>
#include <chrono>
#include <complex>
#include <iostream>
#include <algorithm> //min/max
#include <cstring> //memcpy

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
 * |param align[Alignment] Synchronous or asynchronous multi-channel consumption pattern.
 * When in synchronous mode, work() consumes the same amount from all channels to preserve alignment.
 * When in asynchronous mode, work() consumes all available input from each channel independently.
 * |default false
 * |option [Disable] false
 * |option [Enable] true
 *
 * |factory /blocks/stream_snooper()
 * |initializer setNumPorts(numPorts)
 * |setter setChunkSize(chunkSize)
 * |setter setTriggerRate(triggerRate)
 * |setter setAlignment(align)
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
        _triggerRate(1.0),
        _alignment(false)
    {
        this->setupInput(0);
        this->setupOutput(0);
        this->registerCall(this, POTHOS_FCN_TUPLE(StreamSnooper, setNumPorts));
        this->registerCall(this, POTHOS_FCN_TUPLE(StreamSnooper, setChunkSize));
        this->registerCall(this, POTHOS_FCN_TUPLE(StreamSnooper, getChunkSize));
        this->registerCall(this, POTHOS_FCN_TUPLE(StreamSnooper, setTriggerRate));
        this->registerCall(this, POTHOS_FCN_TUPLE(StreamSnooper, getTriggerRate));
        this->registerCall(this, POTHOS_FCN_TUPLE(StreamSnooper, setAlignment));
        this->registerCall(this, POTHOS_FCN_TUPLE(StreamSnooper, getAlignment));
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

    void setAlignment(const bool enabled)
    {
        _alignment = enabled;
    }

    bool getAlignment(void) const
    {
        return _alignment;
    }

    void activate(void)
    {
        _lastTriggerTimes.resize(this->inputs().size());
        _accumulationBuffs.resize(this->inputs().size());
    }

    void deactivate(void)
    {
        for (auto &pkt : _accumulationBuffs)
        {
            pkt = Pothos::Packet(); //clear
        }
        for (auto inPort : this->inputs())
        {
            inPort->clear();
        }
    }

    void work(void)
    {
        //Alignment: we need to know the minimum number of elements:
        //These are type agnostic ports, we must check the buffer.
        //The call this->workInfo().minInElements can't be used.
        size_t minElements = (1 << 30);
        if (_alignment) for (auto inPort : this->inputs())
        {
            minElements = std::min(minElements, inPort->buffer().elements());
        }

        for (auto inPort : this->inputs())
        {
            //forward messages
            if (inPort->hasMessage())
            {
                const auto msg = inPort->popMessage();
                this->output(inPort->index())->postMessage(msg);
            }

            //determine how many elements are available based on mode
            const size_t num = _alignment?(minElements*inPort->buffer().dtype.size()):inPort->elements();
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
            //when aligned always use port 0 as a time trigger
            auto &lastTriggerTime = _lastTriggerTimes[_alignment?0:inPort->index()];
            const auto timeBetweenUpdates = std::chrono::nanoseconds((long long)(1e9/_triggerRate));
            bool doUpdate = (std::chrono::high_resolution_clock::now() - lastTriggerTime) > timeBetweenUpdates;

            //perform the accumulation buffer update
            //when aligned, only mark the time when we are on the last port
            if (doUpdate and this->handleTrigger(inPort, num))
            {
                if (not _alignment or inPort == this->inputs().back())
                    lastTriggerTime = std::chrono::high_resolution_clock::now();
            }
        }
    }

    void smartAppend(Pothos::BufferChunk &outBuff, const Pothos::BufferChunk &inBuff)
    {
        //the outgoing packet is empty and the input buffer is sufficient
        //simply forward the entire buffer to the outgoing packet
        if (not outBuff and inBuff.elements() >= _chunkSize)
        {
            outBuff = inBuff;
            return;
        }

        //allocate a new buffer to hold an entire chunk
        if (not outBuff)
        {
            outBuff = Pothos::BufferChunk(inBuff.dtype, _chunkSize);
            outBuff.length = 0;
        }

        //copy in the buffer as long as it can fit (it should unless user changed dtypes)
        if (outBuff.getBuffer().getLength()-outBuff.length >= inBuff.length)
        {
            std::memcpy(outBuff.as<char *>() + outBuff.length, inBuff.as<const void *>(), inBuff.length);
            outBuff.length += inBuff.length;
        }

        //otherwise append (which resizes and memcpys)
        else outBuff.append(inBuff);
    }

    bool handleTrigger(Pothos::InputPort *inPort, const size_t num)
    {
        auto &packet = _accumulationBuffs[inPort->index()];
        const auto initialOffset = packet.payload.elements();

        //truncate to the required remaining length
        auto inBuff = inPort->buffer();
        const auto bytesPerEvent = _chunkSize*inBuff.dtype.size();
        inBuff.length = std::min(num, bytesPerEvent - packet.payload.length);

        //append the buffer into the outgoing packet payload
        this->smartAppend(packet.payload, inBuff);

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

    void propagateLabels(const Pothos::InputPort *)
    {
        //do not propagate labels
    }

private:
    size_t _chunkSize;
    double _triggerRate;
    bool _alignment;
    std::vector<std::chrono::high_resolution_clock::time_point> _lastTriggerTimes;
    std::vector<Pothos::Packet> _accumulationBuffs;
};

static Pothos::BlockRegistry registerStreamSnooper(
    "/blocks/stream_snooper", &StreamSnooper::make);
