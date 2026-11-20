import { useState } from "react";
import { STORE_CATALOG, type AppId } from "../catalog";
import "./apps.css";

type Props = {
  onOpen: (id: AppId) => void;
};

export function StoreApp({ onOpen }: Props) {
  const [note, setNote] = useState("Browse apps for this shell.");

  return (
    <div className="app">
      <h3 className="app__title">Store</h3>
      <p className="app__lead">{note}</p>

      <div className="app__store">
        {STORE_CATALOG.map((item) => {
          const Icon = item.icon;
          return (
            <article key={item.id} className="app__card">
              <span className="app__card-glyph" aria-hidden>
                <Icon />
              </span>
              <div>
                <h4 className="app__card-name">{item.name}</h4>
                <p className="app__card-blurb">{item.blurb}</p>
              </div>
              <button
                type="button"
                className="app__btn app__btn--ghost"
                onClick={() => {
                  setNote(`Opening ${item.name}…`);
                  onOpen(item.id);
                }}
              >
                Open
              </button>
            </article>
          );
        })}
      </div>
    </div>
  );
}
