// @playground id=dates_and_durations label="Dates & Durations" html=slicing_ordering_and_types.html scraper=dates_and_durations
dates_and_durations = Scraper()
  | $("section.durations", "durations") {
      find("span.iso-dur", Text) | parse_duration(key="iso_sec", unit="seconds")
      | find("span.human-dur", Text) | parse_duration(key="human_min", unit="minutes")
      | find("span.clock-dur", Text) | parse_duration(key="clock_ms", unit="milliseconds")
    }
  | $("section.dates", "dates") {
      find("time.iso", Attribute, "datetime") | parse_date(output="iso", tz="UTC", key="iso")
      | find("time.iso", Attribute, "datetime") | parse_date(output="date", tz="UTC", key="date_only")
      | find("time.iso", Attribute, "datetime") | parse_date(output="unix", tz="UTC", key="unix")
      | find("time.iso", Attribute, "datetime") | parse_date(output="unix_ms", tz="UTC", key="unix_ms")
      | find("span.custom", Text) | parse_date(fmt="%Y/%m/%d %H:%M:%S", output="%Y-%m-%d", tz="UTC", key="custom")
      | find("span.relative", Text) | parse_date(output="date", tz="UTC", now="2026-05-10T00:00:00Z", key="rel_date")
    }
