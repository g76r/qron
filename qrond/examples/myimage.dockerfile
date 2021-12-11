FROM debian:sid
ENTRYPOINT [ "/bin/true" ]
# can be build with these command lines:
# docker build -t g76r/myimage -f myimage.dockerfile .
# docker build -t myimage2 -f myimage.dockerfile .
