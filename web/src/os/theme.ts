const DARK_KEY = "terminal.darkMode";

export function readDarkPreference(): boolean {
  try {
    const stored = localStorage.getItem(DARK_KEY);
    if (stored === "true") return true;
    if (stored === "false") return false;
  } catch {
    /* ignore */
  }
  return window.matchMedia("(prefers-color-scheme: dark)").matches;
}

export function applyDarkClass(value: boolean): void {
  document.documentElement.classList.toggle("dark", value);
  try {
    localStorage.setItem(DARK_KEY, String(value));
  } catch {
    /* ignore */
  }
}
