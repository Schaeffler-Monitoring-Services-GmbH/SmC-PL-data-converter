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

#include "ClassificationData.pb.h"
#include "TransferMessage.pb.h"

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

static FILE *outstream = stdout;

#ifndef O_BINARY 
#define O_BINARY 0
#endif

int main(int argc, char **argv)
{
  PrintVersionNumber();

  int buffer_length = CheckCommandLineParameters(argc, argv, "classification data", outstream);

  char* buffer = ReadInputFileIntoBuffer(argv[1], buffer_length);

  ConvertHexToBinIfNeeded(buffer_length, buffer);

  std::string binary_data = "";
  smartcheck::TransferMessage transfer_message;
  if (transfer_message.ParseFromArray(buffer, (int) buffer_length) && transfer_message.IsInitialized()
      && transfer_message.has_classification_data())
  {
    fprintf(outstream, "Classification data is in transfer message protobuf format\n");
    binary_data = transfer_message.classification_data().binary_data();
  }
  else
  {
    smartcheck::ClassificationData classification_data;
    classification_data.ParseFromArray(buffer, (int) buffer_length);

    if (classification_data.ParseFromArray(buffer, (int) buffer_length) && classification_data.IsInitialized() && !classification_data.classification_data_uuid().empty())
    {
      fprintf(outstream, "Classification data is in ProtoBuf format\n");
      binary_data = classification_data.binary_data();
    }
    else
    {
      binary_data = std::string(buffer, (int) buffer_length);
    }
  }
  classification_header_t header;

  Uint16_t header_version = *(reinterpret_cast<const Uint16_t*>(&binary_data[0]));
  classification_header_t *p_classification_data_header = nullptr;
  switch (header_version)
  {
    case CLASSIFICATION_DATA_HEADER_VERSION:
    {
      memcpy(&header, &(binary_data[0]), sizeof(classification_header_t));
      break;
    }
    case 1:
    {
      classification_header_v1_t header_v1;
      memcpy(&header_v1, &binary_data[0], sizeof(classification_header_v1_t));
      Uint16_t calculated_checksum_header = CalcChecksum(&header_v1, sizeof(classification_header_v1_t) - 2);
      if (calculated_checksum_header != header_v1.checksum_header)
      {
        fprintf(stderr, "Error: Header checksum not matching in file %s\n", argv[1]);
        exit(-1);
      }

      header.version = header_v1.version;
      header.header_size = header_v1.header_size;
      memcpy(header.serial_number, header_v1.serial_number, 32);
      memcpy(header.comment, header_v1.comment, 256);
      header.compression = header_v1.compression;
      memcpy(header.uuid_characteristic_value_config, header_v1.uuid_characteristic_value_config, sizeof(uuid_t));
      memcpy(header.uuid_classification_data, header_v1.uuid_classification_data, sizeof(uuid_t));
      header.data_type = header_v1.data_type;
      header.period_type = header_v1.period_type;
      header.start_timestamp = header_v1.start_timestamp;
      header.end_timestamp = header_v1.end_timestamp;
      header.modified_timestamp = header_v1.modified_timestamp;
      header.close_timestamp = -1;
      header.dimensions[0] = header_v1.dimensions[0];
      header.dimensions[1] = header_v1.dimensions[1];
      header.sample_rate = header_v1.sample_rate;
      memcpy(header.unused, header_v1.unused, 4);
      header.sample_count = header_v1.sample_count;
      header.byte_count = header_v1.byte_count;
      header.checksum_data = header_v1.checksum_data;
      header.checksum_header = CalcChecksum(&header, sizeof(classification_header_t));
      break;
    }
    default:
      break;
  }

  Uint16_t calculated_checksum_header = CalcChecksum(&header, sizeof(classification_header_t) - 2);
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

  fprintf(outstream, "Header version:       \t%d\n", header.version);
  fprintf(outstream, "Header size:          \t%d\n", header.header_size);
  fprintf(outstream, "Serial number:        \t%s\n", header.serial_number);
  fprintf(outstream, "Comment:              \t%s\n", header.comment);

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
  fprintf(outstream, "Compression:          \t%s (%d)\n", compression_str.c_str(), header.compression);
  fprintf(outstream, "Config-uuid:          \t%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X\n",
          header.uuid_characteristic_value_config[0], header.uuid_characteristic_value_config[1],
          header.uuid_characteristic_value_config[2], header.uuid_characteristic_value_config[3],
          header.uuid_characteristic_value_config[4], header.uuid_characteristic_value_config[5],
          header.uuid_characteristic_value_config[6], header.uuid_characteristic_value_config[7],
          header.uuid_characteristic_value_config[8], header.uuid_characteristic_value_config[9],
          header.uuid_characteristic_value_config[10], header.uuid_characteristic_value_config[11],
          header.uuid_characteristic_value_config[12], header.uuid_characteristic_value_config[13],
          header.uuid_characteristic_value_config[14], header.uuid_characteristic_value_config[15]);
  fprintf(outstream, "Data-uuid:            \t%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X\n",
          header.uuid_classification_data[0], header.uuid_classification_data[1], header.uuid_classification_data[2],
          header.uuid_classification_data[3], header.uuid_classification_data[4], header.uuid_classification_data[5],
          header.uuid_classification_data[6], header.uuid_classification_data[7], header.uuid_classification_data[8],
          header.uuid_classification_data[9], header.uuid_classification_data[10], header.uuid_classification_data[11],
          header.uuid_classification_data[12], header.uuid_classification_data[13], header.uuid_classification_data[14],
          header.uuid_classification_data[15]);
  std::string data_type_str = "unknown";
  if (header.data_type == classification_data_ldd)
  {
    data_type_str = "ldd";
  }
  else if (header.data_type == classification_data_rfc)
  {
    data_type_str = "rfc";
  }
  else if (header.data_type == classification_data_temperature)
  {
    data_type_str = "temperature";
  }
  fprintf(outstream, "Data type:            \t%s (%d)\n", data_type_str.c_str(), header.data_type);

  std::string period_type_str = "unknown";
  if (header.period_type == classification_period_hourly)
  {
    period_type_str = "hourly";
  }
  else if (header.period_type == classification_period_daily)
  {
    period_type_str = "daily";
  }
  else if (header.period_type == classification_period_weekly)
  {
    period_type_str = "weekly";
  }
  else if (header.period_type == classification_period_monthly)
  {
    period_type_str = "monthly";
  }
  else if (header.period_type == classification_period_yearly)
  {
    period_type_str = "yearly";
  }
  else if (header.period_type == classification_period_continuous)
  {
    period_type_str = "continuous";
  }
  else if (header.period_type == classification_period_user_defined)
  {
    period_type_str = "user defined";
  }
  else if (header.period_type == classification_period_initial_data)
  {
    period_type_str = "initial";
  }
  fprintf(outstream, "Period type:          \t%s (%d)\n", period_type_str.c_str(), header.period_type);
  char outstr[200];
  time_t current_time_sec = header.start_timestamp / 1000000;
  struct tm *ptm = gmtime(&current_time_sec);
  strftime(outstr, sizeof(outstr), "%Y-%m-%dT%H:%M:%S UTC", ptm);

  fprintf(outstream, "Start:                \t%s (%lld)\n", outstr, header.start_timestamp);

  current_time_sec = header.end_timestamp / 1000000;
  ptm = gmtime(&current_time_sec);
  strftime(outstr, sizeof(outstr), "%Y-%m-%dT%H:%M:%S UTC", ptm);

  fprintf(outstream, "End:                  \t%s (%lld)\n", outstr, header.end_timestamp);

  current_time_sec = header.modified_timestamp / 1000000;
  ptm = gmtime(&current_time_sec);
  strftime(outstr, sizeof(outstr), "%Y-%m-%dT%H:%M:%S UTC", ptm);
  fprintf(outstream, "Last time written:    \t%s (%lld)\n", outstr, header.modified_timestamp);

  if (header.close_timestamp > 0)
  {
    current_time_sec = header.close_timestamp / 1000000;
    ptm = gmtime(&current_time_sec);
    strftime(outstr, sizeof(outstr), "%Y-%m-%dT%H:%M:%S UTC", ptm);
    fprintf(outstream, "Closed on:    \t%s (%lld)\n", outstr, header.close_timestamp);
  }

  fprintf(outstream, "Dimension 1:\n");
  fprintf(outstream, "  Unit string:        \t%s\n", header.dimensions[0].unit_string);
  fprintf(outstream, "  Unit uuid:          \t%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X\n",
          header.dimensions[0].unit_uuid[0], header.dimensions[0].unit_uuid[1], header.dimensions[0].unit_uuid[2],
          header.dimensions[0].unit_uuid[3], header.dimensions[0].unit_uuid[4], header.dimensions[0].unit_uuid[5],
          header.dimensions[0].unit_uuid[6], header.dimensions[0].unit_uuid[7], header.dimensions[0].unit_uuid[8],
          header.dimensions[0].unit_uuid[9], header.dimensions[0].unit_uuid[10], header.dimensions[0].unit_uuid[11],
          header.dimensions[0].unit_uuid[12], header.dimensions[0].unit_uuid[13], header.dimensions[0].unit_uuid[14],
          header.dimensions[0].unit_uuid[15]);
  fprintf(outstream, "  Number of classes:  \t%d\n", header.dimensions[0].num_classes);
  fprintf(outstream, "  Lower border:       \t%g\n", header.dimensions[0].lower_border);
  fprintf(outstream, "  Upper border:       \t%g\n", header.dimensions[0].upper_border);
  fprintf(outstream, "Dimension 2:\n");
  fprintf(outstream, "  Unit:               \t%s\n", header.dimensions[1].unit_string);
  fprintf(outstream, "  Unit uuid:          \t%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X\n",
          header.dimensions[1].unit_uuid[0], header.dimensions[1].unit_uuid[1], header.dimensions[1].unit_uuid[2],
          header.dimensions[1].unit_uuid[3], header.dimensions[1].unit_uuid[4], header.dimensions[1].unit_uuid[5],
          header.dimensions[1].unit_uuid[6], header.dimensions[1].unit_uuid[7], header.dimensions[1].unit_uuid[8],
          header.dimensions[1].unit_uuid[9], header.dimensions[1].unit_uuid[10], header.dimensions[1].unit_uuid[11],
          header.dimensions[1].unit_uuid[12], header.dimensions[1].unit_uuid[13], header.dimensions[1].unit_uuid[14],
          header.dimensions[1].unit_uuid[15]);
  fprintf(outstream, "  Number of classes:  \t%d\n", header.dimensions[1].num_classes);
  fprintf(outstream, "  Lower border:       \t%g\n", header.dimensions[1].lower_border);
  fprintf(outstream, "  Upper border:       \t%g\n", header.dimensions[1].upper_border);
  fprintf(outstream, "Sample rate:          \t%d\n", header.sample_rate);
  fprintf(outstream, "Sample count:         \t%llu\n", header.sample_count);
  fprintf(outstream, "Byte count:           \t%d\n", header.byte_count);
  fprintf(outstream, "Data checksum:        \t%04X\n", header.checksum_data);
  fprintf(outstream, "Header checksum:      \t%04X\n", header.checksum_header);

  Uint64_t *values = 0;

  if (header.compression != no_compression)
  {
    size_t array_size = header.dimensions[0].num_classes;
    if (header.dimensions[1].num_classes > 0)
    {
      array_size = header.dimensions[0].num_classes * header.dimensions[1].num_classes;
    }
    unsigned char *uncompressed_data = (unsigned char*) malloc(array_size * sizeof(Uint64_t));
    unsigned long length = (unsigned long) array_size * (unsigned long) sizeof(Uint64_t);
    if (UncompressData(uncompressed_data, &length, (unsigned char*) data, header.byte_count,
                       header.compression == zlib ? 15 : 31) != Z_OK)
    {
      fprintf(stderr, "Error: Could not uncompress data from file %s\n", argv[1]);
      exit(-1);

    }
    if (length != array_size * sizeof(Uint64_t))
    {
      fprintf(stderr, "Error: Uncompressed length does not match expected length in file %s\n", argv[1]);
      exit(-1);

    }
    values = (Uint64_t*) uncompressed_data;
  }
  else
  {
    values = (Uint64_t*) data;
  }

  if (values)
  {
    int i = 0;
    int j = 0;
    if (header.dimensions[1].num_classes == 0)
    {
      header.dimensions[1].num_classes = 1;
      fprintf(outstream, "       D1 >");
    }
    else
    {
      fprintf(outstream, "v D2 / D1 >");
    }
    for (i = 0; i < (int) header.dimensions[0].num_classes; i++)
    {
      fprintf(outstream, "\t%8d", i);
    }
    fprintf(outstream, "\n");
    for (j = 0; j < (int) header.dimensions[1].num_classes; j++)
    {
      fprintf(outstream, "%8d", j);

      for (i = 0; i < (int) header.dimensions[0].num_classes; i++)
      {
        fprintf(outstream, "\t%8llu", values[i + header.dimensions[0].num_classes * j]);
      }
      fprintf(outstream, "\n");
    }

  }
  free(buffer);
  free(data);
  if (argc == 3)
  {
    fclose(outstream);
    fprintf(stderr, "%s: Success: Classification data written to file %s\n", argv[0], argv[2]);
  }
  exit(0);
}

