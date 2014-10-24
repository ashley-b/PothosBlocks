// Copyright (c) 2014-2014 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Framework.hpp>

/***********************************************************************
 * |PothosDoc Stream To Packet
 *
 * The stream to packet block is a simple translator between
 * a byte stream input and a message-based packetized output.
 * The block accepts a byte stream with labels on input port 0,
 * and converts the stream into Pothos::Packet message objects.
 * The packet message objects are then produced on output port 0.
 *
 * If the input port 0 has an incoming message,
 * it will be forwarded directly to output port 0.
 *
 * This is zero-copy block implementation.
 * The output packet object holds a reference to the input stream buffer,
 * without incurring a copy of the buffer.
 *
 * |category /Packet
 * |keywords packet message datagram
 *
 * |param mtu[MTU] The maximum size of the payload in an output packet.
 * An MTU of 0 bytes means unconstrained payload size;
 * packet payloads will accept the entire input buffer.
 * |default 0
 * |units bytes
 *
 * |factory /blocks/stream_to_packet()
 * |setter setMTU(mtu)
 **********************************************************************/
class StreamToPacket : public Pothos::Block
{
public:
    StreamToPacket(void):
        _mtu(0)
    {
        this->setupInput(0);
        this->setupOutput(0);
        this->registerCall(this, POTHOS_FCN_TUPLE(StreamToPacket, setMTU));
        this->registerCall(this, POTHOS_FCN_TUPLE(StreamToPacket, getMTU));
    }

    static Block *make(void)
    {
        return new StreamToPacket();
    }

    void setMTU(const size_t mtu)
    {
        _mtu = mtu;
    }

    size_t getMTU(void) const
    {
        return _mtu;
    }

    void work(void)
    {
        auto inputPort = this->input(0);
        auto outputPort = this->output(0);

        //forward messages
        if (inputPort->hasMessage())
        {
            auto msg = inputPort->popMessage();
            outputPort->postMessage(msg);
        }

        //is there any input buffer available?
        if (inputPort->elements() == 0) return;

        //grab the input buffer
        Pothos::Packet packet;
        packet.payload = inputPort->buffer();
        if (_mtu != 0) packet.payload.length = std::min(_mtu, packet.payload.length);
        inputPort->consume(packet.payload.length);

        //grab the input labels
        while (inputPort->labels().begin() != inputPort->labels().end())
        {
            auto label = *inputPort->labels().begin();
            label.index /= packet.payload.dtype.size(); //bytes to elements
            if (label.index >= packet.payload.elements()) break;
            packet.labels.push_back(label);
            inputPort->removeLabel(*inputPort->labels().begin());
        }

        //produce the packet
        outputPort->postMessage(packet);
    }

private:
    size_t _mtu;
};

static Pothos::BlockRegistry registerStreamToPacket(
    "/blocks/stream_to_packet", &StreamToPacket::make);
