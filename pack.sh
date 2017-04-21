PN=tftp

rm -rf ars276-$PN
mkdir ars276-$PN

cp client.c server.c common.h common.c Makefile README.md ars276-$PN
