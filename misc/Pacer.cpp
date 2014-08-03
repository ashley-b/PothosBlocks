// Copyright (c) 2014-2014 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include <Pothos/Framework.hpp>
#include <Poco/Timestamp.h>
#include <Poco/Thread.h>
#include <iostream>

/***********************************************************************
 * |PothosDoc Pacer
 *
 * The forwarder block passivly forwards all data from
 * input port 0 to the output port 0 without copying.
 * The data rate will be limited to the rate setting.
 * This rate limitation is an approximation at best.
 * This block is mainly used for simulation purposes.
 *
 * |category /Misc
 * |keywords pacer time
 *
 * |param dtype[Data Type] The datatype this block consumes.
 * |preview disable
 * |default "float32"
 *
 * |param rate[Data Rate] The rate of elements or messages through the block.
 * |default 1e3
 *
 * |factory /blocks/pacer(dtype)
 * |setter setRate(rate)
 **********************************************************************/
class Pacer : public Pothos::Block
{
public:
    static Block *make(const Pothos::DType &dtype)
    {
        return new Pacer(dtype);
    }

    Pacer(const Pothos::DType &dtype):
        _rate(1.0),
        _actualRate(1.0),
        _startCount(0)
    {
        this->setupInput(0, dtype);
        this->setupOutput(0, dtype);
        this->registerCall(POTHOS_FCN_TUPLE(Pacer, setRate));
        this->registerCall(POTHOS_FCN_TUPLE(Pacer, getRate));
        this->registerCall(POTHOS_FCN_TUPLE(Pacer, getActualRate));
    }

    void setRate(const double rate)
    {
        _rate = rate;
        auto in0 = this->input(0);
        _startTime = Poco::Timestamp();
        _startCount = in0->totalElements() + in0->totalMessages();
    }

    double getRate(void) const
    {
        return _rate;
    }

    double getActualRate(void) const
    {
        return _actualRate;
    }

    void activate(void)
    {
        //reload rate to make a new start point
        this->setRate(this->getRate());
    }

    void work(void)
    {
        auto inputPort = this->input(0);
        auto outputPort = this->output(0);

        //calculate time passed since activate
        auto currentTime = Poco::Timestamp();
        auto currentCount = inputPort->totalElements() + inputPort->totalMessages();
        const auto expectedTime = ((currentCount - _startCount)/_rate)*1e3; //convert s to ms
        const auto actualTime = (currentTime - _startTime)/1e3; //convert us to ms
        _actualRate = double((currentCount - _startCount))/(actualTime/1e3);

        //sleep to approximate the requested rate (sleep takes ms)
        if (actualTime < expectedTime)
        {
            auto maxTimeSleepMs = this->workInfo().maxTimeoutNs/1e6;
            Poco::Thread::sleep(std::min(maxTimeSleepMs, expectedTime-actualTime));
            return this->yield();
        }

        if (inputPort->hasMessage())
        {
            auto m = inputPort->popMessage();
            outputPort->postMessage(m);
        }

        const auto &buffer = inputPort->buffer();
        if (buffer.length != 0)
        {
            outputPort->postBuffer(buffer);
            inputPort->consume(inputPort->elements());
        }
    }

private:
    double _rate;
    double _actualRate;
    Poco::Timestamp _startTime;
    unsigned long long _startCount;
};

static Pothos::BlockRegistry registerPacer(
    "/blocks/pacer", &Pacer::make);
