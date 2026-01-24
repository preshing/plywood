{title text="Time and Date" include="ply-base.h" namespace="ply"}

Plywood provides two families of functions for working with time and dates.

* **Unix timestamps** are used to obtain the current system time and convert it to a human-readable format.
* The **high-resolution timer** is used to measure the elapsed time between two points in code. Useful for profiling and animation.

## Unix Timestamps

A [Unix timestamp](https://en.wikipedia.org/wiki/Unix_time) is an amount of elapsed time since **January 1, 1970 at 00:00 UTC**. Plywood uses Unix timestamps even in non-Unix environments such as Windows. These timestamps can be converted to `DateTime` objects that identify the calendar date and time of day.

{api_summary}
s64 get_unix_timestamp()
DateTime convert_to_date_time(s64 unix_timestamp)
DateTime convert_to_date_time(s64 unix_timestamp, s16 time_zone_offset_in_minutes)
s64 convert_to_unix_timestamp(const DateTime& date_time)
void print_date_time(Stream& out, StringView format, const DateTime& date_time)
{/api_summary}

{api_descriptions}
s64 get_unix_timestamp()
--
Returns the current system time as a Unix timestamp in microseconds.

>>
DateTime convert_to_date_time(s64 unix_timestamp)
DateTime convert_to_date_time(s64 unix_timestamp, s16 time_zone_offset_in_minutes)
--
Converts a Unix timestamp to a `DateTime` object with the following member variables:

{table caption="`DateTime` members"}
`s32`|`year`
`u8`|`month`|1..12
`u8`|`day`|1..31
`u8`|`weekday`|Sunday = 0, Saturday = 6
`u8`|`hour`|0..23
`u8`|`minute`|0..59
`u8`|`second`|0..59
`s16`|`time_zone_offset_in_minutes`|eg. EST = -300
`u32`|`microsecond`|0..999999
{/table}

The same Unix timestamp can produce different `DateTime` objects depending on the time zone used during conversion. The time zone of each `DateTime` object is indicated by the `time_zone_offset_in_minutes` member and expressed relative to [Coordinated Universal Time (UTC)](https://en.wikipedia.org/wiki/CoordinatedUniversalTime). For example, a `time_zone_offset_in_minutes` of `-300` corresponds to [Eastern Standard Time (EST)](https://en.wikipedia.org/wiki/EasternTimeZone), which is 5 hours behind UTC.

The first form of `convert_to_date_time` uses the local time zone offset as reported by the underlying operating system. The second form accepts `time_zone_offset_in_minutes` as an additional argument. If you call the second form, you'll have to determine the time zone offset yourself, since Plywood doesn't provide a way to determine time zone offsets at arbitrary geographic locations.

>>
s64 convert_to_unix_timestamp(const DateTime& date_time)
--
Converts a `DateTime` object back to a Unix timestamp.

>>
void print_date_time(Stream& out, StringView format, const DateTime& date_time)
--
Prints the contents of a `DateTime` object as human-readable text. Month and weekday names are output in English only. The `format` string accepts the following conversion specifiers:
{/api_descriptions}

{table caption="Conversion specifiers"}
`%a`|abbreviated weekday
`%A`|full weekday
`%b`|abbreviated month name
`%B`|full month name
`%d`|day of the month with leading zero
`%e`|day of the month
`%H`|hour with leading zero (24-hour clock)
`%k`|hour (24-hour clock)
`%l`|hour (12-hour clock)
`%m`|month with leading zero
`%M`|minute with leading zero
`%p`|AM or PM
`%P`|am or pm
`%S`|second with leading zero
`%y`|two-digit year
`%Y`|year
`%L`|millisecond with leading zeros
`%R`|microsecond with leading zeros
`%Z`|signed time zone offset
{/table}

    s64 sys_time = get_unix_timestamp();
    DateTime date_time = convert_to_date_time(sys_time);
    Stream out = get_stdout();
    print_date_time(out, "[%Y:%m:%d %H:%M:%S.%L]\n", date_time);
    out.format("The date is {}.\n", String::from_date_time("%A, %B %e, %Y", date_time));
    out.format("The time is {}.\n", String::from_date_time("%l:%M %p (UTC%Z)", date_time));

{output}
[2025-12-01 19:00:01.234]
The date is Monday, December 1, 2025.
The time is 7:00 PM (UTC-05:00).
{/output}

## High-Resolution Timer

The high resolution timer gives precise CPU timing measurements with minimum runtime overhead, letting you accurately measure the elapsed time between two points in code. These functions are typically thin wrappers over equivalent functions in the underlying platform SDK.

{api_summary}
u64 get_cpu_ticks()
float get_cpu_ticks_per_second()
{/api_summary}

{api_descriptions}
u64 get_cpu_ticks()
--
Returns a high-resolution CPU timestamp.

>>
float get_cpu_ticks_per_second()
--
Returns the high-resolution timer frequency. To measure a time interval in seconds, subtract two timestamps and divide the result by this value.
{/api_descriptions}

    u64 start_tick = get_cpu_ticks();
    do_some_work();
    u64 end_tick = get_cpu_ticks();
    float duration = (end_tick - start_tick) / get_cpu_ticks_per_second();
    get_stdout().write("{} seconds elapsed.\n", duration);

{output}
1.234 seconds elapsed.
{/output}

