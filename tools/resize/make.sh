#!/bin/bash

cc -g -DRASPBERRY_PI -DOMX_SKIP64BIT \
-I/opt/vc/include/ -I/opt/vc/include/interface/vmcs_host/ \
-I/opt/vc/include/interface/vcos/pthreads \
-I/opt/vc/include/interface/vmcs_host/linux \
-I/opt/vc/include/IL -I/opt/vc/src/hello_pi/libs/ilclient \
-L/opt/vc/lib -L/opt/vc/src/hello_pi/libs/ilclient \
-lvchiq_arm -lpthread omx_resize_image.c component.c \
-o omx_resize_image -lbcm_host -lopenmaxil -lbrcmGLESv2 -lbrcmEGL -lilclient -lvcos -lvchiq_arm -lpthread -pthread -lopenmaxil -lbcm_host