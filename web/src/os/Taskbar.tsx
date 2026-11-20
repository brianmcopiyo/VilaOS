import { Squares2X2Icon } from "@heroicons/react/24/outline";
import { appById, type AppId } from "./catalog";
import type { WinState } from "./Window";

type Props = {
  windows: WinState[];
  focusKey: string | null;
  launcherOpen: boolean;
  chromeHidden: boolean;
  onToggleLauncher: () => void;
  onActivate: (key: string, appId: AppId) => void;
  onReveal: () => void;
  onScheduleHide: () => void;
};

export function Taskbar({
  windows,
  focusKey,
  launcherOpen,
  chromeHidden,
  onToggleLauncher,
  onActivate,
  onReveal,
  onScheduleHide,
}: Props) {
  const running = windows;
  const appsEmpty = running.length === 0;

  return (
    <footer
      className={[
        "taskbar",
        "desktop-chrome",
        chromeHidden ? "is-chrome-hidden" : "",
      ]
        .filter(Boolean)
        .join(" ")}
      role="navigation"
      aria-label="Taskbar"
      onMouseEnter={onReveal}
      onMouseLeave={onScheduleHide}
    >
      <div className="taskbar-inner">
        <div className="taskbar-zone taskbar-zone--launcher">
          <button
            type="button"
            data-launcher-origin
            className={[
              "taskbar-launcher",
              launcherOpen ? "is-active" : "",
            ]
              .filter(Boolean)
              .join(" ")}
            aria-label="Open application launcher"
            aria-expanded={launcherOpen}
            aria-controls="start-menu"
            onClick={onToggleLauncher}
          >
            <Squares2X2Icon className="taskbar-launcher-icon" />
          </button>
        </div>

        <div
          className={[
            "taskbar-zone",
            "taskbar-zone--apps",
            appsEmpty ? "is-empty" : "",
          ]
            .filter(Boolean)
            .join(" ")}
        >
          <div className="taskbar-apps">
            {running.map((w) => {
              const app = appById(w.appId);
              const Icon = app.icon;
              const active = focusKey === w.key && !w.minimized;
              return (
                <div key={w.key} className="taskbar-item">
                  <button
                    type="button"
                    data-taskbar-app={w.appId}
                    className={[
                      "taskbar-app",
                      active ? "is-active" : "",
                      w.minimized ? "is-minimized" : "",
                    ]
                      .filter(Boolean)
                      .join(" ")}
                    aria-pressed={active}
                    title={app.name}
                    onClick={() => onActivate(w.key, w.appId)}
                  >
                    <span className="taskbar-app-icon">
                      <Icon />
                    </span>
                    <span className="taskbar-app-label">{app.name}</span>
                    <span className="taskbar-app-indicator" aria-hidden />
                  </button>
                </div>
              );
            })}
          </div>
        </div>
      </div>
    </footer>
  );
}
