wsl clang-format -i src/*.h src/*.c* src/bbupdater/*.c src/bboxtolog/*.c
pushd submodules\mc_imgui
call clang-format-all.bat
popd