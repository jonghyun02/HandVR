@echo off
setlocal

set "JAVA_HOME=C:\Program Files\Microsoft\jdk-21.0.10.7-hotspot"
set "ANDROID_HOME=C:\Users\jongh\AppData\Local\Android\Sdk"
set "ANDROID_SDK_ROOT=%ANDROID_HOME%"
set "NDKROOT=%ANDROID_HOME%\ndk\27.2.12479018"
set "NDK_ROOT=%NDKROOT%"
set "PATH=%JAVA_HOME%\bin;%ANDROID_HOME%\platform-tools;%PATH%"

set "UE_ROOT=C:\Program Files\Epic Games\UE_5.7"
set "PROJECT=C:\projects\UnrealEngine\RubberHandVR_W5\HandTrackingDemo.uproject"

echo [APK] JAVA_HOME=%JAVA_HOME%
echo [APK] NDKROOT=%NDKROOT%
echo [APK] PROJECT=%PROJECT%
echo [APK] starting RunUAT BuildCookRun (no deploy)...

call "%UE_ROOT%\Engine\Build\BatchFiles\RunUAT.bat" ^
  BuildCookRun ^
  -project="%PROJECT%" ^
  -platform=Android ^
  -clientconfig=Development ^
  -targetplatform=Android ^
  -cookflavor=ASTC ^
  -build -cook -stage -package -pak ^
  -utf8output

set EXITCODE=%ERRORLEVEL%
echo [APK] RunUAT exit code: %EXITCODE%
endlocal & exit /b %EXITCODE%
