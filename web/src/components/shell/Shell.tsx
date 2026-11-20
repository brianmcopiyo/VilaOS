import type { ReactNode } from "react";
import { Chip } from "../common/Chip";
import "./shell.css";

export type Action = {
  id: string;
  label: string;
};

type Props = {
  title: string;
  children: ReactNode;
  actions: Action[];
  actionFocus: number;
  onAction: (id: string) => void;
};

export function Shell({
  title,
  children,
  actions,
  actionFocus,
  onAction,
}: Props) {
  return (
    <div className="shell">
      <header className="shell__header">
        <h1 className="shell__title">{title}</h1>
      </header>

      <main className="shell__content">{children}</main>

      <footer className="shell__footer">
        <div className="shell__actions">
          {actions.map((action, index) => (
            <Chip
              key={action.id}
              label={action.label}
              selected={index === actionFocus}
              onClick={() => onAction(action.id)}
            />
          ))}
        </div>
      </footer>
    </div>
  );
}
