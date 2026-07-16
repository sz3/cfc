# Add project specific ProGuard rules here.
# You can control the set of applied configuration files using the
# proguardFiles setting in build.gradle.
#
# For more details, see
#   http://developer.android.com/guide/developing/tools/proguard.html

# If your project uses WebView with JS, uncomment the following
# and specify the fully qualified class name to the JavaScript interface
# class:
-keepclassmembers class fqcn.of.javascript.interface.for.webview {
   public *;
}

# ----------------------------------------------------
# 1. DISABLE OBFUSCATION (Keep original names)
# ----------------------------------------------------
-dontobfuscate

# ----------------------------------------------------
# 2. STANDARD JNI & OPENCV KEEP RULES
# ----------------------------------------------------

# Keep all native methods and their declaring classes from being stripped
-keepclasseswithmembernames class * {
    native <methods>;
}

# keep cfc classes
-keep class org.cimbar.** { *; }
-keepclassmembers class org.cimbar.** { *; }

# Keep OpenCV Android SDK Java wrappers from being stripped
-keep class org.opencv.** { *; }
-keepclassmembers class org.opencv.** { *; }
-dontwarn org.opencv.**
