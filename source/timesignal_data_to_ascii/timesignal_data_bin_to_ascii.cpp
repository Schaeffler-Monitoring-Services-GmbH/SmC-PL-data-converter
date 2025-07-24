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
 This programm decodes attached timeSignals data from email attachment into a readable text.
 The attached file has the sufix "scts" - (s)mart(c)heck(t)ime(s)ignal.
 The header and the data will be shown commata separated.
 The attachment is serialized by googles protobuf.
 */

#include <time.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <zlib.h>
#include <string.h>
#include <sys/stat.h>
#include "../common/datatypes.h"
#include "../common/helper_functions.h"
#include "TransferMessage.pb.h"
#include "TimeSignal.pb.h"

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

static FILE* outstream = stdout;

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

  if (timestamp <= 0)
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

int UncompressData(Bytef *dest, uLongf *destLen, const Bytef *source, uLong sourceLen, int windowBits)
{
  z_stream stream;
  int err = 0;

  stream.next_in = (Bytef*) source;
  stream.avail_in = (uInt) sourceLen;
  /* Check for source > 64K on 16-bit machine: */
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

void PrintTimeSignal(const char *pFileName, const std::string &rBinaryData)
{
  int num_row_elements = 1;
  char separator = '\t';
  bool index = true;
  timesignal_header_t header;
  memcpy(&header, &(rBinaryData[0]), sizeof(timesignal_header_t));

  Uint16_t header_checksum_header = 0;
  Uint16_t header_checksum_data = 0;
  if (4 == header.version)
  {
    header_checksum_header = header.checksum_header;
    header_checksum_data = header.checksum_data;
  }
  else
  {
    timesignal_header_v3_t *p_v3_header = (timesignal_header_v3_t*) &header;
    header_checksum_header = p_v3_header->checksum_header;
    header_checksum_data = p_v3_header->checksum_data;
  }

  Uint16_t calculated_checksum_header = CalcChecksum(&header, header.header_size - 2);
  if (calculated_checksum_header != header_checksum_header)
  {
    fprintf(stderr, "Error: Header checksum not matching in file %s\n", pFileName);
    exit(-1);
  }

  if (rBinaryData.size() != (header.header_size + header.byte_count))
  {
    fprintf(stderr, "Error: Data size mismatch %zu <--> %llu in %s\n", rBinaryData.size(),
            header.header_size + header.byte_count, pFileName);
    exit(-1);
  }

  unsigned char *data = new unsigned char[(unsigned int) header.byte_count];
  memcpy(data, &(rBinaryData[header.header_size]), (size_t) header.byte_count);

  Uint16_t calculated_checksum_data = CalcChecksum(data, (size_t) header.byte_count);
  if (calculated_checksum_data != header_checksum_data)
  {
    fprintf(stderr, "Error: Data checksum not matching in file %s\n", pFileName);
    exit(-1);
  }

  // print header information
  fprintf(outstream, "\nHeader version:              \t%d\n", header.version);
  fprintf(outstream, "Header size:                 \t%d\n", header.header_size);

  switch (header.signal_type)
  {
    case not_defined:
      fprintf(outstream, "Signal type:                 \t%s\n", "not defined");
      break;

    case raw_time_signal:
      fprintf(outstream, "Signal type:                 \t%s\n", "raw time signal");
      break;

    case demodulated_time_signal:
      fprintf(outstream, "Signal type:                 \t%s\n", "demodulated time signal");
      break;

    case raw_spectrum:
      fprintf(outstream, "Signal type:                 \t%s\n", "raw spectrum");
      break;

    case demodulated_spectrum:
      fprintf(outstream, "Signal type:                 \t%s\n", "demodulated spectrum");
      break;

    case raw_order_analysis:
      fprintf(outstream, "Signal type:                 \t%s\n", "order analysis");
      break;

    case demodulated_order_analysis:
      fprintf(outstream, "Signal type:                 \t%s\n", "demodulated order analysis");
      break;

    case raw_time_synchronous_average:
      fprintf(outstream, "Signal type:                 \t%s\n", "raw time synchronous average");
      break;

    case demodulated_time_synchronous_average:
      fprintf(outstream, "Signal type:                 \t%s\n", "demodulated time synchronous average");
      break;

    default:
      break;
  }

  fprintf(outstream,
          "Config-uuid:                 \t%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X\n",
          header.uuid_config[0], header.uuid_config[1], header.uuid_config[2], header.uuid_config[3],
          header.uuid_config[4], header.uuid_config[5], header.uuid_config[6], header.uuid_config[7],
          header.uuid_config[8], header.uuid_config[9], header.uuid_config[10], header.uuid_config[11],
          header.uuid_config[12], header.uuid_config[13], header.uuid_config[14], header.uuid_config[15]);
  fprintf(outstream,
          "Measurement-uuid:            \t%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X\n",
          header.uuid_measurement[0], header.uuid_measurement[1], header.uuid_measurement[2],
          header.uuid_measurement[3], header.uuid_measurement[4], header.uuid_measurement[5],
          header.uuid_measurement[6], header.uuid_measurement[7], header.uuid_measurement[8],
          header.uuid_measurement[9], header.uuid_measurement[10], header.uuid_measurement[11],
          header.uuid_measurement[12], header.uuid_measurement[13], header.uuid_measurement[14],
          header.uuid_measurement[15]);

  fprintf(outstream, "Distance between samples:    \t%lg (%uHz)\n", header.delta_x,
          header.delta_x != 0.0 ? (uint32_t) ((1.0 / header.delta_x) + 0.5) : 0);
  fprintf(outstream, "Scaling factor:              \t%lf\n", header.scaling_factor);
  fprintf(outstream, "Offset:                      \t%lf\n", header.offset);
  fprintf(outstream, "Measurement timestamp:       \t%s\n", TimestampAsYYYYMMDDHHMMSSms(header.timestamp_microseconds));
  if (4 == header.version)
  {
    fprintf(outstream, "First sample timestamp:    \t%s\n",
            TimestampAsYYYYMMDDHHMMSSms(header.timestamp_first_sample_microseconds));
    if (header.signal_type == raw_order_analysis || header.signal_type == demodulated_order_analysis
        || header.signal_type == raw_time_synchronous_average
        || header.signal_type == demodulated_time_synchronous_average)
    {
      fprintf(outstream, "Order domain filter delay: \t%lf revolutions (%.2lf°)\n",
              header.order_domain_filter_delay_revolutions, header.order_domain_filter_delay_revolutions * 360.0);
    }
  }

  fprintf(outstream,
          "Unit-uuid:                   \t%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X\n",
          header.unit[0], header.unit[1], header.unit[2], header.unit[3], header.unit[4], header.unit[5],
          header.unit[6], header.unit[7], header.unit[8], header.unit[9], header.unit[10], header.unit[11],
          header.unit[12], header.unit[13], header.unit[14], header.unit[15]);

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
  fprintf(outstream, "Compression:                 \t%s (%d)\n", compression_str.c_str(), header.compression);

  std::string sample_type_str = "unknown";
  int numBytes;
  if (header.sample_type == data_int8_t)
  {
    sample_type_str = "signed int 8";
    numBytes = sizeof(Int8_t);
  }
  else if (header.sample_type == data_int16_t)
  {
    sample_type_str = "signed int 16";
    numBytes = sizeof(Int16_t);
  }
  else if (header.sample_type == data_int32_t)
  {
    sample_type_str = "signed int 32";
    numBytes = sizeof(Int32_t);
  }
  else if (header.sample_type == data_int64_t)
  {
    sample_type_str = "signed int 64";
    numBytes = sizeof(Int64_t);
  }
  else if (header.sample_type == data_uint8_t)
  {
    sample_type_str = "unsigned signed int 8";
    numBytes = sizeof(Uint8_t);
  }
  else if (header.sample_type == data_uint16_t)
  {
    sample_type_str = "unsigned signed int 16";
    numBytes = sizeof(Uint16_t);
  }
  else if (header.sample_type == data_uint32_t)
  {
    sample_type_str = "unsignned int 32";
    numBytes = sizeof(Uint32_t);
  }
  else if (header.sample_type == data_uint64_t)
  {
    sample_type_str = "unsigned int 64";
    numBytes = sizeof(Uint64_t);
  }
  else if (header.sample_type == data_float32_t)
  {
    sample_type_str = "float 32";
    numBytes = sizeof(Float32_t);
  }
  else if (header.sample_type == data_float64_t)
  {
    sample_type_str = "float 64";
    numBytes = sizeof(Float64_t);
  }
  fprintf(outstream, "Data type:                   \t%s (%d)\n", sample_type_str.c_str(), header.sample_type);
  fprintf(outstream, "Number of samples:           \t%lld\n", header.sample_count);
  fprintf(outstream, "Byte count samples:          \t%lld\n", header.byte_count);
  fprintf(outstream, "Rotational frequency:        \t%.3f\n", header.rotational_frequency);
  fprintf(outstream, "Data checksum:               \t%d\n", header_checksum_data);
  fprintf(outstream, "Header checksum:             \t%d\n", header_checksum_header);

  unsigned char *uncompressed_data = NULL;
  unsigned char *pt_data = NULL;

  if (header.compression != no_compression)
  {
    size_t array_size = (size_t) header.sample_count;
    uncompressed_data = (unsigned char*) malloc(array_size * numBytes);
    unsigned long length = (int) header.sample_count * numBytes;
    if (UncompressData(uncompressed_data, &length, (unsigned char*) data, (int) header.byte_count,
                       header.compression == zlib ? 15 : 31) != Z_OK)
    {
      free(uncompressed_data);
      fprintf(stderr, "Error: Could not uncompress data from file %s\n", pFileName);
      exit(-1);

    }
    if (length != array_size * numBytes)
    {
      free(uncompressed_data);
      fprintf(stderr, "Error: Uncompressed length does not match expected length in file %s\n", pFileName);
      exit(-1);

    }
    pt_data = uncompressed_data;
  }
  else
  {
    pt_data = data;
  }
  //
  // size and data types from timesignals may differ, so we have different handling
  //

  char s = separator ? '\t' : ' ';

  if (index == true)
  {
    fprintf(outstream, "\n \tindex   \traw value \tscaled value\n\n");
  }

  if (header.sample_type == data_int8_t)
  {
    Int8_t *values = (Int8_t*) pt_data;

    for (int i = 0; i < header.sample_count; i += num_row_elements)
    {
      num_row_elements =
          i + num_row_elements < (int) header.sample_count ? num_row_elements : (int) header.sample_count - i;

      if (index == true)
      {
        fprintf(outstream, "   \t%4d ", i);
      }

      for (int j = 0; j < num_row_elements; j++)
      {
        fprintf(outstream, "      \t%4d%c  %lf", values[i + j], s,
                (values[i + j] - header.offset) * header.scaling_factor);
      }
      if (index == true)
      {
        fprintf(outstream, "\n");
      }
    }

  }
  else if (header.sample_type == data_int16_t)
  {
    Int16_t *values = (Int16_t*) pt_data;

    for (int i = 0; i < header.sample_count; i += num_row_elements)
    {
      if (index == true)
      {
        fprintf(outstream, "   \t%4d ", i);
      }

      num_row_elements =
          i + num_row_elements < (int) header.sample_count ? num_row_elements : (int) header.sample_count - i;

      for (int j = 0; j < num_row_elements; j++)
      {
        fprintf(outstream, " \t%11d%c  %lf ", values[i + j], s,
                (values[i + j] - header.offset) * header.scaling_factor);
      }
      if (index == true)
      {
        fprintf(outstream, "\n");
      }
    }
  }
  else if (header.sample_type == data_int32_t)
  {
    Int32_t *values = (Int32_t*) pt_data;

    for (int i = 0; i < header.sample_count; i += num_row_elements)
    {
      if (index == true)
      {
        fprintf(outstream, "   \t%4d ", i);
      }

      num_row_elements =
          i + num_row_elements < (int) header.sample_count ? num_row_elements : (int) header.sample_count - i;

      for (int j = 0; j < num_row_elements; j++)
      {
        fprintf(outstream, " \t%11d%c  %lf ", values[i + j], s,
                (values[i + j] - header.offset) * header.scaling_factor);
      }
      if (index == true)
      {
        fprintf(outstream, "\n");
      }
    }
  }
  else if (header.sample_type == data_int64_t)
  {
    Int64_t *values = (Int64_t*) pt_data;

    for (int i = 0; i < header.sample_count; i += num_row_elements)
    {
      if (index == true)
      {
        fprintf(outstream, "   \t%4d ", i);
      }

      num_row_elements =
          i + num_row_elements < (int) header.sample_count ? num_row_elements : (int) header.sample_count - i;

      for (int j = 0; j < num_row_elements; j++)
      {
        fprintf(outstream, " \t%lld%c  %lf ", values[i + j], s,
                (values[i + j] - header.offset) * header.scaling_factor);
      }
      if (index == true)
      {
        fprintf(outstream, "\n");
      }
    }
  }
  else if (header.sample_type == data_uint8_t)
  {
    Uint8_t *values = (Uint8_t*) pt_data;

    for (int i = 0; i < header.sample_count; i += num_row_elements)
    {
      if (index == true)
      {
        fprintf(outstream, "   \t%4d ", i);
      }

      num_row_elements =
          i + num_row_elements < (int) header.sample_count ? num_row_elements : (int) header.sample_count - i;

      for (int j = 0; j < num_row_elements; j++)
      {
        fprintf(outstream, " \t%11u%c  %lf ", values[i + j], s,
                (values[i + j] - header.offset) * header.scaling_factor);
      }
      if (index == true)
      {
        fprintf(outstream, "\n");
      }
    }
  }
  else if (header.sample_type == data_uint16_t)
  {
    Uint16_t *values = (Uint16_t*) pt_data;

    for (int i = 0; i < header.sample_count; i += num_row_elements)
    {
      if (index == true)
      {
        fprintf(outstream, "   \t%4d ", i);
      }

      num_row_elements =
          i + num_row_elements < (int) header.sample_count ? num_row_elements : (int) header.sample_count - i;

      for (int j = 0; j < num_row_elements; j++)
      {
        fprintf(outstream, " \t%11u%c  %lf ", values[i + j], s,
                (values[i + j] - header.offset) * header.scaling_factor);
      }
      if (index == true)
      {
        fprintf(outstream, "\n");
      }
    }

  }
  else if (header.sample_type == data_uint32_t)
  {
    Uint32_t *values = (Uint32_t*) pt_data;

    for (int i = 0; i < header.sample_count; i += num_row_elements)
    {
      if (index == true)
      {
        fprintf(outstream, "   \t%4d ", i);
      }

      num_row_elements =
          i + num_row_elements < (int) header.sample_count ? num_row_elements : (int) header.sample_count - i;

      for (int j = 0; j < num_row_elements; j++)
      {
        fprintf(outstream, " \t%11u%c  %lf ", values[i + j], s,
                (values[i + j] - header.offset) * header.scaling_factor);
      }
      if (index == true)
      {
        fprintf(outstream, "\n");
      }
    }
  }
  else if (header.sample_type == data_uint64_t)
  {
    Uint64_t *values = (Uint64_t*) pt_data;

    for (int i = 0; i < header.sample_count; i += num_row_elements)
    {
      if (index == true)
      {
        fprintf(outstream, "   \t%4d ", i);
      }

      num_row_elements =
          i + num_row_elements < (int) header.sample_count ? num_row_elements : (int) header.sample_count - i;

      for (int j = 0; j < num_row_elements; j++)
      {
        fprintf(outstream, " \t%llu%c  %lf ", values[i + j], s,
                (values[i + j] - header.offset) * header.scaling_factor);
      }
      if (index == true)
      {
        fprintf(outstream, "\n");
      }
    }
  }
  else if (header.sample_type == data_float32_t)
  {
    Float32_t *values = (Float32_t*) pt_data;

    for (int i = 0; i < header.sample_count; i += num_row_elements)
    {
      if (index == true)
      {
        fprintf(outstream, "   \t%4d ", i);
      }

      num_row_elements =
          i + num_row_elements < (int) header.sample_count ? num_row_elements : (int) header.sample_count - i;

      for (int j = 0; j < num_row_elements; j++)
      {
        fprintf(outstream, " \t%lf%c  %lf ", values[i + j], s, (values[i + j] - header.offset) * header.scaling_factor);
      }
      if (index == true)
      {
        fprintf(outstream, "\n");
      }
    }
  }
  else if (header.sample_type == data_float64_t)
  {
    Float64_t *values = (Float64_t*) pt_data;

    for (int i = 0; i < (int) header.sample_count; i += num_row_elements)
    {
      if (index == true)
      {
        fprintf(outstream, "   \t%4d ", i);
      }

      num_row_elements =
          i + num_row_elements < (int) header.sample_count ? num_row_elements : (int) header.sample_count - i;

      for (int j = 0; j < num_row_elements; j++)
      {
        fprintf(outstream, " \t%lf%c  %lf ", values[i + j], s, (values[i + j] - header.offset) * header.scaling_factor);
      }
      if (index == true)
      {
        fprintf(outstream, "\n");
      }
    }
  }
  if (index == true && !separator)
  {
    fprintf(outstream, "---------------------------------------------------------------------------------\n\n");
  }
  else
  {
    fprintf(outstream, "\n");
  }

  free(data);
  free(uncompressed_data);
}

#ifndef O_BINARY
#define O_BINARY 0
#endif

int main(int argc, char **argv)
{
  PrintVersionNumber();
    
  int buffer_length = CheckCommandLineParameters(argc, argv, "timesignal data", outstream);

  char* buffer = ReadInputFileIntoBuffer(argv[1], buffer_length);

  ConvertHexToBinIfNeeded(buffer_length, buffer);

  smartcheck::TransferMessage transfer_message;

  std::string binary_data = "";
  if (transfer_message.ParseFromArray(buffer, (int) buffer_length) && transfer_message.IsInitialized() && transfer_message.has_timesignal())
  {
    fprintf(stderr, "Timesignal is in ProtoBuf TransferMessage format\n");
    binary_data = transfer_message.timesignal().binary_data();
  }
  else
  {
    smartcheck::TimeSignal timesignal_data;
    if (timesignal_data.ParseFromArray(buffer, (int) buffer_length) && timesignal_data.IsInitialized()
        && !timesignal_data.job_data_uuid().empty())
    {
      fprintf(stderr, "Timesignal is in ProtoBuf format\n");
      binary_data = timesignal_data.binary_data();
    }
    else
    {
      fprintf(stderr, "Timesignal is in binary format\n");
      binary_data = std::string(buffer, buffer_length);
    }
  }
  PrintTimeSignal(argv[1], binary_data);
  free(buffer);
  if (argc == 3)
  {
    fclose(outstream);
    fprintf(stdout, "%s: Success: Time signal data written to file %s\n", argv[0], argv[2]);
  }

  exit(0);
}

