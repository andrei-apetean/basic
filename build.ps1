$bin = "bin"
if (!(Test-Path $bin)) { New-Item -ItemType Directory $bin | Out-Null }

clang -o "$bin/main.exe" "src/main.c" "-Iinclude" "-ggdb" "-Wall" "-Wextra" "-Werror"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
