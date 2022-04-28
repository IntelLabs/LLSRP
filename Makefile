###########################################################################
#  Copyright 2022 Intel Corporation
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.
#
###########################################################################
#
#  Written by: Evgeny Stupachenko
#              e-mail: evgeny.v.stupachenko@intel.com
#
###########################################################################

GCC = g++
COMMON_CPP = LLMLNCL/multilink.cpp
COMMON_DEP = ${COMMON_CPP} LLMLNCL/multilink.h Config.h Options.h Makefile
FFMPEG_LIB = -lavcodec -lavutil
SERVER_LIB = -lx264 -lXtst
VIDEO_CLIENT_CPP = ${COMMON_CPP} Client.cpp VideoDecode.cpp DisplayWindow.cpp MouseKbdRead.cpp
VIDEO_CLIENT_DEP = ${COMMON_DEP} ${VIDEO_CLIENT_CPP} DisplayWindow.h VideoDecode.h MemoryBuffer.h
VIDEO_SERVER_CPP = ${COMMON_CPP} Server.cpp VideoEncodeX.cpp ScreenStreamX.cpp MouseKbdWrite.cpp GrabWindow.cpp
VIDEO_SERVER_DEP = ${COMMON_DEP} ${VIDEO_SERVER_CPP} ScreenStreamX.h VideoEncodeX.h MouseKbdWrite.h GrabWindow.h
VIDEO_LIB = -lpthread -lisal -lpopt -lX11 -lXext
FLAGS = -DROUND_TRIPq -DNO_CONVERTq -DPRINT_STATq -DPRINT_WARNq
SIZE_OPT = -ffunction-sections -Wl,--gc-sections -fno-asynchronous-unwind-tables -Wl,--strip-all
OPTIONS = -g -O3 -ffast-math -march=native -std=c++11 -Wno-deprecated-declarations ${SIZE_OPT}

all:  client server

clientMouse: ${VIDEO_CLIENT_DEP}
	${GCC} -o clientMouse ${OPTIONS} ${VIDEO_CLIENT_CPP} -I. -ILLMLNCL -DMOUSE_TEST ${VIDEO_LIB} ${FFMPEG_LIB}
client: ${VIDEO_CLIENT_DEP}
	${GCC} -o client ${OPTIONS} ${VIDEO_CLIENT_CPP} -I. -ILLMLNCL ${FLAGS} ${VIDEO_LIB} ${FFMPEG_LIB} -lXtst
server: ${VIDEO_SERVER_DEP}
	${GCC} -o server ${OPTIONS} ${VIDEO_SERVER_CPP} -I. -ILLMLNCL ${FLAGS} ${VIDEO_LIB} ${FFMPEG_LIB} ${SERVER_LIB} -fopenmp
clean:
	rm -f client server clientMouse
