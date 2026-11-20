import { useEffect, useState } from "react";
import { cd, pwd, run } from "../../native";
import "./apps.css";

export function TerminalApp() {
  const [path, setPath] = useState("…");
  const [nav, setNav] = useState("");
  const [cmd, setCmd] = useState("");
  const [out, setOut] = useState("Ready.");

  useEffect(() => {
    void pwd()
      .then(setPath)
      .catch(() => setPath("(unavailable)"));
  }, []);

  return (
    <div className="app">
      <h3 className="app__title">Terminal</h3>
      <p className="app__lead">
        Talks to the native C kernel — change directory and run commands.
      </p>

      <div className="app__section">
        <p className="app__label">Working directory</p>
        <p className="app__path">{path}</p>
        <input
          className="app__field"
          value={nav}
          placeholder="Path for cd"
          onChange={(e) => setNav(e.target.value)}
          onKeyDown={(e) => {
            if (e.key === "Enter") {
              void (async () => {
                try {
                  const next = await cd(nav.trim());
                  setPath(next);
                  setOut(`cd → ${next}`);
                } catch (err) {
                  setOut(err instanceof Error ? err.message : "cd failed");
                }
              })();
            }
          }}
        />
        <div className="app__actions">
          <button
            type="button"
            className="app__btn"
            onClick={() => {
              void (async () => {
                try {
                  const next = await cd(nav.trim());
                  setPath(next);
                  setOut(`cd → ${next}`);
                } catch (err) {
                  setOut(err instanceof Error ? err.message : "cd failed");
                }
              })();
            }}
          >
            Change directory
          </button>
          <button
            type="button"
            className="app__btn app__btn--ghost"
            onClick={() => {
              void pwd()
                .then((p) => {
                  setPath(p);
                  setOut(`pwd → ${p}`);
                })
                .catch(() => setOut("pwd failed"));
            }}
          >
            Refresh pwd
          </button>
        </div>
      </div>

      <div className="app__section">
        <p className="app__label">Run</p>
        <input
          className="app__field"
          value={cmd}
          placeholder="Command line"
          onChange={(e) => setCmd(e.target.value)}
          onKeyDown={(e) => {
            if (e.key === "Enter") {
              void run(cmd.trim()).then((r) => setOut(r.status));
            }
          }}
        />
        <div className="app__actions">
          <button
            type="button"
            className="app__btn"
            onClick={() => {
              void run(cmd.trim()).then((r) => setOut(r.status));
            }}
          >
            Run
          </button>
        </div>
        <pre className="app__out">{out}</pre>
      </div>
    </div>
  );
}
