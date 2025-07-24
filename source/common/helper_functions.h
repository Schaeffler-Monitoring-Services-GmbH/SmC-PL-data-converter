// Copyright 2025 Schaeffler Monitoring Services GmbH
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated 
// documentation files(the “Software”), to deal in the Software without restriction, including without limitation the 
// rights to use, copy, modify, merge, publish, distribute, sublicense, and /or sell copies of the Software, and to 
// permit persons to whom the Software is furnished to do so, subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the 
// Software.
//
// THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE 
// WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE AUTHORS OR 
// COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR 
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#pragma once

#include <cstdio>

// Data from OPC/UA via the UA-Expert is given as hex, otherwise as int's. If in hex, it is converted here. Can be removed, if data is provided directly in int.
void ConvertHexToBinIfNeeded(int buffer_length, char* pBuffer);

int CheckCommandLineParameters(int argc, char** argv, const char* input_data_type, FILE*& outstream);

char* ReadInputFileIntoBuffer(const char* input_file, int buffer_length);

void PrintVersionNumber();