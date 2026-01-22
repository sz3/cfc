package org.cimbar.camerafilecopy;

import android.Manifest;
import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Bundle;

import androidx.annotation.Nullable;
import androidx.core.app.ActivityCompat;

import android.util.Log;
import android.view.GestureDetector;
import android.view.MotionEvent;
import android.view.SurfaceView;
import android.view.WindowManager;
import android.widget.CompoundButton;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.core.view.GestureDetectorCompat;

import org.opencv.android.BaseLoaderCallback;
import org.opencv.android.CameraBridgeViewBase;
import org.opencv.android.CameraBridgeViewBase.CvCameraViewFrame;
import org.opencv.android.CameraBridgeViewBase.CvCameraViewListener2;
import org.opencv.android.LoaderCallbackInterface;
import org.opencv.android.OpenCVLoader;
import org.opencv.core.Mat;

import java.io.File;
import java.io.FileInputStream;
import java.io.InputStream;
import java.io.OutputStream;

public class MainActivity extends Activity implements CvCameraViewListener2 {
    private static final String TAG = "MainActivity";
    private static final int CAMERA_PERMISSION_REQUEST = 1;
    private static final int CREATE_FILE = 11;

    private GestureDetectorCompat mDetector;
    private Toast introToast;

    private CameraBridgeViewBase mOpenCvCameraView;
    private ModeSelToggle mModeSwitch;
    private int modeVal = 0;
    private int detectedMode = 68;
    private String dataPath;
    private String activePath;

    private BaseLoaderCallback mLoaderCallback = new BaseLoaderCallback(this) {
        @Override
        public void onManagerConnected(int status) {
            if (status == LoaderCallbackInterface.SUCCESS) {
                Log.i(TAG, "OpenCV loaded successfully");

                // Load native library after(!) OpenCV initialization
                System.loadLibrary("cfc-cpp");

                mOpenCvCameraView.enableView();
            } else {
                super.onManagerConnected(status);
            }
        }
    };

    @Override
    public void onCreate(Bundle savedInstanceState) {
        Log.i(TAG, "called onCreate");
        super.onCreate(savedInstanceState);

        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);

        // Permissions for Android 6+
        ActivityCompat.requestPermissions(
                this,
                new String[]{Manifest.permission.CAMERA},
                CAMERA_PERMISSION_REQUEST
        );

        this.dataPath = this.getFilesDir().getPath();

        setContentView(R.layout.activity_main);
        mOpenCvCameraView = findViewById(R.id.main_surface);
        mOpenCvCameraView.setVisibility(SurfaceView.VISIBLE);
        mOpenCvCameraView.setCvCameraViewListener(this);

        mModeSwitch = (ModeSelToggle) findViewById(R.id.mode_switch);
        mModeSwitch.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
            public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
                if (isChecked) {
                    modeVal = detectedMode;
                } else {
                    modeVal = 0;
                }
            }
        });

        // Set up the swipe gestures
        mDetector = new GestureDetectorCompat(this, new FlingGestureListener());

        // and the hint toast
        introToast = Toast.makeText(this, "↕ Swipe to encode data! Or use cimbar.org :)",  Toast.LENGTH_LONG);
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults) {
        if (requestCode == CAMERA_PERMISSION_REQUEST) {
            if (grantResults.length > 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                mOpenCvCameraView.setCameraPermissionGranted();
            } else {
                String message = "Camera permission was not granted";
                Log.e(TAG, message);
                Toast.makeText(this, message, Toast.LENGTH_LONG).show();
            }
        } else {
            Log.e(TAG, "Unexpected permission request");
        }
    }

    @Override
    public void onStart() {
        super.onStart();
        introToast.show();
    }

    @Override
    public void onPause() {
        shutdownJNI();
        super.onPause();
        if (mOpenCvCameraView != null)
            mOpenCvCameraView.disableView();
    }

    @Override
    public void onResume() {
        super.onResume();
        if (!OpenCVLoader.initDebug()) {
            Log.d(TAG, "Internal OpenCV library not found. Using OpenCV Manager for initialization");
            OpenCVLoader.initAsync(OpenCVLoader.OPENCV_VERSION, this, mLoaderCallback);
        } else {
            Log.d(TAG, "OpenCV library found inside package. Using it!");
            mLoaderCallback.onManagerConnected(LoaderCallbackInterface.SUCCESS);
        }
    }

    @Override
    public void onDestroy() {
        shutdownJNI();
        super.onDestroy();
        if (mOpenCvCameraView != null)
            mOpenCvCameraView.disableView();
    }

    @Override
    public void onCameraViewStarted(int width, int height) {
    }

    @Override
    public void onCameraViewStopped() {
    }

    @Override
    public Mat onCameraFrame(CvCameraViewFrame frame) {
        // get current camera frame as OpenCV Mat object
        Mat mat = frame.rgba();

        // native call to process current camera frame
        String res = processImageJNI(mat.getNativeObjAddr(), this.dataPath, this.modeVal);

        // res will contain a file path if we completed a transfer. Ask the user where to save it
        if (res.startsWith("/")) {
            if (res.length() == 2 && res.charAt(1) == '4') {
                detectedMode = 4;
            }
            else if (res.length() == 3 && res.charAt(1) == '6' && res.charAt(2) == '7') {
                detectedMode = 67;
            }
            else {
                detectedMode = 68;
            }
            runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    mModeSwitch.setChecked(true);
                    mModeSwitch.setModeVal(detectedMode);
                }
            });

        }
        else if (!res.isEmpty()) {
            Intent intent = new Intent(Intent.ACTION_CREATE_DOCUMENT);
            intent.addCategory(Intent.CATEGORY_OPENABLE);
            intent.setType("application/octet-stream");
            intent.putExtra(Intent.EXTRA_TITLE, res);
            // can't get putExtra to work for extra values, so we'll save it in the class
            this.activePath = this.dataPath + "/" + res;
            startActivityForResult(intent, CREATE_FILE);
        }

        // return processed frame for live preview
        return mat;
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, @Nullable Intent data) {
        if (resultCode == RESULT_OK && requestCode == CREATE_FILE) {
            if (this.activePath == null)
                return;

            // copy this.activePath (tempfile) to the user-specified location
            try (
                    InputStream istream = new FileInputStream(this.activePath);
                    OutputStream ostream = getContentResolver().openOutputStream(data.getData())
            ) {
                byte[] buf = new byte[8192];
                int length;
                while ((length = istream.read(buf)) > 0) {
                    ostream.write(buf, 0, length);
                }
                ostream.flush();
            } catch (Exception e) {
                Log.e(TAG, "failed to write file " + e.toString());
            } finally {
                try {
                    new File(this.activePath).delete();
                } catch (Exception e) {}
                this.activePath = null;
            }
        }
    }

    private native String processImageJNI(long mat, String path, int modeInt);
    private native void shutdownJNI();

    @Override
    public boolean onTouchEvent(MotionEvent event){
        if (this.mDetector.onTouchEvent(event)) {
            return true;
        }
        return super.onTouchEvent(event);
    }
    @SuppressLint("ClickableViewAccessibility")
    class FlingGestureListener extends GestureDetector.SimpleOnGestureListener {
        private static final String TAG = "Gestures";

        // We only want fling gestures to trigger the view transitions, not scrolling.
        @Override
        public boolean onFling(MotionEvent event1, MotionEvent event2,
                               float velocityX, float velocityY) {
            final int THRESHOLD = 100;
            final int VEL_THRESHOLD = 100;

            if (Math.abs(velocityY) < VEL_THRESHOLD)
                return false;
            if (Math.abs(event1.getY() - event2.getY()) < THRESHOLD)
                return false;
            if (mOpenCvCameraView != null)
                mOpenCvCameraView.disableView();
            if (introToast != null)
                introToast.cancel();

            Intent intent = new Intent(MainActivity.this, WebViewActivity.class);
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            startActivity(intent);
            return true;
        }
    }

}

