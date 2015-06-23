// Copyright (c) 2015-2015 Rinat Zakirov
// Copyright (c) 2015-2015 Josh Blum
// SPDX-License-Identifier: BSL-1.0

#include "SymbolHelpers.hpp"
#include <Pothos/Framework.hpp>
#include <algorithm> //min/max

/***********************************************************************
 * |PothosDoc Symbols To Bits
 *
 * Unpack a stream of symbols from input port 0 to a stream of bits on output port 0.
 * Each input byte represents a symbol of bit width specified by modulus.
 * Each output byte represents a bit and can take the values of 0 and 1.
 *
 * This block can be used to convert between bytes and bits when symbol size is 8.
 *
 * |category /Digital
 * |category /Symbol
 *
 * |param N[Modulus] The number of bits per symbol.
 * |default 2
 * |widget SpinBox(minimum=1, maximum=8)
 *
 * |param bitOrder[Bit Order] The bit ordering: MSBit or LSBit.
 * For MSBit, the high bit of the input symbol becomes output 0.
 * For LSBit, the low bit of the input symbol becomes output 0.
 * |option [MSBit] "MSBit"
 * |option [LSBit] "LSBit"
 * |default "MSBit"
 *
 * |factory /blocks/symbols_to_bits()
 * |setter setModulus(N)
 * |setter setBitOrder(bitOrder)
 **********************************************************************/
class SymbolsToBits : public Pothos::Block
{
public:

    static Block *make(void)
    {
        return new SymbolsToBits();
    }

    SymbolsToBits(void) : _order(BitOrder::MSBit), _mod(1)
    {
        this->setupInput(0, typeid(unsigned char));
        this->setupOutput(0, typeid(unsigned char));
        this->registerCall(this, POTHOS_FCN_TUPLE(SymbolsToBits, getModulus));
        this->registerCall(this, POTHOS_FCN_TUPLE(SymbolsToBits, setModulus));
        this->registerCall(this, POTHOS_FCN_TUPLE(SymbolsToBits, setBitOrder));
        this->registerCall(this, POTHOS_FCN_TUPLE(SymbolsToBits, getBitOrder));
    }

    unsigned char getModulus(void) const
    {
        return _mod;
    }

    void setModulus(const unsigned char mod)
    {
        if (mod < 1 or mod > 8)
        {
            throw Pothos::InvalidArgumentException("SymbolsToBits::setModulus()", "Modulus must be between 1 and 8 inclusive");
        }
        _mod = mod;
    }

    std::string getBitOrder(void) const
    {
        return (_order == BitOrder::LSBit)? "LSBit" : "MSBit";
    }

    void setBitOrder(std::string order)
    {
        if (order == "LSBit") _order = BitOrder::LSBit;
        else if (order == "MSBit") _order = BitOrder::MSBit;
        else throw Pothos::InvalidArgumentException("SymbolsToBits::setBitOrder()", "Order must be LSBit or MSBit");
    }

    void symbolsToBits(const unsigned char *in, unsigned char *out, const size_t len)
    {
        unsigned char sampleBit = 0x1;
        if (_order == BitOrder::MSBit) sampleBit = 1 << (_mod - 1);
        for (size_t i = 0; i < len; i++)
        {
            unsigned char symbol = in[i];
            for (size_t b = 0; b < _mod; b++)
            {
                *out++ = ((sampleBit & symbol) != 0) ? 1 : 0;

                if(_order == BitOrder::MSBit)
                    symbol = symbol << 1;
                else
                    symbol = symbol >> 1;
            }
        }
    }

    void work(void)
    {
        auto inputPort = this->input(0);
        auto outputPort = this->output(0);

        //setup buffers
        auto inBuff = inputPort->buffer();
        auto outBuff = outputPort->buffer();
        const size_t symLen = std::min(inBuff.elements(), outBuff.elements() / _mod);
        if (symLen == 0) return;

        //perform conversion
        this->symbolsToBits(
            inBuff.as<const unsigned char*>(),
            outBuff.as<unsigned char*>(),
            symLen
        );

        //produce/consume
        inputPort->consume(symLen);
        outputPort->produce(symLen * _mod);
    }

    void propagateLabels(const Pothos::InputPort *port)
    {
        auto outputPort = this->output(0);
        for (const auto &label : port->labels())
        {
            outputPort->postLabel(label.toAdjusted(_mod, 1));
        }
    }

protected:
    BitOrder _order;
    unsigned char _mod;
};

static Pothos::BlockRegistry registerSymbolsToBits(
    "/blocks/symbols_to_bits", &SymbolsToBits::make);
