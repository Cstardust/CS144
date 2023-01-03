#include "receiver_harness.hh"
#include "wrapping_integers.hh"

#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>

using namespace std;

enum class Result { NOT_SYN, OK };
static std::string result_name(Result res) {
    switch (res) {
        case Result::NOT_SYN:
            return "(no SYN received, so no ackno available)";
        case Result::OK:
            return "(SYN received, so ackno available)";
        default:
            return "unknown";
    }
}

static void Assert(bool msg)
{
    if(!msg)
    {
        exit(-1);
    }
}

int main() {
    try {
        {
            cout<<"======================my test 01=============="<<endl;
            TCPReceiver receiver(4000);
            //  window size
            Assert(receiver.window_size() == 4000);
            //  未初始化isn
            Assert(!receiver.ackno().has_value());
            Assert(receiver.unassembled_bytes() == 0);

            //  准备tcp segment
                TCPSegment seg;
                seg.payload() = std::string("hello , shc! good bye shc!");
                seg.header().ack = false;
                seg.header().fin = false;
                seg.header().syn = true;
                seg.header().rst = false;
                seg.header().ackno = WrappingInt32(0);
                seg.header().seqno = WrappingInt32(0);
                seg.header().win = 0;

            receiver.segment_received(std::move(seg));

            Result res;
            if (not receiver.ackno().has_value()) {
                res = Result::NOT_SYN;
            } else {
                res = Result::OK;
            }

            if (Result::OK != res) {
                throw ReceiverExpectationViolation("TCPReceiver::segment_received() reported `" + result_name(res) + "`, but it was expected to report `" +result_name(Result::OK) + "`");
            }
        }

        {
            cout<<"======================my test 02=============="<<endl;
            TCPReceiver receiver(200);
            //  window size
            Assert(receiver.window_size() == 200);

            //  tcp segment abcdefg , syn isn = 5
            {
                TCPSegment seg;
                seg.payload() = std::string("abcdefg");
                seg.header().ack = false;
                seg.header().fin = false;
                seg.header().syn = true;
                seg.header().rst = false;
                seg.header().ackno = WrappingInt32(0);
                seg.header().seqno = WrappingInt32(5);
                seg.header().win = 0;
                receiver.segment_received(std::move(seg));
                Assert(receiver.ackno().has_value() == true);
                Assert(receiver.stream_out().bytes_written() == 7);
                Assert(receiver.ackno() == WrappingInt32(13));

                //////// bytestream //////////   ////// recv_window //////
                //////// abcdefg    //////////   ////// empty       ////// 
// abs_seq      ////////01234567    //////////   ////// 8           //////
// seq = abs_seq + 5
            }
            

            {
                //  abc , seqno = 14
                TCPSegment seg;
                seg.payload() = std::string("abc");
                seg.header().ack = false;
                seg.header().fin = false;
                seg.header().syn = false;
                seg.header().rst = false;
                seg.header().ackno = WrappingInt32(0);
                seg.header().seqno = WrappingInt32(14);
                seg.header().win = 0;
                receiver.segment_received(std::move(seg));
                Assert(receiver.stream_out().bytes_written() == 7);
                Assert(receiver.unassembled_bytes() == 3);

                //////// bytestream //////////   ////// recv_window //////
                //////// abcdefg    //////////   ////// abc       //////
            }

            {
                //  a , seqno = 13
                TCPSegment seg;
                seg.payload() = std::string("a");
                seg.header().ack = false;
                seg.header().fin = false;
                seg.header().syn = false;
                seg.header().rst = false;
                seg.header().ackno = WrappingInt32(0);
                seg.header().seqno = WrappingInt32(13);
                seg.header().win = 0;
                receiver.segment_received(std::move(seg));
                Assert(receiver.stream_out().bytes_written() == 11);    //  abcdefgaabc
                Assert(receiver.unassembled_bytes() == 0);
                Assert(receiver.ackno() == WrappingInt32(17));          //  syn(0) + abcdefgaabc[1,11] -> 12 +  isn(5) 
                ByteStream stream = receiver.stream_out();
                cout<<stream.read(stream.buffer_size())<<endl;
                //////// bytestream  //////////   ////// recv_window //////
                //////// abcdefgaabc //////////   ////// empty       //////
// abs_seq      //////0 1234567891011//////////   ////// 12          //////
// seq = abs_seq + 5
            }

            {
                //  "" , fin , seqno = 17
                TCPSegment seg;
                seg.payload() = std::string("");
                seg.header().ack = false;
                seg.header().fin = true;
                seg.header().syn = false;
                seg.header().rst = false;
                seg.header().ackno = WrappingInt32(0);
                seg.header().seqno = WrappingInt32(17);
                seg.header().win = 0;
                receiver.segment_received(std::move(seg));
                Assert(receiver.stream_out().bytes_written() == 11);    //  abcdefgaabc
                Assert(receiver.unassembled_bytes() == 0);
                Assert(receiver.ackno().value() == WrappingInt32(18));          //  syn(0) + abcdefgaabc[1,11] + fin[12] -> 13 + isn(5) 
                //////// bytestream  //////////   ////// recv_window //////
                //////// abcdefgaabc //////////   ////// fin empty       //////
// abs_seq      //////0 1234567891011//////////   ////// 12  13          //////
// seq = abs_seq + 5
            }
        }

        cout<<"===============my test3=============="<<endl;
        /* segment with SYN + data + FIN */
        {
            TCPReceiver receiver(4000);

            uint32_t isn = 1000;
            //  now LISTEN
            Assert(not receiver.ackno().has_value());
                TCPSegment seg;
                seg.payload() = std::string("shc");
                seg.header().ack = false;
                seg.header().fin = true;
                seg.header().syn = true;
                seg.header().rst = false;
                seg.header().ackno = WrappingInt32(0);
                seg.header().seqno = WrappingInt32(isn);
                seg.header().win = 0;
                receiver.segment_received(seg);
            //  already has received syn
            Assert(receiver.ackno().has_value());
            //  now FIN_RECV
            Assert(receiver.stream_out().input_ended());
            Assert(receiver.ackno().value() == WrappingInt32(0 + 3 + 1 + 1 + isn));  //  syn(0) + "shc"[123] + fin(4) -> 5 + isn = 1005;
            Assert(receiver.unassembled_bytes() == 0);
            cout<<receiver.stream_out().read(receiver.stream_out().buffer_size())<<endl;    //  shc
            //  stream empty && closed
            Assert(receiver.stream_out().eof());
                //////// bytestream   //////////   ////// recv_window //////
                //////syn shc         //////////   ////// fin empty       //////
// abs_seq      //////0   123         //////////   ////// 4  5          //////
// seq = abs_seq + isn
        }

        // auto rd = get_random_generator();
        cout<<"======================================"<<endl;
        {
            TCPReceiverTestHarness test{4000};
            test.execute(ExpectWindow{4000});
            test.execute(ExpectAckno{std::optional<WrappingInt32>{}});
            test.execute(ExpectUnassembledBytes{0});
            test.execute(ExpectTotalAssembledBytes{0});
            test.execute(SegmentArrives{}.with_syn().with_seqno(0).with_result(SegmentArrives::Result::OK));
            test.execute(ExpectAckno{WrappingInt32{1}});
            test.execute(ExpectUnassembledBytes{0});
            test.execute(ExpectTotalAssembledBytes{0});
        }
        cout<<"======================================"<<endl;

        {
            TCPReceiverTestHarness test{5435};
            test.execute(ExpectAckno{std::optional<WrappingInt32>{}});
            test.execute(ExpectUnassembledBytes{0});
            test.execute(ExpectTotalAssembledBytes{0});
            test.execute(SegmentArrives{}.with_syn().with_seqno(89347598).with_result(SegmentArrives::Result::OK));
            test.execute(ExpectAckno{WrappingInt32{89347599}});
            test.execute(ExpectUnassembledBytes{0});
            test.execute(ExpectTotalAssembledBytes{0});
        }
        cout<<"======================================"<<endl;

        {
            TCPReceiverTestHarness test{5435};
            test.execute(ExpectAckno{std::optional<WrappingInt32>{}});
            test.execute(ExpectUnassembledBytes{0});
            test.execute(ExpectTotalAssembledBytes{0});
            test.execute(SegmentArrives{}.with_seqno(893475).with_result(SegmentArrives::Result::NOT_SYN));
            test.execute(ExpectAckno{std::optional<WrappingInt32>{}});
            test.execute(ExpectUnassembledBytes{0});
            test.execute(ExpectTotalAssembledBytes{0});
        }
        cout<<"======================================"<<endl;

        {
            TCPReceiverTestHarness test{5435};
            test.execute(ExpectAckno{std::optional<WrappingInt32>{}});
            test.execute(ExpectUnassembledBytes{0});
            test.execute(ExpectTotalAssembledBytes{0});
            test.execute(SegmentArrives{}.with_ack(0).with_fin().with_seqno(893475).with_result(
                SegmentArrives::Result::NOT_SYN));
            test.execute(ExpectAckno{std::optional<WrappingInt32>{}});
            test.execute(ExpectUnassembledBytes{0});
            test.execute(ExpectTotalAssembledBytes{0});
        }
        cout<<"======================================"<<endl;

        {
            TCPReceiverTestHarness test{5435};
            test.execute(ExpectAckno{std::optional<WrappingInt32>{}});
            test.execute(ExpectUnassembledBytes{0});
            test.execute(ExpectTotalAssembledBytes{0});
            test.execute(SegmentArrives{}.with_ack(0).with_fin().with_seqno(893475).with_result(
                SegmentArrives::Result::NOT_SYN));
            test.execute(ExpectAckno{std::optional<WrappingInt32>{}});
            test.execute(ExpectUnassembledBytes{0});
            test.execute(ExpectTotalAssembledBytes{0});
            test.execute(SegmentArrives{}.with_syn().with_seqno(89347598).with_result(SegmentArrives::Result::OK));
            test.execute(ExpectAckno{WrappingInt32{89347599}});
            test.execute(ExpectUnassembledBytes{0});
            test.execute(ExpectTotalAssembledBytes{0});
        }
        cout<<"======================================"<<endl;

        {
            TCPReceiverTestHarness test{4000};
            test.execute(SegmentArrives{}.with_syn().with_seqno(5).with_fin().with_result(SegmentArrives::Result::OK));
            test.execute(ExpectState{TCPReceiverStateSummary::FIN_RECV});
            test.execute(ExpectAckno{WrappingInt32{7}});
            test.execute(ExpectUnassembledBytes{0});
            test.execute(ExpectTotalAssembledBytes{0});
        }
        cout<<"======================================"<<endl;

        {
            // Window overflow
            size_t cap = static_cast<size_t>(UINT16_MAX) + 5;
            TCPReceiverTestHarness test{cap};
            test.execute(ExpectWindow{cap});
        }
    } catch (const exception &e) {
        cerr << e.what() << endl;
        return 1;
    }

    return EXIT_SUCCESS;
}
