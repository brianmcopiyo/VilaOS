/**
 * Literal React port of Chat-Room `resources/js/os/shell/CalendarPanel.vue`
 */
import { useMemo, useState } from "react";
import {
  ChevronLeftIcon,
  ChevronRightIcon,
} from "@heroicons/react/24/outline";

type CalendarEvent = { date?: string | Date; count?: number };

type Props = {
  timeZone?: string;
  events?: CalendarEvent[];
  onSelect?: (payload: { date: string; events: CalendarEvent[] }) => void;
};

type Cell =
  | { key: string; empty: true }
  | {
      key: string;
      empty: false;
      day: number;
      date: Date;
      isToday: boolean;
      isSelected: boolean;
      eventCount: number;
    };

function startOfMonth(date: Date) {
  return new Date(date.getFullYear(), date.getMonth(), 1);
}

function dateKey(value: string | Date | undefined): string {
  if (!value) return "";
  if (typeof value === "string" && /^\d{4}-\d{2}-\d{2}/.test(value)) {
    return value.slice(0, 10);
  }
  const d = value instanceof Date ? value : new Date(value);
  if (Number.isNaN(d.getTime())) return "";
  const y = d.getFullYear();
  const m = String(d.getMonth() + 1).padStart(2, "0");
  const day = String(d.getDate()).padStart(2, "0");
  return `${y}-${m}-${day}`;
}

function zonedNow(timeZone?: string) {
  if (!timeZone) return new Date();
  try {
    const parts = new Intl.DateTimeFormat("en-CA", {
      timeZone,
      year: "numeric",
      month: "2-digit",
      day: "2-digit",
    }).formatToParts(new Date());
    const get = (type: string) => parts.find((p) => p.type === type)?.value;
    return new Date(
      Number(get("year")),
      Number(get("month")) - 1,
      Number(get("day")),
    );
  } catch {
    return new Date();
  }
}

const weekdayLabels = ["M", "T", "W", "T", "F", "S", "S"];

export function CalendarPanel({
  timeZone,
  events = [],
  onSelect,
}: Props) {
  const [view, setView] = useState(() => startOfMonth(zonedNow(timeZone)));
  const [selected, setSelected] = useState<string | null>(null);

  const monthLabel = useMemo(() => {
    const labelDate = new Date(view.getFullYear(), view.getMonth(), 15);
    return labelDate.toLocaleDateString(undefined, {
      month: "long",
      year: "numeric",
    });
  }, [view]);

  const todayKey = useMemo(() => dateKey(zonedNow(timeZone)), [timeZone]);

  const eventCountByDay = useMemo(() => {
    const map = new Map<string, number>();
    for (const event of events || []) {
      const key =
        typeof event?.date === "string" ? event.date : dateKey(event?.date);
      if (!key) continue;
      map.set(key, (map.get(key) || 0) + (Number(event.count) || 1));
    }
    return map;
  }, [events]);

  const cells = useMemo((): Cell[] => {
    const year = view.getFullYear();
    const month = view.getMonth();
    const first = new Date(year, month, 1);
    const startOffset = (first.getDay() + 6) % 7;
    const daysInMonth = new Date(year, month + 1, 0).getDate();
    const result: Cell[] = [];

    for (let i = 0; i < startOffset; i += 1) {
      result.push({ key: `pad-${i}`, empty: true });
    }

    for (let day = 1; day <= daysInMonth; day += 1) {
      const date = new Date(year, month, day);
      const key = dateKey(date);
      result.push({
        key,
        empty: false,
        day,
        date,
        isToday: key === todayKey,
        isSelected: selected === key,
        eventCount: eventCountByDay.get(key) || 0,
      });
    }

    return result;
  }, [eventCountByDay, selected, todayKey, view]);

  function eventsFor(key: string) {
    return (events || []).filter((event) => {
      const date =
        typeof event?.date === "string" ? event.date : dateKey(event?.date);
      return date === key;
    });
  }

  function prevMonth() {
    setView(new Date(view.getFullYear(), view.getMonth() - 1, 1));
  }

  function nextMonth() {
    setView(new Date(view.getFullYear(), view.getMonth() + 1, 1));
  }

  function goToday() {
    const now = zonedNow(timeZone);
    const key = dateKey(now);
    setView(startOfMonth(now));
    setSelected(key);
    onSelect?.({ date: key, events: eventsFor(key) });
  }

  function selectDay(cell: Cell) {
    if (cell.empty) return;
    setSelected(cell.key);
    onSelect?.({ date: cell.key, events: eventsFor(cell.key) });
  }

  return (
    <section className="tray-calendar" aria-label="Calendar">
      <header className="tray-calendar-header">
        <button
          type="button"
          className="tray-calendar-nav"
          aria-label="Previous month"
          onClick={prevMonth}
        >
          <ChevronLeftIcon className="h-4 w-4" />
        </button>
        <div className="tray-calendar-title-wrap">
          <h2 className="tray-calendar-title">{monthLabel}</h2>
          <button
            type="button"
            className="tray-calendar-today"
            onClick={goToday}
          >
            Today
          </button>
        </div>
        <button
          type="button"
          className="tray-calendar-nav"
          aria-label="Next month"
          onClick={nextMonth}
        >
          <ChevronRightIcon className="h-4 w-4" />
        </button>
      </header>

      <div className="tray-calendar-weekdays" aria-hidden="true">
        {weekdayLabels.map((label, idx) => (
          <span key={`${label}-${idx}`}>{label}</span>
        ))}
      </div>

      <div className="tray-calendar-grid" role="grid" aria-label={monthLabel}>
        {cells.map((cell) => (
          <button
            key={cell.key}
            type="button"
            className={[
              "tray-calendar-day",
              cell.empty ? "is-empty" : "",
              !cell.empty && cell.isToday ? "is-today" : "",
              !cell.empty && cell.isSelected ? "is-selected" : "",
              !cell.empty && cell.eventCount > 0 ? "has-events" : "",
            ]
              .filter(Boolean)
              .join(" ")}
            disabled={cell.empty}
            tabIndex={cell.empty ? -1 : 0}
            aria-label={
              cell.empty
                ? undefined
                : `${cell.day}${cell.isToday ? ", today" : ""}${
                    cell.eventCount ? `, ${cell.eventCount} events` : ""
                  }`
            }
            aria-current={!cell.empty && cell.isToday ? "date" : undefined}
            aria-pressed={
              !cell.empty && cell.isSelected ? true : undefined
            }
            onClick={() => selectDay(cell)}
          >
            {!cell.empty ? (
              <span className="tray-calendar-day-num">{cell.day}</span>
            ) : null}
            {!cell.empty && cell.eventCount ? (
              <span
                className="tray-calendar-event-dot"
                title={`${cell.eventCount} event${
                  cell.eventCount === 1 ? "" : "s"
                }`}
              />
            ) : null}
          </button>
        ))}
      </div>
    </section>
  );
}
