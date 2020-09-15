### [INTRODUCTION](https://github.com/sz3/cimbar) | [ABOUT](https://github.com/sz3/cimbar/blob/master/ABOUT.md) | CFC | [LIBCIMBAR](https://github.com/sz3/libcimbar)

## on cfc

This app is, essentially, a testbed for [libcimbar](https://github.com/sz3/libcimbar). It *can* and *does* allow one to receive files over the phone camera, but it is not a user-focused app, just a proof of the concept. The UI is nonexistent, and the only feedback given to the user is whether the onscreen numbers are increasing.

Nearly all the interesting logic is in libcimbar -- included via a git subtree. "cfc" does not do much on its own -- though, sometimes it feels as if doing *anything* nontrivial in an Android app is an accomplishment in and of itself.

## how do I build?

The $10,000 question of android development.

You need android studio, the android ndk, opencv for android, cmake, and probably some alcohol to make the process more palatable.

This repo was incredibly helpful for me when getting started:

https://github.com/VlSomers/native-opencv-android-template

... its instructions are better than anything I could write up. A godsend. 5 stars. Would build prototype app off this repo again.

... short of that, I recommend perseverance, patience, prayer, and an occasional string of profanities directed at google and android. Usually one of those solves the problem. (probably the profanity)

## licensing, dependencies, etc

The code in cfc, such as it is, is MIT licensed.

The libcimbar code is MPL 2.0. libcimbar's dependencies are a variety of MIT, BSD, zlib, boost, apache, ...

## on camera apis

* The app is built off the opencv tutorial apps, and includes a way to use the camera api (the default), or, if built to use `OpencvCamera2View`, the camera2 api. Unfortunately, the camera2 api wrapper code (borrowed from opencv) is very inefficient about pulling down preview images, and as a result has significant framerate problems.

* cameraX is an option, but I lost patience trying to get it set up. It's also in "beta", and has been for years. So there is that dark cloud over its head...

* there are some 3rd party android camera libraries I wanted to look into, but haven't made the time to yet. Might be reasonable?


## on using the GPU for performance speedups

* It is a good idea. Some of the lengthiest operations in a decode are the preprocessing step on the scan (adaptive threshold) and the extraction of the cimbar frame from the image (perspective transform). There is also an optional sharpening filter that can run pre-decode --currently disabled in cfc. These all see radical speedups when run on the GPU. So why are we not using the GPU?
* libcimbar uses OpenCV. For our purposes (Android), OpenCV nominally supports doing operations on the GPU via openCL, and its "TAPI" -- in C++, the cv::UMat class.
	* (this requires a custom OpenCV build. The official OpenCV android .so is built with openCL support disabled.)
* Unfortunately, the runtime cost of copying data from UMat (GPU) -> Mat (CPU) is prohibitive.
	* observed an average of ~90ms for a 1024x1024 image on an opencl-enabled opencv 4.3 build
	* AFAIK, it is theoretically possible to get an openCL buffer onto the CPU without a long delay
		* the general pattern would be the OpenGL PBO (physical buffer object) idea, or something along those lines
		* the buffer copy would happen in the background, freeing up system resources to do other things
		* I don't have any code that does this... yet
