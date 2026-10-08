Remove-Item -Path "../bin/resources" -Recurse -Force -ErrorAction SilentlyContinue

# Copy-Item -Path "..\resources" -Destination ../bin/ -Recurse -Force -ErrorAction SilentlyContinue

cd ../bin/

.\ForradiaLang.exe

cd ../scripts/