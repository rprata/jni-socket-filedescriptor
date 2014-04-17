package com.rprata.socketjni;

import java.io.File;
import java.io.FileDescriptor;
import java.io.FileInputStream;

import android.app.Activity;
import android.os.Bundle;
import android.os.Environment;
import android.util.Log;
import android.view.Menu;
import android.widget.MediaController;

import com.rprata.socketjni.myvideoview.VideoView;
import com.rprata.socketjni.socket.SocketResource;

public class Main extends Activity {
	private VideoView contentView;
	
	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		setContentView(R.layout.main);
		
		contentView = (VideoView) findViewById(R.id.videoview1);
	}

	@Override
	public boolean onCreateOptionsMenu(Menu menu) {
		// Inflate the menu; this adds items to the action bar if it is present.
		getMenuInflater().inflate(R.menu.main, menu);
				
		return true;
	}
	private static final String TAG = "MAIN";
	@Override
	protected void onStart() {
		super.onStart();
		SocketResource resource = new SocketResource();
		String baseDir = Environment.getExternalStorageDirectory().getAbsolutePath();
//		String fileName = "captura_2_service.ts";
		//		String fileName = "globo.ts";
		String fileName = "NativeMedia.ts";
		String path  = baseDir + "/" + fileName;
		
		resource.startServer(path);
		
		try {
			Thread.sleep(1);
			FileDescriptor fd = resource.startLocalClient2("adtv");
			if (fd != null) {
				Log.i(TAG, "filedescriptor is not null");
				resource.startReader("adtv", fd);
				contentView.setMediaController(new MediaController(this));
				
//				contentView.setVideoFD((new FileInputStream(new File(path)).getFD()));
				contentView.setVideoFD(fd);
				contentView.start();
			} else {
				Log.i(TAG, "filedescriptor is null");
			}
			
			
		} catch (Exception e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
	
	}

}
