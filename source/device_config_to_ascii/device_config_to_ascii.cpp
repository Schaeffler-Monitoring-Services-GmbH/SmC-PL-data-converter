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
 This programm decodes attached DeviceConfigData from email attachment into a readable text.
 The attached file has the sufix "scdc" - (s)mart(c)heck(d)evice(c)onfig
 The attachment is serialized by googles protobuf.
 */

#include <time.h>
#include <zlib.h>
#include "../common/datatypes.h"
#include "../common/helper_functions.h"
#include "TransferMessage.pb.h"
#include "DeviceConfig.pb.h"
#include "JobConfig.pb.h"

#ifdef _MSC_VER
#include<winsock.h>
#define sprintf sprintf_s
#define sscanf sscanf_s
#define gmtime_r(x,y) gmtime_s(y,x)
#undef uuid_t
#pragma comment(lib, "Ws2_32.lib")
#else
#include <arpa/inet.h>
#endif

static FILE *outstream = stdout;

const char* TimestampAsYYYYMMDDHHMMSSms(timestamp_t Timestamp)
{
  static char buf[64];
  int year = 0;
  int month = 0;
  int day = 0;
  int hour = 0;
  int minute = 0;
  int second = 0;
  struct tm timeStruct;

  if (!Timestamp)
  {
    sprintf(buf, "-");
    return buf;
  }

  memset(&timeStruct, 0, sizeof(struct tm));
  const time_t ts = Timestamp / 1000000;
  int ms = Timestamp % 1000;
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

std::string ExtractTranslateFromName(const std::string &rName)
{
  std::string text = rName;
  std::string prefix = "$translate(\"";
  std::string suffix = "\")";
  size_t start_pos = 0;
  size_t end_pos = 0;
  while (start_pos < text.length() && (start_pos = text.find(prefix, start_pos)) != std::string::npos
      && end_pos != std::string::npos)
  {
    end_pos = text.find(suffix, start_pos);
    if (end_pos != std::string::npos)
    {
      std::string var_name = text.substr(start_pos + prefix.length(), end_pos - start_pos - prefix.length());
      text = text.substr(0, start_pos) + var_name + text.substr(end_pos + 2, text.length());
      start_pos = start_pos + 1;
    }
  }
  return (text);

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

char* LongFloatToString(Uint32_t NoDigits, Uint32_t NoDecimals, char Delimiter, Float64_t FloatValue)
{
  static char buffer[64];

  if (std::isnan(FloatValue))
  {
    if (NoDigits == 0)
    {
      NoDigits = 1;
    }
    memset(buffer, ' ', NoDigits);
    buffer[NoDigits - 1] = Delimiter;
    buffer[NoDigits] = 0;
  }
  else
  {
    if (NoDigits)
    {
      snprintf(buffer, sizeof(buffer), "%*.*f", NoDigits, NoDecimals, FloatValue);
    }
    else
    {
      snprintf(buffer, sizeof(buffer), "%f", FloatValue);
    }
  }
  return buffer;

//  snprintf(buffer, sizeof(buffer), pFormat, FloatValue);
  if (!strncmp(buffer, "nan", 3)) // not a number
  {
    buffer[0] = Delimiter;
    buffer[1] = 0;
  }
  return buffer;
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

int UncompressData(Bytef *pDest, uLongf *pDestLen, const Bytef *pSource, uLong SourceLen, int WindowBits)
{
  z_stream stream;
  int err = 0;

  stream.next_in = (Bytef*) pSource;
  stream.avail_in = (uInt) SourceLen;
  /* Check for source > 64K on 16-bit machine: */
  if ((uLong) stream.avail_in != SourceLen) return Z_BUF_ERROR;

  stream.next_out = pDest;
  stream.avail_out = (uInt) *pDestLen;
  if ((uLong) stream.avail_out != *pDestLen) return Z_BUF_ERROR;

  stream.zalloc = (alloc_func) 0;
  stream.zfree = (free_func) 0;

  err = inflateInit2(&stream, WindowBits);
  if (err != Z_OK) return err;

  err = inflate(&stream, Z_FINISH);
  if (err != Z_STREAM_END)
  {
    (void) inflateEnd(&stream);
    if (err == Z_NEED_DICT || (err == Z_BUF_ERROR && stream.avail_in == 0)) return Z_DATA_ERROR;
    return err;
  }
  *pDestLen = stream.total_out;

  err = inflateEnd(&stream);
  return err;
}

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

// ########################################################################################

void PrintFrequencyBand(const smartcheck::FrequencyBand &rFrequencyBand, int Blanks, int Index)
{

  fprintf(outstream, "%s%s[%d]\n", PBLANKS, "Frequency band", Index);
  Blanks += 4;

  fprintf(outstream, "%sAbsolute:         \t%s\n", PBLANKS, rFrequencyBand.absolute() == true ? "yes" : "no ");
  fprintf(outstream, "%sBottom frequency: \t%f\n", PBLANKS, rFrequencyBand.bottom_frequency());
  fprintf(outstream, "%sStop frequency:    \t%f\n", PBLANKS, rFrequencyBand.top_frequency());
}

void PrintBaseMeasurementParameter(const smartcheck::BaseMeasurementParameter &rBaseMeasurementParameter, int Blanks)
{
  fprintf(outstream, "%s%s\n", PBLANKS, "Base measurement parameter");
  Blanks += 2;

  fprintf(outstream, "%sMaximum speed:   \t%f\n", PBLANKS, rBaseMeasurementParameter.maximum_speed());
}

void PrintBeltDriveParameter(const smartcheck::BeltDriveParameter &rBeltDriveParameter, int Blanks)
{
  fprintf(outstream, "%s%s\n", PBLANKS, "Belt drive parameter");
  Blanks += 2;

  fprintf(outstream, "%sBelt length:               \t%f\n", PBLANKS, rBeltDriveParameter.belt_length());
  fprintf(outstream, "%sDriving pulley diameter:   \t%f\n", PBLANKS, rBeltDriveParameter.driving_pulley_diameter());
  fprintf(outstream, "%sDriven pulley diameter:    \t%f\n", PBLANKS, rBeltDriveParameter.driven_pulley_diameter());
  fprintf(outstream, "%sMaximum speed:             \t%f\n", PBLANKS, rBeltDriveParameter.maximum_speed());
}

void PrintConditionGuardParameter(const smartcheck::ConditionGuardParameter &rConditionGuardParameter, int Blanks)
{
}

void PrintCouplingParameter(const smartcheck::CouplingParameter &rCouplingParameter, int Blanks)
{
  fprintf(outstream, "%s%s\n", PBLANKS, "Coupling parameter");
  Blanks += 2;

  fprintf(outstream, "%sNumber of bolts: \t%d\n", PBLANKS, rCouplingParameter.number_of_bolts());
  fprintf(outstream, "%sMaximum speed:   \t%f\n", PBLANKS, rCouplingParameter.maximum_speed());
}

void PrintDefaultMeasurementParameter(const smartcheck::DefaultMeasurementParameter rDefaultMeasurementParameter,
                                      int Blanks)
{
  fprintf(outstream, "%s%s\n", PBLANKS, "Default measurement parameter");
  Blanks += 2;

  fprintf(outstream, "%sMaximum speed: \t%f\n", PBLANKS, rDefaultMeasurementParameter.maximum_speed());
}

void PrintFanParameter(const smartcheck::FanParameter &rFanParameter, int Blanks)
{
  fprintf(outstream, "%s%s\n", PBLANKS, "Fan parameter");
  Blanks += 2;

  fprintf(outstream, "%sNumber of blades: \t%d\n", PBLANKS, rFanParameter.number_of_blades());
  fprintf(outstream, "%sMaximum speed:    \t%f\n", PBLANKS, rFanParameter.maximum_speed());
}

void PrintGearStageParameter(const smartcheck::GearStageParameter &rGearStageParameter, int Blanks)
{
  fprintf(outstream, "%s%s\n", PBLANKS, "Gear stage parameter");
  Blanks += 2;

  fprintf(outstream, "%sName: \t%d\n", PBLANKS,
            rGearStageParameter. name().c_str());
  fprintf(outstream, "%sNumber of teeth driving wheel: \t%d\n", PBLANKS,
          rGearStageParameter.number_of_teeth_driving_wheel());
  fprintf(outstream, "%sNumber of teeth driven wheel:  \t%d\n", PBLANKS,
          rGearStageParameter.number_of_teeth_driven_wheel());
  fprintf(outstream, "%sMaximum speed:                 \t%f\n", PBLANKS, rGearStageParameter.maximum_speed());
  fprintf(outstream, "%sRotational speed ratio           \t%f\n", PBLANKS,
          rGearStageParameter.rotational_speed_ratio());
  fprintf(outstream, "%sRotational speed ratio in Hz     \t%f\n", PBLANKS,
          rGearStageParameter.rotational_speed_ratio_in_hz());
}

void PrintJournalBearingParameter(const smartcheck::JournalBearingParameter &rJournalBearingParameter, int Blanks)
{
  fprintf(outstream, "%s%s\n", PBLANKS, "Journal bearing parameter");
  Blanks += 2;

  fprintf(outstream, "%sMaximum speed: \t%f\n", PBLANKS, rJournalBearingParameter.maximum_speed());
}

void PrintLddDimensionParameter(const smartcheck::LddDimensionParameter &rLddDimensionParameter, int Blanks, int Index)
{
  if (Index >= 0)
  {
    fprintf(outstream, "%s%s[%d]\n", PBLANKS, "Ldd dimension parameter", Index);
  }
  Blanks += 4;

  fprintf(outstream, "%sNumber of fields: \t%d\n", PBLANKS, rLddDimensionParameter.number_of_fields());
  fprintf(outstream, "%sLower border:     \t%f\n", PBLANKS, rLddDimensionParameter.lower_border());
  fprintf(outstream, "%sUpper border:     \t%f\n", PBLANKS, rLddDimensionParameter.upper_border());
  UuidStringOut("Unit uuid:        \t", rLddDimensionParameter.unit_uuid(), Blanks, __FUNCTION__);
  fprintf(outstream, "%sUnit name:        \t%s\n", PBLANKS, rLddDimensionParameter.unit_name().c_str());
  UuidStringOut("Input channel:    \t", rLddDimensionParameter.input_channel(), Blanks, __FUNCTION__);
}

void PrintLoadDurationDistributionParameter(
    const smartcheck::LoadDurationDistributionParameter &rLoadDurationDistributionParameter, int Blanks)
{
  fprintf(outstream, "%s%s\n", PBLANKS, "Load duration distribution parameter");
  Blanks += 2;

  fprintf(outstream, "%sFrom date: \t%s\n", PBLANKS,
          TimestampAsYYYYMMDDHHMMSSms(rLoadDurationDistributionParameter.from_date()));
  fprintf(outstream, "%sTo date:   \t%s\n", PBLANKS,
          TimestampAsYYYYMMDDHHMMSSms(rLoadDurationDistributionParameter.to_date()));
  fprintf(outstream, "%sComment:   \t%s\n", PBLANKS, rLoadDurationDistributionParameter.comment().c_str());

  // repeated LddDimensionParameter dimension
  for (int i = 0; i < rLoadDurationDistributionParameter.dimension_size(); ++i)
  {
    PrintLddDimensionParameter(rLoadDurationDistributionParameter.dimension(i), Blanks, i);
  }
}

void PrintTorqueCorrectionParameter(const smartcheck::TorqueCorrectionParameter &rTorqueCorrectionParameter, int Blanks)
{
  fprintf(outstream, "%s%s\n", PBLANKS, "Torque channel:");
  Blanks += 4;
  fprintf(outstream, "%sNominal torque: \t%f\n", PBLANKS, rTorqueCorrectionParameter.nominal_torque());
  fprintf(outstream, "%sm(slope):       \t%f\n", PBLANKS, rTorqueCorrectionParameter.slope());
  fprintf(outstream, "%sn(offset):       \t%f\n", PBLANKS, rTorqueCorrectionParameter.offset());
  Blanks -= 4;
  PrintLddDimensionParameter(rTorqueCorrectionParameter.torque_channel(), Blanks, -1);
}

void PrintOilStatusParameter(const smartcheck::OilStatusParameter &rOilStatusParameter, int Blanks)
{
  fprintf(outstream, "%s%s\n", PBLANKS, "Oil status parameter:");
  Blanks += 4;
  fprintf(outstream, "%sOil type:              \t%s\n", PBLANKS, rOilStatusParameter.oil_name().c_str());
  fprintf(outstream, "%sRuntime correction[h]: \t%d\n", PBLANKS, rOilStatusParameter.runtime_correction());
  if (rOilStatusParameter.last_oil_change() > 0)
  {
    fprintf(outstream, "%sLast oil change:       \t%s\n", PBLANKS,
            TimestampAsYYYYMMDDHHMMSSms(rOilStatusParameter.last_oil_change()));
    fprintf(outstream, "%sOverlap[h]:            \t%d\n", PBLANKS, rOilStatusParameter.overlap());
    fprintf(outstream, "%sPrevious theta equiv.: \t%g\n", PBLANKS, rOilStatusParameter.previous_theta_equiv());
  }
  fprintf(outstream, "%sTemperature Channel:   \t\n", PBLANKS);

  PrintLddDimensionParameter(rOilStatusParameter.oil_temperature_channel_parameter(), Blanks, -1);

  if (rOilStatusParameter.has_torque_correction_parameter())
  {
    PrintTorqueCorrectionParameter(rOilStatusParameter.torque_correction_parameter(), Blanks);
  }

}

const char* TriggerValidatorChannelMonitorCalculationTypeEnum2String(
    const smartcheck::TriggerValidatorChannelMonitorCalculationTypeEnum calculation_type)
{
  switch (calculation_type)
  {
    case smartcheck::TVC_Crest:
      return "Crest factor";
      break;
    case smartcheck::TVC_DC:
      return "DC";
      break;
    case smartcheck::TVC_Peak:
      return "Unsigned peak";
      break;
    case smartcheck::TVC_Peak2Peak:
      return "Peak-peak";
      break;
    case smartcheck::TVC_RMS:
      return "RMS";
      break;
    case smartcheck::TVC_SignedPeak:
      return "Signed peak";
      break;
    default:
      return "Unknown";
      break;

  }
  return "Unknown";
}

void PrintChannelMonitorCharacteristicValueParameter(
    const smartcheck::ChannelMonitorCharacteristicValueParameter &rChannelMonitorCharacteristicValueParameter,
    int Blanks, int Index)
{
  fprintf(outstream, "%s%s[%d]\n", PBLANKS, "Channel monitor parameter", Index);
  Blanks += 4;

  fprintf(
      outstream,
      "%sType:             \t%s\n",
      PBLANKS,
      TriggerValidatorChannelMonitorCalculationTypeEnum2String(
          rChannelMonitorCharacteristicValueParameter.calculation_type()));
  fprintf(outstream, "%sCalculation time: \t%d\n", PBLANKS,
          rChannelMonitorCharacteristicValueParameter.calculation_time());
  fprintf(outstream, "%sSignal length:    \t%d\n", PBLANKS,
          rChannelMonitorCharacteristicValueParameter.signal_length());
  fprintf(outstream, "%sLead time:        \t%d\n", PBLANKS, rChannelMonitorCharacteristicValueParameter.lead_time());
  UuidStringOut("Input channel:    \t", rChannelMonitorCharacteristicValueParameter.input_channel(), Blanks,
                __FUNCTION__);
}

void PrintChannelMonitorParameter(const smartcheck::ChannelMonitorParameter &rChannelMonitorParameter, int Blanks)
{
  fprintf(outstream, "%s%s\n", PBLANKS, "Channel monitor parameter");
  Blanks += 2;
  for (int i = 0; i < rChannelMonitorParameter.channel_monitors_size(); ++i)
  {
    PrintChannelMonitorCharacteristicValueParameter(rChannelMonitorParameter.channel_monitors(i), Blanks, i);
  }

}

const char* ProcessSignalMonitorCalculationTypeEnum2String(const smartcheck::CalculationTypeEnum calculation_type)
{
  switch (calculation_type)
  {
    case smartcheck::DC:
      return "DC";
      break;
    case smartcheck::TimeSignalRms:
      return "RMS (with DC)";
      break;
    case smartcheck::TimeSignalRmsWithoutDC:
      return "RMS (without DC)";
      break;
    case smartcheck::Peak:
      return "Unsigned peak";
      break;
    case smartcheck::SignedPeak:
      return "Signed peak";
      break;
    case smartcheck::Peak2Peak:
      return "Peak-peak";
      break;
    case smartcheck::Crestfactor:
      return "Crest factor";
      break;
    case smartcheck::Kurtosis:
      return "Excess kurtosis";
      break;
    default:
      break;

  }
  static char string[25];
  snprintf(string, 25, "Unknown (%d)", (int) calculation_type);
  return string;
}

void PrintProcessSignalMonitorCharacteristicValueParameter(
    const smartcheck::ProcessSignalMonitorCharacteristicValueParameter &rProcessSignalMonitorCharacteristicValueParameter,
    int Blanks, int Index)
{
  fprintf(outstream, "%s%s[%d]\n", PBLANKS, "Process signal monitor parameter", Index);
  Blanks += 4;

  fprintf(
      outstream,
      "%sType:          \t%s\n",
      PBLANKS,
      ProcessSignalMonitorCalculationTypeEnum2String(
          rProcessSignalMonitorCharacteristicValueParameter.calculation_type()));
  UuidStringOut("Input channel: \t", rProcessSignalMonitorCharacteristicValueParameter.input_channel(), Blanks,
                __FUNCTION__);
}

void PrintProcessSignalMonitorParameter(const smartcheck::ProcessSignalMonitorParameter &rProcessSignalMonitorParameter,
                                        int Blanks)
{
  fprintf(outstream, "%s%s\n", PBLANKS, "Process signal monitor parameter");
  Blanks += 2;
  fprintf(outstream, "%sSignal length:    \t%d\n", PBLANKS, rProcessSignalMonitorParameter.signal_length());
  for (int i = 0; i < rProcessSignalMonitorParameter.process_signal_monitors_size(); ++i)
  {
    PrintProcessSignalMonitorCharacteristicValueParameter(rProcessSignalMonitorParameter.process_signal_monitors(i),
                                                          Blanks, i);
  }

}

void PrintPumpParameter(const smartcheck::PumpParameter &rPumpParameter, int Blanks)
{
  fprintf(outstream, "%s%s\n", PBLANKS, "Pump parameter");
  Blanks += 2;

  fprintf(outstream, "%sNumber of pump vanes: \t%ld\n", PBLANKS, rPumpParameter.number_of_pump_vanes());
  fprintf(outstream, "%sNaximum speed:        \t%f\n", PBLANKS, rPumpParameter.maximum_speed());
}

void PrintRainflowCountParameter(const smartcheck::RainflowCountParameter &rRainflowCountParameter, int Blanks)
{
  fprintf(outstream, "%s%s\n", PBLANKS, "Rainflow count parameter");
  Blanks += 2;

  fprintf(outstream, "%sFrom date: \t%s\n", PBLANKS, TimestampAsYYYYMMDDHHMMSSms(rRainflowCountParameter.from_date()));
  fprintf(outstream, "%sTo date:   \t%s\n", PBLANKS, TimestampAsYYYYMMDDHHMMSSms(rRainflowCountParameter.to_date()));
  fprintf(outstream, "%sNumber of fields: \t%d\n", PBLANKS, rRainflowCountParameter.number_of_fields());
  fprintf(outstream, "%sLower border:     \t%f\n", PBLANKS, rRainflowCountParameter.lower_border());
  fprintf(outstream, "%sUpper border:     \t%f\n", PBLANKS, rRainflowCountParameter.upper_border());
  fprintf(outstream, "%sHysteresis:       \t%f\n", PBLANKS, rRainflowCountParameter.hysteresis());
  UuidStringOut("Unit uuid:        \t", rRainflowCountParameter.unit_uuid(), Blanks, __FUNCTION__);
  fprintf(outstream, "%sUnit name:        \t%s\n", PBLANKS, rRainflowCountParameter.unit_name().c_str());
  fprintf(outstream, "%sComment:          \t%s\n", PBLANKS, rRainflowCountParameter.comment().c_str());
  UuidStringOut("Input channel:    \t", rRainflowCountParameter.input_channel(), Blanks, __FUNCTION__);
}

void PrintRollerBearingParameter(const smartcheck::RollerBearingParameter &rRollerBearingParameter, int Blanks)
{
  fprintf(outstream, "%s%s\n", PBLANKS, "Roller bearing parameter");
  Blanks += 2;

  UuidStringOut("Bearing uuid:                    \t", rRollerBearingParameter.bearing_uuid(), Blanks, __FUNCTION__);
  fprintf(outstream, "%sBearing name:                    \t%s\n", PBLANKS,
          rRollerBearingParameter.bearing_name().c_str());
  UuidStringOut("Manufacturer uuid:               \t", rRollerBearingParameter.bearing_manufacturer_uuid(), Blanks,
                __FUNCTION__);
  fprintf(outstream, "%sManufacturer name:               \t%s\n", PBLANKS,
          rRollerBearingParameter.bearing_manufacturer_name().c_str());
  fprintf(outstream, "%sBall pass frequency inner race:  \t%f\n", PBLANKS,
          rRollerBearingParameter.ball_pass_frequency_inner_race());
  fprintf(outstream, "%sBall pass frequency outer race:  \t%f\n", PBLANKS,
          rRollerBearingParameter.ball_pass_frequency_outer_race());
  fprintf(outstream, "%sBall spin frequency:             \t%f\n", PBLANKS,
          rRollerBearingParameter.ball_spin_frequency());
  fprintf(outstream, "%sFundamental train frequency:     \t%f\n", PBLANKS,
          rRollerBearingParameter.fundamental_train_frequency());
  fprintf(outstream, "%sFixed outer race:                \t%s\n", PBLANKS,
          rRollerBearingParameter.fixed_outer_race() == true ? "yes" : "no ");
  fprintf(outstream, "%sFixed inner race:                \t%s\n", PBLANKS,
          rRollerBearingParameter.fixed_inner_race() == true ? "yes" : "no ");
  fprintf(outstream, "%sMaximum speed:                   \t%f\n", PBLANKS, rRollerBearingParameter.maximum_speed());
  fprintf(outstream, "%sRotational speed ratio           \t%f\n", PBLANKS,
          rRollerBearingParameter.rotational_speed_ratio());
  fprintf(outstream, "%sRotational speed ratio in Hz     \t%f\n", PBLANKS,
          rRollerBearingParameter.rotational_speed_ratio_in_hz());
}

void PrintShaftParameter(const smartcheck::ShaftParameter &rShaftParameter, int Blanks)
{
  fprintf(outstream, "%s%s\n", PBLANKS, "Shaft parameter");
  Blanks += 2;

  rShaftParameter.maximum_speed();
  fprintf(outstream, "%sMaximum speed: \t%f\n", PBLANKS, rShaftParameter.maximum_speed());
}

void PrintTrackedFrequencyBandsParameter(
    const smartcheck::TrackedFrequencyBandsParameter &rTrackedFrequencyBandsParameter, int Blanks)
{
  fprintf(outstream, "%s%s\n", PBLANKS, "Tracked frequency bands parameter");
  Blanks += 2;

  fprintf(outstream, "%sMaximum speed: \t%f\n", PBLANKS, rTrackedFrequencyBandsParameter.maximum_speed());

  // repeated FrequencyBand frequency_bands
  for (int i = 0; i < rTrackedFrequencyBandsParameter.frequency_bands_size(); ++i)
  {
    PrintFrequencyBand(rTrackedFrequencyBandsParameter.frequency_bands(i), Blanks, i);
  }
}

void PrintAbsoluteFrequencyBandsParameter(
    const smartcheck::AbsoluteFrequencyBandsParameter &rAbsoluteFrequencyBandsParameter, int Blanks)
{
  fprintf(outstream, "%s%s\n", PBLANKS, "Absolute frequency bands parameter");
  Blanks += 2;

  // repeated FrequencyBand frequency_bands
  for (int i = 0; i < rAbsoluteFrequencyBandsParameter.frequency_bands_size(); ++i)
  {
    PrintFrequencyBand(rAbsoluteFrequencyBandsParameter.frequency_bands(i), Blanks, i);
  }
}

void PrintMachineAnalysisEssentialsParameter(
    const smartcheck::MachineAnalysisEssentialsParameter &rMachineAnalysisEssentialsParameter, int Blanks)
{
  UuidStringOut("Vibration channel uuid:\t", rMachineAnalysisEssentialsParameter.vibration_channel(), Blanks,
                __FUNCTION__);
  if (rMachineAnalysisEssentialsParameter.temperature_channel().length() > 0)
  {
    UuidStringOut("Speed channel uuid:    \t", rMachineAnalysisEssentialsParameter.temperature_channel(), Blanks,
                  __FUNCTION__);
  }
  if (rMachineAnalysisEssentialsParameter.load_channel().length() > 0)
  {
    UuidStringOut("Speed channel uuid:    \t", rMachineAnalysisEssentialsParameter.load_channel(), Blanks,
                  __FUNCTION__);
  }
  if (rMachineAnalysisEssentialsParameter.speed_channel().length() > 0)
  {
    UuidStringOut("Speed channel uuid:    \t", rMachineAnalysisEssentialsParameter.speed_channel(), Blanks,
                  __FUNCTION__);
    fprintf(outstream, "%sMaximum speed:   \t%f\n", PBLANKS, rMachineAnalysisEssentialsParameter.maximum_speed());
  }
  if (rMachineAnalysisEssentialsParameter.roller_bearings_size() > 0)
  {
    fprintf(outstream, "%sRoller bearings:\n", PBLANKS);
    Blanks += 4;
    for (int i = 0; i < rMachineAnalysisEssentialsParameter.roller_bearings_size(); ++i)
    {
      PrintRollerBearingParameter(rMachineAnalysisEssentialsParameter.roller_bearings(i), Blanks);
    }
  }
  if (rMachineAnalysisEssentialsParameter.gear_stages_size() > 0)
  {
    fprintf(outstream, "%Gear stages:\n", PBLANKS);
    Blanks += 4;
    for (int i = 0; i < rMachineAnalysisEssentialsParameter.gear_stages_size(); ++i)
    {
      PrintGearStageParameter(rMachineAnalysisEssentialsParameter.gear_stages(i), Blanks);
    }
  }
}

void PrintSchaefflerCloudParameter(const smartcheck::SchaefflerCloudParameter &rSchaefflerCloudParameter, int Blanks)
{
  UuidStringOut("Vibration channel uuid:\t", rSchaefflerCloudParameter.vibration_channel(), Blanks, __FUNCTION__);
  UuidStringOut("Temperature chan. uuid:\t", rSchaefflerCloudParameter.temperature_channel(), Blanks, __FUNCTION__);
  if (rSchaefflerCloudParameter.speed_channel().length() > 0)
  {
    UuidStringOut("Speed channel uuid:    \t", rSchaefflerCloudParameter.speed_channel(), Blanks, __FUNCTION__);
    fprintf(outstream, "%sMaximum speed:   \t%f\n", PBLANKS, rSchaefflerCloudParameter.maximum_speed());
  }
  if (rSchaefflerCloudParameter.roller_bearings_size() > 0)
  {
    fprintf(outstream, "%sRoller bearings:\n", PBLANKS);
    Blanks += 4;
    for (int i = 0; i < rSchaefflerCloudParameter.roller_bearings_size(); ++i)
    {
      PrintRollerBearingParameter(rSchaefflerCloudParameter.roller_bearings(i), Blanks);
    }
  }
  if (rSchaefflerCloudParameter.gear_stages_size() > 0)
  {
    fprintf(outstream, "%Gear stages:\n", PBLANKS);
    Blanks += 4;
    for (int i = 0; i < rSchaefflerCloudParameter.gear_stages_size(); ++i)
    {
      PrintGearStageParameter(rSchaefflerCloudParameter.gear_stages(i), Blanks);
    }
  }
}

void PrintAlarmMapDimension(const smartcheck::AlarmMapDimension &rAlarmMapDimension, int Blanks)
{
  fprintf(outstream, "%s%s\n", PBLANKS, "Alarm map dimension");
  Blanks += 2;

  UuidStringOut("Characteristic value config:  ", rAlarmMapDimension.characteristic_value_config(), Blanks,
                __FUNCTION__);
  fprintf(outstream, "%sLower border:                \t%f\n", PBLANKS, rAlarmMapDimension.lower_border());
  fprintf(outstream, "%sUpper border:                \t%f\n", PBLANKS, rAlarmMapDimension.upper_border());
}

void printAlarmMapConfig(const smartcheck::AlarmMapConfig &rAlarmMapConfig, int Blanks)
{
  if (rAlarmMapConfig.has_first_dimension() == false)
  {
    return;
  }

  fprintf(outstream, "%s%s\n", PBLANKS, "Alarm map config:");
  Blanks += 2;

  PrintAlarmMapDimension(rAlarmMapConfig.first_dimension(), Blanks);
  int columns = 10;
  int rows = 1;
  int num_of_elements = columns;

  Float64_t lbf, ubf, dbf, lbs, ubs, dbs;

  if (rAlarmMapConfig.has_second_dimension() == true)
  {
    PrintAlarmMapDimension(rAlarmMapConfig.second_dimension(), Blanks);
    num_of_elements *= num_of_elements;
    if (num_of_elements != rAlarmMapConfig.alarm_map_entries_size())
    {
      return;
    }
    rows = 10;
    lbs = rAlarmMapConfig.second_dimension().lower_border();
    ubs = rAlarmMapConfig.second_dimension().upper_border();
    dbs = (ubs - lbs) / 10.0;
  }
  else
  {
    lbs = ubs = dbs = 0.0;
  }

  lbf = rAlarmMapConfig.first_dimension().lower_border();
  ubf = rAlarmMapConfig.first_dimension().upper_border();
  dbf = (ubf - lbf) / 10.0;

  // headline X axis
  Blanks += 15;
  fprintf(outstream, "%s ", PBLANKS);
  for (int i = 0; i < columns; ++i)
  {
    fprintf(outstream, " \t%10.4lf", lbf + (i + 1) * dbf);
  }
  fprintf(outstream, "\n%s ", PBLANKS);
  for (int i = 0; i < columns; ++i)
  {
    fprintf(outstream, " \t%10.4lf", lbf + (i * dbf));
  }
  Blanks -= 15;

  fprintf(
      outstream,
      "\n%s----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------\n",
      PBLANKS);
  int num_cells = rows * columns;
  if (num_cells > rAlarmMapConfig.alarm_map_entries_size())
  {
    fprintf(outstream, "\n%sNo alarm map data available\n\n", PBLANKS);
    exit(0);
  }
  for (int row = 0; row < rows; ++row)
  {
    fprintf(outstream, "%s\t%10.4lf | ", PBLANKS, ubs - (row * dbs));
    for (int column = 0; column < columns; ++column)
    {
      fprintf(
          outstream,
          " \t%s",
          LongFloatToString(10, 4, '-',
                            rAlarmMapConfig.alarm_map_entries(column * rows + (rows - 1 - row)).upper_main_alarm()));
    }
    fprintf(outstream, "\n");

    fprintf(outstream, "%s\t%10.4lf | ", PBLANKS, ubs - (row * dbs) - dbs);
    for (int column = 0; column < columns; ++column)
    {
      fprintf(
          outstream,
          " \t%s",
          LongFloatToString(10, 4, '-',
                            rAlarmMapConfig.alarm_map_entries(column * rows + (rows - 1 - row)).upper_pre_alarm()));
    }
    fprintf(outstream, "\n");

    fprintf(outstream, "%s\t%s  ", PBLANKS, "  learning |");
    for (int column = 0; column < columns; ++column)
    {
      fprintf(
          outstream,
          "\t%s",
          rAlarmMapConfig.alarm_map_entries(column * rows + (rows - 1 - row)).use_learning_mode() == true ?
            "       yes" :
            "        no");
    }
    fprintf(outstream, "\n");
  }
}

void PrintAlarmConfig(const smartcheck::AlarmConfig &rAlarmConfig, int Blanks)
{
  fprintf(outstream, "%s%s\n", PBLANKS, "Alarm config");
  Blanks += 2;

  switch (rAlarmConfig.learning_mode_type())
  {
    case ::smartcheck::AlarmConfig_LearningModeType_Unknown:
      fprintf(outstream, "%sLearning mode type:             \t%s\n", PBLANKS, "Unknown");
      break;

    case ::smartcheck::AlarmConfig_LearningModeType_Disabled:
      fprintf(outstream, "%sLearning mode type:             \t%s\n", PBLANKS, "Disabled");
      break;

    case ::smartcheck::AlarmConfig_LearningModeType_Average:
      fprintf(outstream, "%sLearning mode type:             \t%s\n", PBLANKS, "Average");
      break;

    case ::smartcheck::AlarmConfig_LearningModeType_StandardDeviation:
      fprintf(outstream, "%sLearning mode type:             \t%s\n", PBLANKS, "StandardDeviation");
      break;

    default:
      break;
  }
  fprintf(outstream, "%sLearning mode number of values: \t%d\n", PBLANKS,
          rAlarmConfig.learning_mode_number_of_values());
  fprintf(outstream, "%sAlarm threshold overruns:       \t%d\n", PBLANKS, rAlarmConfig.alarm_threshold_overruns());
  fprintf(outstream, "%sAutomatic alarm reset:          \t%s\n", PBLANKS,
          rAlarmConfig.automatic_alarm_reset() == true ? "yes" : "no ");
  fprintf(outstream, "%sLower main alarm:               \t%s\n", PBLANKS,
          LongFloatToString(0, 0, '-', rAlarmConfig.lower_main_alarm()));
  fprintf(outstream, "%sLower pre alarm:                \t%f\n", PBLANKS, rAlarmConfig.lower_pre_alarm());
  fprintf(outstream, "%sUpper pre alarm:                \t%f\n", PBLANKS, rAlarmConfig.upper_pre_alarm());
  fprintf(outstream, "%sUpper main alarm:               \t%f\n", PBLANKS, rAlarmConfig.upper_main_alarm());

  if (rAlarmConfig.has_alarm_map())
  {
    printAlarmMapConfig(rAlarmConfig.alarm_map(), Blanks);
  }
}

void printSubCharacteristicValueConfig(const smartcheck::SubCharacteristicValueConfig &rSubCharacteristicValueConfig,
                                       int Blanks, int Index)
{
  fprintf(outstream, "%s%s[%d]\n", PBLANKS, "Sub characteristic value config", Index);
  Blanks += 4;

  UuidStringOut("Current uuid:       \t", rSubCharacteristicValueConfig.current_uuid(), Blanks, __FUNCTION__);
  UuidStringOut("Base uuid:          \t", rSubCharacteristicValueConfig.base_uuid(), Blanks, __FUNCTION__);
  fprintf(outstream, "%sChange date:        \t%s\n", PBLANKS,
          TimestampAsYYYYMMDDHHMMSSms(rSubCharacteristicValueConfig.change_date()));
  fprintf(outstream, "%sName:               \t%s\n", PBLANKS,
          ExtractTranslateFromName(rSubCharacteristicValueConfig.name()).c_str());
  fprintf(outstream, "%sActive:             \t%s\n", PBLANKS, rSubCharacteristicValueConfig.active() ? "yes" : "no");
  fprintf(outstream, "%sVariable:           \t%s\n", PBLANKS,
          ExtractTranslateFromName(rSubCharacteristicValueConfig.variable()).c_str());
  UuidStringOut("Template uuid:      \t", rSubCharacteristicValueConfig.template_uuid(), Blanks, __FUNCTION__);

  switch (rSubCharacteristicValueConfig.type())
  {
    case ::smartcheck::Unknown:
      fprintf(outstream, "%sType:               \t%s\n", PBLANKS, "Unknown");
      break;

    case ::smartcheck::CalculatedCharacteristicValue:
      fprintf(outstream, "%sType:               \t%s\n", PBLANKS, "Calculated characteristic value");
      break;

    case ::smartcheck::TimeSignalRms:
      fprintf(outstream, "%sType:               \t%s\n", PBLANKS, "Time signal RMS");
      break;

    case ::smartcheck::SpectralRms:
      fprintf(outstream, "%sType:               \t%s\n", PBLANKS, "Spectral RMS");
      break;

    case ::smartcheck::DC:
      fprintf(outstream, "%sType:               \t%s\n", PBLANKS, "DC");
      break;

    case ::smartcheck::Peak:
      fprintf(outstream, "%sType:               \t%s\n", PBLANKS, "Unsigned peak");
      break;

    case ::smartcheck::Peak2Peak:
      fprintf(outstream, "%sType:               \t%s\n", PBLANKS, "Peak-peak");
      break;

    case ::smartcheck::Crestfactor:
      fprintf(outstream, "%sType:               \t%s\n", PBLANKS, "Crest factor");
      break;

    case ::smartcheck::WellhausenCounts:
      fprintf(outstream, "%sType:               \t%s\n", PBLANKS, "Wellhausen counts");
      break;

    case ::smartcheck::PeriodicValue:
      fprintf(outstream, "%sType:               \t%s\n", PBLANKS, "Periodic value");
      break;

    case ::smartcheck::CarpetLevel:
      fprintf(outstream, "%sType:               \t%s\n", PBLANKS, "Carpet level");
      break;

    case ::smartcheck::ISO10816:
      fprintf(outstream, "%sType:               \t%s\n", PBLANKS, "ISO10816");
      break;

    case ::smartcheck::VDI3836:
      fprintf(outstream, "%sType:               \t%s\n", PBLANKS, "VDI3836");
      break;

    case ::smartcheck::ConditionGuard:
      fprintf(outstream, "%sType:               \t%s\n", PBLANKS, "Condition guard");
      break;

    case ::smartcheck::RainflowCount:
      fprintf(outstream, "%sType:               \t%s\n", PBLANKS, "Rainflow count");
      break;

    case ::smartcheck::LoadDurationDistribution:
      fprintf(outstream, "%sType:               \t%s\n", PBLANKS, "Load duration distribution");
      break;

    case ::smartcheck::OilTemperatureMonitor:
      fprintf(outstream, "%sType:               \t%s\n", PBLANKS, "Oil temperature monitor");
      break;

    case ::smartcheck::OilLifetimeEstimate:
      fprintf(outstream, "%sType:               \t%s\n", PBLANKS, "Oil lifetime estimate");
      break;

    case ::smartcheck::OilTemperatureOverrun:
      fprintf(outstream, "%sType:               \t%s\n", PBLANKS, "Oil temperature overrun");
      break;

    case ::smartcheck::OilThetaEquivalent:
      fprintf(outstream, "%sType:               \t%s\n", PBLANKS, "Oil theta equivalent");
      break;

    case ::smartcheck::OilTotalUsageTime:
      fprintf(outstream, "%sType:               \t%s\n", PBLANKS, "Oil total usage time");
      break;

    case ::smartcheck::Trigger:
      fprintf(outstream, "%sType:               \t%s\n", PBLANKS, "Trigger");
      break;

    case ::smartcheck::Validator:
      fprintf(outstream, "%sType:               \t%s\n", PBLANKS, "Validator");
      break;

    case ::smartcheck::HourMeter:
      fprintf(outstream, "%sType:               \t%s\n", PBLANKS, "Hour meter");
      break;

    case ::smartcheck::ChannelMonitor:
      fprintf(outstream, "%sType:               \t%s\n", PBLANKS, "Channel monitor");
      break;

    case ::smartcheck::SignedPeak:
      fprintf(outstream, "%sType:               \t%s\n", PBLANKS, "Signed peak");
      break;

    case ::smartcheck::Kurtosis:
      fprintf(outstream, "%sType:               \t%s\n", PBLANKS, "Excess kurtosis");
      break;

    case ::smartcheck::TimeSignalRmsWithoutDC:
      fprintf(outstream, "%sType:               \t%s\n", PBLANKS, "Time signal RMS (without DC)");
      break;
  }

  fprintf(outstream, "%sIs numerical:       \t%s\n", PBLANKS,
          rSubCharacteristicValueConfig.is_numerical() == true ? "yes" : "no ");

  if (rSubCharacteristicValueConfig.has_alarm_config())
  {
    PrintAlarmConfig(rSubCharacteristicValueConfig.alarm_config(), Blanks);
  }

  // repeated MeasurementConfig measurement_config
  for (int i = 0; i < rSubCharacteristicValueConfig.measurement_config_size(); ++i)
  {
    UuidStringOut("Measurement config: \t", rSubCharacteristicValueConfig.measurement_config(i), Blanks, __FUNCTION__);
  }
  UuidStringOut("Unit uuid:          \t", rSubCharacteristicValueConfig.unit_uuid(), Blanks, __FUNCTION__);
  fprintf(outstream, "%sUnit name:          \t%s\n", PBLANKS, rSubCharacteristicValueConfig.unit_name().c_str());

// repeated FrequencyBand frequency_bands
  for (int i = 0; i < rSubCharacteristicValueConfig.frequency_bands_size(); ++i)
  {
    PrintFrequencyBand(rSubCharacteristicValueConfig.frequency_bands(i), Blanks, i);
  }
// repeated SubCharacteristicValueConfig sub_characteristc_values
  for (int i = 0; i < rSubCharacteristicValueConfig.sub_characteristc_values_size(); ++i)
  {
    printSubCharacteristicValueConfig(rSubCharacteristicValueConfig.sub_characteristc_values(i), Blanks, i);
  }
}

void PrintCharacteristicValueConfig(const smartcheck::CharacteristicValueConfig &rCharacteristicValueConfig, int Blanks,
                                    int Index)
{
  fprintf(outstream, "%s%s[%d]\n", PBLANKS, "Characteristic value config", Index);
  Blanks += 4;

  UuidStringOut("Current uuid:    \t", rCharacteristicValueConfig.current_uuid(), Blanks, __FUNCTION__);
  UuidStringOut("Base uuid:       \t", rCharacteristicValueConfig.base_uuid(), Blanks, __FUNCTION__);
  fprintf(outstream, "%sChange date:     \t%s\n", PBLANKS,
          TimestampAsYYYYMMDDHHMMSSms(rCharacteristicValueConfig.change_date()));
  fprintf(outstream, "%sName:            \t%s\n", PBLANKS,
          ExtractTranslateFromName(rCharacteristicValueConfig.name()).c_str());
  fprintf(outstream, "%sActive:          \t%s\n", PBLANKS, rCharacteristicValueConfig.active() ? "yes" : "no");
  UuidStringOut("Template uuid:   \t", rCharacteristicValueConfig.template_uuid(), Blanks, __FUNCTION__);

  switch (rCharacteristicValueConfig.type())
  {
    case ::smartcheck::Unknown:
      fprintf(outstream, "%sType:            \t%s\n", PBLANKS, "Unknown");
      break;

    case ::smartcheck::CalculatedCharacteristicValue:
      fprintf(outstream, "%sType:            \t%s\n", PBLANKS, "Calculated characteristic value");
      break;

    case ::smartcheck::TimeSignalRms:
      fprintf(outstream, "%sType:            \t%s\n", PBLANKS, "Time signal Rms");
      break;

    case ::smartcheck::SpectralRms:
      fprintf(outstream, "%sType:            \t%s\n", PBLANKS, "Spectral Rms");
      break;

    case ::smartcheck::DC:
      fprintf(outstream, "%sType:            \t%s\n", PBLANKS, "DC");
      break;

    case ::smartcheck::Peak:
      fprintf(outstream, "%sType:            \t%s\n", PBLANKS, "Unsigned peak");
      break;

    case ::smartcheck::Peak2Peak:
      fprintf(outstream, "%sType:            \t%s\n", PBLANKS, "Peak-peak");
      break;

    case ::smartcheck::Crestfactor:
      fprintf(outstream, "%sType:            \t%s\n", PBLANKS, "Crest factor");
      break;

    case ::smartcheck::WellhausenCounts:
      fprintf(outstream, "%sType:            \t%s\n", PBLANKS, "Wellhausen counts");
      break;

    case ::smartcheck::PeriodicValue:
      fprintf(outstream, "%sType:            \t%s\n", PBLANKS, "Periodic value");
      break;

    case ::smartcheck::CarpetLevel:
      fprintf(outstream, "%sType:            \t%s\n", PBLANKS, "Carpet level");
      break;

    case ::smartcheck::ISO10816:
      fprintf(outstream, "%sType:            \t%s\n", PBLANKS, "ISO10816");
      break;

    case ::smartcheck::VDI3836:
      fprintf(outstream, "%sType:            \t%s\n", PBLANKS, "VDI3836");
      break;

    case ::smartcheck::ConditionGuard:
      fprintf(outstream, "%sType:            \t%s\n", PBLANKS, "Condition guard");
      break;

    case ::smartcheck::RainflowCount:
      fprintf(outstream, "%sType:            \t%s\n", PBLANKS, "Rainflow count");
      break;

    case ::smartcheck::LoadDurationDistribution:
      fprintf(outstream, "%sType:            \t%s\n", PBLANKS, "Load duration distribution");
      break;

    case ::smartcheck::OilTemperatureMonitor:
      fprintf(outstream, "%sType:               \t%s\n", PBLANKS, "Oil temperature monitor");
      break;

    case ::smartcheck::OilLifetimeEstimate:
      fprintf(outstream, "%sType:               \t%s\n", PBLANKS, "Oil lifetime estimate");
      break;

    case ::smartcheck::Trigger:
      fprintf(outstream, "%sType:            \t%s\n", PBLANKS, "Trigger");
      break;

    case ::smartcheck::Validator:
      fprintf(outstream, "%sType:             \t%s\n", PBLANKS, "Validator");
      break;

    case ::smartcheck::ChannelMonitor:
      fprintf(outstream, "%sType:            \t%s\n", PBLANKS, "Channel monitor");
      break;
  }

  fprintf(outstream, "%sIs numerical:    \t%s\n", PBLANKS,
          rCharacteristicValueConfig.is_numerical() == true ? "yes" : "no ");

  if (rCharacteristicValueConfig.has_alarm_config())
  {
    PrintAlarmConfig(rCharacteristicValueConfig.alarm_config(), Blanks);
  }

  // repeated Uuid measurement_config
  for (int i = 0; i < rCharacteristicValueConfig.measurement_config_size(); ++i)
  {
    UuidStringOut("Measurement config:  \t", rCharacteristicValueConfig.measurement_config(i), Blanks, __FUNCTION__);
  }

  // OneOfParameter
  //  fprintf(outstream, "%s>>> oneof parameter >>>\n", PBLANKS);
  switch (rCharacteristicValueConfig.parameter_case())
  {
    case smartcheck::CharacteristicValueConfig::kBaseMeasurement:
      PrintBaseMeasurementParameter(rCharacteristicValueConfig.base_measurement(), Blanks);
      break;

    case smartcheck::CharacteristicValueConfig::kBeltDrive:
      PrintBeltDriveParameter(rCharacteristicValueConfig.belt_drive(), Blanks);
      break;

    case smartcheck::CharacteristicValueConfig::kConditionGuard:
      PrintConditionGuardParameter(rCharacteristicValueConfig.condition_guard(), Blanks);
      break;

    case smartcheck::CharacteristicValueConfig::kCoupling:
      PrintCouplingParameter(rCharacteristicValueConfig.coupling(), Blanks);
      break;

    case smartcheck::CharacteristicValueConfig::kDefaultMeasurement:
      PrintDefaultMeasurementParameter(rCharacteristicValueConfig.default_measurement(), Blanks);
      break;

    case smartcheck::CharacteristicValueConfig::kFan:
      PrintFanParameter(rCharacteristicValueConfig.fan(), Blanks);
      break;

    case smartcheck::CharacteristicValueConfig::kGearStage:
      PrintGearStageParameter(rCharacteristicValueConfig.gear_stage(), Blanks);
      break;

    case smartcheck::CharacteristicValueConfig::kJournalBearing:
      PrintJournalBearingParameter(rCharacteristicValueConfig.journal_bearing(), Blanks);
      break;

    case smartcheck::CharacteristicValueConfig::kLoadDurationDistribution:
      PrintLoadDurationDistributionParameter(rCharacteristicValueConfig.load_duration_distribution(), Blanks);
      break;

    case smartcheck::CharacteristicValueConfig::kPump:
      PrintPumpParameter(rCharacteristicValueConfig.pump(), Blanks);
      break;

    case smartcheck::CharacteristicValueConfig::kRainflow:
      PrintRainflowCountParameter(rCharacteristicValueConfig.rainflow(), Blanks);
      break;

    case smartcheck::CharacteristicValueConfig::kRollerBearing:
      PrintRollerBearingParameter(rCharacteristicValueConfig.roller_bearing(), Blanks);
      break;

    case smartcheck::CharacteristicValueConfig::kShaft:
      PrintShaftParameter(rCharacteristicValueConfig.shaft(), Blanks);
      break;

    case smartcheck::CharacteristicValueConfig::kTrackedFrequencyBands:
      PrintTrackedFrequencyBandsParameter(rCharacteristicValueConfig.tracked_frequency_bands(), Blanks);
      break;

    case smartcheck::CharacteristicValueConfig::kAbsoluteFrequencyBands:
      PrintAbsoluteFrequencyBandsParameter(rCharacteristicValueConfig.absolute_frequency_bands(), Blanks);
      break;

    case smartcheck::CharacteristicValueConfig::kChannelMonitor:
      PrintChannelMonitorParameter(rCharacteristicValueConfig.channel_monitor(), Blanks);
      break;

    case smartcheck::CharacteristicValueConfig::kOilStatus:
      PrintOilStatusParameter(rCharacteristicValueConfig.oil_status(), Blanks);
      break;

    case smartcheck::CharacteristicValueConfig::kSchaefflerCloud:
      PrintSchaefflerCloudParameter(rCharacteristicValueConfig.schaeffler_cloud(), Blanks);
      break;

    case smartcheck::CharacteristicValueConfig::kProcessSignalMonitor:
      PrintProcessSignalMonitorParameter(rCharacteristicValueConfig.process_signal_monitor(), Blanks);
      break;

    case smartcheck::CharacteristicValueConfig::PARAMETER_NOT_SET:
      // fprintf(outstream, "!!! oneof Parameter is not set !!!\n");
      break;

    default:
      break;
  }
// fprintf(outstream, "%s<<< oneof parameter <<<\n", PBLANKS);

  UuidStringOut("Unit uuid:       \t", rCharacteristicValueConfig.unit_uuid(), Blanks, __FUNCTION__);
  fprintf(outstream, "%sUnit name:       \t%s\n", PBLANKS, rCharacteristicValueConfig.unit_name().c_str());

// repeated FrequencyBand frequency_bands
  for (int i = 0; i < rCharacteristicValueConfig.frequency_bands_size(); ++i)
  {
    PrintFrequencyBand(rCharacteristicValueConfig.frequency_bands(i), Blanks, i);
  }
// repeated SubCharacteristicValueConfig sub_characteristc_values
  fprintf(outstream, "\n%sNumber of sub characteristic values: \t%d\n", PBLANKS,
          rCharacteristicValueConfig.sub_characteristc_values_size());
  for (int i = 0; i < rCharacteristicValueConfig.sub_characteristc_values_size(); ++i)
  {
    printSubCharacteristicValueConfig(rCharacteristicValueConfig.sub_characteristc_values(i), Blanks, i);
  }
}

void PrintOrderAnalysisConfig(const smartcheck::OrderAnalysisConfig &rOrderAnalysisConfig, int Blanks)
{
  fprintf(outstream, "%sOrder analysis config\n", PBLANKS);
  Blanks += 4;
  UuidStringOut("Digital pulse channel:            \t", rOrderAnalysisConfig.digital_pulse_channel_uuid(), Blanks,
                __FUNCTION__);
  fprintf(outstream, "%sMaximum rotational speed:     \t%fHz\n", PBLANKS,
          rOrderAnalysisConfig.maximum_rotational_speed());
  fprintf(outstream, "%sMinimum rotational speed:     \t%fHz\n", PBLANKS,
          rOrderAnalysisConfig.minimum_rotational_speed());
  fprintf(outstream, "%sMaximum orders:               \t%u\n", PBLANKS, rOrderAnalysisConfig.maximum_orders());
  fprintf(outstream, "%sMinimum orders:               \t%u\n", PBLANKS, rOrderAnalysisConfig.minimum_orders());
  fprintf(outstream, "%sPulses per revolution:        \t%u\n", PBLANKS, rOrderAnalysisConfig.pulses_per_revolution());
  if (rOrderAnalysisConfig.averages() > 0)
  {
    fprintf(outstream, "%sAverages:                     \t%u\n", PBLANKS, rOrderAnalysisConfig.averages());
  }
}

void PrintMeasurementConfig(const smartcheck::MeasurementConfig &rMeasurementConfig, int Blanks, int Index)
{
  fprintf(outstream, "%s%s[%d]\n", PBLANKS, "Measurement config", Index);
  Blanks += 4;

  UuidStringOut("Current uuid:    \t", rMeasurementConfig.current_uuid(), Blanks, __FUNCTION__);
  UuidStringOut("Base uuid:       \t", rMeasurementConfig.base_uuid(), Blanks, __FUNCTION__);
  fprintf(outstream, "%sChange date:     \t%s\n", PBLANKS,
          TimestampAsYYYYMMDDHHMMSSms(rMeasurementConfig.change_date()));
  fprintf(outstream, "%sActive:          \t%s\n", PBLANKS, rMeasurementConfig.active() ? "yes" : "no");

  fprintf(outstream, "%sName:            \t%s\n", PBLANKS, ExtractTranslateFromName(rMeasurementConfig.name()).c_str());
  UuidStringOut("Input channel:   \t", rMeasurementConfig.input_channel_uuid(), Blanks, __FUNCTION__);
  switch (rMeasurementConfig.signal_type())
  {
    case smartcheck::SignalType::not_defined:
      fprintf(outstream, "%sSignal type:           \t%s\n", PBLANKS, "not defined");
      break;

    case smartcheck::SignalType::raw_time_signal:
      fprintf(outstream, "%sSignal type:           \t%s\n", PBLANKS, "raw time signal");
      break;

    case smartcheck::SignalType::demodulated_time_signal:
      fprintf(outstream, "%sSignal type:           \t%s\n", PBLANKS, "demodulated time signal");
      break;

    case smartcheck::SignalType::raw_spectrum:
      fprintf(outstream, "%sSignal type:           \t%s\n", PBLANKS, "raw spectrum");
      break;

    case smartcheck::SignalType::demodulated_spectrum:
      fprintf(outstream, "%sSignal type:           \t%s\n", PBLANKS, "demodulated spectrum");
      break;

    case smartcheck::SignalType::raw_order_analysis:
      fprintf(outstream, "%sSignal type:           \t%s\n", PBLANKS, "order analysis");
      break;

    case smartcheck::SignalType::demodulated_order_analysis:
      fprintf(outstream, "%sSignal type:           \t%s\n", PBLANKS, "demodulated order analysis");
      break;

    case smartcheck::SignalType::raw_time_synchronous_average:
      fprintf(outstream, "%sSignal type:           \t%s\n", PBLANKS, "raw time synchronous average");
      break;

    case smartcheck::SignalType::demodulated_time_synchronous_average:
      fprintf(outstream, "%sSignal type:           \t%s\n", PBLANKS, "demodulated time synchronous average");
      break;

    default:
      break;
  }

  UuidStringOut("Unit uuid:       \t", rMeasurementConfig.unit_uuid(), Blanks, __FUNCTION__);
  fprintf(outstream, "%sUnit name:       \t%s\n", PBLANKS, rMeasurementConfig.unit_name().c_str());
  fprintf(outstream, "%sIntegration:     \t%u\n", PBLANKS, rMeasurementConfig.integration());
  fprintf(outstream, "%sNumber of samples: \t%u\n", PBLANKS, rMeasurementConfig.number_of_samples());
  fprintf(outstream, "%sLead time:       \t%u\n", PBLANKS, rMeasurementConfig.lead_time());

  if (rMeasurementConfig.has_max_deviation_config())
  {
    if (rMeasurementConfig.max_deviation_config().max_deviation() > 0.0)
    {
      if (rMeasurementConfig.max_deviation_config().percentage())
      {
        fprintf(outstream, "%sMaximum deviation:   \t%.1f%%\n", PBLANKS,
                rMeasurementConfig.max_deviation_config().max_deviation());
      }
      else
      {
        fprintf(outstream, "%ssMaximum deviation:  \t%.1f%s\n", PBLANKS,
                rMeasurementConfig.max_deviation_config().max_deviation(), rMeasurementConfig.unit_name().c_str());
      }
    }
  }
  if (rMeasurementConfig.has_order_analysis_config())
  {
    PrintOrderAnalysisConfig(rMeasurementConfig.order_analysis_config(), Blanks);
  }
}

void PrintInputChannelConfig(const smartcheck::InputChannelConfig &rInputChannelConfig, int Blanks, int Index)
{
  fprintf(outstream, "%s%s[%d]\n", PBLANKS, "Input Channel Config", Index);
  Blanks += 4;

  UuidStringOut("Current uuid:      \t", rInputChannelConfig.current_uuid(), Blanks, __FUNCTION__);
  UuidStringOut("Base uuid:         \t", rInputChannelConfig.base_uuid(), Blanks, __FUNCTION__);
  fprintf(outstream, "%sChange date:       \t%s\n", PBLANKS,
          TimestampAsYYYYMMDDHHMMSSms(rInputChannelConfig.change_date()));
  fprintf(outstream, "%sName:              \t%s\n", PBLANKS,
          ExtractTranslateFromName(rInputChannelConfig.name()).c_str());
  fprintf(outstream, "%sActive:            \t%s\n", PBLANKS, rInputChannelConfig.active() ? "yes" : "no");
  fprintf(outstream, "%sScaling:           \t%f\n", PBLANKS, rInputChannelConfig.scaling());
  UuidStringOut("Unit uuid:         \t", rInputChannelConfig.unit_uuid(), Blanks, __FUNCTION__);
  fprintf(outstream, "%sUnit name:         \t%s\n", PBLANKS, rInputChannelConfig.unit_name().c_str());
}

const char* VoltageRangeToString(const smartcheck::VoltageRange VoltageRange)
{
  if (VoltageRange == smartcheck::VoltageRange::_0_10V)
  {
    return ("0 - 10V");
  }
  else if (VoltageRange == smartcheck::VoltageRange::_0_24V)
  {
    return ("0 - 24V");
  }
  else if (VoltageRange == smartcheck::VoltageRange::_min_1V5_plus_1V5)
  {
    return ("-1.5V - 1.5V");
  }
  else if (VoltageRange == smartcheck::VoltageRange::_min_5V_plus_5V)
  {
    return ("-5V - 5V");
  }
  else if (VoltageRange == smartcheck::VoltageRange::_min_10V_plus_10V)
  {
    return ("-10V - 10V");
  }
  else
  {
    return ("undefined");
  }
}

const char* CurrentRangeToString(const smartcheck::CurrentRange CurrentRange)
{
  if (CurrentRange == smartcheck::CurrentRange::_0_20mA)
  {
    return ("0 - 20mA");
  }
  else if (CurrentRange == smartcheck::CurrentRange::_4_20mA)
  {
    return ("4 - 20mA");
  }
  else
  {
    return ("undefined");
  }
}

void PrintAnalogInputConfig(const smartcheck::AnalogInputConfig &rAnalogInputConfig, int Blanks)
{
  fprintf(outstream, "%s%s\n", PBLANKS, "Analog Input Config");
  Blanks += 4;
  if (rAnalogInputConfig.voltage_range() != smartcheck::VoltageRange::voltage_range_not_defined)
  {
    fprintf(outstream, "%sInput range:       \t%s\n", PBLANKS,
            VoltageRangeToString(rAnalogInputConfig.voltage_range()));
  }
  if (rAnalogInputConfig.current_range() != smartcheck::CurrentRange::current_range_not_defined)
  {
    fprintf(outstream, "%sInput range:       \t%s\n", PBLANKS,
            CurrentRangeToString(rAnalogInputConfig.current_range()));
  }
  fprintf(outstream, "%sScaling:           \t%f\n", PBLANKS, rAnalogInputConfig.scaling());
  fprintf(outstream, "%sOffset:            \t%f\n", PBLANKS, rAnalogInputConfig.offset());
  fprintf(outstream, "%sMinimum value:     \t%f\n", PBLANKS, rAnalogInputConfig.minimum_value());
  fprintf(outstream, "%sMaximum value:     \t%f\n", PBLANKS, rAnalogInputConfig.maximum_value());
}

void PrintDigitalInputConfig(const smartcheck::DigitalInputConfig &rDigitalInputConfig, int Blanks)
{
  fprintf(outstream, "%s%s\n", PBLANKS, "Digital Input Config");
  Blanks += 4;
  fprintf(outstream, "%sLogic:              \t%d active\n", PBLANKS, (int) rDigitalInputConfig.logic());
  fprintf(outstream, "%sPulses per revolution:\t%d\n", PBLANKS, rDigitalInputConfig.pulses_per_revolution());
  fprintf(outstream, "%sSignal threshold:  \t%f\n", PBLANKS, rDigitalInputConfig.signal_threshold());
  fprintf(outstream, "%sHysteresis:        \t%f\n", PBLANKS, rDigitalInputConfig.hysteresis());
}

void PrintExternalInputConfig(const smartcheck::ExternalInputConfig &rExternalInputConfig, int Blanks)
{
  fprintf(outstream, "%s%s\n", PBLANKS, "External Input Device");
  Blanks += 4;

  UuidStringOut("External device:   \t", rExternalInputConfig.external_device_current_uuid(), Blanks, __FUNCTION__);
  fprintf(outstream, "%sName:              \t%s\n", PBLANKS,
          ExtractTranslateFromName(rExternalInputConfig.name()).c_str());
}

void PrintInputConfig(const smartcheck::InputConfig &rInputConfig, int Blanks, int Index)
{
  fprintf(outstream, "%s%s[%d]\n", PBLANKS, "Input Config", Index);
  Blanks += 4;

  UuidStringOut("Current uuid:      \t", rInputConfig.current_uuid(), Blanks, __FUNCTION__);
  UuidStringOut("Base uuid:         \t", rInputConfig.base_uuid(), Blanks, __FUNCTION__);
  fprintf(outstream, "%sChange date:       \t%s\n", PBLANKS, TimestampAsYYYYMMDDHHMMSSms(rInputConfig.change_date()));
  fprintf(outstream, "%sName:              \t%s\n", PBLANKS, ExtractTranslateFromName(rInputConfig.name()).c_str());
  fprintf(outstream, "%sActive:            \t%s\n", PBLANKS, rInputConfig.active() ? "yes" : "no");
  fprintf(outstream, "%sSample rate:      \t%d\n", PBLANKS, rInputConfig.sample_rate());
  UuidStringOut("Unit uuid:         \t", rInputConfig.unit_uuid(), Blanks, __FUNCTION__);
  fprintf(outstream, "%sUnit name:         \t%s\n", PBLANKS, rInputConfig.unit_name().c_str());
  if (rInputConfig.has_analog_input_config())
  {
    PrintAnalogInputConfig(rInputConfig.analog_input_config(), Blanks);
  }
  if (rInputConfig.has_digital_input_config())
  {
    PrintDigitalInputConfig(rInputConfig.digital_input_config(), Blanks);
  }
  if (rInputConfig.has_external_config())
  {
    PrintExternalInputConfig(rInputConfig.external_config(), Blanks);
  }

  for (int i = 0; i < rInputConfig.input_channel_config_size(); ++i)
  {
    PrintInputChannelConfig(rInputConfig.input_channel_config(i), Blanks, i);
  }
  fprintf(outstream, "%sDevice serial number: \t%s\n", PBLANKS, rInputConfig.device_serial().c_str());
  fprintf(outstream, "%sModule serial number: \t%s\n", PBLANKS, rInputConfig.module_serial().c_str());
  fprintf(outstream, "%sChannel index:        \t%d\n", PBLANKS, rInputConfig.channel_index());
}

void PrintJobConfig(const smartcheck::JobConfig &rJobConfig, int Blanks, int Index)
{
  fprintf(outstream, "%s%s[%d]\n", PBLANKS, "Job config", Index);
  Blanks += 4;

  UuidStringOut("Current uuid:      \t", rJobConfig.current_uuid(), Blanks, __FUNCTION__);
  UuidStringOut("Base uuid:         \t", rJobConfig.base_uuid(), Blanks, __FUNCTION__);
  fprintf(outstream, "%sChange date:       \t%s\n", PBLANKS, TimestampAsYYYYMMDDHHMMSSms(rJobConfig.change_date()));
  fprintf(outstream, "%sName:              \t%s\n", PBLANKS, ExtractTranslateFromName(rJobConfig.name()).c_str());
  fprintf(outstream, "%sActive:            \t%s\n", PBLANKS, rJobConfig.active() ? "yes" : "no");
  fprintf(outstream, "%sTrigger validator: \t%s\n", PBLANKS, rJobConfig.is_trigger_validator() == true ? "yes" : "no ");

// repeated CharacteristicValueConfig characteristic_value_config
  for (int i = 0; i < rJobConfig.characteristic_value_config_size(); ++i)
  {
    PrintCharacteristicValueConfig(rJobConfig.characteristic_value_config(i), Blanks, i);
  }

// repeated MeasurementConfig measurement_config
  fprintf(outstream, "\n%sNumber of measurement configs: \t%d\n", PBLANKS, rJobConfig.measurement_config_size());
  for (int i = 0; i < rJobConfig.measurement_config_size(); ++i)
  {
    PrintMeasurementConfig(rJobConfig.measurement_config(i), Blanks, i);
  }
}

void PrintDeviceConfig(const smartcheck::DeviceConfig &rDeviceConfig)
{
  int blanks = 0;

  fprintf(outstream, "\n");
  UuidStringOut("Current uuid:                    \t", rDeviceConfig.current_uuid(), blanks, __FUNCTION__);
  UuidStringOut("Base uuid:                       \t", rDeviceConfig.base_uuid(), blanks, __FUNCTION__);
  fprintf(outstream, "Change date:                     \t%s\n",
          TimestampAsYYYYMMDDHHMMSSms(rDeviceConfig.change_date()));
  fprintf(outstream, "Name:                            \t%s\n", ExtractTranslateFromName(rDeviceConfig.name()).c_str());
  fprintf(outstream, "Serial number:                   \t%s\n", rDeviceConfig.device_serial().c_str());
  fprintf(outstream, "Firmware version:                \t%s\n", rDeviceConfig.firmware_version().c_str());

  int ipv4 = ntohl(rDeviceConfig.ipv4_addr());
  fprintf(outstream, "IPV4 address:                    \t%d.%d.%d.%d\n", ((unsigned char*) &ipv4)[0],
          ((unsigned char*) &ipv4)[1], ((unsigned char*) &ipv4)[2], ((unsigned char*) &ipv4)[3]);
  int netmask = ntohl(rDeviceConfig.ipv4_netmask());
  fprintf(outstream, "IPV4 netmask:                    \t%d.%d.%d.%d\n", ((unsigned char*) &netmask)[0],
          ((unsigned char*) &netmask)[1], ((unsigned char*) &netmask)[2], ((unsigned char*) &netmask)[3]);
  int gateway = ntohl(rDeviceConfig.ipv4_gateway());
  fprintf(outstream, "IPV4 gateway:                    \t%d.%d.%d.%d\n", ((unsigned char*) &gateway)[0],
          ((unsigned char*) &gateway)[1], ((unsigned char*) &gateway)[2], ((unsigned char*) &gateway)[3]);
  int dns = ntohl(rDeviceConfig.ipv4_dns());
  fprintf(outstream, "IPV4 dns:                        \t%d.%d.%d.%d\n", ((unsigned char*) &dns)[0],
          ((unsigned char*) &dns)[1], ((unsigned char*) &dns)[2], ((unsigned char*) &dns)[3]);
  fprintf(outstream, "Hostname:                        \t%s\n", rDeviceConfig.hostname().c_str());

  smartcheck::JobConfig job_config;
  int num_job_configs = rDeviceConfig.job_config_size();
  fprintf(outstream, "\nNumber of job configs:           \t%d\n", num_job_configs);
  for (int i = 0; i < num_job_configs; ++i)
  {
    PrintJobConfig(rDeviceConfig.job_config(i), blanks, i);
  }

  int num_input_config = rDeviceConfig.input_config_size();
  if (num_input_config > 0)
  {
    fprintf(outstream, "\nNumber of input channel configs: \t%d\n", num_input_config);
    for (int i = 0; i < num_input_config; ++i)
    {
      PrintInputConfig(rDeviceConfig.input_config(i), blanks, i);
    }
  }
}

int main(int argc, char **argv)
{
  PrintVersionNumber();
 
  int buffer_length = CheckCommandLineParameters(argc, argv, "device config", outstream);

  char* buffer = ReadInputFileIntoBuffer(argv[1], buffer_length);

  ConvertHexToBinIfNeeded(buffer_length, buffer);

  smartcheck::TransferMessage transfer_message;
  if (transfer_message.ParseFromArray(buffer, buffer_length) && transfer_message.device_config().IsInitialized()
      && !transfer_message.device_config().current_uuid().empty())
  {
    fprintf(stderr, "Device config is in transfer message protobuf format\n");
    PrintDeviceConfig(transfer_message.device_config());
  }
  else
  {
    smartcheck::DeviceConfig device_config;
    if (device_config.ParseFromArray(buffer, buffer_length) && device_config.IsInitialized()
        && !device_config.current_uuid().empty())
    {
      fprintf(stderr, "Device config is in protobuf format\n");
      PrintDeviceConfig(device_config);
    }
    else
    {
      fprintf(stderr, "Error: Could parse device config from %s\n", argv[1]);
    }
  }

  free(buffer);

  if (argc == 3)
  {
      fprintf(stdout, "%s: Success: Device config written to file %s\n", argv[0], argv[2]);
      fclose(outstream);
  }
  exit(0);
}



