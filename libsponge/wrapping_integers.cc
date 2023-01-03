#include "wrapping_integers.hh"
#include<iostream>
#include <cmath>

using std::cout;
using std::endl;

// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t abs_seq, WrappingInt32 isn) {
    // DUMMY_CODE(n, isn);
//  absolute seqno -> seqno
    // uint32_t seq = (abs_seq + isn.raw_value()) % WrappingInt32::_MOD;
    // return WrappingInt32(seq);
    return isn + static_cast<uint32_t>(abs_seq);    //  abs_seq 从第32位开始皆可直接舍去，因此seq的域是[0,2^32-1]。abs_seq每增加2^32，其对应的seq就相当于转了一圈，回到了seq的起点isn。（abs_seq 0 , 2^32 , k * 2^32 -> seq 0)
}

//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The relative sequence number
//! \param isn The initial sequence number
//! \param checkpoint A recent absolute 64-bit sequence number
//! \returns the 64-bit sequence number that wraps to `n` and is closest to `checkpoint`
//!
//! \note Each of the two streams of the TCP connection has its own ISN. One stream
//! runs from the local TCPSender to the remote TCPReceiver and has one ISN,
//! and the other stream runs from the remote TCPSender to the local TCPReceiver and
//! has a different ISN.
uint64_t unwrap(WrappingInt32 seq, WrappingInt32 isn, uint64_t checkpoint) {
//  seqno -> absolute seq no
//  closet : 没有向上取最接近还是向下取最接近之分。只要求最接近即可。

    uint32_t base_abs = seq - isn;
    // cout<<seq<<" "<<isn<<" "<<checkpoint<<" "<<static_cast<uint32_t>(checkpoint)<<" "<<base_abs<<endl;    
    if( base_abs >= static_cast<uint32_t>(checkpoint))
    {
        uint32_t offset = base_abs - static_cast<uint32_t>(checkpoint);
        if(offset <= (1ul<<31))
        {
            // cout<<"branch1a\t"<<offset<<" "<<checkpoint + offset<<endl;;
            return checkpoint + offset;
        }
        else
        {
            // cout<<"branch1b\t"<<offset<<" "<<WrappingInt32::_MOD - offset<<" "<<checkpoint - (WrappingInt32::_MOD - offset)<<endl;
            //  corner case : 当下溢时 选择右侧 而不是左侧溢出到2^64
            if(checkpoint < (WrappingInt32::_MOD - offset))
                return checkpoint + offset;
            return checkpoint - (WrappingInt32::_MOD - offset);
        }
    } 
    else
    {
        uint32_t offset = static_cast<uint32_t>(checkpoint) - base_abs;
        if(offset > (1ul<<31))
        {
            // cout<<"branch2a\t"<<offset<<" "<<checkpoint + (WrappingInt32::_MOD - offset)<<endl;;
            return checkpoint + (WrappingInt32::_MOD - offset);
        }
        else
        {
            if(checkpoint < offset)
                return checkpoint + (WrappingInt32::_MOD - offset);
            // cout<<"branch2b\t"<<offset<<" "<<checkpoint - offset<<endl;
            return checkpoint - offset;
        }
    }
}

