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

#define CLASSIFICATION_HEADER_VERSION   1

// Integer typedefs
typedef signed char Int8_t;
typedef unsigned char Uint8_t;
typedef signed short Int16_t;
typedef unsigned short Uint16_t;
typedef signed int Int32_t;
typedef unsigned int Uint32_t;
typedef signed long long Int64_t;
typedef unsigned long long Uint64_t;

// Uuid
typedef unsigned char uuid_t[16];

// Fract typedefs
typedef Int16_t Fract16_t;
typedef Int32_t Fract32_t;
typedef Int64_t Fract64_t;

// Floating-point typedefs
typedef float Float32_t;
typedef double Float64_t;

enum sample_datatype32_t
{
  data_not_def_t = 0,
  data_int8_t = 1,
  data_int16_t = 2,
  data_int32_t = 3,
  data_int64_t = 4,
  data_float32_t = 5,
  data_float64_t = 6,
  data_uint8_t = 7,
  data_uint16_t = 8,
  data_uint32_t = 9,
  data_uint64_t = 10
};

enum compression32_t
{
  compression_not_defined = 0,
  no_compression = 1, 
  zlib = 2, 
  int24 = 3, // Currently not supported, just place holder.
  gzip = 4
};

// Timestamp in µs.
typedef Int64_t timestamp_t;

// Signal type of the binary file
enum signal32_t
{
  not_defined = 0,
  raw_time_signal = 1,                       // The unit of delta_x is s (seconds)
  demodulated_time_signal = 2,               // The unit of delta_x is s (seconds)
  raw_spectrum = 3,                          // The unit of delta_x is Hz (Hertz)
  demodulated_spectrum = 4,                  // The unit of delta_x is Hz (Hertz)
  raw_order_analysis = 5,                    // The unit of delta_x is ° (degree)
  demodulated_order_analysis = 6,            // The unit of delta_x is ° (degree)
  raw_time_synchronous_average = 7,          // The unit of delta_x is ° (degree)
  demodulated_time_synchronous_average = 8   // The unit of delta_x is ° (degree)
};

// Window type for fft calculation
enum fft_windowtype32_t
{
  invalid_window_type = 0,
  rectangular_window = 1,
  hamming_window = 2,
  hann_window = 3,
  flat_top_window = 4,
  harris_window = 5,
  kaiser8_window = 6,
};

// Alarm status of a value in the trend
enum alarmstatus8_t: Uint8_t
{
  unknown_alarm_status = 0, 
  no_alarm = 1, 
  pre_alarm = 2, 
  main_alarm = 3, 
  charval_error = 4,
};

// These are the calculator types which are used to c
enum Calculator_t
{
  calculator_not_defined = 0,
  calculator_dc = 1,
  calculator_rms = 2,
  calculator_crest = 3,
  calculator_peak = 4,
  calculator_peak2peak = 5
};

// These are the characteristic value config types available on SmartCheck
enum charval_types32_t
{
  invalid_charval = 0,
  standard_charval = 1,            // char val of types defined in standard_charval_types32_t (@sa CharValBase.h)
  formula_charval = 2,             // Numeric or alarm formula
  Iso10816_charval = 3,            // ISO 10816 Class I - Class IV
  Vdi3836_charval = 4,             // VDI 3836 Category 1 - Category 4
  condition_guard_charval = 5,     // Condition guard
  load_duration_distribution = 6,  // ldd
  rain_flow_count = 7,             // rfc
  channel_monitor = 8,             // channel monitor
  trigger_charval = 9,             // Trigger
  validator_charval = 10           // Validatior
};

// Increase header version when changes are done in struct trend_header_t
const Uint16_t TREND_HEADER_VERSION = 3;
struct trend_header_t
{ // Size: 96 byte
  Uint16_t version;                         // Version of the header
  Uint16_t header_size;                     // sizeof(timesignal_header_t)
  compression32_t compression;              // Defines which compression is used,if any
  uuid_t uuid_characteristic_value_config;  // uuid of the charac value's configuration to which this trend belongs to
  uuid_t uuid_trend;                        // uuid of the trend chunk (each trend chunk has its own uuid)
  timestamp_t first_timestamp;              // Timestamp of the first value of this trend chunk in microseconds since 01.01.1970
  timestamp_t last_timestamp;               // Timestamp of the last value of this trend chunk in microseconds since 01.01.1970
  uuid_t unit;                              // uuid of the unit from unit system
  Float32_t lower_pre_alarm_level;          // The lower pre alarm level is defined for the whole alarm map
  Float32_t lower_main_alarm_level;         // The lower main alarm level is defined for the whole alarm map
  Int32_t value_count;                      // Number of value entries in the data-array
  Int32_t byte_count;                       // Number of bytes in the data-array (real size regarding compression;
                                            // if no compression is used: sample_count*sizeof(sample_type) )
  Uint8_t unused[4];                        // Padding bytes
  Uint16_t checksum_data;                   // xor-checksum over the associated data array
  Uint16_t checksum_header;                 // xor-checksum over (header - checksum): checksum over complete header should always be zero
};

struct trend_entry_t
{ // Size: 32 byte
  timestamp_t timestamp;        // Timestamp when this value was measured
  Float64_t value;              // The value of the charac value at the given timestamp
  Float32_t main_alarm_level;   // The value of the main alarm level at the given timestamp. This depends on the position in the alarm map (if one exists).
  Float32_t pre_alarm_level;    // The value of the pre-alarm level at the given timestamp. This depends on the position in the alarm map (if one exists).
  Uint8_t alarm_map_index;      // The position in the alarm map at the given timestamp (0 if none exists). Index is 0..9 (one-dimensional) or 0..99 (2-dimensional)
  alarmstatus8_t alarm_status;  // The alarm status of this trend value
  bool learning_mode_active;    // This trend value was used for learning new alarm limits
  Uint8_t unused;               // Padding byte
  Float32_t speed;              // Speed value in Hz used to calculate this charval. NaN when no value available.
};

struct trend_bin_t
{
  trend_header_t header;
  trend_entry_t *data_array; // this is a array of trend_entry_t[] with datatypes depending on the definition in the header
};

// Increase header version when changes are done in struct timesignal_header_t
const Uint16_t TIMESIGNAL_HEADER_VERSION = 4;
struct timesignal_header_t
{ // Size: 200 byte
  Uint16_t version;                                 // Version of the header
  Uint16_t header_size;                             // sizeof(timesignal_header_t)
  signal32_t signal_type;                           // Type of the signal (e.g. time signal or spectrum)
  uuid_t uuid_config;                               // uuid of the configuration
  uuid_t uuid_measurement;                          // uuid of the time signal measurement (every time signal has its own
                                                    // uuid)
  Float64_t delta_x;                                // Distance between two samples
  Float64_t scaling_factor;                         // Scaling factor for each sample in the data array (multiply each sample with this value)
  Float64_t offset;                                 // Signal offset. Subtract this value from every sample BEFORE scaling_factor is applied.
  timestamp_t timestamp_microseconds;               // Timestamp in µs since 01.01.1970
  uuid_t unit;                                      // uuid of unit from unit system
  compression32_t compression;                      // Defines which compression is used, if any
  sample_datatype32_t sample_type;                  // Defines the type of the samples in the data array
  Uint64_t sample_count;                            // Number of samples in the data array
  Uint64_t byte_count;                              // Number of bytes in the data array (real size regarding compression;
                                                    // if no compression is used: sample_count*sizeof(sample_type))
  Float32_t rotational_frequency;                   // Measured rotational frequency, if measured for this time signal, NaN if none available
  Float32_t order_domain_filter_delay_revolutions;  // Delay in the order domain in revolutions caused by the fir filter
  timestamp_t timestamp_first_sample_microseconds;  // Timestamp of first sample in µs since 01.01.1970
  Uint8_t unused2[68];                              // Unused bytes in header
  Uint16_t checksum_data;                           // xor-checksum over the associated data array
                                                    // :TODO: CHECKSUM OF TIME SIGNAL DATA SHOULD ALSO BE BUILD IF THE
                                                    // THE TIME SIGNAL IS COMPRESSED.
  Uint16_t checksum_header;                         // xor-checksum over (header - checksum): checksum over complete
                                                    // header should always be zero
};

struct timesignal_header_v3_t
{ // Size: 120 byte
  Uint16_t version;                     // Version of the header
  Uint16_t header_size;                 // sizeof(timesignal_header_t)
  signal32_t signal_type;               // Type of the signal (e.g. time signal or spectrum)
  uuid_t uuid_config;                   // uuid of the configuration
  uuid_t uuid_measurement;              // uuid of the time signal measurement (every time signal has its own
                                        // uuid)
  Float64_t delta_x;                    // Distance between two samples
  Float64_t scaling_factor;             // Scaling factor for each sample in the data array (multiply each sample with this value)
  Float64_t offset;                     // Signal offset. Subtract this value from every sample BEFORE scaling_factor is applied.
  timestamp_t timestamp_microseconds;   // Timestamp in µs since 01.01.1970
  uuid_t unit;                          // uuid of unit from unit system
  compression32_t compression;          // Defines which compression is used, if any
  sample_datatype32_t sample_type;      // Defines the type of the samples in the data array
  Uint64_t sample_count;                // Number of samples in the data array
  Uint64_t byte_count;                  // Number of bytes in the data array (real size regarding compression;
                                        // if no compression is used: sample_count*sizeof(sample_type))
  Float32_t rotational_frequency;       // Measured rotational frequency, if measured for this time signal, NaN if none available
  Uint16_t checksum_data;               // xor-checksum over the associated data array
                                        // :TODO: CHECKSUM OF TIME SIGNAL DATA SHOULD ALSO BE BUILD IF THE
                                        // THE TIME SIGNAL IS COMPRESSED.
  Uint16_t checksum_header;             // xor-checksum over (header - checksum): checksum over complete
                                        // header should always be zero
};

#define CLASSIFICATION_DATA_HEADER_VERSION 2

typedef enum classification_period_type32
{
    classification_period_unknown = 0,      // uninitialized period type
    classification_period_hourly = 1,       // classification period of one hour
    classification_period_daily = 2,        // classification period of one day
    classification_period_weekly = 3,       // classification period of one week
    classification_period_monthly = 4,      // classification period of one month
    classification_period_yearly = 5,       // classification period of one year
    classification_period_continuous = 6,   // classification period of one year
    classification_period_user_defined = 7, // classification period defined by user
    classification_period_initial_data = 8  // initial data is uploaded into the smartcheck and added to continous matrix
} classification_period_type32_t;

typedef enum classification_data_type32
{
    classification_data_unknown = 0,        // uninitialized classification type
    classification_data_ldd = 1,            // load duration distribution type
    classification_data_rfc = 2,            // rainflow type
    classification_data_temperature = 3     // temperature type
} classification_data_type32_t;

struct classification_dimension_type_t
{
    char unit_string[16];       // unit of the dimension as a zero terminated string
    uuid_t unit_uuid;           // uuid of the unit from FAG SmartCheck unit system of this dimension
    Uint32_t num_classes;       // Number of classes of this dimension
    Float32_t lower_border;     // Lower border of this dimension, scaled in given unit.
    Float32_t upper_border;     // Upper border of this dimension, scaled in given unit.
    Uint8_t unused[4];          // Padding (bytes always zero) to 8 bytes boundary
};

struct classification_header_v1_t
{ // Size: 480 bytes
    Uint16_t version;                               // Version of the header
    Uint16_t header_size;                           // sizeof(timesignal_header_t)
    char serial_number[32];                         // Device serial number serial number
    char comment[256];                              // A comment field, can be used e.g. to describe the component monitored
    compression32_t compression;                    // Defines which compression is used,if any
    uuid_t uuid_characteristic_value_config;        // uuid of the configuration of the characteristic value to which this classification data belongs to
    uuid_t uuid_classification_data;                // uuid of the classification data
    classification_data_type32_t data_type;         // The type of the classification data (ldd, rfc, temperature)
    classification_period_type32_t period_type;     // The type of the classification period (daily, weekly, monthly, yearly, user defined)
    timestamp_t start_timestamp;                    // Timestamp of the start of the period of this classification data in microseconds since 01.01.1970
    timestamp_t end_timestamp;                      // Timestamp of the end of the period of this classification data in microseconds since 01.01.1970
    timestamp_t modified_timestamp;                 // Timestamp when last sample was added to the matrix in microseconds since 01.01.1970
    classification_dimension_type_t dimensions[2];  // Dimension data (maximum 2 dimensions);
    Uint32_t sample_rate;                           // Sampling rate
    Uint8_t unused[4];                              // Padding bytes (always zero) to 8 byte boundary
    Uint64_t sample_count;                          // Total number of samples collected so far
    Uint32_t byte_count;                            // Number of bytes in the data-array (real size regarding compression)
                                                    // if no compression is used: sample_count*sizeof(sample_type) )
    Uint16_t checksum_data;                         // xor-checksum over the associated data array
    Uint16_t checksum_header;                       // xor-checksum over (header - checksum_header): checksum over complete header should always be zero
};

struct classification_header_t
{ // Size: 488 bytes
    Uint16_t version;                               // Version of the header
    Uint16_t header_size;                           // sizeof(timesignal_header_t)
    char serial_number[32];                         // Device serial number
    char comment[256];                              // A comment field, can be used e.g. to describe the component monitored
    compression32_t compression;                    // Defines which compression is used,if any
    uuid_t uuid_characteristic_value_config;        // uuid of the configuration of the characteristic value to which this classification data belongs to
    uuid_t uuid_classification_data;                // uuid of the classification data
    classification_data_type32_t data_type;         // The type of the classification data (ldd, rfc, temperature)
    classification_period_type32_t period_type;     // The type of the classification period (daily, weekly, monthly, yearly, user defined)
    timestamp_t start_timestamp;                    // Timestamp of the start of the period of this classification data in microseconds since 01.01.1970
    timestamp_t end_timestamp;                      // Timestamp of the end of the period of this classification data in microseconds since 01.01.1970
    timestamp_t modified_timestamp;                 // Timestamp when last sample was added to the matrix in microseconds since 01.01.1970
    timestamp_t close_timestamp;                    // Timestamp, when this matrix has been closed, e.g. by an oil change
    classification_dimension_type_t dimensions[2];  // Dimension data (maximum 2 dimensions);
    Uint32_t sample_rate;                           // Sampling rate
    Uint8_t unused[4];                              // Padding bytes (always zero) to 8 byte boundary
    Uint64_t sample_count;                          // Total number of samples collected so far
    Uint32_t byte_count;                            // Number of bytes in the data-array (real size regarding compression)
                                                    // if no compression is used: sample_count*sizeof(sample_type) )
    Uint16_t checksum_data;                         // xor-checksum over the associated data array
    Uint16_t checksum_header;                       // xor-checksum over (header - checksum_header): checksum over complete header should always be zero
};

struct classification_bin_t
{
    classification_header_t header;
    Uint64_t* data_array;   // this is the matrix with the binary data. Can be one or two dimensional, depending on the header
                            // Layout of an m x n matrix:
                            // (d0_0,d1_0), (d0_1,d1_0), ..., (d0_m,d1_0)
                            // (d0_0,d1_1), (d0_1,d1_1), ..., (d0_m,d1_1)
                            //              ...
                            // (d0_0,d1_n), (d0_1,d1_n), ..., (d0_m,d1_n)

};
