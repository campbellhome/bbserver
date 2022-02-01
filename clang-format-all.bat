wsl clang-format -i src/*.h src/*.c* src/bbupdater/*.c src/bboxtolog/*.c
pushd mc_imgui
call clang-format-all.bat
popd
pushd mc_common
call clang-format-all.bat
popd
pushd bbclient
call clang-format-all.bat
popd