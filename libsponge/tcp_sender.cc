#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>


void Timer::start(uint64_t initial_alarm)
{
    _active = true;
    _initial_alarm = initial_alarm;
    _alarm = _initial_alarm;
}

//  Timer eplapse到期之后(return true) user一定要接着调用reset
bool Timer::elapse(uint64_t elapsed)
{
    if(!_active)
        throw "unstart"; 
    if(_alarm > elapsed)
    {
        _alarm -= elapsed;
        return false;
    }
    else    
    {
        _alarm = 0;
        return true;
    }
}



void Timer::reset()
{
    _active = false;
    _alarm = 0;
    _initial_alarm = 0;
}



// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _send_window{}
    , _receive_window_size(1)       //  >??
    , _initial_retransmission_timeout{retx_timeout}
    , _timer{}
    , _stream(capacity) 
    , _consecutive_retransmissions_cnt(0)
    , _aborted(false)
{
    cout<<capacity<<" "<<_initial_retransmission_timeout<<" "<<_isn<<endl;
    _next_seqno = 0;    //  start abs_seq = 0
}

uint64_t TCPSender::bytes_in_flight() const { 
    // return _send_window_size;
    // return 0
    uint64_t send_window_size = 0;
    for(const auto & seg : _send_window)
    {
        send_window_size += seg.length_in_sequence_space();
    }
    return send_window_size;
}


static void Assert(bool msg)
{
    if(!msg)
    {
        exit(-1);
    }
}


void TCPSender::fill_window() //  try to send segment to fill the receive window
{

    Assert(1);
    cout<<"============fill window start=========="<<endl;
    if(_aborted)
    {
        cout<<"this tcp connection is aborted"<<endl;
        return ;
    }
    
    //  1. 发送超时重传 数据  ? 难道不是在fill_window中实现吗？果然不是。似乎是在tick中实现。

    size_t remaining_recv_window_sz = _receive_window_size == 0 ? 1 : _receive_window_size;
    if(bytes_in_flight() > remaining_recv_window_sz)
    {
        cout<<"send byte more than 1 bytes when the receive window is null"<<endl;
        cout<<"never send more before recive window is not null"<<endl;
        return ;
    }

    //  !!! sender目前已知的 receive_window_size 减去 之前发送的 bytes。
        //  感觉目的是为了让sender和receiver看到的receive window size大小相同 ,还不确定
    remaining_recv_window_sz -= bytes_in_flight();

    if(_receive_window_size == 0)
    {
        cout<<"_receive_window_size = 0"<<endl;
    }
    
    cout<<"remaing_recv_window_sz = "<<remaining_recv_window_sz<<endl;
    //  什么时候发送结束 : remaining_recv_window_sz == 0 or 没有报文可以发送(payload empty and flag is empty)
    while(remaining_recv_window_sz > 0)
    {
        cout<<"looping "<<remaining_recv_window_sz<<endl;
        //  2. 发送还未发送的新数据
        cout<<"_next_abs_seqno "<<_next_seqno<<endl;

        //  build tcpsegment
        TCPSegment seg;
            seg.header().seqno = next_seqno();
            //  syn
            if(state() == State::CLOSED && remaining_recv_window_sz >= 1)
            {
                seg.header().syn = true;
            }

            //  payload
            cout<<TCPConfig::MAX_PAYLOAD_SIZE <<" "<< remaining_recv_window_sz - seg.header().syn<< " " <<_stream.buffer_size()<<endl;
            
            size_t payload_sz = min({TCPConfig::MAX_PAYLOAD_SIZE,remaining_recv_window_sz - seg.header().syn,_stream.buffer_size()});
            seg.payload() = _stream.read(payload_sz);       //  bytestream中读取出来的是tcp payload。至于tcp header 是由sender自己填写。
            
            cout<<"payload_sz"<<" "<<payload_sz<<" seg.payload() "<<seg.payload().copy()<<endl;
            cout<<"stream_eof "<<_stream.eof()<<endl;
            //  fin
            //  我目前认为 : fin 并不占据 stream idx 故 可以 >=  _receive_window_size >= payload_sz
            //  目前认为 _receive_window_size >= payload_sz 不加也可以吧。反正receiver自己会判断。
            //  最新目前认为：还是要判断_receive_window_size >= payload_sz接收方能否接收fin的，因为要维护_next_seqno变量
            //  _next_seqno 假定 sender所有发送的字节都被receiver成功接收?
            //  最最新目前认为：sender看receive_windows中 其中是含有syn和fin的。也即syn和fin会占据receive_window的字节位置.
                //  疑问：receiver在实现的时候 syn和fin并没有占据receive_window的字节位置。
                //  为什么sender要认为占据了。
                //  先按照占据了来做吧。等做完再看看。
            
            if(state() == SYN_ACKED_2 && remaining_recv_window_sz > payload_sz + seg.header().syn)
            {
                seg.header().fin = true;
            }

        //  如果这个segment 既没有 flag 如 syn fin；又没有 payload 则 不必发送该seg
        if(seg.length_in_sequence_space() == 0)
        {
            cout<<"nothing to send"<<endl;
            break;
        }
        //  send seg
        _segments_out.push(seg);
        //  update send_window
        _send_window.push_back(seg);
        //  如果这是 send_window empty之后 第一次发送数据（装入数据到_send_window）。
        if(!_timer.active())
        {
            cout<<"start a timer"<<endl;
            _timer.reset();
            _timer.start(_initial_retransmission_timeout);
        }

        //  update _next_seq
        _next_seqno += seg.length_in_sequence_space();
        //  update _send_window_size
        // _send_window_size += seg.length_in_sequence_space();
        //  update receive_window_size
        cout<<seg.length_in_sequence_space()<<" "<<remaining_recv_window_sz<<" "<<remaining_recv_window_sz-seg.length_in_sequence_space()<<endl;
        remaining_recv_window_sz -= seg.length_in_sequence_space();

    }

    cout<<"============fill window end=========="<<endl;
}

//  receiver 返回给 sender window_size()
//  所以从sender中获取的window size ，是没有包括syn和fin的window size
//  window_size是由stream_idx计算出的


/**
 * 读取好后，如果满足以下条件，则增加 FIN
 *  1. 从来没发送过 FIN
 *  2. 输入字节流处于 EOF
 *  3. window 减去 payload 大小后，仍然可以存放下 FIN
*/

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) { 
    
    _receive_window_size = window_size;

    uint64_t abs_ackno = unwrap(ackno,_isn,_next_seqno);

    cout<<"============ack_received start=========== "<<abs_ackno<<" "<<_receive_window_size<<endl;

    if(_aborted)
    {
        cout<<"this tcp connection is aborted"<<endl;
        return ;
    }
    
    if(abs_ackno > _next_seqno)     // ack: receiver 期待 sender发送的下一个字节索引 ，_next_seqno : sender顺序将要发送的下一个字节索引
    {
        cout<<"never happened ! the byte after _next_seqno hasn't been sent !"<<endl;
        return ;
    }
    //  如果接收到了已确认数据的ack，那么该怎么办？我这里采用的是忽略该ack
        //  即如果send_window empty的话，则ack无用。确认谁啊。没谁能确认了。
    if(_send_window.empty())
    {
        cout<<"send window is empty"<<endl;
        return ;
    }

    bool seg_acked = false;     //  send_window中的seg被确认了。(可以顺带排除ack < left edge of the send_window)
    //  remove acked seg from the send_window  
    for(deque<TCPSegment>::iterator iter = _send_window.begin();iter!=_send_window.end();)
    {
        uint64_t abs_idx = unwrap(iter->header().seqno,_isn,_next_seqno);
        uint64_t len = iter->length_in_sequence_space();
        if(abs_idx + len <=  abs_ackno)     //  如果对于abs_idx < abs_ackno , abs_idx + len > abs_ackno的情况呢 ? 该如何处理 ?
        {
            cout<<"\t remove it from sender window "<<abs_idx<<" "<<len<<" "<<abs_ackno<<" payload "<<iter->payload().copy()<<" syn "<<iter->header().syn<<" fin "<<iter->header().fin<<endl;
            seg_acked = true;
            iter = _send_window.erase(iter);
        }
        else
        {
            cout<<"\t not remove it from sender window "<<abs_idx<<" "<<len<<" "<<abs_ackno<<" payload "<<iter->payload().copy()<<" syn "<<iter->header().syn<<" fin "<<iter->header().fin<<endl;
            ++iter;
        }
    } 

    //  send_window中没有字节被ack，故不需要重启 / 关闭定时器 ，也不需要发送数据
    if(!seg_acked)
    {
        cout<<"invalid acked"<<endl;
        return ;
    }
    //  上一个计时重传的分组被移除 故 下一个重新计数
    _consecutive_retransmissions_cnt = 0;
    //  如果send_window中还有未发送的分组 则 为send_window新的最左侧分组开启timer
    if(!_send_window.empty())
    {
        cout<<"close and start a new timer"<<endl;
        _timer.reset();
        _timer.start(_initial_retransmission_timeout);
    }
    //  否则关闭老timer
    else
    {
        cout<<"send_window is empty"<<endl;
        _timer.reset();
    }

    //  接着从next_seqno发送新分组 (扩大send_window) 填充receive_window
    fill_window();

    cout<<"============ack_received end==========="<<endl;

}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) { 
    cout<<"==============tick start============="<<endl;
    
    cout<<"alarm "<<_timer.alarm()<<" ms_since_last_tick "<<ms_since_last_tick<<" receive window size "<<_receive_window_size<<endl;
    
    if(!_timer.active())        //  send_extra.cc 最后在alarm不工作的时候又调用了一次tick
    {
        cout<<"not active but want to use ???"<<endl;
        return ;
    }
    if(_timer.elapse(ms_since_last_tick))
    {
        TCPSegment & oldest_seg = _send_window.front();
                        //  不是这样重新发送的。因为fill_window是从bytestream中读取新数据 再 发送。而这里是要重新发送老数据。老数据都在send_window中
                        // _next_seqno = unwrap(oldest_seg.header().seqno,_isn,_next_seqno);
                        // //  重新发送_next_seqno分组
                        // fill_window();

        //  重新发送_next_seqno分组
        cout<<"cnt "<<_consecutive_retransmissions_cnt<<endl;
        // if(_consecutive_retransmissions_cnt == TCPConfig::MAX_RETX_ATTEMPTS)
        // {
        //     cout<<"this tcp connection is aborted"<<endl;
        //     _aborted = true;
        //     return ;
        // }
        uint64_t timeout = _timer.initial_alarm();
        //  只有当window size > 0的时候 才 double alarm ; 才 ++ cnt
        //  why ？ 有啥依据吗 ？？
        if(_receive_window_size > 0)
        {
            timeout <<= 1;
            ++_consecutive_retransmissions_cnt;         // > MAX_RETX_ATTEMPTS 该怎么办 ? 放弃该tcp连接？如何放弃 ？
        }
        else
        {
            cout<<"_receive_window_size == 0"<<endl;
        }
        //  如果recv windowsize == 0 则 不double 不记录cnt
        _timer.reset();
        _timer.start(timeout);
        _segments_out.push(oldest_seg);

        cout<<"restart the timer "<<_timer.alarm()<<endl;
    }
    cout<<"==============tick end============="<<endl;
}

unsigned int TCPSender::consecutive_retransmissions() const { 
    return _consecutive_retransmissions_cnt;
}

void TCPSender::send_empty_segment() 
{
    if(_aborted)
    {
        cout<<"this tcp connection is aborted"<<endl;
        return ;
    }
    
    cout<<"=======send_empty_segment start=========="<<endl;
    TCPSegment seg;
    seg.header().seqno = wrap(_next_seqno,_isn);
    _next_seqno += 0;
    _segments_out.push(seg);
    cout<<"=======send_empty_segment end=========="<<endl;
}
