/**
 * Appearance controls mirror Chat-Room SettingsHub appearance tab structure.
 */
import { useEffect, useState } from "react";
import { pwd } from "../../native";
import "./apps.css";
import "./settings-appearance.css";

const WALLPAPERS = [
  { id: "abstract", label: "Abstract", preview: "/os/desktop-wallpaper-abstract.png" },
  { id: "aurora", label: "Aurora", preview: "/os/desktop-wallpaper-aurora.png" },
  { id: "dusk", label: "Dusk", preview: "/os/desktop-wallpaper-dusk.png" },
  { id: "mist", label: "Mist", preview: "/os/desktop-wallpaper-mist.png" },
  { id: "forest", label: "Forest", preview: "/os/desktop-wallpaper-forest.png" },
  { id: "midnight", label: "Midnight", preview: "/os/desktop-wallpaper-midnight.png" },
] as const;

type Props = {
  wallpaper: string;
  hour12: boolean;
  darkMode: boolean;
  onWallpaper: (w: string) => void;
  onHour12: (v: boolean) => void;
  onDarkMode: (v: boolean) => void;
};

export function SettingsApp({
  wallpaper,
  hour12,
  darkMode,
  onWallpaper,
  onHour12,
  onDarkMode,
}: Props) {
  const [cwd, setCwd] = useState("…");

  useEffect(() => {
    void pwd()
      .then(setCwd)
      .catch(() => setCwd("(unavailable)"));
  }, []);

  return (
    <div className="app">
      <h3 className="app__title">Settings</h3>
      <p className="app__lead">
        Light or dark interface for the OS shell and apps
      </p>

      <section className="app-section">
        <header className="app-section__head">
          <h4 className="app-section__title">Theme</h4>
          <p className="app-section__desc">
            Light or dark interface for the OS shell and apps
          </p>
        </header>

        <div className="theme-row">
          <div className="theme-row__copy">
            <p className="theme-row__label">Dark mode</p>
            <p className="theme-row__hint">
              Lower glare across windows, menus, and applications
            </p>
          </div>
          <button
            type="button"
            className={["theme-toggle", darkMode ? "is-on" : ""]
              .filter(Boolean)
              .join(" ")}
            role="switch"
            aria-checked={darkMode}
            onClick={() => onDarkMode(!darkMode)}
          >
            <span className="theme-toggle__knob" />
          </button>
        </div>

        <div className="theme-cards">
          <button
            type="button"
            className={["theme-card", !darkMode ? "is-active" : ""]
              .filter(Boolean)
              .join(" ")}
            onClick={() => onDarkMode(false)}
          >
            <p className="theme-card__title">Light</p>
            <p className="theme-card__copy">Bright surfaces and soft contrast</p>
          </button>
          <button
            type="button"
            className={["theme-card", darkMode ? "is-active" : ""]
              .filter(Boolean)
              .join(" ")}
            onClick={() => onDarkMode(true)}
          >
            <p className="theme-card__title">Dark</p>
            <p className="theme-card__copy">Lower glare for longer sessions</p>
          </button>
        </div>
      </section>

      <section className="app-section">
        <header className="app-section__head">
          <h4 className="app-section__title">Wallpaper</h4>
          <p className="app-section__desc">
            Desktop background behind your windows
          </p>
        </header>
        <div className="wallpaper-grid">
          {WALLPAPERS.map((paper) => (
            <button
              key={paper.id}
              type="button"
              className={[
                "wallpaper-tile",
                wallpaper === paper.id ? "is-active" : "",
              ]
                .filter(Boolean)
                .join(" ")}
              onClick={() => onWallpaper(paper.id)}
            >
              <div
                className="wallpaper-tile__preview"
                style={{ backgroundImage: `url("${paper.preview}")` }}
              />
              <span className="wallpaper-tile__label">{paper.label}</span>
            </button>
          ))}
        </div>
      </section>

      <section className="app-section">
        <header className="app-section__head">
          <h4 className="app-section__title">Clock</h4>
        </header>
        <div className="app__row">
          <button
            type="button"
            className={["app__choice", hour12 ? "app__choice--on" : ""]
              .filter(Boolean)
              .join(" ")}
            onClick={() => onHour12(true)}
          >
            12-hour
          </button>
          <button
            type="button"
            className={["app__choice", !hour12 ? "app__choice--on" : ""]
              .filter(Boolean)
              .join(" ")}
            onClick={() => onHour12(false)}
          >
            24-hour
          </button>
        </div>
      </section>

      <section className="app-section">
        <header className="app-section__head">
          <h4 className="app-section__title">About</h4>
        </header>
        <p className="app__about">
          Terminal desktop shell — Chat-Room OS chrome, native C kernel.
          <br />
          <br />
          Current directory:
          <br />
          {cwd}
        </p>
      </section>
    </div>
  );
}
