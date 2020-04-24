//package com.example.nativeopencvandroidtemplate;
//
//import android.Manifest;
//import android.app.Activity;
//import android.content.pm.PackageManager;
//import android.os.Bundle;
//import android.support.v4.app.ActivityCompat;
//import android.util.Log;
//import android.view.SurfaceView;
//import android.view.WindowManager;
//import android.widget.Toast;
//
//import org.jetbrains.annotations.NotNull;
//import org.opencv.android.BaseLoaderCallback;
//import org.opencv.android.CameraBridgeViewBase;
//import org.opencv.android.CameraBridgeViewBase.CvCameraViewFrame;
//import org.opencv.android.CameraBridgeViewBase.CvCameraViewListener2;
//import org.opencv.android.LoaderCallbackInterface;
//import org.opencv.android.OpenCVLoader;
//import org.opencv.core.Mat;
//
//public class MainActivity extends Activity implements CvCameraViewListener2 {
//    private static final String TAG = "MainActivity";
//    private static final int CAMERA_PERMISSION_REQUEST = 1;
//
//    private CameraBridgeViewBase mOpenCvCameraView;
//
//    private BaseLoaderCallback mLoaderCallback = new BaseLoaderCallback(this) {
//        @Override
//        public void onManagerConnected(int status) {
//            if (status == LoaderCallbackInterface.SUCCESS) {
//                Log.i(TAG, "OpenCV loaded successfully");
//
//                // Load native library after(!) OpenCV initialization
//                System.loadLibrary("native-lib");
//
//                mOpenCvCameraView.enableView();
//            } else {
//                super.onManagerConnected(status);
//            }
//        }
//    };
//
//    @Override
//    public void onCreate(Bundle savedInstanceState) {
//        Log.i(TAG, "called onCreate");
//        super.onCreate(savedInstanceState);
//        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
//
//        // Permissions for Android 6+
//        ActivityCompat.requestPermissions(
//                this,
//                new String[]{Manifest.permission.CAMERA},
//                CAMERA_PERMISSION_REQUEST
//        );
//
//        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
//        setContentView(R.layout.activity_main);
//
//        mOpenCvCameraView = findViewById(R.id.main_surface);
//
//        mOpenCvCameraView.setVisibility(SurfaceView.VISIBLE);
//
//        mOpenCvCameraView.setCvCameraViewListener(this);
//    }
//
//    @Override
//    public void onRequestPermissionsResult(int requestCode, @NotNull String[] permissions, @NotNull int[] grantResults) {
//        if (requestCode == CAMERA_PERMISSION_REQUEST) {
//            if (grantResults.length > 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
//                mOpenCvCameraView.setCameraPermissionGranted();
//            } else {
//                String message = "Camera permission was not granted";
//                Log.e(TAG, message);
//                Toast.makeText(this, message, Toast.LENGTH_LONG).show();
//            }
//        } else {
//            Log.e(TAG, "Unexpected permission request");
//        }
//    }
//
//    @Override
//    public void onPause() {
//        super.onPause();
//        if (mOpenCvCameraView != null)
//            mOpenCvCameraView.disableView();
//    }
//
//    @Override
//    public void onResume() {
//        super.onResume();
//        if (!OpenCVLoader.initDebug()) {
//            Log.d(TAG, "Internal OpenCV library not found. Using OpenCV Manager for initialization");
//            OpenCVLoader.initAsync(OpenCVLoader.OPENCV_VERSION, this, mLoaderCallback);
//        } else {
//            Log.d(TAG, "OpenCV library found inside package. Using it!");
//            mLoaderCallback.onManagerConnected(LoaderCallbackInterface.SUCCESS);
//        }
//    }
//
//    @Override
//    public void onDestroy() {
//        super.onDestroy();
//        if (mOpenCvCameraView != null)
//            mOpenCvCameraView.disableView();
//    }
//
//    @Override
//    public void onCameraViewStarted(int width, int height) {
//    }
//
//    @Override
//    public void onCameraViewStopped() {
//    }
//
//    @Override
//    public Mat onCameraFrame(CvCameraViewFrame frame) {
//        // get current camera frame as OpenCV Mat object
//        Mat mat = frame.gray();
//
//        // native call to process current camera frame
//        adaptiveThresholdFromJNI(mat.getNativeObjAddr());
//
//        // return processed frame for live preview
//        return mat;
//    }
//
//    private native void adaptiveThresholdFromJNI(long mat);
//}
