// Copyright (c) 2016-2016 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Framework.hpp>
#include <chrono>
#include <thread>
#include <iostream>

/***********************************************************************
 * |PothosDoc Reinterpret
 *
 * The reinterpret block changes the data type of an input buffer
 * without modifying its contents. Input buffers and packet
 * messages are forwarded from input port 0 to output port 0.
 * The data type will be changed to match the specified type.
 *
 * |category /Stream
 * |category /Convert
 *
 * |param dtype[Data Type] The output data type.
 * |widget DTypeChooser(float=1,cfloat=1,int=1,cint=1,uint=1,cuint=1,dim=1)
 * |default "complex_float64"
 * |preview disable
 *
 * |factory /blocks/reinterpret(dtype)
 **********************************************************************/
class Reinterpret : public Pothos::Block
{
public:
    static Block *make(const Pothos::DType &dtype)
    {
        return new Reinterpret(dtype);
    }

    Reinterpret(const Pothos::DType &dtype)
    {
        this->setupInput(0);
        this->setupOutput(0, dtype, this->uid()); //unique domain because of buffer forwarding
    }

    void work(void)
    {
        auto inputPort = this->input(0);
        auto outputPort = this->output(0);

        //got a packet message
        if (inputPort->hasMessage())
        {
            auto pkt = inputPort->popMessage().convert<Pothos::Packet>();
            const auto inType = pkt.payload.dtype;
            pkt.payload.dtype = outputPort->dtype();
            for (auto &label : pkt.labels)
            {
                label = label.toAdjusted(inType.size(), //multiply to convert input elements to bytes
                    outputPort->dtype().size()); //divide to convert bytes to output elements
                if (label.width == 0) label.width = 1;
            }
            outputPort->postMessage(pkt);
        }

        //got a stream buffer
        auto buff = inputPort->buffer();
        if (buff.length != 0)
        {
            buff.dtype = outputPort->dtype();
            outputPort->postBuffer(buff);
        }

        inputPort->consume(inputPort->elements());
    }

    void propagateLabels(const Pothos::InputPort *port)
    {
        auto outputPort = this->output(0);
        for (const auto &label : port->labels())
        {
            //retain relative byte offset when converting into output elements
            auto outLabel = label.toAdjusted(1, outputPort->dtype().size());
            if (outLabel.width == 0) outLabel.width = 1;
            outputPort->postLabel(outLabel);
        }
    }
};

static Pothos::BlockRegistry registerReinterpret(
    "/blocks/reinterpret", &Reinterpret::make);
