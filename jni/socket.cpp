#include <jni.h>
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stddef.h>

#include <android/log.h>

#define MAX_LOG_MESSAGE_LENGTH 256
#define MAX_BUFFER_SIZE 80

// number of buffers in our buffer queue, an arbitrary number
#define NB_BUFFERS 16

// we're streaming MPEG-2 transport stream data, operate on transport stream block size
#define MPEG2_TS_PACKET_SIZE 188

// number of MPEG-2 transport stream blocks per buffer, an arbitrary number
#define PACKETS_PER_BUFFER 20

// determines how much memory we're dedicating to memory caching
#define BUFFER_SIZE (PACKETS_PER_BUFFER*MPEG2_TS_PACKET_SIZE)

static char videoBuffer[BUFFER_SIZE * NB_BUFFERS];

#define TAG "SocketJNI"

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__)

static int NewLocalSocket(JNIEnv* env, jobject obj);
static void ThrowException(JNIEnv * env, const char* className, const char* message);
static void ThrowErrnoException(JNIEnv * env, const char* className, int errnum);
static void BindLocalSocketToName(JNIEnv* env, jobject obj, int sd, const char* name);
static unsigned short GetSocketPort(JNIEnv* env, jobject obj, int sd);
static void ListenOnSocket(JNIEnv* env, jobject obj, int sd, int backlog);
static void LogAddress(JNIEnv* env, jobject obj, const char* message, const struct sockaddr_in* address);
static int AcceptOnLocalSocket(JNIEnv* env, jobject obj, int sd);
static ssize_t ReceiveFromSocket(JNIEnv* env, jobject obj, int sd, char* buffer, size_t bufferSize);
static ssize_t SendToSocket(JNIEnv* env, jobject obj, int sd, const char* buffer, size_t bufferSize);
static void ConnectToLocalAddress(JNIEnv* env, jobject obj, int sd, const char * name);

extern "C" {
        JNIEXPORT void JNICALL Java_com_rprata_socketjni_socket_SocketResource_nativeStartLocalServer(JNIEnv* env, jobject obj, jstring name, jstring filename);
        JNIEXPORT jobject JNICALL Java_com_rprata_socketjni_socket_SocketResource_nativeStartLocalClient(JNIEnv* env, jobject obj, jstring name);
};

jobject Java_com_rprata_socketjni_socket_SocketResource_nativeStartLocalClient(JNIEnv* env, jobject obj, jstring name)
{

	int clientSocket = NewLocalSocket(env, obj);

	if (NULL == env->ExceptionOccurred())
	{
		const char * nameText = env->GetStringUTFChars(name, NULL);
		if (NULL == nameText)
			goto exit;

		// Connect to IP address and port
		ConnectToLocalAddress(env, obj, clientSocket, nameText);
		// Release the IP address
		env->ReleaseStringUTFChars(name, nameText);

		// If connection was successful
		if (NULL != env->ExceptionOccurred())
			goto exit;
 		
 		// jobject fdObject = jniCreateFileDescriptor(env, clientSocket);
		jclass class_fdesc = env->FindClass("java/io/FileDescriptor");
		
		jmethodID const_fdesc = env->GetMethodID(class_fdesc, "<init>", "()V");
		if (const_fdesc == NULL) return NULL;
		
		jobject fdObject = env->NewObject(class_fdesc, const_fdesc);
		
		jfieldID field_fd = env->GetFieldID(class_fdesc, "descriptor", "I");
		
		if (field_fd == NULL) return NULL;
		
		env->SetIntField(fdObject, field_fd, clientSocket);

		return fdObject;
	}
		
exit:
	if (clientSocket > 0)
	{
		close(clientSocket);
	}
 
 	return NULL;

}

void Java_com_rprata_socketjni_socket_SocketResource_nativeStartLocalServer(JNIEnv* env, jobject obj, jstring name, jstring filename)
{
	// Construct a new local UNIX socket.
	int serverSocket = NewLocalSocket(env, obj);

	if (NULL == env->ExceptionOccurred())
	{
		// Get filename from String class
		const char * utf8 = env->GetStringUTFChars(filename, NULL);

	    FILE * file = fopen(utf8, "rb");
	    if (file == NULL)
	    {
	    	LOGI("Erro ao abrir o arquivo");
	    	goto exit;
	    }

	   	env->ReleaseStringUTFChars(filename, utf8);

		// Get name as C string
		const char * nameText = env->GetStringUTFChars(name, NULL);
		if (NULL == nameText)
			goto exit;

		// Bind socket to a port number
		BindLocalSocketToName(env, obj, serverSocket, nameText);

		// Release the name text
		env->ReleaseStringUTFChars(name, nameText);

		// If bind is failed
		if (NULL != env->ExceptionOccurred())
			goto exit;

		// Listen on socket with a backlog of 4 pending connections
		ListenOnSocket(env, obj, serverSocket, 4);
		if (NULL != env->ExceptionOccurred())
			goto exit;

		// Accept a client connection on socket
		int clientSocket = AcceptOnLocalSocket(env, obj, serverSocket);
		if (NULL != env->ExceptionOccurred())
			goto exit;

		char buffer[MAX_BUFFER_SIZE];
		ssize_t recvSize;
		ssize_t sentSize;

		size_t bytesRead;
		
		// Receive and send back the data
		while ((bytesRead = fread(videoBuffer, 1, BUFFER_SIZE, file)) > 0)
		{
			sentSize = SendToSocket(env, obj, clientSocket,
					videoBuffer, bytesRead);
		}

		// Close the client socket
		close(clientSocket);
	}

exit:
	if (serverSocket > 0)
	{
		close(serverSocket);
	}

}

static int NewLocalSocket(JNIEnv* env, jobject obj)
{
	// Construct socket
	LOGI("Constructing a new Local UNIX socket...");
	int localSocket = socket(PF_LOCAL, SOCK_STREAM, 0);

	// Check if socket is properly constructed
	if (-1 == localSocket)
	{
		// Throw an exception with error number
		ThrowErrnoException(env, "java/io/IOException", errno);
	}

	return localSocket;
}

static void BindLocalSocketToName(JNIEnv* env, jobject obj, int sd, const char* name)
{
	struct sockaddr_un address;

	// Name length
	const size_t nameLength = strlen(name);

	// Path length is initiall equal to name length
	size_t pathLength = nameLength;

	// If name is not starting with a slash it is
	// in the abstract namespace
	bool abstractNamespace = ('/' != name[0]);

	// Abstract namespace requires having the first
	// byte of the path to be the zero byte, update
	// the path length to include the zero byte
	if (abstractNamespace)
	{
		pathLength++;
	}

	// Check the path length
	if (pathLength > sizeof(address.sun_path))
	{
		// Throw an exception with error number
		ThrowException(env, "java/io/IOException", "Name is too big.");
	}
	else
	{
		// Clear the address bytes
		memset(&address, 0, sizeof(address));
		address.sun_family = PF_LOCAL;

		// Socket path
		char* sunPath = address.sun_path;

		// First byte must be zero to use the abstract namespace
		if (abstractNamespace)
		{
			*sunPath++ = '\0';
		}

		// Append the local name
		strcpy(sunPath, name);

		// Address length
		socklen_t addressLength =
				(offsetof(struct sockaddr_un, sun_path))
				+ pathLength;

		// Unlink if the socket name is already binded
		unlink(address.sun_path);

		// Bind socket
		LOGI("Binding to local name %s%s.",
				(abstractNamespace) ? "(null)" : "",
				name);



		int so_reuseaddr = 1;
    	setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &so_reuseaddr, sizeof so_reuseaddr);
		
		if (-1 == bind(sd, (struct sockaddr*) &address, addressLength))
		{
			// Throw an exception with error number
			ThrowErrnoException(env, "java/io/IOException", errno);
		}
	}
}

static void ConnectToLocalAddress(JNIEnv* env, jobject obj, int sd, const char * name)
{
	struct sockaddr_un address;
	
	// Connecting to given name adress
	LOGI("Connecting to %s", name);

	// Name length
	const size_t nameLength = strlen(name);

	// Path length is initiall equal to name length
	size_t pathLength = nameLength;

	// If name is not starting with a slash it is
	// in the abstract namespace
	bool abstractNamespace = ('/' != name[0]);

	// Abstract namespace requires having the first
	// byte of the path to be the zero byte, update
	// the path length to include the zero byte
	if (abstractNamespace)
	{
		pathLength++;
	}

	// Check the path length
	if (pathLength > sizeof(address.sun_path))
	{
		// Throw an exception with error number
		ThrowException(env, "java/io/IOException", "Name is too big.");
	}
	else
	{
		// Clear the address bytes
		memset(&address, 0, sizeof(address));
		address.sun_family = PF_LOCAL;

		// Socket path
		char* sunPath = address.sun_path;

		// First byte must be zero to use the abstract namespace
		if (abstractNamespace)
		{
			*sunPath++ = '\0';
		}

		// Append the local name
		strcpy(sunPath, name);

		// Address length
		socklen_t addressLength =
				(offsetof(struct sockaddr_un, sun_path))
				+ pathLength;

		if (-1 == connect(sd, (struct sockaddr*) &address, addressLength))
		{
			// Throw an exception with error number
			ThrowErrnoException(env, "java/io/IOException", errno);
		}
	}
}


static int AcceptOnLocalSocket(JNIEnv* env, jobject obj, int sd)
{
	// Blocks and waits for an incoming client connection
	// and accepts it
	LOGI("Waiting for a client connection...");
	int clientSocket = accept(sd, NULL, NULL);

	// If client socket is not valid
	if (-1 == clientSocket)
	{
		// Throw an exception with error number
		ThrowErrnoException(env, "java/io/IOException", errno);
	}

	return clientSocket;
}

static void ThrowErrnoException(JNIEnv * env, const char* className, int errnum)
{
	char buffer[MAX_LOG_MESSAGE_LENGTH];

	// Get message for the error number
	if (-1 == strerror_r(errnum, buffer, MAX_LOG_MESSAGE_LENGTH))
	{
		strerror_r(errno, buffer, MAX_LOG_MESSAGE_LENGTH);
	}

	// Throw exception
	ThrowException(env, className, buffer);
}

static void ThrowException(JNIEnv* env, const char* className, const char* message)
{
	// Get the exception class
	jclass clazz = env->FindClass(className);

	// If exception class is found
	if (NULL != clazz)
	{
		// Throw exception
		env->ThrowNew(clazz, message);

		// Release local class reference
		env->DeleteLocalRef(clazz);
	}
}

static unsigned short GetSocketPort(JNIEnv* env, jobject obj, int sd)
{
	unsigned short port = 0;

	struct sockaddr_in address;
	socklen_t addressLength = sizeof(address);

	// Get the socket address
	if (-1 == getsockname(sd, (struct sockaddr*) &address, &addressLength))
	{
		// Throw an exception with error number
		ThrowErrnoException(env, "java/io/IOException", errno);
	}
	else
	{
		// Convert port to host byte order
		port = ntohs(address.sin_port);

		LOGI("Binded to random port %hu.", port);
	}

	return port;
}

static void ListenOnSocket(JNIEnv* env, jobject obj, int sd, int backlog)
{
	// Listen on socket with the given backlog
	LOGI("Listening on socket with a backlog of %d pending connections.", backlog);

	if (-1 == listen(sd, backlog))
	{
		// Throw an exception with error number
		ThrowErrnoException(env, "java/io/IOException", errno);
	}
}

static void LogAddress(JNIEnv* env, jobject obj, const char* message, const struct sockaddr_in* address)
{
	char ip[INET_ADDRSTRLEN];

	// Convert the IP address to string
	if (NULL == inet_ntop(PF_INET,
			&(address->sin_addr),
			ip,
			INET_ADDRSTRLEN))
	{
		// Throw an exception with error number
		ThrowErrnoException(env, "java/io/IOException", errno);
	}
	else
	{
		// Convert port to host byte order
		unsigned short port = ntohs(address->sin_port);

		// Log address
		LOGI("%s %s:%hu.", message, ip, port);
	}
}

static ssize_t ReceiveFromSocket(JNIEnv* env, jobject obj, int sd, char* buffer, size_t bufferSize)
{
	// Block and receive data from the socket into the buffer
	LOGI("Receiving from the socket...");
	ssize_t recvSize = recv(sd, buffer, bufferSize - 1, 0);

	// If receive is failed
	if (-1 == recvSize)
	{
		// Throw an exception with error number
		ThrowErrnoException(env, "java/io/IOException", errno);
	}
	else
	{
		// NULL terminate the buffer to make it a string
		// buffer[recvSize] = '\0';

		// If data is received
		if (recvSize > 0)
		{
			LOGI("Received %ld bytes: %s", recvSize, buffer);
		}
		else
		{
			LOGI("Client disconnected.");
		}
	}

	return recvSize;
}

static ssize_t SendToSocket(JNIEnv* env, jobject obj, int sd, const char* buffer, size_t bufferSize)
{
	// Send data buffer to the socket
	// LOGI("Sending to the socket...");
	ssize_t sentSize = send(sd, buffer, bufferSize, 0);

	// If send is failed
	if (-1 == sentSize)
	{
		// Throw an exception with error number
		ThrowErrnoException(env, "java/io/IOException", errno);
	}
	else
	{
		if (sentSize > 0)
		{
			// LOGI("Sent %ld bytes: %s", sentSize, buffer);
		}
		else
		{
			LOGI("Client disconnected.");
		}
	}

	return sentSize;
}
