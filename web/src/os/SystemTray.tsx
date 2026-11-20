/**
 * Literal React port of Chat-Room `resources/js/os/shell/SystemTray.vue`
 * (fullscreen action pill removed per product request)
 */
import { useEffect, useState, type KeyboardEvent } from "react";
import { SystemTrayPanel } from "./SystemTrayPanel";

type Props = {
  chromeHidden: boolean;
  panelOpen: boolean;
  onTogglePanel: () => void;
  onClosePanel: () => void;
  onReveal: () => void;
  onScheduleHide: () => void;
};

export function SystemTray({
  chromeHidden,
  panelOpen,
  onTogglePanel,
  onClosePanel,
  onReveal,
  onScheduleHide,
}: Props) {
  const [now, setNow] = useState(() => new Date());
  const timeZone =
    Intl.DateTimeFormat().resolvedOptions().timeZone || "UTC";

  useEffect(() => {
    const clockTimer = window.setInterval(() => {
      setNow(new Date());
    }, 15_000);
    return () => window.clearInterval(clockTimer);
  }, []);

  const clockLabel = now.toLocaleTimeString([], {
    hour: "2-digit",
    minute: "2-digit",
    timeZone,
  });

  const dateLabel = now.toLocaleDateString([], {
    weekday: "short",
    month: "short",
    day: "numeric",
    timeZone,
  });

  function onTrayKeydown(event: KeyboardEvent<HTMLButtonElement>) {
    if (event.key === "Enter" || event.key === " ") {
      event.preventDefault();
      onTogglePanel();
    }
  }

  return (
    <>
      <aside
        className={[
          "system-tray",
          "desktop-chrome",
          chromeHidden ? "is-chrome-hidden" : "",
        ]
          .filter(Boolean)
          .join(" ")}
        role="complementary"
        aria-label="System tray"
        data-system-tray
        onMouseEnter={onReveal}
        onMouseLeave={onScheduleHide}
      >
        <div className="system-tray-pill system-tray-pill--clock">
          <button
            type="button"
            className="system-tray-button"
            aria-expanded={panelOpen ? "true" : "false"}
            aria-controls="system-tray-panel"
            aria-label={`Calendar and notifications, ${clockLabel}, ${dateLabel}`}
            onClick={onTogglePanel}
            onKeyDown={onTrayKeydown}
          >
            <span className="system-tray-clock">
              <span className="system-tray-time">{clockLabel}</span>
              <span className="system-tray-date">{dateLabel}</span>
            </span>
          </button>
        </div>
      </aside>

      <SystemTrayPanel
        open={panelOpen}
        timeZone={timeZone}
        onClose={onClosePanel}
      />
    </>
  );
}
