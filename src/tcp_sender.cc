#include "tcp_sender.hh"
#include "tcp_config.hh"

#include <algorithm>
#include <iostream>

using namespace std;

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  uint64_t num = 0;
  for(auto it = segments.begin(); it != segments.end(); it++){
    num += (*it).sequence_length();
  }
  return num;
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  return retrans_times_;
}

void TCPSender::push( const TransmitFunction& transmit )
{
  if(window_size_ < sequence_numbers_in_flight())
    return;

  uint64_t sz = window_size_ - sequence_numbers_in_flight();
  uint64_t length;

  if(sz == 0 && window_size_ == 0){
    sz = 1;
  }
    

  if(sz > 0 && next_seqno_ == 0 && input_.reader().is_finished() && !has_send_SYN && !has_send_FIN){
    //cout << "into super special place" << endl;
    TCPSenderMessage t;
    t.FIN = true;
    t.SYN = true;
    has_send_SYN = true;
    has_send_FIN = true;
    t.seqno = Wrap32::wrap(next_seqno_, isn_);
    if(input_.has_error())
      t.RST = true;
    transmit(t);
    segments.push_back(t);
    if(alarm.has_on == false){
      alarm.has_on = true;
      alarm.t = 0;
      alarm.expire_time = RTO_;
    }
    sz -= 2;
    next_seqno_ += 2;
    return;
  }

  if(sz > 0 && input_.reader().bytes_buffered() == 0 && input_.reader().is_finished() && !has_send_FIN){
    // send fin with 0 payload
    //cout << "into special place: sz: " << sz << endl;
    TCPSenderMessage t;
    t.FIN = true;
    has_send_FIN = true;
    t.seqno = Wrap32::wrap(next_seqno_, isn_);
    if(input_.has_error())
      t.RST = true;
    transmit(t);
    segments.push_back(t);
    if(alarm.has_on == false){
      alarm.has_on = true;
      alarm.t = 0;
      alarm.expire_time = RTO_;
    }
    sz -= 1;
    next_seqno_ += 1;
    return;
  }

  //cout << "sz: " << sz << " bufsz: " << input_.reader().bytes_buffered() << endl;
  while(sz > 0 && input_.reader().bytes_buffered()){
    TCPSenderMessage t;
    uint64_t bufsz = input_.reader().bytes_buffered();
    uint64_t maxsz = TCPConfig::MAX_PAYLOAD_SIZE;

    
    if(input_.reader().bytes_popped() == 0 && next_seqno_ == 0){
      t.SYN = true;
      bufsz += 1;
      maxsz += 1;
    }
    

    if(input_.writer().is_closed()){
      //cout << "bufsz: " << bufsz << " sz: " << sz << endl;
      t.FIN = true;
      bufsz += 1;
      maxsz += 1;
      if((bufsz > sz && maxsz > sz) || (maxsz < sz && maxsz < bufsz)){
        t.FIN = false;
        bufsz -= 1;
        maxsz -= 1;
      }
    }

    t.payload = input_.reader().peek();
    length = min({sz, maxsz, bufsz}) - t.SYN - t.FIN;
    t.payload = t.payload.substr(0, length);
    t.seqno = Wrap32::wrap(next_seqno_, isn_);
    if(input_.has_error())
      t.RST = true;

    input_.reader().pop(t.payload.size());
    sz -= t.sequence_length();

    if(t.sequence_length()){
      transmit(t);
      if(t.SYN)
        has_send_SYN = true;
      if(t.FIN)
        has_send_FIN = true;
      segments.push_back(t);
      if(window_size_ == 0)
        is_retry_ = true;
    }
      
    
    next_seqno_ += t.sequence_length();

    if(alarm.has_on == false){
      alarm.has_on = true;
      alarm.t = 0;
      alarm.expire_time = RTO_;
    }

  }

  if(sz > 0 && input_.reader().bytes_buffered() == 0 && input_.reader().bytes_popped() == 0 && next_seqno_ == 0 && !has_send_SYN){
    // send isn and syn with 0 payload
    TCPSenderMessage t;
    t.SYN = true;
    has_send_SYN = true;
    t.seqno = Wrap32::wrap(next_seqno_, isn_);
    if(input_.has_error())
      t.RST = true;
    transmit(t);
    segments.push_back(t);
    if(alarm.has_on == false){
      alarm.has_on = true;
      alarm.t = 0;
      alarm.expire_time = RTO_;
    }
    sz -= 1;
    next_seqno_ += 1;
  }

  
  
}

TCPSenderMessage TCPSender::make_empty_message() const
{
  TCPSenderMessage t;
  t.seqno = Wrap32::wrap(next_seqno_, isn_);
  if(input_.has_error())
    t.RST = true;
  return t;
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{ 
  if(msg.RST)
    input_.set_error();
  
  if(msg.ackno->unwrap(isn_, input_.reader().bytes_popped()) > next_seqno_)
    return;
  
  for(auto it = segments.begin(); it != segments.end();){
    if(msg.ackno->unwrap(isn_, input_.reader().bytes_popped()) 
    >= (*it).seqno.unwrap(isn_, input_.reader().bytes_popped()) 
    + (*it).sequence_length())
    {
      it = segments.erase(it);
      RTO_ = initial_RTO_ms_;
      alarm.t = 0;
      alarm.expire_time = RTO_;
      retrans_times_ = 0;
    } else {
      it++;
    }
  }
  
  window_size_ = msg.window_size;
  if(window_size_)
    is_retry_ = false;

  
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
  alarm.t += ms_since_last_tick;
  //cout << "in tick: " << "alarm: " << alarm.t << " RTO: " << RTO_ << endl;
  if(segments.empty())
    return;

  if(alarm.t >= alarm.expire_time){
    //cout << "timeout" << " RTO: " << RTO_ << endl;
    retrans_times_ += 1;
    transmit(segments.front());
    alarm.t = 0;
    if(!is_retry_)
      RTO_ *= 2;
    alarm.expire_time = RTO_;
  }
  
}
