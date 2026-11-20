/**
 * Literal React port of Chat-Room `resources/js/os/shell/SystemTrayPanel.vue`
 * Teleport → createPortal(document.body)
 */
import {
  useEffect,
  useLayoutEffect,
  useRef,
  useState,
  type CSSProperties,
} from "react";
import { createPortal } from "react-dom";
import { CalendarPanel } from "./CalendarPanel";
import { NotificationCenter } from "./NotificationCenter";

type Props = {
  open: boolean;
  timeZone?: string;
  traySelector?: string;
  onClose: () => void;
};

export function SystemTrayPanel({
  open,
  timeZone,
  traySelector = "[data-system-tray]",
  onClose,
}: Props) {
  const stackRef = useRef<HTMLDivElement>(null);
  const [panelStyle, setPanelStyle] = useState<Record<string, string>>({
    "--tray-panel-right": "16px",
    "--tray-panel-bottom": "90px",
    "--tray-panel-width": "min(400px, calc(100vw - 24px))",
  });
  const calendarEvents: Array<{ date?: string; count?: number }> = [];

  function positionPanel() {
    const tray = document.querySelector(traySelector);
    const margin = 12;
    const gap = 10;
    const maxWidth = Math.min(400, window.innerWidth - margin * 2);

    if (!tray) {
      setPanelStyle({
        "--tray-panel-right": `${margin}px`,
        "--tray-panel-bottom": `${margin + 58 + gap}px`,
        "--tray-panel-width": `${maxWidth}px`,
      });
      return;
    }

    const rect = tray.getBoundingClientRect();
    let right = Math.max(margin, window.innerWidth - rect.right);
    const bottom = Math.max(margin, window.innerHeight - rect.top + gap);

    const stackWidth = stackRef.current?.offsetWidth || maxWidth;
    if (right + stackWidth > window.innerWidth - margin) {
      right = Math.max(margin, window.innerWidth - stackWidth - margin);
    }

    setPanelStyle({
      "--tray-panel-right": `${Math.round(right)}px`,
      "--tray-panel-bottom": `${Math.round(bottom)}px`,
      "--tray-panel-width": `${Math.round(maxWidth)}px`,
    });
  }

  useLayoutEffect(() => {
    if (!open) return;
    positionPanel();
    const id = window.requestAnimationFrame(() => positionPanel());
    return () => window.cancelAnimationFrame(id);
  }, [open, traySelector]);

  useEffect(() => {
    if (!open) return;

    function isInsideTrayUi(target: EventTarget | null) {
      if (!(target instanceof Element)) return false;
      return Boolean(
        target.closest(
          "#system-tray-panel, .tray-panel-stack, [data-system-tray]",
        ),
      );
    }

    function onPointerDownOutside(event: PointerEvent) {
      if (isInsideTrayUi(event.target)) return;
      onClose();
    }

    function onKeydown(event: KeyboardEvent) {
      if (event.key === "Escape") {
        event.preventDefault();
        onClose();
      }
    }

    function onResize() {
      positionPanel();
    }

    window.addEventListener("keydown", onKeydown);
    window.addEventListener("resize", onResize);
    document.addEventListener("pointerdown", onPointerDownOutside, true);
    return () => {
      window.removeEventListener("keydown", onKeydown);
      window.removeEventListener("resize", onResize);
      document.removeEventListener("pointerdown", onPointerDownOutside, true);
    };
  }, [open, onClose]);

  if (!open) return null;

  return createPortal(
    <div className="tray-panel-root" role="presentation">
      <div className="tray-panel-backdrop" aria-hidden="true" />

      <div
        id="system-tray-panel"
        ref={stackRef}
        className="tray-panel-stack"
        role="dialog"
        aria-label="Calendar and notification center"
        aria-modal="false"
        style={panelStyle as CSSProperties}
      >
        <div className="tray-panel tray-panel--notifications">
          <NotificationCenter />
        </div>

        <div className="tray-panel tray-panel--calendar">
          <CalendarPanel timeZone={timeZone} events={calendarEvents} />
        </div>
      </div>
    </div>,
    document.body,
  );
}
