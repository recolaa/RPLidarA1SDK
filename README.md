To compile navigate to /workspaces/vc14/lidarReceiver
Run the command "msbuild lidarReceiver.vcxproj /p:Configuration=Release"

To run the Receiver/Sender navigate to CustomLidarSDK\workspaces\output\Win32\Release
Run the command "lidarReceiver.exe --channel --serial <com port> [baudrate]" 
EXAMPLE: "lidarReceiver.exe --channel --serial COM5 115200" 
