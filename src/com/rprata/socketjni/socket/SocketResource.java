package com.rprata.socketjni.socket;

import java.io.FileDescriptor;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;

import android.net.LocalSocket;
import android.net.LocalSocketAddress;
import android.util.Log;

public class SocketResource {

	private static final String TAG = "Socket Resource";

	/** Native methods, implemented in jni folder */
	private native void nativeStartLocalServer(String name, String filename) throws Exception;
	private native FileDescriptor nativeStartLocalClient(String name);

	public void startLocalServer(String name, String filename) throws Exception {
		nativeStartLocalServer(name, filename);
	}
	
	public FileDescriptor startLocalClient2(String name) throws Exception {
		return (FileDescriptor) nativeStartLocalClient(name);
	}

	static {
		System.loadLibrary("socket");
	}


	public void startServer(String filename) {
		ServerTask serverTask = new ServerTask("adtv", filename);
		serverTask.start();
	}
	
	public void startReader(String name, FileDescriptor descriptor) {
		ClientTask clientTask = new ClientTask(name, descriptor);
		clientTask.start();
	}

	private boolean isFilesystemSocket(String name) {
		return name.startsWith("/");
	}

	private void startLocalClient(String name, String message) throws Exception {
		// Construct a local socket
		LocalSocket clientSocket = new LocalSocket();
		try {
			// Set the socket namespace
			LocalSocketAddress.Namespace namespace;
			if (isFilesystemSocket(name)) {
				namespace = LocalSocketAddress.Namespace.FILESYSTEM;
			} else {
				namespace = LocalSocketAddress.Namespace.ABSTRACT;
			}

			// Construct local socket address
			LocalSocketAddress address = new LocalSocketAddress(name, namespace);

			// Connect to local socket
			Log.i(TAG, "Connecting to " + name);		
			clientSocket.connect(address);
			Log.i(TAG, "Connected.");

			// Get message as bytes
			byte[] messageBytes = message.getBytes();

			// Send message bytes to the socket
			Log.i(TAG, "Sending to the socket...");
			OutputStream outputStream = clientSocket.getOutputStream();
			outputStream.write(messageBytes);
			Log.i(TAG, String.format("Sent %d bytes: %s",messageBytes.length, message));

			// Receive the message back from the socket
			Log.i(TAG, "Receiving from the socket...");
			InputStream inputStream = clientSocket.getInputStream();
			int readSize = inputStream.read(messageBytes);

			String receivedMessage = new String(messageBytes, 0, readSize);
			Log.i(TAG, String.format("Received %d bytes: %s", 
					readSize, receivedMessage));

			// Close streams
			outputStream.close();
			inputStream.close();

		} finally { 
			// Close the local socket
			clientSocket.close();
		}
	}

	public class ServerTask extends Thread {
		/** Socket name. */
		private final String name;
		private final String filename;


		public ServerTask(String name, String filename) {
			this.name = name;
			this.filename = filename;
		}

		public void run() {
			Log.i(TAG, "Starting server.");

			try {
				startLocalServer(name, filename);
			} catch (Exception e) {
				Log.e(TAG, e.getMessage());
			}

			Log.i(TAG, "Server terminated.");
		}
	}

	/**
	 * Client task.
	 */
	public class ClientTask extends Thread {

		private final String name;
		private final FileDescriptor descriptor;
			
		public ClientTask(String name, FileDescriptor descriptor) {
			super();
			this.name = name;		
			this.descriptor = descriptor;
		}

		public void run() {
			Log.i(TAG, "Starting client.");

			try {
				
				if (descriptor == null)
					Log.i(TAG, "eh nulo");
				
//				FileInputStream fileInputStream = new FileInputStream(descriptor);
				int content;
				byte buffer[] = new byte[20*188];
				
//				while ((content = fileInputStream.read(buffer)) != -1)
//				{ 
//					StringBuilder sb = new StringBuilder();
//					for (int i = 0; i < buffer.length; i++) {
//						sb.append(buffer[i]);
//					}
//					Log.i(TAG, sb.toString());
//				}
				
			} catch (Exception e) {
				Log.i(TAG, e.getMessage());
			}

			Log.i(TAG, "Client terminated.");
		}
	}

}
