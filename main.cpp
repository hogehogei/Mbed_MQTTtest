#if !FEATURE_LWIP
    #error [NOT_SUPPORTED] LWIP not supported for this target
#endif

#include "mbed.h"
#include "EthernetInterface.h"
#include "TCPServer.h"
#include "TCPSocket.h"

#include "mqtt/mqtt.hpp"

#if 0
#define HTTP_STATUS_LINE "HTTP/1.0 200 OK"
#define HTTP_HEADER_FIELDS "Content-Type: text/html; charset=utf-8"
#define HTTP_MESSAGE_BODY ""                                     \
"<html>" "\r\n"                                                  \
"  <body style=\"display:flex;text-align:center\">" "\r\n"       \
"    <div style=\"margin:auto\">" "\r\n"                         \
"      <h1>Hello World</h1>" "\r\n"                              \
"      <p>It works !</p>" "\r\n"                                 \
"    </div>" "\r\n"                                              \
"  </body>" "\r\n"                                               \
"</html>"

#define HTTP_RESPONSE HTTP_STATUS_LINE "\r\n"   \
                      HTTP_HEADER_FIELDS "\r\n" \
                      "\r\n"                    \
                      HTTP_MESSAGE_BODY "\r\n"
#endif

int main()
{
    printf("Basic HTTP server example\n");
    
    EthernetInterface eth;
    eth.set_network( "192.168.24.100", "255.255.255.0", "192.168.24.1" );
    int result = 0;
    if( (result = eth.connect()) == NSAPI_ERROR_OK ){
    	printf( "ethernet linkup.\n");
    }
    else {
    	printf( "ethernet linkup failed. ErrorCode: %d\n", result );
    }
    
    printf("The target IP address is '%s'\n", eth.get_ip_address());
    
    //TCPServer srv;
    TCPSocket clt_sock;

    mqtt::Publisher* publisher = nullptr;
    mqtt::PubData data = { "topic/greeting", "Hello World!" };
    Timer timer;
    timer.start();

    bool is_connected = false;
    bool is_publish_send = false;
    while (true) {

    	if( timer.read_ms() > 1000 ){
#if 1
			if( !is_connected ){
			    if( (result = clt_sock.open( &eth )) == NSAPI_ERROR_OK ){
			    	printf( "Open TCP socket.\n" );
			    }
			    else {
			    	printf( "Could not open TCP socket. ErrorCode: %d\n", result );
			    }

				int result = clt_sock.connect( "192.168.24.128", 1883 );

				if( result == NSAPI_ERROR_OK ){
					printf( "TCP connect succeeded.\n" );
					is_connected = true;
					publisher = new mqtt::Publisher( "hogeisan", clt_sock );
				}
				else {
					printf( "TCP connect failed. ErrorCode: %d\n", result );
					clt_sock.close();
				}
			}
			else {
				if( !publisher->IsConnected() ){
					printf( "MQTT connect send.\n" );
					publisher->Connect();
				}
				else {
					printf( "MQTT publish send.\n" );
					publisher->Publish( data );
					is_publish_send = true;
				}
			}
#endif
    		timer.reset();
    	}

    	if( publisher ){
    		publisher->Update();

    		if( is_publish_send ){
    			clt_sock.close();
    			is_connected = false;
    			is_publish_send = false;
    			delete publisher;
    			publisher = nullptr;
    		}
    	}
    }
}
