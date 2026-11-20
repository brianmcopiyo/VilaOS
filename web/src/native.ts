export type PwdResult = { ok: boolean; path?: string; error?: string };
export type CdResult = { ok: boolean; path?: string; error?: string };
export type RunResult = { ok: boolean; code: number; status: string };

type NativeApi = {
  nativePwd: () => Promise<PwdResult>;
  nativeCd: (path: string) => Promise<CdResult>;
  nativeRun: (cmd: string) => Promise<RunResult>;
  nativeQuit: () => Promise<{ ok: boolean }>;
};

function api(): NativeApi | null {
  const w = window as Window & Partial<NativeApi>;
  if (typeof w.nativePwd === "function") {
    return w as unknown as NativeApi;
  }
  return null;
}

/** True when running inside the embedded WebView (the real app). */
export function isNative(): boolean {
  return api() !== null;
}

export async function pwd(): Promise<string> {
  const n = api();
  if (!n) {
    return "C:\\Users\\HP\\Desktop\\Terminal";
  }
  const r = await n.nativePwd();
  return r.path ?? "(unavailable)";
}

export async function cd(path: string): Promise<string> {
  const n = api();
  if (!n) {
    return path || "Home";
  }
  const r = await n.nativeCd(path);
  if (!r.ok) {
    throw new Error(r.error ?? "cd failed");
  }
  return r.path ?? path;
}

export async function run(cmd: string): Promise<RunResult> {
  const n = api();
  if (!n) {
    return { ok: true, code: 0, status: `Queued: ${cmd}` };
  }
  return n.nativeRun(cmd);
}

export async function quit(): Promise<void> {
  const n = api();
  if (!n) {
    window.close();
    return;
  }
  await n.nativeQuit();
}
