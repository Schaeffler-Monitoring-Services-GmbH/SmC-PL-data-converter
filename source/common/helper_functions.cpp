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

#include "helper_functions.h"
#include <ctype.h>
#include <stdio.h>
#include <cstdlib>
#include <sys/stat.h>
#include <fcntl.h>
#include "version.h"

#ifdef _MSC_VER
#include <io.h>
#define read _read
#define open _open
#pragma comment(lib, "Ws2_32.lib")
#else
#include <unistd.h>
#endif
#include <iostream>

#ifndef O_BINARY
#define O_BINARY 0
#endif

// Data from OPC/UA via the UA-Expert is given as hex, otherwise as int's. If in hex, it is converted here. Can be removed, if data is provided directly in int.
void ConvertHexToBinIfNeeded(int buffer_length, char* pBuffer)
{
    bool is_hex = true;

    for (int i = 0; i < (buffer_length / 2 * 2); i++)
    {
        if (!isxdigit(pBuffer[i]))
        {
            is_hex = false;
        }
    }

    if (is_hex)
    {
        for (int i = 0; i < buffer_length / 2; i++)
        {
            int tmp = 0;
            sscanf(pBuffer + i * 2, "%02x", &tmp);
            pBuffer[i] = tmp & 0xFF;
        }
        buffer_length /= 2;
    }
}

int CheckCommandLineParameters(int argc, char** argv, const char* input_data_type, FILE*& outstream)
{
    if (argc != 2 && argc != 3)
    {
        fprintf(stderr, "Usage: %s <%s input file> [output file]\n", argv[0], input_data_type);
        exit(-1);
    }
    if (argc == 3)
    {
        outstream = fopen(argv[2], "w");
        if (outstream == NULL)
        {
            fprintf(stderr, "Error: %s: Could not open output file %s\n", argv[0], argv[2]);
            fprintf(stderr, "Usage: %s <%s input file> [output file]\n", argv[0], input_data_type);
            exit(-1);
        }
    }
    struct stat f_stat;

    if (stat(argv[1], &f_stat) != 0)
    {
        fprintf(stderr, "Error: Could not determine size of file %s\n", argv[1]);
        exit(-1);
    }

    return f_stat.st_size;
}

char* ReadInputFileIntoBuffer(const char* input_file, int buffer_length)
{
    int fh = open(input_file, O_RDONLY | O_BINARY);
    if (fh < 0)
    {
        fprintf(stderr, "Error: Could not open file %s\n", input_file);
        exit(-1);
    }

    char* buffer = new char[buffer_length];

    if (read(fh, buffer, buffer_length) != (int)buffer_length)
    {
        fprintf(stderr, "Error: Could not read %d bytes from file into buffer %s\n", (int)buffer_length, input_file);
        exit(-1);
    }

    return buffer;
}


void PrintVersionNumber()
{
    fprintf(stdout, "%s, version %s\n", program_name.c_str(), version_number.c_str());
}