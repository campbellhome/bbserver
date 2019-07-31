#!/bin/sh

rm -r ../obj/linux/
mkdir -p ../obj/linux
mkdir -p ../bin/linux
cd ../obj/linux
clang -c -g -Werror -Wall -Wextra -I../../submodules/mc_imgui/submodules/mc_common/submodules/bbclient/include ../../submodules/mc_imgui/submodules/mc_common/submodules/bbclient/src/*.c
cd ../../linux
clang++ -g -Werror -Wall -Wextra -I../submodules/mc_imgui/submodules/mc_common/submodules/bbclient/include ../obj/linux/*.o ../submodules/mc_imgui/submodules/mc_common/submodules/bbclient/examples/bbclient_example_wchar.cpp -o ../bin/linux/bbclient_example_wchar
clang -g -Werror -Wall -Wextra -I../submodules/mc_imgui/submodules/mc_common/submodules/bbclient/include -I../src/bboxtolog ../obj/linux/*.o ../src/bboxtolog/bboxtolog.c -DBB_WIDECHAR=0 -o ../bin/linux/bboxtolog
cp ../bin/linux/bboxtolog ../bin/linux/bbcat
cp ../bin/linux/bboxtolog ../bin/linux/bbtail