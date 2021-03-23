# threaded_libwebsockets
Threaded  libwebsockets clients in a single connection

#### Purpose
A simple example of how to implement a threaded libwebsocket client that can share context from different threads so that it can reduce the memory usage and keep running in a thread-safe way. It also solved a popular issue how libwebsocket_callback_on_writable event can trigger callback LWS_CALLBACK_SERVER_WRITEABLE when called from a different thread as well as an error handling mechanism to keep the connection up if other threads throw an exception in their processing.

#### Test Enviroment
##### msvc
##### c++17
##### websockets.lib (latest)
##### libssl.lib  (latest)

#### Example
A real world example to implement a threaded socket client for just-in-time market data from an crypto exchange. To prevent from maintaining the safe and redundancy connections to a market data server by creating connection pools where one-thread-one-connection way overuses memory usage from idling connections, this sample showcases a different maintenance way that could create scale-free threads on the demand of necessity so threads can share the same context and connection resources, for an instance, we can have two or more watchdog threads to monitor and reconnect in case of uncertainties which will not pay double or more memory costs.

#### How to run
##### go to root or example directories
##### run ```cmake .```
