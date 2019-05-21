rem Run this script after a build to copy the binary to the web repo

cd build\MinSizeRel
powershell.exe -nologo -noprofile -command "& { Compress-Archive -Path empirical.exe -DestinationPath deploy.zip }"
move /y deploy.zip ..\..\..\website\windows.zip
