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
uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
//  seqno -> absolute seq no
//  closet : 没有向上取最接近还是向下取最接近之分。只要求最接近即可。
    // DUMMY_CODE(n, isn, checkpoint);
    WrappingInt32 & seq = n;
    //  absolute seq % _MOD
    uint64_t base_abs = (seq + (WrappingInt32::_MOD - isn.raw_value())).raw_64_value();

    // cout<<seq<<" "<<isn<<" "<<checkpoint<<" "<<base_abs<<endl;

    if(checkpoint % WrappingInt32::_MOD !=0 )
    {
        uint64_t m1 = WrappingInt32::ROUNDUP(checkpoint) - checkpoint + base_abs;
        uint64_t m2 = checkpoint - WrappingInt32::ROUNDDOWN(checkpoint) - base_abs;
        if(checkpoint > (WrappingInt32::ROUNDDOWN(checkpoint) + base_abs))
        {
            // cout<<"\t branch1 b1 "<<checkpoint<<" "<<WrappingInt32::ROUNDDOWN(checkpoint)<<" "<<base_abs<<" "<<(WrappingInt32::ROUNDDOWN(checkpoint) + base_abs)<<endl;
            m2 = checkpoint - WrappingInt32::ROUNDDOWN(checkpoint) - base_abs;
        }
        else
        {
            // cout<<"\t branch1 b2 "<<checkpoint<<" "<<WrappingInt32::ROUNDDOWN(checkpoint)<<" "<<base_abs<<" "<<(WrappingInt32::ROUNDDOWN(checkpoint) + base_abs)<<endl;
            m2 = base_abs + WrappingInt32::ROUNDDOWN(checkpoint) - checkpoint;
        }

        uint64_t m3 = checkpoint - WrappingInt32::ROUNDDOWN(checkpoint) + WrappingInt32::_MOD - base_abs;

        // cout<<"\t branch1 "<<WrappingInt32::ROUNDDOWN(checkpoint)<<" "<<checkpoint<<" "<<WrappingInt32::ROUNDUP(checkpoint)<<endl;
        // cout<<"\t branch1 "<<m1<<" "<<m2<<endl;
        // cout<<"\t branch1 "<<WrappingInt32::ROUNDUP(checkpoint) + base_abs<<" "<<WrappingInt32::ROUNDDOWN(checkpoint) + base_abs<<endl;

        uint64_t r = min(min(m1,m2),m3);
        if(r == m1)
        {
            return WrappingInt32::ROUNDUP(checkpoint) + base_abs;
        }
        else if(r == m2)
        {
            return WrappingInt32::ROUNDDOWN(checkpoint) + base_abs;
        }
        else if(r == m3)
        {
            return WrappingInt32::ROUNDDOWN(checkpoint) - WrappingInt32::_MOD + base_abs;
        }
        else
        {
            cout<<"ASDSADASDADS"<<endl;
            return -1;
        }
        // return m1 <= m2 ? WrappingInt32::ROUNDUP(checkpoint) + base_abs : WrappingInt32::ROUNDDOWN(checkpoint) + base_abs;
    }
    else
    {
        uint64_t m1 = base_abs;
        if(checkpoint == 0)             //  不能理解。corner case。居然不是循环回UINT64_MAX
            return m1;
        uint64_t m2 = WrappingInt32::_MOD - base_abs;
        // cout<<"\t branch2 "<<m1<<" "<<m2<<endl;
        // cout<<"\t branch2 "<<checkpoint + base_abs<<" "<<checkpoint + base_abs - WrappingInt32::_MOD<<endl;
        return m1 <= m2 ? checkpoint + base_abs : checkpoint + base_abs - WrappingInt32::_MOD ;

    }
    // uint64_t res1 = checkpoint + base_abs;
    // if(checkpoint >= WrappingInt32::_MOD)
    //     uint64_t res2 = checkpoint - WrappingInt32::_MOD + base_abs;    
    return 0;

    // WrappingInt32 wrap_checkpoint = wrap(checkpoint, isn);
    // int32_t diff = n - wrap_checkpoint;
    // int64_t res = checkpoint + diff;
    // if (res < 0) {
    //     return res + (1ul << 32);
    // }
    // return res;
}
