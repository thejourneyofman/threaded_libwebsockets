///** --------------------------------------------------------------------------
// *  LWSHelper.cpp
// *
// *  A simple example of how to implement a threaded libwebsocket client that 
// *  can share context from different threads so that it can reduce the memory
// *  usage and keep running in a thread-safe way. It also solved a popular
// *  issue how libwebsocket_callback_on_writable event can trigger callback
// *  LWS_CALLBACK_SERVER_WRITEABLE when called from a different thread as well
// *  as an error handling mechanism to keep the connection up if other threads
// *  throw an exception in their processing.
// *
// *  Author    : Chase (Chao) Xu
// *  Copyright : 2021
// *  -------------------------------------------------------------------------- 
// **/

#include <iostream>
#include <string.h>
#include "libwebsockets.h"
#include <mutex>
#include <thread>

#define MAX_BUFF_SIZE 1024

static std::mutex mutex;

class libWSHelper {

public:
	static libWSHelper* getInstance()
	{

		if (NULL == singleton)
		{

			mutex.lock();
			if (NULL == singleton)
			{
				singleton = new libWSHelper;
			}
			mutex.unlock();
		}
		return singleton;
	}

	// callback function
	static int eventCallback(struct lws* wsi,
		enum lws_callback_reasons reason,
		void* user,
		void* in,
		size_t len) {

		const char msg[MAX_BUFF_SIZE] = "{ \"echo\": \"auth\" }";
		unsigned char buf[LWS_PRE + MAX_BUFF_SIZE];
		memset(&buf[LWS_PRE], 0, MAX_BUFF_SIZE);
		strncpy((char*)buf + LWS_PRE, msg, MAX_BUFF_SIZE);

		switch (reason)
		{

		case LWS_CALLBACK_CLIENT_ESTABLISHED:
			printf("Established.\n");
			lws_write(wsi, &buf[LWS_PRE], MAX_BUFF_SIZE, LWS_WRITE_TEXT);
			break;
		case LWS_CALLBACK_CLIENT_WRITEABLE:
		{
			break;
		}
		case LWS_CALLBACK_CLIENT_RECEIVE:
		{
			printf("Received: \"%s\"\n", (char*)in);
			lws_write(wsi, &buf[LWS_PRE], MAX_BUFF_SIZE, LWS_WRITE_TEXT);
			break;
		}
		case LWS_CALLBACK_CLIENT_CLOSED:
		{
			printf("Disconnected.\n");
			lws_set_timeout(wsi, PENDING_TIMEOUT_KILLED_BY_PARENT, LWS_TO_KILL_ASYNC);
			break;
		}
		case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
			printf("Error: %s\n", in ? (char*)in : "(Unknow Error)");
			break;
		default:
			printf("Noticed: %d\n", reason);
			break;
		}

		return 0;
	};

	// attach a context to the callback function
	static void
		attachToEventCallback(struct lws_context* context, int tsi, void* opaque)
	{
		struct lws_client_connect_info clientConnectInfo;
		memset(&clientConnectInfo, 0, sizeof(clientConnectInfo));
		clientConnectInfo.context = context;
		clientConnectInfo.address = "echo.websocket.org";
		clientConnectInfo.port = 80;
		clientConnectInfo.path = "/";
		clientConnectInfo.host = clientConnectInfo.address;
		clientConnectInfo.origin = NULL;
		clientConnectInfo.ietf_version_or_minus_one = -1;
		clientConnectInfo.protocol = "ws";
		clientConnectInfo.ssl_connection = 2;
		// Connect via the client info
		lws_client_connect_via_info(&clientConnectInfo);

	}

	// A non-threadsafe helper __lws_system_attach() is provided to do the actual work inside the system-specific locking.
	// Ref: https://libwebsockets.org/lws-api-doc-v4.1-stable/html/structlws__system__ops.html
	static int
		lws_attach_in_threads(struct lws_context* context, int tsi,
			lws_attach_cb_t cb, lws_system_states_t state,
			void* opaque, struct lws_attach_item** get)
	{
		int n;
		mutex.lock();
		n = __lws_system_attach(context, tsi, cb, state, opaque, get);
		mutex.unlock();
		return n;
	}

	lws_system_ops_t ops = {
		nullptr,
		nullptr,
		lws_attach_in_threads
	};

	// registered protocols
	inline static const struct lws_protocols protocols[] = {
	   {
		   "protocol",
		   libWSHelper::eventCallback,
		   0,
		   MAX_BUFF_SIZE,
	   },
	   { NULL, NULL, 0 }
	};

	// initialize the context for ONLY once
	void generate() {
		if (NULL == context_)
			context_ = lws_create_context(&ctxCreationInfo_);
		lws_system_get_ops(context_)->attach(context_, 0, attachToEventCallback,
			LWS_SYSTATE_OPERATIONAL,
			NULL, NULL);
	}

	// return the context
	struct lws_context* getContext() { return context_; }

	// service on
	void run(const int& sid)
	{
		try {
			lws_service(context_, 50);
			printf("Thread %d is working\n", sid);
		}
		catch (std::runtime_error& e)
		{
			printf("Thread %d has a runtime error %s\n", sid, e.what());
		}
		catch (...) {
			// do nothing
		}
	}

	// terminate
	void terminate()
	{
		lws_context_destroy(context_);
	}

private:
	struct lws_context_creation_info ctxCreationInfo_; // context creation info
	struct lws_context* context_;
	static inline libWSHelper* singleton = NULL;

	// constructor that should be defined as PRIVATE
	libWSHelper() {

		lws_set_log_level(LLL_ERR | LLL_WARN, lwsl_emit_syslog);
		context_ = NULL;
		memset(&ctxCreationInfo_, 0, sizeof(ctxCreationInfo_));
		ctxCreationInfo_.port = CONTEXT_PORT_NO_LISTEN;
		ctxCreationInfo_.protocols = protocols;
		ctxCreationInfo_.system_ops = &ops;
	}

};

// TEST threads
void thread01()
{
	struct lws_context* ctx = libWSHelper::getInstance()->getContext();
	lws_system_get_ops(ctx)->attach(ctx, 0, libWSHelper::getInstance()->attachToEventCallback,
		LWS_SYSTATE_OPERATIONAL,
		NULL, NULL);
	while (1) {
		libWSHelper::getInstance()->run(1);
	}

}

// simulate an Exception in Thread 02
void thread02()
{
	struct lws_context* ctx = libWSHelper::getInstance()->getContext();
	lws_system_get_ops(ctx)->attach(ctx, 0, libWSHelper::getInstance()->attachToEventCallback,
		LWS_SYSTATE_OPERATIONAL,
		NULL, NULL);
	int i = 0;
	int min = 2;
	int max = 21;
	int triggerNum = (rand() % (max - min)) + min;

	while (1) {
		i++;
		libWSHelper::getInstance()->run(2);
		try {
			if (i == triggerNum)
				throw - 9999;
		}
		catch (...)
		{
			printf("Thread 2 exited unexpectly with trigger number %d.\n", triggerNum);
			break;
		}
	}
}

// simulate an Exception in Thread 03
void thread03()
{
	struct lws_context* ctx = libWSHelper::getInstance()->getContext();
	lws_system_get_ops(ctx)->attach(ctx, 0, libWSHelper::getInstance()->attachToEventCallback,
		LWS_SYSTATE_OPERATIONAL,
		NULL, NULL);
	int i = 0;
	int min = 22;
	int max = 41;
	int triggerNum = (rand() % (max - min)) + min;
	while (1) {
		i++;
		libWSHelper::getInstance()->run(3);
		try {
			if (i == triggerNum)
				throw - 9999;
		}
		catch (...)
		{
			printf("Thread 3 exited unexpectly with trigger number %d.\n", triggerNum);
			break;
		}
	}
}


// Main function entry
int main()
{
	libWSHelper::getInstance()->generate();
	std::thread thread1(thread01);
	std::thread thread2(thread02);
	std::thread thread3(thread03);
	// main thread
	while (1) {
		//run
		libWSHelper::getInstance()->run(0);
	}
	thread1.join();
	thread2.join();
	thread3.join();
	//terminate
	libWSHelper::getInstance()->terminate();

	return 0;
}