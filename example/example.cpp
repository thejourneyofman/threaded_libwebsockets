///** --------------------------------------------------------------------------
// *  HuobiConnHelper.cpp
// *
// *  A real world example to implement a threaded socket client for just-in-time
// *  market data from an crypto exchange.
// *
// *  Author    : Chase (Chao) Xu
// *  Copyright : 2021
// *  REF: https://github.com/hbdmapi/huobi_swap_Cpp
// * -------------------------------------------------------------------------- 
// **/

#include <string.h>
#include "libwebsockets.h"
#include <mutex>
#include <thread>
#include <zlib.h>

#define MAX_BUFF_SIZE 4096 * 100

static std::mutex mutex;

//REF: https://github.com/hbdmapi/huobi_swap_Cpp/blob/master/src/Utils/gzip.cpp
int gzDecompress(const char* src, int srcLen, const char* dst, int dstLen) {
	z_stream strm;
	strm.zalloc = NULL;
	strm.zfree = NULL;
	strm.opaque = NULL;

	strm.avail_in = srcLen;
	strm.avail_out = dstLen;
	strm.next_in = (Bytef*)src;
	strm.next_out = (Bytef*)dst;

	int err = -1, ret = -1;
	err = inflateInit2(&strm, MAX_WBITS + 16);
	if (err == Z_OK) {
		err = inflate(&strm, Z_FINISH);
		if (err == Z_STREAM_END) {
			ret = strm.total_out;
		}
		else {
			inflateEnd(&strm);
			return err;
		}
	}
	else {
		inflateEnd(&strm);
		return err;
	}
	inflateEnd(&strm);
	return err;
}

class HuobiConnHelper {

public:
	static HuobiConnHelper* getInstance()
	{

		if (NULL == singleton)
		{

			mutex.lock();
			if (NULL == singleton)
			{
				singleton = new HuobiConnHelper;
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
		
		switch (reason)
		{
		case LWS_CALLBACK_CLIENT_ESTABLISHED:
		{
			// sent to subscribe a symbol
			const char sendMsg[128] = "{\"sub\":\"market.BTC-USD.kline.1min\",\"id\":\"1330176468\"}"; // a sample subscribed symbol
			unsigned char buf[LWS_PRE + 128];
			memset(&buf[LWS_PRE], 0, 128);
			strncpy((char*)buf + LWS_PRE, sendMsg, 128);
			printf("Established: \"%s\".\n", sendMsg);
			lws_write(wsi, &buf[LWS_PRE], 128, LWS_WRITE_TEXT);
			break;
		}
		case LWS_CALLBACK_CLIENT_RECEIVE:
		{
			std::string message;
			char buf[MAX_BUFF_SIZE] = { 0 };
			unsigned int l = MAX_BUFF_SIZE;
			l = gzDecompress((char*)in, len, buf, l);
			message.assign(buf);
			std::string::size_type position = message.find("ping");
			// sent by a ping-pong response
			if (position != std::string::npos) {
				message.replace(position + 1, 1, "o");
				printf("Sent: %s.\n", message.c_str());
				unsigned char pongStr[LWS_PRE + 128];

				memset(&pongStr[LWS_PRE], 0, 128);
				strncpy((char*)pongStr + LWS_PRE, message.c_str(), 128);
				lws_write(wsi, &pongStr[LWS_PRE], 128, LWS_WRITE_TEXT);
			} else {
				printf("Received: %s.\n", message.c_str());
			}
			break;
		}			
		case LWS_CALLBACK_CLIENT_WRITEABLE:
			break;
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

	// The registered protocols
	inline static const struct lws_protocols protocols[] = {
	   {
		   "huobi-protocol",
		   HuobiConnHelper::eventCallback,
		   0,
		   MAX_BUFF_SIZE
	   },
	   { NULL, NULL, 0 } // Always needed at the end
	};

	// attach a context to the callback function
	static void
		attachToEventCallback(struct lws_context* context, int tsi, void* opaque)
	{
		struct lws_client_connect_info clientConnectInfo;
		memset(&clientConnectInfo, 0, sizeof(clientConnectInfo));
		clientConnectInfo.context = context;
		clientConnectInfo.address = "api.hbdm.vn";
		clientConnectInfo.port = 80;
		clientConnectInfo.path = "/swap-ws";
		clientConnectInfo.protocol = "ws";
		clientConnectInfo.origin = "origin";
		clientConnectInfo.host = clientConnectInfo.address;
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
	static inline HuobiConnHelper* singleton = NULL;

	// constructor that should be defined as PRIVATE
	HuobiConnHelper() {

		lws_set_log_level(LLL_ERR | LLL_WARN, lwsl_emit_syslog);
		context_ = NULL;
		memset(&ctxCreationInfo_, 0, sizeof(ctxCreationInfo_));
		ctxCreationInfo_.port = CONTEXT_PORT_NO_LISTEN;
		ctxCreationInfo_.protocols = HuobiConnHelper::protocols;
		ctxCreationInfo_.system_ops = &ops;
	}

};

// TEST threads
void thread01()
{
	struct lws_context* ctx = HuobiConnHelper::getInstance()->getContext();
	lws_system_get_ops(ctx)->attach(ctx, 0, HuobiConnHelper::getInstance()->attachToEventCallback,
		LWS_SYSTATE_OPERATIONAL,
		NULL, NULL);
	while (1) {
		HuobiConnHelper::getInstance()->run(1);
	}

}

// simulate an Exception in Thread 02
void thread02()
{
	struct lws_context* ctx = HuobiConnHelper::getInstance()->getContext();
	lws_system_get_ops(ctx)->attach(ctx, 0, HuobiConnHelper::getInstance()->attachToEventCallback,
		LWS_SYSTATE_OPERATIONAL,
		NULL, NULL);
	int i = 0;
	int min = 2;
	int max = 21;
	int triggerNum = (rand() % (max - min)) + min;

	while (1) {
		i++;
		HuobiConnHelper::getInstance()->run(2);
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
	struct lws_context* ctx = HuobiConnHelper::getInstance()->getContext();
	lws_system_get_ops(ctx)->attach(ctx, 0, HuobiConnHelper::getInstance()->attachToEventCallback,
		LWS_SYSTATE_OPERATIONAL,
		NULL, NULL);
	int i = 0;
	int min = 22;
	int max = 41;
	int triggerNum = (rand() % (max - min)) + min;
	while (1) {
		i++;
		HuobiConnHelper::getInstance()->run(3);
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
	HuobiConnHelper::getInstance()->generate();
	std::thread thread1(thread01);
	std::thread thread2(thread02);
	std::thread thread3(thread03);
	// main thread
	while (1) {
		//run
		HuobiConnHelper::getInstance()->run(0);
	}
	thread1.join();
	thread2.join();
	thread3.join();
	//terminate
	HuobiConnHelper::getInstance()->terminate();

	return 0;
}