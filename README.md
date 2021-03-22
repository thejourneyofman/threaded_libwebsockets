# threaded_libwebsockets
Threaded  libwebsockets clients in a single connection

#### Purpose
A simple example of how to implement a threaded libwebsocket client that can share context from different threads so that it can reduce the memory usage and keep running in a thread-safe way. It also solved a popular issue how libwebsocket_callback_on_writable event can trigger callback LWS_CALLBACK_SERVER_WRITEABLE when called from a different thread as well as an error handling mechanism to keep the connection up if other threads throw an exception in their processing.

#### Test Enviroment
##### msvc
##### c++17
##### websockets.lib (latest)
##### libssl.lib  (latest)