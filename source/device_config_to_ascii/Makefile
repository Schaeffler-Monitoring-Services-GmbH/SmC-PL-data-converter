# Copyright 2025 Schaeffler Monitoring Services GmbH
#
# Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated 
# documentation files(the “Software”), to deal in the Software without restriction, including without limitation the 
# rights to use, copy, modify, merge, publish, distribute, sublicense, and /or sell copies of the Software, and to 
# permit persons to whom the Software is furnished to do so, subject to the following conditions :
#
# The above copyright notice and this permission notice shall be included in all copies or substantial portions of the 
# Software.
#
# THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE 
# WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE AUTHORS OR 
# COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR 
# OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

CFLAGS     := 
LD_FLAGS   := -Wl,--copy-dt-needed-entries -lz -lprotobuf-lite
TARGETDIR  := ../../bin

PROTOS   := JobConfig.proto DeviceConfig.proto SystemState.proto JobData.proto TimeSignal.proto Trend.proto ClassificationData.proto TransferMessage.proto

BIN      := device_config_to_ascii

SRCS     := $(wildcard *.cpp) $(wildcard ../common/*.cpp)
OBJS     := $(SRCS:.cpp=.o)

PBOBJS   := $(PROTOS:%.proto=%.pb.o)
PBGENS   := $(PROTOS:%.proto=%.pb.cc) $(PROTOS:*.proto=%.pb.h)

all: $(BIN)

clean:
	rm -f $(TARGETDIR)/$(BIN) $(OBJS) $(PBOBJS) $(PBGENS) *.pb.cc *.pb.h

$(BIN): $(OBJS)

.PHONY: $(PROTOS)

%.pb.cc: %.proto
	protoc -I../protobuf --cpp_out=. ../protobuf/$<

%.pb.o : %.pb.cc
	$(CXX) $(CXX_FLAGS) -c -o $@ $<

%.o: %.cpp
	$(CXX) $(CXX_FLAGS) -c -o $@ $<

$(BIN): %: $(PBOBJS) $(OBJS)
	$(CXX) -o $(TARGETDIR)/$@ $^ $(LD_FLAGS)
