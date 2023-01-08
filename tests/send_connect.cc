#include "sender_harness.hh"
#include "wrapping_integers.hh"

#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>

using namespace std;

int main() {
    try {
        auto rd = get_random_generator();
        cout<<"===================================="<<endl;
        {
            TCPConfig cfg;
            WrappingInt32 isn(rd());
            cfg.fixed_isn = isn;
            cout<<"config isn "<<isn<<endl;
            TCPSenderTestHarness test{"SYN sent test", cfg};            //  sender会先发送一个payload() = empty 的 , seq = isn 的 syn报文
            test.execute(ExpectState{TCPSenderStateSummary::SYN_SENT});
            test.execute(ExpectSegment{}.with_no_flags().with_syn(true).with_payload_size(0).with_seqno(isn));  //  check刚才发送的syn报文 是否在 send_window的头部
            test.execute(ExpectBytesInFlight{1});                       //  syn
        }

        cout<<"===================================="<<endl;

        {
            TCPConfig cfg;
            WrappingInt32 isn(rd());
            cfg.fixed_isn = isn;

            TCPSenderTestHarness test{"SYN acked test", cfg};
            test.execute(ExpectState{TCPSenderStateSummary::SYN_SENT});
            test.execute(ExpectSegment{}.with_no_flags().with_syn(true).with_payload_size(0).with_seqno(isn));
            test.execute(ExpectBytesInFlight{1});
            test.execute(AckReceived{WrappingInt32{isn + 1}});
            test.execute(ExpectState{TCPSenderStateSummary::SYN_ACKED});
            test.execute(ExpectNoSegment{});
            test.execute(ExpectBytesInFlight{0});
        }
        cout<<"===================================="<<endl;

        {
            TCPConfig cfg;
            WrappingInt32 isn(rd());
            cfg.fixed_isn = isn;

            TCPSenderTestHarness test{"SYN -> wrong ack test", cfg};
            test.execute(ExpectState{TCPSenderStateSummary::SYN_SENT});
            test.execute(ExpectSegment{}.with_no_flags().with_syn(true).with_payload_size(0).with_seqno(isn));
            test.execute(ExpectBytesInFlight{1});
            test.execute(AckReceived{WrappingInt32{isn}});
            test.execute(ExpectState{TCPSenderStateSummary::SYN_SENT});
            test.execute(ExpectNoSegment{});
            test.execute(ExpectBytesInFlight{1});
        }
        cout<<"===================================="<<endl;

        {
            TCPConfig cfg;
            WrappingInt32 isn(rd());
            cfg.fixed_isn = isn;
            //  reivew
            //  send syn + empty payload 
            TCPSenderTestHarness test{"SYN acked, data", cfg};          // sender.fill_window(); collect_output();
            test.execute(ExpectState{TCPSenderStateSummary::SYN_SENT});
            test.execute(ExpectSegment{}.with_no_flags().with_syn(true).with_payload_size(0).with_seqno(isn));
            test.execute(ExpectBytesInFlight{1});
            //  recv ack for syn 报文
            test.execute(AckReceived{WrappingInt32{isn + 1}});
            test.execute(ExpectState{TCPSenderStateSummary::SYN_ACKED});
            test.execute(ExpectNoSegment{});
            test.execute(ExpectBytesInFlight{0});
            //  send abcdefgh (start from seq(isn+1) , abs_seq(1))
            test.execute(WriteBytes{"abcdefgh"});
            test.execute(Tick{1});
            test.execute(ExpectState{TCPSenderStateSummary::SYN_ACKED});
            test.execute(ExpectSegment{}.with_seqno(isn + 1).with_data("abcdefgh"));
            test.execute(ExpectBytesInFlight{8});
            //  recv ack for abcdefgh 报文
            test.execute(AckReceived{WrappingInt32{isn + 9}});
            test.execute(ExpectState{TCPSenderStateSummary::SYN_ACKED});
            test.execute(ExpectNoSegment{});
            test.execute(ExpectBytesInFlight{0});
            test.execute(ExpectSeqno{WrappingInt32{isn + 9}});
        }
        cout<<"===================================="<<endl;

    } catch (const exception &e) {
        cerr << e.what() << endl;
        return 1;
    }


    cout<<"pass the send_connect"<<endl;
    return EXIT_SUCCESS;
}
