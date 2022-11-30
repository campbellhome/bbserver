#!/bin/sh

mkdir -p ../obj/linux
mkdir -p ../bin/linux

cd ../obj/linux
echo Compiling bbclient...
clang -c -g -Werror -Wall -Wextra -I../../bbclient/include ../../bbclient/src/*.c

cd ../../linux
echo Compiling bbclient_example_wchar...
clang++ -g -Werror -Wall -Wextra -I../bbclient/include ../obj/linux/*.o ../bbclient/examples/bbclient_example_wchar.cpp -o ../bin/linux/bbclient_example_wchar -lpthread

cd ../obj/linux
echo Compiling mc_common...
clang -c -g -Werror -Wall -Wextra -I../../bbclient/include -I../../bbclient/include/bbclient -I../../mc_common/include -I../../thirdparty ../../mc_common/src/*.c ../../thirdparty/parson/parson.c ../../thirdparty/sqlite/sqlite3.c ../../mc_common/src/uuid_rfc4122/*.c ../../mc_common/src/md5_rfc1321/*.c ../../mc_common/src/mc_callstack/*.c

cd ../../linux
echo Compiling bboxtolog...
clang -g -Werror -Wall -Wextra -I../bbclient/include -I../bbclient/include/bbclient -I../mc_common/include -I../thirdparty -I../src/bboxtolog ../obj/linux/*.o ../src/bboxtolog/bboxtolog.c -DBB_WIDECHAR=0 -o ../bin/linux/bboxtolog -lpthread -ldl
cp ../bin/linux/bboxtolog ../bin/linux/bbcat
cp ../bin/linux/bboxtolog ../bin/linux/bbtail

rm -r ../obj/linux/
echo done
