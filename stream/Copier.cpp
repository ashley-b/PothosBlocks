// Copyright (c) 2014-2018 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Framework.hpp>
#include <cstring> //memcpy
#include <algorithm> //min/max

/***********************************************************************
 * |PothosDoc Copier
 *
 * The copier block copies all data from input port 0 to the output port 0.
 * This block is used to bridge connections between incompatible domains.
 *
 * |category /Stream
 * |category /Convert
 * |keywords copier copy memcpy
 *
 * |factory /blocks/copier()
 **********************************************************************/
class Copier : public Pothos::Block
{
public:
    static Block *make(void)
    {
        return new Copier();
    }

    Copier(void)
    {
        this->setupInput(0);
        this->setupOutput(0);
    }

    void work(void)
    {
        auto inputPort = this->input(0);
        auto outputPort = this->output(0);

        if (inputPort->hasMessage())
        {
            auto m = inputPort->popMessage();
            if (m.type() == typeid(Pothos::Packet))
            {
                auto pkt = m.extract<Pothos::Packet>();
                auto outBuff = outputPort->getBuffer(pkt.payload.length);
                outBuff.dtype = pkt.payload.dtype;
                std::memcpy(outBuff.as<void *>(), pkt.payload.as<const void *>(), outBuff.length);
                pkt.payload = std::move(outBuff);
                outputPort->postMessage(std::move(pkt));
            }
            else outputPort->postMessage(std::move(m));
        }

        //get input buffer
        auto inBuff = inputPort->buffer();
        if (inBuff.length == 0) return;

        //setup output buffer
        auto outBuff = outputPort->buffer();
        outBuff.dtype = inBuff.dtype;
        outBuff.length = std::min(inBuff.elements(), outBuff.elements())*outBuff.dtype.size();

        //copy input to output
        std::memcpy(outBuff.as<void *>(), inBuff.as<const void *>(), outBuff.length);

        //produce/consume
        inputPort->consume(outBuff.length);
        outputPort->popElements(outBuff.length);
        outputPort->postBuffer(outBuff);
    }
};

static Pothos::BlockRegistry registerCopier(
    "/blocks/copier", &Copier::make);
