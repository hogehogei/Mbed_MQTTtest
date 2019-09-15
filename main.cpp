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
    eth.connect();
    
    printf("The target IP address is '%s'\n", eth.get_ip_address());
    
    //TCPServer srv;
    TCPSocket clt_sock;
    //SocketAddress clt_addr;
    
    /* Open the server on ethernet stack */
    //srv.open(&eth);
    
    /* Bind the HTTP port (TCP 80) to the server */
    //srv.bind(eth.get_ip_address(), 1883);
    
    /* Can handle 5 simultaneous connections */
    //srv.listen(5);
    
    clt_sock.bind(eth.get_ip_address(), 1883);
    clt_sock.connect( SocketAddress( "192.168.24.128", 1883 ) );

    mqtt::Publisher publisher( "hogeisan", clt_sock );
    mqtt::PubData data = { "topic/greeting", "Hello World!" };
    Timer timer;
    timer.start();

    while (true) {
        //srv.accept(&clt_sock, &clt_addr);
        //printf("accept %s:%d\n", clt_addr.get_ip_address(), clt_addr.get_port());
    	if( timer.read_ms() > 1000 ){
    		timer.reset();
    		if( !publisher.IsConnected() ){
    			publisher.Connect();
    		}
    		else {
    			publisher.Publish( data );
    		}
    	}

        publisher.Update();
    }
}
