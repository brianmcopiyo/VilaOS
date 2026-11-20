import { useCallback, useEffect, useRef, useState } from "react";
import type { AppId } from "./os/catalog";
import { Desktop } from "./os/Desktop";
import { SettingsApp } from "./os/apps/SettingsApp";
import { StoreApp } from "./os/apps/StoreApp";
import { TerminalApp } from "./os/apps/TerminalApp";
import { StartMenu } from "./os/StartMenu";
import { SystemTray } from "./os/SystemTray";
import { Taskbar } from "./os/Taskbar";
import { applyDarkClass, readDarkPreference } from "./os/theme";
import { Window, type WinState } from "./os/Window";

let seq = 1;
let zTop = 10;

function nextKey(appId: AppId): string {
  return `${appId}-${seq++}`;
}

function spawn(appId: AppId, count: number): WinState {
  const offset = 48 + (count % 5) * 28;
  return {
    key: nextKey(appId),
    appId,
    x: offset,
    y: offset,
    w: 720,
    h: 480,
    z: ++zTop,
    minimized: false,
    maximized: false,
  };
}

export default function App() {
  const [windows, setWindows] = useState<WinState[]>([]);
  const [focusKey, setFocusKey] = useState<string | null>(null);
  const [launcherOpen, setLauncherOpen] = useState(false);
  const [hour12, setHour12] = useState(true);
  const [wallpaper, setWallpaper] = useState("abstract");
  const [darkMode, setDarkMode] = useState(() => readDarkPreference());
  const [taskbarPeek, setTaskbarPeek] = useState(false);
  const [trayPeek, setTrayPeek] = useState(false);
  const [trayPanelOpen, setTrayPanelOpen] = useState(false);

  const taskbarHideTimer = useRef<number | null>(null);
  const trayHideTimer = useRef<number | null>(null);
  const hideDesktopChromeRef = useRef(false);
  const launcherOpenRef = useRef(false);
  const trayPanelOpenRef = useRef(false);

  const focused = windows.find((w) => w.key === focusKey);
  const hideDesktopChrome = !!(
    focused &&
    !focused.minimized &&
    focused.maximized
  );
  const taskbarHidden =
    hideDesktopChrome && !taskbarPeek && !launcherOpen;
  const trayHidden = hideDesktopChrome && !trayPeek && !trayPanelOpen;

  hideDesktopChromeRef.current = hideDesktopChrome;
  launcherOpenRef.current = launcherOpen;
  trayPanelOpenRef.current = trayPanelOpen;

  const clearTaskbarTimer = useCallback(() => {
    if (taskbarHideTimer.current != null) {
      window.clearTimeout(taskbarHideTimer.current);
      taskbarHideTimer.current = null;
    }
  }, []);

  const clearTrayTimer = useCallback(() => {
    if (trayHideTimer.current != null) {
      window.clearTimeout(trayHideTimer.current);
      trayHideTimer.current = null;
    }
  }, []);

  const revealTaskbar = useCallback(() => {
    clearTaskbarTimer();
    setTaskbarPeek(true);
  }, [clearTaskbarTimer]);

  const revealTray = useCallback(() => {
    clearTrayTimer();
    setTrayPeek(true);
  }, [clearTrayTimer]);

  const scheduleHideTaskbar = useCallback(() => {
    if (!hideDesktopChromeRef.current) return;
    if (launcherOpenRef.current) return;
    clearTaskbarTimer();
    taskbarHideTimer.current = window.setTimeout(() => {
      setTaskbarPeek(false);
      taskbarHideTimer.current = null;
    }, 420);
  }, [clearTaskbarTimer]);

  const scheduleHideTray = useCallback(() => {
    if (!hideDesktopChromeRef.current) return;
    if (trayPanelOpenRef.current) return;
    clearTrayTimer();
    trayHideTimer.current = window.setTimeout(() => {
      setTrayPeek(false);
      trayHideTimer.current = null;
    }, 420);
  }, [clearTrayTimer]);

  useEffect(() => {
    applyDarkClass(darkMode);
  }, [darkMode]);

  useEffect(() => {
    document.documentElement.style.setProperty(
      "--desktop-wallpaper-image",
      `url("/os/desktop-wallpaper-${wallpaper}.png")`,
    );
  }, [wallpaper]);

  useEffect(() => {
    if (hideDesktopChrome) {
      setLauncherOpen(false);
      setTrayPanelOpen(false);
    }
    setTaskbarPeek(false);
    setTrayPeek(false);
    clearTaskbarTimer();
    clearTrayTimer();
  }, [hideDesktopChrome, clearTaskbarTimer, clearTrayTimer]);

  useEffect(() => {
    if (launcherOpen) {
      revealTaskbar();
      setTrayPanelOpen(false);
    }
  }, [launcherOpen, revealTaskbar]);

  useEffect(() => {
    if (trayPanelOpen) {
      revealTray();
      setLauncherOpen(false);
    }
  }, [trayPanelOpen, revealTray]);

  useEffect(() => {
    return () => {
      clearTaskbarTimer();
      clearTrayTimer();
    };
  }, [clearTaskbarTimer, clearTrayTimer]);

  const focus = useCallback((key: string) => {
    setFocusKey(key);
    setWindows((list) =>
      list.map((w) =>
        w.key === key ? { ...w, z: ++zTop, minimized: false } : w,
      ),
    );
  }, []);

  const openApp = useCallback(
    (appId: AppId) => {
      setLauncherOpen(false);
      const existing = windows.find((w) => w.appId === appId);
      if (existing) {
        focus(existing.key);
        return;
      }
      const win = spawn(appId, windows.length);
      setWindows((list) => [...list, win]);
      setFocusKey(win.key);
    },
    [focus, windows],
  );

  const closeWin = useCallback((key: string) => {
    setWindows((list) => list.filter((w) => w.key !== key));
    setFocusKey((cur) => (cur === key ? null : cur));
  }, []);

  const activate = useCallback(
    (key: string) => {
      const w = windows.find((x) => x.key === key);
      if (!w) return;
      if (focusKey === key && !w.minimized) {
        setWindows((list) =>
          list.map((x) => (x.key === key ? { ...x, minimized: true } : x)),
        );
        return;
      }
      focus(key);
    },
    [focus, focusKey, windows],
  );

  const renderApp = (win: WinState) => {
    if (win.appId === "terminal") return <TerminalApp />;
    if (win.appId === "settings") {
      return (
        <SettingsApp
          wallpaper={wallpaper}
          hour12={hour12}
          darkMode={darkMode}
          onWallpaper={setWallpaper}
          onHour12={setHour12}
          onDarkMode={setDarkMode}
        />
      );
    }
    return <StoreApp onOpen={openApp} />;
  };

  return (
    <div
      id="app-main"
      className={[
        "shell",
        "relative",
        "h-[100dvh]",
        "w-screen",
        "overflow-hidden",
        "bg-surface",
        "text-fg",
        hideDesktopChrome ? "is-fullscreen-app" : "",
      ]
        .filter(Boolean)
        .join(" ")}
      tabIndex={-1}
    >
      <Desktop />

      <div className="windows absolute inset-0 z-[10] overflow-visible">
        {windows.map((win) => (
          <Window
            key={win.key}
            win={win}
            focused={focusKey === win.key}
            onFocus={() => focus(win.key)}
            onClose={() => closeWin(win.key)}
            onMinimize={() =>
              setWindows((list) =>
                list.map((w) =>
                  w.key === win.key ? { ...w, minimized: true } : w,
                ),
              )
            }
            onMaximize={() =>
              setWindows((list) =>
                list.map((w) =>
                  w.key === win.key
                    ? { ...w, maximized: !w.maximized }
                    : w,
                ),
              )
            }
            onMove={(x, y) =>
              setWindows((list) =>
                list.map((w) => (w.key === win.key ? { ...w, x, y } : w)),
              )
            }
          >
            {renderApp(win)}
          </Window>
        ))}
      </div>

      {hideDesktopChrome ? (
        <>
          <button
            type="button"
            className={[
              "chrome-peek",
              "chrome-peek--taskbar",
              !taskbarHidden ? "is-open" : "",
            ]
              .filter(Boolean)
              .join(" ")}
            aria-label="Show taskbar"
            onMouseEnter={revealTaskbar}
            onMouseLeave={scheduleHideTaskbar}
            onFocus={revealTaskbar}
          >
            <span className="chrome-peek-pill" aria-hidden />
          </button>
          <button
            type="button"
            className={[
              "chrome-peek",
              "chrome-peek--tray",
              !trayHidden ? "is-open" : "",
            ]
              .filter(Boolean)
              .join(" ")}
            aria-label="Show system tray"
            onMouseEnter={revealTray}
            onMouseLeave={scheduleHideTray}
            onFocus={revealTray}
          >
            <span className="chrome-peek-pill" aria-hidden />
          </button>
        </>
      ) : null}

      <Taskbar
        windows={windows}
        focusKey={focusKey}
        launcherOpen={launcherOpen}
        chromeHidden={taskbarHidden}
        onToggleLauncher={() => setLauncherOpen((v) => !v)}
        onActivate={(key) => activate(key)}
        onReveal={revealTaskbar}
        onScheduleHide={scheduleHideTaskbar}
      />

      <SystemTray
        chromeHidden={trayHidden}
        panelOpen={trayPanelOpen}
        onTogglePanel={() => setTrayPanelOpen((v) => !v)}
        onClosePanel={() => setTrayPanelOpen(false)}
        onReveal={revealTray}
        onScheduleHide={scheduleHideTray}
      />

      <StartMenu
        open={launcherOpen}
        onLaunch={openApp}
        onClose={() => setLauncherOpen(false)}
      />
    </div>
  );
}
