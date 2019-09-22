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
: m_Length( 2 + topic.size() + data.size() ),
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
	int topic_len = m_Topic.size();
	buf[0] = topic_len >> 8;
	buf[1] = topic_len;
	// Topic
	dst = &buf[2];
	m_Topic.copy( reinterpret_cast<char*>(dst), m_Topic.size() );
	// Data
	dst = &buf[2 + m_Topic.size()];
	m_Data.copy( reinterpret_cast<char*>(dst), m_Data.size() );

	return Length();
}

uint32_t PublishPacketHdr::Length() const
{
	return m_Length;
}

Message::Message( PacketType type, uint32_t len )
 :    m_PayloadLength( len ),
	  m_Buffer( nullptr )
{
	uint32_t header_len = 1 + CalcLengthFieldSize( m_PayloadLength );
	uint32_t buffer_len = Length();
	m_Buffer = new uint8_t[header_len + buffer_len];

	m_Buffer[0] = static_cast<uint8_t>(type) << 4;
	EncodeLength( &m_Buffer[1], len );
}

Message::Message( Duplicate duplicate_message, QOS qos, Retain retain, uint32_t len )
:     m_PayloadLength( len ),
	  m_Buffer( nullptr )
{
	uint32_t header_len = 1 + CalcLengthFieldSize( m_PayloadLength );
	uint32_t buffer_len = Length();
	m_Buffer = new uint8_t[header_len + buffer_len];
	m_Buffer[0] = 0x10;

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
{
	delete [] m_Buffer;
}

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

Publisher::Publisher( const char* client_id, EthernetInterface& eth, const SocketAddress dst_address )
:
	m_Eth( eth ),
	m_DstAddress( dst_address ),
	m_Socket(),
	m_Timer(),
	m_ClientID( client_id ),
	m_PubThread(),
	m_PubData(),
	m_QueueMtx(),
	m_State( MQTTStateWait ),
	m_PubPktID( 0 )
{
	m_PubThread.start( this, &Publisher::update );
}

Publisher::~Publisher()
{
	m_PubThread.terminate();
	m_Socket.close();
}

bool Publisher::Publish( PubData& pubdata )
{
	m_QueueMtx.lock();
	m_PubData.push( pubdata );
	m_QueueMtx.unlock();

	return true;
}

void Publisher::update()
{
	while( 1 ){
		switch( m_State ){
		case MQTTStateWait:
			connectBrokerIfPubDataExist();
			break;
		case MQTTConnectAckWait:
			checkRecvConnectAckPacket();
			break;
		case MQTTPubReady:
			publishQueuedData();
			break;
		}
	}
}

void Publisher::connectBrokerIfPubDataExist()
{
	m_QueueMtx.lock();
	bool is_empty = m_PubData.empty();
	m_QueueMtx.unlock();

	if( !is_empty ){
		int result = m_Socket.open( &m_Eth );
	    if( result == NSAPI_ERROR_OK ){
	    	printf( "Open TCP socket.\n" );

			result = m_Socket.connect( m_DstAddress );
			if( result == NSAPI_ERROR_OK ){
				printf( "TCP connect succeeded.\n" );
				sendConnectPacket( m_ClientID );
				m_State = MQTTConnectAckWait;
			}
			else {
				printf( "TCP connect failed. ErrorCode: %d\n", result );
				m_Socket.close();
			}
	    }
	    else {
	    	printf( "Could not open TCP socket. ErrorCode: %d\n", result );
	    }
	}
}

void Publisher::sendConnectPacket( const char* client_id )
{
	ConnectPacketHdr connect_hdr( client_id );
	Message connect_pkt( Message::DuplicateOff, Message::QOS_0, Message::RetainOff, connect_hdr.Length() );

	if( connect_pkt.Write( connect_hdr ) ){
		if( m_Socket.send( connect_pkt.Buffer(), connect_pkt.Length() ) < 0 ){
			printf( "Connect pkt write Failed.\n" );
			m_State = MQTTStateWait;
		}
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
	uint16_t received = 0;
	uint8_t buf[4] = {0,};
	while( received < 4 && (result = m_Socket.recv( &buf, sizeof(buf) - received )) > 0 ){
		printf( "recv wait.\n");
		received += result;
	}

	if( result < 0 ){
		printf( "socket Receive failed. ErrorCode: %d", result );
		return;
	}

	// ヘッダが見つかった
	// 固定ヘッダをチェック
	if( !Is_CONNACK_Header(buf[0]) ){
		printf( "Is not CONNACK Header.\n" );
		return;
	}
	// 長さフィールドをチェック
	// 可変ヘッダの長さが2、接続許可(0x00）であればOK
	if( buf[1] == 2 && buf[3] == 0x00 ){
		m_State = MQTTPubReady;
		m_Timer.stop();
		printf( "CONACK Received. MQTTPubReady.\n" );
	}
	else {
		m_State = MQTTConnectAckWait;
		if( m_Timer.read_ms() >= sk_ConnectTimeOut_ms ){
			m_Timer.stop();
			resetToMQTTWaitState();
		}
	}
}

void Publisher::resetToMQTTWaitState()
{
	m_Socket.close();
	m_State = MQTTStateWait;
}

void Publisher::publishQueuedData()
{
	PubData data;

	while( 1 ){
		m_QueueMtx.lock();
		if( m_PubData.empty() ){
			m_QueueMtx.unlock();
			break;
		}

		data = m_PubData.front();
		m_PubData.pop();
		m_QueueMtx.unlock();

		PublishPacketHdr pubhdr( data.topic, data.data, m_PubPktID );
		++m_PubPktID;

		Message publish_message(Message::TypePublish, pubhdr.Length());
		if( publish_message.Write( pubhdr ) > 0 ){
			int result = m_Socket.send( publish_message.Buffer(), publish_message.Length() );
			if( result < 0 ){
				printf( "Send Failed. ErrorCode: %d\n", result );
				break;
			}

			printf( "Publish Succeded.\n" );
		}
	}

	resetToMQTTWaitState();
}

}	// end of namespace mqtt
