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

/*
 This programm decodes attached trendData from email attachment into a readable text.
 The attached file has the sufix "sctd" - (s)mart(c)heck(t)rend(d)ata
 The header and the data will be commata separated
 The attachment is serialized by googles protobuf.
 */

#include <time.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <zlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include "../common/datatypes.h"
#include "../common/helper_functions.h"

#include "TransferMessage.pb.h"
#include "Trend.pb.h"

#ifdef _MSC_VER
#include<winsock.h>
#include <io.h>
#define read _read
#define open _open
#define sprintf sprintf_s
#define sscanf sscanf_s
#define gmtime_r(x,y) gmtime_s(y,x)
#undef uuid_t
#pragma comment(lib, "Ws2_32.lib")
#else
#include <arpa/inet.h>
#endif

Uint16_t CalcChecksum(const void *pData, size_t Length)
{
  Uint64_t int_length = Length / sizeof(Uint16_t);
  const Uint16_t *p_int_data = (Uint16_t*) pData;

  Uint16_t checksum = 0;
  Uint32_t i = 0;
  for (i = 0; i < int_length; ++i)
  {
    checksum ^= p_int_data[i];
  }

  if (0 != Length % sizeof(Uint16_t))
  {
    Uint16_t temp = 0;
    memcpy(&temp, &p_int_data[int_length], Length % sizeof(Uint16_t));
    checksum ^= temp;
  }

  return (checksum);
}

const char* TimestampAsYYYYMMDDHHMMSSms(timestamp_t timestamp)
{
  static char buf[64];
  int year = 0;
  int month = 0;
  int day = 0;
  int hour = 0;
  int minute = 0;
  int second = 0;
  struct tm timeStruct;

  if (!timestamp)
  {
    sprintf(buf, "-");
    return buf;
  }

  memset(&timeStruct, 0, sizeof(struct tm));
  const time_t ts = timestamp / 1000000;
  int ms = timestamp % 1000;
  gmtime_r(&ts, &timeStruct);

  year = timeStruct.tm_year + 1900;
  month = timeStruct.tm_mon + 1;
  day = timeStruct.tm_mday;
  hour = timeStruct.tm_hour;
  minute = timeStruct.tm_min;
  second = timeStruct.tm_sec;

  sprintf(buf, "%04d-%02d-%02d %02d:%02d:%02d.%03d (UTC)", year, month, day, hour, minute, second, ms);
  return buf;
}

char* LongFloatToString(const char *pFormat, char Delimiter, Float64_t FloatValue)
{
  static char buffer[64];
  snprintf(buffer, sizeof(buffer), pFormat, FloatValue);
  if (!strncmp(buffer, "nan", 3))
  {
    buffer[0] = Delimiter;
    buffer[1] = 0;
  }
  return buffer;
}

int UncompressData(Bytef *dest, uLongf *destLen, const Bytef *source, uLong sourceLen, int windowBits)
{
  z_stream stream;
  int err = 0;
  stream.next_in = (Bytef*) source;
  stream.avail_in = (uInt) sourceLen;
  /* Check for source > 64K on A16-bit machine: */

  if ((uLong) stream.avail_in != sourceLen) return Z_BUF_ERROR;
  stream.next_out = dest;
  stream.avail_out = (uInt) *destLen;

  if ((uLong) stream.avail_out != *destLen) return Z_BUF_ERROR;

  stream.zalloc = (alloc_func) 0;
  stream.zfree = (free_func) 0;

  err = inflateInit2(&stream, windowBits);
  if (err != Z_OK) return err;

  err = inflate(&stream, Z_FINISH);
  if (err != Z_STREAM_END)
  {

    (void) inflateEnd(&stream);
    if (err == Z_NEED_DICT || (err == Z_BUF_ERROR && stream.avail_in == 0)) return Z_DATA_ERROR;
    return err;
  }
  *destLen = stream.total_out;
  err = inflateEnd(&stream);
  return err;
}

#ifndef O_BINARY 
#define O_BINARY 0
#endif

static FILE *outstream = stdout;

int main(int argc, char **argv)
{
  PrintVersionNumber();

  int buffer_length = CheckCommandLineParameters(argc, argv, "trend data", outstream);

  char* buffer = ReadInputFileIntoBuffer(argv[1], buffer_length);

  ConvertHexToBinIfNeeded(buffer_length, buffer);

  std::string binary_data = "";

  smartcheck::TransferMessage transfer_message;
  if (transfer_message.ParseFromArray(buffer, (int) buffer_length) && transfer_message.IsInitialized()
      && transfer_message.has_trend())
  {
    fprintf(stdout, "Trend is in transfer message protobuf format\n");
    binary_data = transfer_message.trend().binary_data();
  }
  else
  {
    smartcheck::Trend trend_data;
    if (trend_data.ParseFromArray(buffer, (int) buffer_length) && trend_data.IsInitialized() && !trend_data.trend_uuid().empty())
    {
      fprintf(stdout, "Trend is in protobuf format\n");
      binary_data = trend_data.binary_data();
    }
    else
    {
      binary_data = std::string(buffer, buffer_length);
    }
  }
  unsigned char *uncompressed_data = NULL;
  unsigned char *pt_data = NULL;

  trend_header_t header;
  memcpy(&header, &(binary_data[0]), sizeof(trend_header_t));

  Uint16_t calculated_checksum_header = CalcChecksum(&header, sizeof(trend_header_t) - 2);
  if (calculated_checksum_header != header.checksum_header)
  {
    fprintf(stderr, "Error: Header checksum not matching in file %s\n", argv[1]);
    exit(-1);
  }

  if (binary_data.size() != (header.header_size + header.byte_count))
  {
    fprintf(stderr, "Error: Data size mismatch %zu <--> %d in %s\n", binary_data.size(),
            header.header_size + header.byte_count, argv[1]);

    exit(-1);
  }

  unsigned char *data = new unsigned char[header.byte_count];
  memcpy(data, &(binary_data[header.header_size]), header.byte_count);

  Uint16_t calculated_checksum_data = CalcChecksum(data, header.byte_count);
  if (calculated_checksum_data != header.checksum_data)
  {
    fprintf(stderr, "Error: Data checksum not matching in file %s\n", argv[1]);
    exit(-1);
  }

  // print header information
  fprintf(outstream, "\nHeader version:             \t%d\n", header.version);
  fprintf(outstream, "Header size:                \t%d\n", header.header_size);

  std::string compression_str = "unknown";
  if (header.compression == no_compression)
  {
    compression_str = "no compression";
  }
  else if (header.compression == zlib)
  {
    compression_str = "zlib";
  }
  else if (header.compression == int24)
  {
    compression_str = "int24";
  }
  else if (header.compression == gzip)
  {
    compression_str = "gzip";
  }
  fprintf(outstream, "Compression:                \t%s (%d)\n", compression_str.c_str(), header.compression);
  fprintf(outstream,
          "Config-uuid:                \t%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X\n",
          header.uuid_characteristic_value_config[0], header.uuid_characteristic_value_config[1],
          header.uuid_characteristic_value_config[2], header.uuid_characteristic_value_config[3],
          header.uuid_characteristic_value_config[4], header.uuid_characteristic_value_config[5],
          header.uuid_characteristic_value_config[6], header.uuid_characteristic_value_config[7],
          header.uuid_characteristic_value_config[8], header.uuid_characteristic_value_config[9],
          header.uuid_characteristic_value_config[10], header.uuid_characteristic_value_config[11],
          header.uuid_characteristic_value_config[12], header.uuid_characteristic_value_config[13],
          header.uuid_characteristic_value_config[14], header.uuid_characteristic_value_config[15]);

  fprintf(outstream,
          "Trend-uuid:                 \t%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X\n",
          header.uuid_trend[0], header.uuid_trend[1], header.uuid_trend[2], header.uuid_trend[3], header.uuid_trend[4],
          header.uuid_trend[5], header.uuid_trend[6], header.uuid_trend[7], header.uuid_trend[8], header.uuid_trend[9],
          header.uuid_trend[10], header.uuid_trend[11], header.uuid_trend[12], header.uuid_trend[13],
          header.uuid_trend[14], header.uuid_trend[15]);
//  fprintf(outstream, "First timestamp:              %ld\n", header.first_timestamp);
//  fprintf(outstream, "Last timestamp:               %ld\n", header.last_timestamp);

  fprintf(outstream, "First timestamp:            \t%s\n", TimestampAsYYYYMMDDHHMMSSms(header.first_timestamp));
  fprintf(outstream, "Last timestamp:             \t%s\n", TimestampAsYYYYMMDDHHMMSSms(header.last_timestamp));
  fprintf(outstream,
          "Unit-uuid:                  \t%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X\n",
          header.unit[0], header.unit[1], header.unit[2], header.unit[3], header.unit[4], header.unit[5],
          header.unit[6], header.unit[7], header.unit[8], header.unit[9], header.unit[10], header.unit[11],
          header.unit[12], header.unit[13], header.unit[14], header.unit[15]);

  fprintf(outstream, "Lower pre alarm level:      \t%f\n", header.lower_pre_alarm_level);
  fprintf(outstream, "Lower main alarm level:     \t%s\n", LongFloatToString("%f", '-', header.lower_main_alarm_level));
  fprintf(outstream, "Number of trend entries:    \t%d\n", header.value_count);
  fprintf(outstream, "Byte count samples:         \t%d\n", header.byte_count);
  fprintf(outstream, "Data checksum:              \t%d\n", header.checksum_data);
  fprintf(outstream, "Header checksum:            \t%d\n", header.checksum_header);

  if (header.compression != no_compression)
  {
    size_t array_size = header.value_count;
    uncompressed_data = (unsigned char*) malloc(array_size * sizeof(trend_entry_t));
    unsigned long length = (unsigned long) array_size * (unsigned long) sizeof(trend_entry_t);
    if (UncompressData(uncompressed_data, &length, (unsigned char*) data, header.byte_count,
                       header.compression == zlib ? 15 : 31) != Z_OK)
    {
      free(uncompressed_data);
      fprintf(stderr, "Error: Could not uncompress data from file %s\n", argv[1]);
      exit(-1);

    }
    if (length != array_size * sizeof(trend_entry_t))
    {
      free(uncompressed_data);
      fprintf(stderr, "Error: Uncompressed length does not match expected length in file %s\n", argv[1]);
      exit(-1);

    }
    pt_data = uncompressed_data;
    // fprintf(outstream, "length sample array=%d:    \t (%d)\n", length);

  }
  else
  {
    pt_data = data;
  }

  trend_entry_t *values = (trend_entry_t*) pt_data;
  fprintf(
      outstream,
      "\n \t entry                       \ttimestamp        \t value      \tmain_alarm_level   \t pre_alarm_level  \talarm_map_index  \t alarm_status   \tlearning_mode  speed\n");
//fprintf(outstream, "---------------------------------------------------------------------------------------------------------------------------------------------\n");
  for (int i = 0; i < header.value_count; i++)
  {
    fprintf(
        outstream,
        "  \t%4d  \t%s  \t%12.6lf          \t%12.6f       \t%12.6f                \t%d             \t%d               \t%s   \t%s\n",
        i, TimestampAsYYYYMMDDHHMMSSms(values[i].timestamp), values[i].value, values[i].main_alarm_level,
        values[i].pre_alarm_level, values[i].alarm_map_index, values[i].alarm_status,
        values[i].learning_mode_active == true ? "1" : "0", LongFloatToString("%f", ' ', values[i].speed));
  }

  fprintf(outstream, "\n");

  free(buffer);
  free(data);
  free(uncompressed_data);
  if (argc == 3)
  {
    fclose(outstream);
    fprintf(stdout, "%s: Success: Trend data written to file %s\n", argv[0], argv[2]);
  }

  exit(0);
}

