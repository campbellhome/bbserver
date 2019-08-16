#!/bin/sh

mkdir -p ../obj/linux
mkdir -p ../bin/linux
cd ../obj/linux
echo Compiling bbclient...
clang -c -g -Werror -Wall -Wextra -I../../submodules/mc_imgui/submodules/mc_common/submodules/bbclient/include ../../submodules/mc_imgui/submodules/mc_common/submodules/bbclient/src/*.c
cd ../../linux
echo Compiling bbclient_example_wchar...
clang++ -g -Werror -Wall -Wextra -I../submodules/mc_imgui/submodules/mc_common/submodules/bbclient/include ../obj/linux/*.o ../submodules/mc_imgui/submodules/mc_common/submodules/bbclient/examples/bbclient_example_wchar.cpp -o ../bin/linux/bbclient_example_wchar
echo Compiling bboxtolog...
clang -g -Werror -Wall -Wextra -I../submodules/mc_imgui/submodules/mc_common/submodules/bbclient/include -I../src/bboxtolog ../obj/linux/*.o ../src/bboxtolog/bboxtolog.c -DBB_WIDECHAR=0 -o ../bin/linux/bboxtolog
cp ../bin/linux/bboxtolog ../bin/linux/bbcat
cp ../bin/linux/bboxtolog ../bin/linux/bbtail

cd ../obj/linux
echo Compiling mc_common...
clang -c -g -Werror -Wall -Wextra -I../../submodules/mc_imgui/submodules/mc_common/submodules/bbclient/include -I../../submodules/mc_imgui/submodules/mc_common/submodules/bbclient/include/bbclient -I../../submodules/mc_imgui/submodules/mc_common/include -I../../submodules/mc_imgui/submodules/mc_common/submodules ../../submodules/mc_imgui/submodules/mc_common/src/*.c
clang -c -g -Werror -Wall -Wextra -I../../submodules/mc_imgui/submodules/mc_common/submodules/bbclient/include -I../../submodules/mc_imgui/submodules/mc_common/submodules/bbclient/include/bbclient -I../../submodules/mc_imgui/submodules/mc_common/include -I../../submodules/mc_imgui/submodules/mc_common/submodules ../../submodules/mc_imgui/submodules/mc_common/submodules/parson/*.c
clang -c -g -Werror -Wall -Wextra -I../../submodules/mc_imgui/submodules/mc_common/submodules/bbclient/include -I../../submodules/mc_imgui/submodules/mc_common/submodules/bbclient/include/bbclient -I../../submodules/mc_imgui/submodules/mc_common/include -I../../submodules/mc_imgui/submodules/mc_common/submodules ../../submodules/mc_imgui/submodules/mc_common/src/uuid_rfc4122/*.c
clang -c -g -Werror -Wall -Wextra -I../../submodules/mc_imgui/submodules/mc_common/submodules/bbclient/include -I../../submodules/mc_imgui/submodules/mc_common/submodules/bbclient/include/bbclient -I../../submodules/mc_imgui/submodules/mc_common/include -I../../submodules/mc_imgui/submodules/mc_common/submodules ../../submodules/mc_imgui/submodules/mc_common/src/md5_rfc1321/*.c
clang -c -g -Werror -Wall -Wextra -I../../submodules/mc_imgui/submodules/mc_common/submodules/bbclient/include -I../../submodules/mc_imgui/submodules/mc_common/submodules/bbclient/include/bbclient -I../../submodules/mc_imgui/submodules/mc_common/include -I../../submodules/mc_imgui/submodules/mc_common/submodules ../../submodules/mc_imgui/submodules/mc_common/src/mc_callstack/*.c
cd ../../linux
rm -r ../obj/linux/
echo done
