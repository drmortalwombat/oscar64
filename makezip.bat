mkdir r:\oscar64
mkdir r:\oscar64\bin
mkdir r:\oscar64\include

xcopy /y bin\oscar64.exe r:\oscar64\bin
xcopy /y /e include r:\oscar64\include

tar -caf r:\oscar64.zip r:\oscar64



