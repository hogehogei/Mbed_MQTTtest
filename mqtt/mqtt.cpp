/*
 * mqtt.cpp
 *
 *  Created on: 2019/09/08
 *      Author: hogehogei
 */

#include "mqtt.hpp"
#include <algorithm>

namespace mqtt
{

void EncodeLength( uint8_t* buf_to_write, uint32_t len )
{
	uint32_t remain_len = len;
	uint32_t idx = 0;

	while( 1 ){
		if( remain_len > 0x7F ){
			buf_to_write[idx] = 0x80 | (remain_len & 0x7F);
			remain_len >>= 7;
		}
		else {
			buf_to_write[idx] = (remain_len & 0x7F);
			break;
		}
		++idx;
	}
}

uint32_t CalcLengthFieldSize( uint32_t len )
{
	uint32_t byte_len = 0;
	if( len <= 0x7F ){
		byte_len = 1;
	}
	else if( len <= 0x3FFF ){
		byte_len = 2;
	}
	else if( len <= 0x1FFFFF ){
		byte_len = 3;
	}
	else if( len <= 0xFFFFFFF ){
		byte_len = 4;
	}
	else {
		// IT MUST BE BUG!!
		byte_len = 4;
	}

	return byte_len;
}

uint32_t CalcVariableStringFieldLength( const char* str )
{
	if( str == nullptr ){
		return 2;
	}

	return 2 + strlen( reinterpret_cast<const char*>(str) );
}

bool Is_CONNACK_Header( uint8_t hdr )
{
	if( ((hdr & 0xF0) >> 4) == Message::TypeConAck &&
		(hdr & 0x0F) == 0 ){
		return true;
	}

	return false;
}

ConnectPacketHdr::ConnectPacketHdr( const char* client_id )
:
	m_ConnectFlags( 0 ),
	m_KeepAliveTimer( sk_DefaultKeepAliveTimer ),
	m_ClientID( client_id )
{
	m_ConnectFlags = ConnectFlag::CleanSession;
}

ConnectPacketHdr::~ConnectPacketHdr() noexcept
{}


uint32_t ConnectPacketHdr::Serialize( uint8_t* buf, uint32_t buflen ) const
{
	if( m_ClientID == nullptr ){
		return 0;
	}
	if( buflen < Length() ){
		return 0;
	}

	// byte0-1 Length
	buf[0] = 0x00;
	buf[1] = 0x06;
	// byte2-7 Protocol Name
	buf[2] = 'M';
	buf[3] = 'Q';
	buf[4] = 'I';
	buf[5] = 's';
	buf[6] = 'd';
	buf[7] = 'p';
	// byte8   Protocol Version
	buf[8] = 0x03;
	// byte9   Connect Flags
	buf[9] = m_ConnectFlags;
	// byte10-11  KeepAlive Timer
	buf[10] = m_KeepAliveTimer >> 8;
	buf[11] = m_KeepAliveTimer & 0xFF;

	// byte12-13  Set ClientID Length( MSB, LSB )
	uint32_t client_id_len = strlen( m_ClientID );
	buf[12] = client_id_len >> 8;
	buf[13] = client_id_len & 0xFF;

	// byte14~    Copy ClientID
	uint8_t* dst = &buf[14];
	const uint8_t* src = reinterpret_cast<const uint8_t*>(m_ClientID);

	while( *src != 0 ){
		*dst++ = *src++;
	}

	return static_cast<uint32_t>(&buf[0] - dst);
}

uint32_t ConnectPacketHdr::Length() const
{
	return sk_ConnectHdrBaseLen + CalcVariableStringFieldLength( m_ClientID );
}

PublishPacketHdr::PublishPacketHdr( const std::string& topic, const std::string& data, uint16_t msgid )
: m_Length( 2 + topic.size() + 2 + data.size() ),
  m_Topic( topic ),
  m_ID( msgid ),
  m_Data( data )
{}

PublishPacketHdr::~PublishPacketHdr()
{}

uint32_t PublishPacketHdr::Serialize( uint8_t* buf, uint32_t buflen ) const
{
	uint8_t* dst = nullptr;
	if( buf == nullptr ){
		return 0;
	}

	if( buflen < Length() ){
		return 0;
	}

	// byte0-1 Length
	buf[0] = m_Length >> 8;
	buf[1] = m_Length;
	// Topic
	dst = &buf[2];
	m_Topic.copy( reinterpret_cast<char*>(buf), m_Topic.size() );
	// Message ID
	dst = &buf[2 + m_Topic.size()];
	dst[0] = m_ID >> 8;
	dst[1] = m_ID;
	// Data
	dst = &buf[2 + m_Topic.size() + 2];
	m_Data.copy( reinterpret_cast<char*>(buf), m_Data.size() );

	return Length();
}

uint32_t PublishPacketHdr::Length() const
{
	return 2 + m_Topic.size() + 2 + m_Data.size();
}

Message::Message( PacketType type, uint32_t len )
 :    m_PayloadLength( len ),
	  m_Buffer( nullptr )
{
	uint32_t header_len = 1 + CalcLengthFieldSize( m_PayloadLength );
	uint32_t buffer_len = Length();
	m_Buffer = new uint8_t[header_len + buffer_len];

	m_Buffer[0] = 0;
	EncodeLength( &m_Buffer[1], len );
}

Message::Message( Duplicate duplicate_message, QOS qos, Retain retain, uint32_t len )
:     m_PayloadLength( len ),
	  m_Buffer( nullptr )
{
	uint32_t header_len = 1 + CalcLengthFieldSize( m_PayloadLength );
	uint32_t buffer_len = Length();
	m_Buffer = new uint8_t[header_len + buffer_len];

	if( duplicate_message == DuplicateOn ){
		m_Buffer[0] |= 0x08;
	}
	if( qos == QOS_1 ){
		m_Buffer[0] |= 0x02;
	}
	if( qos == QOS_2 ){
		m_Buffer[0] |= 0x04;
	}
	if( retain == RetainOn ){
		m_Buffer[0] |= 0x01;
	}

	EncodeLength( &m_Buffer[1], len );
}

Message::~Message()
{}

bool Message::Write( const ConnectPacketHdr& connect_hdr )
{
	uint32_t header_len = 1 + CalcLengthFieldSize( m_PayloadLength );
	return connect_hdr.Serialize( &m_Buffer[header_len], m_PayloadLength );
}

bool Message::Write( const PublishPacketHdr& pubhdr )
{
	uint32_t header_len = 1 + CalcLengthFieldSize( m_PayloadLength );
	return pubhdr.Serialize( &m_Buffer[header_len], m_PayloadLength );
}

uint8_t Message::Type() const
{
	return ((m_Buffer[0] >> 4) & 0x0F);
}

uint8_t* Message::Buffer()
{
	return m_Buffer;
}

uint32_t Message::Length() const
{
	// ControlHeader = 1byte
	// Length        = 1~4Byte
	// Message       = Length(byte)
	return 1 + CalcLengthFieldSize( m_PayloadLength ) + m_PayloadLength;
}

uint32_t Message::PayloadsLength() const
{
	return m_PayloadLength;
}

Publisher::Publisher( const char* client_id, TCPSocket socket )
:
	m_Socket( socket ),
	m_Timer(),
	m_ClientID( client_id ),
	m_PubData(),
	m_QueueMtx(),
	m_State( MQTTStateWait ),
	m_PubPktID( 0 )
{
	m_Socket.set_blocking( false );
}

Publisher::~Publisher()
{}

void Publisher::Connect()
{
	if( m_State != MQTTStateWait ){
		return;
	}

	sendConnectPacket( m_ClientID );
	m_Timer.reset();
	m_Timer.start();
	m_State = MQTTConnectAckWait;
}

void Publisher::Disconnect()
{
	if( !(m_State == MQTTConnectAckWait || m_State == MQTTPubReady) ){
		return;
	}

	// Disconnect を通知するパケットを送る必要があるなら送る

	m_State = MQTTStateWait;
}

bool Publisher::Publish( PubData& pubdata )
{
	if( m_State != MQTTPubReady ){
		return false;
	}

	m_QueueMtx.lock();
	m_PubData.push( pubdata );
	m_QueueMtx.unlock();

	return true;
}

void Publisher::Update()
{
	switch( m_State ){
	case MQTTStateWait:
		break;
	case MQTTConnectAckWait:
		checkRecvConnectAckPacket();
		break;
	case MQTTPubReady:
		publishQueuedData();
		break;
	}
}

bool Publisher::IsConnected() const
{
	return m_State == MQTTPubReady;
}

void Publisher::sendConnectPacket( const char* client_id )
{
	ConnectPacketHdr connect_hdr( client_id );
	Message connect_pkt( Message::DuplicateOff, Message::QOS_0, Message::RetainOff, connect_hdr.Length() );

	if( connect_pkt.Write( connect_hdr ) > 0 ){
		m_Socket.send( connect_pkt.Buffer(), connect_pkt.Length() );
	}

	m_State = MQTTConnectAckWait;
	m_Timer.start();
}

void Publisher::checkRecvConnectAckPacket()
{
	if( m_State != MQTTConnectAckWait ){
		return;
	}

	nsapi_size_or_error_t result;
	uint8_t hdr = 0;
	bool is_find_header;
	while( (result = m_Socket.recv( &hdr, 1 )) > 0 ){
		if( Is_CONNACK_Header( hdr )){
			is_find_header = true;
			break;
		}
	}

	if( !is_find_header ){
		return;
	}

	// ヘッダが見つかった
	// 長さフィールドをチェック
	uint8_t buf[3] = {0,};
	while( (result = m_Socket.recv( buf, 3 )) > 0 ){
	}

	// 可変ヘッダの長さが2、接続許可(0x00）であればOK
	if( buf[0] == 2 && buf[2] == 0x00 ){
		m_State = MQTTPubReady;
	}
	else {
		m_State = MQTTConnectAckWait;
		if( m_Timer.read_ms() >= sk_ConnectTimeOut_ms ){
			m_Timer.stop();
			m_State = MQTTStateWait;
		}
	}
}

void Publisher::publishQueuedData()
{
	PubData data;

	while( 1 ){
		m_QueueMtx.lock();
		if( m_PubData.empty() ){
			m_QueueMtx.unlock();
			return;
		}

		data = m_PubData.front();
		m_PubData.pop();
		m_QueueMtx.unlock();

		PublishPacketHdr pubhdr( data.topic, data.data, m_PubPktID );
		++m_PubPktID;

		Message publish_message(Message::TypePublish, pubhdr.Length());
		if( publish_message.Write( pubhdr ) > 0 ){
			m_Socket.send( publish_message.Buffer(), publish_message.Length() );
		}
	}
}

}	// end of namespace mqtt
