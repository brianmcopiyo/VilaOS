/**
 * Literal React port of Chat-Room `resources/js/os/shell/AppLauncher.vue`
 * Position is anchored to `[data-launcher-origin]` (Start button), not the viewport.
 */
import {
  useEffect,
  useLayoutEffect,
  useMemo,
  useRef,
  useState,
  type CSSProperties,
} from "react";
import { createPortal } from "react-dom";
import { MagnifyingGlassIcon } from "@heroicons/react/24/outline";
import { APPS, type AppId } from "./catalog";

type Props = {
  open: boolean;
  onLaunch: (id: AppId) => void;
  onClose: () => void;
};

function positionMenu(
  menuEl: HTMLElement | null,
): Record<string, string> {
  const launcher = document.querySelector("[data-launcher-origin]");
  const taskbar = document.querySelector(".taskbar");
  const styles = getComputedStyle(document.documentElement);
  const pad =
    Number.parseFloat(styles.getPropertyValue("--taskbar-bottom")) || 16;
  const gap =
    Number.parseFloat(styles.getPropertyValue("--start-gap")) || pad;
  const taskbarHeight =
    Number.parseFloat(styles.getPropertyValue("--taskbar-height")) || 58;

  const fallbackWidth = Math.min(480, window.innerWidth - pad * 2);
  const measuredWidth = menuEl?.offsetWidth ?? 0;
  const menuWidth = measuredWidth > 0 ? measuredWidth : fallbackWidth;

  const taskbarRect = taskbar?.getBoundingClientRect();
  const launcherRect = launcher?.getBoundingClientRect();

  const centerX = launcherRect
    ? launcherRect.left + launcherRect.width / 2
    : window.innerWidth / 2;

  let left = centerX - menuWidth / 2;
  left = Math.max(pad, Math.min(left, window.innerWidth - menuWidth - pad));

  const originX =
    menuWidth > 0 ? ((centerX - left) / menuWidth) * 100 : 50;

  const bottom = taskbarRect
    ? Math.max(pad, window.innerHeight - taskbarRect.top + gap)
    : pad + taskbarHeight + gap;

  const topPad = 16;
  const maxHeight = Math.max(
    280,
    Math.floor(window.innerHeight - bottom - topPad),
  );

  return {
    "--start-menu-left": `${Math.round(left)}px`,
    "--start-menu-bottom": `${Math.round(bottom)}px`,
    "--start-menu-width": `${Math.round(menuWidth)}px`,
    "--start-menu-max-height": `${maxHeight}px`,
    "--start-origin-x": `${Math.max(8, Math.min(92, originX))}%`,
  };
}

export function StartMenu({ open, onLaunch, onClose }: Props) {
  const [query, setQuery] = useState("");
  const searchRef = useRef<HTMLInputElement>(null);
  const menuRef = useRef<HTMLDivElement>(null);
  const [menuStyle, setMenuStyle] = useState<Record<string, string>>(() =>
    positionMenu(null),
  );

  useLayoutEffect(() => {
    if (!open) {
      setQuery("");
      return;
    }
    setMenuStyle(positionMenu(menuRef.current));
    const id = window.requestAnimationFrame(() => {
      setMenuStyle(positionMenu(menuRef.current));
      searchRef.current?.focus();
    });
    return () => window.cancelAnimationFrame(id);
  }, [open]);

  useEffect(() => {
    if (!open) return;

    const onKey = (e: KeyboardEvent) => {
      if (e.key === "Escape") onClose();
    };
    const onDown = (e: PointerEvent) => {
      const target = e.target;
      if (!(target instanceof Element)) return;
      if (
        target.closest(
          "#start-menu, .start-menu, [data-launcher-origin], .taskbar-launcher",
        )
      ) {
        return;
      }
      onClose();
    };
    const onResize = () => setMenuStyle(positionMenu(menuRef.current));

    window.addEventListener("keydown", onKey);
    window.addEventListener("resize", onResize);
    document.addEventListener("pointerdown", onDown, true);
    return () => {
      window.removeEventListener("keydown", onKey);
      window.removeEventListener("resize", onResize);
      document.removeEventListener("pointerdown", onDown, true);
    };
  }, [open, onClose]);

  const filtered = useMemo(() => {
    const q = query.trim().toLowerCase();
    if (!q) return APPS;
    return APPS.filter(
      (a) =>
        a.name.toLowerCase().includes(q) ||
        a.blurb.toLowerCase().includes(q),
    );
  }, [query]);

  if (!open) return null;

  return createPortal(
    <div className="start-menu-root" role="presentation">
      <div className="start-menu-backdrop" aria-hidden="true" />
      <div
        id="start-menu"
        ref={menuRef}
        className="start-menu"
        role="dialog"
        aria-label="Application launcher"
        aria-modal="false"
        style={menuStyle as CSSProperties}
      >
        <header className="start-menu-header">
          <div className="start-menu-search">
            <MagnifyingGlassIcon className="start-menu-search-icon" />
            <input
              ref={searchRef}
              type="search"
              className="start-menu-search-input"
              placeholder="Search applications…"
              autoComplete="off"
              spellCheck={false}
              value={query}
              onChange={(e) => setQuery(e.target.value)}
            />
          </div>
        </header>

        <div className="start-menu-body os-scroll">
          <section className="start-menu-section">
            <h3 className="start-menu-section-title">
              {query.trim() ? "Results" : "Applications"}
            </h3>
            {filtered.length ? (
              <div className="start-menu-grid">
                {filtered.map((app) => {
                  const Icon = app.icon;
                  return (
                    <button
                      key={app.id}
                      type="button"
                      className="start-menu-app"
                      title={app.blurb || app.name}
                      onClick={() => onLaunch(app.id)}
                    >
                      <span className="start-menu-app-icon">
                        <Icon />
                      </span>
                      <span className="start-menu-app-label">{app.name}</span>
                    </button>
                  );
                })}
              </div>
            ) : (
              <p className="start-menu-empty">
                No applications match “{query}”.
              </p>
            )}
          </section>
        </div>
      </div>
    </div>,
    document.body,
  );
}
