import {
  useCallback,
  useRef,
  type PointerEvent as ReactPointerEvent,
  type ReactNode,
} from "react";
import {
  MinusIcon,
  Square2StackIcon,
  StopIcon,
  XMarkIcon,
} from "@heroicons/react/24/outline";
import { appById, type AppId } from "./catalog";

export type WinState = {
  key: string;
  appId: AppId;
  x: number;
  y: number;
  w: number;
  h: number;
  z: number;
  minimized: boolean;
  maximized: boolean;
};

type Props = {
  win: WinState;
  focused: boolean;
  children: ReactNode;
  onFocus: () => void;
  onClose: () => void;
  onMinimize: () => void;
  onMaximize: () => void;
  onMove: (x: number, y: number) => void;
};

export function Window({
  win,
  focused,
  children,
  onFocus,
  onClose,
  onMinimize,
  onMaximize,
  onMove,
}: Props) {
  const drag = useRef<{ ox: number; oy: number; sx: number; sy: number } | null>(
    null,
  );
  const def = appById(win.appId);
  const Icon = def.icon;

  const onPointerDown = useCallback(
    (e: ReactPointerEvent<HTMLElement>) => {
      if ((e.target as HTMLElement).closest?.(".window-titlebar__controls")) {
        return;
      }
      if (win.maximized) {
        onFocus();
        return;
      }
      onFocus();
      e.currentTarget.setPointerCapture(e.pointerId);
      drag.current = {
        ox: e.clientX,
        oy: e.clientY,
        sx: win.x,
        sy: win.y,
      };
    },
    [onFocus, win.maximized, win.x, win.y],
  );

  const onPointerMove = useCallback(
    (e: ReactPointerEvent<HTMLElement>) => {
      if (!drag.current) return;
      const dx = e.clientX - drag.current.ox;
      const dy = e.clientY - drag.current.oy;
      onMove(drag.current.sx + dx, Math.max(0, drag.current.sy + dy));
    },
    [onMove],
  );

  const onPointerUp = useCallback((e: ReactPointerEvent<HTMLElement>) => {
    if (drag.current) {
      e.currentTarget.releasePointerCapture(e.pointerId);
      drag.current = null;
    }
  }, []);

  const style = win.maximized
    ? {
        left: 0,
        top: 0,
        width: "100%",
        height: "100%",
        zIndex: win.z,
      }
    : {
        left: win.x,
        top: win.y,
        width: win.w,
        height: win.h,
        zIndex: win.z,
      };

  if (win.minimized) {
    return null;
  }

  return (
    <section
      className={[
        "window",
        "absolute",
        "flex",
        "flex-col",
        focused ? "window--active" : "window--inactive",
        win.maximized ? "window--flush" : "",
      ]
        .filter(Boolean)
        .join(" ")}
      style={style}
      role="application"
      aria-label={def.name}
      onMouseDown={onFocus}
    >
      <header
        className={[
          "window-titlebar",
          focused ? "window-titlebar--active" : "window-titlebar--inactive",
        ].join(" ")}
        role="banner"
        title="Double-click to maximize or restore"
        onPointerDown={onPointerDown}
        onPointerMove={onPointerMove}
        onPointerUp={onPointerUp}
        onDoubleClick={onMaximize}
      >
        <span className="window-titlebar__icon" aria-hidden>
          <Icon className="h-5 w-5" />
        </span>
        <h2 className="window-titlebar__title">{def.name}</h2>
        <div
          className="window-titlebar__controls"
          onPointerDown={(e) => e.stopPropagation()}
          onDoubleClick={(e) => e.stopPropagation()}
        >
          <button
            type="button"
            className="window-titlebar__ctrl"
            aria-label="Minimize"
            onClick={onMinimize}
          >
            <MinusIcon className="h-3.5 w-3.5" />
          </button>
          <button
            type="button"
            className="window-titlebar__ctrl"
            aria-label={win.maximized ? "Restore" : "Maximize"}
            onClick={onMaximize}
          >
            {win.maximized ? (
              <Square2StackIcon className="h-3.5 w-3.5" />
            ) : (
              <StopIcon className="h-3 w-3" />
            )}
          </button>
          <button
            type="button"
            className="window-titlebar__ctrl window-titlebar__ctrl--close"
            aria-label="Close"
            onClick={onClose}
          >
            <XMarkIcon className="h-3.5 w-3.5" />
          </button>
        </div>
      </header>

      <div className="window-titlebar-gap" aria-hidden />

      <div className="window-body">
        <div className="window-body__content">{children}</div>
      </div>
    </section>
  );
}
