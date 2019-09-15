/*
 * mqtt.hpp
 *
 *  Created on: 2019/09/08
 *      Author: hogehogei
 */

#ifndef MQTT_MQTT_HPP_
#define MQTT_MQTT_HPP_

#include "mbed.h"
#include "TCPSocket.h"
#include <string>
#include <queue>

namespace mqtt
{

static constexpr uint16_t sk_DefaultKeepAliveTimer = 10;     // Connectパケットで設定する KeepAlive値[s]
static constexpr int sk_ConnectTimeOut_ms = 1000;			 // 接続時のタイムアウト判定時間[ms]


void EncodeLength( uint8_t* buf_to_write, uint32_t len );
uint32_t CalcLengthFieldSize( uint32_t len );
uint32_t CalcVariableStringFieldLength( const char* str );

bool Is_CONNACK_Header( uint8_t hdr );

struct PubData
{
	std::string topic;
	std::string data;
};

class ConnectPacketHdr
{
public:

	enum ConnectFlag
	{
		CleanSession = 0x02,
	};

public:

	ConnectPacketHdr( const char* client_id );
	~ConnectPacketHdr() noexcept;

	uint32_t Serialize( uint8_t* buf, uint32_t buflen ) const;
	uint32_t Length() const;

private:

	static constexpr uint32_t sk_ConnectHdrBaseLen = 12;

	// uint16_t protocol_name_len;
	// uint8_t  protocol_name[6];
	// uint8_t  protocol_version;
	uint8_t  m_ConnectFlags;
	uint16_t m_KeepAliveTimer;

	const char* m_ClientID;
};

class PublishPacketHdr
{
public:

	PublishPacketHdr( const std::string& topic, const std::string& data, uint16_t msgid );
	~PublishPacketHdr() noexcept;

	uint32_t Serialize( uint8_t* buf, uint32_t buflen ) const;
	uint32_t Length() const;

private:

	uint16_t    m_Length;
	std::string m_Topic;
	uint16_t	m_ID;
	std::string m_Data;
};

class Message
{
public:
	enum PacketType
	{
		TypeConnect		= 1,
		TypeConAck		= 2,
		TypePublish		= 3,
		TypePubAck		= 4,
		TypePubRec		= 5,
		TypePubRel		= 6,
		TypePubConp		= 7,
		TypeSubscribe	= 8,
		TypeSubAck		= 9,
		TypeUnSubscribe = 10,
		TypeUnSubAck	= 11,
		TypePingReq		= 12,
		TypePingResp	= 13,
		TypeDisconnect	= 14,
	};

	enum Duplicate {
		DuplicateOff,
		DuplicateOn,
	};

	enum Retain {
		RetainOff,
		RetainOn,
	};

	enum QOS {
		QOS_0 = 0,
		QOS_1,
		QOS_2,
	};

	static constexpr uint32_t sk_MaxMessageLen = 0xFFFFFFF;

public:

	Message( PacketType type, uint32_t len );
	Message( Duplicate duplicate_message, QOS qos, Retain retain, uint32_t len );
	~Message() noexcept;

	bool Write( const ConnectPacketHdr& connect_hdr );
	bool Write( const PublishPacketHdr& pubhdr );

	uint8_t Type() const;
	uint8_t* Buffer();
	uint32_t Length() const;
	uint32_t PayloadsLength() const;

private:

	uint32_t m_PayloadLength;
	uint8_t* m_Buffer;
};

class Publisher
{
private:

	enum State {
		MQTTStateWait,
		MQTTConnectAckWait,
		MQTTPubReady
	};
public:

	Publisher( const char* client_id, TCPSocket socket );
	~Publisher() noexcept;

	void Connect();
	void Disconnect();
	bool Publish( PubData& pubdata );
	void Update();

	bool IsConnected() const;

private:

	void sendConnectPacket( const char* client_id );
	void checkRecvConnectAckPacket();
	void publishQueuedData();

	TCPSocket               m_Socket;
	Timer					m_Timer;
	const char*             m_ClientID;

	std::queue<PubData>		m_PubData;
	Mutex					m_QueueMtx;
	State					m_State;
	uint16_t				m_PubPktID;
};

}

#endif /* MQTT_MQTT_HPP_ */
