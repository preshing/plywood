{title text="Time and Date" include="ply-base.h" namespace="ply"}

Plywood provides two families of functions for working with time and dates.

* **Unix timestamps** are used to obtain the current system time and convert it to a human-readable format.
* The **high-resolution timer** is used to measure the elapsed time between two points in code. Useful for profiling and animation.

## Unix Timestamps

A [Unix timestamp](https://en.wikipedia.org/wiki/Unix_time) is an amount of elapsed time since **January 1, 1970 at 00:00 UTC**. Plywood uses Unix timestamps even in non-Unix environments such as Windows. These timestamps can be converted to `DateTime` objects that identify the calendar date and time of day.

{apiSummary}
s64 getUnixTimestamp()
DateTime convertToDateTime(s64 unixTimestamp)
DateTime convertToDateTime(s64 unixTimestamp, s16 timeZoneOffsetInMinutes)
s64 convertToUnixTimestamp(const DateTime& dateTime)
void printDateTime(Stream& out, StringView format, const DateTime& dateTime)
{/apiSummary}

`s64 getUnixTimestamp()`
> Returns the current system time as a Unix timestamp in microseconds.

`DateTime convertToDateTime(s64 unixTimestamp)`
`DateTime convertToDateTime(s64 unixTimestamp, s16 timeZoneOffsetInMinutes)`
> Converts a Unix timestamp to a `DateTime` object with the following member variables:
>
> {table caption="`DateTime` members"}
> `s32`|`year`
> `u8`|`month`|1..12
> `u8`|`day`|1..31
> `u8`|`weekday`|Sunday = 0, Saturday = 6
> `u8`|`hour`|0..23
> `u8`|`minute`|0..59
> `u8`|`second`|0..59
> `s16`|`timeZoneOffsetInMinutes`|eg. EST = -300
> `u32`|`microsecond`|0..999999
> {/table}
>
> The same Unix timestamp can produce different `DateTime` objects depending on the time zone used during conversion. The time zone of each `DateTime` object is indicated by the `timeZoneOffsetInMinutes` member and expressed relative to [Coordinated Universal Time (UTC)](https://en.wikipedia.org/wiki/CoordinatedUniversalTime). For example, a `timeZoneOffsetInMinutes` of `-300` corresponds to [Eastern Standard Time (EST)](https://en.wikipedia.org/wiki/EasternTimeZone), which is 5 hours behind UTC.
>
> The first form of `convertToDateTime` uses the local time zone offset as reported by the underlying operating system. The second form accepts `timeZoneOffsetInMinutes` as an additional argument. If you call the second form, you'll have to determine the time zone offset yourself, since Plywood doesn't provide a way to determine time zone offsets at arbitrary geographic locations.

`s64 convertToUnixTimestamp(const DateTime& dateTime)`
> Converts a `DateTime` object back to a Unix timestamp.

`void printDateTime(Stream& out, StringView format, const DateTime& dateTime)`
> Prints the contents of a `DateTime` object as human-readable text. Month and weekday names are output in English only. The `format` string accepts the following conversion specifiers:

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

    s64 sysTime = getUnixTimestamp();
    DateTime dateTime = convertToDateTime(sysTime);
    Stream out = getStdOut();
    printDateTime(out, "[%Y-%m-%d %H:%M:%S.%L]\n", dateTime);
    out.format("The date is {}.\n", String::fromDateTime("%A, %B %e, %Y", dateTime));
    out.format("The time is {}.\n", String::fromDateTime("%l:%M %p (UTC%Z)", dateTime));

{output}
[2025-12-01 19:00:01.234]
The date is Monday, December 1, 2025.
The time is 7:00 PM (UTC-05:00).
{/output}

## High-Resolution Timer

The high resolution timer gives precise CPU timing measurements with minimum runtime overhead, letting you accurately measure the elapsed time between two points in code. These functions are typically thin wrappers over equivalent functions in the underlying platform SDK.

{apiSummary}
u64 getCpuTicks()
float getCpuTicksPerSecond()
{/apiSummary}

`u64 getCpuTicks()`
> Returns a high-resolution CPU timestamp.

`float getCpuTicksPerSecond()`
> Returns the high-resolution timer frequency. To measure a time interval in seconds, subtract two timestamps and divide the result by this value.

    u64 startTick = getCpuTicks();
    doSomeWork();
    u64 endTick = getCpuTicks();
    float duration = (endTick - startTick) / getCpuTicksPerSecond();
    getStdOut().write("{} seconds elapsed.\n", duration);

{output}
1.234 seconds elapsed.
{/output}

