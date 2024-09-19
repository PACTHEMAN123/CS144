#include "tcp_receiver.hh"

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message )
{
  if(message.RST){
    reassembler_.reader().set_error();
    return;
  }

  if(message.SYN){
    ISN = message.seqno;
    ISN_recv = true;
    reassembler_.insert(0, message.payload, message.FIN);
    ack_abseqno += message.sequence_length();
    return;
  }

  if(!ISN_recv)
    return;

  
  uint64_t first_unindex = reassembler_.writer().bytes_pushed();
  reassembler_.insert(message.seqno.unwrap(ISN, first_unindex)-1, message.payload, message.FIN);
  ack_abseqno = reassembler_.writer().bytes_pushed() + 1;
  if(reassembler_.writer().is_closed())
    ack_abseqno += 1;
}

TCPReceiverMessage TCPReceiver::send() const
{
  TCPReceiverMessage t;
  if(ISN_recv){
    t.ackno = Wrap32::wrap(ack_abseqno, ISN);
  }
  
  if(reassembler_.reader().has_error())
    t.RST = true;

  t.window_size = (reassembler_.writer().available_capacity() > UINT16_MAX) ? UINT16_MAX : reassembler_.writer().available_capacity();
  return t;
}
