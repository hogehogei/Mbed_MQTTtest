#if !FEATURE_LWIP
    #error [NOT_SUPPORTED] LWIP not supported for this target
#endif

#include "mbed.h"
#include "EthernetInterface.h"
#include "TCPServer.h"
#include "TCPSocket.h"

#include "mqtt/mqtt.hpp"

int main()
{
    printf( "MQTT Publish exsample.\n" );
    
    EthernetInterface eth;
    eth.set_network( "192.168.24.100", "255.255.255.0", "192.168.24.1" );
    int result = 0;
    if( (result = eth.connect()) == NSAPI_ERROR_OK ){
    	printf( "Ethernet Linkup.\n");
    }
    else {
    	printf( "Ethernet Linkup failed. ErrorCode: %d\n", result );
    }
    
    printf("The target IP address is '%s'\n", eth.get_ip_address());

    mqtt::Publisher publisher( "hogeisan", eth, SocketAddress("192.168.24.128", 1883) );
    mqtt::PubData data = { "topic/greeting", "Hello World!" };
    Timer timer;
    timer.start();

    while (true) {
    	if( timer.read_ms() > 1000 ){
    		publisher.Publish( data );
    		timer.reset();
    	}
    }
}
