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
#include "JobData.pb.h"

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


const char* PrintBlanks(int NumOfBlanks)
{
  static char blank_buffer[64];
  int i = 0;

  blank_buffer[0] = '?';
  blank_buffer[1] = 0;

  if (NumOfBlanks < sizeof(blank_buffer))
  {
    for (; i < NumOfBlanks; ++i)
    {
      blank_buffer[i] = ' ';
    }
    blank_buffer[i] = 0;
  }
  return blank_buffer;
}

#define PBLANKS PrintBlanks(Blanks)

#ifndef O_BINARY
#define O_BINARY 0
#endif

void UuidStringOut(const char *pText, const std::string &rUuid, int Blanks, const char *pCallingFunction)
{
  if (rUuid.size() != 16)
  {
    // to detect errors which function is calling this one
    //fprintf(stderr, "!!! unexpected size=%d for uuidStringOut(%s), callingFunction=%s\n", rUuid.size(), text, callingFunction);
    return;
  }

  fprintf(outstream, "%s%s%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X\n", PBLANKS, pText,
          (unsigned char) rUuid[0], (unsigned char) rUuid[1], (unsigned char) rUuid[2], (unsigned char) rUuid[3],
          (unsigned char) rUuid[4], (unsigned char) rUuid[5], (unsigned char) rUuid[6], (unsigned char) rUuid[7],
          (unsigned char) rUuid[8], (unsigned char) rUuid[9], (unsigned char) rUuid[10], (unsigned char) rUuid[11],
          (unsigned char) rUuid[12], (unsigned char) rUuid[13], (unsigned char) rUuid[14], (unsigned char) rUuid[15]);

  return;
}

void PrintCharacteristicValueData(const smartcheck::CharacteristicValueData &rCharacteristicValueData, int Blanks)
{
  UuidStringOut("Characteristic value current uuid:   \t", rCharacteristicValueData.characteristic_value_config_current_uuid(), 0, __FUNCTION__);
  fprintf(outstream, "%sMeasurement time:                    \t%s\n", PBLANKS, TimestampAsYYYYMMDDHHMMSSms(rCharacteristicValueData.timestamp()));
  fprintf(outstream, "%sAlarm status:                        \t%d\n", PBLANKS, (int)rCharacteristicValueData.alarm_state());
  fprintf(outstream, "%sAlarm state since:                   \t%s\n", PBLANKS, TimestampAsYYYYMMDDHHMMSSms(rCharacteristicValueData.alarm_state_since()));
  fprintf(outstream, "%sSub characteristic values:           \t%d\n", PBLANKS, rCharacteristicValueData.sub_characteristic_values_size());
  for (int j = 0; j < rCharacteristicValueData.sub_characteristic_values_size(); ++j)
  {
    const smartcheck::SubCharacteristicValueData& r_sub_charval = rCharacteristicValueData.sub_characteristic_values(j);
    fprintf(outstream, "%s%2d.: ", PBLANKS, j+1);
    UuidStringOut("Characteristic value config:    \t", r_sub_charval.characteristic_value_config_current_uuid(), 0, __FUNCTION__);
    Blanks += 5;
    fprintf(outstream, "%sValue:                          \t%f\n", PBLANKS, r_sub_charval.numerical_values().value());
    fprintf(outstream, "%sAlarm status:                   \t%d\n", PBLANKS, (int)r_sub_charval.numerical_values().alarm_state());
    fprintf(outstream, "%sAlarm state since:              \t%s\n", PBLANKS,
            TimestampAsYYYYMMDDHHMMSSms(r_sub_charval.numerical_values().alarm_state_since()));
    fprintf(outstream, "%sAlarm map index:                \t%u\n", PBLANKS, r_sub_charval.numerical_values().alarm_index());
    fprintf(outstream, "%sLower main alarm limit:         \t%f\n", PBLANKS, r_sub_charval.numerical_values().lower_main_alarm());
    fprintf(outstream, "%sLower pre alarm limit:          \t%f\n", PBLANKS, r_sub_charval.numerical_values().lower_pre_alarm());
    fprintf(outstream, "%sUpper pre alarm limit:          \t%f\n", PBLANKS, r_sub_charval.numerical_values().upper_pre_alarm());
    fprintf(outstream, "%sUpper main alarm limit:         \t%f\n", PBLANKS, r_sub_charval.numerical_values().upper_main_alarm());
    fprintf(outstream, "%sLearning mode active:           \t%f\n", PBLANKS, r_sub_charval.numerical_values().learning_mode());
    UuidStringOut("Unit uuid:                      \t", r_sub_charval.numerical_values().unit_uuid(), Blanks, __FUNCTION__);
    fprintf(outstream, "%sUnit string:                    \t%s\n", PBLANKS, r_sub_charval.numerical_values().unit_name().c_str());
    fprintf(outstream, "%sTimesignal uudis:               \t%d\n", PBLANKS, r_sub_charval.timesignal_uuid_size());
    for (int k = 0; k < r_sub_charval.timesignal_uuid_size(); ++k)
    {
      fprintf(outstream, "%s%2d.: ", PBLANKS, k+1);
      UuidStringOut("                           \t", r_sub_charval.timesignal_uuid(k), 0, __FUNCTION__);
    }
    UuidStringOut("Timesignal unit uuid:           \t", r_sub_charval.timesignal_unit_uuid(), Blanks, __FUNCTION__);
    fprintf(outstream, "%sTimesignal unit name:           \t%s\n", PBLANKS, r_sub_charval.timesignal_unit_name().c_str());
    fprintf(outstream, "%sTimesignal signal type:         \t%d\n", PBLANKS, r_sub_charval.signal_type());
    Blanks -= 5;
  }

}

void PrintJobData(const smartcheck::JobData &rJobData)
{
  int Blanks = 0;

  fprintf(outstream, "\n");
  UuidStringOut("Job data uuid:                             \t", rJobData.job_data_uuid(), Blanks, __FUNCTION__);
  UuidStringOut("Job configuration current uuid:            \t", rJobData.job_config_current_uuid(), Blanks, __FUNCTION__);
  fprintf(outstream, "Measurement time:                         \t%s\n", TimestampAsYYYYMMDDHHMMSSms(rJobData.timestamp()));
  fprintf(outstream, "Alarm status:                             \t%d\n", (int)rJobData.alarm_state());
  fprintf(outstream, "Alarm state since:                        \t%s\n", TimestampAsYYYYMMDDHHMMSSms(rJobData.alarm_state_since()));

  fprintf(outstream, "Characteristic values:                    \t%d\n", rJobData.characteristic_values_size());
  for (int i = 0; i < rJobData.characteristic_values_size(); ++i)
  {
    const smartcheck::CharacteristicValueData& r_charval = rJobData.characteristic_values(i);
    fprintf(outstream, "%2d.: ", i+1);
    Blanks += 5;
    PrintCharacteristicValueData(r_charval, Blanks);
  }
}



int main(int argc, char **argv)
{
  PrintVersionNumber();
  bool is_job_data = true;

  int buffer_length = CheckCommandLineParameters(argc, argv, "job data", outstream);

  char* buffer = ReadInputFileIntoBuffer(argv[1], buffer_length);

  ConvertHexToBinIfNeeded(buffer_length, buffer);

  smartcheck::TransferMessage transfer_message;

  bool parse_success = transfer_message.ParseFromArray(buffer, (int) buffer_length);
  if (parse_success && transfer_message.IsInitialized() && transfer_message.has_job_data())
  {
    fprintf(stderr, "Job data is in ProtoBuf TransferMessage format\n");
    PrintJobData(transfer_message.job_data());
  }
  else if (parse_success && transfer_message.IsInitialized() && transfer_message.has_characteristic_value_data())
  {
    fprintf(stderr, "Characteristic value data is in TransferMessage format\n");
    PrintCharacteristicValueData(transfer_message.characteristic_value_data(), 0);
    is_job_data = false;
  }
  else
  {
    smartcheck::JobData job_data;
    smartcheck::CharacteristicValueData charval_data;
    if (job_data.ParseFromArray(buffer, (int) buffer_length) && job_data.IsInitialized()
        && !job_data.job_data_uuid().empty())
    {
      fprintf(stderr, "Job data is in ProtoBuf format\n");
      PrintJobData(job_data);
    }
    else if (charval_data.ParseFromArray(buffer, (int) buffer_length) && charval_data.IsInitialized()
        && !charval_data.characteristic_value_config_current_uuid().empty())
    {
      fprintf(stderr, "Characteristic value data is in ProtoBuf format\n");
      PrintCharacteristicValueData(charval_data, 0);
      is_job_data = false;
    }
    else
    {
      fprintf(stderr, "Error: Could not parse job data or characteristic value data from %s\n", argv[1]);
    }
  }

  free(buffer);

  if (argc == 3)
  {
    fclose(outstream);
    if (is_job_data)
    {
      fprintf(stdout, "%s: Success: Job data written to file %s\n", argv[0], argv[2]);
    }
    else
    {
      fprintf(stdout, "%s: Success: Characteristic value data written to file %s\n", argv[0], argv[2]);
    }
  }

  exit(0);
}

